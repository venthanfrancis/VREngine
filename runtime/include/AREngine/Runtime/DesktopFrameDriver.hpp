#pragma once

#include "AREngine/Frame/FrameDriver.hpp"
#include "AREngine/Platform/Clock.hpp"
#include "AREngine/Platform/Window.hpp"

namespace AREngine::Runtime
{
    // The desktop implementation of Frame::FrameDriver — this engine's
    // first FrameDriver, used for every milestone before OpenXR (M9)
    // exists. See docs/ARCHITECTURE.md, "The FrameDriver Abstraction".
    //
    // Responsible only for desktop frame lifecycle/timing. It does not
    // touch rendering: no Present, no swapchain (see ARCHITECTURE.md,
    // "RHI Presentation" — that's still deliberately undesigned), and it
    // produces exactly one placeholder ViewInfo, since there is no
    // camera system yet (Scene, M5+).
    //
    // Lives in Runtime, not Frame or Platform: it needs both Frame (the
    // interface it implements) and Platform (the Window/Clock it
    // drives), and Runtime is the one module already wired to depend on
    // both — see docs/ARCHITECTURE.md, "DesktopFrameDriver Placement".
    class DesktopFrameDriver final : public Frame::FrameDriver
    {
    public:
        // Does not own the window — the caller (Runtime) does, and must
        // keep it alive for at least as long as this DesktopFrameDriver.
        explicit DesktopFrameDriver(Platform::Window& window);

        DesktopFrameDriver(const DesktopFrameDriver&) = delete;
        DesktopFrameDriver& operator=(const DesktopFrameDriver&) = delete;
        DesktopFrameDriver(DesktopFrameDriver&&) = delete;
        DesktopFrameDriver& operator=(DesktopFrameDriver&&) = delete;

        Frame::FrameContext PrepareFrame() override;
        void BeginFrame() override;
        std::vector<Frame::ViewInfo> GetViews() override;
        void EndFrame() override;

    private:
        // Currently unused beyond being stored — kept because a
        // DesktopFrameDriver is inherently tied to a specific window
        // (its eventual Present call, and view sizing once a camera
        // exists, both need it), and taking it in the constructor now
        // means Runtime never has to pass it again later.
        Platform::Window& m_window;
        Platform::SteadyClock m_clock;
    };
}
