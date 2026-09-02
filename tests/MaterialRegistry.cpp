#include "MaterialRegistry.hpp"

namespace ARDemo
{
    void MaterialRegistry::Register(AREngine::Scene::MaterialId id, VkDescriptorSet descriptorSet)
    {
        m_materials[id] = descriptorSet; // last-write-wins, matching MeshRegistry::Register's own precedent
    }

    VkDescriptorSet MaterialRegistry::Resolve(AREngine::Scene::MaterialId id) const
    {
        const auto it = m_materials.find(id);
        return it != m_materials.end() ? it->second : VK_NULL_HANDLE;
    }
}
