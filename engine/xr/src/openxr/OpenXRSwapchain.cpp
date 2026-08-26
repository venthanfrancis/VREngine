#include "OpenXRSwapchain.hpp"

#include "OpenXRResult.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

#include <format>
#include <string>

namespace AREngine::XR::OpenXR
{
    namespace
    {
        // Small, self-contained CheckVkResult-equivalent - deliberately
        // duplicated rather than reused across modules, same discipline
        // OpenXRVulkanGraphicsBinding.cpp already established for this
        // exact reason (see its own copy of this helper).
        void CheckVkResultHere(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
            {
                const std::string message = std::format("{} failed: VkResult({})", operation, static_cast<int>(result));
                AR_LOG_ERROR(message);
                AR_ASSERT_MSG(false, message.c_str());
            }
        }
    }
    std::vector<std::int64_t> EnumerateSwapchainFormats(XrInstance instance, XrSession session)
    {
        std::uint32_t count = 0;
        CheckXrResult(instance,
            xrEnumerateSwapchainFormats(session, 0, &count, nullptr),
            "xrEnumerateSwapchainFormats (count query)");

        std::vector<std::int64_t> formats(count);
        if (count == 0)
        {
            return formats;
        }

        CheckXrResult(instance,
            xrEnumerateSwapchainFormats(session, count, &count, formats.data()),
            "xrEnumerateSwapchainFormats (data query)");
        return formats;
    }

    std::optional<std::int64_t> SelectSwapchainColorFormat(const std::vector<std::int64_t>& supportedFormats)
    {
        for (const std::int64_t format : supportedFormats)
        {
            if (format == static_cast<std::int64_t>(VK_FORMAT_B8G8R8A8_SRGB))
            {
                return format;
            }
        }
        for (const std::int64_t format : supportedFormats)
        {
            if (format == static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_SRGB))
            {
                return format;
            }
        }
        if (!supportedFormats.empty())
        {
            return supportedFormats.front();
        }
        return std::nullopt;
    }

    OpenXRSwapchain::OpenXRSwapchain(XrInstance instance, XrSession session, VkDevice device, std::int64_t format,
                                      std::uint32_t width, std::uint32_t height, std::uint32_t sampleCount,
                                      XrSwapchainUsageFlags usageFlags)
        : m_instance(instance)
        , m_device(device)
        , m_format(format)
        , m_width(width)
        , m_height(height)
    {
        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.createFlags = 0;
        createInfo.usageFlags = usageFlags;
        createInfo.format = format;
        createInfo.sampleCount = sampleCount;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.faceCount = 1;
        createInfo.arraySize = 1;
        createInfo.mipCount = 1;

        CheckXrResult(instance, xrCreateSwapchain(session, &createInfo, &m_swapchain), "xrCreateSwapchain");

        std::uint32_t imageCount = 0;
        CheckXrResult(instance,
            xrEnumerateSwapchainImages(m_swapchain, 0, &imageCount, nullptr),
            "xrEnumerateSwapchainImages (count query)");

        std::vector<XrSwapchainImageVulkan2KHR> images(imageCount, XrSwapchainImageVulkan2KHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        if (imageCount > 0)
        {
            CheckXrResult(instance,
                xrEnumerateSwapchainImages(m_swapchain, imageCount, &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),
                "xrEnumerateSwapchainImages (data query)");
        }

        m_images.reserve(images.size());
        for (const XrSwapchainImageVulkan2KHR& image : images)
        {
            m_images.push_back(image.image);
        }

        // AREngine-owned VkImageViews over the OpenXR-owned VkImages
        // above - one per image, same order/index. Never allocates or
        // binds any VkDeviceMemory (that would violate the "OpenXR owns
        // VkImage" rule) - a view has no memory of its own to allocate.
        m_imageViews.reserve(m_images.size());
        for (const VkImage image : m_images)
        {
            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.image = image;
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = static_cast<VkFormat>(m_format);
            viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;

            VkImageView view = VK_NULL_HANDLE;
            CheckVkResultHere(vkCreateImageView(m_device, &viewCreateInfo, nullptr, &view), "vkCreateImageView (OpenXRSwapchain)");
            m_imageViews.push_back(view);
        }
    }

    OpenXRSwapchain::~OpenXRSwapchain()
    {
        // Views before the swapchain: they are views INTO the
        // swapchain's images, so they must not outlive xrDestroySwapchain
        // (same "dependent object destroyed first" discipline as every
        // other owned-resource-over-borrowed-resource pair in this
        // codebase).
        for (const VkImageView view : m_imageViews)
        {
            vkDestroyImageView(m_device, view, nullptr);
        }
        m_imageViews.clear();

        if (m_swapchain != XR_NULL_HANDLE)
        {
            xrDestroySwapchain(m_swapchain);
            m_swapchain = XR_NULL_HANDLE;
        }
    }
}
