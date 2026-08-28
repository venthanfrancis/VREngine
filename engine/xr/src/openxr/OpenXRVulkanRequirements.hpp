#pragma once

// Private OpenXR/Vulkan integration boundary — see
// OpenXRVulkanGraphicsBinding.hpp for why this code exists and where
// it lives architecturally (docs/ARCHITECTURE.md, "XR-Vulkan
// Integration Placement (M9C)").
//
// vulkan.h must be included before openxr_platform.h, with
// XR_USE_GRAPHICS_API_VULKAN defined first - that's what unlocks the
// Vulkan-flavored OpenXR structs/typedefs (XrVulkanInstanceCreateInfoKHR,
// PFN_xrCreateVulkanInstanceKHR, etc.). Every file in this integration
// boundary repeats this same three-line sequence rather than relying on
// include-order luck from whatever happens to be included first
// elsewhere in a translation unit.
#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

#include <cstdint>
#include <optional>
#include <string>

namespace AREngine::XR::OpenXR
{
    // IMPORTANT: XrVersion and Vulkan's VkVersion use DIFFERENT bit
    // layouts. XrVersion (used by XrGraphicsRequirementsVulkan2KHR's
    // min/maxApiVersionSupported) packs major/minor/patch as 16/16/32
    // bits (see XR_VERSION_MAJOR/MINOR/PATCH, OpenXRVersion.hpp).
    // Vulkan's VkVersion (VK_API_VERSION_1_2, etc.) packs them as
    // 7/10/12 bits via VK_MAKE_API_VERSION. These are NOT bit-
    // reinterpretable - reinterpreting one as the other silently
    // produces a nonsense version number. The runtime encodes these
    // fields using OpenXR's own XR_MAKE_VERSION macro (that is what the
    // "XrVersion" type means here - a generic OpenXR version-packing
    // convention applied to represent a Vulkan version value), so
    // decoding via XR_VERSION_MAJOR/MINOR correctly recovers the
    // intended Vulkan major/minor - it must then be RE-ENCODED with
    // Vulkan's own VK_MAKE_API_VERSION before it means anything to a
    // real Vulkan call. See docs/ARCHITECTURE.md, "Vulkan Graphics
    // Requirements (M9C)".
    //
    // Patch is intentionally dropped: Vulkan API version comparisons
    // and VkApplicationInfo::apiVersion requests are conventionally
    // major.minor-only (every VK_API_VERSION_1_x constant already
    // hard-codes patch as 0) - carrying a patch number through this
    // conversion would imply a precision neither side actually uses.
    //
    // Pure bit manipulation, no API calls - directly unit-testable.
    [[nodiscard]] constexpr std::uint32_t XrVersionToVkApiVersion(XrVersion xrVersion)
    {
        return VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(xrVersion), XR_VERSION_MINOR(xrVersion), 0);
    }

    // The Vulkan API version range an OpenXR runtime supports, already
    // converted into Vulkan's own version encoding (see
    // XrVersionToVkApiVersion above) so every other function in this
    // file works in one consistent unit.
    struct VulkanVersionRange
    {
        std::uint32_t minVkApiVersion = 0;
        std::uint32_t maxVkApiVersion = 0;
    };

    [[nodiscard]] constexpr VulkanVersionRange DecodeVulkanVersionRange(XrVersion minSupported, XrVersion maxSupported)
    {
        return VulkanVersionRange{XrVersionToVkApiVersion(minSupported), XrVersionToVkApiVersion(maxSupported)};
    }

    // Pure logic, no API calls - directly unit-testable with synthetic
    // ranges. Comparison is major.minor-only (see above), which is
    // exactly what VK_MAKE_API_VERSION(0, major, minor, 0) already
    // encodes - patch stays 0 on every value this file produces.
    [[nodiscard]] constexpr bool IsVulkanApiVersionSupported(std::uint32_t candidateVkApiVersion, const VulkanVersionRange& range)
    {
        return candidateVkApiVersion >= range.minVkApiVersion && candidateVkApiVersion <= range.maxVkApiVersion;
    }

    // Picks the Vulkan API version AREngine's XR path should request:
    // `preferredVkApiVersion` (AREngine's desktop target - see
    // docs/ARCHITECTURE.md, "Vulkan Version Selection (M9C)" for why
    // this file does not simply #include the desktop's private
    // VulkanVersion.hpp to get it) if the runtime's reported range
    // supports it, otherwise the runtime's own minimum - never
    // silently requesting a version outside what the runtime declared
    // it supports. Pure logic, no API calls - directly unit-testable.
    [[nodiscard]] constexpr std::uint32_t SelectVulkanApiVersion(std::uint32_t preferredVkApiVersion, const VulkanVersionRange& range)
    {
        return IsVulkanApiVersionSupported(preferredVkApiVersion, range) ? preferredVkApiVersion : range.minVkApiVersion;
    }

    // Decides whether AREngine's XR-compatible VkDevice should request
    // the `timelineSemaphore` feature (see docs/ARCHITECTURE.md, "Timeline
    // Semaphore Device-Feature Negotiation (M11.2)") - purely from
    // queried capability, never a runtime-name check. Pure logic, no API
    // calls - directly unit-testable.
    struct TimelineSemaphoreSelection
    {
        // Whether to chain VkPhysicalDeviceTimelineSemaphoreFeatures
        // (timelineSemaphore = VK_TRUE) into VkDeviceCreateInfo::pNext.
        bool enable = false;

        // Whether the VK_KHR_timeline_semaphore device extension string
        // must also be enabled. Only relevant below Vulkan 1.2 (where
        // timeline semaphores are core and the extension would be
        // redundant) - see the "Vulkan Version Cases" reasoning in
        // docs/ARCHITECTURE.md's M11.2 section.
        bool requiresExtension = false;
    };

    [[nodiscard]] constexpr TimelineSemaphoreSelection SelectTimelineSemaphoreSupport(
        std::uint32_t selectedVkApiVersion,
        bool physicalDeviceSupportsTimelineSemaphore,
        bool timelineSemaphoreExtensionAvailable)
    {
        if (!physicalDeviceSupportsTimelineSemaphore)
        {
            // Case C: no support at all - never enabled, never
            // speculative.
            return TimelineSemaphoreSelection{false, false};
        }
        if (selectedVkApiVersion >= VK_API_VERSION_1_2)
        {
            // Case A: core in the selected version, no extension needed.
            return TimelineSemaphoreSelection{true, false};
        }
        // Case B: pre-1.2, needs VK_KHR_timeline_semaphore too - but
        // only if the runtime's VkPhysicalDevice actually reports it;
        // never enable an extension that isn't there.
        if (timelineSemaphoreExtensionAvailable)
        {
            return TimelineSemaphoreSelection{true, true};
        }
        return TimelineSemaphoreSelection{false, false};
    }

    // Human-readable "major.minor.patch" for a Vulkan-encoded version
    // (VK_MAKE_API_VERSION layout - NOT an XrVersion; see
    // XrVersionToVkApiVersion above for why the two are not
    // interchangeable). Used for logging the requirements
    // range/selected version in Vulkan's own conventional format.
    [[nodiscard]] std::string FormatVkApiVersion(std::uint32_t vkApiVersion);

    // The four XR_KHR_vulkan_enable2 functions M9C needs, retrieved via
    // xrGetInstanceProcAddr - never assumed directly linkable, since
    // extension functions are not part of the loader's static import
    // table (the same reasoning VulkanInstance.cpp already applies to
    // vkCreateDebugUtilsMessengerEXT, just for OpenXR's own dispatch
    // mechanism instead of Vulkan's). See docs/ARCHITECTURE.md,
    // "Function Pointer Loading (M9C)".
    struct OpenXRVulkanFunctions
    {
        PFN_xrGetVulkanGraphicsRequirements2KHR getVulkanGraphicsRequirements2KHR = nullptr;
        PFN_xrCreateVulkanInstanceKHR createVulkanInstanceKHR = nullptr;
        PFN_xrGetVulkanGraphicsDevice2KHR getVulkanGraphicsDevice2KHR = nullptr;
        PFN_xrCreateVulkanDeviceKHR createVulkanDeviceKHR = nullptr;
    };

    // Loads all four function pointers via xrGetInstanceProcAddr(instance,
    // ...). Returns std::nullopt (not a partially-filled struct) if any
    // single one is unavailable - callers must not use any of these
    // functions unless every one of them loaded successfully. Makes
    // real OpenXR calls - not unit-tested, only exercised by the manual
    // demo.
    [[nodiscard]] std::optional<OpenXRVulkanFunctions> LoadOpenXRVulkanFunctions(XrInstance instance);
}
