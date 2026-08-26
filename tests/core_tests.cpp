// M1 tests for AREngine::Core: math (Vec3, Mat4, Quaternion) and the
// Event base type. No external test framework — CTest just checks this
// executable's exit code (0 = all checks passed).

#include "AREngine/Core/Core.hpp"

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

    using namespace AREngine::Core::Math;

    void TestVec3()
    {
        const Vec3 a(1.0f, 2.0f, 3.0f);
        const Vec3 b(4.0f, 5.0f, 6.0f);

        Check((a + b) == Vec3(5.0f, 7.0f, 9.0f), "Vec3 addition");
        Check((a - b) == Vec3(-3.0f, -3.0f, -3.0f), "Vec3 subtraction");
        Check(Dot(a, b) == 32.0f, "Vec3 dot product"); // 1*4 + 2*5 + 3*6 = 32

        CheckNearlyEqual(Length(Vec3(3.0f, 4.0f, 0.0f)), 5.0f, "Vec3 length (3-4-5 triangle)");

        const Vec3 normalized = Normalize(Vec3(3.0f, 4.0f, 0.0f));
        CheckNearlyEqual(Length(normalized), 1.0f, "Vec3 normalize produces unit length");

        // Right-handed basis check — see docs/WORLD_CONVENTIONS.md.
        //
        // This engine defines Right = +X, Up = +Y, Forward = -Z. In a
        // standard right-handed system, X cross Y = Z. Because Forward
        // is -Z (not +Z) here, Cross(Right, Up) works out to +Z — the
        // OPPOSITE of Forward, not Forward itself. This is expected,
        // not a bug: it's the same relationship OpenGL-style
        // right-handed, Y-up, look-down-negative-Z conventions have.
        const Vec3 crossRightUp = Cross(kWorldRight, kWorldUp);
        Check(crossRightUp == Vec3(0.0f, 0.0f, 1.0f), "Cross(Right, Up) == +Z");
        Check(crossRightUp == -kWorldForward, "Cross(Right, Up) == -Forward");
    }

    void TestMat4()
    {
        const Mat4 identity = Mat4::Identity();
        Check(identity.At(0, 0) == 1.0f && identity.At(1, 1) == 1.0f &&
              identity.At(2, 2) == 1.0f && identity.At(3, 3) == 1.0f,
              "Mat4 identity has 1s on the diagonal");
        Check(identity.At(0, 1) == 0.0f && identity.At(1, 0) == 0.0f,
              "Mat4 identity has 0s off the diagonal");
        Check((identity * identity) == identity, "Mat4 multiplication: identity * identity == identity");

        // A translation-by-5-on-X matrix. identity*identity alone can't
        // catch a row/column indexing bug (identity is symmetric); this
        // matrix isn't, so it can.
        Mat4 translateX5 = Mat4::Identity();
        translateX5.Set(0, 3, 5.0f);

        const Mat4 translateX10 = translateX5 * translateX5;
        Check(translateX10.At(0, 3) == 10.0f, "Mat4 multiplication composes two translations (5 + 5 == 10)");
        Check(translateX10.At(1, 3) == 0.0f && translateX10.At(2, 3) == 0.0f,
              "Mat4 multiplication leaves other translation components at 0");
    }

    void TestQuaternion()
    {
        const Quaternion identity = Quaternion::Identity();
        Check(identity == Quaternion(1.0f, 0.0f, 0.0f, 0.0f), "Quaternion identity is (1,0,0,0)");
        Check(Quaternion() == Quaternion::Identity(), "Default-constructed Quaternion is identity");
    }

    void TestQuaternionFromAxisAngle()
    {
        const Quaternion noRotation = Quaternion::FromAxisAngle(kWorldUp, 0.0f);
        CheckNearlyEqual(noRotation.w, 1.0f, "FromAxisAngle(axis, 0) has w == 1");
        CheckNearlyEqual(noRotation.x, 0.0f, "FromAxisAngle(axis, 0) has x == 0");
        CheckNearlyEqual(noRotation.y, 0.0f, "FromAxisAngle(axis, 0) has y == 0");
        CheckNearlyEqual(noRotation.z, 0.0f, "FromAxisAngle(axis, 0) has z == 0");

        // 90 degrees around +Y: w = cos(45deg), y = sin(45deg), x = z = 0.
        const float halfPi = std::numbers::pi_v<float> / 2.0f;
        const Quaternion quarterTurnAroundY = Quaternion::FromAxisAngle(kWorldUp, halfPi);
        const float expected = std::numbers::sqrt2_v<float> / 2.0f; // cos(45deg) == sin(45deg)
        CheckNearlyEqual(quarterTurnAroundY.w, expected, "FromAxisAngle(Up, 90deg) has the expected w");
        CheckNearlyEqual(quarterTurnAroundY.y, expected, "FromAxisAngle(Up, 90deg) has the expected y");
        CheckNearlyEqual(quarterTurnAroundY.x, 0.0f, "FromAxisAngle(Up, 90deg) leaves x at 0");
        CheckNearlyEqual(quarterTurnAroundY.z, 0.0f, "FromAxisAngle(Up, 90deg) leaves z at 0");
    }

    void TestQuaternionMultiplication()
    {
        const Quaternion identity = Quaternion::Identity();
        const float halfPi = std::numbers::pi_v<float> / 2.0f;
        const Quaternion yaw90 = Quaternion::FromAxisAngle(kWorldUp, halfPi);

        Check(identity * yaw90 == yaw90, "identity * q == q");
        Check(yaw90 * identity == yaw90, "q * identity == q");

        // Two 90-degree turns around the same axis compose into one
        // 180-degree turn — an easy-to-verify concrete case.
        const Quaternion yaw180 = yaw90 * yaw90;
        const Quaternion expectedYaw180 = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float>);
        CheckNearlyEqual(yaw180.w, expectedYaw180.w, "Two composed 90deg yaws: w matches one 180deg yaw");
        CheckNearlyEqual(yaw180.y, expectedYaw180.y, "Two composed 90deg yaws: y matches one 180deg yaw");
    }

    void TestQuaternionRotate()
    {
        // Reuses the exact fact already proven at the Mat4 level in
        // TestMat4TransformFactories: rotating Right (+X) by 90deg
        // around Up (+Y) lands on Forward (-Z). Proving Quaternion's
        // own Rotate() gives the same answer confirms the two rotation
        // representations agree, not just that each is internally
        // consistent.
        const float halfPi = std::numbers::pi_v<float> / 2.0f;
        const Quaternion yaw90 = Quaternion::FromAxisAngle(kWorldUp, halfPi);
        const Vec3 rotatedRight = Rotate(yaw90, kWorldRight);
        CheckNearlyEqual(rotatedRight.x, kWorldForward.x, "Quaternion Rotate: Right rotated 90deg around Up lands on Forward.x");
        CheckNearlyEqual(rotatedRight.y, kWorldForward.y, "Quaternion Rotate: Right rotated 90deg around Up lands on Forward.y");
        CheckNearlyEqual(rotatedRight.z, kWorldForward.z, "Quaternion Rotate: Right rotated 90deg around Up lands on Forward.z");

        Check(Rotate(Quaternion::Identity(), kWorldForward) == kWorldForward, "Rotate by identity leaves a vector unchanged");
    }

    void TestQuaternionConjugate()
    {
        // Added in M9G, for deriving a view matrix from a located
        // view's pose. Identity is its own conjugate/inverse.
        Check(Conjugate(Quaternion::Identity()) == Quaternion::Identity(), "Conjugate of identity is identity");

        // A deliberately per-component-nonzero quaternion (not
        // identity, so this would actually catch a sign-flip-on-the-
        // wrong-component bug).
        const Quaternion q(0.5f, 0.1f, 0.2f, 0.3f);
        const Quaternion conjugate = Conjugate(q);
        CheckNearlyEqual(conjugate.w, 0.5f, "Conjugate: w unchanged");
        CheckNearlyEqual(conjugate.x, -0.1f, "Conjugate: x negated");
        CheckNearlyEqual(conjugate.y, -0.2f, "Conjugate: y negated");
        CheckNearlyEqual(conjugate.z, -0.3f, "Conjugate: z negated");

        // The defining property that makes Conjugate usable as a
        // rotation inverse: q * Conjugate(q) == Identity, for a unit
        // quaternion.
        const Quaternion yaw90 = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f);
        const Quaternion shouldBeIdentity = yaw90 * Conjugate(yaw90);
        CheckNearlyEqual(shouldBeIdentity.w, 1.0f, "q * Conjugate(q): w is 1 (identity)");
        CheckNearlyEqual(shouldBeIdentity.x, 0.0f, "q * Conjugate(q): x is 0 (identity)");
        CheckNearlyEqual(shouldBeIdentity.y, 0.0f, "q * Conjugate(q): y is 0 (identity)");
        CheckNearlyEqual(shouldBeIdentity.z, 0.0f, "q * Conjugate(q): z is 0 (identity)");
    }

    void TestMat4TransformFactories()
    {
        const Mat4 translation = Mat4::Translation(Vec3(2.0f, 3.0f, 4.0f));
        Check(translation.At(0, 3) == 2.0f && translation.At(1, 3) == 3.0f && translation.At(2, 3) == 4.0f,
              "Mat4::Translation places its offset in the translation column");
        Check(translation.At(0, 0) == 1.0f && translation.At(1, 1) == 1.0f && translation.At(2, 2) == 1.0f,
              "Mat4::Translation leaves the diagonal at 1");

        const Mat4 scale = Mat4::Scale(Vec3(2.0f, 3.0f, 4.0f));
        Check(scale.At(0, 0) == 2.0f && scale.At(1, 1) == 3.0f && scale.At(2, 2) == 4.0f && scale.At(3, 3) == 1.0f,
              "Mat4::Scale places its factors on the diagonal");
        Check(scale.At(0, 1) == 0.0f && scale.At(1, 0) == 0.0f,
              "Mat4::Scale leaves off-diagonal entries at 0");

        Check(Mat4::Rotation(Quaternion::Identity()) == Mat4::Identity(),
              "Mat4::Rotation(identity quaternion) == Mat4::Identity()");

        // Core-level proof that Mat4::Rotation respects this engine's
        // handedness (not just tested indirectly through Scene, per
        // M5's design guidance): rotating world Right (+X) by 90
        // degrees around world Up (+Y) should land on -Z, i.e. World
        // Forward — see docs/WORLD_CONVENTIONS.md.
        const float halfPi = std::numbers::pi_v<float> / 2.0f;
        const Quaternion quarterTurnAroundY = Quaternion::FromAxisAngle(kWorldUp, halfPi);
        const Mat4 rotationMatrix = Mat4::Rotation(quarterTurnAroundY);
        const Vec3 rotatedRight = TransformPoint(rotationMatrix, kWorldRight);
        CheckNearlyEqual(rotatedRight.x, kWorldForward.x, "Rotating Right 90deg around Up lands on Forward.x");
        CheckNearlyEqual(rotatedRight.y, kWorldForward.y, "Rotating Right 90deg around Up lands on Forward.y");
        CheckNearlyEqual(rotatedRight.z, kWorldForward.z, "Rotating Right 90deg around Up lands on Forward.z");

        // TRS with a pure translation: "2 meters forward" should be
        // exactly position (0, 0, -2) after composition.
        const Mat4 trs = Mat4::TRS(Vec3(0.0f, 0.0f, -2.0f), Quaternion::Identity(), Vec3(1.0f, 1.0f, 1.0f));
        const Vec3 worldOrigin = TransformPoint(trs, Vec3(0.0f, 0.0f, 0.0f));
        Check(worldOrigin == Vec3(0.0f, 0.0f, -2.0f), "TRS translation of -2 on Z places a point 2 meters forward");

        // Multiplication order genuinely matters — proven, not assumed.
        // Translate-then-scale and scale-then-translate give different
        // results for the same point.
        const Mat4 translateThenScale = Mat4::Translation(Vec3(1.0f, 0.0f, 0.0f)) * Mat4::Scale(Vec3(2.0f, 2.0f, 2.0f));
        const Mat4 scaleThenTranslate = Mat4::Scale(Vec3(2.0f, 2.0f, 2.0f)) * Mat4::Translation(Vec3(1.0f, 0.0f, 0.0f));
        const Vec3 pointA = TransformPoint(translateThenScale, Vec3(1.0f, 0.0f, 0.0f));
        const Vec3 pointB = TransformPoint(scaleThenTranslate, Vec3(1.0f, 0.0f, 0.0f));
        Check(pointA == Vec3(3.0f, 0.0f, 0.0f), "Translation * Scale applied to (1,0,0) gives (3,0,0)");
        Check(pointB == Vec3(4.0f, 0.0f, 0.0f), "Scale * Translation applied to (1,0,0) gives (4,0,0) - a different result");
        Check(!(pointA == pointB), "Matrix multiplication order changes the result, as expected");
    }

    void TestLookAtRH()
    {
        // Camera at (0,0,3) looking at the origin, matching the world
        // convention's -Z forward: the target should end up exactly 3
        // meters in front of the camera in view space (0,0,-3), and the
        // camera's own position should map to the view-space origin.
        const Vec3 eye(0.0f, 0.0f, 3.0f);
        const Vec3 target(0.0f, 0.0f, 0.0f);
        const Mat4 view = LookAtRH(eye, target, kWorldUp);

        const Vec3 targetInView = TransformPoint(view, target);
        CheckNearlyEqual(targetInView.x, 0.0f, "LookAtRH: target.x is centered in view space");
        CheckNearlyEqual(targetInView.y, 0.0f, "LookAtRH: target.y is centered in view space");
        CheckNearlyEqual(targetInView.z, -3.0f, "LookAtRH: target is 3 meters in front (forward = -Z)");

        const Vec3 eyeInView = TransformPoint(view, eye);
        CheckNearlyEqual(eyeInView.x, 0.0f, "LookAtRH: the eye itself maps to the view-space origin (x)");
        CheckNearlyEqual(eyeInView.y, 0.0f, "LookAtRH: the eye itself maps to the view-space origin (y)");
        CheckNearlyEqual(eyeInView.z, 0.0f, "LookAtRH: the eye itself maps to the view-space origin (z)");
    }

    void TestPerspectiveRH_ZO()
    {
        // fovY = 90deg, aspect = 1 -> focalLength = 1/tan(45deg) = 1,
        // chosen so the resulting matrix entries are easy to verify by
        // hand.
        const float fovY = std::numbers::pi_v<float> / 2.0f;
        const Mat4 proj = PerspectiveRH_ZO(fovY, /*aspect=*/1.0f, /*nearZ=*/1.0f, /*farZ=*/10.0f);

        CheckNearlyEqual(proj.At(0, 0), 1.0f, "PerspectiveRH_ZO: X scale is focalLength/aspect");
        // No graphics-API-specific Y flip here — that's a Vulkan-layer
        // concern (VulkanClipSpace::ApplyVulkanYFlip), not part of the
        // RH/ZO convention itself. See docs/ARCHITECTURE.md,
        // "Core/Vulkan Clip-Space Split".
        CheckNearlyEqual(proj.At(1, 1), 1.0f, "PerspectiveRH_ZO: Y scale is POSITIVE focalLength - no API-specific flip in Core");

        // Zero-to-one depth range ("ZO"), not OpenGL's [-1,1]: a point
        // on the near plane (view-space z = -near) must map to NDC
        // depth 0; a point on the far plane (view-space z = -far) must
        // map to NDC depth 1.
        const Vec4 nearPoint = proj * Vec4(0.0f, 0.0f, -1.0f, 1.0f); // view-space near plane
        CheckNearlyEqual(nearPoint.w, 1.0f, "PerspectiveRH_ZO: near-plane point has clip w = -viewZ = 1");
        CheckNearlyEqual(nearPoint.z / nearPoint.w, 0.0f, "PerspectiveRH_ZO: near plane maps to NDC depth 0");

        const Vec4 farPoint = proj * Vec4(0.0f, 0.0f, -10.0f, 1.0f); // view-space far plane
        CheckNearlyEqual(farPoint.w, 10.0f, "PerspectiveRH_ZO: far-plane point has clip w = -viewZ = 10");
        CheckNearlyEqual(farPoint.z / farPoint.w, 1.0f, "PerspectiveRH_ZO: far plane maps to NDC depth 1");

        // Without any API-specific flip, a point straight "up" from the
        // camera (positive view-space Y) lands at POSITIVE NDC Y - the
        // ordinary, un-flipped mathematical result. (Vulkan's backend-
        // specific flip is tested separately, in
        // tests/vulkan_tests.cpp, alongside ApplyVulkanYFlip itself.)
        const Vec4 upPoint = proj * Vec4(0.0f, 1.0f, -1.0f, 1.0f);
        Check(upPoint.y / upPoint.w > 0.0f, "PerspectiveRH_ZO: a world-space 'up' point has POSITIVE NDC y (no flip)");
    }

    void TestPerspectiveOffCenterRH_ZOSymmetricCaseMatchesPerspectiveRH_ZO()
    {
        // fovY=90deg, aspect=1 -> symmetric half-angle 45deg on every
        // side. PerspectiveOffCenterRH_ZO given exactly those four
        // angles must reproduce PerspectiveRH_ZO's own matrix exactly -
        // the symmetric formula is a special case of the general
        // off-center one, not a separate, independently-derived
        // formula. See ViewProjection.hpp's own derivation comment.
        const float half = std::numbers::pi_v<float> / 4.0f; // 45deg
        const Mat4 symmetric = PerspectiveRH_ZO(std::numbers::pi_v<float> / 2.0f, 1.0f, 1.0f, 10.0f);
        const Mat4 offCenter = PerspectiveOffCenterRH_ZO(-half, half, half, -half, 1.0f, 10.0f);

        for (std::size_t row = 0; row < 4; ++row)
        {
            for (std::size_t col = 0; col < 4; ++col)
            {
                CheckNearlyEqual(offCenter.At(row, col), symmetric.At(row, col),
                                  "PerspectiveOffCenterRH_ZO: symmetric angles reproduce PerspectiveRH_ZO exactly");
            }
        }
    }

    void TestPerspectiveOffCenterRH_ZOAsymmetricFrustumBoundaries()
    {
        // A genuinely asymmetric frustum - none of the four angles are
        // mirror images of their opposite. Each frustum boundary, at
        // the near plane, must map to exactly the corresponding NDC
        // boundary (+-1) - proving the asymmetry is preserved and
        // correctly positioned, not silently collapsed toward a
        // symmetric center.
        constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
        const float angleLeft = -30.0f * kDegToRad;
        const float angleRight = 60.0f * kDegToRad;
        const float angleUp = 45.0f * kDegToRad;
        const float angleDown = -20.0f * kDegToRad;
        const float nearZ = 1.0f;
        const float farZ = 10.0f;
        const Mat4 proj = PerspectiveOffCenterRH_ZO(angleLeft, angleRight, angleUp, angleDown, nearZ, farZ);

        // Asymmetry actually preserved, not collapsed to a symmetric
        // matrix (m02/m12 would both be exactly 0 in the symmetric
        // case - see the test above).
        Check(proj.At(0, 2) != 0.0f, "PerspectiveOffCenterRH_ZO: horizontal asymmetry produces a non-zero X/Z skew term");
        Check(proj.At(1, 2) != 0.0f, "PerspectiveOffCenterRH_ZO: vertical asymmetry produces a non-zero Y/Z skew term");

        const Vec4 leftEdge = proj * Vec4(nearZ * std::tan(angleLeft), 0.0f, -nearZ, 1.0f);
        CheckNearlyEqual(leftEdge.x / leftEdge.w, -1.0f, "PerspectiveOffCenterRH_ZO: left frustum boundary maps to NDC x=-1");

        const Vec4 rightEdge = proj * Vec4(nearZ * std::tan(angleRight), 0.0f, -nearZ, 1.0f);
        CheckNearlyEqual(rightEdge.x / rightEdge.w, 1.0f, "PerspectiveOffCenterRH_ZO: right frustum boundary maps to NDC x=+1");

        const Vec4 topEdge = proj * Vec4(0.0f, nearZ * std::tan(angleUp), -nearZ, 1.0f);
        CheckNearlyEqual(topEdge.y / topEdge.w, 1.0f, "PerspectiveOffCenterRH_ZO: top frustum boundary maps to NDC y=+1");

        const Vec4 bottomEdge = proj * Vec4(0.0f, nearZ * std::tan(angleDown), -nearZ, 1.0f);
        CheckNearlyEqual(bottomEdge.y / bottomEdge.w, -1.0f, "PerspectiveOffCenterRH_ZO: bottom frustum boundary maps to NDC y=-1");
    }

    void TestPerspectiveOffCenterRH_ZODepthMapping()
    {
        // Depth mapping must be exactly PerspectiveRH_ZO's own ZO
        // convention (near -> NDC depth 0, far -> NDC depth 1) -
        // off-center shear must not perturb it, since it only affects
        // X/Y, not Z/W.
        constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
        const Mat4 proj = PerspectiveOffCenterRH_ZO(-30.0f * kDegToRad, 60.0f * kDegToRad, 45.0f * kDegToRad, -20.0f * kDegToRad, 2.0f, 20.0f);

        const Vec4 nearPoint = proj * Vec4(0.0f, 0.0f, -2.0f, 1.0f);
        CheckNearlyEqual(nearPoint.z / nearPoint.w, 0.0f, "PerspectiveOffCenterRH_ZO: near plane maps to NDC depth 0");

        const Vec4 farPoint = proj * Vec4(0.0f, 0.0f, -20.0f, 1.0f);
        CheckNearlyEqual(farPoint.z / farPoint.w, 1.0f, "PerspectiveOffCenterRH_ZO: far plane maps to NDC depth 1");
    }

    void TestViewMatrixFromPoseRH()
    {
        // The milestone's own worked example: eye offset along +X only,
        // identity orientation - transforming a world point should just
        // cancel the eye's own X offset, changing nothing else.
        const Mat4 view = ViewMatrixFromPoseRH(Vec3(1.0f, 0.0f, 0.0f), Quaternion::Identity());
        const Vec3 viewPoint = TransformPoint(view, Vec3(1.0f, 0.0f, -2.0f));
        CheckNearlyEqual(viewPoint.x, 0.0f, "ViewMatrixFromPoseRH: eye's own X offset is cancelled");
        CheckNearlyEqual(viewPoint.y, 0.0f, "ViewMatrixFromPoseRH: Y unchanged");
        CheckNearlyEqual(viewPoint.z, -2.0f, "ViewMatrixFromPoseRH: Z unchanged (identity orientation)");

        // Sanity: transforming the eye's own world position must always
        // land at the view-space origin, regardless of pose - this is
        // what "view space is centered on the eye" means.
        const Vec3 eyeInView = TransformPoint(view, Vec3(1.0f, 0.0f, 0.0f));
        CheckNearlyEqual(eyeInView.x, 0.0f, "ViewMatrixFromPoseRH: the eye itself maps to the view-space origin (x)");
        CheckNearlyEqual(eyeInView.y, 0.0f, "ViewMatrixFromPoseRH: the eye itself maps to the view-space origin (y)");
        CheckNearlyEqual(eyeInView.z, 0.0f, "ViewMatrixFromPoseRH: the eye itself maps to the view-space origin (z)");
    }

    void TestViewMatrixFromPoseRHRotated()
    {
        // Eye at the origin, yawed 90 degrees (reusing the exact yaw90
        // already established in TestQuaternionRotate: Right rotates
        // onto Forward - i.e. this eye's own world-space forward
        // direction is now -kWorldRight = (-1,0,0)). A world point
        // directly in front of this rotated eye must land on view-space
        // -Z ("straight ahead"), proving rotation - not just
        // translation - is correctly inverted.
        const Quaternion yaw90 = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f);
        const Mat4 view = ViewMatrixFromPoseRH(Vec3(0.0f, 0.0f, 0.0f), yaw90);
        const Vec3 viewPoint = TransformPoint(view, Vec3(-1.0f, 0.0f, 0.0f));
        CheckNearlyEqual(viewPoint.x, 0.0f, "ViewMatrixFromPoseRH (rotated): point directly ahead has view-space x=0");
        CheckNearlyEqual(viewPoint.y, 0.0f, "ViewMatrixFromPoseRH (rotated): point directly ahead has view-space y=0");
        CheckNearlyEqual(viewPoint.z, -1.0f, "ViewMatrixFromPoseRH (rotated): point directly ahead is at view-space -Z (in front)");
    }

    void TestMvpMultiplicationOrderMatters()
    {
        // model translates the origin to (1,0,0); "view" here is a pure
        // 90-degree yaw (testing composition order itself, not full
        // view-matrix semantics - reuses the exact yaw90 fact already
        // established in TestQuaternionRotate/TestMat4TransformFactories);
        // projection is identity. The CORRECT order (projection*view*model)
        // translates first, then rotates - the translated point actually
        // moves under rotation. The WRONG order (model*view*projection)
        // rotates first (rotating the origin does nothing), then
        // translates - a completely different result. This tells the
        // two orders apart by their actual output, not just by
        // asserting the intended one "works."
        const Mat4 model = Mat4::Translation(Vec3(1.0f, 0.0f, 0.0f));
        const Mat4 view = Mat4::Rotation(Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f));
        const Mat4 projection = Mat4::Identity();

        const Vec4 correctResult = (projection * view * model) * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        CheckNearlyEqual(correctResult.x, 0.0f, "MVP order (proj*view*model): translated-then-rotated point has x=0");
        CheckNearlyEqual(correctResult.z, -1.0f, "MVP order (proj*view*model): translated-then-rotated point has z=-1");

        const Vec4 wrongResult = (model * view * projection) * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        CheckNearlyEqual(wrongResult.x, 1.0f, "MVP order (WRONG model*view*proj): rotated-then-translated origin has x=1 (visibly different)");
        CheckNearlyEqual(wrongResult.z, 0.0f, "MVP order (WRONG model*view*proj): rotated-then-translated origin has z=0 (visibly different)");
    }

    void TestTwoEyePosesProduceDifferentMvps()
    {
        // Same model, same projection, two different eye positions (a
        // small, IPD-scale offset - mirroring the real M9F/M9G evidence:
        // two views, only their pose differs) - the resulting MVPs must
        // differ, proving the "one model, N views" pipeline actually
        // uses each view's own pose, not accidentally sharing one view
        // matrix across both eyes.
        const Mat4 model = Mat4::Translation(Vec3(0.0f, 0.0f, -2.0f));
        const Mat4 projection = PerspectiveRH_ZO(std::numbers::pi_v<float> / 2.0f, 1.0f, 0.1f, 100.0f);

        const Mat4 viewLeft = ViewMatrixFromPoseRH(Vec3(-0.03f, 0.0f, 0.0f), Quaternion::Identity());
        const Mat4 viewRight = ViewMatrixFromPoseRH(Vec3(0.03f, 0.0f, 0.0f), Quaternion::Identity());

        const Mat4 mvpLeft = projection * viewLeft * model;
        const Mat4 mvpRight = projection * viewRight * model;

        Check(!(mvpLeft == mvpRight), "Two different eye poses produce two different MVPs for the same model transform");
    }

    void TestModelViewProjectionComposition()
    {
        // The exact fixed camera M8F's demo uses: eye at (0,0,3),
        // looking at the origin. Composing Model (identity - the point
        // is already at the world origin) * View * Projection and
        // transforming the world origin should land it centered on
        // screen (NDC x=y=0) with a valid depth strictly between the
        // near and far planes. Uses Core's PerspectiveRH_ZO directly
        // (no Vulkan Y-flip) - that flip is a Vulkan-layer concern, not
        // part of what this Core-level composition test is proving.
        const Mat4 model = Mat4::Identity();
        const Mat4 view = LookAtRH(Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, 0.0f), kWorldUp);
        const Mat4 proj = PerspectiveRH_ZO(std::numbers::pi_v<float> / 2.0f, 1.0f, 1.0f, 10.0f);
        const Mat4 mvp = proj * view * model;

        const Vec4 clip = mvp * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        Check(clip.w > 0.0f, "MVP composition: a point in front of the camera has positive clip w");
        CheckNearlyEqual(clip.x / clip.w, 0.0f, "MVP composition: world origin projects to NDC x=0");
        CheckNearlyEqual(clip.y / clip.w, 0.0f, "MVP composition: world origin projects to NDC y=0");
        const float ndcDepth = clip.z / clip.w;
        Check(ndcDepth > 0.0f && ndcDepth < 1.0f, "MVP composition: NDC depth lies within [0,1] between near and far");
    }

    void TestEvent()
    {
        struct DummyEvent : AREngine::Core::Event {};

        DummyEvent event;
        Check(event.handled == false, "Event defaults to not handled");
        event.handled = true;
        Check(event.handled == true, "Event.handled can be set");
    }
}

int main()
{
    TestVec3();
    TestMat4();
    TestQuaternion();
    TestQuaternionFromAxisAngle();
    TestQuaternionMultiplication();
    TestQuaternionRotate();
    TestQuaternionConjugate();
    TestMat4TransformFactories();
    TestLookAtRH();
    TestPerspectiveRH_ZO();
    TestPerspectiveOffCenterRH_ZOSymmetricCaseMatchesPerspectiveRH_ZO();
    TestPerspectiveOffCenterRH_ZOAsymmetricFrustumBoundaries();
    TestPerspectiveOffCenterRH_ZODepthMapping();
    TestViewMatrixFromPoseRH();
    TestViewMatrixFromPoseRHRotated();
    TestMvpMultiplicationOrderMatters();
    TestTwoEyePosesProduceDifferentMvps();
    TestModelViewProjectionComposition();
    TestEvent();

    if (g_failureCount == 0)
    {
        AR_LOG_INFO("All Core checks passed");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
