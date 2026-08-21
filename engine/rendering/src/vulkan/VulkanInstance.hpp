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
    // No surface extensions are enabled — M8A has no window/surface to
    // present to yet (see docs/ARCHITECTURE.md, "What's Deferred to
    // M8B+"). Not copyable or movable: exactly one VkInstance per
    // VulkanInstance, destroyed exactly once, by this object alone.
    class VulkanInstance
    {
    public:
        VulkanInstance();
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;
        VulkanInstance(VulkanInstance&&) = delete;
        VulkanInstance& operator=(VulkanInstance&&) = delete;

        [[nodiscard]] VkInstance Get() const { return m_instance; }
        [[nodiscard]] bool IsValidationEnabled() const { return m_debugMessenger != VK_NULL_HANDLE; }

    private:
        void SetUpDebugMessenger();

        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    };
}
