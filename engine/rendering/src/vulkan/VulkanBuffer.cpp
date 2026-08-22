#include "VulkanBuffer.hpp"

#include "VulkanMemory.hpp"
#include "VulkanOneTimeCommands.hpp"
#include "VulkanResult.hpp"

#include "AREngine/Core/Assert.hpp"

#include <cstring>

namespace AREngine::Rendering::Vulkan
{
    VulkanBuffer::VulkanBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
                                VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
        : m_device(device)
        , m_size(size)
        , m_hostVisible((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        CheckVkResult(vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer), "vkCreateBuffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memProperties, memRequirements.memoryTypeBits, properties);

        CheckVkResult(vkAllocateMemory(device, &allocInfo, nullptr, &m_memory), "vkAllocateMemory");
        CheckVkResult(vkBindBufferMemory(device, m_buffer, m_memory, 0), "vkBindBufferMemory");
    }

    VulkanBuffer::~VulkanBuffer()
    {
        // Buffer before memory: a VkBuffer bound to a VkDeviceMemory
        // must not outlive the memory it's bound to, so unbinding
        // (destroying the buffer) comes first.
        if (m_buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }
        if (m_memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }
    }

    void VulkanBuffer::CopyDataIn(const void* data, VkDeviceSize size)
    {
        AR_ASSERT_MSG(m_hostVisible, "VulkanBuffer::CopyDataIn requires a HOST_VISIBLE buffer");
        AR_ASSERT_MSG(size <= m_size, "VulkanBuffer::CopyDataIn size exceeds the buffer's allocated size");

        void* mapped = nullptr;
        CheckVkResult(vkMapMemory(m_device, m_memory, 0, size, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, data, static_cast<std::size_t>(size));
        vkUnmapMemory(m_device, m_memory);
    }

    std::unique_ptr<VulkanBuffer> CreateDeviceLocalBuffer(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkCommandPool commandPool, VkQueue queue,
        const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
    {
        // HOST_VISIBLE | HOST_COHERENT: the CPU can write directly into
        // this memory, and doesn't need an explicit vkFlushMappedMemoryRanges
        // call for the GPU to see it (COHERENT handles that). See
        // docs/ARCHITECTURE.md, "Memory Type Selection (M8D)".
        VulkanBuffer staging(physicalDevice, device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.CopyDataIn(data, size);

        // DEVICE_LOCAL: the fastest memory for the GPU to read from
        // during rendering, at the cost of the CPU not being able to
        // write into it directly — exactly why the staging buffer above
        // exists. See docs/ARCHITECTURE.md, "Memory Type Selection
        // (M8D)".
        auto destination = std::make_unique<VulkanBuffer>(physicalDevice, device, size,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, commandPool);
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, staging.Get(), destination->Get(), 1, &copyRegion);
        EndOneTimeCommands(device, commandPool, queue, commandBuffer);

        return destination;

        // `staging` is destroyed here, automatically, once this
        // function returns — EndOneTimeCommands already waited for the
        // copy to finish, so the staging buffer's job is done and it is
        // not kept alive any longer than necessary. See
        // docs/ARCHITECTURE.md, "Staging Buffer Behavior (M8D)".
    }
}
