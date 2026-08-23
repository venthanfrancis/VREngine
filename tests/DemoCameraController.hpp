#pragma once

// M8G's manual Vulkan demo support code — NOT part of the permanent
// engine. See docs/ARCHITECTURE.md, "Temporary Demo-Controller
// Decisions (M8G)". Deliberately has zero Vulkan dependency (only
// Core/Scene/Input) so it — and its pure-logic pieces — can be
// unit-tested without a GPU (see tests/demo_camera_controller_tests.cpp,
// built unconditionally, not gated behind ARENGINE_ENABLE_VULKAN).

#include "AREngine/Core/Math/MathUtil.hpp"
#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec3.hpp"
#include "AREngine/Input/Input.hpp"
#include "AREngine/Scene/Transform.hpp"

#include <algorithm>

namespace ARDemo
{
    // A small, deliberately temporary free-fly camera controller.
    // Mixes Input *policy* (which keys/buttons mean what) with motion
    // math — exactly why this stays out of Scene::Camera (pure
    // projection data, no idea Input exists) and out of Scene::Transform
    // (pure pose data). A permanent camera-control system, if one is
    // ever built, would very likely look different once it has to
    // support gameplay-specific bindings, controller/gamepad input, and
    // XR — none of which this demo needs to guess at.
    //
    // Yaw/pitch are stored here as plain floats specifically so they can
    // be incremented smoothly frame to frame without decomposing a
    // quaternion back into angles every time — Quaternion remains
    // AREngine's one canonical rotation representation regardless;
    // GetOrientation() below is the only place these floats become a
    // real Quaternion, written into a Scene::Transform. Both fields are
    // radians, consistently — never degrees, anywhere in this type.
    struct DemoCameraController
    {
        float yawRadians = 0.0f;
        float pitchRadians = 0.0f;

        float moveSpeedMetersPerSecond = 4.0f;
        // Radians of yaw/pitch per pixel of raw mouse movement.
        float lookSensitivityRadiansPerPixel = 0.0025f;

        // ~89 degrees, radians — stops the camera from ever looking
        // exactly straight up/down, where yaw and pitch would become
        // ambiguous (gimbal-like) and LookAtRH's cross product (Right =
        // worldUp x Forward) would approach zero length. See
        // docs/ARCHITECTURE.md, "Pitch Limit (M8G)".
        static constexpr float kPitchLimitRadians = 1.5533430343f; // 89 deg

        // Pure logic: applies one frame's raw mouse delta (the same
        // pixel units InputSystem::GetMouseDelta() reports: +x right,
        // +y down) to yaw/pitch, clamping pitch. No Input dependency —
        // directly unit-testable with synthetic deltas. See
        // docs/ARCHITECTURE.md, "Yaw/Pitch Sign Convention (M8G)" for
        // the worked-out reasoning behind the signs below.
        void ApplyLook(float mouseDeltaX, float mouseDeltaY)
        {
            yawRadians -= mouseDeltaX * lookSensitivityRadiansPerPixel;
            pitchRadians -= mouseDeltaY * lookSensitivityRadiansPerPixel;
            pitchRadians = std::clamp(pitchRadians, -kPitchLimitRadians, kPitchLimitRadians);
        }

        // Pure logic: the orientation quaternion for the current yaw/
        // pitch — yaw around world Up, pitch around local Right,
        // composed as `yaw * pitch` (pitch applied first, in the not-
        // yet-yawed frame, then yaw laid on top) — the standard FPS-
        // camera composition order; see Quaternion::operator*'s doc
        // comment for why that order is what "compose" means here.
        [[nodiscard]] AREngine::Core::Math::Quaternion GetOrientation() const
        {
            using namespace AREngine::Core::Math;
            const Quaternion yaw = Quaternion::FromAxisAngle(kWorldUp, yawRadians);
            const Quaternion pitch = Quaternion::FromAxisAngle(kWorldRight, pitchRadians);
            return yaw * pitch;
        }

        // Pure logic: the new position after moving for
        // `deltaTimeSeconds`, given which movement inputs are currently
        // held. Forward/backward/left/right follow `transform`'s
        // CURRENT orientation — a free-fly camera, not a yaw-locked
        // character controller — see docs/ARCHITECTURE.md, "Forward
        // Movement Follows Orientation (M8G)". Up/down deliberately use
        // world Up, not the camera's local up, so looking up/down
        // doesn't change what Space/Ctrl do — the standard "editor fly
        // camera" convention. No Input dependency — directly
        // unit-testable.
        [[nodiscard]] AREngine::Core::Math::Vec3 ComputeNewPosition(
            const AREngine::Scene::Transform& transform,
            bool moveForward, bool moveBackward, bool moveLeft, bool moveRight,
            bool moveUp, bool moveDown, float deltaTimeSeconds) const
        {
            using namespace AREngine::Core::Math;

            Vec3 direction(0.0f, 0.0f, 0.0f);
            if (moveForward)  { direction = direction + transform.GetForward(); }
            if (moveBackward) { direction = direction - transform.GetForward(); }
            if (moveRight)    { direction = direction + transform.GetRight(); }
            if (moveLeft)     { direction = direction - transform.GetRight(); }
            if (moveUp)       { direction = direction + kWorldUp; }
            if (moveDown)     { direction = direction - kWorldUp; }

            // Only normalize a nonzero direction — Normalize asserts on
            // a zero-length vector, and "no movement keys held" is a
            // completely normal case here, not an error.
            if (Length(direction) > kEpsilon)
            {
                direction = Normalize(direction);
            }

            return transform.position + direction * moveSpeedMetersPerSecond * deltaTimeSeconds;
        }

        // Impure: reads InputSystem, writes `transform` in place. The
        // ONLY place this type touches Input::InputSystem — everything
        // above is pure and independently testable. Look is gated on
        // the right mouse button (click-drag), not always-on — see
        // docs/ARCHITECTURE.md, "Mouse Look (M8G)" for why.
        void Update(AREngine::Scene::Transform& transform, const AREngine::Input::InputSystem& input, float deltaTimeSeconds)
        {
            using namespace AREngine::Core;

            if (input.IsMouseButtonDown(MouseButton::Right))
            {
                const Math::Vec2 delta = input.GetMouseDelta();
                ApplyLook(delta.x, delta.y);
            }
            transform.rotation = GetOrientation();

            transform.position = ComputeNewPosition(
                transform,
                input.IsKeyDown(KeyCode::W), input.IsKeyDown(KeyCode::S),
                input.IsKeyDown(KeyCode::A), input.IsKeyDown(KeyCode::D),
                input.IsKeyDown(KeyCode::Space), input.IsKeyDown(KeyCode::LeftCtrl),
                deltaTimeSeconds);
        }
    };
}
