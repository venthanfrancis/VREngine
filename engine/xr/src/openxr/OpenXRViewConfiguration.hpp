#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: view configuration (how many
// views, their recommended/max dimensions and sample counts) is a
// pure OpenXR concept, independent of which graphics API eventually
// backs the session. See docs/ARCHITECTURE.md, "Selected Primary View
// Configuration (M9D)".

#include <openxr/openxr.h>

#include <optional>
#include <vector>

namespace AREngine::XR::OpenXR
{
    // Makes a real xrEnumerateViewConfigurations call - not
    // unit-tested, only exercised by the manual session demo.
    [[nodiscard]] std::vector<XrViewConfigurationType> EnumerateViewConfigurationTypes(XrInstance instance, XrSystemId systemId);

    // Pure logic over already-queried data - directly unit-testable.
    [[nodiscard]] bool IsViewConfigurationTypeSupported(
        const std::vector<XrViewConfigurationType>& supported, XrViewConfigurationType type);

    // Prefers XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO - never
    // assumed present. Returns std::nullopt (NOT a silent fallback to
    // MONO or anything else) if stereo is not in `supported` - the
    // caller (the demo) is expected to report that clearly and stop,
    // per the M9D brief, the same "report clearly, do not invent a
    // fallback" discipline M9C already applied to
    // XR_KHR_vulkan_enable2. Pure logic, directly unit-testable.
    [[nodiscard]] std::optional<XrViewConfigurationType> SelectPrimaryViewConfigurationType(
        const std::vector<XrViewConfigurationType>& supported);

    // Makes a real xrGetViewConfigurationProperties call - not
    // unit-tested.
    [[nodiscard]] XrViewConfigurationProperties GetViewConfigurationProperties(
        XrInstance instance, XrSystemId systemId, XrViewConfigurationType type);

    // Makes a real xrEnumerateViewConfigurationViews call - not
    // unit-tested. Does NOT allocate any Vulkan/swapchain image from
    // the result - purely diagnostic in M9D, see
    // docs/ARCHITECTURE.md, "View Configuration Views (M9D)".
    [[nodiscard]] std::vector<XrViewConfigurationView> EnumerateViewConfigurationViews(
        XrInstance instance, XrSystemId systemId, XrViewConfigurationType type);
}
