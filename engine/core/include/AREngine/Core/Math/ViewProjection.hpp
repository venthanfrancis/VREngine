#pragma once

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

#include <cmath>

namespace AREngine::Core::Math
{
    // A right-handed "look at" view matrix, following AREngine's world
    // convention (docs/WORLD_CONVENTIONS.md: right-handed, +Y up, -Z
    // forward). Transforms a world-space point into view space (camera
    // at the origin, looking down -Z). This is the standard RH lookAt
    // construction — not specific to any graphics API; the same view
    // matrix is correct everywhere, since only the *projection* step
    // differs between backends (see PerspectiveRH_ZO below).
    //
    // Not constexpr: relies on Normalize (which asserts/uses
    // std::sqrt), same as the rest of Vec3's non-trivial operations.
    // Added first in M8F, once Rendering (M8F's fixed demo camera)
    // genuinely needed it — see docs/ARCHITECTURE.md, "View Matrix
    // Convention (M8F)".
    [[nodiscard]] inline Mat4 LookAtRH(const Vec3& eye, const Vec3& target, const Vec3& up)
    {
        const Vec3 zAxis = Normalize(eye - target); // points from target toward eye ("backward")
        const Vec3 xAxis = Normalize(Cross(up, zAxis)); // "right"
        const Vec3 yAxis = Cross(zAxis, xAxis); // "up" - already unit length (xAxis/zAxis are orthonormal)

        Mat4 result = Mat4::Identity();
        result.Set(0, 0, xAxis.x); result.Set(0, 1, xAxis.y); result.Set(0, 2, xAxis.z);
        result.Set(1, 0, yAxis.x); result.Set(1, 1, yAxis.y); result.Set(1, 2, yAxis.z);
        result.Set(2, 0, zAxis.x); result.Set(2, 1, zAxis.y); result.Set(2, 2, zAxis.z);
        result.Set(0, 3, -Dot(xAxis, eye));
        result.Set(1, 3, -Dot(yAxis, eye));
        result.Set(2, 3, -Dot(zAxis, eye));
        return result;
    }

    // A right-handed perspective projection matrix with NDC depth in
    // [0, 1] — "RH" (right-handed) + "ZO" (zero-to-one depth range),
    // named after the mathematical convention it implements, not any
    // graphics API. This is the convention shared by Vulkan, Direct3D,
    // and Metal (as opposed to OpenGL's traditional right-handed,
    // [-1, 1]-depth "RH_NO" convention) — genuinely backend-neutral
    // Core math, on the same footing as LookAtRH above.
    //
    // Deliberately does NOT include any framebuffer-orientation
    // correction (e.g. a Y-axis flip). Whether — and how — a
    // renderer's NDC needs to be flipped to match AREngine's +Y-up
    // world convention is a property of that renderer's specific
    // screen-space mapping, not of the RH/ZO convention itself (OpenGL,
    // for instance, needs no such flip despite also being able to use
    // a right-handed view). That correction belongs in, and lives in,
    // whichever graphics backend actually needs it — see
    // engine/rendering/src/vulkan/VulkanClipSpace.hpp for Vulkan's.
    // See docs/ARCHITECTURE.md, "Core/Vulkan Clip-Space Split" for the
    // full reasoning behind this boundary.
    //
    // `fovYRadians` is the full vertical field of view. `aspect` is
    // width/height. Requires 0 < nearZ < farZ.
    [[nodiscard]] inline Mat4 PerspectiveRH_ZO(float fovYRadians, float aspect, float nearZ, float farZ)
    {
        const float focalLength = 1.0f / std::tan(fovYRadians * 0.5f);

        Mat4 result; // zero-initialized
        result.Set(0, 0, focalLength / aspect);
        result.Set(1, 1, focalLength);
        result.Set(2, 2, farZ / (nearZ - farZ));
        result.Set(2, 3, (farZ * nearZ) / (nearZ - farZ));
        result.Set(3, 2, -1.0f);
        return result;
    }

    // A right-handed, zero-to-one-depth perspective projection with an
    // asymmetric (off-center) frustum — the general case
    // PerspectiveRH_ZO's symmetric formula is a special case of. Added
    // in M9F, once real OpenXR view data (XrFovf: independent
    // angleLeft/angleRight/angleUp/angleDown) genuinely needed it — a
    // stereo headset's per-eye frustum is not generally symmetric
    // around the view's forward axis, and reducing it to one symmetric
    // vertical FOV would throw away real runtime-provided data. Angle
    // parameter order matches XrFovf's own member order
    // (angleLeft, angleRight, angleUp, angleDown) purely for call-site
    // correspondence — this function itself has no OpenXR awareness;
    // any asymmetric-FOV description (angles in radians, signed:
    // left/down conventionally negative, right/up conventionally
    // positive) can drive it.
    //
    // Derivation: at the near plane (view-space z = -nearZ), the
    // visible x-range is [nearZ*tan(angleLeft), nearZ*tan(angleRight)],
    // and must map to NDC x in [-1, +1] (same for y with up/down).
    // Solving that linear system for the near-plane's two x boundaries
    // gives m00/m02 below (same derivation, transposed, for m11/m12).
    // The depth terms (m22/m23/m32) are exactly PerspectiveRH_ZO's own
    // — off-center shear does not affect depth mapping. Substituting
    // the symmetric case (angleLeft=-angleRight, angleDown=-angleUp)
    // collapses m02/m12 to exactly 0 and m00/m11 to exactly
    // PerspectiveRH_ZO's own focalLength/aspect and focalLength —
    // verified by hand, and by TestPerspectiveOffCenterRH_ZOSymmetricCaseMatchesPerspectiveRH_ZO
    // (tests/core_tests.cpp). No Vulkan Y-flip — same policy as
    // PerspectiveRH_ZO: that correction belongs in, and lives in,
    // whichever graphics backend actually needs it.
    [[nodiscard]] inline Mat4 PerspectiveOffCenterRH_ZO(
        float angleLeftRadians, float angleRightRadians,
        float angleUpRadians, float angleDownRadians,
        float nearZ, float farZ)
    {
        const float tanLeft = std::tan(angleLeftRadians);
        const float tanRight = std::tan(angleRightRadians);
        const float tanUp = std::tan(angleUpRadians);
        const float tanDown = std::tan(angleDownRadians);

        Mat4 result; // zero-initialized
        result.Set(0, 0, 2.0f / (tanRight - tanLeft));
        result.Set(0, 2, (tanRight + tanLeft) / (tanRight - tanLeft));
        result.Set(1, 1, 2.0f / (tanUp - tanDown));
        result.Set(1, 2, (tanUp + tanDown) / (tanUp - tanDown));
        result.Set(2, 2, farZ / (nearZ - farZ));
        result.Set(2, 3, (farZ * nearZ) / (nearZ - farZ));
        result.Set(3, 2, -1.0f);
        return result;
    }
}
