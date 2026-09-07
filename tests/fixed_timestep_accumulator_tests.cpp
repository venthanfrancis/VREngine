// M18 pure-logic tests for AREngine::Physics::FixedTimestepAccumulator
// (engine/physics/include/AREngine/Physics/FixedTimestepAccumulator.hpp).
// Built unconditionally, not gated behind ARENGINE_ENABLE_PHYSICS: this
// class has zero Jolt/physics-backend dependency - it's pure timing
// arithmetic. No human interaction, fully headless. See
// docs/ARCHITECTURE.md, "M18 - Physics Foundation".

#include "AREngine/Physics/FixedTimestepAccumulator.hpp"

#include <cstdio>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    using AREngine::Physics::FixedTimestepAccumulator;
    constexpr float kFixedDelta = 1.0f / 60.0f;

    void TestBelowFixedStepProducesZeroSteps()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        Check(accumulator.Advance(kFixedDelta * 0.5f) == 0, "A delta below one fixed step produces zero steps");
    }

    void TestExactlyFixedStepProducesOneStepAndResets()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        Check(accumulator.Advance(kFixedDelta) == 1, "A delta of exactly one fixed step produces exactly one step");
        Check(accumulator.Advance(0.0f) == 0, "After consuming exactly one fixed step, the accumulator is back to zero (no leftover step)");
    }

    void Test2point5xFixedStepProducesTwoStepsPlusRemainder()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        Check(accumulator.Advance(kFixedDelta * 2.5f) == 2, "2.5x fixedStep produces exactly 2 steps");
        // The remaining 0.5x should still be there next call - not discarded, since we're under the cap.
        Check(accumulator.Advance(kFixedDelta * 0.5f) == 1, "The 0.5x remainder plus another 0.5x crosses one more full fixed step");
    }

    void TestMultipleRenderFramesAccumulateCorrectly()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        int totalSteps = 0;
        // Ten frames of 0.3x fixedStep each = 3.0x total -> exactly 3 steps overall.
        for (int i = 0; i < 10; ++i)
        {
            totalSteps += accumulator.Advance(kFixedDelta * 0.3f);
        }
        Check(totalSteps == 3, "Several small render-frame deltas accumulate correctly across many Advance() calls");
    }

    void TestMaxSubstepCapWorks()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta, /*maxStepsPerFrame=*/4);
        Check(accumulator.Advance(kFixedDelta * 10.0f) == 4, "A huge delta is capped at maxStepsPerFrame steps in one call");
    }

    void TestExactlyAtCapBoundaryLeavesZeroRemainder()
    {
        // A delta of EXACTLY maxSteps * fixedStep should consume all of
        // it via the natural loop termination - not the discard branch -
        // and leave the accumulator at exactly 0.
        FixedTimestepAccumulator accumulator(kFixedDelta, /*maxStepsPerFrame=*/4);
        Check(accumulator.Advance(kFixedDelta * 4.0f) == 4, "A delta of exactly maxSteps*fixedStep produces exactly maxSteps steps");
        Check(accumulator.Advance(0.0f) == 0, "Exactly-at-the-cap leaves zero remainder - no extra step next call");
    }

    void TestCapPlusRemainderIsDiscarded()
    {
        // A delta of maxSteps*fixedStep PLUS an extra half-step should
        // still only produce maxSteps steps, and that extra half-step
        // must be discarded (not retained for the next call) - this is
        // the actual spiral-of-death protection.
        FixedTimestepAccumulator accumulator(kFixedDelta, /*maxStepsPerFrame=*/4);
        Check(accumulator.Advance(kFixedDelta * 4.5f) == 4, "A delta of maxSteps*fixedStep + 0.5x still produces only maxSteps steps");
        Check(accumulator.Advance(0.0f) == 0, "The 0.5x excess beyond the cap was discarded, not retained for the next call");
        Check(accumulator.Advance(kFixedDelta * 0.5f) == 0,
              "Discarded half-step cannot combine with the next half-step to advance physics");
    }

    void TestZeroDeltaIsSafe()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        Check(accumulator.Advance(0.0f) == 0, "A zero delta is safe and produces zero steps");
    }

    void TestNegativeDeltaIsClampedNotSubtracted()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        Check(accumulator.Advance(kFixedDelta * 0.9f) == 0, "Priming the accumulator below one step");
        Check(accumulator.Advance(-1000.0f) == 0, "A large negative delta is clamped to 0, never subtracts from the accumulator");
        Check(accumulator.Advance(kFixedDelta * 0.1f) == 1, "The primed 0.9x is still intact after the negative delta was clamped, not discarded - 0.9x + 0.1x crosses one step");
    }

    void TestHugeDeltaDoesNotSpiralAcrossManyFrames()
    {
        // Simulate many consecutive frames each reporting a huge delta
        // (e.g. a debugger breakpoint every frame) - the accumulator
        // must never let the backlog grow unbounded; every call should
        // still return at most maxStepsPerFrame.
        FixedTimestepAccumulator accumulator(kFixedDelta, /*maxStepsPerFrame=*/4);
        bool everExceededCap = false;
        for (int i = 0; i < 100; ++i)
        {
            if (accumulator.Advance(10.0f) > 4)
            {
                everExceededCap = true;
            }
        }
        Check(!everExceededCap, "A sustained huge delta across many frames never exceeds maxStepsPerFrame in any single call");
    }

    void TestFixedDeltaSecondsAccessor()
    {
        FixedTimestepAccumulator accumulator(kFixedDelta);
        Check(accumulator.FixedDeltaSeconds() == kFixedDelta, "FixedDeltaSeconds() returns the value passed to the constructor");
    }
}

int main()
{
    TestBelowFixedStepProducesZeroSteps();
    TestExactlyFixedStepProducesOneStepAndResets();
    Test2point5xFixedStepProducesTwoStepsPlusRemainder();
    TestMultipleRenderFramesAccumulateCorrectly();
    TestMaxSubstepCapWorks();
    TestExactlyAtCapBoundaryLeavesZeroRemainder();
    TestCapPlusRemainderIsDiscarded();
    TestZeroDeltaIsSafe();
    TestNegativeDeltaIsClampedNotSubtracted();
    TestHugeDeltaDoesNotSpiralAcrossManyFrames();
    TestFixedDeltaSecondsAccessor();

    if (g_failureCount == 0)
    {
        std::printf("All M18 FixedTimestepAccumulator checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
