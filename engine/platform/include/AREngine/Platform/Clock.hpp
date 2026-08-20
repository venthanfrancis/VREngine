#pragma once

#include <chrono>

namespace AREngine::Platform
{
    // A monotonic, high-resolution clock for frame delta timing, engine
    // uptime, and future profiling. Wraps std::chrono::steady_clock —
    // the standard library already does exactly what's needed here, so
    // there's no reason to reach for a Win32 timing API just because
    // this happens to be the Windows module. Public API exposes plain
    // seconds as double, not a platform-specific time representation.
    class SteadyClock
    {
    public:
        SteadyClock();

        // Seconds elapsed since this clock was constructed (or last
        // Reset()).
        [[nodiscard]] double ElapsedSeconds() const;

        // Seconds elapsed since the previous call to Tick() (since
        // construction, for the first call). Also advances the
        // "previous" timestamp used for the next call.
        [[nodiscard]] double Tick();

        void Reset();

    private:
        std::chrono::steady_clock::time_point m_start;
        std::chrono::steady_clock::time_point m_lastTick;
    };
}
