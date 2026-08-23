// M8G tests for ARDemo::DemoCameraController's pure-logic pieces
// (ApplyLook, GetOrientation, ComputeNewPosition) — zero Input, zero
// Vulkan, zero GPU. Built unconditionally (not gated behind
// ARENGINE_ENABLE_VULKAN) since DemoCameraController itself has no
// Vulkan dependency — see docs/ARCHITECTURE.md, "Temporary
// Demo-Controller Decisions (M8G)".

#include "DemoCameraController.hpp"

#include "AREngine/Core/Math/MathUtil.hpp"

#include <cstdio>
#include <numbers>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    void CheckNearlyEqual(float actual, float expected, const char* description)
    {
        Check(AREngine::Core::Math::NearlyEqual(actual, expected), description);
    }

    using namespace ARDemo;
    using namespace AREngine::Core::Math;
    using namespace AREngine::Scene;

    void TestDefaultOrientationIsIdentity()
    {
        const DemoCameraController controller;
        Check(controller.GetOrientation() == Quaternion::Identity(),
              "A freshly constructed controller (yaw=pitch=0) has identity orientation");
    }

    void TestApplyLookUpdatesYawAndPitch()
    {
        DemoCameraController controller;
        controller.ApplyLook(/*mouseDeltaX=*/100.0f, /*mouseDeltaY=*/0.0f);
        Check(controller.yawRadians < 0.0f, "Mouse moving right (positive deltaX) decreases yaw (turns the camera right)");
        Check(controller.pitchRadians == 0.0f, "A purely horizontal mouse delta does not change pitch");
    }

    void TestApplyLookClampsPitch()
    {
        DemoCameraController controller;
        // An enormous downward delta should clamp, not overshoot.
        controller.ApplyLook(0.0f, 1'000'000.0f);
        CheckNearlyEqual(controller.pitchRadians, -DemoCameraController::kPitchLimitRadians,
                          "An extreme downward mouse delta clamps pitch at the negative limit, not beyond it");

        DemoCameraController controller2;
        controller2.ApplyLook(0.0f, -1'000'000.0f);
        CheckNearlyEqual(controller2.pitchRadians, DemoCameraController::kPitchLimitRadians,
                          "An extreme upward mouse delta clamps pitch at the positive limit, not beyond it");
    }

    void TestComputeNewPositionNoInputIsStationary()
    {
        const DemoCameraController controller;
        Transform transform;
        transform.position = Vec3(1.0f, 2.0f, 3.0f);

        const Vec3 result = controller.ComputeNewPosition(
            transform, false, false, false, false, false, false, /*deltaTimeSeconds=*/1.0f);
        Check(result == transform.position, "No movement input held: position is unchanged, even with dt=1s");
    }

    void TestComputeNewPositionMovesForwardAtDefaultOrientation()
    {
        DemoCameraController controller;
        controller.moveSpeedMetersPerSecond = 2.0f;
        Transform transform; // identity rotation -> forward = world -Z

        const Vec3 result = controller.ComputeNewPosition(
            transform, /*forward=*/true, false, false, false, false, false, /*deltaTimeSeconds=*/0.5f);
        // 2 m/s * 0.5s = 1m along -Z.
        CheckNearlyEqual(result.x, 0.0f, "Forward movement at default orientation: x unchanged");
        CheckNearlyEqual(result.y, 0.0f, "Forward movement at default orientation: y unchanged");
        CheckNearlyEqual(result.z, -1.0f, "Forward movement at default orientation: moves along world -Z");
    }

    void TestComputeNewPositionFollowsRotatedOrientation()
    {
        // The important case the brief explicitly calls out: "if the
        // camera turns right, pressing W should move toward where the
        // camera is looking, not always world -Z."
        DemoCameraController controller;
        controller.moveSpeedMetersPerSecond = 1.0f;

        Transform transform;
        // Yaw -90 degrees: per the sign convention established in
        // ApplyLook (mouse right -> yaw decreases -> camera turns
        // right), a negative yaw here means "turned right" — and
        // matches core_tests.cpp's already-proven fact that rotating
        // Right (+X) by +90deg around Up lands on Forward (-Z); by the
        // same rotation, -90deg around Up rotates Forward (-Z) onto
        // Right (+X). So "turned right" should move the camera along
        // world +X, not -Z.
        transform.rotation = Quaternion::FromAxisAngle(kWorldUp, -std::numbers::pi_v<float> / 2.0f);

        const Vec3 result = controller.ComputeNewPosition(
            transform, /*forward=*/true, false, false, false, false, false, /*deltaTimeSeconds=*/1.0f);
        CheckNearlyEqual(result.x, 1.0f, "After turning right, forward movement goes toward world +X, not -Z");
        CheckNearlyEqual(result.y, 0.0f, "After turning right, forward movement: y unchanged");
        CheckNearlyEqual(result.z, 0.0f, "After turning right, forward movement: z unchanged (not -1)");
    }

    void TestComputeNewPositionDiagonalIsNormalized()
    {
        DemoCameraController controller;
        controller.moveSpeedMetersPerSecond = 1.0f;
        Transform transform;

        // Forward + Right held together: without normalization this
        // would move sqrt(2) m in 1s; with it, exactly 1m total.
        const Vec3 result = controller.ComputeNewPosition(
            transform, /*forward=*/true, false, false, /*right=*/true, false, false, /*deltaTimeSeconds=*/1.0f);
        const float distanceMoved = Length(result - transform.position);
        CheckNearlyEqual(distanceMoved, 1.0f, "Diagonal (forward+right) movement is normalized to exactly moveSpeed * dt, not faster");
    }

    void TestComputeNewPositionUpUsesWorldUpRegardlessOfPitch()
    {
        DemoCameraController controller;
        controller.moveSpeedMetersPerSecond = 1.0f;

        Transform transform;
        // Pitched to look straight-ish down - if Up/Down used the
        // camera's local up instead of world up, this would move
        // mostly horizontally instead of straight up.
        transform.rotation = Quaternion::FromAxisAngle(kWorldRight, -1.2f);

        const Vec3 result = controller.ComputeNewPosition(
            transform, false, false, false, false, /*up=*/true, false, /*deltaTimeSeconds=*/1.0f);
        CheckNearlyEqual(result.x, 0.0f, "Up movement while pitched down: x unchanged (world Up, not local up)");
        CheckNearlyEqual(result.y, 1.0f, "Up movement while pitched down: moves 1m along world +Y regardless of pitch");
        CheckNearlyEqual(result.z, 0.0f, "Up movement while pitched down: z unchanged");
    }
}

int main()
{
    TestDefaultOrientationIsIdentity();
    TestApplyLookUpdatesYawAndPitch();
    TestApplyLookClampsPitch();
    TestComputeNewPositionNoInputIsStationary();
    TestComputeNewPositionMovesForwardAtDefaultOrientation();
    TestComputeNewPositionFollowsRotatedOrientation();
    TestComputeNewPositionDiagonalIsNormalized();
    TestComputeNewPositionUpUsesWorldUpRegardlessOfPitch();

    if (g_failureCount == 0)
    {
        std::printf("All DemoCameraController (pure-logic) M8G checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
