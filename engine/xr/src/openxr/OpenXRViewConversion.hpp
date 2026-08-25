#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: converting an XrView's pose/FOV
// into AREngine-owned math types and Frame::ViewInfo is a pure OpenXR
// + Core::Math concept, independent of which graphics API backs the
// session. See docs/ARCHITECTURE.md, "OpenXR/AREngine Coordinate
// Compatibility (M9F)" and "Quaternion Conversion (M9F)".

#include "AREngine/Core/Math/ViewProjection.hpp"
#include "AREngine/Frame/ViewInfo.hpp"

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // XrVector3f -> Core::Math::Vec3. No axis conversion: OpenXR
    // reference spaces are right-handed, +Y up, -Z forward, in meters
    // (per the OpenXR specification's own coordinate-system
    // definition) - identical to AREngine's world convention
    // (docs/WORLD_CONVENTIONS.md). A straight field-for-field copy is
    // therefore correct; this was verified against the spec before
    // writing this function; it is not an unchecked assumption. Pure
    // logic, directly unit-testable.
    [[nodiscard]] constexpr Core::Math::Vec3 ConvertXrPosition(const XrVector3f& position)
    {
        return Core::Math::Vec3(position.x, position.y, position.z);
    }

    // XrQuaternionf -> Core::Math::Quaternion. This is a component-
    // REORDER, not a coordinate-system conversion (see
    // ConvertXrPosition above - no axis flip is involved anywhere in
    // this file): XrQuaternionf stores {x, y, z, w}, while
    // AREngine::Core::Math::Quaternion is Hamilton {w, x, y, z} and its
    // constructor takes (w, x, y, z) in that order. Confirmed by
    // reading Quaternion.hpp before writing this - never assumed. Pure
    // logic, directly unit-testable (with a deliberately per-component-
    // asymmetric test quaternion, not just identity, so a reorder bug
    // would actually be caught).
    [[nodiscard]] constexpr Core::Math::Quaternion ConvertXrOrientation(const XrQuaternionf& orientation)
    {
        return Core::Math::Quaternion(orientation.w, orientation.x, orientation.y, orientation.z);
    }

    // Converts one located XrView (real pose + real asymmetric FOV)
    // into a generic Frame::ViewInfo. `nearZ`/`farZ` are engine/
    // application clip-distance policy - OpenXR's XrFovf provides
    // angular FOV only, never near/far distances, so these must come
    // from the caller, not be invented here. The projection is built
    // via Core::Math::PerspectiveOffCenterRH_ZO directly from
    // `view.fov`'s four independent angles - never collapsed to a
    // symmetric approximation, and never Vulkan-Y-flipped (see
    // ViewInfo.hpp's own documentation for why that stays a downstream
    // renderer-layer concern). Pure logic, directly unit-testable.
    [[nodiscard]] inline Frame::ViewInfo ConvertXrViewToViewInfo(const XrView& view, float nearZ, float farZ)
    {
        Frame::ViewInfo result;
        result.position = ConvertXrPosition(view.pose.position);
        result.orientation = ConvertXrOrientation(view.pose.orientation);
        result.projection = Core::Math::PerspectiveOffCenterRH_ZO(
            view.fov.angleLeft, view.fov.angleRight, view.fov.angleUp, view.fov.angleDown, nearZ, farZ);
        return result;
    }

    // True only when the runtime reports BOTH orientation and position
    // as valid for this xrLocateViews call - per the OpenXR spec, pose
    // data must not be used at all when either VALID bit is unset. The
    // *_TRACKED bits are deliberately NOT required here: the spec
    // permits a runtime to report VALID-but-not-TRACKED (a "last known
    // good" pose during a brief tracking interruption), which is still
    // legitimate, usable data - not a hard failure. Pure logic, directly
    // unit-testable.
    [[nodiscard]] constexpr bool IsViewStateValid(XrViewStateFlags flags)
    {
        constexpr XrViewStateFlags kRequired = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        return (flags & kRequired) == kRequired;
    }
}
