#pragma once

// Private OpenXR/Vulkan integration boundary.
//
// M9C's whole job: prove every value a future XrGraphicsBindingVulkan2KHR
// needs can actually be produced, through OpenXR's own Vulkan creation
// functions (XR_KHR_vulkan_enable2) rather than AREngine's ordinary
// desktop Vulkan bring-up path. This is NOT a replacement for, or a
// merge with, Rendering's Vulkan backend (engine/rendering/src/vulkan/):
// that backend keeps driving the M8 desktop window/swapchain path
// completely unchanged. This file is the seam between OpenXR (which
// owns XR concepts and lifecycle) and Vulkan (which Rendering owns as
// an implementation) - see docs/ARCHITECTURE.md, "XR-Vulkan Integration
// Placement (M9C)" for the full placement reasoning:
//
//   OpenXR
//       |  requirements/device selection
//       v
//   XR-Vulkan integration   <-- this file
//       |
//       v
//   Vulkan instance/device/queue
//
// Deliberately lives under engine/xr/src/openxr/ (private, like every
// other OpenXR implementation file), not engine/rendering/: the whole
// point of this integration is driven by OpenXR's API surface
// (xrCreateVulkanInstanceKHR etc. are OpenXR calls, even though they
// produce Vulkan objects), and keeping it here means Rendering's
// CMakeLists and public headers never need to know OpenXR exists.
// arengine_xr only links Vulkan::Vulkan when BOTH ARENGINE_ENABLE_OPENXR
// and ARENGINE_ENABLE_VULKAN are ON (see engine/xr/CMakeLists.txt) -
// XR does not depend on the entire Rendering module to get here, only
// on the raw Vulkan SDK, same as Rendering itself does.

#include "OpenXRVulkanRequirements.hpp"

#include <cstdint>
#include <vector>

namespace AREngine::XR::OpenXR
{
    // Finds the first queue family that supports graphics operations.
    // Pure logic over already-queried queue family properties, no
    // Vulkan API calls - directly unit-testable with synthetic data.
    //
    // Deliberately a fresh, self-contained copy of the same ~10 lines
    // Rendering::Vulkan::FindGraphicsQueueFamily already implements
    // (VulkanPhysicalDevice.hpp) - not a shared/imported function. XR
    // does not include Rendering's private headers (that would create
    // exactly the cross-module coupling the M9C brief explicitly warns
    // against), and this logic is small and stable enough that
    // duplicating it here is a far smaller cost than that coupling
    // would be. See docs/ARCHITECTURE.md, "Queue Family Selection
    // (M9C)".
    [[nodiscard]] std::optional<std::uint32_t> FindGraphicsQueueFamily(
        const std::vector<VkQueueFamilyProperties>& queueFamilies);

    // Every value a future XrGraphicsBindingVulkan2KHR will need,
    // captured in a plain, backend-neutral-in-spirit (but necessarily
    // Vulkan-typed) struct. Maps field-for-field onto
    // XrGraphicsBindingVulkan2KHR (minus `type`/`next`) so M9D can
    // construct that struct directly from this data with no
    // translation. See docs/ARCHITECTURE.md, "Graphics Binding Data
    // (M9C)".
    struct VulkanGraphicsBindingData
    {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        std::uint32_t queueFamilyIndex = 0;
        std::uint32_t queueIndex = 0;

        [[nodiscard]] bool IsValid() const
        {
            return instance != VK_NULL_HANDLE && physicalDevice != VK_NULL_HANDLE && device != VK_NULL_HANDLE;
        }
    };

    // Owns a Vulkan VkInstance + VkDevice created THROUGH OpenXR's own
    // XR_KHR_vulkan_enable2 functions (xrCreateVulkanInstanceKHR /
    // xrCreateVulkanDeviceKHR) - deliberately NOT the ordinary
    // vkCreateInstance/vkCreateDevice desktop path, and deliberately a
    // brand-new VkInstance/VkDevice pair, never the M8 desktop
    // renderer's own objects. See docs/ARCHITECTURE.md, "Why the M8
    // Desktop Vulkan Objects Are Not Reused (M9C)".
    //
    // Ownership (see docs/ARCHITECTURE.md, "Ownership / Destruction
    // Order (M9C)"):
    //   - Does NOT own `instance` (XrInstance) or `systemId` - both are
    //     borrowed handles the caller must keep alive for this object's
    //     entire lifetime (OpenXR extension functions and the XR system
    //     they were resolved/queried against remain in use throughout
    //     construction). Callers are expected to declare their
    //     OpenXRInstance BEFORE constructing this object, so C++'s
    //     reverse-destruction-order rule destroys this object first -
    //     see the demo (tests/openxr_vulkan_demo.cpp) for the exact,
    //     explicitly-commented ordering.
    //   - DOES own the VkInstance and VkDevice it creates, and the
    //     optional debug messenger attached to the VkInstance. Destroys
    //     them in the correct dependency order: debug messenger, then
    //     VkDevice, then VkInstance (a VkDevice must never outlive the
    //     VkInstance it was created from).
    //   - Does NOT own the VkPhysicalDevice OpenXR selects (Vulkan
    //     physical devices are never destroyed by an application -
    //     they are enumerated/queried handles owned by the driver).
    //   - Does NOT own the VkQueue it retrieves (queues are owned by
    //     the VkDevice that created them, destroyed implicitly when
    //     that device is destroyed).
    //
    // Not copyable or movable: exactly one Vulkan instance/device pair
    // per object, destroyed exactly once, by this object alone - same
    // discipline as every Vulkan RAII wrapper in Rendering's own
    // backend.
    class OpenXRVulkanGraphicsBinding
    {
    public:
        // Does all of M9C's real work: loads the four XR_KHR_vulkan_enable2
        // function pointers, queries Vulkan graphics requirements,
        // selects a compatible Vulkan API version, creates the
        // XR-compatible VkInstance (with validation enabled if
        // available), asks OpenXR which VkPhysicalDevice to use, finds
        // a graphics-capable queue family, and creates the
        // XR-compatible VkDevice. Every step past the caller's own
        // upfront XR_KHR_vulkan_enable2 support check (performed by the
        // demo before this object is even constructed - see
        // docs/ARCHITECTURE.md, "XR_KHR_vulkan_enable2 Selection (M9C)")
        // is treated as fatal on failure (AR_ASSERT_MSG) - by this
        // point the extension is confirmed present and enabled, so any
        // further failure is a genuine bug/environment problem, not an
        // ordinary machine state, matching the same bring-up-is-fatal
        // policy Rendering's Vulkan backend already uses throughout.
        OpenXRVulkanGraphicsBinding(XrInstance instance, XrSystemId systemId);
        ~OpenXRVulkanGraphicsBinding();

        OpenXRVulkanGraphicsBinding(const OpenXRVulkanGraphicsBinding&) = delete;
        OpenXRVulkanGraphicsBinding& operator=(const OpenXRVulkanGraphicsBinding&) = delete;
        OpenXRVulkanGraphicsBinding(OpenXRVulkanGraphicsBinding&&) = delete;
        OpenXRVulkanGraphicsBinding& operator=(OpenXRVulkanGraphicsBinding&&) = delete;

        [[nodiscard]] const VulkanGraphicsBindingData& GetBindingData() const { return m_bindingData; }
        [[nodiscard]] VulkanVersionRange GetSupportedVersionRange() const { return m_supportedVersionRange; }
        [[nodiscard]] std::uint32_t GetSelectedVulkanApiVersion() const { return m_selectedVulkanApiVersion; }
        [[nodiscard]] const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const { return m_physicalDeviceProperties; }
        [[nodiscard]] bool IsValidationEnabled() const { return m_debugMessenger != VK_NULL_HANDLE; }

        // Not part of VulkanGraphicsBindingData - see the field's
        // declaration comment below. Confirms the queue selected by
        // queueFamilyIndex/queueIndex is real and retrievable.
        [[nodiscard]] VkQueue GetQueue() const { return m_queue; }

    private:
        VulkanGraphicsBindingData m_bindingData;
        VulkanVersionRange m_supportedVersionRange;
        std::uint32_t m_selectedVulkanApiVersion = 0;
        VkPhysicalDeviceProperties m_physicalDeviceProperties{};
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

        // Owned by m_bindingData.device (destroyed implicitly when that
        // device is destroyed) - not separately destroyed by this
        // object. Not part of XrGraphicsBindingVulkan2KHR's own fields;
        // see the constructor's comment for why it is retrieved anyway.
        VkQueue m_queue = VK_NULL_HANDLE;
    };
}
