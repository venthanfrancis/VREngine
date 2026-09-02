#pragma once

#include "AREngine/Core/Math/Vec4.hpp"
#include "AREngine/Scene/MaterialId.hpp"
#include "AREngine/Scene/MeshId.hpp"

namespace AREngine::Scene
{
    // The smallest possible "this entity can be drawn" component: which
    // mesh, which material (render appearance), what tint, and whether
    // it should currently be drawn at all. Deliberately not a full
    // Material/MeshRenderer system — no shaders, no per-material
    // pipelines, nothing GPU-shaped here; `material` is just as opaque
    // as `mesh`. See docs/ARCHITECTURE.md, "M12 - Renderable Scene
    // Integration Foundation" and "M13 - Material & Render Resource
    // Binding Foundation".
    //
    // `material` is placed right after `mesh`, not appended at the end,
    // deliberately: every positional-aggregate-init call site (of which
    // there are many, including one that reconstructs a Renderable
    // every frame in tests/xr_demo.cpp) must supply it explicitly - a
    // trailing field would let it silently default to an invalid
    // MaterialId at any call site nobody remembered to update. See
    // docs/ARCHITECTURE.md, "M13 - Material Field Placement".
    struct Renderable
    {
        MeshId mesh;
        MaterialId material;
        Core::Math::Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        bool visible = true;
    };
}
