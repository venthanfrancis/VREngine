#include "VulkanVersion.hpp"

#include <format>

namespace AREngine::Rendering::Vulkan
{
    std::string FormatVulkanVersion(std::uint32_t packedVersion)
    {
        const VulkanVersionParts parts = DecodeVulkanVersion(packedVersion);
        return std::format("{}.{}.{}", parts.major, parts.minor, parts.patch);
    }
}
