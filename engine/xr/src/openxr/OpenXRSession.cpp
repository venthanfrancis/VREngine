#include "OpenXRSession.hpp"

#include "OpenXRResult.hpp"

#include "AREngine/Core/Log.hpp"

#include <format>

namespace AREngine::XR::OpenXR
{
    SessionEventPollResult PollSessionEvents(XrInstance instance)
    {
        SessionEventPollResult result;

        while (true)
        {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            const XrResult pollResult = xrPollEvent(instance, &event);
            if (pollResult == XR_EVENT_UNAVAILABLE)
            {
                break;
            }
            CheckXrResult(instance, pollResult, "xrPollEvent");

            switch (event.type)
            {
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                {
                    // Standard OpenXR polymorphic-event pattern: every
                    // event struct shares XrEventDataBuffer's
                    // {type; next;} initial layout, and the buffer is
                    // sized (4000 bytes) specifically to hold any of
                    // them - reinterpreting after checking `type` is
                    // the documented, correct way to read it (the same
                    // pattern Khronos's own samples use).
                    const auto& stateEvent = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
                    result.sessionStateChanged = true;
                    result.newSessionState = stateEvent.state;
                    result.sessionStateSequence.push_back(stateEvent.state);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                {
                    const auto& lossEvent = reinterpret_cast<const XrEventDataInstanceLossPending&>(event);
                    result.instanceLossPending = true;
                    result.lossTime = lossEvent.lossTime;
                    break;
                }
                case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                {
                    const auto& lostEvent = reinterpret_cast<const XrEventDataEventsLost&>(event);
                    AR_LOG_WARNING(std::format(
                        "OpenXR reported {} lost event(s) - the runtime's event queue overflowed", lostEvent.lostEventCount));
                    break;
                }
                default:
                    // Ignored: not relevant to M9D. Deliberately not a
                    // giant event-dispatch system - see
                    // docs/ARCHITECTURE.md, "Event Polling (M9D)".
                    break;
            }
        }

        return result;
    }

    OpenXRSession::OpenXRSession(XrInstance instance, XrSystemId systemId, const VulkanGraphicsBindingData& bindingData)
        : m_instance(instance)
    {
        XrGraphicsBindingVulkan2KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        graphicsBinding.instance = bindingData.instance;
        graphicsBinding.physicalDevice = bindingData.physicalDevice;
        graphicsBinding.device = bindingData.device;
        graphicsBinding.queueFamilyIndex = bindingData.queueFamilyIndex;
        graphicsBinding.queueIndex = bindingData.queueIndex;

        XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
        createInfo.next = &graphicsBinding;
        createInfo.systemId = systemId;
        // createFlags left at zero - see the class comment in OpenXRSession.hpp.

        CheckXrResult(instance, xrCreateSession(instance, &createInfo, &m_session), "xrCreateSession");
    }

    OpenXRSession::~OpenXRSession()
    {
        if (m_session != XR_NULL_HANDLE)
        {
            xrDestroySession(m_session);
            m_session = XR_NULL_HANDLE;
        }
    }

    void OpenXRSession::BeginSession(XrViewConfigurationType primaryViewConfigurationType)
    {
        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = primaryViewConfigurationType;
        CheckXrResult(m_instance, xrBeginSession(m_session, &beginInfo), "xrBeginSession");
        m_running = true;
    }

    void OpenXRSession::EndSession()
    {
        CheckXrResult(m_instance, xrEndSession(m_session), "xrEndSession");
        m_running = false;
    }

    void OpenXRSession::RequestExit()
    {
        CheckXrResult(m_instance, xrRequestExitSession(m_session), "xrRequestExitSession");
    }
}
