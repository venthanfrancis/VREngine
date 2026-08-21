#pragma once

#include "AREngine/Core/Math/Vec3.hpp"

#include <cmath>

namespace AREngine::Core::Math
{
    // Represents a rotation. Kept minimal: construction, identity, and
    // (as of M5) axis-angle construction, which is genuinely needed to
    // build meaningful, testable rotations for Scene's transform tests.
    // Rotation composition (quaternion * quaternion) and applying a
    // quaternion directly to a Vec3 are still not implemented — M5's
    // transform composition works entirely through Mat4 (see
    // docs/ARCHITECTURE.md, "M5 Implementation Notes"), so neither was
    // genuinely required. Added later if something actually needs them.
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

        // Builds a rotation of `angleRadians` around `axis` (which need
        // not already be normalized). Not constexpr, since it needs
        // std::sin/std::cos. The result is always unit-length by
        // construction (given a normalized axis and the cos/sin(half
        // angle) identity), so there is no separate Quaternion
        // normalization function yet — nothing else constructs a
        // non-unit Quaternion for it to fix.
        [[nodiscard]] static Quaternion FromAxisAngle(const Vec3& axis, float angleRadians)
        {
            const Vec3 normalizedAxis = Normalize(axis);
            const float halfAngle = angleRadians * 0.5f;
            const float s = std::sin(halfAngle);
            return Quaternion(std::cos(halfAngle),
                               normalizedAxis.x * s,
                               normalizedAxis.y * s,
                               normalizedAxis.z * s);
        }
    };

    constexpr bool operator==(const Quaternion& a, const Quaternion& b)
    {
        return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
    }
}
