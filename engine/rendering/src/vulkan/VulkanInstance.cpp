// VK_KHR_win32_surface's extension-name macro (used below, to enable it
// by name rather than a hand-typed string literal) only exists once
// VK_USE_PLATFORM_WIN32_KHR is defined before vulkan.h is first
// included - which in turn requires HWND/HINSTANCE to already be
// known, hence <Windows.h> here. This makes this one translation unit
// Windows-specific, same as WindowsWindow.cpp; nothing it declares is
// visible outside this file. See docs/ARCHITECTURE.md, "Instance
// Extensions (M8B)".
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR

#include <Windows.h>

#include "VulkanInstance.hpp"

#include "VulkanResult.hpp"
#include "VulkanVersion.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

#include <cstring>
#include <format>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    namespace
    {
        constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

        bool IsValidationLayerAvailable()
        {
            std::uint32_t layerCount = 0;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> layers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

            for (const VkLayerProperties& layer : layers)
            {
                if (std::strcmp(layer.layerName, kValidationLayerName) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsInstanceExtensionAvailable(const std::vector<VkExtensionProperties>& available, const char* name)
        {
            for (const VkExtensionProperties& extension : available)
            {
                if (std::strcmp(extension.extensionName, name) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
            void* userData)
        {
            (void)type;
            (void)userData;

            const std::string message = std::format("[Vulkan] {}", callbackData->pMessage);

            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                AR_LOG_ERROR(message);
            }
            else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                AR_LOG_WARNING(message);
            }
            else
            {
                AR_LOG_INFO(message);
            }

            // Returning VK_FALSE means "do not abort the call that
            // triggered this" — the standard, recommended behavior;
            // returning VK_TRUE is reserved for validation-layer
            // testing, not normal use.
            return VK_FALSE;
        }
    }

    VulkanInstance::VulkanInstance(bool enablePresentationExtensions)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "AREngine Vulkan Demo";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        appInfo.pEngineName = "AREngine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        appInfo.apiVersion = kTargetApiVersion;

        std::vector<const char*> enabledLayers;
        std::vector<const char*> enabledExtensions;

        // Validation is requested in debug builds only — the same
        // NDEBUG-gated pattern Core::Assert already established. Even
        // when requested, it is only actually enabled if the layer is
        // present; if not, bring-up continues without it rather than
        // failing outright. See docs/ARCHITECTURE.md, "Validation
        // Behavior".
#if !defined(NDEBUG)
        const bool validationRequested = true;
#else
        const bool validationRequested = false;
#endif

        bool validationAvailable = false;
        if (validationRequested)
        {
            validationAvailable = IsValidationLayerAvailable();
            if (validationAvailable)
            {
                enabledLayers.push_back(kValidationLayerName);
                enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else
            {
                AR_LOG_WARNING(
                    "Vulkan validation layer (VK_LAYER_KHRONOS_validation) requested but not available "
                    "(Vulkan SDK not installed, or an older SDK) - continuing without it");
            }
        }

        // Presentation extensions are queried, never assumed present -
        // see docs/ARCHITECTURE.md, "Instance Extensions (M8B)". Asserts
        // if requested but unavailable: without them no surface can ever
        // be created, so bring-up cannot usefully continue.
        if (enablePresentationExtensions)
        {
            std::uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

            const bool hasSurface = IsInstanceExtensionAvailable(availableExtensions, VK_KHR_SURFACE_EXTENSION_NAME);
            const bool hasWin32Surface = IsInstanceExtensionAvailable(availableExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
            AR_ASSERT_MSG(hasSurface && hasWin32Surface,
                "Presentation requested but VK_KHR_surface/VK_KHR_win32_surface are not available on this Vulkan implementation");

            enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
            enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
            m_presentationExtensionsEnabled = true;
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();

        const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        CheckVkResult(result, "vkCreateInstance");

        if (validationAvailable)
        {
            SetUpDebugMessenger();
        }
    }

    VulkanInstance::~VulkanInstance()
    {
        // Debug messenger before instance: it depends on the instance
        // and must not outlive it — see docs/ARCHITECTURE.md, "Exact
        // Destruction Order".
        if (m_debugMessenger != VK_NULL_HANDLE)
        {
            auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyFn != nullptr)
            {
                destroyFn(m_instance, m_debugMessenger, nullptr);
            }
            m_debugMessenger = VK_NULL_HANDLE;
        }

        if (m_instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }
    }

    void VulkanInstance::SetUpDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = &DebugMessengerCallback;

        // VK_EXT_debug_utils is an extension, so its functions aren't
        // in the loader's static import table — they have to be looked
        // up at runtime via vkGetInstanceProcAddr.
        auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

        if (createFn == nullptr)
        {
            AR_LOG_WARNING("vkCreateDebugUtilsMessengerEXT not available - continuing without a debug messenger");
            return;
        }

        const VkResult result = createFn(m_instance, &createInfo, nullptr, &m_debugMessenger);
        CheckVkResult(result, "vkCreateDebugUtilsMessengerEXT");
    }
}
