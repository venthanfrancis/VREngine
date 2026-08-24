#include "OpenXRSwapchain.hpp"

#include "OpenXRResult.hpp"

namespace AREngine::XR::OpenXR
{
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

    OpenXRSwapchain::OpenXRSwapchain(XrInstance instance, XrSession session, std::int64_t format,
                                      std::uint32_t width, std::uint32_t height, std::uint32_t sampleCount,
                                      XrSwapchainUsageFlags usageFlags)
        : m_instance(instance)
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
    }

    OpenXRSwapchain::~OpenXRSwapchain()
    {
        if (m_swapchain != XR_NULL_HANDLE)
        {
            xrDestroySwapchain(m_swapchain);
            m_swapchain = XR_NULL_HANDLE;
        }
    }
}
