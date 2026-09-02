#pragma once

// M12: the generic "extract once, render against N views" step between
// Scene::ExtractRenderables() and actual GPU draw execution. Pure logic
// only - no Vulkan, no OpenXR, no Rendering type anywhere in this file -
// so it compiles and is fully testable with ARENGINE_ENABLE_VULKAN OFF,
// and is shared unchanged between the desktop scene demo (1 view) and
// the integrated XR demo (N eye views), with no hardcoded left/right
// branching anywhere. See docs/ARCHITECTURE.md, "M12 - Renderable Scene
// Integration Foundation".

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Vec4.hpp"
#include "AREngine/Scene/MeshId.hpp"
#include "AREngine/Scene/RenderableInstance.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ARDemo
{
    // One resolved (view, renderable) pair, ready for GPU draw
    // execution - still backend-neutral (MeshId, not a VulkanMesh*).
    struct PlannedDraw
    {
        std::size_t viewIndex = 0;
        AREngine::Core::Math::Mat4 mvp;
        AREngine::Scene::MeshId mesh;
        AREngine::Core::Math::Vec4 tint;
    };

    // For each view in `viewProjections`, for each renderable in
    // `renderables`: one PlannedDraw with
    // mvp = viewProjections[view] * renderable.worldTransform. Never
    // assumes exactly 1 or 2 views - `renderables.size() *
    // viewProjections.size()` planned draws are always produced, in
    // (view, renderable) order.
    [[nodiscard]] std::vector<PlannedDraw> BuildDrawPlan(
        std::span<const AREngine::Scene::RenderableInstance> renderables,
        std::span<const AREngine::Core::Math::Mat4> viewProjections);
}
