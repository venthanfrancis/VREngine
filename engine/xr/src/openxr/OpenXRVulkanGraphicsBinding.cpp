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
        // No static version cap: the confirmed conflict (see the device-
        // creation block below) was specifically between an
        // app-provided VkPhysicalDeviceVulkan12Features and a runtime-
        // injected VkPhysicalDeviceTimelineSemaphoreFeatures. Since
        // device creation below never chains VkPhysicalDeviceVulkan12Features
        // (or any Features2 wrapper) at all, that conflict cannot occur
        // regardless of the selected API version - so the version this
        // object requests is purely SelectVulkanApiVersion's own
        // capability-driven decision, unmodified. See
        // docs/ARCHITECTURE.md, "Vulkan Device Feature Requirement
        // Discovered in M9D".

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

        // This XR-compatible device is used internally by the OpenXR
        // runtime's own compositor (its shaders are compiled against
        // THIS VkDevice the moment xrCreateSession runs - M9D's own
        // manual validation hit this directly: SteamVR's compositor
        // failed vkCreateShaderModule for a missing geometryShader
        // capability, and separately for a missing
        // shaderOutputViewportIndex/shaderOutputLayer capability).
        // AREngine cannot inspect or control what a third-party
        // compositor's shaders actually need, so - for THIS device
        // only, never the M8 desktop device, which keeps its
        // deliberately minimal M8A-established feature set - every
        // core 1.0 feature the physical device genuinely reports
        // supporting (queried via vkGetPhysicalDeviceFeatures, never
        // assumed) is enabled. Not speculative: nothing is requested
        // beyond what the hardware/driver already advertises. See
        // docs/ARCHITECTURE.md, "Vulkan Device Feature Requirement
        // Discovered in M9D".
        VkPhysicalDeviceFeatures enabledFeatures{};
        vkGetPhysicalDeviceFeatures(m_bindingData.physicalDevice, &enabledFeatures);

        // shaderOutputViewportIndex/shaderOutputLayer are satisfied via
        // the plain VK_EXT_shader_viewport_index_layer device extension
        // - deliberately NOT the Vulkan-1.2-core VkPhysicalDeviceVulkan12Features
        // feature bits, which is what actually caused the confirmed
        // conflict with this runtime's own feature-struct injection
        // (see the comment on `m_selectedVulkanApiVersion`'s computation
        // above). The extension has no feature struct of its own, so it
        // has nothing to conflict with, at any Vulkan version this
        // object might select. Availability is checked, never assumed;
        // on a device that doesn't support it, this proceeds without
        // it - a compositor shader that genuinely needs the capability
        // would then fail on its own terms, which is a property of that
        // compositor/device pairing, not something this narrow fix
        // needs to solve more generally.
        std::uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_bindingData.physicalDevice, nullptr, &deviceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
        vkEnumerateDeviceExtensionProperties(m_bindingData.physicalDevice, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());

        bool timelineSemaphoreExtensionAvailable = false;
        std::vector<const char*> enabledDeviceExtensions;
        for (const VkExtensionProperties& extension : availableDeviceExtensions)
        {
            if (std::strcmp(extension.extensionName, "VK_EXT_shader_viewport_index_layer") == 0)
            {
                enabledDeviceExtensions.push_back("VK_EXT_shader_viewport_index_layer");
            }
            else if (std::strcmp(extension.extensionName, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0)
            {
                timelineSemaphoreExtensionAvailable = true;
            }
        }

        // --- Timeline semaphore device-feature negotiation (M11.2) ---
        // See docs/ARCHITECTURE.md, "Timeline Semaphore Device-Feature
        // Negotiation (M11.2)": Meta XR Simulator's compositor uses a
        // timeline semaphore on this shared VkDevice and fails
        // (VUID-VkSemaphoreTypeCreateInfo-timelineSemaphore-03252) unless
        // the feature is enabled. SteamVR never hit this because its own
        // xrCreateVulkanDeviceKHR unconditionally injects its own
        // VkPhysicalDeviceTimelineSemaphoreFeatures - so this device
        // already had the feature there, just never requested by
        // AREngine. Deliberately the NARROW VkPhysicalDeviceTimelineSemaphoreFeatures
        // struct, never VkPhysicalDeviceVulkan12Features - the M9D
        // conflict was specifically the aggregate struct colliding with
        // that same runtime-injected narrow one (Vulkan forbids
        // supplying both an aggregate promoted-feature struct and one of
        // its individually-promoted structs in the same pNext chain);
        // the narrow struct alone was never actually tried against
        // SteamVR before this milestone. Purely capability-driven - no
        // runtime-name check anywhere in this decision.
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{};
        timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;

        bool physicalDeviceSupportsTimelineSemaphore = false;
        if (m_selectedVulkanApiVersion >= VK_API_VERSION_1_1)
        {
            // vkGetPhysicalDeviceFeatures2 is core Vulkan 1.1 - only
            // safe to call via vkGetInstanceProcAddr against an instance
            // created targeting >= 1.1 (VK_KHR_get_physical_device_properties2
            // is not enabled on this instance, so there is no pre-1.1
            // fallback path; both real runtimes tested select 1.2.0, so
            // this guard is not currently limiting anything observed).
            auto getPhysicalDeviceFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
                vkGetInstanceProcAddr(m_bindingData.instance, "vkGetPhysicalDeviceFeatures2"));
            if (getPhysicalDeviceFeatures2 != nullptr)
            {
                VkPhysicalDeviceFeatures2 queryFeatures2{};
                queryFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                queryFeatures2.pNext = &timelineSemaphoreFeatures;
                getPhysicalDeviceFeatures2(m_bindingData.physicalDevice, &queryFeatures2);
                physicalDeviceSupportsTimelineSemaphore = (timelineSemaphoreFeatures.timelineSemaphore == VK_TRUE);
                // Reset - queryFeatures2 wrote the queried (supported)
                // value; the struct is reused below to REQUEST the
                // feature, not report on it, so it must not retain
                // query-time contents beyond what's explicitly set again.
                timelineSemaphoreFeatures = VkPhysicalDeviceTimelineSemaphoreFeatures{};
                timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
            }
        }

        const TimelineSemaphoreSelection timelineSemaphoreSelection = SelectTimelineSemaphoreSupport(
            m_selectedVulkanApiVersion, physicalDeviceSupportsTimelineSemaphore, timelineSemaphoreExtensionAvailable);

        AR_LOG_INFO(std::format("Timeline semaphore: physical device supports={}, selected Vulkan version={}, "
                                 "extension available={}, will enable={}, requires VK_KHR_timeline_semaphore={}",
            physicalDeviceSupportsTimelineSemaphore, FormatVkApiVersion(m_selectedVulkanApiVersion),
            timelineSemaphoreExtensionAvailable, timelineSemaphoreSelection.enable, timelineSemaphoreSelection.requiresExtension));

        if (timelineSemaphoreSelection.requiresExtension)
        {
            enabledDeviceExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        }

        VkPhysicalDeviceFeatures2 enabledFeatures2{};
        enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        enabledFeatures2.features = enabledFeatures;

        VkDeviceCreateInfo vulkanDeviceCreateInfo{};
        vulkanDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vulkanDeviceCreateInfo.queueCreateInfoCount = 1;
        vulkanDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        vulkanDeviceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledDeviceExtensions.size());
        vulkanDeviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
        // No VK_KHR_swapchain - see docs/ARCHITECTURE.md, "Why the
        // Desktop VkSurfaceKHR/Swapchain Is Absent (M9C)".

        if (timelineSemaphoreSelection.enable)
        {
            // Vulkan forbids supplying both pEnabledFeatures and a
            // VkPhysicalDeviceFeatures2 in pNext - the core-1.0 features
            // move into enabledFeatures2.features instead, unchanged in
            // value, just relocated.
            timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;
            enabledFeatures2.pNext = &timelineSemaphoreFeatures;
            vulkanDeviceCreateInfo.pNext = &enabledFeatures2;
            vulkanDeviceCreateInfo.pEnabledFeatures = nullptr;
        }
        else
        {
            // Unchanged from before M11.2: plain core-1.0 features via
            // pEnabledFeatures, no pNext feature chain at all.
            vulkanDeviceCreateInfo.pEnabledFeatures = &enabledFeatures;
        }

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
