#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "VulkanQueueFamilies.hpp"

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

    // --- M8B: presentation-aware selection ---
    //
    // Deliberately a separate function from SelectPhysicalDevice above,
    // not a modified/overloaded version of it: M8A's bring-up demo and
    // tests keep working against exactly the function they always used,
    // and this one's extra requirements (a real VkSurfaceKHR, present
    // support, VK_KHR_swapchain) are new, M8B-specific criteria that
    // don't apply to M8A's simpler bring-up. See docs/ARCHITECTURE.md,
    // "Physical Device Selection (M8B)".

    struct SelectedPresentableDevice
    {
        VkPhysicalDevice device = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties properties{};
        QueueFamilyIndices queueFamilies;
    };

    // True if `device` reports support for the VK_KHR_swapchain device
    // extension. Makes a real Vulkan API call — not unit-tested.
    [[nodiscard]] bool DeviceSupportsSwapchainExtension(VkPhysicalDevice device);

    // Finds a queue family that supports presenting to `surface` —
    // which may or may not be the same family FindGraphicsQueueFamily
    // finds; this function does not assume either way. Makes real
    // Vulkan API calls (one vkGetPhysicalDeviceSurfaceSupportKHR call
    // per queue family) — not unit-tested.
    [[nodiscard]] std::optional<std::uint32_t> FindPresentQueueFamily(
        VkPhysicalDevice device, VkSurfaceKHR surface, std::uint32_t queueFamilyCount);

    // Like SelectPhysicalDevice, but additionally requires: a
    // graphics-capable queue family, a (possibly different)
    // present-capable queue family for `surface`, VK_KHR_swapchain
    // support, and adequate swapchain support (at least one surface
    // format and present mode — see VulkanSwapchainSupport.hpp).
    // Asserts if no suitable device is found. Makes real Vulkan API
    // calls — not unit-tested, only exercised by the manual
    // presentation demo.
    [[nodiscard]] SelectedPresentableDevice SelectPhysicalDeviceForPresentation(VkInstance instance, VkSurfaceKHR surface);
}
