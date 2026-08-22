#include "VulkanOneTimeCommands.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        CheckVkResult(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "vkAllocateCommandBuffers (one-time)");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CheckVkResult(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer (one-time)");

        return commandBuffer;
    }

    void EndOneTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
    {
        CheckVkResult(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer (one-time)");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        CheckVkResult(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit (one-time)");

        // vkQueueWaitIdle rather than a dedicated fence: simpler, and
        // perfectly adequate for M8D's actual usage — a handful of
        // one-shot uploads at startup, not a steady-state per-frame
        // operation where stalling the whole queue would cost real
        // throughput. See docs/ARCHITECTURE.md, "Synchronous Upload
        // Limitation (M8D)".
        CheckVkResult(vkQueueWaitIdle(queue), "vkQueueWaitIdle (one-time)");

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
}
