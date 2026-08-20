// Manual M2 validation demo — NOT part of the automated CTest suite.
// See tests/CMakeLists.txt: this target is built but never registered
// via add_test, since CTest cannot wait for a human and must never hang
// expecting one.
//
// Run it manually, e.g.: build\bin\Debug\arengine_platform_window_demo.exe
//
// What it proves:
//   1. A real Win32 window opens, titled "AREngine M2", sized 1280x720.
//   2. Messages are processed every loop iteration (PollEvents).
//   3. Clicking the close button is detected (ShouldClose() becomes
//      true) and the loop exits on its own.
//   4. The Window's destructor releases the OS window cleanly on exit
//      (no hang, no crash).
//
// This is only a Platform-level demo, not the engine's main loop — the
// real Runtime loop (built against Frame's FrameDriver) is M3.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Platform/Platform.hpp"

int main()
{
    using namespace AREngine;

    Platform::WindowDesc desc;
    desc.title = "AREngine M2";
    desc.width = 1280;
    desc.height = 720;

    auto window = Platform::CreateAppWindow(desc);

    window->SetEventCallback([](Core::Event& event)
    {
        if (dynamic_cast<Platform::WindowCloseEvent*>(&event) != nullptr)
        {
            AR_LOG_INFO("WindowCloseEvent received");
        }
        else if (auto* resizeEvent = dynamic_cast<Platform::WindowResizeEvent*>(&event))
        {
            AR_LOG_INFO("WindowResizeEvent received");
            (void)resizeEvent;
        }
    });

    AR_LOG_INFO("AREngine M2 window demo: close the window to exit.");

    Platform::SteadyClock clock;

    while (!window->ShouldClose())
    {
        window->PollEvents();
        (void)clock.Tick(); // exercised here only to prove Clock works; unused otherwise
    }

    AR_LOG_INFO("Window closed - exiting cleanly.");
    return 0;
}
