#pragma once

#include "AREngine/Core/Assert.hpp"

#include <cmath>

namespace AREngine::Core::Math
{
    // A 3D vector: a position, direction, or displacement, depending on
    // context. Follows the engine's world convention (see
    // docs/WORLD_CONVENTIONS.md): 1 unit = 1 meter, right-handed,
    // +X = right, +Y = up, -Z = forward.
    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vec3() = default;
        constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    };

    constexpr Vec3 operator+(const Vec3& a, const Vec3& b)
    {
        return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    constexpr Vec3 operator-(const Vec3& a, const Vec3& b)
    {
        return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    constexpr Vec3 operator-(const Vec3& v)
    {
        return Vec3(-v.x, -v.y, -v.z);
    }

    constexpr Vec3 operator*(const Vec3& v, float scalar)
    {
        return Vec3(v.x * scalar, v.y * scalar, v.z * scalar);
    }

    constexpr Vec3 operator*(float scalar, const Vec3& v)
    {
        return v * scalar;
    }

    constexpr Vec3 operator/(const Vec3& v, float scalar)
    {
        return Vec3(v.x / scalar, v.y / scalar, v.z / scalar);
    }

    constexpr bool operator==(const Vec3& a, const Vec3& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    [[nodiscard]] constexpr float Dot(const Vec3& a, const Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // Right-handed cross product. Note that because this engine defines
    // Forward as -Z (not +Z), Cross(Right, Up) works out to -Forward,
    // not Forward. See docs/WORLD_CONVENTIONS.md and the accompanying
    // test for the worked-out reasoning — don't assume the "obvious"
    // direction without checking.
    [[nodiscard]] constexpr Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    [[nodiscard]] inline float Length(const Vec3& v)
    {
        return std::sqrt(Dot(v, v));
    }

    // Returns v scaled to length 1.
    [[nodiscard]] inline Vec3 Normalize(const Vec3& v)
    {
        const float len = Length(v);
        AR_ASSERT_MSG(len > 0.0f, "Cannot normalize a zero-length vector");
        return v / len;
    }

    // World convention basis vectors (see docs/WORLD_CONVENTIONS.md).
    inline constexpr Vec3 kWorldRight(1.0f, 0.0f, 0.0f);
    inline constexpr Vec3 kWorldUp(0.0f, 1.0f, 0.0f);
    inline constexpr Vec3 kWorldForward(0.0f, 0.0f, -1.0f);
}
