// M2 automated tests for AREngine::Platform: SteadyClock, and a basic
// Window create+destroy lifecycle. No human interaction required — this
// does NOT test closing via the close button (that needs a human, and
// is covered instead by the manual platform_window_demo, which is
// deliberately not registered as a CTest test).
//
// Note: creating a Window here does briefly show a real OS window while
// this test runs, since it goes through the same CreateAppWindow path
// as the real engine — there is no separate "headless" window mode.

#include "AREngine/Platform/Platform.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

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

    void TestSteadyClock()
    {
        AREngine::Platform::SteadyClock clock;

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const double elapsed = clock.ElapsedSeconds();
        Check(elapsed > 0.0, "SteadyClock::ElapsedSeconds advances over time");

        const double delta = clock.Tick();
        Check(delta > 0.0, "SteadyClock::Tick returns a positive delta");

        clock.Reset();
        Check(clock.ElapsedSeconds() >= 0.0, "SteadyClock::Reset leaves a valid clock");
    }

    void TestWindowLifecycle()
    {
        AREngine::Platform::WindowDesc desc;
        desc.title = "AREngine Platform Test";
        desc.width = 640;
        desc.height = 480;

        auto window = AREngine::Platform::CreateAppWindow(desc);
        Check(window != nullptr, "CreateAppWindow returns a Window");
        Check(window->GetWidth() == 640, "Window reports the requested width");
        Check(window->GetHeight() == 480, "Window reports the requested height");
        Check(window->ShouldClose() == false, "A freshly created window does not request close");

        // Destroying `window` here (end of scope) exercises the real
        // Win32 teardown path with no human interaction required.
    }
}

int main()
{
    TestSteadyClock();
    TestWindowLifecycle();

    if (g_failureCount == 0)
    {
        std::printf("All Platform M2 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
