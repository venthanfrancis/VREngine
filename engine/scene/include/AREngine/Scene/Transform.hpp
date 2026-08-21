#pragma once

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

namespace AREngine::Scene
{
    // An entity's transform relative to its parent (or to the world, if
    // it has no parent) — see docs/ARCHITECTURE.md, "Local vs World
    // Transform". Follows the engine's world convention: 1 unit = 1
    // meter, right-handed, +X right, +Y up, -Z forward (see
    // docs/WORLD_CONVENTIONS.md).
    struct Transform
    {
        Core::Math::Vec3 position;
        Core::Math::Quaternion rotation = Core::Math::Quaternion::Identity();
        Core::Math::Vec3 scale{1.0f, 1.0f, 1.0f};

        // Composes this transform into a single matrix using AREngine's
        // TRS = Translation * Rotation * Scale order — see
        // docs/ARCHITECTURE.md, "TRS Composition Order".
        [[nodiscard]] Core::Math::Mat4 ToMatrix() const
        {
            return Core::Math::Mat4::TRS(position, rotation, scale);
        }
    };
}
