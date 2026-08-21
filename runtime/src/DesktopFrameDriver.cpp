#include "AREngine/Runtime/DesktopFrameDriver.hpp"

namespace AREngine::Runtime
{
    DesktopFrameDriver::DesktopFrameDriver(Platform::Window& window)
        : m_window(window)
    {
    }

    Frame::FrameTiming DesktopFrameDriver::WaitForNextFrame()
    {
        // "Wait" is a no-op on desktop today — there is nothing to
        // synchronize with yet (no vsync/present; see
        // docs/ARCHITECTURE.md, "RHI Presentation"). This is still the
        // right place for that to live once it exists: only this
        // method's body would change, not Runtime's loop or the
        // FrameDriver interface itself.
        Frame::FrameTiming timing;
        timing.deltaTimeSeconds = m_clock.Tick();
        timing.totalTimeSeconds = m_clock.ElapsedSeconds();
        // predictedDisplayTimeSeconds stays at its default (0) —
        // meaningless on desktop; real values arrive with XRFrameDriver.
        return timing;
    }

    std::vector<Frame::ViewInfo> DesktopFrameDriver::GetViews()
    {
        // Exactly one placeholder view: a fixed identity-pose camera.
        // There is no camera system yet (Scene, M5+) — this is
        // deliberately the simplest thing that satisfies "a frame needs
        // some number of views" without pretending to be a real one.
        return { Frame::ViewInfo{} };
    }

    void DesktopFrameDriver::SubmitFrame()
    {
        // Nothing to submit yet — there is no renderer. Once Rendering
        // exists, this is where a desktop present would happen.
    }
}
