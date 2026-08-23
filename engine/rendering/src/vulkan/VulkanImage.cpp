#include "VulkanImage.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanImageLayoutTransition.hpp"
#include "VulkanMemory.hpp"
#include "VulkanOneTimeCommands.hpp"
#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanImage::VulkanImage(VkPhysicalDevice physicalDevice, VkDevice device,
                              std::uint32_t width, std::uint32_t height, VkFormat format,
                              VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                              VkImageAspectFlags aspectMask)
        : m_device(device)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        CheckVkResult(vkCreateImage(device, &imageInfo, nullptr, &m_image), "vkCreateImage");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, m_image, &memRequirements);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memProperties, memRequirements.memoryTypeBits, properties);

        CheckVkResult(vkAllocateMemory(device, &allocInfo, nullptr, &m_memory), "vkAllocateMemory (image)");
        CheckVkResult(vkBindImageMemory(device, m_image, m_memory, 0), "vkBindImageMemory");

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectMask;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        CheckVkResult(vkCreateImageView(device, &viewInfo, nullptr, &m_view), "vkCreateImageView (texture)");
    }

    VulkanImage::~VulkanImage()
    {
        // View before image before memory: the view depends on the
        // image, and the image is bound to the memory.
        if (m_view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_device, m_view, nullptr);
            m_view = VK_NULL_HANDLE;
        }
        if (m_image != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_device, m_image, nullptr);
            m_image = VK_NULL_HANDLE;
        }
        if (m_memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }
    }

    std::unique_ptr<VulkanImage> CreateTextureFromPixels(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkCommandPool commandPool, VkQueue queue,
        std::uint32_t width, std::uint32_t height, const std::uint8_t* pixels)
    {
        // sRGB: this is color data meant to be seen, not sampled as raw
        // linear numbers (a normal map or other data texture would use
        // an *_UNORM format instead) - see docs/ARCHITECTURE.md,
        // "Texture Format (M8E)".
        constexpr VkFormat kTextureFormat = VK_FORMAT_R8G8B8A8_SRGB;
        const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

        VulkanBuffer staging(physicalDevice, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.CopyDataIn(pixels, imageSize);

        auto image = std::make_unique<VulkanImage>(physicalDevice, device, width, height, kTextureFormat,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);

        TransitionImageLayout(commandBuffer, image->Get(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;   // 0 = tightly packed, same as the pixel buffer's layout
        region.bufferImageHeight = 0; // 0 = tightly packed
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(commandBuffer, staging.Get(), image->Get(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        TransitionImageLayout(commandBuffer, image->Get(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        EndOneTimeCommands(device, commandPool, queue, commandBuffer);

        return image;

        // `staging` is destroyed here, automatically - same "not kept
        // alive past the upload it exists for" policy as M8D's
        // CreateDeviceLocalBuffer.
    }
}
