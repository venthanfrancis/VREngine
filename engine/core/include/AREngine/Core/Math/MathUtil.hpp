#pragma once

namespace AREngine::Core::Math
{
    // Default tolerance for floating-point comparisons. Not physically
    // meaningful — just "close enough" for single-precision arithmetic.
    inline constexpr float kEpsilon = 1e-5f;

    [[nodiscard]] constexpr bool NearlyEqual(float a, float b, float epsilon = kEpsilon)
    {
        const float diff = a - b;
        return (diff < 0.0f ? -diff : diff) <= epsilon;
    }
}
