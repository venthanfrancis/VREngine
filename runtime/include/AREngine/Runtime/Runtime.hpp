#pragma once

#include "AREngine/Frame/FrameDriver.hpp"
#include "AREngine/Platform/Window.hpp"
#include "AREngine/Runtime/RuntimeConfig.hpp"

#include <memory>

namespace AREngine::Runtime
{
    // Owns the window and frame driver for one running application
    // instance, and runs its main loop. See docs/ARCHITECTURE.md,
    // "Runtime Ownership Model" (M3).
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
        // Declaration order matters: members are destroyed in reverse
        // declaration order, and m_frameDriver holds a reference to
        // *m_window, so m_frameDriver must be destroyed first. Keeping
        // m_window declared before m_frameDriver is what guarantees
        // that automatically.
        std::unique_ptr<Platform::Window> m_window;
        std::unique_ptr<Frame::FrameDriver> m_frameDriver;
    };
}
