#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <string>

namespace AREngine::Rendering::Vulkan
{
    // A short, human-readable name for a VkResult (e.g.
    // "VK_ERROR_OUT_OF_HOST_MEMORY"), for logging. Covers the failure
    // codes bring-up code can actually hit; falls back to the numeric
    // value for anything else rather than enumerating all ~40 VkResult
    // values just to be exhaustive — see docs/ARCHITECTURE.md, "Error
    // Handling".
    [[nodiscard]] std::string VkResultToString(VkResult result);

    // Every Vulkan bring-up call in M8A goes through this: logs which
    // operation failed and why (via AR_LOG_ERROR), then asserts. There
    // is no fallback path for a failed vkCreateInstance/vkCreateDevice
    // in bring-up code — no swapchain or renderer exists yet to
    // gracefully degrade to — so treating failure as fatal here is
    // simpler and more honest than a Result<T,E>/exception framework
    // this milestone doesn't need. See docs/ARCHITECTURE.md, "Error
    // Handling".
    void CheckVkResult(VkResult result, const char* operationDescription);
}
