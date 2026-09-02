#pragma once

// M14: the one shared material-setup helper used by BOTH the desktop
// scene-render demo and the integrated XR demo, mirroring
// PopulateDemoScene's own "one definition, two consumers" shape. Loads
// two real PNG files through AssetManager, uploads/caches their GPU
// textures through a TextureCache, allocates+writes their descriptor
// sets, and registers them into a MaterialRegistry - the exact same
// DemoMaterialIds shape M13 already defined, now file-backed instead
// of generated in memory. See docs/ARCHITECTURE.md, "M14 - Asset-
// Backed Texture & Material Loading Foundation".
//
// Parameters are kept flat (not bundled into a new "render context"
// struct) - no such bundling precedent exists anywhere in this
// codebase; this matches CreateTextureFromPixels's own established
// individual-Vulkan-handle signature.

#include "AREngine/Assets/Assets.hpp"

#include "MaterialRegistry.hpp"
#include "PopulateDemoScene.hpp"
#include "TextureCache.hpp"

#include "vulkan/VulkanDescriptorPool.hpp"
#include "vulkan/VulkanDescriptorSetLayout.hpp"
#include "vulkan/VulkanSampler.hpp"

namespace ARDemo
{
    // Loads textures/checker_red.png and textures/checker_blue.png
    // through `assetManager` (relative to whatever asset root it was
    // constructed with - both demos point this at tests/data/assets/),
    // resolves/caches their GPU textures through `textureCache`,
    // allocates one descriptor set per texture from `descriptorPool`
    // (which must already be sized for at least 2 sets), and registers
    // both into `materialRegistry`. No file I/O, decode, or GPU upload
    // happens here more than once per distinct asset - this is a setup-
    // time call only, never called from a frame loop.
    [[nodiscard]] DemoMaterialIds PopulateDemoMaterials(
        AREngine::Assets::AssetManager& assetManager,
        TextureCache& textureCache,
        const AREngine::Rendering::Vulkan::VulkanDescriptorSetLayout& descriptorSetLayout,
        AREngine::Rendering::Vulkan::VulkanDescriptorPool& descriptorPool,
        const AREngine::Rendering::Vulkan::VulkanSampler& sampler,
        MaterialRegistry& materialRegistry,
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue);
}
