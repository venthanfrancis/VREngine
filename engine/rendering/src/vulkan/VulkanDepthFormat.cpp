#include "VulkanDepthFormat.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Rendering::Vulkan
{
    VkFormat SelectDepthFormat(const std::vector<DepthFormatCandidate>& candidates)
    {
        for (const DepthFormatCandidate& candidate : candidates)
        {
            if ((candidate.properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
            {
                return candidate.format;
            }
        }

        AR_ASSERT_MSG(false, "None of the candidate depth formats support VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT");
        return VK_FORMAT_UNDEFINED;
    }

    VkFormat FindSupportedDepthFormat(VkPhysicalDevice physicalDevice)
    {
        const std::vector<VkFormat> preferredOrder = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };

        std::vector<DepthFormatCandidate> candidates;
        candidates.reserve(preferredOrder.size());
        for (VkFormat format : preferredOrder)
        {
            DepthFormatCandidate candidate;
            candidate.format = format;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &candidate.properties);
            candidates.push_back(candidate);
        }

        return SelectDepthFormat(candidates);
    }
}
