#include "AREngine/Platform/Clock.hpp"

namespace AREngine::Platform
{
    namespace
    {
        double SecondsBetween(std::chrono::steady_clock::time_point start,
                               std::chrono::steady_clock::time_point end)
        {
            return std::chrono::duration<double>(end - start).count();
        }
    }

    SteadyClock::SteadyClock()
        : m_start(std::chrono::steady_clock::now())
        , m_lastTick(m_start)
    {
    }

    double SteadyClock::ElapsedSeconds() const
    {
        return SecondsBetween(m_start, std::chrono::steady_clock::now());
    }

    double SteadyClock::Tick()
    {
        const auto now = std::chrono::steady_clock::now();
        const double delta = SecondsBetween(m_lastTick, now);
        m_lastTick = now;
        return delta;
    }

    void SteadyClock::Reset()
    {
        m_start = std::chrono::steady_clock::now();
        m_lastTick = m_start;
    }
}
