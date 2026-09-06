#pragma once

// M14/M16: the one shared material-setup helper used by BOTH the
// desktop scene-render demo and the integrated XR demo, mirroring
// PopulateDemoScene's own "one definition, two consumers" shape. Loads
// two real PNG files through AssetManager and uploads/caches/binds
// them through the engine-owned VulkanRenderResourceContext (M16 -
// previously a demo-local TextureCache + VulkanSampler +
// VulkanDescriptorPool + MaterialRegistry, hand-constructed by each
// demo) - the exact same DemoMaterialIds shape M13 already defined.
// See docs/ARCHITECTURE.md, "M16 - Render Resource Context Foundation".
//
// Parameters are kept flat (not bundled into a new "render context"
// struct) - no such bundling precedent exists anywhere in this
// codebase.

#include "AREngine/Assets/Assets.hpp"

#include "PopulateDemoScene.hpp"

#include "vulkan/VulkanRenderResourceContext.hpp"

namespace ARDemo
{
    // Loads textures/checker_red.png and textures/checker_blue.png
    // through `assetManager` (relative to whatever asset root it was
    // constructed with - both demos point this at tests/data/assets/),
    // uploads/caches/binds both through `context`, and returns the
    // resulting DemoMaterialIds (converted from the context's own
    // backend-neutral Rendering::MaterialHandle values). No file I/O,
    // decode, or GPU upload happens here more than once per distinct
    // asset - this is a setup-time call only, never called from a
    // frame loop.
    [[nodiscard]] DemoMaterialIds PopulateDemoMaterials(
        AREngine::Assets::AssetManager& assetManager,
        AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context);
}
