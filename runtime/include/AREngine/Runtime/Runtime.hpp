#pragma once

#include "AREngine/Frame/FrameDriver.hpp"
#include "AREngine/Input/Input.hpp"
#include "AREngine/Platform/Window.hpp"
#include "AREngine/Rendering/Handles.hpp"
#include "AREngine/Rendering/RenderDevice.hpp"
#include "AREngine/Runtime/RuntimeConfig.hpp"

#include <memory>

namespace AREngine::Runtime
{
    // Owns the window, frame driver, render device, and input system
    // for one running application instance, and runs its main loop. See
    // docs/ARCHITECTURE.md, "Runtime Ownership Model" (M3), "M4
    // Implementation Notes", and "M7 Implementation Notes".
    //
    // Deliberately not a singleton and not a service locator: an
    // application constructs exactly one Runtime explicitly and owns it
    // (Sandbox does this on the stack in main()). There is no global
    // mutable engine state anywhere in this class.
    class Runtime
    {
    public:
        explicit Runtime(const RuntimeConfig& config);
        ~Runtime();

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        Runtime(Runtime&&) = delete;
        Runtime& operator=(Runtime&&) = delete;

        // Runs the main loop until the window is closed. Blocks until
        // then.
        void Run();

    private:
        // Declaration order matters — members are destroyed in reverse
        // declaration order:
        //
        //   1. m_inputSystem must outlive m_window. m_window's event
        //      callback (registered in the constructor) calls into
        //      m_inputSystem, and Win32's DestroyWindow (called from
        //      ~WindowsWindow, reached via ~m_window) can synchronously
        //      fire one last WM_KILLFOCUS *during window teardown* —
        //      which the callback forwards to m_inputSystem.OnEvent().
        //      If m_inputSystem were declared (and therefore destroyed)
        //      before m_window, that late event would call OnEvent() on
        //      an already-destroyed InputSystem: a real, reproducible
        //      access-violation crash caught during M7 manual testing —
        //      see docs/ARCHITECTURE.md, "Event Routing" for the full
        //      story. Declaring m_inputSystem first means it is
        //      destroyed *last*, after m_window, so it is guaranteed to
        //      still be alive for any event the window's destruction
        //      itself generates.
        //   2. m_frameDriver holds a reference to *m_window, so it must
        //      be destroyed before m_window.
        //   3. m_renderDevice and m_dummyVertexBuffer reference neither
        //      of the above, so their position is not load-bearing.
        Input::InputSystem m_inputSystem;
        std::unique_ptr<Platform::Window> m_window;
        std::unique_ptr<Frame::FrameDriver> m_frameDriver;
        std::unique_ptr<Rendering::RenderDevice> m_renderDevice;

        // TEMPORARY (M4 validation only): a placeholder vertex buffer
        // that exists purely to give the dummy draw call in Run()
        // something valid to reference, proving CreateBuffer ->
        // SubmitDraw works end to end. Removed once Scene (M5) provides
        // real geometry to draw.
        Rendering::BufferHandle m_dummyVertexBuffer;
    };
}
