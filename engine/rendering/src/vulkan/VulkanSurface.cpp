// The only .cpp in Rendering's Vulkan backend that needs the real
// Win32 surface-creation API - see VulkanSurface.hpp. Defines
// VK_USE_PLATFORM_WIN32_KHR and includes <Windows.h> itself, same
// pattern as WindowsWindow.cpp; nothing here is visible outside this
// file.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR

#include <Windows.h>

#include "VulkanSurface.hpp"

#include "VulkanResult.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanSurface::VulkanSurface(VkInstance instance, const AREngine::Platform::NativeWindowHandle& handle)
        : m_instance(instance)
    {
        AR_ASSERT_MSG(handle.platform == AREngine::Platform::NativeWindowPlatform::Windows,
            "VulkanSurface only knows how to create a surface for a Windows NativeWindowHandle");

        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = reinterpret_cast<HINSTANCE>(handle.instance);
        createInfo.hwnd = reinterpret_cast<HWND>(handle.window);

        const VkResult result = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &m_surface);
        CheckVkResult(result, "vkCreateWin32SurfaceKHR");
    }

    VulkanSurface::~VulkanSurface()
    {
        if (m_surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
    }
}
