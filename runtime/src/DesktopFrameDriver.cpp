#include "AREngine/Runtime/DesktopFrameDriver.hpp"

namespace AREngine::Runtime
{
    DesktopFrameDriver::DesktopFrameDriver(Platform::Window& window)
        : m_window(window)
    {
    }

    Frame::FrameContext DesktopFrameDriver::PrepareFrame()
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
        // shouldRender stays at its default (true) — desktop has no
        // reason to ever skip rendering. FrameStatus stays at its
        // default (Continue) — desktop never returns Idle or Stop; see
        // docs/ARCHITECTURE.md, "Desktop Mapping (M9E.5)".
        return Frame::FrameContext{timing, Frame::FrameStatus::Continue};
    }

    void DesktopFrameDriver::BeginFrame()
    {
        // Nothing to do yet — there is no renderer. Once Rendering
        // exists, this is where per-frame desktop setup would happen.
    }

    std::vector<Frame::ViewInfo> DesktopFrameDriver::GetViews()
    {
        // Exactly one placeholder view: a fixed identity-pose camera.
        // There is no camera system yet (Scene, M5+) — this is
        // deliberately the simplest thing that satisfies "a frame needs
        // some number of views" without pretending to be a real one.
        return { Frame::ViewInfo{} };
    }

    void DesktopFrameDriver::EndFrame()
    {
        // Nothing to submit yet — there is no renderer. Once Rendering
        // exists, this is where a desktop present would happen.
    }
}
