#pragma once

namespace AREngine::Core::Math
{
    // A 4D vector, primarily used as the homogeneous-coordinate building
    // block for Mat4. Minimal on purpose.
    struct Vec4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        constexpr Vec4() = default;
        constexpr Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    };

    constexpr Vec4 operator+(const Vec4& a, const Vec4& b)
    {
        return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
    }

    constexpr Vec4 operator-(const Vec4& a, const Vec4& b)
    {
        return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
    }

    constexpr Vec4 operator*(const Vec4& v, float scalar)
    {
        return Vec4(v.x * scalar, v.y * scalar, v.z * scalar, v.w * scalar);
    }

    constexpr Vec4 operator/(const Vec4& v, float scalar)
    {
        return Vec4(v.x / scalar, v.y / scalar, v.z / scalar, v.w / scalar);
    }

    constexpr bool operator==(const Vec4& a, const Vec4& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }
}
