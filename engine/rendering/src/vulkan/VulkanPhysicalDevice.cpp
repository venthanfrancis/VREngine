#include "VulkanPhysicalDevice.hpp"

#include "VulkanSwapchainSupport.hpp"
#include "VulkanVersion.hpp"

#include "AREngine/Core/Assert.hpp"

#include <cstring>
#include <limits>

namespace AREngine::Rendering::Vulkan
{
    std::string PhysicalDeviceTypeToString(VkPhysicalDeviceType type)
    {
        switch (type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual GPU";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:             return "CPU (software)";
            default:                                     return "other";
        }
    }

    std::optional<std::uint32_t> FindGraphicsQueueFamily(const std::vector<VkQueueFamilyProperties>& queueFamilies)
    {
        for (std::size_t i = 0; i < queueFamilies.size(); ++i)
        {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                return static_cast<std::uint32_t>(i);
            }
        }
        return std::nullopt;
    }

    SelectedPhysicalDevice SelectPhysicalDevice(VkInstance instance)
    {
        std::uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        AR_ASSERT_MSG(deviceCount > 0, "No Vulkan-capable physical devices found");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        SelectedPhysicalDevice best;
        int bestRank = std::numeric_limits<int>::max();
        bool found = false;

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);

            if (properties.apiVersion < kTargetApiVersion)
            {
                continue; // doesn't meet AREngine's minimum Vulkan API target
            }

            std::uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            const std::optional<std::uint32_t> graphicsFamily = FindGraphicsQueueFamily(queueFamilies);
            if (!graphicsFamily.has_value())
            {
                continue; // no graphics-capable queue family - not usable
            }

            const int rank = RankPhysicalDeviceType(properties.deviceType);
            if (!found || rank < bestRank)
            {
                best.device = device;
                best.properties = properties;
                best.graphicsQueueFamilyIndex = *graphicsFamily;
                bestRank = rank;
                found = true;
            }
        }

        AR_ASSERT_MSG(found, "No suitable Vulkan physical device found (needs a graphics-capable queue family and API >= target version)");
        return best;
    }

    bool DeviceSupportsSwapchainExtension(VkPhysicalDevice device)
    {
        std::uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

        for (const VkExtensionProperties& extension : extensions)
        {
            if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            {
                return true;
            }
        }
        return false;
    }

    std::optional<std::uint32_t> FindPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface, std::uint32_t queueFamilyCount)
    {
        for (std::uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport == VK_TRUE)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    SelectedPresentableDevice SelectPhysicalDeviceForPresentation(VkInstance instance, VkSurfaceKHR surface)
    {
        std::uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        AR_ASSERT_MSG(deviceCount > 0, "No Vulkan-capable physical devices found");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        SelectedPresentableDevice best;
        int bestRank = std::numeric_limits<int>::max();
        bool found = false;

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);

            if (properties.apiVersion < kTargetApiVersion)
            {
                continue;
            }

            if (!DeviceSupportsSwapchainExtension(device))
            {
                continue;
            }

            std::uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            const std::optional<std::uint32_t> graphicsFamily = FindGraphicsQueueFamily(queueFamilies);
            if (!graphicsFamily.has_value())
            {
                continue;
            }

            const std::optional<std::uint32_t> presentFamily = FindPresentQueueFamily(device, surface, queueFamilyCount);
            if (!presentFamily.has_value())
            {
                continue; // this device can't present to our surface at all
            }

            const SwapchainSupportDetails swapchainSupport = QuerySwapchainSupport(device, surface);
            if (!IsSwapchainSupportAdequate(!swapchainSupport.formats.empty(), !swapchainSupport.presentModes.empty()))
            {
                continue;
            }

            const int rank = RankPhysicalDeviceType(properties.deviceType);
            if (!found || rank < bestRank)
            {
                best.device = device;
                best.properties = properties;
                best.queueFamilies.graphicsFamily = *graphicsFamily;
                best.queueFamilies.presentFamily = *presentFamily;
                bestRank = rank;
                found = true;
            }
        }

        AR_ASSERT_MSG(found,
            "No suitable presentable Vulkan physical device found "
            "(needs graphics + present queue families, VK_KHR_swapchain, and adequate swapchain support)");
        return best;
    }
}
