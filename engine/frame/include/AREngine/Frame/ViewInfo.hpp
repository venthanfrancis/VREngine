#pragma once

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

namespace AREngine::Frame
{
    // The minimum information needed to render one view of a scene:
    // where the viewer is, which way they're facing, and how to project
    // 3D points onto that view. A frame may need zero, one, or several
    // of these (0 while headless, 1 on a desktop window, 2 for stereo
    // XR) — callers must not assume a fixed count.
    struct ViewInfo
    {
        Core::Math::Vec3 position;
        Core::Math::Quaternion orientation = Core::Math::Quaternion::Identity();

        // Precomputed projection matrix for this view. Left as a plain
        // Mat4 rather than FOV/near/far fields, since desktop and XR
        // views compute this differently and neither is known yet.
        Core::Math::Mat4 projection = Core::Math::Mat4::Identity();
    };
}
