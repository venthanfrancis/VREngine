#include "PopulateDemoMaterials.hpp"

#include "AREngine/Core/Assert.hpp"

namespace ARDemo
{
    namespace
    {
        // Loads+resolves one texture asset by relative path and
        // creates a material for it via `context`. Returns std::nullopt
        // if the asset fails to load - the caller decides how to react;
        // M14's demos treat this as a setup-time hard failure
        // (AR_ASSERT), since a missing committed test/demo fixture is a
        // build/packaging bug, not a normal runtime condition.
        std::optional<AREngine::Scene::MaterialId> LoadAndCreateMaterial(
            AREngine::Assets::AssetManager& assetManager,
            AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context,
            const std::filesystem::path& relativePath)
        {
            const std::optional<AREngine::Assets::AssetId> assetId = assetManager.LoadTexture(relativePath);
            if (!assetId.has_value())
            {
                return std::nullopt;
            }

            const AREngine::Assets::TextureAsset& imageAsset = assetManager.GetTexture(*assetId);
            context.CreateTexture(*assetId, imageAsset);

            const AREngine::Rendering::MaterialHandle handle = context.CreateMaterial(*assetId);
            return AREngine::Scene::MaterialId{handle.id};
        }
    }

    DemoMaterialIds PopulateDemoMaterials(
        AREngine::Assets::AssetManager& assetManager,
        AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context)
    {
        const std::optional<AREngine::Scene::MaterialId> redChecker =
            LoadAndCreateMaterial(assetManager, context, "textures/checker_red.png");
        AR_ASSERT_MSG(redChecker.has_value(), "Failed to load the committed demo texture asset textures/checker_red.png - packaging/asset-root bug");

        const std::optional<AREngine::Scene::MaterialId> blueChecker =
            LoadAndCreateMaterial(assetManager, context, "textures/checker_blue.png");
        AR_ASSERT_MSG(blueChecker.has_value(), "Failed to load the committed demo texture asset textures/checker_blue.png - packaging/asset-root bug");

        return DemoMaterialIds{*redChecker, *blueChecker};
    }
}
