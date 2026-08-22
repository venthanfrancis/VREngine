#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Allocates one primary command buffer from `commandPool` and begins
    // it with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT. Pair with
    // EndOneTimeCommands below. Used for M8D's staging-buffer upload
    // copy — see docs/ARCHITECTURE.md, "Upload Strategy (M8D)". Not a
    // general-purpose command-recording system: no async transfer
    // scheduling, no dedicated transfer queue, one command buffer at a
    // time.
    [[nodiscard]] VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool commandPool);

    // Ends `commandBuffer`, submits it to `queue`, waits for `queue` to
    // go idle (this makes the whole one-time-commands path synchronous
    // — see docs/ARCHITECTURE.md, "Synchronous Upload Limitation
    // (M8D)"), then frees the command buffer back to `commandPool`.
    void EndOneTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);
}
