#pragma once

// Private OpenXR/Vulkan integration boundary - see
// OpenXRVulkanGraphicsBinding.hpp for the vulkan.h/openxr_platform.h
// include-order requirement this file also depends on transitively.
//
// An XrSwapchain is OpenXR's own presentation surface - fundamentally
// different from the desktop VkSwapchainKHR/VkSurfaceKHR path
// (engine/rendering/src/vulkan/VulkanSwapchain.cpp): there is no
// window, no VkSurfaceKHR, and the compositor - not AREngine - decides
// when images are actually presented. This file creates and owns an
// XrSwapchain and the VkImages OpenXR exposes through it via
// XrSwapchainImageVulkan2KHR. See docs/ARCHITECTURE.md, "XR Swapchain
// Topology (M9E)" and "VkImage Ownership (M9E)".

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace AREngine::XR::OpenXR
{
    // Makes a real xrEnumerateSwapchainFormats call - not
    // unit-tested, only exercised by the manual frame demo. The
    // returned int64_t values are, for a Vulkan-backed session,
    // directly VkFormat values (the OpenXR spec defines this mapping
    // for XR_KHR_vulkan_enable2 explicitly) - no separate decode step
    // is needed, unlike XrVersion/VkVersion in OpenXRVulkanRequirements.hpp.
    [[nodiscard]] std::vector<std::int64_t> EnumerateSwapchainFormats(XrInstance instance, XrSession session);

    // Prefers an sRGB color format - B8G8R8A8_SRGB first, then
    // R8G8B8A8_SRGB - since swapchain content is color meant to be
    // seen, not raw linear data (same reasoning as M8E's
    // CreateTextureFromPixels choosing VK_FORMAT_R8G8B8A8_SRGB for
    // texture data - see docs/ARCHITECTURE.md, "Texture Format (M8E)").
    // Neither is assumed present - both are checked against
    // `supportedFormats` first, per M9E's explicit brief ("never assume
    // VK_FORMAT_B8G8R8A8_SRGB is supported"). Falls back to the first
    // format the runtime reports if neither preferred sRGB format is
    // available, rather than failing outright - some runtime is always
    // better than none for M9E's diagnostic purpose, and the demo logs
    // exactly which format was actually selected either way. Returns
    // std::nullopt only if the runtime reports zero formats at all.
    // Pure logic, directly unit-testable.
    [[nodiscard]] std::optional<std::int64_t> SelectSwapchainColorFormat(const std::vector<std::int64_t>& supportedFormats);

    // Owns one XrSwapchain, created for a single view (see
    // docs/ARCHITECTURE.md, "XR Swapchain Topology (M9E)" for why M9E
    // creates one swapchain per view rather than one array swapchain),
    // plus the OpenXR-owned VkImages it exposes.
    //
    // Ownership:
    //   - Does NOT own `instance`/`session` - both are borrowed handles
    //     the caller must keep alive for this object's entire lifetime,
    //     same discipline as OpenXRSession/OpenXRReferenceSpace.
    //   - Does NOT own the VkImages returned by xrEnumerateSwapchainImages
    //     - these belong to the OpenXR runtime's compositor, are never
    //     destroyed by this class, and are never backed by
    //     application-allocated VkDeviceMemory. This class only ever
    //     reads them (GetImages()), never destroys or reallocates them.
    //   - DOES own the VkImageViews it creates over those images (M9G) -
    //     AREngine-owned views over OpenXR-owned images, per the
    //     ownership split M9E's own comments already anticipated ("If
    //     image views are not already created/cached by OpenXRSwapchain,
    //     add the smallest clean capability needed"). Destroyed in this
    //     object's destructor, before xrDestroySwapchain - see
    //     docs/ARCHITECTURE.md, "OpenXR Swapchain Image Views (M9G)".
    //
    // Not copyable or movable: exactly one XrSwapchain per
    // OpenXRSwapchain, destroyed exactly once (xrDestroySwapchain), by
    // this object alone.
    class OpenXRSwapchain
    {
    public:
        // `device` (new in M9G) is needed to create the per-image
        // VkImageViews below - borrowed, not owned (same discipline as
        // every other Vulkan handle this codebase borrows rather than
        // owns; the caller's OpenXRVulkanGraphicsBinding/session already
        // keep the real VkDevice alive for at least as long as any
        // OpenXRSwapchain constructed from it).
        //
        // `usageFlags` defaults to COLOR_ATTACHMENT + TRANSFER_DST:
        // COLOR_ATTACHMENT because this is a color swapchain the
        // compositor will read (and, as of M9G, an actual render-pass
        // color attachment), TRANSFER_DST because M9E's per-eye proof-
        // of-life used vkCmdClearColorImage, which requires
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT (XrSwapchainUsageFlags bits
        // map directly onto the corresponding VkImageUsageFlags bits for
        // a Vulkan-backed swapchain, per the OpenXR spec) - kept as the
        // default so M9E's own demo is unaffected; M9G's render-pass-
        // based clear doesn't strictly need it but the flag is harmless
        // to keep set. No SAMPLED bit - nothing in this engine samples
        // from these images (yet). `sampleCount`/`width`/`height` must
        // come from the runtime's own XrViewConfigurationView
        // (recommendedSwapchainSampleCount/recommendedImageRectWidth/
        // recommendedImageRectHeight) - never hard-coded. `faceCount`
        // and `arraySize` are always 1 (one view per swapchain, not a
        // cubemap); `mipCount` is always 1 (no mipmapping for a
        // compositor target).
        OpenXRSwapchain(XrInstance instance, XrSession session, VkDevice device, std::int64_t format,
                         std::uint32_t width, std::uint32_t height, std::uint32_t sampleCount,
                         XrSwapchainUsageFlags usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT);
        ~OpenXRSwapchain();

        OpenXRSwapchain(const OpenXRSwapchain&) = delete;
        OpenXRSwapchain& operator=(const OpenXRSwapchain&) = delete;
        OpenXRSwapchain(OpenXRSwapchain&&) = delete;
        OpenXRSwapchain& operator=(OpenXRSwapchain&&) = delete;

        [[nodiscard]] XrSwapchain Get() const { return m_swapchain; }
        [[nodiscard]] const std::vector<VkImage>& GetImages() const { return m_images; }
        // AREngine-owned VkImageViews, one per image in GetImages(), same
        // order/index. See the class comment above for ownership.
        [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
        [[nodiscard]] std::int64_t GetFormat() const { return m_format; }
        [[nodiscard]] std::uint32_t GetWidth() const { return m_width; }
        [[nodiscard]] std::uint32_t GetHeight() const { return m_height; }

    private:
        XrInstance m_instance = XR_NULL_HANDLE; // borrowed, not owned
        VkDevice m_device = VK_NULL_HANDLE; // borrowed, not owned
        XrSwapchain m_swapchain = XR_NULL_HANDLE;
        std::int64_t m_format = 0;
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        std::vector<VkImage> m_images; // OpenXR-owned - never destroyed by this class
        std::vector<VkImageView> m_imageViews; // AREngine-owned - destroyed by this class, before xrDestroySwapchain
    };
}
