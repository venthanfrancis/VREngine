#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    // Finds the index of a memory type that satisfies both:
    //  - `typeFilter`: the bitmask of acceptable memory type indices
    //    (from VkMemoryRequirements::memoryTypeBits for whatever buffer
    //    is being allocated for)
    //  - `requiredProperties`: the property flags that type must have
    //    (e.g. HOST_VISIBLE, HOST_COHERENT, DEVICE_LOCAL — see
    //    docs/ARCHITECTURE.md, "Memory Type Selection (M8D)")
    //
    // Pure logic over an already-queried VkPhysicalDeviceMemoryProperties
    // struct — no Vulkan API calls, so this is directly unit-testable
    // with synthetic data, no GPU required.
    //
    // Asserts if no memory type satisfies both constraints: every
    // combination M8D actually requests (host-visible+coherent for a
    // staging buffer, device-local for the destination) is guaranteed
    // available on any real Vulkan-capable GPU per the spec, so failure
    // here would mean a genuine bug, not a normal "gracefully degrade"
    // case.
    [[nodiscard]] std::uint32_t FindMemoryType(
        const VkPhysicalDeviceMemoryProperties& memoryProperties,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags requiredProperties);
}
