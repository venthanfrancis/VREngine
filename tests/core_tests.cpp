// M1 tests for AREngine::Core: math (Vec3, Mat4, Quaternion) and the
// Event base type. No external test framework — CTest just checks this
// executable's exit code (0 = all checks passed).

#include "AREngine/Core/Core.hpp"

#include <cstdio>

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
    TestEvent();

    if (g_failureCount == 0)
    {
        AR_LOG_INFO("All Core M1 checks passed");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
