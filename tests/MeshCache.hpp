#pragma once

// M15: caches ONE GPU VulkanMesh per AssetId, so a MeshId referencing
// the same mesh asset twice never triggers a duplicate upload. Same
// owning-cache shape as TextureCache (M14) - unlike MeshRegistry/
// MaterialRegistry (non-owning lookup tables over demo-owned objects),
// MeshCache OWNS every VulkanMesh it creates - its whole job is
// "create once, cache, own it." Small, explicit, and owned entirely by
// whichever demo constructs it - not a global registry. See
// docs/ARCHITECTURE.md, "M15 - Asset-Backed Mesh Loading Foundation".
//
// No automated test coverage: GetOrCreate calls real Vulkan creation/
// upload APIs (via the existing CreateVulkanMesh) with no opaque-
// handle shortcut available - verified manually through the demos
// only, consistent with TextureCache's own established posture.

#include "AREngine/Assets/AssetId.hpp"
#include "AREngine/Assets/MeshAsset.hpp"

#include "vulkan/VulkanMesh.hpp"

#include <unordered_map>
#include <vulkan/vulkan.h>

namespace ARDemo
{
    class MeshCache
    {
    public:
        // Returns the cached VulkanMesh* for `assetId` if one already
        // exists (zero Vulkan calls on a cache hit); otherwise converts
        // `meshAsset`'s unified vertex/index data into a
        // Rendering::MeshData, uploads it via the existing
        // CreateVulkanMesh, caches the result, and returns it.
        [[nodiscard]] const AREngine::Rendering::Vulkan::VulkanMesh* GetOrCreate(
            AREngine::Assets::AssetId assetId, const AREngine::Assets::MeshAsset& meshAsset,
            VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue);

    private:
        std::unordered_map<AREngine::Assets::AssetId, std::unique_ptr<AREngine::Rendering::Vulkan::VulkanMesh>> m_meshes;
    };
}
