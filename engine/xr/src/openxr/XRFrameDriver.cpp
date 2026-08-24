#include "XRFrameDriver.hpp"

#include "OpenXRFrameTiming.hpp"
#include "OpenXRResult.hpp"
#include "OpenXRSessionState.hpp"

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

    XRFrameDriver::XRFrameDriver(XrInstance instance, OpenXRSession& session,
                                  XrViewConfigurationType primaryViewConfigurationType,
                                  XrEnvironmentBlendMode environmentBlendMode)
        : m_instance(instance)
        , m_session(session)
        , m_primaryViewConfigurationType(primaryViewConfigurationType)
        , m_environmentBlendMode(environmentBlendMode)
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
        return {};
    }

    void XRFrameDriver::EndFrame()
    {
        XrFrameEndInfo frameEndInfo{};
        frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
        frameEndInfo.displayTime = m_lastPredictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_environmentBlendMode;
        frameEndInfo.layerCount = 0;
        frameEndInfo.layers = nullptr;
        CheckXrResult(m_instance, xrEndFrame(m_session.Get(), &frameEndInfo), "xrEndFrame");
    }

    void XRFrameDriver::RequestExit()
    {
        m_session.RequestExit();
    }
}
