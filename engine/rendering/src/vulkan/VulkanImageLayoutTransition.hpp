#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Records a VkImageMemoryBarrier (via vkCmdPipelineBarrier) that
    // transitions a color, single-mip, single-layer image's layout.
    // Deliberately supports exactly the two transitions M8E's texture
    // upload needs - not a generic parameterized barrier builder (M8B's
    // now-deleted VulkanImageBarrier was that shape, for a different
    // purpose that no longer exists - see docs/ARCHITECTURE.md,
    // "Image Layout Transitions (M8E)" for why a narrower helper was
    // written fresh instead of reviving it):
    //
    //  - VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //    (before the buffer-to-image copy)
    //  - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    //    (after the copy, before the fragment shader samples it)
    //
    // Asserts on any other (oldLayout, newLayout) pair - this is not a
    // general-purpose synchronization framework.
    void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                VkImageLayout oldLayout, VkImageLayout newLayout);
}
