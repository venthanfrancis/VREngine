#include "WindowsWindow.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Platform/KeyPressedEvent.hpp"
#include "AREngine/Platform/KeyReleasedEvent.hpp"
#include "AREngine/Platform/MouseButtonPressedEvent.hpp"
#include "AREngine/Platform/MouseButtonReleasedEvent.hpp"
#include "AREngine/Platform/MouseMovedEvent.hpp"
#include "AREngine/Platform/WindowCloseEvent.hpp"
#include "AREngine/Platform/WindowFocusLostEvent.hpp"
#include "AREngine/Platform/WindowResizeEvent.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace AREngine::Platform
{
    namespace
    {
        constexpr const wchar_t* kWindowClassName = L"AREngineWindowClass";

        // Translates a Win32 virtual-key code into AREngine's KeyCode.
        // Only the keys KeyCode.hpp actually defines — not every key a
        // keyboard has. Returns KeyCode::Unknown for anything else,
        // rather than asserting: an unmapped key being pressed is a
        // normal, expected occurrence (this engine just doesn't have an
        // opinion about it yet), not a programmer error.
        //
        // Left/right Shift and Ctrl need special handling: Win32 always
        // reports WM_KEYDOWN/UP for either Shift key as the generic
        // VK_SHIFT (and both Ctrl keys as VK_CONTROL) — the only way to
        // tell left from right is to pull the hardware scan code out of
        // lParam and ask MapVirtualKeyW to expand it to the specific
        // extended virtual-key. (Alt/Ctrl also have a simpler
        // lParam-extended-bit trick, but it doesn't work for Shift, so
        // the scan-code approach is used uniformly for both here.)
        Core::KeyCode TranslateVirtualKey(WPARAM vkCode, LPARAM lParam)
        {
            if (vkCode >= 'A' && vkCode <= 'Z')
            {
                return static_cast<Core::KeyCode>(
                    static_cast<int>(Core::KeyCode::A) + static_cast<int>(vkCode - 'A'));
            }
            if (vkCode >= '0' && vkCode <= '9')
            {
                return static_cast<Core::KeyCode>(
                    static_cast<int>(Core::KeyCode::Num0) + static_cast<int>(vkCode - '0'));
            }

            if (vkCode == VK_SHIFT || vkCode == VK_CONTROL)
            {
                const UINT scanCode = static_cast<UINT>((lParam >> 16) & 0xFF);
                const UINT extendedVk = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);

                if (vkCode == VK_SHIFT)
                {
                    return (extendedVk == VK_RSHIFT) ? Core::KeyCode::RightShift : Core::KeyCode::LeftShift;
                }
                return (extendedVk == VK_RCONTROL) ? Core::KeyCode::RightCtrl : Core::KeyCode::LeftCtrl;
            }

            switch (vkCode)
            {
                case VK_ESCAPE: return Core::KeyCode::Escape;
                case VK_SPACE:  return Core::KeyCode::Space;
                case VK_RETURN: return Core::KeyCode::Enter;
                case VK_TAB:    return Core::KeyCode::Tab;
                case VK_UP:     return Core::KeyCode::Up;
                case VK_DOWN:   return Core::KeyCode::Down;
                case VK_LEFT:   return Core::KeyCode::Left;
                case VK_RIGHT:  return Core::KeyCode::Right;
                default:        return Core::KeyCode::Unknown;
            }
        }

        std::wstring ToWide(const std::string& text)
        {
            if (text.empty())
            {
                return {};
            }

            const int requiredLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            std::wstring wide(static_cast<std::size_t>(requiredLength), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), requiredLength);
            wide.resize(static_cast<std::size_t>(requiredLength) - 1); // drop the trailing null
            return wide;
        }

        // Registers the Win32 window class the first time any
        // WindowsWindow is created, and never again — RegisterClassExW
        // only needs to happen once per process. This is a
        // function-local static, not a Platform-wide singleton: it's an
        // implementation detail of "how do I make a window," not shared
        // engine state.
        void EnsureWindowClassRegistered()
        {
            static bool registered = false;
            if (registered)
            {
                return;
            }

            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(WNDCLASSEXW);
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            windowClass.lpfnWndProc = &WindowsWindow::WndProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            windowClass.lpszClassName = kWindowClassName;

            const ATOM result = RegisterClassExW(&windowClass);
            AR_ASSERT_MSG(result != 0, "Failed to register the AREngine window class");
            registered = true;
        }
    }

    WindowsWindow::WindowsWindow(const WindowDesc& desc)
        : m_width(desc.width)
        , m_height(desc.height)
    {
        EnsureWindowClassRegistered();

        const std::wstring wideTitle = ToWide(desc.title);

        constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW;

        // WindowDesc::width/height mean the client (renderable) area —
        // what a caller actually cares about — but CreateWindowExW's
        // nWidth/nHeight are the OUTER window size, title bar and
        // borders included. AdjustWindowRect converts "I want this much
        // client area" into "so create the window this big overall," so
        // the client area we end up with actually matches what was
        // requested instead of being smaller by however big the frame
        // is.
        RECT windowRect{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
        AdjustWindowRect(&windowRect, kWindowStyle, FALSE);

        // Passing `this` as the trailing lpParam is what lets WndProc
        // (a plain C-style callback with no notion of "this") find its
        // way back to the right WindowsWindow instance — see WndProc
        // below.
        m_hwnd = CreateWindowExW(
            0,
            kWindowClassName,
            wideTitle.c_str(),
            kWindowStyle,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
            nullptr, nullptr, GetModuleHandleW(nullptr),
            this);

        AR_ASSERT_MSG(m_hwnd != nullptr, "Failed to create the window");

        ShowWindow(m_hwnd, SW_SHOW);
    }

    WindowsWindow::~WindowsWindow()
    {
        // The single place this window is ever asked to be destroyed.
        // WM_CLOSE (handled below) deliberately does NOT call
        // DestroyWindow itself — it only sets m_shouldClose and
        // notifies listeners — so there is exactly one code path that
        // can initiate destruction.
        //
        // m_hwnd itself is set back to nullptr by the WM_NCDESTROY
        // handler below, not here — WM_NCDESTROY is the message Win32
        // sends once it has *definitively* finished tearing the native
        // window down (it fires synchronously inside this very
        // DestroyWindow call), so that is the one place that can say
        // for certain "the HWND is gone now," regardless of what
        // triggered destruction. By the time DestroyWindow() returns
        // below, m_hwnd is already nullptr.
        if (m_hwnd != nullptr)
        {
            DestroyWindow(m_hwnd);
        }
    }

    void WindowsWindow::PollEvents()
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    bool WindowsWindow::ShouldClose() const
    {
        return m_shouldClose;
    }

    std::uint32_t WindowsWindow::GetWidth() const
    {
        return m_width;
    }

    std::uint32_t WindowsWindow::GetHeight() const
    {
        return m_height;
    }

    void WindowsWindow::SetEventCallback(EventCallback callback)
    {
        m_eventCallback = std::move(callback);
    }

    NativeWindowHandle WindowsWindow::GetNativeHandle() const
    {
        NativeWindowHandle handle;
        handle.platform = NativeWindowPlatform::Windows;
        handle.window = reinterpret_cast<void*>(m_hwnd);
        handle.instance = reinterpret_cast<void*>(GetModuleHandleW(nullptr));
        return handle;
    }

    LRESULT CALLBACK WindowsWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            // CreateWindowExW's lpParam (the `this` we passed in) is
            // available here via lParam. Stash it in the HWND's user
            // data so every later message can find its way back to the
            // right C++ object.
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* window = static_cast<WindowsWindow*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }

        auto* window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (window != nullptr)
        {
            return window->HandleMessage(hwnd, message, wParam, lParam);
        }

        // Messages that can arrive before WM_NCCREATE has run (or if
        // something goes looking for user data too early) fall back to
        // default handling using the real hwnd — never m_hwnd, which
        // this WindowsWindow instance may not have finished storing yet.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT WindowsWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
            case WM_CLOSE:
            {
                // The user (or the OS) is asking this window to close.
                // We do NOT call DestroyWindow() here — see the
                // destructor's comment for why there must be exactly
                // one owner of that decision. We just record the
                // request and notify whoever is listening; it's up to
                // them to actually destroy the Window (e.g. Runtime's
                // loop sees ShouldClose() and lets it go out of scope).
                m_shouldClose = true;

                if (m_eventCallback)
                {
                    WindowCloseEvent event;
                    m_eventCallback(event);
                }

                // Returning 0 without calling DefWindowProc is what
                // stops the default handling (which would otherwise
                // call DestroyWindow itself) from running.
                return 0;
            }

            case WM_DESTROY:
            {
                // Only ever reached because our own destructor called
                // DestroyWindow(). Teardown is underway but not yet
                // final — the HWND is still a valid handle at this
                // point, just about to stop being one. Nothing to do
                // here yet; if something needs to release a resource
                // tied specifically to this window's lifetime later,
                // this is where it would go. Do not clear m_hwnd here —
                // that happens in WM_NCDESTROY, once destruction is
                // actually final.
                return 0;
            }

            case WM_NCDESTROY:
            {
                // The last message this window will ever receive: Win32
                // has now definitively finished destroying the native
                // window, and the HWND is no longer valid past this
                // call. This message must still reach DefWindowProc —
                // that is where Win32 releases the window's internal
                // per-window data — so we forward it before clearing
                // our own state, not instead of forwarding it.
                //
                // Deliberately not "if (hwnd == m_hwnd)": m_hwnd itself
                // is what we're about to null out, so comparing against
                // it here would be circular. hwnd (the real, live
                // handle Win32 just handed us) is what this window
                // actually is, regardless of what our own member
                // currently holds.
                const LRESULT result = DefWindowProcW(hwnd, message, wParam, lParam);
                m_hwnd = nullptr;
                return result;
            }

            case WM_SIZE:
            {
                m_width = LOWORD(lParam);
                m_height = HIWORD(lParam);

                if (m_eventCallback)
                {
                    WindowResizeEvent event(m_width, m_height);
                    m_eventCallback(event);
                }

                return 0;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                // Fired on every WM_KEYDOWN, repeats included — see the
                // comment on KeyPressedEvent for why filtering repeats
                // into a genuine up->down transition is InputSystem's
                // job, not this one's.
                if (m_eventCallback)
                {
                    KeyPressedEvent event(TranslateVirtualKey(wParam, lParam));
                    m_eventCallback(event);
                }
                return 0;
            }

            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                if (m_eventCallback)
                {
                    KeyReleasedEvent event(TranslateVirtualKey(wParam, lParam));
                    m_eventCallback(event);
                }
                return 0;
            }

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            {
                if (m_eventCallback)
                {
                    const Core::MouseButton button = (message == WM_LBUTTONDOWN) ? Core::MouseButton::Left
                                                    : (message == WM_RBUTTONDOWN) ? Core::MouseButton::Right
                                                                                   : Core::MouseButton::Middle;
                    MouseButtonPressedEvent event(button);
                    m_eventCallback(event);
                }
                return 0;
            }

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            {
                if (m_eventCallback)
                {
                    const Core::MouseButton button = (message == WM_LBUTTONUP) ? Core::MouseButton::Left
                                                    : (message == WM_RBUTTONUP) ? Core::MouseButton::Right
                                                                                 : Core::MouseButton::Middle;
                    MouseButtonReleasedEvent event(button);
                    m_eventCallback(event);
                }
                return 0;
            }

            case WM_MOUSEMOVE:
            {
                if (m_eventCallback)
                {
                    // Client-area coordinates already match the
                    // documented convention (top-left origin, +x right,
                    // +y down) with no conversion needed. Cast through
                    // SHORT, not plain LOWORD/HIWORD, so coordinates
                    // that go negative (e.g. while dragging the cursor
                    // partly off-window) come out correctly signed.
                    const float x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                    const float y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                    MouseMovedEvent event(Core::Math::Vec2(x, y));
                    m_eventCallback(event);
                }
                return 0;
            }

            case WM_KILLFOCUS:
            {
                // See WindowFocusLostEvent: this exists so InputSystem
                // can treat every currently-held key/button as released
                // rather than leaving it stuck down forever, since the
                // matching key-up message for a key held when focus is
                // lost may never arrive at this window.
                if (m_eventCallback)
                {
                    WindowFocusLostEvent event;
                    m_eventCallback(event);
                }
                return 0;
            }

            default:
                return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }

    std::unique_ptr<Window> CreateAppWindow(const WindowDesc& desc)
    {
        return std::make_unique<WindowsWindow>(desc);
    }
}
