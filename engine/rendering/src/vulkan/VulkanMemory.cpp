#include "VulkanMemory.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Rendering::Vulkan
{
    std::uint32_t FindMemoryType(
        const VkPhysicalDeviceMemoryProperties& memoryProperties,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags requiredProperties)
    {
        for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            const bool typeAllowed = (typeFilter & (1u << i)) != 0;
            const bool hasProperties =
                (memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties;

            if (typeAllowed && hasProperties)
            {
                return i;
            }
        }

        AR_ASSERT_MSG(false, "No Vulkan memory type satisfies both the type filter and required property flags");
        return 0;
    }
}
