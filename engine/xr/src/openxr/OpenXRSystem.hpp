#pragma once

// Private OpenXR bring-up implementation — see OpenXRInstance.hpp.

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // True if `result` specifically means "this form factor genuinely
    // isn't available right now" (e.g. no HMD connected/active) rather
    // than some other, more surprising failure. Pure logic - directly
    // unit-testable with synthetic XrResult values, no loader/runtime/
    // headset required. See docs/ARCHITECTURE.md, "System Selection
    // (M9A)".
    [[nodiscard]] constexpr bool IsFormFactorUnavailable(XrResult result)
    {
        return result == XR_ERROR_FORM_FACTOR_UNAVAILABLE || result == XR_ERROR_FORM_FACTOR_UNSUPPORTED;
    }

    // Result of requesting a head-mounted-display-class XrSystemId.
    // `found` is false whenever `rawResult` is not XR_SUCCESS - callers
    // should inspect `rawResult` (with IsFormFactorUnavailable above) to
    // tell "no HMD connected" apart from a genuinely unexpected failure,
    // and log accordingly, per docs/ARCHITECTURE.md, "Headset Absent
    // Case (M9A)".
    struct SystemRequestResult
    {
        bool found = false;
        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        XrResult rawResult = XR_SUCCESS;
    };

    // Requests an XrSystemId for XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY via
    // xrGetSystem. Does NOT assert or abort on failure - unlike
    // Vulkan's SelectPhysicalDevice (M8A), "no HMD connected" is a
    // completely normal machine state for OpenXR (a runtime can be
    // installed and active with no headset plugged in), not a
    // programmer error. Makes a real OpenXR call - not unit-tested,
    // only exercised by the manual bring-up demo. See
    // docs/ARCHITECTURE.md, "System Selection (M9A)".
    [[nodiscard]] SystemRequestResult TryGetHmdSystem(XrInstance instance);
}
