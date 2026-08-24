// M3 automated tests for AREngine::Runtime: DesktopFrameDriver timing
// and lifecycle behavior, with no human interaction. This briefly opens
// a real OS window (same as platform_tests.cpp), since
// DesktopFrameDriver needs a Platform::Window to construct, but never
// waits on it to close.
//
// M9E.5 updates this to the redesigned PrepareFrame/BeginFrame/
// GetViews/EndFrame interface and adds FrameStatus/shouldRender
// coverage - see docs/ARCHITECTURE.md, "Desktop Mapping (M9E.5)".

#include "AREngine/Platform/Platform.hpp"
#include "AREngine/Runtime/DesktopFrameDriver.hpp"

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

    void TestDesktopFrameDriver()
    {
        AREngine::Platform::WindowDesc desc;
        desc.title = "AREngine Runtime Test";
        desc.width = 640;
        desc.height = 480;

        auto window = AREngine::Platform::CreateAppWindow(desc);
        AREngine::Runtime::DesktopFrameDriver frameDriver(*window);

        double previousTotal = -1.0;
        bool allDeltasNonNegative = true;
        bool totalTimeMonotonic = true;
        bool alwaysOneView = true;
        bool alwaysContinue = true;
        bool alwaysShouldRender = true;

        constexpr int kFrameCount = 5;
        for (int i = 0; i < kFrameCount; ++i)
        {
            const AREngine::Frame::FrameContext context = frameDriver.PrepareFrame();
            const AREngine::Frame::FrameTiming& timing = context.timing;

            if (context.status != AREngine::Frame::FrameStatus::Continue)
            {
                alwaysContinue = false;
            }
            if (!timing.shouldRender)
            {
                alwaysShouldRender = false;
            }
            if (timing.deltaTimeSeconds < 0.0)
            {
                allDeltasNonNegative = false;
            }
            if (timing.totalTimeSeconds < previousTotal)
            {
                totalTimeMonotonic = false;
            }
            previousTotal = timing.totalTimeSeconds;

            frameDriver.BeginFrame(); // must not crash with nothing to begin

            const auto views = frameDriver.GetViews();
            if (views.size() != 1)
            {
                alwaysOneView = false;
            }

            frameDriver.EndFrame(); // must not crash with nothing to submit to
        }

        Check(allDeltasNonNegative, "DesktopFrameDriver delta time is always non-negative");
        Check(totalTimeMonotonic, "DesktopFrameDriver total time never goes backwards");
        Check(alwaysOneView, "DesktopFrameDriver always produces exactly one desktop view");
        Check(alwaysContinue, "DesktopFrameDriver's FrameStatus is always Continue - it never returns Idle or Stop");
        Check(alwaysShouldRender, "DesktopFrameDriver's shouldRender is always true");
        Check(previousTotal >= 0.0, "DesktopFrameDriver ran for the expected number of frames");
    }
}

int main()
{
    TestDesktopFrameDriver();

    if (g_failureCount == 0)
    {
        std::printf("All Runtime M3 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
