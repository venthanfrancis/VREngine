#pragma once

#include "AREngine/Core/Math/Vec3.hpp"

#include <cmath>

namespace AREngine::Core::Math
{
    // Represents a rotation. Kept minimal: construction, identity, and
    // (as of M5) axis-angle construction, which is genuinely needed to
    // build meaningful, testable rotations for Scene's transform tests.
    // Rotation composition (quaternion * quaternion, below) and applying
    // a quaternion directly to a Vec3 (Rotate, below) were added in M8G,
    // once a free-fly camera controller genuinely needed both (yaw and
    // pitch composed into one orientation, and that orientation's
    // forward/right vectors read back out — see
    // docs/ARCHITECTURE.md, "Rotation Representation (M8G)").
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

    // Quaternion composition (the Hamilton product): `a * b` means "b
    // applied first, then a" — matching Mat4's existing composition
    // convention (TRS = T * R * S, applied to a point p as
    // T * (R * (S * p))), so the two rotation representations agree on
    // what "compose" means. Used by M8G's camera controller to build one
    // orientation from separate yaw (world Up) and pitch (local Right)
    // rotations: `yawQuat * pitchQuat` applies pitch first (in the
    // not-yet-yawed local frame), then yaw — the standard FPS-camera
    // composition order.
    [[nodiscard]] constexpr Quaternion operator*(const Quaternion& a, const Quaternion& b)
    {
        return Quaternion(
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
        );
    }

    // The inverse rotation, for a unit quaternion (every Quaternion
    // this engine constructs is unit-length - see FromAxisAngle). For a
    // unit quaternion, the conjugate (negate the vector part, keep w)
    // equals the inverse: q * Conjugate(q) == Identity. Added in M9G
    // specifically for deriving a view matrix (viewFromWorld) from a
    // pose's orientation (worldFromView) — a closed-form rigid-
    // transform inverse, not a general quaternion-division operation.
    // See docs/ARCHITECTURE.md, "worldFromView -> viewFromWorld
    // Derivation (M9G)".
    [[nodiscard]] constexpr Quaternion Conjugate(const Quaternion& q)
    {
        return Quaternion(q.w, -q.x, -q.y, -q.z);
    }

    // Rotates `v` by `q`. Standard optimized form of q * (0,v) * q^-1
    // that avoids constructing a full pure-quaternion intermediate.
    // Added in M8G specifically so Transform can derive its
    // forward/right/up directions straight from `rotation`, without
    // needing to go through Mat4::Rotation first — see
    // docs/ARCHITECTURE.md, "Rotation Representation (M8G)".
    [[nodiscard]] constexpr Vec3 Rotate(const Quaternion& q, const Vec3& v)
    {
        const Vec3 qv(q.x, q.y, q.z);
        const Vec3 t = Cross(qv, v) * 2.0f;
        return v + t * q.w + Cross(qv, t);
    }
}
