#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "AREngine/Platform/NativeWindowHandle.hpp"

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns a VkSurfaceKHR created for an AREngine Platform::Window via
    // its NativeWindowHandle. This is the one place Rendering's Vulkan
    // backend touches Platform - isolated to this single file, not the
    // generic RenderDevice API (see docs/ARCHITECTURE.md, "Platform
    // Dependency Isolation (M8B)"). Platform itself never sees or owns
    // a VkSurfaceKHR - Vulkan Rendering owns every Vulkan object it
    // creates.
    //
    // Windows-only today: the .cpp asserts if handle.platform isn't
    // NativeWindowPlatform::Windows, since AREngine has no other
    // platform's Window implementation yet.
    //
    // Must be destroyed before the VkInstance that created it - see
    // docs/ARCHITECTURE.md, "Exact Destruction Order (M8B)". Not
    // copyable or movable: exactly one VkSurfaceKHR per VulkanSurface,
    // destroyed exactly once, by this object alone.
    class VulkanSurface
    {
    public:
        VulkanSurface(VkInstance instance, const AREngine::Platform::NativeWindowHandle& handle);
        ~VulkanSurface();

        VulkanSurface(const VulkanSurface&) = delete;
        VulkanSurface& operator=(const VulkanSurface&) = delete;
        VulkanSurface(VulkanSurface&&) = delete;
        VulkanSurface& operator=(VulkanSurface&&) = delete;

        [[nodiscard]] VkSurfaceKHR Get() const { return m_surface; }

    private:
        VkInstance m_instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    };
}
