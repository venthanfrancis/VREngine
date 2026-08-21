#pragma once

#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

#include <array>
#include <cstddef>

namespace AREngine::Core::Math
{
    // A 4x4 matrix, stored column-major as 16 floats. This is an
    // AREngine convention, not a Vulkan requirement — Vulkan doesn't
    // care how C++ stores a matrix, only how shader data is laid out.
    // This convention must stay consistent between engine math code,
    // shader data layouts, transformation code, and future rendering
    // backends. element(row, col) lives at m[col * 4 + row].
    //
    // Kept minimal: identity, construction, multiplication (M1), plus
    // (as of M5) the Translation/Rotation/Scale/TRS factories Scene's
    // transforms genuinely need. Projection/view/camera helpers are
    // still deferred until Rendering actually needs them.
    struct Mat4
    {
        std::array<float, 16> m{};

        constexpr Mat4() = default;

        [[nodiscard]] constexpr float At(std::size_t row, std::size_t col) const
        {
            return m[col * 4 + row];
        }

        constexpr void Set(std::size_t row, std::size_t col, float value)
        {
            m[col * 4 + row] = value;
        }

        [[nodiscard]] static constexpr Mat4 Identity()
        {
            Mat4 result;
            result.Set(0, 0, 1.0f);
            result.Set(1, 1, 1.0f);
            result.Set(2, 2, 1.0f);
            result.Set(3, 3, 1.0f);
            return result;
        }

        [[nodiscard]] static constexpr Mat4 Translation(const Vec3& t)
        {
            Mat4 result = Identity();
            result.Set(0, 3, t.x);
            result.Set(1, 3, t.y);
            result.Set(2, 3, t.z);
            return result;
        }

        [[nodiscard]] static constexpr Mat4 Scale(const Vec3& s)
        {
            Mat4 result;
            result.Set(0, 0, s.x);
            result.Set(1, 1, s.y);
            result.Set(2, 2, s.z);
            result.Set(3, 3, 1.0f);
            return result;
        }

        // Standard unit-quaternion-to-rotation-matrix conversion,
        // assuming `q` is normalized (see Quaternion::FromAxisAngle).
        // Written so that transforming a column-vector point p as
        // Rotation(q) * p rotates p the same way q does.
        [[nodiscard]] static constexpr Mat4 Rotation(const Quaternion& q)
        {
            Mat4 result = Identity();

            const float xx = q.x * q.x;
            const float yy = q.y * q.y;
            const float zz = q.z * q.z;
            const float xy = q.x * q.y;
            const float xz = q.x * q.z;
            const float yz = q.y * q.z;
            const float wx = q.w * q.x;
            const float wy = q.w * q.y;
            const float wz = q.w * q.z;

            result.Set(0, 0, 1.0f - 2.0f * (yy + zz));
            result.Set(0, 1, 2.0f * (xy - wz));
            result.Set(0, 2, 2.0f * (xz + wy));

            result.Set(1, 0, 2.0f * (xy + wz));
            result.Set(1, 1, 1.0f - 2.0f * (xx + zz));
            result.Set(1, 2, 2.0f * (yz - wx));

            result.Set(2, 0, 2.0f * (xz - wy));
            result.Set(2, 1, 2.0f * (yz + wx));
            result.Set(2, 2, 1.0f - 2.0f * (xx + yy));

            return result;
        }

        // AREngine's transform composition order: TRS = Translation *
        // Rotation * Scale. Applied to a column-vector point p (as
        // TRS * p), this scales p first, then rotates it, then
        // translates it — see docs/ARCHITECTURE.md, "TRS Composition
        // Order".
        //
        // Declared here but defined below, after operator* — its body
        // needs Mat4 * Mat4, which (being a free function outside this
        // class) must already be declared at the point of use, unlike a
        // member declared later in the same class.
        [[nodiscard]] static constexpr Mat4 TRS(const Vec3& position, const Quaternion& rotation, const Vec3& scale);
    };

    [[nodiscard]] constexpr Mat4 operator*(const Mat4& a, const Mat4& b)
    {
        Mat4 result;
        for (std::size_t col = 0; col < 4; ++col)
        {
            for (std::size_t row = 0; row < 4; ++row)
            {
                float sum = 0.0f;
                for (std::size_t k = 0; k < 4; ++k)
                {
                    sum += a.At(row, k) * b.At(k, col);
                }
                result.Set(row, col, sum);
            }
        }
        return result;
    }

    constexpr bool operator==(const Mat4& a, const Mat4& b)
    {
        for (std::size_t i = 0; i < 16; ++i)
        {
            if (a.m[i] != b.m[i])
            {
                return false;
            }
        }
        return true;
    }

    constexpr Mat4 Mat4::TRS(const Vec3& position, const Quaternion& rotation, const Vec3& scale)
    {
        return Translation(position) * Rotation(rotation) * Scale(scale);
    }

    // Treats `point` as a homogeneous point (w = 1) and returns the
    // transformed x/y/z, dropping w. Sufficient for M5's needs (reading
    // "where did this point end up" out of a world matrix in tests); a
    // general Vec4 transform is added if/when something needs one.
    [[nodiscard]] constexpr Vec3 TransformPoint(const Mat4& m, const Vec3& point)
    {
        return Vec3(
            m.At(0, 0) * point.x + m.At(0, 1) * point.y + m.At(0, 2) * point.z + m.At(0, 3),
            m.At(1, 0) * point.x + m.At(1, 1) * point.y + m.At(1, 2) * point.z + m.At(1, 3),
            m.At(2, 0) * point.x + m.At(2, 1) * point.y + m.At(2, 2) * point.z + m.At(2, 3)
        );
    }
}
