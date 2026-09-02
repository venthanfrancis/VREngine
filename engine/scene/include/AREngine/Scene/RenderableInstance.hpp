#pragma once

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Vec4.hpp"
#include "AREngine/Scene/EntityId.hpp"
#include "AREngine/Scene/MaterialId.hpp"
#include "AREngine/Scene/MeshId.hpp"

namespace AREngine::Scene
{
    // One entity's renderable state, snapshotted out of a Scene by
    // Scene::ExtractRenderables(): a resolved WORLD transform (not the
    // entity's own local Transform), plus its mesh/material/tint.
    // Backend-neutral — no Vulkan/OpenXR type appears here — so it can
    // be consumed by any presentation path (desktop or XR) without
    // either depending on Scene. See docs/ARCHITECTURE.md, "M12 -
    // Renderable Scene Integration Foundation" and "M13 - Material &
    // Render Resource Binding Foundation".
    struct RenderableInstance
    {
        EntityId entity;
        Core::Math::Mat4 worldTransform;
        MeshId mesh;
        MaterialId material;
        Core::Math::Vec4 tint;
    };
}
