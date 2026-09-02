#pragma once

// M14: caches ONE GPU VulkanImage per AssetId, so a material referencing
// the same image asset twice never triggers a duplicate upload. Unlike
// MeshRegistry/MaterialRegistry (non-owning lookup tables over demo-
// owned objects), TextureCache OWNS every VulkanImage it creates - its
// whole job is "create once, cache, own it." Small, explicit, and owned
// entirely by whichever demo constructs it - not a global registry. See
// docs/ARCHITECTURE.md, "M14 - Asset-Backed Texture & Material Loading
// Foundation".
//
// No automated test coverage: GetOrCreate calls real Vulkan creation/
// upload APIs (via the existing CreateTextureFromPixels) with no
// opaque-handle shortcut available (unlike MaterialRegistry's fake-
// VkDescriptorSet trick) - verified manually through the demos only,
// consistent with this codebase's existing treatment of
// vulkan_demo.cpp/vulkan_present_demo.cpp (built, never
// add_test-registered).

#include "AREngine/Assets/AssetId.hpp"
#include "AREngine/Assets/TextureAsset.hpp"

#include "vulkan/VulkanImage.hpp"

#include <unordered_map>
#include <vulkan/vulkan.h>

namespace ARDemo
{
    class TextureCache
    {
    public:
        // Returns the cached VulkanImage* for `assetId` if one already
        // exists (zero Vulkan calls on a cache hit); otherwise uploads
        // `imageAsset`'s decoded pixels via the existing
        // CreateTextureFromPixels, caches the result, and returns it.
        [[nodiscard]] const AREngine::Rendering::Vulkan::VulkanImage* GetOrCreate(
            AREngine::Assets::AssetId assetId, const AREngine::Assets::TextureAsset& imageAsset,
            VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue);

    private:
        std::unordered_map<AREngine::Assets::AssetId, std::unique_ptr<AREngine::Rendering::Vulkan::VulkanImage>> m_textures;
    };
}
