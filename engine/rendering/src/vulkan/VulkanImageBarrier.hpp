#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Records a VkImageMemoryBarrier (via vkCmdPipelineBarrier) that
    // transitions `image`'s layout and access. A small helper function,
    // not a class - M8B needs exactly two transitions (UNDEFINED ->
    // TRANSFER_DST_OPTIMAL before the clear, TRANSFER_DST_OPTIMAL ->
    // PRESENT_SRC_KHR after it) and this is the whole of what's needed
    // to express both; see docs/ARCHITECTURE.md, "Clearing (M8B)" for
    // the exact access-mask/stage values the caller passes for each.
    //
    // Always targets the whole image, one color mip/layer (subresource
    // range aspectMask=COLOR_BIT, baseMipLevel=0, levelCount=1,
    // baseArrayLayer=0, layerCount=1) - the only kind of image M8B's
    // swapchain images ever are.
    void RecordImageLayoutTransition(VkCommandBuffer commandBuffer,
                                      VkImage image,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout,
                                      VkAccessFlags srcAccessMask,
                                      VkAccessFlags dstAccessMask,
                                      VkPipelineStageFlags srcStage,
                                      VkPipelineStageFlags dstStage);
}
