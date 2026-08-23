#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // One depth format candidate paired with its already-queried
    // format properties (via vkGetPhysicalDeviceFormatProperties).
    struct DepthFormatCandidate
    {
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkFormatProperties properties{};
    };

    // Picks the first candidate whose optimal-tiling features include
    // VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT. Pure logic over
    // already-queried data — no Vulkan calls, directly unit-testable
    // with synthetic VkFormatProperties. Asserts if none of the
    // candidates qualify. See docs/ARCHITECTURE.md, "Depth Format
    // Selection (M8F)".
    [[nodiscard]] VkFormat SelectDepthFormat(const std::vector<DepthFormatCandidate>& candidates);

    // Queries AREngine's preferred depth format order —
    // VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
    // VK_FORMAT_D24_UNORM_S8_UINT — against `physicalDevice`'s real
    // format support, and returns the first one SelectDepthFormat
    // accepts. Makes real Vulkan calls — not unit-tested, only
    // exercised by the manual presentation demo. M8F only requires
    // depth capability; a candidate with a stencil component is fine
    // to select (none of these three are used for stencil operations
    // either way), but stencil is never a selection requirement.
    [[nodiscard]] VkFormat FindSupportedDepthFormat(VkPhysicalDevice physicalDevice);
}
