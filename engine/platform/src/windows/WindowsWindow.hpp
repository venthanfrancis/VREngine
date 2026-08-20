#pragma once

// Win32-specific. This header is private implementation detail of the
// Platform module — it must never be included by anything outside
// engine/platform/src/windows/, and Windows.h must never leak past this
// boundary. See docs/ARCHITECTURE.md: Platform's public headers do not
// expose Win32 types.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <Windows.h>

#include "AREngine/Platform/Window.hpp"

namespace AREngine::Platform
{
    class WindowsWindow final : public Window
    {
    public:
        explicit WindowsWindow(const WindowDesc& desc);
        ~WindowsWindow() override;

        // Non-copyable, non-movable: this class owns exactly one HWND
        // for its entire lifetime, and the destructor is the single
        // place that destroys it. Allowing moves would mean deciding
        // which of two WindowsWindow objects owns the HWND after a
        // move, for no benefit at this milestone — simplest to just not
        // allow it.
        WindowsWindow(const WindowsWindow&) = delete;
        WindowsWindow& operator=(const WindowsWindow&) = delete;
        WindowsWindow(WindowsWindow&&) = delete;
        WindowsWindow& operator=(WindowsWindow&&) = delete;

        void PollEvents() override;
        [[nodiscard]] bool ShouldClose() const override;
        [[nodiscard]] std::uint32_t GetWidth() const override;
        [[nodiscard]] std::uint32_t GetHeight() const override;
        void SetEventCallback(EventCallback callback) override;

        // Public because RegisterClassExW needs its address from outside
        // the class (see EnsureWindowClassRegistered in the .cpp) — it's
        // a Win32 callback handed to the OS, not part of the ownership
        // API, but C++ access control doesn't distinguish those cases.
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        HWND m_hwnd = nullptr;
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        bool m_shouldClose = false;
        EventCallback m_eventCallback;
    };
}
