#pragma once

namespace AREngine::Physics
{
    // Converts a variable per-frame render delta into a whole number of
    // fixed-size physics steps, per the standard "Fix Your Timestep"
    // accumulator pattern. Physics must never step directly using
    // arbitrary render delta time (framerate-dependent simulation is
    // not stable) - see docs/ARCHITECTURE.md, "M18 - Physics
    // Foundation".
    //
    // Deliberately Jolt-free and Core-only: this is pure timing
    // arithmetic with zero physics-backend dependency, kept unconditional
    // (builds and is pure-tested even with ARENGINE_ENABLE_PHYSICS=OFF)
    // specifically so it's testable without a GPU or a physics backend -
    // the same reasoning M16's HandleAllocator extraction already
    // established for this codebase.
    //
    // Placed in engine/physics (not engine/frame or engine/core): no
    // other module needs fixed-timestep logic today, and touching either
    // of those stable, already-shipped modules for a niche utility only
    // Physics currently needs isn't warranted.
    class FixedTimestepAccumulator
    {
    public:
        // `maxStepsPerFrame` caps how many fixed steps one call to
        // Advance() can ever return - protection against a "spiral of
        // death" after a huge frame stall (e.g. a breakpoint, a stutter,
        // an OS scheduling hiccup). Any accumulated time beyond what
        // `maxStepsPerFrame` steps can consume is DISCARDED (the
        // accumulator is reset to 0), never carried forward to a later
        // frame - carrying it forward would only delay the spiral, not
        // prevent it.
        explicit FixedTimestepAccumulator(float fixedDeltaSeconds, int maxStepsPerFrame = 8);

        // Adds `frameDeltaSeconds` to the internal accumulator (a
        // negative value is clamped to 0 first - it never subtracts),
        // then returns how many fixed steps the caller should run this
        // call, each consuming FixedDeltaSeconds() of the accumulator.
        // Returns 0 if less than one fixed step's worth of time has
        // accumulated yet.
        [[nodiscard]] int Advance(float frameDeltaSeconds);

        [[nodiscard]] float FixedDeltaSeconds() const { return m_fixedDelta; }

    private:
        float m_fixedDelta;
        int m_maxStepsPerFrame;
        float m_accumulator = 0.0f;
    };
}
