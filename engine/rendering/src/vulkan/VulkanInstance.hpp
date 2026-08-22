#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns a VkInstance and, in debug builds when the Khronos
    // validation layer is available, a VK_EXT_debug_utils messenger
    // that routes Vulkan's validation output through AREngine logging.
    // See docs/ARCHITECTURE.md, "Instance Ownership" and "Validation
    // Behavior".
    //
    // `enablePresentationExtensions` defaults to false, preserving
    // M8A's exact behavior (no surface extensions - there was no
    // window/surface to present to yet). Pass true (as the M8B
    // presentation demo does) to additionally request VK_KHR_surface
    // and VK_KHR_win32_surface - queried via
    // vkEnumerateInstanceExtensionProperties first, never assumed
    // present. See docs/ARCHITECTURE.md, "Instance Extensions (M8B)".
    //
    // Not copyable or movable: exactly one VkInstance per
    // VulkanInstance, destroyed exactly once, by this object alone.
    class VulkanInstance
    {
    public:
        explicit VulkanInstance(bool enablePresentationExtensions = false);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;
        VulkanInstance(VulkanInstance&&) = delete;
        VulkanInstance& operator=(VulkanInstance&&) = delete;

        [[nodiscard]] VkInstance Get() const { return m_instance; }
        [[nodiscard]] bool IsValidationEnabled() const { return m_debugMessenger != VK_NULL_HANDLE; }
        [[nodiscard]] bool ArePresentationExtensionsEnabled() const { return m_presentationExtensionsEnabled; }

    private:
        void SetUpDebugMessenger();

        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        bool m_presentationExtensionsEnabled = false;
    };
}
