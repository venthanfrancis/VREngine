#include "VulkanQueueFamilies.hpp"

namespace AREngine::Rendering::Vulkan
{
    std::vector<std::uint32_t> GetUniqueQueueFamilies(const QueueFamilyIndices& indices)
    {
        if (!HasSeparatePresentQueue(indices))
        {
            return {indices.graphicsFamily};
        }
        return {indices.graphicsFamily, indices.presentFamily};
    }
}
