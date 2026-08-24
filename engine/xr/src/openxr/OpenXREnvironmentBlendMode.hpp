#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: an environment blend mode is a
// pure OpenXR/compositor concept (how the compositor combines
// rendered content with the real world or a black background) - it
// says nothing about which graphics API backs the session. See
// docs/ARCHITECTURE.md, "Environment Blend Mode Selection (M9E)".

#include <openxr/openxr.h>

#include <optional>
#include <vector>

namespace AREngine::XR::OpenXR
{
    // Makes a real xrEnumerateEnvironmentBlendModes call - not
    // unit-tested, only exercised by the manual frame demo.
    [[nodiscard]] std::vector<XrEnvironmentBlendMode> EnumerateEnvironmentBlendModes(
        XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType);

    // Pure logic over already-queried data - directly unit-testable.
    [[nodiscard]] bool IsEnvironmentBlendModeSupported(
        const std::vector<XrEnvironmentBlendMode>& supported, XrEnvironmentBlendMode mode);

    // Prefers OPAQUE, then ALPHA_BLEND, then ADDITIVE - never assumed
    // present, never hard-coded without checking `supported` first.
    // OPAQUE is preferred because M9E (and AREngine generally, so far)
    // has no real passthrough camera pipeline: an opaque background is
    // the most conservative, universally-correct choice for a runtime
    // that might be a VR headset, a passthrough-capable AR headset, or
    // (as in this milestone's actual test environment) a simulated/null
    // HMD with no real-world view to blend with at all. Returns
    // std::nullopt (not a silent fallback to some other mode) if none
    // of the three are supported - the caller is expected to report
    // that clearly and stop, same "report clearly, do not invent a
    // fallback" discipline as SelectPrimaryViewConfigurationType. Pure
    // logic, directly unit-testable. See docs/ARCHITECTURE.md,
    // "Environment Blend Mode Selection (M9E)".
    [[nodiscard]] std::optional<XrEnvironmentBlendMode> SelectEnvironmentBlendMode(
        const std::vector<XrEnvironmentBlendMode>& supported);

    // Human-readable name, for logging.
    [[nodiscard]] const char* EnvironmentBlendModeToString(XrEnvironmentBlendMode mode);
}
