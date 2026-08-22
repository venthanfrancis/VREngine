#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Queries a physical device's swapchain support for a given
    // surface. Makes real Vulkan API calls — not unit-tested, only
    // exercised by the manual presentation demo.
    [[nodiscard]] SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    // A device/surface combination is usable for a swapchain only if it
    // reports at least one supported format AND at least one supported
    // present mode — an empty set of either means "not supported,"
    // per the Vulkan spec. Pure logic, no Vulkan calls — directly
    // unit-testable.
    [[nodiscard]] constexpr bool IsSwapchainSupportAdequate(bool hasFormats, bool hasPresentModes)
    {
        return hasFormats && hasPresentModes;
    }

    // Prefers an SRGB-capable BGRA format when available (the common,
    // well-supported choice for a presented swapchain image), falling
    // back to whatever the first reported format is otherwise — never
    // hard-codes one format with no fallback. Pure logic over an
    // already-queried format list — directly unit-testable. See
    // docs/ARCHITECTURE.md, "Surface Format".
    [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);

    // Always FIFO: guaranteed by the Vulkan spec to be supported by
    // every implementation, and behaves like vsync — the safe, simple
    // choice for M8B (see docs/ARCHITECTURE.md, "Present Mode"; not
    // chasing mailbox/immediate for lower latency yet). Still searches
    // `available` and asserts if FIFO is somehow missing, rather than
    // blindly assuming, since that would indicate a spec violation
    // worth knowing about loudly. Pure logic — directly unit-testable
    // for the found-in-list case.
    [[nodiscard]] VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available);

    // minImageCount + 1, clamped to maxImageCount if the device
    // reports a nonzero maximum (0 means "no maximum"). Not an
    // assumption of triple buffering — just one more than the device's
    // stated minimum, capped at whatever it allows. Pure logic over an
    // already-queried capabilities struct — directly unit-testable.
    // See docs/ARCHITECTURE.md, "Swapchain Image Count".
    [[nodiscard]] std::uint32_t ChooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& capabilities);

    // Uses the surface's currentExtent when the surface dictates one
    // (indicated by width != UINT32_MAX); otherwise clamps the
    // window's client-area size (never the outer window size — see the
    // M2 AdjustWindowRect decision) to the surface's reported min/max
    // extent. Pure logic — directly unit-testable for both branches.
    // See docs/ARCHITECTURE.md, "Swapchain Extent".
    [[nodiscard]] VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                                    std::uint32_t windowWidth,
                                                    std::uint32_t windowHeight);
}
