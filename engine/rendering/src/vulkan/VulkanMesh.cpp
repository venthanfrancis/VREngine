#include "VulkanMesh.hpp"

#include "VulkanBuffer.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanMesh::VulkanMesh(VkPhysicalDevice physicalDevice, VkDevice device,
                            VkCommandPool commandPool, VkQueue queue, const MeshData& meshData)
        : m_indexCount(static_cast<std::uint32_t>(meshData.indices.size()))
    {
        AR_ASSERT_MSG(meshData.IsValid(),
            "VulkanMesh: MeshData must be valid (non-empty vertices/indices, every index in range)");

        m_vertexBuffer = CreateDeviceLocalBuffer(
            physicalDevice, device, commandPool, queue,
            meshData.vertices.data(), sizeof(MeshVertex) * meshData.vertices.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        m_indexBuffer = CreateDeviceLocalBuffer(
            physicalDevice, device, commandPool, queue,
            meshData.indices.data(), sizeof(std::uint32_t) * meshData.indices.size(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    // Declared out-of-line (not defaulted in the header) specifically so
    // this translation unit - which includes VulkanBuffer.hpp - is the
    // one that instantiates std::unique_ptr<VulkanBuffer>'s destructor,
    // not every file that merely includes VulkanMesh.hpp (which only
    // forward-declares VulkanBuffer).
    VulkanMesh::~VulkanMesh() = default;

    void VulkanMesh::Bind(VkCommandBuffer commandBuffer) const
    {
        VkBuffer vertexBuffers[] = {m_vertexBuffer->Get()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->Get(), 0, VK_INDEX_TYPE_UINT32);
    }

    void VulkanMesh::Draw(VkCommandBuffer commandBuffer) const
    {
        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
    }

    std::unique_ptr<VulkanMesh> CreateVulkanMesh(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkCommandPool commandPool, VkQueue queue, const MeshData& meshData)
    {
        return std::make_unique<VulkanMesh>(physicalDevice, device, commandPool, queue, meshData);
    }
}
