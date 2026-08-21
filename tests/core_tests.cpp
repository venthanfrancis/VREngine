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
    TestMat4TransformFactories();
    TestEvent();

    if (g_failureCount == 0)
    {
        AR_LOG_INFO("All Core checks passed");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
