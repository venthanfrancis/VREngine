#pragma once

namespace AREngine::Core::Math
{
    // A 2D vector. Minimal on purpose — nothing in the engine needs more
    // than construction and basic arithmetic from Vec2 yet.
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vec2() = default;
        constexpr Vec2(float x, float y) : x(x), y(y) {}
    };

    constexpr Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2(a.x + b.x, a.y + b.y); }
    constexpr Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2(a.x - b.x, a.y - b.y); }
    constexpr Vec2 operator*(const Vec2& v, float scalar) { return Vec2(v.x * scalar, v.y * scalar); }
    constexpr Vec2 operator/(const Vec2& v, float scalar) { return Vec2(v.x / scalar, v.y / scalar); }

    constexpr bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }
}
