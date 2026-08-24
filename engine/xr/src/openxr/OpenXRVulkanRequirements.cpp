#include "OpenXRVulkanRequirements.hpp"

#include <format>

namespace AREngine::XR::OpenXR
{
    std::string FormatVkApiVersion(std::uint32_t vkApiVersion)
    {
        return std::format("{}.{}.{}",
            VK_API_VERSION_MAJOR(vkApiVersion), VK_API_VERSION_MINOR(vkApiVersion), VK_API_VERSION_PATCH(vkApiVersion));
    }

    namespace
    {
        // Looks up one extension function by name and casts it to the
        // requested PFN_ type. Returns nullptr (not an assert) on
        // failure - the caller (LoadOpenXRVulkanFunctions) decides what
        // "any one missing" means as a whole.
        template <typename PfnT>
        [[nodiscard]] PfnT LoadFunction(XrInstance instance, const char* name)
        {
            PFN_xrVoidFunction function = nullptr;
            const XrResult result = xrGetInstanceProcAddr(instance, name, &function);
            if (XR_FAILED(result))
            {
                return nullptr;
            }
            return reinterpret_cast<PfnT>(function);
        }
    }

    std::optional<OpenXRVulkanFunctions> LoadOpenXRVulkanFunctions(XrInstance instance)
    {
        OpenXRVulkanFunctions functions;
        functions.getVulkanGraphicsRequirements2KHR =
            LoadFunction<PFN_xrGetVulkanGraphicsRequirements2KHR>(instance, "xrGetVulkanGraphicsRequirements2KHR");
        functions.createVulkanInstanceKHR =
            LoadFunction<PFN_xrCreateVulkanInstanceKHR>(instance, "xrCreateVulkanInstanceKHR");
        functions.getVulkanGraphicsDevice2KHR =
            LoadFunction<PFN_xrGetVulkanGraphicsDevice2KHR>(instance, "xrGetVulkanGraphicsDevice2KHR");
        functions.createVulkanDeviceKHR =
            LoadFunction<PFN_xrCreateVulkanDeviceKHR>(instance, "xrCreateVulkanDeviceKHR");

        if (functions.getVulkanGraphicsRequirements2KHR == nullptr || functions.createVulkanInstanceKHR == nullptr ||
            functions.getVulkanGraphicsDevice2KHR == nullptr || functions.createVulkanDeviceKHR == nullptr)
        {
            return std::nullopt;
        }
        return functions;
    }
}
