#include "OpenXRVulkanGraphicsBinding.hpp"

#include "OpenXRResult.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

#include <cstring>
#include <format>

namespace AREngine::XR::OpenXR
{
    namespace
    {
        constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

        // AREngine's desktop Vulkan target (see Rendering's private
        // VulkanVersion.hpp, kTargetApiVersion). Redeclared here rather
        // than included from Rendering: this file must not depend on
        // Rendering's private headers (see the placement reasoning in
        // OpenXRVulkanGraphicsBinding.hpp), and a single version
        // constant is a far smaller duplication cost than that
        // dependency would be. Used only as the *preferred* version -
        // SelectVulkanApiVersion (OpenXRVulkanRequirements.hpp) falls
        // back to the runtime's own minimum if this falls outside its
        // supported range; see docs/ARCHITECTURE.md, "Vulkan Version
        // Selection (M9C)".
        constexpr std::uint32_t kPreferredVulkanApiVersion = VK_API_VERSION_1_2;

        // Small, self-contained CheckVkResult-equivalent - deliberately
        // not Rendering's private VulkanResult.hpp (same
        // no-cross-module-coupling reasoning as everywhere else in this
        // file). Fatal on failure: by the point this is called, Vulkan
        // bring-up failures are genuinely unexpected, matching the
        // policy Rendering's own CheckVkResult already applies.
        void CheckVkResultHere(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
            {
                const std::string message = std::format("{} failed: VkResult({})", operation, static_cast<int>(result));
                AR_LOG_ERROR(message);
                AR_ASSERT_MSG(false, message.c_str());
            }
        }

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

        // Routes validation output through AREngine's existing logging
        // (AR_LOG_ERROR/WARNING/INFO), same as Rendering's Vulkan
        // backend already does for the desktop path - see
        // docs/ARCHITECTURE.md, "Vulkan Validation (M9C)". A fresh copy
        // of VulkanInstance.cpp's callback, not a shared one, for the
        // same decoupling reasoning as everywhere else in this file.
        VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
            void* userData)
        {
            (void)type;
            (void)userData;

            const std::string message = std::format("[OpenXR/Vulkan] {}", callbackData->pMessage);

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
            return VK_FALSE;
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

    OpenXRVulkanGraphicsBinding::OpenXRVulkanGraphicsBinding(XrInstance instance, XrSystemId systemId)
    {
        // --- Function pointers (see docs/ARCHITECTURE.md, "Function
        // Pointer Loading (M9C)") ---
        const std::optional<OpenXRVulkanFunctions> functions = LoadOpenXRVulkanFunctions(instance);
        AR_ASSERT_MSG(functions.has_value(),
            "XR_KHR_vulkan_enable2 was enabled on this XrInstance but one or more of its functions "
            "could not be loaded via xrGetInstanceProcAddr - this should not happen for an enabled extension");

        // --- Vulkan graphics requirements (see docs/ARCHITECTURE.md,
        // "Vulkan Graphics Requirements (M9C)") ---
        XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        CheckXrResult(instance,
            functions->getVulkanGraphicsRequirements2KHR(instance, systemId, &requirements),
            "xrGetVulkanGraphicsRequirements2KHR");
        m_supportedVersionRange = DecodeVulkanVersionRange(requirements.minApiVersionSupported, requirements.maxApiVersionSupported);
        m_selectedVulkanApiVersion = SelectVulkanApiVersion(kPreferredVulkanApiVersion, m_supportedVersionRange);

        // --- XR-compatible VkInstance (see docs/ARCHITECTURE.md,
        // "XR-Controlled VkInstance Creation (M9C)") ---
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "AREngine OpenXR/Vulkan Demo";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        appInfo.pEngineName = "AREngine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        appInfo.apiVersion = m_selectedVulkanApiVersion;

        std::vector<const char*> enabledLayers;
        std::vector<const char*> enabledExtensions;

        // Validation preserved for the XR-created instance too - see
        // docs/ARCHITECTURE.md, "Vulkan Validation (M9C)": OpenXR
        // participating in instance creation is not a reason to lose
        // it. Same debug-build-only gating as Rendering's desktop
        // VulkanInstance.
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
                    "Vulkan validation layer (VK_LAYER_KHRONOS_validation) requested but not available for the "
                    "OpenXR/Vulkan instance - continuing without it");
            }
        }
        // No VK_KHR_surface/VK_KHR_win32_surface here - see
        // docs/ARCHITECTURE.md, "Why the Desktop VkSurfaceKHR/Swapchain
        // Is Absent (M9C)": there is no Windows presentation surface in
        // the XR path at all, OpenXR owns its own swapchains later.

        VkInstanceCreateInfo vulkanInstanceCreateInfo{};
        vulkanInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vulkanInstanceCreateInfo.pApplicationInfo = &appInfo;
        vulkanInstanceCreateInfo.enabledLayerCount = static_cast<std::uint32_t>(enabledLayers.size());
        vulkanInstanceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
        vulkanInstanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
        vulkanInstanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

        XrVulkanInstanceCreateInfoKHR xrInstanceCreateInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        xrInstanceCreateInfo.systemId = systemId;
        xrInstanceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        xrInstanceCreateInfo.vulkanCreateInfo = &vulkanInstanceCreateInfo;
        xrInstanceCreateInfo.vulkanAllocator = nullptr;

        VkResult vkInstanceResult = VK_SUCCESS;
        CheckXrResult(instance,
            functions->createVulkanInstanceKHR(instance, &xrInstanceCreateInfo, &m_bindingData.instance, &vkInstanceResult),
            "xrCreateVulkanInstanceKHR");
        CheckVkResultHere(vkInstanceResult, "xrCreateVulkanInstanceKHR's internal vkCreateInstance");

        if (validationAvailable)
        {
            VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
            debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                                      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                                                      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                                      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugMessengerCreateInfo.pfnUserCallback = &DebugMessengerCallback;

            auto createDebugMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_bindingData.instance, "vkCreateDebugUtilsMessengerEXT"));
            if (createDebugMessenger != nullptr)
            {
                CheckVkResultHere(
                    createDebugMessenger(m_bindingData.instance, &debugMessengerCreateInfo, nullptr, &m_debugMessenger),
                    "vkCreateDebugUtilsMessengerEXT");
            }
            else
            {
                AR_LOG_WARNING("vkCreateDebugUtilsMessengerEXT not available on the OpenXR/Vulkan instance - "
                                "continuing without a debug messenger");
            }
        }

        // --- XR-selected VkPhysicalDevice (see docs/ARCHITECTURE.md,
        // "XR-Controlled Physical Device Selection (M9C)") - authoritative,
        // NOT AREngine's own desktop ranking algorithm. ---
        XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        deviceGetInfo.systemId = systemId;
        deviceGetInfo.vulkanInstance = m_bindingData.instance;
        CheckXrResult(instance,
            functions->getVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &m_bindingData.physicalDevice),
            "xrGetVulkanGraphicsDevice2KHR");

        vkGetPhysicalDeviceProperties(m_bindingData.physicalDevice, &m_physicalDeviceProperties);

        // --- Graphics queue family (see docs/ARCHITECTURE.md, "Queue
        // Family Selection (M9C)") - no VkSurfaceKHR/presentation
        // support involved; this XR path has no Windows surface at
        // all. ---
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_bindingData.physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_bindingData.physicalDevice, &queueFamilyCount, queueFamilies.data());

        const std::optional<std::uint32_t> graphicsQueueFamily = FindGraphicsQueueFamily(queueFamilies);
        AR_ASSERT_MSG(graphicsQueueFamily.has_value(),
            "OpenXR-selected VkPhysicalDevice has no graphics-capable queue family - should not happen on any real GPU");
        m_bindingData.queueFamilyIndex = *graphicsQueueFamily;
        m_bindingData.queueIndex = 0; // no concrete reason to use anything else - see docs/ARCHITECTURE.md, "Queue Family Selection (M9C)"

        // --- XR-compatible VkDevice (see docs/ARCHITECTURE.md,
        // "XR-Controlled VkDevice Creation (M9C)") ---
        constexpr float kQueuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = m_bindingData.queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &kQueuePriority;

        VkDeviceCreateInfo vulkanDeviceCreateInfo{};
        vulkanDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vulkanDeviceCreateInfo.queueCreateInfoCount = 1;
        vulkanDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        // No device extensions (deliberately no VK_KHR_swapchain - see
        // docs/ARCHITECTURE.md, "Why the Desktop VkSurfaceKHR/Swapchain
        // Is Absent (M9C)") and no pEnabledFeatures (no optional GPU
        // feature is needed yet - nothing speculative).

        XrVulkanDeviceCreateInfoKHR xrDeviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        xrDeviceCreateInfo.systemId = systemId;
        xrDeviceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        xrDeviceCreateInfo.vulkanPhysicalDevice = m_bindingData.physicalDevice;
        xrDeviceCreateInfo.vulkanCreateInfo = &vulkanDeviceCreateInfo;
        xrDeviceCreateInfo.vulkanAllocator = nullptr;

        VkResult vkDeviceResult = VK_SUCCESS;
        CheckXrResult(instance,
            functions->createVulkanDeviceKHR(instance, &xrDeviceCreateInfo, &m_bindingData.device, &vkDeviceResult),
            "xrCreateVulkanDeviceKHR");
        CheckVkResultHere(vkDeviceResult, "xrCreateVulkanDeviceKHR's internal vkCreateDevice");

        // VkQueue itself is not part of XrGraphicsBindingVulkan2KHR
        // (OpenXR re-derives it internally from queueFamilyIndex/
        // queueIndex when it needs to) - retrieved here anyway, and
        // kept on this object (not VulkanGraphicsBindingData, which
        // stays an exact mirror of the binding struct's real fields),
        // as concrete proof this queue actually exists and is
        // retrievable, not just a plausible-looking index.
        vkGetDeviceQueue(m_bindingData.device, m_bindingData.queueFamilyIndex, m_bindingData.queueIndex, &m_queue);
    }

    OpenXRVulkanGraphicsBinding::~OpenXRVulkanGraphicsBinding()
    {
        // Debug messenger, then device, then instance - see
        // docs/ARCHITECTURE.md, "Ownership / Destruction Order (M9C)".
        // Neither the XrInstance/XrSystemId this object borrowed nor
        // the VkPhysicalDevice/VkQueue it never owned are touched here.
        if (m_debugMessenger != VK_NULL_HANDLE)
        {
            auto destroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_bindingData.instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugMessenger != nullptr)
            {
                destroyDebugMessenger(m_bindingData.instance, m_debugMessenger, nullptr);
            }
            m_debugMessenger = VK_NULL_HANDLE;
        }
        if (m_bindingData.device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_bindingData.device, nullptr);
            m_bindingData.device = VK_NULL_HANDLE;
        }
        if (m_bindingData.instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_bindingData.instance, nullptr);
            m_bindingData.instance = VK_NULL_HANDLE;
        }
    }
}
