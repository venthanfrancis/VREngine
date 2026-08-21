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

## 11. M3 Implementation Notes

M3 connected `Core` + `Frame` + `Platform` into the first real running
engine loop: `Runtime` and `DesktopFrameDriver`. Concrete decisions
worth recording:

- **`DesktopFrameDriver` placement**: lives in `runtime/`
  (`AREngine::Runtime::DesktopFrameDriver`), not in `Frame` or
  `Platform`. It needs both Frame's `FrameDriver` interface/types and
  Platform's `Window`/`SteadyClock`, and `Runtime` is the one module
  already wired to depend on both — adding it there required zero new
  cross-module dependency edges anywhere in the build. `Frame` and
  `Platform` remain peers at the same layer (Section 1); neither gained
  a dependency on the other.
- **Runtime ownership model**: `Runtime` owns exactly two things —
  `std::unique_ptr<Platform::Window>` and
  `std::unique_ptr<Frame::FrameDriver>` — both constructed in its
  constructor and torn down implicitly by its destructor (pure RAII, no
  explicit `Shutdown()` call needed). No singleton, no service locator,
  no global mutable state. An application constructs exactly one
  `Runtime`, on the stack, and owns it directly (`sandbox/src/main.cpp`
  does this). Member declaration order (`m_window` before
  `m_frameDriver`) is what guarantees `m_frameDriver` — which holds a
  reference to `*m_window` — is destroyed first, since C++ destroys
  members in reverse declaration order.
- **`DesktopFrameDriver` is constructed directly**, not through a
  factory function: unlike `Platform::Window` (which hides
  `WindowsWindow` behind `CreateAppWindow` specifically to keep Win32
  out of public headers), `DesktopFrameDriver` already lives in the same
  module as `Runtime` and leaks no platform-specific types, so there's
  nothing for an extra layer of indirection to hide.

  The precise architectural guarantee here is narrower than "swapping in
  `XRFrameDriver` is a one-line change": **`Run()`'s loop should not
  require structural changes**, because it only ever calls through the
  `Frame::FrameDriver` interface (`WaitForNextFrame`/`GetViews`/
  `SubmitFrame`) and has no idea which concrete implementation is
  underneath. That guarantee does not extend to everything around the
  loop — XR session initialization, OpenXR instance/session ownership,
  window/surface requirements, and other XR-specific setup may well
  need real changes outside the loop itself (e.g. in how and when
  `Runtime` is constructed, or what it's constructed with) once M9
  actually builds `XRFrameDriver`. Which of those changes are needed
  won't be known for certain until M9.
- **Main loop order**: `PollEvents()` then `ShouldClose()` is checked
  *before* any frame work starts, every iteration. Reasoning: if a new
  frame were started first and `ShouldClose()` only checked afterward, a
  close request would still cost one more full frame of work before the
  loop noticed — pointless today since nothing renders yet, but the
  wrong habit to build. Checking immediately after processing messages
  means a close request is acted on the moment it's known.
- **Authoritative close signal**: `Window::ShouldClose()` is the only
  thing that ends `Runtime::Run()`'s loop. `WindowCloseEvent` is also
  wired up (via `Window::SetEventCallback`) but used purely for logging
  — it never drives control flow. This keeps exactly one source of truth
  for "should the app end," matching the same principle already applied
  to `WM_CLOSE`/`WM_QUIT` in Section 10.
- **No FrameDriver redesign needed.** The M1 interface (`WaitForNextFrame`
  → `FrameTiming`, `GetViews` → `vector<ViewInfo>`, `SubmitFrame`) fit
  M3's needs exactly as designed; nothing was changed. `DesktopFrameDriver`
  implements `WaitForNextFrame` by ticking a `Platform::SteadyClock` (no
  actual "wait," since there's no vsync/present to synchronize with
  yet), `GetViews` by returning one default-constructed `ViewInfo` (no
  camera system exists yet — Scene, M5+), and `SubmitFrame` as a no-op
  (no renderer exists yet — see Section 4, "RHI Presentation").
- **FPS logging, not per-frame**: `Runtime::Run()` accumulates delta
  time and frame count, logging an FPS line only once per elapsed
  second, to avoid flooding the console — the loop currently runs
  unthrottled (no frame limiting; not implemented per design, since
  there's no renderer to pace against yet), so per-frame logging would
  be well over a million lines a second.

  This unthrottled behavior will **not** be resolved by M4: M4 adds a
  "null" `Rendering` backend that only logs draw calls (see M4's roadmap
  entry) — it provides no real GPU presentation and no vsync, so it
  gives `DesktopFrameDriver::WaitForNextFrame()` nothing to actually wait
  on. The loop is expected to remain effectively unthrottled until a
  real graphics/presentation path exists — realistically not before the
  Vulkan milestone (M8), and only once that backend's `Present()`
  equivalent (deliberately undesigned so far — see Section 4, "RHI
  Presentation") is implemented. No frame limiting is being added ahead
  of that.

## 12. M4 Implementation Notes

M4 implemented `Rendering`'s public API and its `NullRenderDevice`
backend, and wired a minimal, clearly-temporary draw into `Runtime`.
Concrete decisions worth recording:

- **Minimal Rendering API**: five things — two resource kinds
  (`BufferHandle`/`BufferDesc`, `TextureHandle`/`TextureDesc`), one
  `DrawCommand`, and the `RenderDevice` interface itself
  (`CreateBuffer`/`DestroyBuffer`, `CreateTexture`/`DestroyTexture`,
  `BeginRendering`/`SubmitDraw`/`EndRendering`). Nothing else. No render
  graph, material system, descriptor framework, bindless system,
  multi-threaded command recording, or shader reflection — M4 exists
  only to prove the seam between engine code and a future graphics
  backend, not to design the final renderer.
- **Pipelines and shaders are deferred entirely, not stubbed.** No
  `PipelineHandle`, no `PipelineDesc`, no `CreatePipeline`, and
  `DrawCommand` has no pipeline reference. A placeholder `PipelineDesc`
  with no real fields would just be guessing at what a real backend
  needs before any real backend exists to ask. This is deferred until
  Vulkan (M8) reveals actual shader-stage and vertex-layout
  requirements — documenting the deferral instead of guessing, per M4's
  own design guidance.
- **Texture upload/content is out of scope.** `TextureDesc` describes
  width/height/format only; `NullRenderDevice::CreateTexture` validates
  and stores the descriptor but never touches pixel data. Loading image
  content belongs to `Assets` (M6+), not `Rendering`.
- **Handle strategy**: `BufferHandle`/`TextureHandle` are each just a
  `std::uint32_t id` (0 = invalid) with an `IsValid()` check — no
  generational reuse counters, no exposed backend memory objects, two
  separate plain structs rather than one templated `Handle<Tag>` (kept
  as simple and readable as the codebase's existing style, e.g. `Mat4`'s
  explicit `At`/`Set` over an `operator()` template). `NullRenderDevice`
  maps ids to descriptors in `std::unordered_map`; a future Vulkan
  backend would map the same ids to `VkBuffer`/`VkImage` it owns
  privately — the id itself carries no backend meaning.
- **`RenderDevice` knows nothing about how a frame is paced or
  presented.** `BeginRendering`/`EndRendering` only bracket one frame's
  worth of draw submissions (so a backend can group/count them, and so
  `Runtime` has one clear place to plug rendering into its loop) — they
  are explicitly not `Present()` and carry no swapchain/vsync meaning.
  This is the same boundary from Section 4, restated because M4 is the
  first milestone where it's actually exercised end to end: `Frame`
  (frame lifecycle/timing) and `Rendering` (GPU operations) remain
  separate modules with no dependency between them in either direction.
- **`SubmitDraw` returns `bool`, `CreateBuffer`/`CreateTexture` assert.**
  Two different failure philosophies, deliberately: a zero-size buffer
  or zero-dimension texture passed to `Create*` is a caller bug (an
  `AR_ASSERT_MSG`, consistent with M1/M2's established use of Core's
  assertion facility for genuine invariant violations), whereas a
  `DrawCommand` referencing an unknown or stale handle is a plausible,
  checkable runtime outcome — `SubmitDraw` rejects it (logs a warning,
  returns `false`, submits nothing) rather than crashing, so both
  production code and tests can handle it predictably.
- **`Destroy*` is a no-op on invalid/unknown/already-destroyed
  handles** — the same "predictable, not an error" philosophy as
  `delete nullptr`. No garbage collection, no deferred GPU destruction;
  real Vulkan resource-lifetime requirements (GPU work still in flight
  when destroy is requested, etc.) are a Vulkan-milestone (M8) problem,
  not an M4 one.
- **`NullRenderDevice`'s diagnostic methods
  (`GetLiveBufferCount`/`GetLiveTextureCount`/`GetTotalDrawCount`/
  `GetLastFrameDrawCount`) are on the concrete class, not on
  `RenderDevice`.** Tests construct a `NullRenderDevice` directly to
  reach them; `Runtime` only ever holds the abstract `RenderDevice`
  interface (via `CreateNullRenderDevice()`) and never sees them. This
  keeps the generic interface free of Null-backend-specific concerns,
  the same pattern used for `WindowsWindow`'s Win32-only surface area.
- **Runtime integration required no architectural changes** — it
  followed the exact ownership pattern `DesktopFrameDriver` established
  in M3: a `std::unique_ptr<Rendering::RenderDevice>` member,
  constructed via `CreateNullRenderDevice()` (never naming
  `NullRenderDevice` itself), destroyed automatically in reverse
  declaration order. `Runtime::Run()` now does one hard-coded, clearly
  temporary `BeginRendering`/`SubmitDraw`/`EndRendering` per frame
  against a placeholder vertex buffer created at startup, purely to
  prove `CreateBuffer` → `SubmitDraw` works end to end — both are
  removed once `Scene` (M5) provides real geometry.

## 13. M5 Implementation Notes

M5 implemented `Scene`: entity identity, `Transform`, parent/child
hierarchy, and world-matrix composition, plus the minimal additional
`Core::Math` needed to support it. Concrete decisions worth recording:

- **`EntityId` strategy**: a bare `std::uint64_t` with 0 as the invalid
  sentinel, handed out by a monotonically increasing counter that never
  resets and never reuses a value — the same minimal-handle philosophy
  as `Rendering::BufferHandle`/`TextureHandle`. No generation counter.
  This is deliberately weaker than a full generational-index scheme (it
  can't distinguish "this exact id was reused by something else" —
  impossible here, since ids are never reused — from "this id was never
  reused"), but nothing in M5 needs that distinction. A future ECS
  might introduce generational indices if dense storage/reuse ever
  becomes necessary for performance; not needed while `Scene` stores
  entities in a `std::unordered_map`.
- **`Scene` ownership**: no singleton, no global "current scene." An
  application owns a `Scene` instance (or several) explicitly, the same
  ownership philosophy used everywhere else in the engine so far. **M5
  does not add `Scene` to `Runtime`.** Nothing in M5's validation needs
  it — `Scene`'s correctness (entity/transform/hierarchy/world-matrix
  behavior) is fully provable through headless automated tests alone,
  and there is still nothing that would *consume* a `Runtime`-owned
  `Scene` (no `SceneRenderer` bridging `Scene` and `Rendering` exists
  yet). Adding an unused `std::unique_ptr<Scene::Scene>` member now
  would be scope creep with no payoff. `Runtime` gains a `Scene` once
  something is actually built to read one — most plausibly whichever
  milestone introduces that `Scene`→`Rendering` bridge.
- **Transform representation**: `position`/`rotation`/`scale` as
  `Vec3`/`Quaternion`/`Vec3`, defaulting to `(0,0,0)` /
  identity / `(1,1,1)` — exactly the M5 requirements, nothing more (no
  Euler angles; `Quaternion::FromAxisAngle` is enough to construct
  meaningful rotations for tests without them).
- **Local vs. world transform**: every entity stores only a LOCAL
  `Transform` (relative to its parent, or to the world if it has no
  parent). There is no stored "world transform" field anywhere — world
  matrices are always *derived*, via `Scene::GetWorldMatrix`, by
  walking up the parent chain and composing each ancestor's local
  matrix: `world(E) = local(root) * ... * local(parent) * local(E)`.
  Not cached; recomputed on every call. This keeps invalidation logic
  (the hard part of caching) out of scope for M5, at the cost of
  repeated work if something calls `GetWorldMatrix` in a hot loop — an
  acceptable trade for a milestone whose job is proving the *shape* of
  this API, not its performance.
- **TRS composition order**: `TRS = Translation * Rotation * Scale`,
  applied to a column-vector point `p` as `TRS * p` — meaning `p` is
  scaled first, then rotated, then translated. This is `Mat4::TRS`
  (Core, not Scene-specific), and `Transform::ToMatrix()` just calls it.
  Proven with tests, not assumed: `core_tests.cpp`'s
  `TestMat4TransformFactories` checks translate-then-scale and
  scale-then-translate give *different*, specific results for the same
  input point.
- **Hierarchy rules**: `SetParent(child, parent)` rejects (returns
  `false`, makes no change) three cases — either id invalid/unknown,
  `child == parent`, or `parent` already a descendant of `child` (which
  would close a cycle). Cycle detection walks `child`'s subtree looking
  for `parent`, bounded by `entity count + 1` steps so it can't loop
  forever even in principle. `Scene::GetWorldMatrix`'s parent-chain walk
  carries the same bound, purely as defense in depth — `SetParent` is
  what's actually responsible for cycles never existing in the first
  place.
- **Reparenting behavior**: `SetParent` changes only the hierarchy
  edge. The child's LOCAL transform is left untouched, so its WORLD
  transform can (and usually will) change as a side effect — this is
  the simpler of the two possible designs (the alternative, "keep world
  transform," would require solving for a new local transform that
  reproduces the old world transform under the new parent — not
  implemented; deferred, per the M5 brief, to if/when something
  actually needs it).
- **Entity destruction**: destroying an entity recursively destroys all
  of its descendants first (so no child is ever left pointing at a
  destroyed parent), then detaches itself from its own parent's
  children list. `DestroyEntity` on an invalid/unknown/already-destroyed
  id is a no-op — the same "predictable, not an error" philosophy
  `Rendering::NullRenderDevice::DestroyBuffer` already established.
- **Two failure philosophies for invalid `EntityId`s, deliberately
  different, matching precedent from M4**: `SetParent`/`ClearParent`/
  `DestroyEntity` (commands that mutate state using an id that could
  plausibly be stale) reject predictably — return `false` or silently
  no-op, never crash. `GetTransform`/`GetName`/`GetParent`/
  `GetChildren`/`GetWorldMatrix` (queries that must return real data)
  instead assert (`AR_ASSERT_MSG`) on an invalid/unknown id, since there
  is no safe fallback value that wouldn't risk silently masking a bug —
  `IsValid()` is the sanctioned way to check first. This mirrors exactly
  the split M4 made between `SubmitDraw` (predictable rejection) and
  `CreateBuffer`'s descriptor validation (assert).
- **Why `Scene` does not depend on `Rendering`**: `Scene` represents
  world *data* — what exists, where it is, how it's related — and has
  no notion of GPU resources, draw calls, or even that `Rendering`
  exists. This is the same seam established in Section 4/12: a future,
  higher-level system (not built yet) will read `Scene` data and
  translate it into `Rendering` submissions, so neither module needs to
  know about the other. Confirmed structurally, not just by convention:
  `engine/scene/CMakeLists.txt` links only `AREngine::Core`.
- **What's deferred to a future ECS/component system**: everything an
  ECS would normally provide beyond identity + transform + hierarchy —
  components, archetypes/chunks, a component registry, reflection,
  serialization, scene files, prefabs, a renderable/camera/light
  component, multithreaded updates. M5 is deliberately just enough to
  prove `Scene` can represent a small 3D world; a real
  component/gameplay model is a later, larger design decision.
- **New `Core::Math` added, and why each piece was genuinely needed**:
  `Quaternion::FromAxisAngle` (the only new way to construct a
  meaningful, testable rotation); `Mat4::Translation`/`Scale`/`Rotation`
  (the three pieces `TRS` composes); `Mat4::TRS` (the composition
  itself); free function `TransformPoint(Mat4, Vec3)` (turns "where did
  this point end up" into one readable call instead of manual matrix
  indexing in every test). **Not added, because M5 never needed them**:
  quaternion-quaternion multiplication and quaternion-vector rotation —
  all of `Scene`'s composition happens through `Mat4`, never by
  combining quaternions directly, so neither was genuinely required (see
  the note in `Quaternion.hpp`). All new `Mat4`/`Quaternion` operations
  have direct Core-level tests in `core_tests.cpp`, not just indirect
  coverage through `Scene`'s tests.

No architectural issues were discovered. One implementation-only note:
`Mat4::TRS` is declared inside the `Mat4` struct but *defined* after the
free `operator*(Mat4, Mat4)`, because its body calls that operator — a
free function outside the class must already be declared at the point
of use (unlike a class member, which benefits from "complete-class
context" and can refer to members declared later in the same class).
This is a compile-order detail, not a design change.

## 14. M6 Implementation Notes

M6 implemented `Assets`: `AssetId`, `TextAsset`/`BinaryAsset`, and
`AssetManager` (an asset root, path resolution/normalization, caching,
and predictable failure handling). Concrete decisions worth recording:

- **`AssetId` semantics**: a bare `std::uint64_t`, 0 = invalid, handed
  out by a per-instance monotonically increasing counter — the same
  pattern as `Scene::EntityId`. Meaningful only within the
  `AssetManager` instance that issued it; two different managers both
  start counting from 1, so ids from different managers must never be
  compared or mixed. No UUIDs, no cross-instance uniqueness — nothing in
  M6 needs either.
- **Why `Assets` does not depend on `Platform`**: `std::filesystem` and
  `std::ifstream` already provide everything M6 needs — path
  resolution, normalization, and file reading — as portable standard
  C++. There is no OS-specific behavior for `Platform` to abstract away
  here (unlike `Window`, which genuinely needs Win32). Confirmed
  structurally: `engine/assets/CMakeLists.txt` links only
  `AREngine::Core` (the `Platform` dependency inherited from the M0
  scaffold was removed).
- **Asset root model**: `AssetManager` owns exactly one root directory,
  canonicalized (and required to already exist — `AR_ASSERT_MSG`s
  otherwise, a setup-time caller bug, not a runtime condition) at
  construction. Every `LoadText`/`LoadBinary` call takes a path relative
  to that root; there is no way to address a file outside it except by
  escaping (which is rejected — see below). Example: root
  `C:/Projects/VREngine/TestAssets`, asset path `textures/checker.txt`,
  resolved path `C:/Projects/VREngine/TestAssets/textures/checker.txt`.
- **Normalization strategy**: a candidate path is resolved as
  `weakly_canonical(root / relativePath)` — `weakly_canonical` (not
  `canonical`) specifically because the target file may not exist yet
  (that's exactly what the missing-file test exercises), but its
  directory structure should still resolve `.`/`..` lexically. The
  resulting absolute path's `generic_string()` (forward slashes,
  platform-independent) is the cache key, so `textures/test.txt` and
  `textures/./test.txt` hit the same cache entry. **Documented
  limitation**: cache keys are case-sensitive strings. On a
  case-insensitive filesystem (Windows' default), `Test.txt` and
  `test.txt` may be the same file on disk but are treated as two
  independent cache entries — no cross-platform case-folding is
  implemented for M6.
- **Root traversal handling**: after resolving and normalizing, the
  candidate path is checked against the root via
  `std::filesystem::relative(candidate, root)` — if the result's first
  path component is `".."`, the candidate falls outside the root and is
  rejected. An absolute input path is rejected outright before any of
  this, even one that happens to resolve back inside the root (e.g.
  `root / "hello.txt"` passed directly) — asset paths are relative by
  construction, not "anything that resolves somewhere valid." This is a
  containment check, not a security sandbox: it stops accidental/naive
  `../` traversal, not a determined, deliberately hostile caller with
  other means of reading files. Tested directly (`../outside.txt`, a
  deeper `subdir/../../outside.txt`, and an absolute path).
- **Cache key strategy / same-path-different-type behavior**: text and
  binary assets are cached in **entirely separate maps**
  (`m_textPathToId`/`m_textAssets` vs. `m_binaryPathToId`/
  `m_binaryAssets`), both keyed by the same normalized-path string.
  `LoadText("data.bin")` and `LoadBinary("data.bin")` therefore succeed
  independently, each producing its own distinct `AssetId` — this is
  the "cache separately by {path, type}" option from the M6 brief,
  chosen because it's simpler than "reject type mismatch" (no need to
  track or check what type a path was previously loaded as) and there's
  no genuine conflict to detect: loading the same file's bytes two
  different ways isn't a contradiction.
- **Ownership**: `AssetManager` owns every loaded asset for its own
  lifetime, in `std::unordered_map<AssetId, TextAsset/BinaryAsset>`.
  Callers hold the lightweight `AssetId` and query `GetText`/`GetBinary`
  for a `const&` into manager-owned storage — the same "manager owns
  storage, caller holds a cheap id" model `Scene` already established
  for `EntityId`/`Transform`. No `shared_ptr`, no generational handles;
  neither solves a problem M6 actually has.
- **Failure model, and the same two-philosophy split from M4/M5**:
  `LoadText`/`LoadBinary` (read real files that could plausibly be
  missing, unreadable, or path-escaping) return `std::optional<AssetId>`
  — `std::nullopt` on failure, logged via `AR_LOG_WARNING`, never a
  crash. `GetText`/`GetBinary` (queries on an id the caller should
  already know is valid, typically one they just received from a
  successful `Load*`) instead `AR_ASSERT_MSG` on an unknown id — there's
  no safe fallback `TextAsset`/`BinaryAsset` to return that wouldn't risk
  masking a bug; `IsValid()` is the sanctioned way to check first.
  Exactly the same split `Scene` uses for commands vs. queries.
- **No common `Asset` base type.** `TextAsset` and `BinaryAsset` are
  independent, non-polymorphic structs. `LoadText`/`GetText` and
  `LoadBinary`/`GetBinary` are separate, statically-typed entry points —
  callers always know at compile time which kind they're asking for, so
  there is never a need for runtime dispatch across asset kinds. A
  shared base class (with or without virtual functions) would add a
  layer of indirection with no caller benefit, and risks inviting
  exactly the RTTI-heavy/reflection-heavy design the M6 brief warns
  against. Real future content types (`MeshAsset`, `TextureAsset`) are
  expected to build on top of `TextAsset`/`BinaryAsset` (e.g. "parse
  this `BinaryAsset`'s bytes into mesh data"), as their own independent
  structs, following the same pattern — not by retrofitting a base
  class in today.
- **Test fixtures**: `tests/data/assets/` holds three tiny files
  (`hello.txt`, `second.txt`, `sample.bin`, all a handful of bytes,
  none copyrighted/downloaded — `sample.bin` was generated locally via
  a one-line PowerShell `WriteAllBytes` call). `tests/CMakeLists.txt`
  passes their directory to the test executable via
  `AR_TEST_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/data/assets"` — a
  path CMake computes from the current checkout, not a hard-coded
  machine-specific path, so this works on any machine that clones and
  builds the repo.

No architectural issues were discovered.
