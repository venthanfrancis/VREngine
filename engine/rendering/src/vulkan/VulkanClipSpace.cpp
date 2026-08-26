#include "VulkanClipSpace.hpp"

namespace AREngine::Rendering::Vulkan
{
    AREngine::Core::Math::Mat4 ApplyVulkanYFlip(AREngine::Core::Math::Mat4 projection)
    {
        // Negates every term that contributes to clip.y (the whole of
        // row 1: m10/m11/m12/m13), not just m11 - see this function's
        // own header comment for why. Every projection built before
        // M9G was symmetric, so m10/m12/m13 were always already 0 and
        // negating only m11 happened to be correct; M9G's real,
        // asymmetric OpenXR-FOV-driven projections
        // (PerspectiveOffCenterRH_ZO) produce a genuinely nonzero m12,
        // which negating only m11 would leave un-flipped - a real bug,
        // found before it ever shipped. Backward-compatible by
        // construction: for every symmetric projection, m10/m12/m13
        // are already 0, so negating them changes nothing - this
        // produces bit-for-bit identical output to the old
        // single-element version for every desktop call site. See
        // docs/ARCHITECTURE.md, "ApplyVulkanYFlip Row-1 Fix (M9G)".
        projection.Set(1, 0, -projection.At(1, 0));
        projection.Set(1, 1, -projection.At(1, 1));
        projection.Set(1, 2, -projection.At(1, 2));
        projection.Set(1, 3, -projection.At(1, 3));
        return projection;
    }
}
