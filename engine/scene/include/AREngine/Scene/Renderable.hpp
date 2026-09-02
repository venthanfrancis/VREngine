#pragma once

#include "AREngine/Core/Math/Vec4.hpp"
#include "AREngine/Scene/MeshId.hpp"

namespace AREngine::Scene
{
    // The smallest possible "this entity can be drawn" component: which
    // mesh, what tint, and whether it should currently be drawn at all.
    // Deliberately not a Material/MeshRenderer system — no shaders, no
    // textures, no per-instance buffers, nothing GPU-shaped. See
    // docs/ARCHITECTURE.md, "M12 - Renderable Scene Integration
    // Foundation".
    struct Renderable
    {
        MeshId mesh;
        Core::Math::Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        bool visible = true;
    };
}
