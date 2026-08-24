#include "OpenXRSessionState.hpp"

#include <format>

namespace AREngine::XR::OpenXR
{
    std::string FormatSessionState(XrSessionState state)
    {
        switch (state)
        {
            case XR_SESSION_STATE_UNKNOWN:      return "XR_SESSION_STATE_UNKNOWN";
            case XR_SESSION_STATE_IDLE:         return "XR_SESSION_STATE_IDLE";
            case XR_SESSION_STATE_READY:        return "XR_SESSION_STATE_READY";
            case XR_SESSION_STATE_SYNCHRONIZED: return "XR_SESSION_STATE_SYNCHRONIZED";
            case XR_SESSION_STATE_VISIBLE:      return "XR_SESSION_STATE_VISIBLE";
            case XR_SESSION_STATE_FOCUSED:      return "XR_SESSION_STATE_FOCUSED";
            case XR_SESSION_STATE_STOPPING:     return "XR_SESSION_STATE_STOPPING";
            case XR_SESSION_STATE_LOSS_PENDING: return "XR_SESSION_STATE_LOSS_PENDING";
            case XR_SESSION_STATE_EXITING:      return "XR_SESSION_STATE_EXITING";
            default:                             return std::format("XR_SESSION_STATE_UNKNOWN({})", static_cast<int>(state));
        }
    }
}
