#include "XRFrameDriver.hpp"

#include "OpenXRFrameTiming.hpp"
#include "OpenXRResult.hpp"
#include "OpenXRSessionState.hpp"
#include "OpenXRViewConversion.hpp"

#include "AREngine/Core/Log.hpp"

#include <format>
#include <thread>

namespace AREngine::XR::OpenXR
{
    namespace
    {
        double SecondsBetween(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
        {
            return std::chrono::duration<double>(end - start).count();
        }

        // Paces the FrameStatus::Idle branch of PrepareFrame() - see
        // that method's own comment for why this exists.
        constexpr std::chrono::milliseconds kIdlePollInterval{16};
    }

    XRFrameDriver::XRFrameDriver(XrInstance instance, OpenXRSession& session, OpenXRReferenceSpace& localSpace,
                                  XrViewConfigurationType primaryViewConfigurationType,
                                  XrEnvironmentBlendMode environmentBlendMode,
                                  float nearZ, float farZ)
        : m_instance(instance)
        , m_session(session)
        , m_localSpace(localSpace)
        , m_primaryViewConfigurationType(primaryViewConfigurationType)
        , m_environmentBlendMode(environmentBlendMode)
        , m_nearZ(nearZ)
        , m_farZ(farZ)
    {
    }

    Frame::FrameContext XRFrameDriver::PrepareFrame()
    {
        const SessionEventPollResult pollResult = PollSessionEvents(m_instance);

        if (pollResult.instanceLossPending)
        {
            AR_LOG_WARNING("XrEventDataInstanceLossPending received - reporting FrameStatus::Stop");
            return Frame::FrameContext{Frame::FrameTiming{}, Frame::FrameStatus::Stop};
        }

        if (pollResult.sessionStateChanged)
        {
            // Every transition observed this cycle is processed IN
            // ORDER, not just the last one - see
            // DetermineSessionLifecycleActions's own documentation for
            // why (a runtime can legitimately deliver e.g. READY
            // immediately followed by STOPPING within one draining
            // cycle; reacting only to the last state would silently
            // skip the required xrBeginSession or xrEndSession call).
            const std::vector<SessionLifecycleAction> actions =
                DetermineSessionLifecycleActions(pollResult.sessionStateSequence, m_session.IsRunning());

            for (std::size_t i = 0; i < pollResult.sessionStateSequence.size(); ++i)
            {
                m_currentState = pollResult.sessionStateSequence[i];
                AR_LOG_INFO(std::format("Session state changed -> {}", FormatSessionState(m_currentState)));

                if (actions[i] == SessionLifecycleAction::Begin)
                {
                    m_session.BeginSession(m_primaryViewConfigurationType);
                    AR_LOG_INFO("xrBeginSession succeeded - session is now running");
                }
                else if (actions[i] == SessionLifecycleAction::End)
                {
                    m_session.EndSession();
                    AR_LOG_INFO("xrEndSession succeeded - session is no longer running");
                }
            }
        }

        const Frame::FrameStatus status = DetermineFrameStatus(m_currentState, m_session.IsRunning());
        if (status != Frame::FrameStatus::Continue)
        {
            if (status == Frame::FrameStatus::Idle)
            {
                // CPU-hot-spin guard: xrWaitFrame cannot legally be
                // called here (the session isn't running - either not
                // yet begun, or between STOPPING and EXITING), so
                // unlike the Continue path below (which blocks inside
                // the real xrWaitFrame call), nothing paces this
                // branch. Without this sleep, a caller looping tightly
                // on FrameStatus::Idle would busy-spin PollSessionEvents
                // at full CPU while waiting for READY/EXITING.
                // Deliberately implementation-private to XRFrameDriver,
                // not part of the generic Frame API - see
                // docs/ARCHITECTURE.md, "CPU Behavior While Idle
                // (M9E.5)".
                std::this_thread::sleep_for(kIdlePollInterval);
            }

            Frame::FrameTiming timing;
            timing.shouldRender = false; // unused when status != Continue; explicit for clarity
            return Frame::FrameContext{timing, status};
        }

        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        CheckXrResult(m_instance, xrWaitFrame(m_session.Get(), &frameWaitInfo, &frameState), "xrWaitFrame");
        m_lastPredictedDisplayTime = frameState.predictedDisplayTime;

        const auto now = std::chrono::steady_clock::now();
        Frame::FrameTiming timing;
        timing.deltaTimeSeconds = SecondsBetween(m_lastTick, now);
        m_lastTick = now;
        timing.totalTimeSeconds = SecondsBetween(m_clockStart, now);
        timing.predictedDisplayTimeSeconds = XrTimeToSeconds(frameState.predictedDisplayTime);
        timing.shouldRender = (frameState.shouldRender != XR_FALSE);

        return Frame::FrameContext{timing, Frame::FrameStatus::Continue};
    }

    void XRFrameDriver::BeginFrame()
    {
        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CheckXrResult(m_instance, xrBeginFrame(m_session.Get(), &frameBeginInfo), "xrBeginFrame");
    }

    std::vector<Frame::ViewInfo> XRFrameDriver::GetViews()
    {
        m_lastLocatedViews.clear();

        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = m_primaryViewConfigurationType;
        // THE CURRENT frame's predicted display time - stashed by this
        // same frame's own xrWaitFrame call in PrepareFrame(). Never
        // wall-clock time, never a previous frame's time - the located
        // views must correspond to the frame actually being rendered/
        // submitted.
        locateInfo.displayTime = m_lastPredictedDisplayTime;
        locateInfo.space = m_localSpace.Get();

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        std::uint32_t viewCountOutput = 0;
        CheckXrResult(m_instance,
            xrLocateViews(m_session.Get(), &locateInfo, &viewState, 0, &viewCountOutput, nullptr),
            "xrLocateViews (count query)");

        std::vector<XrView> views(viewCountOutput, XrView{XR_TYPE_VIEW});
        if (viewCountOutput > 0)
        {
            CheckXrResult(m_instance,
                xrLocateViews(m_session.Get(), &locateInfo, &viewState, viewCountOutput, &viewCountOutput, views.data()),
                "xrLocateViews (data query)");
        }

        const bool valid = IsViewStateValid(viewState.viewStateFlags);
        if (valid != m_lastViewStateValid)
        {
            // Logged only on a valid<->invalid transition, not every
            // frame - avoids log spam while still surfacing the
            // condition clearly when it actually changes.
            if (!valid)
            {
                AR_LOG_WARNING(std::format(
                    "xrLocateViews: required pose data is not valid (XrViewState flags {:#x}) - "
                    "returning no usable views this frame rather than fabricating an identity pose",
                    static_cast<unsigned int>(viewState.viewStateFlags)));
            }
            else
            {
                AR_LOG_INFO("xrLocateViews: pose data is valid again");
            }
            m_lastViewStateValid = valid;
        }

        if (!valid)
        {
            return {};
        }

        m_lastLocatedViews = views;

        std::vector<Frame::ViewInfo> result;
        result.reserve(views.size());
        for (const XrView& view : views)
        {
            result.push_back(ConvertXrViewToViewInfo(view, m_nearZ, m_farZ));
        }
        return result;
    }

    void XRFrameDriver::EndFrame()
    {
        std::vector<const XrCompositionLayerBaseHeader*> layers;
        if (m_pendingProjectionLayer != nullptr)
        {
            layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(m_pendingProjectionLayer));
        }

        XrFrameEndInfo frameEndInfo{};
        frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
        frameEndInfo.displayTime = m_lastPredictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_environmentBlendMode;
        frameEndInfo.layerCount = static_cast<std::uint32_t>(layers.size());
        frameEndInfo.layers = layers.data();
        CheckXrResult(m_instance, xrEndFrame(m_session.Get(), &frameEndInfo), "xrEndFrame");

        // Consumed - reset so a future tick that never calls
        // SetPendingProjectionLayer() (shouldRender was false, or no
        // valid views) safely defaults to zero layers, not a stale
        // pointer from a previous frame.
        m_pendingProjectionLayer = nullptr;
    }

    void XRFrameDriver::RequestExit()
    {
        m_session.RequestExit();
    }
}
