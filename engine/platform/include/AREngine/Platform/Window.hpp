#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Platform/WindowDesc.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace AREngine::Platform
{
    // A single OS window. This public interface is deliberately free of
    // any Win32 (or other OS-specific) type — no HWND, no message
    // types, nothing. Concrete implementations (WindowsWindow today; a
    // future LinuxWindow/AndroidWindow) live entirely under
    // src/<platform>/ and are never named outside this module. See
    // docs/ARCHITECTURE.md.
    class Window
    {
    public:
        using EventCallback = std::function<void(Core::Event&)>;

        virtual ~Window() = default;

        // Processes any OS messages currently pending for this window.
        // Must be called regularly (e.g. once per frame) by whoever
        // owns it — nothing happens automatically in the background.
        virtual void PollEvents() = 0;

        // True once something (usually the user clicking the close
        // button) has asked this window to close. The Window does NOT
        // destroy itself when this becomes true — the owner decides
        // when to actually destroy it, e.g. by letting the owning
        // std::unique_ptr<Window> go out of scope.
        [[nodiscard]] virtual bool ShouldClose() const = 0;

        [[nodiscard]] virtual std::uint32_t GetWidth() const = 0;
        [[nodiscard]] virtual std::uint32_t GetHeight() const = 0;

        // Invoked for each window event (WindowCloseEvent,
        // WindowResizeEvent, ...) as it happens, if a callback is set.
        // Entirely optional — a Window works fine with none set.
        virtual void SetEventCallback(EventCallback callback) = 0;
    };

    // Creates a platform-appropriate Window (Windows today; see
    // docs/ARCHITECTURE.md for how Linux/Android would plug in later).
    // Ownership is returned to the caller.
    //
    // Named CreateAppWindow, not "CreateWindow": Windows.h #defines
    // CreateWindow as a macro (to CreateWindowA/CreateWindowW), which
    // would silently rewrite a function of that name wherever
    // Windows.h happens to be visible.
    [[nodiscard]] std::unique_ptr<Window> CreateAppWindow(const WindowDesc& desc);
}
