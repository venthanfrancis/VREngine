#include "VulkanSwapchain.hpp"

#include "VulkanResult.hpp"
#include "VulkanSwapchainSupport.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanSwapchain::VulkanSwapchain(VkPhysicalDevice physicalDevice,
                                      VkDevice device,
                                      VkSurfaceKHR surface,
                                      const QueueFamilyIndices& queueFamilies,
                                      std::uint32_t windowWidth,
                                      std::uint32_t windowHeight)
        : m_device(device)
    {
        // Re-queries support here rather than threading it through as a
        // parameter: physical device selection already proved this
        // device/surface pair is adequate (SelectPhysicalDeviceForPresentation),
        // and a fresh query is also exactly right on swapchain
        // recreation, where capabilities may have changed since.
        const SwapchainSupportDetails support = QuerySwapchainSupport(physicalDevice, surface);

        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.formats);
        const VkPresentModeKHR presentMode = ChoosePresentMode(support.presentModes);
        const VkExtent2D extent = ChooseSwapchainExtent(support.capabilities, windowWidth, windowHeight);
        const std::uint32_t imageCount = ChooseSwapchainImageCount(support.capabilities);

        m_imageFormat = surfaceFormat.format;
        m_extent = extent;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        // TRANSFER_DST: M8B clears the swapchain image directly via
        // vkCmdClearColorImage, not a render pass - see
        // docs/ARCHITECTURE.md, "Clearing (M8B)". COLOR_ATTACHMENT is
        // also requested even though nothing renders to it yet: a
        // VkImageView (below) requires the image it's a view of to
        // have at least one of a small set of usage bits, none of
        // which TRANSFER_DST is part of - found the hard way, as a
        // real validation error, when M8B was first run against real
        // hardware. COLOR_ATTACHMENT is also what M8C's render pass
        // will need these images for anyway.
        createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const std::vector<std::uint32_t> uniqueFamilies = GetUniqueQueueFamilies(queueFamilies);
        if (HasSeparatePresentQueue(queueFamilies))
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(uniqueFamilies.size());
            createInfo.pQueueFamilyIndices = uniqueFamilies.data();
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        const VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain);
        CheckVkResult(result, "vkCreateSwapchainKHR");

        std::uint32_t actualImageCount = 0;
        vkGetSwapchainImagesKHR(device, m_swapchain, &actualImageCount, nullptr);
        m_images.resize(actualImageCount);
        vkGetSwapchainImagesKHR(device, m_swapchain, &actualImageCount, m_images.data());

        m_imageViews.resize(actualImageCount);
        for (std::size_t i = 0; i < m_images.size(); ++i)
        {
            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.image = m_images[i];
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = m_imageFormat;
            viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;

            const VkResult viewResult = vkCreateImageView(device, &viewCreateInfo, nullptr, &m_imageViews[i]);
            CheckVkResult(viewResult, "vkCreateImageView");
        }
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        // Image views before the swapchain: they depend on it and must
        // not outlive it — see docs/ARCHITECTURE.md, "Exact Destruction
        // Order (M8B)".
        for (VkImageView view : m_imageViews)
        {
            vkDestroyImageView(m_device, view, nullptr);
        }
        m_imageViews.clear();

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }
    }
}
