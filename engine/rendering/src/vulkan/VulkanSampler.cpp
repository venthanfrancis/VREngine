#include "VulkanSampler.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanSampler::VulkanSampler(VkDevice device)
        : m_device(device)
    {
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        // LINEAR: smooth-filtered sampling, the common default for a
        // color texture (NEAREST would keep the checkerboard's tile
        // edges hard-pixelated - either is fine to prove sampling
        // works; LINEAR was picked as the more broadly useful default
        // for real texture work later).
        createInfo.magFilter = VK_FILTER_LINEAR;
        createInfo.minFilter = VK_FILTER_LINEAR;
        // CLAMP_TO_EDGE, not REPEAT: the demo's quad UVs span exactly
        // [0,1] and never sample outside that range, so tiling
        // behavior is irrelevant either way here - CLAMP_TO_EDGE is
        // chosen anyway as the more broadly correct default for a
        // single, non-tiling texture (it avoids any wraparound
        // artifact at the very edge texels under linear filtering,
        // which REPEAT would not).
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        // Anisotropy deliberately NOT enabled just because the device
        // may support it - M8E has no stated requirement for it. See
        // docs/ARCHITECTURE.md, "Sampler Settings (M8E)".
        createInfo.anisotropyEnable = VK_FALSE;
        createInfo.maxAnisotropy = 1.0f;
        createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        createInfo.unnormalizedCoordinates = VK_FALSE; // sample with [0,1] UVs, the normal convention
        createInfo.compareEnable = VK_FALSE;
        createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        createInfo.mipLodBias = 0.0f;
        createInfo.minLod = 0.0f;
        createInfo.maxLod = 0.0f; // no mipmaps yet - a single level, so LOD never moves

        const VkResult result = vkCreateSampler(device, &createInfo, nullptr, &m_sampler);
        CheckVkResult(result, "vkCreateSampler");
    }

    VulkanSampler::~VulkanSampler()
    {
        if (m_sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
    }
}
