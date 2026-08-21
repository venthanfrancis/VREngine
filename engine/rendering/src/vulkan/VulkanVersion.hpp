#pragma once

// Private Vulkan bring-up implementation. Never included from
// Rendering's public headers — see docs/ARCHITECTURE.md, "M8A
// Implementation Notes".

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace AREngine::Rendering::Vulkan
{
    // AREngine's target Vulkan API version: 1.2. Chosen for broad
    // compatibility (released 2020; supported by essentially all
    // Vulkan-capable hardware still in use) over requiring newer
    // optional features nothing in this engine needs yet (dynamic
    // rendering, ray tracing, mesh shaders — all explicitly out of
    // scope for M8A and every near-term milestone after it). See
    // docs/ARCHITECTURE.md, "Chosen Vulkan API Target".
    inline constexpr std::uint32_t kTargetApiVersion = VK_API_VERSION_1_2;

    struct VulkanVersionParts
    {
        std::uint32_t major = 0;
        std::uint32_t minor = 0;
        std::uint32_t patch = 0;
    };

    // Decodes a packed Vulkan API version (as returned by
    // vkEnumerateInstanceVersion, or VkPhysicalDeviceProperties::
    // apiVersion) into its components. Pure bit manipulation over the
    // VK_API_VERSION_* macros — no Vulkan API calls, so this is
    // directly unit-testable without a GPU or even a working Vulkan
    // runtime.
    [[nodiscard]] constexpr VulkanVersionParts DecodeVulkanVersion(std::uint32_t packedVersion)
    {
        return VulkanVersionParts{
            VK_API_VERSION_MAJOR(packedVersion),
            VK_API_VERSION_MINOR(packedVersion),
            VK_API_VERSION_PATCH(packedVersion)
        };
    }

    [[nodiscard]] std::string FormatVulkanVersion(std::uint32_t packedVersion);
}
