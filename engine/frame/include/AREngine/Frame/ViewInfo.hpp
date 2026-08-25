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
    //
    // M9F evaluated this shape against real OpenXR view data
    // (xrLocateViews) and found it already sufficient — no field
    // changes were needed, only this comment. See docs/ARCHITECTURE.md,
    // "ViewInfo Architecture Decision (M9F)".
    struct ViewInfo
    {
        // The view's pose (position + orientation) in whatever
        // reference space the frame driver used to produce it — i.e.
        // "worldFromView": where the view is and how it's facing,
        // expressed in that space, NOT a view matrix and NOT its
        // inverse. A renderer that needs a conventional view matrix
        // (viewFromWorld) computes it from this pose on demand -
        // deliberately not precomputed/stored here, the same
        // "don't duplicate what can be derived" discipline
        // Scene::Camera already applies to its own pose vs. view
        // matrix. No such computation exists yet anywhere in this
        // engine (M9F does not render); it is expected to arrive
        // whenever a real renderer first needs one, with that
        // renderer's own real requirements shaping its exact form.
        Core::Math::Vec3 position;
        Core::Math::Quaternion orientation = Core::Math::Quaternion::Identity();

        // Precomputed projection matrix for this view. Left as a plain
        // Mat4 rather than FOV/near/far fields, since desktop and XR
        // views compute this differently. On the XR path (M9F) this is
        // built from the runtime's own real, generally asymmetric FOV
        // via Core::Math::PerspectiveOffCenterRH_ZO - never collapsed
        // to a symmetric approximation. Like Scene::Camera's own
        // projection, this carries NO backend-specific correction (e.g.
        // Vulkan's NDC Y-flip) - that stays a renderer-layer concern,
        // applied downstream by whichever Vulkan code actually
        // rasterizes with it, not baked in here.
        Core::Math::Mat4 projection = Core::Math::Mat4::Identity();
    };
}
