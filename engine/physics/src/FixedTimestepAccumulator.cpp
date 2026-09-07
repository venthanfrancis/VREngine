#include "AREngine/Physics/FixedTimestepAccumulator.hpp"
#include "AREngine/Core/Assert.hpp"
#include <algorithm>
#include <cmath>

namespace AREngine::Physics
{
    namespace
    {
        // A step boundary reached by summing several float deltas (e.g.
        // two 0.5x-fixedDelta calls) can land a handful of ULPs below
        // the true boundary due to ordinary float rounding, not any
        // real timing shortfall. Comparing with a small tolerance
        // instead of `>=` directly avoids losing a step to that noise.
        // Chosen well below any real fixed-step duration this engine
        // uses (milliseconds-scale) and well above the rounding error
        // float accumulation of this magnitude actually produces.
        constexpr float kStepEpsilon = 1e-5f;
    }

    FixedTimestepAccumulator::FixedTimestepAccumulator(float fixedDeltaSeconds, int maxStepsPerFrame)
        : m_fixedDelta(fixedDeltaSeconds)
        , m_maxStepsPerFrame(maxStepsPerFrame)
    {
        AR_ASSERT_MSG(std::isfinite(fixedDeltaSeconds) && fixedDeltaSeconds > kStepEpsilon,
            "Fixed timestep must be finite and greater than the rounding tolerance");
        AR_ASSERT_MSG(maxStepsPerFrame > 0, "Step cap must be positive");
    }

    int FixedTimestepAccumulator::Advance(float frameDeltaSeconds)
    {
        if (std::isfinite(frameDeltaSeconds) && frameDeltaSeconds > 0.0f)
        {
            m_accumulator += frameDeltaSeconds;
        }

        const float stepThreshold = m_fixedDelta - kStepEpsilon;

        int steps = 0;
        while (m_accumulator >= stepThreshold && steps < m_maxStepsPerFrame)
        {
            m_accumulator -= m_fixedDelta;
            ++steps;
        }

        // Discard ALL remainder at the cap, including fractional steps.
        if (steps == m_maxStepsPerFrame)
        {
            m_accumulator = 0.0f;
        }

        m_accumulator = std::max(0.0f, m_accumulator);
        return steps;
    }
}
