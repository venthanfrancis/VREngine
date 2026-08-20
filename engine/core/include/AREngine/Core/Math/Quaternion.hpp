#pragma once

namespace AREngine::Core::Math
{
    // Represents a rotation. Deliberately minimal for M1: construction
    // and identity only. Rotation composition, axis-angle construction,
    // and applying a quaternion to a Vec3 are added later, once
    // something in the engine actually needs them.
    //
    // Stored in Hamilton (w, x, y, z) form; identity = (1, 0, 0, 0).
    struct Quaternion
    {
        float w = 1.0f;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Quaternion() = default;
        constexpr Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

        [[nodiscard]] static constexpr Quaternion Identity()
        {
            return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
        }
    };

    constexpr bool operator==(const Quaternion& a, const Quaternion& b)
    {
        return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
    }
}
