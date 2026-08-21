#include "VulkanResult.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

#include <format>

namespace AREngine::Rendering::Vulkan
{
    std::string VkResultToString(VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS:                        return "VK_SUCCESS";
            case VK_ERROR_OUT_OF_HOST_MEMORY:        return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED:     return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT:         return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT:     return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT:       return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER:       return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_DEVICE_LOST:               return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_TOO_MANY_OBJECTS:          return "VK_ERROR_TOO_MANY_OBJECTS";
            default:                                 return std::format("VkResult({})", static_cast<int>(result));
        }
    }

    void CheckVkResult(VkResult result, const char* operationDescription)
    {
        if (result != VK_SUCCESS)
        {
            const std::string message = std::format("{} failed: {}", operationDescription, VkResultToString(result));
            AR_LOG_ERROR(message);
            AR_ASSERT_MSG(false, message.c_str());
        }
    }
}
