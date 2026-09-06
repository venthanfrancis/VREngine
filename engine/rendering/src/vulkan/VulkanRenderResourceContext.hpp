#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.
//
// M16: promotes the proven M12-M15 render-resource ownership pattern
// (AssetId -> cached GPU mesh/texture -> MeshId/MaterialId) out of
// tests/-leaf demo helpers (the old MeshCache/MeshRegistry/TextureCache/
// MaterialRegistry, one hand-duplicated set per demo) into one reusable
// engine-owned type. See docs/ARCHITECTURE.md, "M16 - Render Resource
// Context Foundation" for the full audit/design rationale, in
// particular why this returns AREngine::Rendering::MeshHandle/
// MaterialHandle rather than AREngine::Scene::MeshId/MaterialId
// directly (Rendering must never depend on Scene) and why it accepts
// AREngine::Assets::MeshAsset/TextureAsset directly (a deliberate,
// narrow, one-directional Rendering -> Assets dependency, confined to
// this private Vulkan-gated file - see engine/rendering/CMakeLists.txt).
//
// Named with this directory's own established convention: every type
// under src/vulkan/ is prefixed "Vulkan" (VulkanMesh, VulkanImage,
// VulkanDescriptorPool, ...) - this is honestly Vulkan-typed (its
// constructor and GetMesh/GetMaterial return real Vulkan types), not a
// fake-backend-neutral wrapper.
//
// Not copyable or movable: owns real Vulkan resources (a sampler, a
// descriptor pool, and every VulkanMesh/VulkanImage it creates),
// destroyed exactly once, by this object alone - same discipline as
// every other owned Vulkan resource in this backend.

#include "../HandleAllocators.hpp"
#include "VulkanDescriptorPool.hpp"
#include "VulkanSampler.hpp"

#include "AREngine/Assets/AssetId.hpp"
#include "AREngine/Assets/MeshAsset.hpp"
#include "AREngine/Assets/TextureAsset.hpp"
#include "AREngine/Rendering/Handles.hpp"
#include "AREngine/Rendering/MeshData.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    class VulkanMesh;
    class VulkanImage;

    class VulkanRenderResourceContext
    {
    public:
        // `descriptorSetLayout` is borrowed, not owned - the caller
        // (whoever constructs the shared VulkanGraphicsPipeline, whose
        // ownership this milestone deliberately leaves unchanged) must
        // keep it alive for this context's whole lifetime. `maxMaterials`
        // sizes the internal descriptor pool once, at construction - no
        // growth, matching VulkanDescriptorPool's own fixed-capacity
        // model.
        VulkanRenderResourceContext(
            VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue,
            VkDescriptorSetLayout descriptorSetLayout, std::uint32_t maxMaterials);
        ~VulkanRenderResourceContext();

        VulkanRenderResourceContext(const VulkanRenderResourceContext&) = delete;
        VulkanRenderResourceContext& operator=(const VulkanRenderResourceContext&) = delete;
        VulkanRenderResourceContext(VulkanRenderResourceContext&&) = delete;
        VulkanRenderResourceContext& operator=(VulkanRenderResourceContext&&) = delete;

        // Idempotent per assetId: calling this twice with the same
        // AssetId uploads nothing the second time and returns the same
        // MeshHandle - the concrete "AssetId -> one GPU upload" proof
        // M15's MeshCache already established.
        [[nodiscard]] MeshHandle CreateMesh(Assets::AssetId assetId, const Assets::MeshAsset& meshAsset);

        // For content with no AssetId to key by (procedural geometry) -
        // always mints a fresh MeshHandle, uploads unconditionally.
        [[nodiscard]] MeshHandle CreateProceduralMesh(const MeshData& meshData);

        // nullptr if `handle` is unknown - same soft-fail posture as
        // M12's MeshRegistry::Resolve (an unresolvable id skips that
        // one draw, it is not a fatal error).
        [[nodiscard]] const VulkanMesh* GetMesh(MeshHandle handle) const;

        // Idempotent per assetId, mirroring CreateMesh - uploads the
        // texture at most once. Texture identity stays entirely
        // internal (keyed by AssetId only) - it never becomes
        // Scene-visible, unlike mesh/material identity, since nothing
        // outside material creation needs to reference "the texture"
        // independently.
        void CreateTexture(Assets::AssetId assetId, const Assets::TextureAsset& textureAsset);

        // Allocates a new descriptor set for the texture already
        // registered under `textureAssetId` and mints a fresh
        // MaterialHandle for it. Deliberately NOT idempotent - unlike
        // CreateMesh/CreateTexture, calling this twice with the same
        // textureAssetId mints two independent MaterialHandles sharing
        // one underlying GPU texture (material identity is allowed to
        // diverge from texture identity - see docs/ARCHITECTURE.md).
        // Asserts if `textureAssetId` was never passed to CreateTexture
        // first - a setup-order/programmer bug, not a predictable
        // runtime condition, mirroring AssetManager::GetTexture's own
        // assert-on-unknown-id philosophy.
        [[nodiscard]] MaterialHandle CreateMaterial(Assets::AssetId textureAssetId);

        // VK_NULL_HANDLE if `handle` is unknown - same soft-fail
        // posture as M13's MaterialRegistry::Resolve.
        [[nodiscard]] VkDescriptorSet GetMaterial(MaterialHandle handle) const;

        // Diagnostics only, for demo-side logging - mirrors what the
        // demos already log today (e.g. "Uploaded N meshes").
        [[nodiscard]] std::size_t MeshCount() const { return m_meshHandles.Count(); }
        [[nodiscard]] std::size_t TextureCount() const { return m_textures.size(); }
        [[nodiscard]] std::size_t MaterialCount() const { return m_materialHandles.Count(); }

    private:
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
        VkQueue m_queue = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE; // borrowed

        // Declared in this order deliberately, so that reverse-
        // declaration-order destruction reproduces the exact sequence
        // already proven validation-clean by the pre-M16 demos
        // (materialRegistry -> descriptorPool -> sampler -> textureCache):
        // m_materials (trivial) -> m_descriptorPool (frees all sets
        // implicitly) -> m_sampler -> m_textures -> m_meshes ->
        // allocators (trivial). See docs/ARCHITECTURE.md, "M16 -
        // Render Resource Context Foundation".
        MeshHandleAllocator m_meshHandles;
        MaterialHandleAllocator m_materialHandles;
        std::unordered_map<MeshHandle, std::unique_ptr<VulkanMesh>> m_meshes;
        std::unordered_map<Assets::AssetId, std::unique_ptr<VulkanImage>> m_textures;
        VulkanSampler m_sampler;
        VulkanDescriptorPool m_descriptorPool;
        std::unordered_map<MaterialHandle, VkDescriptorSet> m_materials; // trivial - m_descriptorPool owns the sets
    };
}
