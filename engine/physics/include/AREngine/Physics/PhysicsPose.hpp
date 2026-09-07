#pragma once

#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

namespace AREngine::Physics
{
    // A minimal, backend-neutral rigid-body pose: position + rotation
    // only. Deliberately NOT Scene::Transform - Transform also carries
    // `scale` (rigid-body engines don't support arbitrary dynamic scale,
    // and Physics never needs it - see docs/ARCHITECTURE.md, "Scale
    // Policy") and is scoped to Scene's own hierarchy semantics, which
    // Physics must never depend on. No parent, no Vulkan/OpenXR/Scene
    // type anywhere in this file.
    struct PhysicsPose
    {
        Core::Math::Vec3 position;
        Core::Math::Quaternion rotation = Core::Math::Quaternion::Identity();
    };
}
