#pragma once

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Vec4.hpp"
#include "AREngine/Rendering/Handles.hpp"

namespace AREngine::Rendering
{
    // M17: one backend-neutral, view-independent render instance - the
    // Rendering-owned counterpart to Scene::RenderableInstance, with
    // Scene::MeshId/MaterialId converted to Rendering::MeshHandle/
    // MaterialHandle (see docs/ARCHITECTURE.md, "M17 - Scene Render
    // Submission Foundation" for why Rendering never references the
    // Scene types directly). Deliberately stores the world-space
    // `model` matrix, not a pre-baked per-view MVP - one RenderItem
    // exists per renderable regardless of how many views it's drawn
    // into; SubmitRenderItems computes `viewProjection * model` per
    // item, per view, on the fly.
    //
    // No Vulkan, no OpenXR, no Scene, no Asset type, no camera
    // ownership, no entity mutation - just the four fields a draw
    // actually needs.
    struct RenderItem
    {
        MeshHandle mesh;
        MaterialHandle material;
        Core::Math::Mat4 model;
        Core::Math::Vec4 tint;
    };
}
