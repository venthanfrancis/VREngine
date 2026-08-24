// M1 tests for AREngine::Frame: FrameTiming, ViewInfo, and the
// FrameDriver interface (exercised via a minimal dummy implementation —
// not a real desktop or XR driver; those come in later milestones).
//
// M9E.5 adds FrameStatus/FrameContext coverage and updates
// DummyFrameDriver to the redesigned PrepareFrame/BeginFrame/GetViews/
// EndFrame interface - see docs/ARCHITECTURE.md, "New Lifecycle API
// (M9E.5)".

#include "AREngine/Frame/Frame.hpp"

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

    class DummyFrameDriver : public AREngine::Frame::FrameDriver
    {
    public:
        AREngine::Frame::FrameContext PrepareFrame() override
        {
            AREngine::Frame::FrameTiming timing;
            timing.deltaTimeSeconds = 0.016;
            return AREngine::Frame::FrameContext{timing, AREngine::Frame::FrameStatus::Continue};
        }

        void BeginFrame() override
        {
            ++beginCount;
        }

        std::vector<AREngine::Frame::ViewInfo> GetViews() override
        {
            return { AREngine::Frame::ViewInfo{} };
        }

        void EndFrame() override
        {
            ++endCount;
        }

        int beginCount = 0;
        int endCount = 0;
    };

    void TestFrameTiming()
    {
        const AREngine::Frame::FrameTiming timing;
        Check(timing.deltaTimeSeconds == 0.0, "FrameTiming defaults deltaTimeSeconds to 0");
        Check(timing.totalTimeSeconds == 0.0, "FrameTiming defaults totalTimeSeconds to 0");
        Check(timing.predictedDisplayTimeSeconds == 0.0, "FrameTiming defaults predictedDisplayTimeSeconds to 0");
        Check(timing.shouldRender == true, "FrameTiming defaults shouldRender to true");
    }

    void TestFrameStatus()
    {
        using AREngine::Frame::FrameStatus;
        Check(FrameStatus::Continue != FrameStatus::Idle, "Continue and Idle are distinct");
        Check(FrameStatus::Continue != FrameStatus::Stop, "Continue and Stop are distinct");
        Check(FrameStatus::Idle != FrameStatus::Stop, "Idle and Stop are distinct");
    }

    void TestFrameContext()
    {
        const AREngine::Frame::FrameContext context;
        Check(context.status == AREngine::Frame::FrameStatus::Continue, "FrameContext defaults status to Continue");
        Check(context.timing.shouldRender == true, "FrameContext's default timing defaults shouldRender to true");
    }

    void TestViewInfo()
    {
        using namespace AREngine::Core::Math;

        const AREngine::Frame::ViewInfo view;
        Check(view.orientation == Quaternion::Identity(), "ViewInfo defaults to identity orientation");
        Check(view.projection == Mat4::Identity(), "ViewInfo defaults to identity projection");
    }

    void TestFrameDriver()
    {
        DummyFrameDriver driver;

        const auto context = driver.PrepareFrame();
        Check(context.timing.deltaTimeSeconds == 0.016, "FrameDriver::PrepareFrame returns timing");
        Check(context.status == AREngine::Frame::FrameStatus::Continue, "FrameDriver::PrepareFrame returns a status");

        driver.BeginFrame();
        Check(driver.beginCount == 1, "FrameDriver::BeginFrame can be called");

        const auto views = driver.GetViews();
        Check(views.size() == 1, "FrameDriver::GetViews can return a view");

        driver.EndFrame();
        Check(driver.endCount == 1, "FrameDriver::EndFrame can be called");
    }
}

int main()
{
    TestFrameTiming();
    TestFrameStatus();
    TestFrameContext();
    TestViewInfo();
    TestFrameDriver();

    if (g_failureCount == 0)
    {
        std::printf("All Frame M1 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
