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

        // This transform's local axes, expressed in world space — i.e.
        // `rotation` applied to the corresponding world basis vector.
        // Added in M8G for the free-fly camera controller (forward/
        // right define movement direction; a camera's own GetViewMatrix
        // uses GetForward too) but meaningful for any Transform, not
        // camera-specific — see docs/ARCHITECTURE.md, "Camera Vs
        // Transform Responsibilities (M8G)". Ignores `scale` — a
        // direction has no length to scale.
        [[nodiscard]] Core::Math::Vec3 GetForward() const
        {
            return Core::Math::Rotate(rotation, Core::Math::kWorldForward);
        }

        [[nodiscard]] Core::Math::Vec3 GetRight() const
        {
            return Core::Math::Rotate(rotation, Core::Math::kWorldRight);
        }

        [[nodiscard]] Core::Math::Vec3 GetUp() const
        {
            return Core::Math::Rotate(rotation, Core::Math::kWorldUp);
        }
    };
}
