#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "AREngine/Core/Math/Mat4.hpp"

namespace AREngine::Rendering::Vulkan
{
    // Applies Vulkan's NDC Y-flip to a projection matrix built by
    // Core::Math::PerspectiveRH_ZO (or any other right-handed,
    // zero-to-one-depth projection): negates the Y-scale term, since
    // Vulkan's NDC +Y points down the framebuffer, opposite AREngine's
    // +Y-up world convention (docs/WORLD_CONVENTIONS.md). Without this,
    // AREngine's world-space "up" would render toward the bottom of
    // the screen.
    //
    // This is the ONE place in AREngine's Vulkan backend this
    // correction is applied — not scattered as ad-hoc flips elsewhere,
    // and deliberately not baked into Core's projection helper (which
    // must stay graphics-backend-neutral - see docs/ARCHITECTURE.md,
    // "Core/Vulkan Clip-Space Split"). A negative-height-viewport trick
    // would be an alternative way to achieve the same correction; this
    // engine does not use it, for the same "one explicit place, not
    // two competing mechanisms" reasoning.
    //
    // Pure logic (no Vulkan API calls) — directly unit-testable.
    [[nodiscard]] AREngine::Core::Math::Mat4 ApplyVulkanYFlip(AREngine::Core::Math::Mat4 projection);
}
