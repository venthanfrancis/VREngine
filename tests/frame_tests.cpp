// M1 tests for AREngine::Frame: FrameTiming, ViewInfo, and the
// FrameDriver interface (exercised via a minimal dummy implementation —
// not a real desktop or XR driver; those come in later milestones).

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
        AREngine::Frame::FrameTiming WaitForNextFrame() override
        {
            AREngine::Frame::FrameTiming timing;
            timing.deltaTimeSeconds = 0.016;
            return timing;
        }

        std::vector<AREngine::Frame::ViewInfo> GetViews() override
        {
            return { AREngine::Frame::ViewInfo{} };
        }

        void SubmitFrame() override
        {
            ++submitCount;
        }

        int submitCount = 0;
    };

    void TestFrameTiming()
    {
        const AREngine::Frame::FrameTiming timing;
        Check(timing.deltaTimeSeconds == 0.0, "FrameTiming defaults deltaTimeSeconds to 0");
        Check(timing.totalTimeSeconds == 0.0, "FrameTiming defaults totalTimeSeconds to 0");
        Check(timing.predictedDisplayTimeSeconds == 0.0, "FrameTiming defaults predictedDisplayTimeSeconds to 0");
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

        const auto timing = driver.WaitForNextFrame();
        Check(timing.deltaTimeSeconds == 0.016, "FrameDriver::WaitForNextFrame returns timing");

        const auto views = driver.GetViews();
        Check(views.size() == 1, "FrameDriver::GetViews can return a view");

        driver.SubmitFrame();
        Check(driver.submitCount == 1, "FrameDriver::SubmitFrame can be called");
    }
}

int main()
{
    TestFrameTiming();
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
