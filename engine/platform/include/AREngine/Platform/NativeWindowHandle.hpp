#pragma once

namespace AREngine::Platform
{
    // Which OS a NativeWindowHandle's raw values came from — so a
    // consumer of a NativeWindowHandle can tell what `window`/
    // `instance` actually mean before touching them.
    enum class NativeWindowPlatform
    {
        Windows,
    };

    // A deliberately minimal, intentional escape hatch: raw,
    // OS-specific window handle values, opaque to everyone except the
    // one thing that genuinely needs them at this level — a graphics
    // API (Vulkan today; OpenXR later) creating a surface for this
    // window.
    //
    // This is NOT for gameplay code, and not for anything that can be
    // expressed through Window's normal API. It exists because Vulkan's
    // VkWin32SurfaceCreateInfoKHR genuinely needs an HWND and HINSTANCE
    // — there is no way to create a presentable surface without them.
    // Everything else about a window (size, events, close state) is
    // still reached through Window's ordinary methods; do not reach for
    // this to bypass them.
    //
    // On Windows: `window` is an HWND, `instance` is an HINSTANCE, both
    // stored as void* specifically so this header never needs to
    // #include Windows.h — see docs/ARCHITECTURE.md, "Native Window
    // Handle". A consumer that actually needs to use these (e.g.
    // Rendering's private Vulkan Win32 surface code) reinterpret_casts
    // them back to HWND/HINSTANCE itself, in its own Windows.h-including
    // translation unit.
    struct NativeWindowHandle
    {
        NativeWindowPlatform platform = NativeWindowPlatform::Windows;
        void* window = nullptr;
        void* instance = nullptr;
    };
}
