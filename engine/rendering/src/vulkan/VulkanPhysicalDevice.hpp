#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // Ranks a physical device type for selection purposes — lower is
    // better. Deliberately tiny: prefer a discrete GPU, then an
    // integrated one, then anything else Vulkan reports (virtual GPU,
    // CPU, "other") equally last. No ray tracing/mesh-shader/feature
    // scoring — see docs/ARCHITECTURE.md, "Physical Device Selection".
    //
    // Pure logic, no Vulkan API calls — directly unit-testable with
    // synthetic VkPhysicalDeviceType values, no GPU required.
    [[nodiscard]] constexpr int RankPhysicalDeviceType(VkPhysicalDeviceType type)
    {
        switch (type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 0;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 1;
            default:                                     return 2;
        }
    }

    [[nodiscard]] std::string PhysicalDeviceTypeToString(VkPhysicalDeviceType type);

    // Finds the first queue family that supports graphics operations.
    // Pure logic over already-queried queue family properties — no
    // Vulkan API calls, so this is directly unit-testable with
    // synthetic data, no GPU required. See docs/ARCHITECTURE.md,
    // "Queue Selection".
    [[nodiscard]] std::optional<std::uint32_t> FindGraphicsQueueFamily(
        const std::vector<VkQueueFamilyProperties>& queueFamilies);

    struct SelectedPhysicalDevice
    {
        VkPhysicalDevice device = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties properties{};
        std::uint32_t graphicsQueueFamilyIndex = 0;
    };

    // Enumerates every physical device visible to `instance`, discards
    // any that report an apiVersion below kTargetApiVersion or have no
    // graphics-capable queue family (FindGraphicsQueueFamily), and
    // returns the best of what's left by RankPhysicalDeviceType.
    // Asserts if no suitable device is found. This function makes real
    // Vulkan API calls (vkEnumeratePhysicalDevices, etc.) — it is not
    // unit-tested, only exercised by the manual bring-up demo. See
    // docs/ARCHITECTURE.md, "Physical Device Selection".
    [[nodiscard]] SelectedPhysicalDevice SelectPhysicalDevice(VkInstance instance);
}
