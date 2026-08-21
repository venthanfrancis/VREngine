#include "AREngine/Runtime/Runtime.hpp"

#include "AREngine/Core/Core.hpp"
#include "AREngine/Platform/WindowCloseEvent.hpp"
#include "AREngine/Platform/WindowResizeEvent.hpp"
#include "AREngine/Runtime/DesktopFrameDriver.hpp"

#include <format>

namespace AREngine::Runtime
{
    Runtime::Runtime(const RuntimeConfig& config)
    {
        Platform::WindowDesc windowDesc;
        windowDesc.title = config.windowTitle;
        windowDesc.width = config.windowWidth;
        windowDesc.height = config.windowHeight;

        m_window = Platform::CreateAppWindow(windowDesc);

        // Logging only. Window::ShouldClose() — checked directly in
        // Run() — is the single authoritative signal for ending the
        // loop; this callback never drives control flow, so there is
        // exactly one source of truth for "should the app end," not two
        // that could disagree.
        m_window->SetEventCallback([](Core::Event& event)
        {
            if (dynamic_cast<Platform::WindowCloseEvent*>(&event) != nullptr)
            {
                AR_LOG_INFO("Window close requested");
            }
            else if (auto* resize = dynamic_cast<Platform::WindowResizeEvent*>(&event))
            {
                AR_LOG_INFO(std::format("Window resized to {}x{}", resize->width, resize->height));
            }
        });

        // DesktopFrameDriver is constructed directly here rather than
        // through a factory: unlike Platform::Window (which hides
        // WindowsWindow behind CreateAppWindow specifically to keep
        // Win32 out of public headers), DesktopFrameDriver already lives
        // in this same module and leaks no platform-specific types, so
        // there is nothing for an extra layer of indirection to hide.
        // Swapping this one line for an XRFrameDriver later is the
        // entire cost of adding XR support to Runtime — Run()'s loop
        // below only ever calls through the Frame::FrameDriver
        // interface, so it would not need to change at all.
        m_frameDriver = std::make_unique<DesktopFrameDriver>(*m_window);

        AR_LOG_INFO("Runtime initialized");
    }

    Runtime::~Runtime()
    {
        // Both members are std::unique_ptr and clean themselves up; this
        // destructor exists to log the moment of shutdown. Destruction
        // order (frame driver, then window) is guaranteed correct by
        // member declaration order — see the comment in Runtime.hpp.
        AR_LOG_INFO("Runtime shutting down");
    }

    void Runtime::Run()
    {
        AR_LOG_INFO("Runtime loop starting");

        // FPS is accumulated over roughly one second rather than logged
        // every frame, which would flood the console at hundreds or
        // thousands of frames per second.
        double fpsAccumulatedSeconds = 0.0;
        int fpsFrameCount = 0;

        while (true)
        {
            // Process OS messages, then check ShouldClose() immediately
            // afterward — before doing any frame work. If a new frame
            // were started first and ShouldClose() only checked
            // afterward, closing the window would still cost one more
            // full frame of (currently pointless, since nothing renders
            // yet) work before the loop noticed. Checking right here
            // means a close request is acted on the moment it's known.
            m_window->PollEvents();
            if (m_window->ShouldClose())
            {
                break;
            }

            const Frame::FrameTiming timing = m_frameDriver->WaitForNextFrame();
            const std::vector<Frame::ViewInfo> views = m_frameDriver->GetViews();
            (void)views; // nothing consumes views yet — no renderer until M4+

            // "Update runtime/application state" belongs here once
            // there is any state to update (Scene, M5+).

            fpsAccumulatedSeconds += timing.deltaTimeSeconds;
            ++fpsFrameCount;
            if (fpsAccumulatedSeconds >= 1.0)
            {
                const double fps = static_cast<double>(fpsFrameCount) / fpsAccumulatedSeconds;
                AR_LOG_INFO(std::format("FPS: {:.1f} ({} frames / {:.3f}s)",
                                        fps, fpsFrameCount, fpsAccumulatedSeconds));
                fpsAccumulatedSeconds = 0.0;
                fpsFrameCount = 0;
            }

            m_frameDriver->SubmitFrame();
        }

        AR_LOG_INFO("Runtime loop stopped");
    }
}
