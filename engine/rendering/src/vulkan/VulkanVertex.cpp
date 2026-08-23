#include "VulkanVertex.hpp"

#include <cstddef>

namespace AREngine::Rendering::Vulkan
{
    VkVertexInputBindingDescription GetVertexBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Rendering::MeshVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    std::array<VkVertexInputAttributeDescription, 3> GetVertexAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributes{};

        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Vec3
        attributes[0].offset = offsetof(Rendering::MeshVertex, position);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Rendering::MeshVertex, color);

        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(Rendering::MeshVertex, uv);

        return attributes;
    }
}
