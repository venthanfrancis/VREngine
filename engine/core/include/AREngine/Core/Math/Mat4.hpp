#pragma once

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
    // Deliberately minimal for M1: identity, construction, and
    // multiplication only. Projection/view/camera helpers are added
    // once Rendering actually needs them.
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
}
