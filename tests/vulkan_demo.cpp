// Manual M8A validation demo — NOT part of the automated CTest suite,
// since it requires a real Vulkan-capable GPU and driver, which CI/
// headless systems may lack. Built by CMake but deliberately not
// registered with add_test. Run it manually.
//
// Proves Vulkan bring-up end to end: instance -> physical device
// selection -> logical device + graphics queue -> clean shutdown. Does
// NOT create a window, surface, or swapchain — M8A is bring-up only,
// see docs/ROADMAP.md.
//
// This demo reaches directly into Rendering's private src/vulkan/
// implementation (not through any public Rendering API) because no
// public API exposes Vulkan bring-up yet, by design — see
// docs/ARCHITECTURE.md, "RenderDevice Relationship" (Option B).

#include "AREngine/Core/Core.hpp"

#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanInstance.hpp"
#include "vulkan/VulkanPhysicalDevice.hpp"
#include "vulkan/VulkanVersion.hpp"

#include <format>

int main()
{
    using namespace AREngine::Rendering::Vulkan;

    AR_LOG_INFO(std::format("AREngine Vulkan bring-up demo - targeting Vulkan API {}",
                             FormatVulkanVersion(kTargetApiVersion)));

    VulkanInstance instance;
    AR_LOG_INFO(instance.IsValidationEnabled()
                    ? "Validation layer enabled (VK_LAYER_KHRONOS_validation)"
                    : "Validation layer NOT enabled (unavailable, or a Release build)");

    const SelectedPhysicalDevice physicalDevice = SelectPhysicalDevice(instance.Get());
    AR_LOG_INFO(std::format("Selected GPU: {} ({})",
                             physicalDevice.properties.deviceName,
                             PhysicalDeviceTypeToString(physicalDevice.properties.deviceType)));
    AR_LOG_INFO(std::format("Device Vulkan API version: {}",
                             FormatVulkanVersion(physicalDevice.properties.apiVersion)));
    AR_LOG_INFO(std::format("Selected graphics queue family index: {}",
                             physicalDevice.graphicsQueueFamilyIndex));

    VulkanDevice device(physicalDevice.device, physicalDevice.graphicsQueueFamilyIndex);
    AR_LOG_INFO("Logical device created; graphics queue retrieved");

    AR_LOG_INFO("Vulkan bring-up complete - shutting down");
    return 0;

    // `device` and `instance` are destroyed here, automatically, in
    // reverse construction order: device first, then instance (whose
    // own destructor destroys its debug messenger before the
    // VkInstance itself) — see docs/ARCHITECTURE.md, "Exact Destruction
    // Order".
}
