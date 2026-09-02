#pragma once

// M13: resolves the backend-neutral Scene::MaterialId a PlannedDraw
// carries to the actual, already-allocated-and-written VkDescriptorSet
// it refers to. Exact mirror of MeshRegistry.hpp's shape and
// reasoning: Vulkan-only, kept at this leaf (tests/) level, small,
// explicit, and owned entirely by whichever demo constructs it - not a
// global registry. See docs/ARCHITECTURE.md, "M13 - Material & Render
// Resource Binding Foundation".
//
// Stores only the VkDescriptorSet handle, not the underlying texture
// (VulkanImage) it was written from - the descriptor set is what's
// actually bound at draw time, and the pool that allocated it already
// owns its lifetime. Each demo keeps its VulkanImage texture objects
// alive itself (named locals, mirroring how MeshRegistry's demo owns
// its VulkanMesh objects) - this registry never allocates, writes, or
// destroys anything Vulkan-side.

#include "AREngine/Scene/MaterialId.hpp"

#include <unordered_map>
#include <vulkan/vulkan.h>

namespace ARDemo
{
    class MaterialRegistry
    {
    public:
        void Register(AREngine::Scene::MaterialId id, VkDescriptorSet descriptorSet);

        // VK_NULL_HANDLE if `id` was never registered.
        [[nodiscard]] VkDescriptorSet Resolve(AREngine::Scene::MaterialId id) const;

    private:
        std::unordered_map<AREngine::Scene::MaterialId, VkDescriptorSet> m_materials;
    };
}
