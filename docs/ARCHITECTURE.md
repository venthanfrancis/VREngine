# AREngine Architecture

Status: approved as of M0 (repository scaffolding). This document is the
source of truth for module layout and layering rules — check it before
adding a dependency between modules.

## 1. High-Level Architecture

The engine is organized in layers. A module may only depend on modules
below it, never above.

```
Layer 6:  Editor            Sandbox (test games)
Layer 5:  Runtime          (wires everything together, owns the main loop)
Layer 4:  Scene   Physics*  Audio*  Assets   Input   XR
Layer 3:  Rendering (RHI — GPU operations only, no presentation)
Layer 2:  Frame     (FrameDriver / FrameTiming / ViewInfo)
Layer 2:  Platform  (windowing, files, threads, time — OS-specific, hidden)
Layer 1:  Core      (math, logging, assertions, events — depends on nothing)
```

`*` Physics and Audio are architected at this layer but their
implementation is deprioritized — see docs/ROADMAP.md.

**Why layers:** if `Scene` (the game world) could call Vulkan or OpenXR
directly, every gameplay feature would be tangled with graphics/XR
details, and changing the renderer or porting to Android would mean
touching gameplay code too. Layers force communication through small,
stable interfaces so what's underneath an interface can change without
breaking what's above it.

## 2. Module Responsibilities

- **Core** — math, logging, assertions, event system. Depends on nothing.
  Use `std::` containers throughout the engine; no custom container types
  without a measured (profiled) reason.
- **Frame** — `FrameDriver` interface, `FrameTiming`, `ViewInfo`. Depends
  only on Core. See Section 3.
- **Platform** — the only module allowed to call OS-specific APIs (Win32
  today; Android/Linux later): window creation, raw input polling, file
  I/O, timing, dynamic library loading.
- **Rendering** — the RHI (Render Hardware Interface): GPU rendering
  operations only (create buffer/texture/pipeline, submit draw calls).
  Kept intentionally small. See Section 4.
- **Scene** — the game world: entities, transforms, hierarchy. Holds
  data; hands a render item list to Rendering but never calls a graphics
  API itself.
- **XR** — wraps XR runtime concepts (head pose, hand/controller
  tracking, spatial anchors, passthrough) behind generic engine types, so
  gameplay never calls an XR API directly. Will eventually implement
  `XRFrameDriver`.
- **Input** — turns raw device input into named actions that gameplay
  binds to, instead of binding to specific devices.
- **Assets** — loads and caches resources (meshes, textures, audio,
  scene files) from disk, exposed via resource handles.
- **Physics** *(deprioritized)* — collision & simulation, reads/writes
  Scene's transform data through an interface. Scene never depends on
  Physics.
- **Audio** *(deprioritized)* — playback and spatial (3D) sound.
- **Runtime** — the glue: owns the main loop and module
  init/update/shutdown order, and a `FrameDriver`. Depends on every
  engine module. What Editor and Sandbox link against.
- **Editor** — authoring tool (scene view, inspector, asset browser).
  Depends only on Runtime.
- **Sandbox** — lightweight test bed / example projects. Depends only on
  Runtime.

## 3. The FrameDriver Abstraction

A normal desktop loop controls its own timing: decide to render, call the
graphics API, present, repeat. OpenXR inverts this — the XR runtime tells
the app when to start rendering (wait for next frame), hands it a
**predicted display time**, the app asks for the **predicted head pose(s)**
for that specific future moment, renders (potentially two views, one per
eye), and submits. The XR runtime is in control of pacing, not the app.

If `Runtime`'s main loop assumed it always owns timing and always has one
camera, it would need a rewrite when OpenXR arrives. Instead, `Runtime`'s
loop is written against a small `FrameDriver` interface:

- **Wait for the next frame** → returns timing info (predicted display
  time, delta time)
- **Get the view(s) to render for this frame** → a small list of
  `{pose, projection}` — length 1 on desktop, length 2 under OpenXR stereo
- **Submit the completed frame**

Two implementations, neither more "real" than the other:
- `DesktopFrameDriver` — plain loop + vsync/present, single fixed-camera
  view. Used from the first runnable milestone onward.
- `XRFrameDriver` (M9) — wraps OpenXR's wait/locate-views/end-frame,
  real predicted head pose, stereo views.

**Why this lives in its own `Frame` module, not in `Core`:** Core must
stay a minimal foundation (math, logging, assertions, events) with no
rendering/XR concepts. `Frame` depends only on Core, and `Runtime`,
`DesktopFrameDriver`, and eventually `XRFrameDriver` (inside the `XR`
module) all depend on `Frame` — without `Core` ever knowing rendering or
XR exist.

This is deliberately just one interface and two small data structs — not
a render graph or job scheduler.

## 4. RHI Presentation — Deliberately Deferred

`Rendering` is scoped to GPU operations only (create buffer/texture/
pipeline, draw). It does **not** define `Present()` or own frame
lifecycle/submission.

**Why:** desktop Vulkan presentation (swapchain-based) and OpenXR frame
submission (`xrEndFrame`, layers, timing) have different lifecycle
requirements. Designing a shared `Present()` into the RHI now, before
either backend exists, would mean guessing at both at once. Frame
lifecycle and presentation/submission are kept separate from GPU
rendering operations for now, owned instead by `Frame` + `Runtime`. This
interface will be refined once the Vulkan backend (M8) and the OpenXR
backend (M9) reveal their actual requirements.

## 5. Dependency Graph

```
Core
 ├─ Frame     (Core only)
 ├─ Platform  (Core only)
 ├─ Rendering (Core only, for now — see Section 4)
 ├─ Scene     (Core only, optionally Assets later)
 ├─ Assets    (Core, Platform)
 ├─ Input     (Core, Platform)
 └─ XR        (Core, Platform, Frame)

Runtime  ← depends on Core, Frame, Platform, Rendering, Scene, Assets, Input, XR
Editor   ← depends only on Runtime
Sandbox  ← depends only on Runtime
```

Hard rule: dependencies only point downward. `Core` never includes a
`Rendering` header. `Rendering` never includes a `Scene` header. Nothing
under `engine/` or `runtime/` ever includes anything from `editor/` or
`sandbox/`.

## 6. Modularity Conventions

Each module is its own library target with a public/private split:
`include/` is the module's public API (all other modules may see this);
`src/` is private implementation. CMake target names are
`arengine_<module>` (e.g. `arengine_core`), aliased to `AREngine::<Module>`
(e.g. `AREngine::Core`) for consumers. All modules build as C++20 static
libraries for now.

## 7. World Conventions

See `docs/WORLD_CONVENTIONS.md` for units and coordinate system — these
are fixed and affect `Core`'s math library from the first real
implementation onward.

## 8. Roadmap

See `docs/ROADMAP.md` for the milestone plan and current status.

## 9. M1 Implementation Notes

M1 implemented `Core`'s math/logging/assertions/events and `Frame`'s
data/interface types. Two concrete decisions worth recording:

- **`Mat4` storage**: 16 floats, `element(row, col)` at `m[col * 4 + row]`.

  `Mat4` uses column-major storage as an AREngine convention.

  This convention must remain consistent between engine math code,
  shader data layouts, transformation code, and future rendering
  backends.

  Vulkan itself does not require the engine to use column-major
  matrices — matrix interpretation depends on how we lay out data and
  how our shaders read it. Column-major is simply the convention this
  engine has chosen; it is not a Vulkan requirement.
- **`Quaternion` storage**: Hamilton `(w, x, y, z)` order; identity is
  `(1, 0, 0, 0)`.

`Core`'s math, logging, assertions, and `Event` base type, and `Frame`'s
`FrameTiming`/`ViewInfo`/`FrameDriver`, are now implemented as designed
in Sections 2–4 above — this section only records decisions not already
covered there (e.g. exact memory layout). See `docs/WORLD_CONVENTIONS.md`
for the cross-product handedness note the math library's tests rely on.

## 10. M2 Implementation Notes

M2 implemented `Platform`'s `Window` abstraction and its Windows/Win32
backend, plus `SteadyClock`. Concrete decisions worth recording:

- **Public/private boundary**: `Window`, `WindowDesc`, `WindowCloseEvent`,
  `WindowResizeEvent`, and `Clock` (all under `engine/platform/include/`)
  expose zero Win32 types. `Windows.h` is included in exactly two files,
  both private implementation: `engine/platform/src/windows/
  WindowsWindow.hpp` and `.cpp`. Neither is ever included from outside
  that folder. `WIN32_LEAN_AND_MEAN`, `NOMINMAX`, `UNICODE`, and
  `_UNICODE` are defined immediately before that single `#include
  <Windows.h>` — `NOMINMAX` specifically so Windows.h's `min`/`max`
  macros can never contaminate engine code that includes a Platform
  header transitively.
- **`CreateAppWindow`, not `CreateWindow`**: `Windows.h` `#define`s
  `CreateWindow` as a macro to `CreateWindowA`/`CreateWindowW`. A
  function named `CreateWindow` in `Window.hpp` would get silently
  rewritten by that macro in any translation unit where both the header
  and `Windows.h` are visible. Naming it `CreateAppWindow` sidesteps the
  collision entirely rather than working around it with `#undef`.
- **`WindowDesc::width`/`height` mean the client (renderable) area**,
  not the OS window's outer size. `CreateWindowExW`'s `nWidth`/`nHeight`
  are the outer size (title bar and borders included), so
  `WindowsWindow`'s constructor calls `AdjustWindowRect` to convert the
  requested client size into the outer size `CreateWindowExW` actually
  needs. This was caught by `tests/platform_tests.cpp`'s
  `TestWindowLifecycle` check, which failed until this conversion was
  added — worth keeping in mind as an example of what that test is
  actually for.
- **No Win32 lifecycle/singleton for Platform itself**: there is no
  `Platform::Init()`/`Shutdown()`. The one piece of process-wide Win32
  setup M2 needs — `RegisterClassExW` for the window class — happens
  lazily, once, via a function-local `static bool` inside
  `WindowsWindow.cpp`. This is scoped implementation detail of "how do I
  make a window," not shared engine state, so it doesn't count as the
  "giant Platform singleton" the project wants to avoid. A real
  lifecycle abstraction can be introduced later if something (multiple
  windows with different classes, XR) actually needs shared init order.

  **Window-class registration lifecycle**, spelled out for when
  `Runtime` starts depending on `Platform` and possibly creates more
  than one `Window`:

  - Every `WindowsWindow` — regardless of how many exist — shares the
    *same* registered class, `"AREngineWindowClass"`. This is safe and
    standard: the class only supplies `WndProc` and a couple of static
    properties (cursor, style); all of a window's actual state
    (`m_hwnd`, `m_width`, callbacks, ...) lives in the per-instance
    `WindowsWindow` object, reached via `GWLP_USERDATA`, not in the
    class. One class describing many window instances is the normal
    Win32 pattern, not a shortcut specific to having only one window
    today.
  - `EnsureWindowClassRegistered()`'s `static bool registered` guard
    means a second (or third, ...) `WindowsWindow` never attempts to
    register the class again — `RegisterClassExW` on an
    already-registered class name would fail, so this guard isn't just
    an optimization, it's required correctness. **Window A registers,
    Window B reuses — never re-registers.**
  - The class is **never unregistered** — there is no
    `UnregisterClassW` call anywhere. Destroying a `WindowsWindow` only
    ever destroys *that window's HWND*; it has no effect on the shared
    class. This sidesteps the "Window A destroyed, unregisters the
    class, Window B still needs it" hazard entirely, by design: nothing
    ever unregisters the class while the process is running. The OS
    automatically cleans up all of a process's registered window
    classes when the process exits, so there's no leak to manage by
    hand.
  - Net effect: registration is a one-time, per-process, lazily-paid
    cost with no teardown to get wrong. This holds regardless of how
    many `Window`s `Runtime` creates or destroys, in what order.
- **File I/O was not implemented.** Nothing in M2 needs it — the window
  demo reads no files. Per `docs/ROADMAP.md`'s rule against speculative
  code, this is deferred to whichever milestone has an actual consumer
  (`Assets`, M6, or earlier if one comes up sooner).
- **Window close/destroy lifetime**: `WM_CLOSE` only sets an internal
  `m_shouldClose` flag and fires `WindowCloseEvent` — it does not call
  `DestroyWindow`. The `WindowsWindow` destructor is the single place
  `DestroyWindow` is ever called; `WM_DESTROY` (sent synchronously by
  that call) does no teardown of its own, since teardown is underway
  but not yet final. `m_hwnd` is cleared to `nullptr` in the
  `WM_NCDESTROY` handler, not in the destructor and not in
  `WM_DESTROY` — `WM_NCDESTROY` is the last message a window ever
  receives, sent once Win32 has *definitively* finished tearing it
  down, so it's the one point that can say for certain "the HWND is
  gone," regardless of what triggered destruction. This keeps exactly
  one owner of "when does the OS window actually get destroyed" and
  gives `m_hwnd` an accurate view of the native object's lifetime — see
  the destructor's and the `WM_CLOSE`/`WM_DESTROY`/`WM_NCDESTROY`
  handlers' comments in `WindowsWindow.cpp` for the full reasoning.
  `PostQuitMessage`/`WM_QUIT` are deliberately not used: the demo loop
  checks `Window::ShouldClose()` directly rather than waiting on
  `WM_QUIT` via a blocking `GetMessage` loop, so there is only one
  "should the app end" signal, not two that could disagree.
