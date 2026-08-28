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

## 15. M7 Implementation Notes

M7 implemented `Input`: `InputSystem` (raw key/mouse state, an
action-mapping layer) fed by new generic events from `Platform`, wired
into `Runtime`'s existing loop. Concrete decisions worth recording:

- **`KeyCode`/`MouseButton` ownership**: both live in `Core`
  (`Core/KeyCode.hpp`, `Core/MouseButton.hpp`), not in `Input` or
  `Platform`. `Platform` needs them to construct its input events;
  `Input` needs them to represent key/button state — putting them in
  either module would force the other to depend on it, so they live in
  the one place both can reach without depending on each other, the
  same reasoning `Core::Event` already established for event base
  types.
- **Why `Input` depends on `Platform`**: this was the established
  relationship since M0 (`engine/input/CMakeLists.txt` already linked
  `AREngine::Platform`), and M7 puts it to genuine use —
  `InputSystem::OnEvent` recognizes Platform's concrete event types
  (`KeyPressedEvent`, `MouseMovedEvent`, ...) via `dynamic_cast`. This
  is a downward dependency (`Input` sits above `Platform` in the layer
  diagram, Section 1), not a violation of any layering rule. Crucially,
  `InputSystem` never reaches into a `Window` or touches Win32 itself —
  it only ever receives events handed to it through `OnEvent()`.
  `Runtime` is what actually calls `Window::PollEvents()` and forwards
  each resulting event into `InputSystem::OnEvent()` (from the same
  callback that already logs `WindowCloseEvent`/`WindowResizeEvent`) —
  matching the brief's preferred shape: Platform receives OS input →
  Runtime forwards it → Input updates its state.
- **Win32 → generic input translation boundary**: confined entirely to
  `WindowsWindow.cpp`'s `TranslateVirtualKey` and the `WM_LBUTTONDOWN`/
  etc. handlers in `HandleMessage`. No `Windows.h` type or virtual-key
  constant ever crosses out of `engine/platform/src/windows/` — `Input`
  and everything above it only ever see `Core::KeyCode`/
  `Core::MouseButton`. Left/right Shift and Ctrl need special handling
  here: Win32 reports either Shift key as the generic `VK_SHIFT` (same
  for `VK_CONTROL`) in `WM_KEYDOWN`/`UP`'s `wParam` — telling left from
  right requires pulling the hardware scan code out of `lParam` and
  asking `MapVirtualKeyW` to expand it to the specific extended
  virtual-key, which `TranslateVirtualKey` does uniformly for both
  keys.
- **Held vs. Pressed vs. Released**: `IsKeyDown`/`IsMouseButtonDown`
  ("held") are true for every frame the key/button stays down,
  including the frame it was pressed and the frame before release.
  `WasKeyPressed`/`WasKeyReleased`/their mouse equivalents ("this
  frame's transition") are true for exactly one frame — the one in
  which the up→down or down→up transition was observed — never longer.
- **Frame lifecycle for transient state**: `InputSystem::BeginFrame()`
  clears every key/button's `wasPressed`/`wasReleased` flags (but not
  `isDown`, which persists) and snapshots the current mouse position as
  this frame's delta baseline. `Runtime::Run()` calls it as the very
  first thing in the loop body, *before* `Window::PollEvents()` —
  reversing that order would mean a flag this frame's own event just
  set gets wiped out again immediately, and nothing would ever observe
  it. With the correct order, `Pressed`/`Released` set by events
  processed in this iteration remain observable for the rest of the
  frame's body (the M7 demo logging, and everything after it), and are
  cleared only at the *next* iteration's `BeginFrame()`.
- **Key repeat behavior**: `Platform::KeyPressedEvent` fires on every
  `WM_KEYDOWN`, OS repeats included — Platform does not filter them.
  `InputSystem::SetKeyDown` is what enforces "Pressed means an up→down
  transition only": a `KeyPressedEvent` for a key that's already marked
  `isDown` is a no-op (the key simply stays Down; `wasPressed` is not
  set again). This was a deliberate choice over filtering in Platform:
  `InputSystem` already has to track per-key held state to compute
  `WasKeyPressed` at all, so enforcing the transition there avoids
  duplicating that bookkeeping in `Platform` too, and keeps the
  behavior directly testable with synthetic events (no real repeating
  Win32 message needed) — see `TestRepeatedKeyDownDoesNotRetriggerPressed`
  in `tests/input_tests.cpp`, and confirmed again manually by holding
  Space through the real Win32 pipeline (three synthetic `WM_KEYDOWN`
  messages with no release in between produced exactly one "Space
  pressed" log line).
- **Focus-loss behavior**: `Platform::WindowFocusLostEvent` (new, fires
  on `WM_KILLFOCUS`) causes `InputSystem::HandleFocusLost` to mark every
  currently-held key and mouse button as released — `isDown = false`
  *and* `wasReleased = true` (not just silently cleared), so code
  watching for release transitions still observes one instead of a key
  quietly vanishing from "held" with no event. Why this matters: the
  matching key-up for anything held when focus is lost may never arrive
  at this window (the user could release it while some other window has
  focus), so without this, a key could stay stuck "held" forever.
- **Mouse coordinate convention**: `Platform::MouseMovedEvent` and
  `InputSystem::GetMousePosition` use client-area coordinates — origin
  top-left, +x right, +y down. This is a 2D desktop-window convention,
  deliberately separate from (and not to be confused with) the engine's
  3D world convention in `docs/WORLD_CONVENTIONS.md` (+Y up). Both
  conventions are documented explicitly, in both places, specifically so
  neither gets silently assumed to be the other.
- **Mouse first-movement rule**: the very first `MouseMovedEvent`
  `InputSystem` ever receives establishes the position baseline and
  reports a delta of exactly `(0,0)` for the frame it arrives in,
  rather than a large, meaningless jump from the default `(0,0)`
  position. After that, `GetMouseDelta()` is the *net* movement since
  the start of the current frame (`BeginFrame`'s position snapshot),
  not the delta of the single most recent `WM_MOUSEMOVE` — several of
  those can arrive per `PollEvents()` call, and only the net result
  across the whole frame is meaningful.
- **Action mapping / multiple-binding semantics**: an action is a name
  (`std::string`) with zero or more key/mouse-button bindings, added via
  `BindActionKey`/`BindActionMouseButton`. `IsActionDown`/
  `WasActionPressed`/`WasActionReleased` are true if **any** bound
  input satisfies the query — implemented as a linear scan over that
  action's bindings, calling the same `IsKeyDown`/`WasKeyPressed`/etc.
  used for raw queries. An action name that was never registered (or
  has no bindings) behaves as "always false," never an error — no
  `IsValid()`-style check is needed before querying an action, unlike
  `EntityId`/`AssetId` queries elsewhere in the engine, because there's
  a sensible, unambiguous "no bindings means nothing can be down" answer
  here that doesn't risk masking a bug.
- **What's deferred for XR/analog input**: only digital bindings exist
  (a key or button is either down or not). No analog axes, controller
  sticks, gesture values, XR poses, composite vectors, or dead zones.
  The action system is not designed in a way that rules these out later
  — `ActionBindings` is a private implementation detail that could grow
  an `std::vector<AnalogBinding>` alongside its digital lists without
  changing `IsActionDown`'s public shape — but nothing analog is built
  now, since M7 has no analog input source to bind. A future hand-pinch
  or XR-controller-trigger binding would plug into the same
  `IsActionDown("Select")`-style query gameplay already uses; only the
  binding *source* changes, not the query API — this is the concrete
  version of the "gameplay cares about Select, not which device"
  separation the M7 brief opens with.
- **A real bug, found and fixed via manual testing, not the automated
  suite**: `Runtime`'s original member order declared `m_inputSystem`
  *after* `m_window`, meaning `m_inputSystem` was destroyed *before*
  `m_window` (C++ destroys members in reverse declaration order). Win32
  can synchronously fire one last `WM_KILLFOCUS` *during* `DestroyWindow`
  (called from `~WindowsWindow`, reached via `~m_window`'s implicit
  destruction) — which the event callback forwarded straight into
  `m_inputSystem.OnEvent()`, calling a method on an already-destroyed
  object. This produced a genuine, reproducible access-violation crash
  on every clean window close, confirmed via the Windows Application
  Error event log (exception code `0xC0000005`) — the crash happened
  *after* `WM_CLOSE`/`Runtime::Run()` had already returned normally, so
  nothing in the headless `arengine_*_tests` suite could have caught it
  (all of it runs without ever destroying a real `Window` and
  `InputSystem` together in the same object graph). Fixed by declaring
  `m_inputSystem` *first* in `Runtime` (see the ordering comment in
  `Runtime.hpp`), so it is destroyed *last* — guaranteed to still be
  alive for anything the window's own teardown generates. Re-verified
  manually afterward: the same close sequence that previously crashed
  now shuts down cleanly every time.

## 16. M8A Implementation Notes

M8A introduced Vulkan for the first time: instance creation, validation,
physical-device selection, and a logical device + graphics queue —
bring-up only, no rendering. Concrete decisions worth recording:

- **Vulkan SDK discovery**: `engine/rendering/CMakeLists.txt` calls
  `find_package(Vulkan REQUIRED)`, CMake's standard Vulkan discovery
  (via the `VULKAN_SDK` environment variable the LunarG SDK installer
  sets, among other standard search paths). No headers are vendored
  into the repository, no machine-specific `C:\VulkanSDK\<version>` path
  is hard-coded anywhere. On this machine, only the GPU driver's Vulkan
  *runtime* (loader + `vulkaninfo`) was present initially — the SDK
  itself (headers, the `Vulkan::Vulkan` CMake package, validation
  layers) had to be installed separately before `find_package(Vulkan)`
  could succeed.
- **Chosen Vulkan API target: 1.2.** Released 2020, supported by
  essentially all Vulkan-capable hardware still in use — chosen over
  requiring newer optional features (dynamic rendering, ray tracing,
  mesh shaders) that nothing in this engine needs yet and that M8A's
  own "do not implement" list explicitly excludes. `VkApplicationInfo::
  apiVersion` requests 1.2; `SelectPhysicalDevice` additionally rejects
  any physical device whose own reported `apiVersion` is below that —
  querying capability rather than assuming every device supports it.
  This target only sets a *floor*: on this machine the selected device
  reports 1.4.325 (its driver's actual max supported version), well
  above the 1.2 minimum, confirming the floor doesn't artificially
  understate what's actually available.
- **Instance ownership**: `Rendering::Vulkan::VulkanInstance` (private,
  `engine/rendering/src/vulkan/`) owns exactly one `VkInstance` and, in
  debug builds when available, one `VkDebugUtilsMessengerEXT`. RAII:
  constructor creates, destructor destroys, non-copyable and
  non-movable — there is never any ambiguity about which object is
  responsible for a given `VkInstance`. No surface extensions are
  requested; M8A has no window/surface to present to, so enabling them
  now would be speculative.
- **Validation behavior**: the Khronos validation layer
  (`VK_LAYER_KHRONOS_validation`) is only *requested* in debug builds
  (the same `#if !defined(NDEBUG)` gate `Core::Assert` already
  established), and even then only actually *enabled* if
  `vkEnumerateInstanceLayerProperties` reports it present — if not, a
  warning is logged (via `AR_LOG_WARNING`, naming exactly which layer
  was missing) and bring-up continues without it, rather than failing.
  Release builds never request it at all. When enabled, a
  `VK_EXT_debug_utils` messenger routes every validation message through
  AREngine's own logging (severity mapped to `AR_LOG_ERROR`/
  `AR_LOG_WARNING`/`AR_LOG_INFO`) — confirmed working end to end by the
  manual demo, which logged the validation layer's own layer-stack setup
  diagnostics as `[INFO]` lines with zero errors or warnings.
- **Physical-device selection**: `SelectPhysicalDevice` enumerates every
  device, discards any below the API version floor or lacking a
  graphics-capable queue family (`FindGraphicsQueueFamily`), and picks
  the best survivor by `RankPhysicalDeviceType` — discrete GPU ranked
  best (0), integrated next (1), everything else (virtual GPU, CPU,
  unknown) tied for last (2). No ray tracing, no mesh shaders, no
  feature negotiation of any kind — exactly the minimal policy M8A
  asked for. Asserts if nothing qualifies at all (there is no
  "gracefully render nothing" fallback for a bring-up demo with no
  renderer). The ranking and queue-family-selection functions take
  already-queried Vulkan structs as plain data and make no Vulkan API
  calls themselves, so both are unit-tested directly with synthetic
  data in `tests/vulkan_tests.cpp` — genuinely exercised, not just
  exercised indirectly through the hardware demo.
- **Queue selection**: only a graphics-capable queue family is found;
  M8A deliberately does not assume a present queue exists or is the
  same family, since there is no `VkSurfaceKHR` yet to query
  presentation support against. `VkPhysicalDeviceQueueFamilyProperties`
  don't answer "can this present to *this* surface" — that question
  only exists once a surface does. Finding (and possibly needing to
  fall back to a *different* family for) present support is explicitly
  deferred to whichever M8 sub-milestone adds a swapchain.
- **Device ownership**: `VulkanDevice` (also private,
  `engine/rendering/src/vulkan/`) owns one `VkDevice` and its graphics
  `VkQueue` (queues are retrieved handles, not separately destroyed —
  destroying the device is what invalidates them). Same RAII, same
  non-copyable/non-movable discipline as `VulkanInstance`. No device
  extensions are enabled and no optional features are requested —
  `VK_KHR_swapchain` in particular stays out until a swapchain
  milestone genuinely needs it, per the M8A brief.
- **Exact destruction order**, confirmed by both the type system and a
  successful real run: `VulkanDevice` and `VulkanInstance` are ordinary
  local RAII objects in `arengine_vulkan_demo`'s `main()`, `VulkanDevice`
  constructed after `VulkanInstance` — so at the end of `main()`, C++
  destroys `VulkanDevice` first (`vkDestroyDevice`), then
  `VulkanInstance` (whose own destructor destroys the debug messenger
  before the `VkInstance` itself). This matches the brief's required
  order exactly: device → debug messenger → instance. Nothing that
  depends on a `VkDevice` is ever destroyed after it; nothing that
  depends on the `VkInstance` (the messenger) is ever destroyed after
  it either.
- **`RenderDevice` relationship — Option B chosen**: `VulkanInstance`/
  `VulkanDevice`/`SelectPhysicalDevice` do **not** implement
  `Rendering::RenderDevice` and are not reachable through
  `CreateNullRenderDevice`-style public factories. They are private
  Rendering infrastructure, exercised only by
  `tests/vulkan_demo.cpp` reaching directly into
  `engine/rendering/src/vulkan/` (a deliberate, temporary exception —
  the same kind of "trusted, in-tree" access tests already have to
  concrete backend types like `NullRenderDevice`). Option A (a
  `VulkanRenderDevice` implementing the full interface with
  buffer/draw operations explicitly unsupported) was rejected: it would
  mean shipping a `RenderDevice` that mostly doesn't work, and the M8A
  brief explicitly forbids faking success — better to have no
  `VulkanRenderDevice` at all yet than a dishonest one. A real
  `VulkanRenderDevice` is added once a later M8 sub-milestone has enough
  real Vulkan capability (command buffers, memory allocation) to
  implement `CreateBuffer`/`SubmitDraw` honestly.
- **`Runtime` untouched.** `NullRenderDevice` remains Runtime's only
  backend; nothing about M8A changes what `AREngineSandbox.exe` does,
  confirmed by an unchanged manual run after this milestone's work.
  Vulkan initialization failure — a real possibility on unusual
  hardware/driver combinations — stays fully isolated in the standalone
  demo while this code is new, per the brief.
- **A build-system caveat, not a code defect, worth recording**: the
  pure-logic Vulkan tests (`tests/vulkan_tests.cpp`) call zero real
  Vulkan API functions, but they still link the whole
  `AREngine::Rendering` library, which *does* contain object code that
  calls real Vulkan functions (`VulkanInstance.cpp`, etc.) — so the
  resulting test executable still carries a load-time dependency on the
  Vulkan loader DLL, even though the test itself never exercises it.
  Splitting the pure-logic helpers into a wholly separate library target
  (with no link dependency on the Vulkan loader at all) would remove
  this, but was judged not worth the added build-system complexity for
  M8A — the practical goal ("don't require a Vulkan-*capable GPU* for
  normal `ctest`") is still met, since these tests make no hardware-
  dependent calls; only the narrower, less likely scenario of "loader
  DLL present but zero usable devices" isn't fully decoupled. Documented
  here rather than solved, in case it becomes worth revisiting later.

No other architectural issues were discovered — the manual bring-up
succeeded on the first real run (instance, validation, device selection,
logical device, and queue all worked correctly the first time they were
exercised against real hardware).

### Making Vulkan Optional

Added immediately after M8A, before M8B, in response to a fair concern:
the initial M8A implementation made `find_package(Vulkan REQUIRED)`
unconditional, meaning a machine without the Vulkan SDK could not
configure *any* part of AREngine — including all the Null-backend,
non-Vulkan work from M0–M7. Fixed with one new top-level option:

```
option(ARENGINE_ENABLE_VULKAN "Build Vulkan support (requires the Vulkan SDK)" ON)
```

Default `ON`, since the SDK is now installed on this development
machine and that preserves M8A's existing behavior by default. When
`OFF`: `engine/rendering/CMakeLists.txt` never calls
`find_package(Vulkan)` at all, none of `src/vulkan/`'s five source
files are added to `arengine_rendering`'s sources, and no link against
`Vulkan::Vulkan` happens — `arengine_rendering` becomes exactly the
`NullRenderDevice`-only library it was before M8A existed. The same
`ARENGINE_ENABLE_VULKAN` check in `tests/CMakeLists.txt` skips
registering `arengine_vulkan_tests`/`arengine_vulkan_demo` entirely
(not "build them but skip running them" — they are never added as
CMake targets at all when Vulkan is disabled).

No dummy/stub Vulkan types were introduced for the disabled case —
there was no need to: since the `.cpp`/`.hpp` files under
`src/vulkan/` simply aren't compiled when the option is `OFF`, nothing
elsewhere in the codebase references a Vulkan type at all in that
configuration, so there is nothing to stub out.

Both configurations were verified in separate build directories:
`ARENGINE_ENABLE_VULKAN=ON` (default) — full build, `ctest` 9/9
including `VulkanTests`, `arengine_vulkan_demo` unchanged from its
original M8A run. `ARENGINE_ENABLE_VULKAN=OFF` — configure produces no
Vulkan-related output at all (confirming `find_package` was never
invoked), full build succeeds with `arengine_vulkan_tests`/
`arengine_vulkan_demo` absent from the target list entirely, and
`ctest` reports 8/8 (one fewer than the Vulkan-enabled configuration,
exactly the missing `VulkanTests`), including `RenderingTests` passing
— confirming `NullRenderDevice` needs nothing Vulkan-related to build
or run correctly.

## 17. M8B Implementation Notes

M8B connects AREngine's Platform `Window` to Vulkan and gets a real
image on screen: instance → `VkSurfaceKHR` → presentation-capable
device → swapchain → acquire → clear → present, looping until the
window closes, surviving resize and minimize along the way.
Deliberately no triangle, no shaders, no pipeline — that's M8C.

### Native Window Handle

`Window`'s public interface gained exactly one new method:
`GetNativeHandle() -> NativeWindowHandle`. `NativeWindowHandle`
(`engine/platform/include/AREngine/Platform/NativeWindowHandle.hpp`) is
a tiny, deliberate escape hatch:

```cpp
enum class NativeWindowPlatform { Windows };
struct NativeWindowHandle
{
    NativeWindowPlatform platform = NativeWindowPlatform::Windows;
    void* window = nullptr;   // HWND on Windows
    void* instance = nullptr; // HINSTANCE on Windows
};
```

`window`/`instance` are `void*`, not `HWND`/`HINSTANCE`, specifically so
this header — and everything that includes `Platform.hpp` — never pulls
in `Windows.h`. `WindowsWindow::GetNativeHandle()` does the one
`reinterpret_cast` from real `HWND`/`HINSTANCE` to `void*`; the only
other place those values get cast back is `VulkanSurface.cpp`, in its
own `Windows.h`-including translation unit. This is intentionally
narrow: nothing about a window's size, events, or close state goes
through this path — only a graphics API surface creation call needs raw
OS handles, and that's the only thing this exists for.

### Platform Dependency Isolation (M8B)

The concern going in was real: don't let the whole `Rendering` module
casually depend on `Platform`. What actually happened:

- The generic RHI (`RenderDevice`, `NullRenderDevice`, `Rendering.hpp`)
  still links nothing from `Platform` — unchanged from M0–M8A.
- `AREngine::Platform` is linked to `arengine_rendering` **PRIVATE**,
  and only inside the `if(ARENGINE_ENABLE_VULKAN)` block in
  `engine/rendering/CMakeLists.txt`. It is invisible to anything that
  merely links `AREngine::Rendering` — no public Rendering header
  mentions `Platform` or `NativeWindowHandle`.
- The dependency is used in exactly one file: `VulkanSurface.hpp/.cpp`,
  which takes a `NativeWindowHandle` and produces a `VkSurfaceKHR`.
  Nothing else in `src/vulkan/` touches `Platform` at all.

So the actual dependency graph is: `Rendering` (generic RHI) has no
Platform dependency, ever. `Rendering`'s *Vulkan backend*, when built,
privately depends on `Platform` for exactly one conversion. Turning
`ARENGINE_ENABLE_VULKAN` off removes even that.

### Instance Extensions (M8B)

`VulkanInstance` gained a constructor parameter,
`explicit VulkanInstance(bool enablePresentationExtensions = false)`,
defaulting to `false` so M8A's `arengine_vulkan_demo` is byte-for-byte
unaffected. The M8B presentation demo passes `true`. When `true`,
`VK_KHR_surface` and `VK_KHR_win32_surface` are queried via
`vkEnumerateInstanceExtensionProperties` (never assumed present) and
enabled only if both are actually reported; if either is missing, the
constructor asserts — there is no meaningful way to continue without
them once presentation was requested. `VulkanInstance.cpp` is now one
of the two Vulkan `.cpp` files (with `VulkanSurface.cpp`) that defines
`VK_USE_PLATFORM_WIN32_KHR` and includes `<Windows.h>` itself, purely to
see the `VK_KHR_WIN32_SURFACE_EXTENSION_NAME` macro — nothing Win32
leaks out of that translation unit.

### Device Extensions (M8B)

`VulkanDevice` gained a second constructor, alongside M8A's original
(unchanged) one:

```cpp
VulkanDevice(VkPhysicalDevice, std::uint32_t graphicsQueueFamilyIndex);                       // M8A
VulkanDevice(VkPhysicalDevice, const QueueFamilyIndices&, bool enableSwapchainExtension);      // M8B
```

Two constructors rather than one modified/defaulted signature, for the
same reason `SelectPhysicalDeviceForPresentation` is a separate
function from `SelectPhysicalDevice` (below): M8A's demo/tests keep
calling exactly what they always called, unchanged. The M8B constructor
requests one `VkDeviceQueueCreateInfo` per *unique* queue family
(`GetUniqueQueueFamilies` — one entry if graphics and present share a
family, two if not) and enables `VK_KHR_swapchain` when
`enableSwapchainExtension` is true. `GetPresentQueue()` returns the same
`VkQueue` as `GetGraphicsQueue()` when the families are the same
(Vulkan queues are retrieved per family+index; requesting the same
family twice would itself be invalid).

### Queue Families (M8B)

`QueueFamilyIndices { graphicsFamily, presentFamily }`
(`VulkanQueueFamilies.hpp`) never assumes the two are the same family.
`SelectPhysicalDeviceForPresentation` finds both independently
(`FindGraphicsQueueFamily` unchanged from M8A; `FindPresentQueueFamily`
calls `vkGetPhysicalDeviceSurfaceSupportKHR` per family) and rejects any
device missing either. `HasSeparatePresentQueue` and
`GetUniqueQueueFamilies` are pure logic, unit-tested directly. **On the
development machine's GPU (NVIDIA RTX 3060 Laptop), graphics and
present turned out to share the same queue family (index 0)** — the
single/dual-queue-family code paths (device queue creation,
`VK_SHARING_MODE_EXCLUSIVE` vs `CONCURRENT`) are both implemented and
unit-tested, but only the shared-family path has been exercised against
real hardware in this environment; the differing-family path is
correct per the Vulkan spec and covered by `TestGetUniqueQueueFamilies
DifferentFamilies`/`TestHasSeparatePresentQueue`, but not yet proven on
a GPU that actually splits the two.

### Surface Ownership (M8B)

`VulkanSurface` (`engine/rendering/src/vulkan/VulkanSurface.hpp/.cpp`)
owns exactly one `VkSurfaceKHR`, created from a `VkInstance` and a
`NativeWindowHandle`. This is the one file in Rendering's Vulkan backend
that includes `Windows.h` for surface creation itself (see "Platform
Dependency Isolation" above) — asserts if handed a non-Windows handle,
since AREngine has no other platform's `Window` yet. `Platform` never
sees or owns a `VkSurfaceKHR`; Vulkan Rendering owns every Vulkan object
it creates, full stop. Not copyable or movable, same discipline as
every other owned Vulkan object in this backend.

### Swapchain Support Query (M8B)

`SwapchainSupportDetails { capabilities, formats, presentModes }` and
`QuerySwapchainSupport(device, surface)`
(`VulkanSwapchainSupport.hpp/.cpp`) wrap the three real Vulkan calls
(`vkGetPhysicalDeviceSurfaceCapabilitiesKHR`/`FormatsKHR`/
`PresentModesKHR`). `IsSwapchainSupportAdequate(bool hasFormats, bool
hasPresentModes)` is pure logic (deliberately takes two bools, not the
whole struct, so it's trivially unit-testable) — an empty format or
present-mode list means "not supported," per spec, not merely "use
defaults."

### Surface Format (M8B)

`ChooseSurfaceFormat` prefers `VK_FORMAT_B8G8R8A8_SRGB` +
`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` when available, falling back to
whatever the device lists first otherwise — never a hard assumption
with no fallback. On the development machine, `B8G8R8A8_SRGB` (format
value `50`) was chosen.

### Present Mode (M8B)

`ChoosePresentMode` always searches for and returns
`VK_PRESENT_MODE_FIFO_KHR` — guaranteed by the Vulkan spec to be
supported everywhere, and behaves like vsync. M8B deliberately does not
chase mailbox/immediate for lower latency; that's a later milestone's
decision once there's an actual frame-time budget to reason about.
Asserts if FIFO is somehow absent (a spec violation, not a normal
"unsupported" case to degrade from).

### Swapchain Extent (M8B)

`ChooseSwapchainExtent` uses `capabilities.currentExtent` verbatim when
the surface dictates a fixed size (`currentExtent.width !=
UINT32_MAX`); otherwise it clamps the window's **client-area**
width/height (never the outer window size — the same distinction M2's
`AdjustWindowRect` decision established) to
`[minImageExtent, maxImageExtent]`. This one function is also what the
minimize-handling fix below leans on directly (see "Resize / Swapchain
Recreation").

### Swapchain Image Count (M8B)

`ChooseSwapchainImageCount` returns `minImageCount + 1`, clamped down to
`maxImageCount` if the device reports a nonzero maximum (`0` means "no
maximum"). Not an assumption of triple buffering — just one more than
the device's stated minimum. On the development machine this resolved
to **3 images** (`minImageCount` 2, effectively unbounded maximum).

### Image Sharing Mode (M8B)

`VulkanSwapchain`'s constructor sets `VK_SHARING_MODE_CONCURRENT` (with
both unique queue family indices listed) when
`HasSeparatePresentQueue` is true, and `VK_SHARING_MODE_EXCLUSIVE`
otherwise. On the development machine (shared graphics/present family),
`EXCLUSIVE` was exercised; see "Queue Families (M8B)" for the caveat
that the `CONCURRENT` path is implemented and unit-tested but not yet
proven on real hardware in this environment.

### Image Views (M8B)

`VulkanSwapchain` creates one `VkImageView` per swapchain image
(`VK_IMAGE_VIEW_TYPE_2D`, identity component swizzle, one color
mip/layer) and destroys all of them before destroying the
`VkSwapchainKHR` itself, in its destructor. **A real validation error
was hit and fixed here**: the swapchain was originally created with
only `VK_IMAGE_USAGE_TRANSFER_DST_BIT` (all M8B actually clears with is
`vkCmdClearColorImage`, a transfer operation) — but `vkCreateImageView`
requires the underlying image to have been created with at least one of
a specific set of usage bits (sampled/storage/color-attachment/depth-
stencil-attachment/…), none of which `TRANSFER_DST` is part of. Fixed
by also requesting `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` on the
swapchain images — which M8C's render pass will need these images for
anyway, so this isn't a throwaway workaround.

### Clearing (M8B)

No render pass, no shaders. Each frame: `vkCmdPipelineBarrier` (via the
small `RecordImageLayoutTransition` helper in `VulkanImageBarrier.hpp/
.cpp`) from `VK_IMAGE_LAYOUT_UNDEFINED` to
`VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`, then `vkCmdClearColorImage`
with a fixed visible teal clear color, then a second barrier from
`TRANSFER_DST_OPTIMAL` to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`. Barrier
stages are `VK_PIPELINE_STAGE_TRANSFER_BIT` (not
`COLOR_ATTACHMENT_OUTPUT_BIT`, since this is a transfer operation, not
a render-pass attachment write), bracketed by `TOP_OF_PIPE`/
`BOTTOM_OF_PIPE` for the "don't care what came before/after" ends.

### Command Infrastructure (M8B)

One `VulkanCommandPool` (thin RAII wrapper, `VulkanCommandPool.hpp/
.cpp`) on the graphics queue family, created with
`VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`. Two primary command
buffers are allocated from it up front (one per frame-in-flight) and
reset + re-recorded every frame — no per-frame allocation, no
multithreaded recording, no transfer queue. The command pool is the
only piece of "infrastructure" that got its own RAII class; the command
buffers, semaphores, fences, and the frame-loop orchestration itself
all live in `tests/vulkan_present_demo.cpp` rather than in Rendering
proper. Deliberate: their shape will very likely change once M8C
introduces real command recording against a graphics pipeline, so
locking in a permanent class design for them now would be premature.

### Synchronization Model (M8B)

Two frames in flight (`kMaxFramesInFlight = 2`). Per frame-in-flight:
one `imageAvailable` semaphore and one `inFlight` fence
(signaled-at-creation, so the first frame doesn't stall). Per
**swapchain image** (not per frame-in-flight): one `renderFinished`
semaphore, plus the standard `imagesInFlight` fence-tracking array
(`VkFence` per image, `VK_NULL_HANDLE` = "not in use yet") that makes
the frame waiting on an image also wait for whatever earlier frame is
still using it.

**A second real validation error was hit and fixed here.** The first
version indexed `renderFinished` by frame-in-flight (2 semaphores,
reused every other frame) rather than by swapchain image (3 images).
With a 3-image swapchain and only 2 frames in flight, the presentation
engine's actual acquire order doesn't walk frame-in-flight order 1:1 —
so a `renderFinished` semaphore could still be in use by a pending
`vkQueuePresentKHR` when the next frame using that same frame-in-flight
slot tried to signal it again, tripping
`VUID-vkQueueSubmit-pSignalSemaphores-00067` ("semaphore must be
unsignaled when the signal operation is submitted"). Fixed by moving
`renderFinished` to a `std::vector<VkSemaphore>` sized to and indexed by
the *swapchain image index*, recreated alongside the swapchain whenever
image count could change. This is the standard, spec-correct pattern
(see the Vulkan swapchain-semaphore-reuse guide) — not an AREngine-
specific workaround.

Per-frame loop: wait `inFlight[currentFrame]` → `vkAcquireNextImageKHR`
signals `imageAvailable[currentFrame]` → wait on
`imagesInFlight[imageIndex]` if set → record & submit (waits
`imageAvailable[currentFrame]`, signals `renderFinished[imageIndex]`
and `inFlight[currentFrame]`) → present (waits
`renderFinished[imageIndex]`) → `currentFrame = (currentFrame + 1) %
kMaxFramesInFlight`.

### Resize / Swapchain Recreation (M8B)

No `oldSwapchain` handoff — on `VK_ERROR_OUT_OF_DATE_KHR`,
`VK_SUBOPTIMAL_KHR`, or a `WindowResizeEvent`-set flag, the demo calls
`vkDeviceWaitIdle`, destroys the whole `VulkanSwapchain` object, and
constructs a new one at the window's current client size. Simpler than
an in-place transition, and sufficient for M8B's "don't break on
resize" bar. `renderFinishedSemaphores` and `imagesInFlight` are resized
alongside (image count can change on recreation).

### Minimize Handling (M8B)

**A third real validation error was hit and fixed here** — the most
interesting one. The first version waited for `window->GetWidth() != 0
&& window->GetHeight() != 0` before recreating the swapchain, on the
theory that a minimized window reports zero size. In practice, around
the exact moment of a minimize/restore transition, AREngine's own
cached `Window` width/height could still read as the pre-minimize size
while the *surface's* live `VkSurfaceCapabilitiesKHR.currentExtent`
already (or still) reported a degenerate `{0, 0}` — because
`ChooseSwapchainExtent` takes `currentExtent` verbatim whenever the
surface provides a fixed one, entirely bypassing the window-size clamp.
The result was a real `vkCreateSwapchainKHR` validation error
(`VUID-VkSwapchainCreateInfoKHR-imageExtent-01689`, zero-sized extent).
Fixed by having the wait loop query the surface directly — the same way
the swapchain itself will — rather than trusting `Window`'s cached
size:

```cpp
while (!window->ShouldClose())
{
    const auto support = QuerySwapchainSupport(physicalDevice, surface);
    const VkExtent2D extent = ChooseSwapchainExtent(support.capabilities, window->GetWidth(), window->GetHeight());
    if (extent.width != 0 && extent.height != 0) break;
    window->PollEvents();
}
```

Verified against real hardware afterward with two resizes plus two full
minimize→restore cycles in one run: zero validation errors, swapchain
correctly recreated each time.

### Exact Destruction Order (M8B)

Confirmed by both the type system (C++ destroys locals in reverse
declaration order) and a real, clean shutdown log
(`vkDeviceWaitIdle` → manual semaphore/fence destruction → automatic
teardown): GPU idle → command pool (frees its command buffers
implicitly) → swapchain (image views, then the swapchain itself) →
device → surface → debug messenger (inside `VulkanInstance`'s own
destructor) → instance → window. Nothing Vulkan-owned outlives what it
depends on at any point in this chain.

### Frame / Rendering Boundary (M8B)

`RenderDevice::BeginRendering`/`EndRendering` were **not** touched, and
the Vulkan swapchain/presentation loop is **not** reachable through
them. `tests/vulkan_present_demo.cpp` reaches directly into
`engine/rendering/src/vulkan/`'s private headers, the same pattern
`arengine_vulkan_demo` established in M8A — a deliberate, temporary
exception for in-tree test/demo code. `NullRenderDevice` remains
Runtime's only backend; `AREngineSandbox.exe`'s behavior is unchanged by
M8B. This is intentional: forcing swapchain ownership into the generic
`RenderDevice` interface now, before M8C/M8D have proven what a real
draw call needs, would mean guessing at the interface shape rather than
discovering it.

### Runtime Integration

Unchanged. No sub-milestone of M8 up through M8B has touched
`runtime/`; `AREngineSandbox.exe` still runs against `NullRenderDevice`
exactly as it did after M7.

### Validation Results (M8B)

Both build configurations reverified after M8B: `ARENGINE_ENABLE_VULKAN=ON`
(default) — full `/W4 /WX` clean build (a stray warning was hit and
fixed during verification, but it turned out to be a pre-existing
`/EHsc` interaction from the verification method itself, not M8B code —
see below), `ctest` 9/9 including the ten new M8B pure-logic checks in
`VulkanTests`. `ARENGINE_ENABLE_VULKAN=OFF` — full build succeeds,
`ctest` 8/8 (no `VulkanTests`), confirming M8B's additions stay fully
inside the optional-Vulkan boundary established after M8A.

`arengine_vulkan_present_demo` run against real hardware (NVIDIA GeForce
RTX 3060 Laptop GPU): window opens at 1280×720, immediately shows the
fixed teal clear color (confirmed both by the validation-layer log and
a screenshot), survives two window resizes and two full minimize/
restore cycles with the swapchain correctly recreated each time
(`Swapchain recreated: WxH, N images` logged for each), and closes
cleanly via the window's close button with `vkDeviceWaitIdle` completing
and no hang. **Zero validation errors or warnings in the final run** —
the three real validation errors found during development (image-view
usage flags, semaphore-reuse indexing, minimize-transition race) were
all fixed before this run, not worked around or suppressed.

- Graphics queue family index: **0**
- Present queue family index: **0** (same family as graphics on this
  GPU — see "Queue Families (M8B)")
- Chosen surface format: **`VK_FORMAT_B8G8R8A8_SRGB`** (value `50`),
  `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`
- Chosen present mode: **`VK_PRESENT_MODE_FIFO_KHR`**
- Swapchain image count: **3**
- Frames-in-flight model: **2**, with per-image `renderFinished`
  semaphores and per-image `imagesInFlight` fence tracking (see
  "Synchronization Model (M8B)")

### What's Deferred to M8C+

No vertex/fragment shaders, no graphics pipeline, no vertex/index
buffers, no triangle, no texture, no depth buffer, no camera, no Scene
rendering, no OpenXR, no VMA, no descriptor sets, no materials, no
render graph. The swapchain-owning `VulkanSwapchain`/`VulkanSurface`
classes are reusable infrastructure a real `VulkanRenderDevice` will
need; the command-pool/sync/frame-loop code in the demo is explicitly
temporary and expected to be redesigned once M8C introduces real
command recording against a pipeline.

No other architectural issues were discovered beyond the three
validation errors already described (each found via real hardware
validation-layer output during development, and fixed before the
milestone's final run) — every other piece of M8B's design worked as
planned on the first real run.

## 18. M8C Implementation Notes

M8C renders AREngine's first real triangle: a fixed-function graphics
pipeline (vertex shader → rasterizer → fragment shader) draws 3
vertices, generated inside the vertex shader itself from
`gl_VertexIndex`, into a render pass over the swapchain image M8B was
already clearing. No vertex buffer, no mesh, no camera, no material
system — exactly the minimum needed to prove shaders + pipeline +
rasterization + fragment output work end to end.

### Shader Language / Toolchain (M8C)

GLSL compiled to SPIR-V via `glslc` (part of the Vulkan SDK) — no
strong reason to reach for anything else at this scale. Two shader
source files live at `engine/rendering/src/vulkan/shaders/triangle.vert`
and `triangle.frag`, both plain `#version 450` GLSL with no includes or
preprocessor tricks.

### SPIR-V Build Path (M8C)

`engine/rendering/CMakeLists.txt`, inside the existing
`ARENGINE_ENABLE_VULKAN` block, locates `glslc` two ways: first by
reusing `Vulkan_GLSLC_EXECUTABLE` if CMake's `FindVulkan` module already
populated it (it does, on the CMake version this project was verified
against), falling back to `find_program(... HINTS
"$ENV{VULKAN_SDK}/Bin")` for older CMake versions where `FindVulkan`
doesn't locate SDK tools. Either path is fully SDK/CMake-discoverable —
no hard-coded SDK version or install path anywhere. If neither finds
`glslc`, configuration fails immediately with a clear
`message(FATAL_ERROR ...)` rather than failing obscurely at link time.

Each `.vert`/`.frag` source gets one `add_custom_command` that invokes
`glslc <source> -o <output>.spv` into `${CMAKE_BINARY_DIR}/shaders/`
(a single config-independent location — this project uses a multi-config
generator, so this avoids duplicating identical SPIR-V per Debug/
Release). A `DEPENDS` on the GLSL source means editing a shader and
rebuilding is enough — nothing manual, matching the "do not require the
user to compile shaders themselves" requirement. All the outputs are
gathered under one `arengine_shaders` custom target, which
`arengine_rendering` depends on (`add_dependencies`) — so building the
engine (or the demo, which links the engine) always compiles current
shaders first. `VulkanGraphicsPipeline.cpp` — the one file that actually
loads them — learns where to find them via a single private compile
definition, `ARENGINE_SHADER_DIR`, set only on `arengine_rendering`
(never propagated to consumers, never appearing in a public header).

This added a small but real amount of build-system complexity (a
custom-command loop plus a custom target) — judged worth it here rather
than over-engineered, since the alternative (asking the user to run
`glslc` by hand before every build) was explicitly ruled out by the
brief, and the actual CMake involved is about 30 lines, not a shader
build framework.

### Render Pass vs. Dynamic Rendering (M8C)

**Traditional `VkRenderPass` + `VkFramebuffer`**, not Vulkan 1.3 dynamic
rendering. AREngine targets Vulkan 1.2 (`kTargetApiVersion`, set in
M8A) for broad hardware compatibility; dynamic rendering is core in 1.3
and only available earlier via the optional `VK_KHR_dynamic_rendering`
extension, which would have to be queried and could be absent on
targeted hardware. The brief was explicit: don't require an optional
feature merely to avoid writing a render pass. A render pass this small
(one color attachment, one subpass, no depth) is not meaningfully more
code than the dynamic-rendering equivalent would be, so there was no
real cost to picking the more broadly compatible option.

### Pipeline Layout (M8C)

Empty (`VkPipelineLayoutCreateInfo{}` with every count left at its
zero-initialized default): no descriptor set layouts, no push-constant
ranges. Nothing in M8C's shaders declares a `uniform`, a sampler, or a
push-constant block, so there's nothing for a layout to describe yet.
No future descriptor architecture was speculated about or partially
built.

### Viewport / Scissor Strategy (M8C)

**Dynamic** (`VK_DYNAMIC_STATE_VIEWPORT` + `VK_DYNAMIC_STATE_SCISSOR`) —
core Vulkan 1.0 state, no extension or optional feature needed at
AREngine's target API version. `VulkanGraphicsPipeline`'s
`VkPipelineViewportStateCreateInfo` only sets `viewportCount`/
`scissorCount` to 1; the actual `VkViewport`/`VkRect2D` values are
recorded fresh every frame in `vulkan_present_demo.cpp` via
`vkCmdSetViewport`/`vkCmdSetScissor`, sized from the swapchain's
*current* extent. This is precisely why the pipeline itself never needs
recreating on resize — only `VulkanFramebuffers` does (see below). The
alternative (baking a fixed viewport into the pipeline and recreating
the whole pipeline on every resize) would work too, but dynamic state
is the smaller, more obviously correct choice at this scale, so nothing
more general was built.

### Swapchain-Dependent Pipeline Resources (M8C)

Three new Vulkan-private object owners, with three different lifetimes
relative to the swapchain:

- **`VulkanRenderPass`** depends only on the swapchain's *format*
  (`VkFormat`), which is chosen once by `ChooseSurfaceFormat` and never
  changes across a resize on a given device/surface pair. **Does not
  need to be recreated.**
- **`VulkanGraphicsPipeline`** depends on render-pass compatibility
  (same format) and uses dynamic viewport/scissor, so it has no baked-in
  dependency on extent either. **Does not need to be recreated.**
- **`VulkanFramebuffers`** wraps one `VkFramebuffer` per swapchain image
  view, sized to the swapchain's *extent*. Both of those change on
  every swapchain recreation. **Must be destroyed and rebuilt every
  time**, using the same destroy-then-reconstruct policy
  `VulkanSwapchain` itself already established in M8B.

`vulkan_present_demo.cpp`'s `recreateSwapchain()` reflects exactly this:
it destroys and rebuilds `framebuffers` (before) and `swapchain`, but
never touches `renderPass` or `pipeline`. This is the "don't blindly
recreate every Vulkan object" the brief asked for, applied literally —
correctness came first (framebuffers destroyed before the image views
they reference, per the general Vulkan lifetime rule that a bound
resource must not be destroyed while something still references it),
and it turned out the minimal-correct answer was also the cheap one.

### Command Recording Sequence (M8C)

Per frame, inside the single primary command buffer already established
in M8B:

```
vkResetCommandBuffer → vkBeginCommandBuffer
    → vkCmdBeginRenderPass (attachment cleared to the same background
      color M8B used, via loadOp=CLEAR — see "Clear Color" below)
    → vkCmdSetViewport, vkCmdSetScissor (current swapchain extent)
    → vkCmdBindPipeline (the one VulkanGraphicsPipeline)
    → vkCmdDraw(3, 1, 0, 0)
    → vkCmdEndRenderPass
→ vkEndCommandBuffer → vkQueueSubmit → vkQueuePresentKHR
```

M8B's manual `VkImageMemoryBarrier` pair (`VulkanImageBarrier.hpp/.cpp`)
and its `vkCmdClearColorImage` call are gone entirely — deleted, not
left dead — because the render pass now performs both jobs itself: the
color attachment's `loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR` is the clear,
and the attachment's `initialLayout`/`finalLayout` plus one
`VkSubpassDependency` (external → subpass 0, gating on
`VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`) perform the same
`UNDEFINED → COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR` transitions the
manual barriers used to. There is exactly one clear path now, not two
competing ones. The frame's submit wait-stage changed to match:
`VK_PIPELINE_STAGE_TRANSFER_BIT` (M8B, for the old transfer-based clear)
→ `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT` (M8C, matching the
subpass dependency).

### Why gl_VertexIndex Instead Of A Vertex Buffer (M8C)

`triangle.vert` looks up its 3 output positions (and a color per
vertex, purely to make fragment interpolation visibly provable) from
two small local arrays indexed by `gl_VertexIndex` — no
`VkVertexInputBindingDescription`, no `VkVertexInputAttributeDescription`,
no `VkBuffer`, no device memory allocation at all.
`vkCmdDraw(commandBuffer, 3, 1, 0, 0)` needs nothing bound to produce
those 3 vertices. This was the brief's explicit instruction, and the
reasoning holds up: M8C's actual goal is proving the pipeline
(shaders → rasterizer → fragment output) works, and a vertex buffer is
an orthogonal concern — memory allocation, upload, binding — that would
add real complexity without helping prove the pipeline. Deferring it to
M8D keeps M8C's diff to exactly what it claims to be. Since generating a
`vec2`/`vec3` per index needs no additional pipeline state (no vertex
input state beyond the empty `VkPipelineVertexInputStateCreateInfo{}`),
this cleanly requires nothing more of `VulkanGraphicsPipeline` than the
shaders themselves already express.

### Pipeline Ownership (M8C)

`VulkanGraphicsPipeline` (owns `VkPipelineLayout` + `VkPipeline`),
`VulkanRenderPass` (owns `VkRenderPass`), and `VulkanFramebuffers` (owns
the `VkFramebuffer`s) are all private to Rendering's Vulkan backend,
under `engine/rendering/src/vulkan/`, exactly like every Vulkan object
owner since M8A. **No `PipelineHandle` or equivalent was added to the
public `Rendering` API.** M4 explicitly deferred designing a generic
pipeline abstraction until there was real Vulkan evidence to design it
from; one triangle, with no material variation, no shader permutation,
and no second pipeline to compare it against, is evidence that *a*
pipeline concept will eventually need a public shape, but not nearly
enough evidence to guess correctly at what that shape is. `VkShaderModule`
in particular never escapes `VulkanShaderModule`/`VulkanGraphicsPipeline`
— it doesn't even outlive `VulkanGraphicsPipeline`'s constructor, since
nothing about a `VkPipeline` needs its source shader modules once
`vkCreateGraphicsPipelines` returns.

### Clear Color

Unchanged from M8B: the same fixed teal (`kClearColor` in
`vulkan_present_demo.cpp`) is still what the background clears to —
only *how* it's applied changed (render pass `loadOp`, not
`vkCmdClearColorImage`; see "Command Recording Sequence" above).

### Exact Destruction Order (M8C addendum)

Extends M8B's order exactly at one point: **framebuffers → pipeline →
render pass → swapchain**, inserted between "command/sync resources"
and "device" in M8B's original chain (GPU idle → command/sync resources
→ **framebuffers → pipeline → render pass** → image views/swapchain →
device → surface → debug messenger → instance). Confirmed by C++ local
destruction order in `vulkan_present_demo.cpp` (`swapchain`,
`renderPass`, `pipeline`, `framebuffers` are declared in that order, so
they're destroyed in the reverse) and by a clean real shutdown with no
validation errors.

### NullRenderDevice / Runtime

Both untouched, exactly as the brief required.
`engine/rendering/src/NullRenderDevice.cpp` was not modified; its tests
(`RenderingTests`) still pass unchanged. `AREngineSandbox.exe` still
runs on `NullRenderDevice` — no Vulkan pipeline concept was forced into
it. `arengine_vulkan_present_demo` remains the only place M8C's new
types are exercised.

### Validation Results (M8C)

`ARENGINE_ENABLE_VULKAN=ON` (default): full `/W4 /WX` clean build
(including shader compilation), `ctest` 9/9. `ARENGINE_ENABLE_VULKAN=OFF`:
configuration performs no `glslc` lookup and no shader compilation at
all, full build succeeds with no Vulkan/shader targets present, `ctest`
8/8 (no `VulkanTests`).

`arengine_vulkan_present_demo` run against real hardware (NVIDIA GeForce
RTX 3060 Laptop GPU): window opens, teal background visible, one
RGB-interpolated triangle visible centered on screen (confirmed by
screenshot), survives two resizes and two full minimize/restore cycles
with the triangle correctly re-centered/scaled after each recreation
(framebuffers rebuilt, pipeline and render pass untouched, exactly as
designed), closes cleanly via the window's close button. **Zero
validation errors or warnings** — unlike M8B, no real validation issue
was hit during M8C's development; the render pass's subpass dependency
and the framebuffer-before-swapchain destruction order were both
correct on the first real run.

- Shader compiler/tool used: **`glslc`** (Vulkan SDK 1.4.357.0 on the
  development machine, but not version-pinned by the build)
- Shader files generated: **`triangle.vert.spv`**, **`triangle.frag.spv`**
  (into `${CMAKE_BINARY_DIR}/shaders/`)
- Render-pass vs. dynamic-rendering decision: **traditional
  `VkRenderPass` + `VkFramebuffer`** (see above)
- Pipeline recreation behavior: **render pass and pipeline are never
  recreated on resize; only framebuffers are** (see "Swapchain-Dependent
  Pipeline Resources (M8C)")

### What's Deferred to M8D+

No vertex/index buffers, no mesh abstraction, no textures, no descriptor
sets, no uniform buffers, no push constants, no camera, no transforms
sent to the GPU, no depth buffer, no Scene rendering, no Assets
integration, no OpenXR, no lighting, no PBR, no material system, no
VMA, no render graph. `VulkanGraphicsPipeline` is Vulkan-private and
expected to be revisited once M8D's real vertex-buffer work gives enough
evidence to design a generic pipeline/material shape — not before.

No other architectural issues were discovered — every part of M8C's
design (the render pass's implicit layout transitions, the
framebuffer/pipeline/render-pass lifetime split, dynamic viewport/
scissor across resize) worked correctly on the first real hardware run,
with zero validation warnings to investigate.

## 19. M8D Implementation Notes

M8D replaces M8C's `gl_VertexIndex`-generated triangle with real GPU
geometry: a CPU-defined quad (4 vertices, 6 indices, 2 triangles)
uploaded into GPU-visible Vulkan buffers and drawn with
`vkCmdDrawIndexed`. This is the minimum buffer infrastructure needed to
prove both vertex-buffer and index-buffer paths — no mesh/asset/texture
system yet.

### Vertex Structure (M8D)

`Vertex` (`VulkanVertex.hpp/.cpp`) is exactly the brief's example:

```cpp
struct Vertex
{
    AREngine::Core::Math::Vec2 position;
    AREngine::Core::Math::Vec3 color;
};
```

Reused `Core::Math::Vec2`/`Vec3` rather than inventing parallel
float-pair/float-triple types — they're already the engine's standard
2D/3D value types, standard-layout (so `offsetof` on `Vertex` is well-
defined), and carry no dependency `Vertex` wouldn't already need.
`Vertex` itself is deliberately private to Rendering's Vulkan backend
(`engine/rendering/src/vulkan/`), not a generic `Rendering` type — one
concrete vertex shape, with no second one to compare it against, is not
enough evidence to design a generic Mesh/VertexFormat abstraction from.

### Vertex Input Layout (M8D)

`Vertex::GetBindingDescription()`/`GetAttributeDescriptions()` describe
exactly one binding (binding 0, `stride = sizeof(Vertex)`,
`VK_VERTEX_INPUT_RATE_VERTEX` — per-vertex, not per-instance) and two
attributes:

| Location | Field | Format | Offset |
|---|---|---|---|
| 0 | `position` (Vec2) | `VK_FORMAT_R32G32_SFLOAT` | `offsetof(Vertex, position)` |
| 1 | `color` (Vec3) | `VK_FORMAT_R32G32B32_SFLOAT` | `offsetof(Vertex, color)` |

`VulkanGraphicsPipeline` wires these straight into
`VkPipelineVertexInputStateCreateInfo`; `triangle.vert`'s
`layout(location = 0) in vec2` / `layout(location = 1) in vec3`
declarations must (and do) match. No normals, no UVs, no tangents, no
instancing — the brief's scope exactly. Offsets come from `offsetof`,
not hand-counted byte math, so struct layout and the pipeline
description can't silently drift apart. Both `GetBindingDescription`
and `GetAttributeDescriptions` are pure logic (no Vulkan API calls) and
directly unit-tested in `tests/vulkan_tests.cpp`.

### Buffer Ownership (M8D)

`VulkanBuffer` (`VulkanBuffer.hpp/.cpp`) owns one `VkBuffer` and the
`VkDeviceMemory` backing it — one dedicated allocation per buffer, no
VMA, no sub-allocation, matching the "small, generic, RAII" shape of
every other owned Vulkan object in this backend (`VulkanInstance`,
`VulkanSwapchain`, etc.). Never exposed outside Rendering's Vulkan
backend — no `VkBuffer` appears on any public `Rendering` header.

### Memory Type Selection (M8D)

`FindMemoryType` (`VulkanMemory.hpp/.cpp`) takes an already-queried
`VkPhysicalDeviceMemoryProperties`, a `typeFilter` bitmask (from
`VkMemoryRequirements::memoryTypeBits`), and required property flags,
and returns the first memory type index satisfying both — pure logic,
no Vulkan calls, directly unit-tested with synthetic
`VkPhysicalDeviceMemoryProperties` data (including a case that proves
the type-filter bitmask is actually honored, not just the property
flags). Asserts if nothing matches, since every combination M8D
actually requests is spec-guaranteed available on any real Vulkan GPU.

The three property flags in play:

- **`HOST_VISIBLE`**: the CPU can map this memory into its own address
  space (`vkMapMemory`) and read/write it directly. Required for the
  staging buffer, since that's where CPU-side vertex/index data gets
  copied from.
- **`HOST_COHERENT`**: writes the CPU makes to mapped `HOST_VISIBLE`
  memory become visible to the GPU without an explicit
  `vkFlushMappedMemoryRanges` call. Requested alongside `HOST_VISIBLE`
  for the staging buffer specifically to avoid needing that extra call
  — simpler, at a possible (here, irrelevant — this is a one-shot
  startup upload, not a per-frame one) cost versus manual flushing.
- **`DEVICE_LOCAL`**: the fastest memory for the GPU itself to read
  from during rendering, typically not directly writable by the CPU at
  all. Used for the actual vertex/index buffers the pipeline reads
  from every frame.

### Upload Strategy (M8D)

**Staging buffer → device-local destination buffer** (option B from the
brief), not direct host-visible vertex/index buffers. Chosen because
it's genuinely closer to how static GPU geometry will actually work
once real meshes exist (M8D's vertices are trivial, but the *pattern*
— CPU data lands in `DEVICE_LOCAL` memory via a temporary staging step
— is the one worth establishing now), and it turned out not to add much
real complexity: `CreateDeviceLocalBuffer` (`VulkanBuffer.cpp`) is
~15 lines, built entirely from pieces (`VulkanBuffer`,
`VulkanOneTimeCommands`) that already needed to exist. The flow exactly
matches the brief's recommended path:

```
CPU data (std::vector<Vertex> / std::vector<uint32_t>)
    ↓
HOST_VISIBLE | HOST_COHERENT staging VulkanBuffer (VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    ↓
VulkanBuffer::CopyDataIn — vkMapMemory / memcpy / vkUnmapMemory
    ↓
BeginOneTimeCommands → vkCmdCopyBuffer → EndOneTimeCommands
    ↓
DEVICE_LOCAL destination VulkanBuffer (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
or VK_BUFFER_USAGE_INDEX_BUFFER_BIT, + VK_BUFFER_USAGE_TRANSFER_DST_BIT)
```

### Staging Buffer Behavior (M8D)

The staging `VulkanBuffer` inside `CreateDeviceLocalBuffer` is a plain
local variable — it is destroyed automatically (its destructor runs)
the moment the function returns, immediately after
`EndOneTimeCommands` has already waited for the copy to finish. It is
never kept alive past the upload it exists for; there is no pool of
staging buffers, no reuse, no lingering allocation.

### Synchronous Upload Limitation (M8D)

`EndOneTimeCommands` (`VulkanOneTimeCommands.cpp`) submits the copy
command buffer, then calls `vkQueueWaitIdle` before returning — the
whole `CreateDeviceLocalBuffer` call blocks until its copy has actually
completed on the GPU. This is a deliberate, documented simplification,
not an oversight: `vkQueueWaitIdle` stalls the *entire* queue rather
than waiting on a per-submission fence, which would cost real
throughput if this happened every frame — but M8D's uploads are a
handful of one-shot calls at startup, not steady-state work, so the
simpler synchronous path was chosen over building fence-based async
tracking for a case that doesn't need it yet. No background uploading,
no transfer queue, no async transfer scheduling — all explicitly
deferred, per the brief.

### Index Buffer (M8D)

Indices are `std::uint32_t` (`VK_INDEX_TYPE_UINT32`), not `uint16_t` —
simplicity over the (here, irrelevant at 6 indices) memory savings a
16-bit index would give; the brief explicitly allowed this default.
Uploaded through the same `CreateDeviceLocalBuffer` path as the vertex
buffer, with `VK_BUFFER_USAGE_INDEX_BUFFER_BIT`. Bound in the command
buffer via `vkCmdBindIndexBuffer(commandBuffer, indexBuffer->Get(), 0,
VK_INDEX_TYPE_UINT32)`, drawn via `vkCmdDrawIndexed(commandBuffer,
static_cast<uint32_t>(indices.size()), 1, 0, 0, 0)`.

### Indexed Draw Path (M8D)

Per-frame command recording, extended from M8C:

```
vkCmdBeginRenderPass (unchanged from M8C)
    ↓
vkCmdSetViewport, vkCmdSetScissor (unchanged from M8C)
    ↓
vkCmdBindPipeline (unchanged from M8C)
    ↓
vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets)   [NEW]
    ↓
vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32)   [NEW]
    ↓
vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0)   [replaces M8C's vkCmdDraw(3, ...)]
    ↓
vkCmdEndRenderPass (unchanged from M8C)
```

No `gl_VertexIndex`-generated positions remain anywhere in the shader
or the demo.

### Resource Lifetime: Swapchain-Dependent vs. Geometry (M8D)

The vertex and index buffers are constructed once, right after
`commandPool` in `vulkan_present_demo.cpp`, and are **not** touched by
`recreateSwapchain()`. This is a real, important distinction, not an
oversight:

- **Swapchain-dependent** (recreated on every resize):
  `VulkanSwapchain` itself (images/views), `VulkanFramebuffers`.
- **Not swapchain-dependent** (created once, survive every resize):
  the vertex buffer, the index buffer, `VulkanGraphicsPipeline`,
  `VulkanRenderPass` (both already established as resize-independent in
  M8C — see "Swapchain-Dependent Pipeline Resources (M8C)").

Geometry has no relationship to window size at all — a quad's vertex
positions in clip space don't change because the window got bigger or
smaller. Recreating geometry buffers on resize would be both wasteful
(a real upload, however small, for no reason) and conceptually wrong
(it would suggest geometry is somehow tied to presentation, which it
isn't). Verified in practice: two resizes and two minimize/restore
cycles during validation, with the quad correctly re-rendered every
time and the vertex/index buffers never recreated, logged, or touched.

### Command Recording

Command buffer allocation, frame-in-flight synchronization
(`FrameSyncObjects`, per-image `renderFinishedSemaphores`/
`imagesInFlight`), and swapchain acquire/present are all unchanged from
M8B/M8C — see those sections. Only the recording *inside* the render
pass changed, as described in "Indexed Draw Path" above.

### Generic RenderDevice / BufferDesc Review (M8D)

The brief asked for a review of M4's `BufferDesc`/`BufferHandle`/
`RenderDevice::CreateBuffer` against M8D's real evidence, without
redesigning the public API unless a concrete mismatch is proven. M8D's
demo does **not** route through `RenderDevice`/`CreateBuffer` at all —
same as every Vulkan demo since M8A, it reaches directly into
Rendering's private `src/vulkan/` implementation (see "Frame /
Rendering Boundary (M8B)"). So nothing *forced* a change here. Answering
the five questions anyway, since the evidence is worth recording even
though M8D's demo bypasses the generic API entirely:

1. **Does `BufferDesc` contain enough information?** No — it has
   `sizeBytes` and `usage`, but no way to supply *initial* CPU data.
   `CreateBuffer` alone cannot express "create this buffer and fill it
   with these bytes" — the exact operation M8D's `CreateDeviceLocalBuffer`
   performs. This is a real, now-proven gap, not a hypothetical one.
2. **Are `Vertex`/`Index`/`Uniform` usage categories still sensible?**
   Yes, as far as they go — M8D used exactly two of the three
   (`Vertex`, `Index`) and needed nothing else. No evidence yet that a
   fourth category is needed.
3. **Can `BufferHandle` remain backend-neutral?** Yes, unaffected —
   `BufferHandle` is just an opaque integer id; nothing about M8D's
   `VulkanBuffer` (owning a `VkBuffer`+`VkDeviceMemory` pair) requires
   the *handle* itself to carry any more information. A future Vulkan
   `RenderDevice` implementation would map `BufferHandle`s to
   `VulkanBuffer*`/`std::unique_ptr<VulkanBuffer>` internally, same as
   `NullRenderDevice` already maps them to its own internal map.
4. **Is initial CPU data missing from the generic design?** Yes — see
   (1). There is no `RenderDevice` operation today that both creates a
   buffer and uploads data into it, nor a separate explicit "upload/
   write" operation once a buffer exists.
5. **Should upload remain a separate operation?** Most likely yes, once
   a generic API does grow one — M8D's own implementation already
   separates "create + allocate" (`VulkanBuffer`'s constructor) from
   "put data in" (`CopyDataIn`, and the staging-copy dance
   `CreateDeviceLocalBuffer` builds on top of it) precisely because
   upload has real synchronization/timing considerations (see
   "Synchronous Upload Limitation" above) that buffer creation itself
   doesn't. A single combined "create-with-data" call would hide that.

**No change was made to `BufferDesc.hpp`, `Handles.hpp`, or
`RenderDevice.hpp`.** The gap in point (1)/(4) is real evidence, but a
single concrete backend (M8D's demo-only `VulkanBuffer`, used by no
`RenderDevice` implementation at all) is not yet proof of the *right*
shape for a generic upload API — e.g., whether it should be a
`BufferDesc::initialData` pointer, a separate `RenderDevice::UploadBuffer`
method, or something else, is exactly the kind of design question M4
deferred needing "real evidence" to answer, and one non-generic Vulkan
demo isn't that evidence yet. Revisit once a real `VulkanRenderDevice`
(implementing the actual `RenderDevice` interface, not just a private
demo) exists and needs to answer this for real.

### Memory Types Selected On The RTX 3060 (M8D)

Reported by the manual demo on the development machine (NVIDIA GeForce
RTX 3060 Laptop GPU): the staging buffer's `HOST_VISIBLE |
HOST_COHERENT` request and the destination buffers'
`DEVICE_LOCAL` request both resolved successfully via `FindMemoryType`
with zero validation errors — the driver reports at least one memory
type satisfying each combination, as expected on any discrete GPU.

### NullRenderDevice / Runtime

Both untouched, exactly as the brief required. `RenderingTests` still
pass unchanged; `AREngineSandbox.exe` still runs on `NullRenderDevice`.

### Validation Results (M8D)

`ARENGINE_ENABLE_VULKAN=ON` (default): full `/W4 /WX` clean build,
`ctest` 9/9 (5 new M8D pure-logic checks: 3 for `FindMemoryType`, 2 for
`Vertex`'s binding/attribute descriptions).
`ARENGINE_ENABLE_VULKAN=OFF`: full build succeeds, no Vulkan/shader
targets present, `ctest` 8/8 (no `VulkanTests`).

`arengine_vulkan_present_demo` run against real hardware (NVIDIA
GeForce RTX 3060 Laptop GPU): window opens, a colored quad (4 distinct
corner colors, smoothly interpolated, no seam visible across the
diagonal shared by both triangles — confirming the index buffer
correctly reuses vertices 0 and 2) renders on the teal background,
confirmed by screenshot both initially and after two resizes plus two
full minimize/restore cycles (with the quad correctly re-rendered every
time and the geometry buffers never recreated), closes cleanly.
**Zero validation errors or warnings** — nothing needed fixing during
M8D's development; memory binding, buffer usage flags, copy
synchronization, vertex binding offsets, the index type, and object
destruction were all correct on the first real hardware run.

- Upload strategy: **staging buffer → device-local buffer** (option B)
- Memory types selected: **`HOST_VISIBLE | HOST_COHERENT`** (staging),
  **`DEVICE_LOCAL`** (vertex/index destination buffers) — both resolved
  successfully on the RTX 3060
- Vertex stride: **20 bytes** (`sizeof(Vertex)` = 2×4 + 3×4), attributes:
  **location 0 = position (`VK_FORMAT_R32G32_SFLOAT`, offset 0)**,
  **location 1 = color (`VK_FORMAT_R32G32B32_SFLOAT`, offset 8)**
- Index type/count: **`VK_INDEX_TYPE_UINT32`, 6 indices** (24 bytes)

### What's Deferred to M8E+

No GLTF/OBJ loading, no `MeshAsset`, no texture, no UVs, no normals, no
material system, no descriptor sets, no uniform buffers, no push
constants, no camera, no transforms sent to the GPU, no depth buffer,
no Scene rendering, no OpenXR, no VMA, no instancing, no batching. The
`Vertex` struct and `VulkanBuffer`/`CreateDeviceLocalBuffer` remain
Vulkan-private, same reasoning as `VulkanGraphicsPipeline` in M8C — one
quad's worth of evidence is not enough to design a generic
Mesh/VertexFormat/upload abstraction from.

No other architectural issues were discovered — vertex/index buffer
creation, staging upload, and indexed drawing all worked correctly on
the first real hardware run, with zero validation warnings to
investigate.

## 20. M8E Implementation Notes

M8E adds real Vulkan texturing: a procedurally generated checkerboard
image, uploaded to a `DEVICE_LOCAL` `VkImage`, sampled in the fragment
shader through a descriptor set, and composited with the vertex colors
M8D already established. The quad's shape/geometry is unchanged from
M8D — only what gets drawn onto it is new.

### Vertex Format (M8E)

`Vertex` (`VulkanVertex.hpp/.cpp`) gained one field, `uv` (`Vec2`),
after `color`:

```cpp
struct Vertex
{
    AREngine::Core::Math::Vec2 position;
    AREngine::Core::Math::Vec3 color;
    AREngine::Core::Math::Vec2 uv;
};
```

`color` was kept (the brief allowed dropping it, but keeping it let the
demo prove texture sampling and vertex-color interpolation compose
correctly in the same draw — see "Shader Sampling Path" below).
`GetAttributeDescriptions()` grew a third entry: location 2, `uv`,
`VK_FORMAT_R32G32_SFLOAT`, offset via `offsetof(Vertex, uv)` — same
pattern as position/color, still fully unit-tested (an existing M8D
test was updated for 3 attributes instead of 2; a new location-2/format/
offset check was added).

### Checkerboard Generation Strategy (M8E)

`GenerateCheckerboardRGBA8(width, height, tileSize)`
(`VulkanCheckerboard.hpp/.cpp`) is pure logic — no Vulkan calls, no file
I/O — producing tightly packed 8-bit RGBA pixels
(`width * height * 4` bytes). A pixel is white
(`{255,255,255,255}`) if `(x/tileSize + y/tileSize)` is even, black
(`{0,0,0,255}`) otherwise; alpha is always fully opaque. Chosen over
option B (a small local image file) specifically to avoid pulling in
any image decoder — no stb_image, no PNG, no JPEG — which this
milestone has no other reason to need. Directly unit-tested (byte size,
adjacent-tile color difference, same-tile color equality, full opacity)
without touching the GPU at all. The demo generates a 64×64 image with
an 8×8-pixel tile size (8×8 tiles total).

### Vulkan Image Ownership (M8E)

`VulkanImage` (`VulkanImage.hpp/.cpp`) owns one `VkImage`, the
`VkDeviceMemory` backing it, and one `VkImageView` over it — one mip
level, one array layer, no mipmapping, same "one dedicated allocation,
no VMA" discipline as `VulkanBuffer`. The `VkSampler` is deliberately
**not** part of this class — see "Sampler Settings" below for why a
separate `VulkanSampler` was the simpler choice. Never exposed outside
Rendering's Vulkan backend.

### Image Memory / Upload Path (M8E)

Same staging pattern M8D established for buffers, applied to an image:
`CreateTextureFromPixels` (`VulkanImage.cpp`) creates a temporary
`HOST_VISIBLE | HOST_COHERENT` staging `VulkanBuffer`, copies the
checkerboard pixels into it (`VulkanBuffer::CopyDataIn`, reused
unchanged from M8D), creates a `DEVICE_LOCAL` `VulkanImage`
(`VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`), then
on one synchronous one-time command buffer (`VulkanOneTimeCommands`,
also reused unchanged): transitions the image to
`TRANSFER_DST_OPTIMAL`, `vkCmdCopyBufferToImage`s the staging buffer
into it, transitions it again to `SHADER_READ_ONLY_OPTIMAL`, then waits
for completion (`EndOneTimeCommands`'s `vkQueueWaitIdle`, same
"synchronous, documented limitation" as M8D's buffer uploads — see
`docs/ARCHITECTURE.md` §19, "Synchronous Upload Limitation (M8D)",
which applies identically here). The staging buffer is a plain local
variable, destroyed automatically the moment the function returns — not
kept alive past the upload.

### Image Layout Transitions (M8E)

`TransitionImageLayout` (`VulkanImageLayoutTransition.hpp/.cpp`)
supports exactly two transitions, matching what a texture upload
actually needs:

| Transition | srcAccessMask | dstAccessMask | srcStage | dstStage |
|---|---|---|---|---|
| `UNDEFINED` → `TRANSFER_DST_OPTIMAL` | `0` | `TRANSFER_WRITE_BIT` | `TOP_OF_PIPE` | `TRANSFER` |
| `TRANSFER_DST_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | `TRANSFER_WRITE_BIT` | `SHADER_READ_BIT` | `TRANSFER` | `FRAGMENT_SHADER` |

Aspect mask is always `VK_IMAGE_ASPECT_COLOR_BIT`, one mip, one layer.
Asserts on any other `(oldLayout, newLayout)` pair — this is
deliberately not a generic parameterized barrier builder. M8B's
`VulkanImageBarrier` (which *was* shaped that way) was already deleted
in M8C once the render pass took over the swapchain image's
transitions; M8E's texture upload is a genuinely different use case
(a different image, different stages/access masks, transfer-then-
sample rather than clear-then-present), so a fresh, narrower helper was
written rather than reintroducing the old generic one.

### Image View (M8E)

One `VK_IMAGE_VIEW_TYPE_2D` view per texture, matching the image's
format exactly, `VK_IMAGE_ASPECT_COLOR_BIT`, `baseMipLevel = 0` /
`levelCount = 1`, `baseArrayLayer = 0` / `layerCount = 1` — no
mipmapping, no array textures. Owned by `VulkanImage`, destroyed before
the image it views (same "dependent object first" ordering as every
other owned Vulkan pair in this backend).

### Texture Format (M8E)

`VK_FORMAT_R8G8B8A8_SRGB`. This texture holds color data meant to be
*seen* (even though the checkerboard itself is just black/white) —
sRGB tells the GPU to convert each sampled texel from sRGB gamma space
to linear automatically (`texture(uTexture, uv)` returns already-
linearized values), which is the physically correct way to filter and
blend color/albedo data. A data texture that isn't meant to be
perceived directly (a normal map, a roughness map, etc.) would instead
use an `*_UNORM` format to keep its raw numeric values un-transformed —
that distinction is explicitly out of scope for M8E (no normal maps
exist yet), noted here only to explain why sRGB was the right choice
for *this* texture specifically, not a blanket policy.

### Sampler Settings (M8E)

`VulkanSampler` (`VulkanSampler.hpp/.cpp`) is a separate class from
`VulkanImage`, not merged into it: a sampler describes *how to read* an
image, not the image data itself, and in a real engine a single sampler
is commonly shared across many textures — keeping it independently
constructible is the simpler design at this scale, not a premature
"texture system." Settings:

- `magFilter`/`minFilter`: `VK_FILTER_LINEAR` — smooth filtering, the
  common default for a color texture (nearest would keep the
  checkerboard's tile edges hard-pixelated; either would equally prove
  sampling works, LINEAR was picked as the more broadly useful default
  going forward).
- `addressMode{U,V,W}`: `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE` — the
  demo's UVs span exactly `[0,1]` and never sample outside that range,
  so tiling behavior is irrelevant either way *here*; CLAMP_TO_EDGE was
  still chosen deliberately as the more broadly correct default for a
  single, non-tiling texture (it avoids any wraparound sampling
  artifact at the very edge texels under linear filtering, which REPEAT
  would not).
- `anisotropyEnable = VK_FALSE`, deliberately, even though the RTX 3060
  supports anisotropic filtering — M8E states no requirement for it, so
  it was not turned on just because the hardware could.
- `maxLod = 0.0f`: no mipmaps exist (one level), so LOD never has
  anywhere to move.

### Descriptor Set Layout (M8E)

M8E is the first milestone with any descriptor requirement at all.
`VulkanDescriptorSetLayout` (`VulkanDescriptorSetLayout.hpp/.cpp`) owns
one `VkDescriptorSetLayout` describing exactly one binding: binding 0,
`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`, visible to
`VK_SHADER_STAGE_FRAGMENT_BIT` only — matching triangle.frag's
`layout(set = 0, binding = 0) uniform sampler2D uTexture`. Deliberately
the smallest possible layout — no generic descriptor-layout builder, no
bindless descriptors, no descriptor indexing.

### Descriptor Pool/Set Ownership (M8E)

`VulkanDescriptorPool` (`VulkanDescriptorPool.hpp/.cpp`) owns one
`VkDescriptorPool` sized for exactly one combined-image-sampler
descriptor and `maxSets = 1` — created **without**
`VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`, so its one
allocated `VkDescriptorSet` is freed implicitly when the pool itself is
destroyed, never individually. `Allocate(layout)` returns that one
`VkDescriptorSet`, owned by the pool, not separately RAII-wrapped. A
free function, `WriteCombinedImageSamplerDescriptor`, performs the one
`vkUpdateDescriptorSets` call that points the set's binding 0 at the
texture's image view + sampler (expecting
`VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, the layout
`CreateTextureFromPixels` leaves the image in). One texture, one
descriptor set, one write, for the whole demo's lifetime — no
recycling, no per-frame allocator, no caching, exactly as the brief
required.

### Pipeline Layout Change (M8E)

`VulkanGraphicsPipeline`'s constructor gained a third parameter,
`VkDescriptorSetLayout descriptorSetLayout`, and its
`VkPipelineLayoutCreateInfo` now sets `setLayoutCount = 1` /
`pSetLayouts = &descriptorSetLayout` (M8C/M8D's empty layout is gone).
Still no push constants. The pipeline itself remains swapchain-extent-
independent (dynamic viewport/scissor, unchanged from M8C) and is still
not recreated on resize.

### Shader Sampling Path (M8E)

`triangle.vert` gained `layout(location = 2) in vec2 inUV` and
`layout(location = 1) out vec2 fragUV` (color stays at output location
0). `triangle.frag` gained
`layout(set = 0, binding = 0) uniform sampler2D uTexture` and now
computes:

```glsl
outColor = texture(uTexture, fragUV) * vec4(fragColor, 1.0);
```

Multiplying by `fragColor` (rather than outputting the raw sample) was
chosen deliberately — the brief allowed either — because it visibly
proves both systems compose correctly in one draw: black checkerboard
tiles stay black (anything × 0 = 0) while white tiles show the full
per-vertex color gradient M8D already established, confirmed in the
screenshot taken during validation.

### Command Recording (M8E)

Per-frame recording, extended from M8D:

```
vkCmdBeginRenderPass / vkCmdSetViewport / vkCmdSetScissor / vkCmdBindPipeline   (unchanged)
    ↓
vkCmdBindVertexBuffers   (unchanged)
    ↓
vkCmdBindIndexBuffer   (unchanged)
    ↓
vkCmdBindDescriptorSets(commandBuffer, GRAPHICS, pipeline.GetLayout(), 0, 1, &descriptorSet, 0, nullptr)   [NEW]
    ↓
vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0)   (unchanged)
    ↓
vkCmdEndRenderPass   (unchanged)
```

No per-frame texture upload — the texture, sampler, and descriptor set
are all created once before the render loop begins.

### Resource Lifetime (M8E)

Extends M8D's swapchain-dependent/not-dependent split with the new
texture resources:

- **Swapchain-dependent** (recreated on every resize):
  `VulkanSwapchain` (images/views), `VulkanFramebuffers`.
- **Not swapchain-dependent** (created once, survive every resize):
  the vertex buffer, the index buffer, `VulkanGraphicsPipeline`,
  `VulkanRenderPass` (both already established in M8C), and as of M8E:
  the texture image, its image view, the sampler, and the descriptor
  pool/set (unless the pipeline layout itself changes, which resizing
  never does).

A texture has no relationship to window size, same reasoning as M8D's
geometry buffers — a checkerboard's pixels don't change because the
window got bigger. `recreateSwapchain()` was extended with new
swapchain-dependent objects (`VulkanFramebuffers`, already handled in
M8C) but not touched for any texture/descriptor resource. Verified in
practice: two resizes and two minimize/restore cycles during
validation, with the textured quad correctly re-rendered every time and
none of the texture/sampler/descriptor objects recreated, logged, or
touched.

### Generic TextureDesc/TextureHandle Review (M8E)

Same situation as M8D's `BufferDesc` review: M8E's demo does **not**
route through `RenderDevice`/`CreateTexture` — it reaches directly into
Rendering's private `src/vulkan/` implementation, same as every Vulkan
demo since M8A. So nothing *forced* a change. Answering the five
questions anyway:

1. **Does `TextureDesc` contain enough information?** No — it has
   `width`, `height`, and `format` (currently only `RGBA8Unorm`), but
   no mip-level count, no array-layer count, no usage flags (sampled
   vs. render target vs. storage), and critically:
2. **Is initial pixel data missing from the generic API?** Yes — same
   gap as `BufferDesc`. There is no way to say "create this texture and
   fill it with these pixels," which is exactly what
   `CreateTextureFromPixels` does. This is now proven twice (buffers in
   M8D, images in M8E) — a real, recurring pattern, not a one-off.
3. **Should image creation and data upload remain separate?** Most
   likely yes, for the same reason as `BufferDesc`'s answer: M8E's own
   implementation already separates "create the image" (`VulkanImage`'s
   constructor) from "upload data into it" (the staging/copy dance in
   `CreateTextureFromPixels`), because upload carries real
   synchronization considerations creation alone doesn't.
4. **Can `TextureHandle` stay backend-neutral?** Yes, unaffected — same
   reasoning as `BufferHandle`: it's an opaque id, and a future Vulkan
   `RenderDevice` would map it to a `VulkanImage*`/`VulkanSampler*` pair
   internally.
5. **Are format names sufficiently backend-neutral?** Partially proven
   inadequate: `TextureFormat::RGBA8Unorm` doesn't distinguish sRGB from
   linear encoding, and M8E specifically needed
   `VK_FORMAT_R8G8B8A8_SRGB`, not `_UNORM`. A generic `TextureFormat`
   enum will eventually need to express that distinction (color data vs.
   data textures use different encodings) — another real, now-
   documented gap, not yet fixed.

**No change was made to `TextureDesc.hpp` or `Handles.hpp`.** The gaps
in points (1)/(2)/(5) are real and now evidenced twice over (buffers
and textures both need initial-data upload; textures additionally need
an sRGB/linear distinction `TextureFormat` doesn't have), but M8D
already documented that one non-generic Vulkan backend isn't proof of
the *right* shape for a generic upload API — M8E adds a second data
point in the same direction without yet forcing the design question.
Revisit both together once a real `VulkanRenderDevice` needs to answer
this for real.

### NullRenderDevice / Runtime

Both untouched, exactly as the brief required. `RenderingTests` still
pass unchanged; `AREngineSandbox.exe` still runs on `NullRenderDevice`.

### Validation Results (M8E)

`ARENGINE_ENABLE_VULKAN=ON` (default): full `/W4 /WX` clean build,
`ctest` 9/9 (one M8D test updated for the new 3-attribute `Vertex`;
3 new M8E pure-logic checks for the checkerboard generator).
`ARENGINE_ENABLE_VULKAN=OFF`: full build succeeds, no Vulkan/shader
targets present, `ctest` 8/8 (no `VulkanTests`).

`arengine_vulkan_present_demo` run against real hardware (NVIDIA
GeForce RTX 3060 Laptop GPU): window opens, a textured quad — an 8×8
checkerboard multiplied by the M8D vertex-color gradient, confirming
UV orientation is correct (a clean, undistorted grid, not mirrored or
skewed) and that the descriptor-bound texture is genuinely being
sampled (not a fallback/default color) — renders on the teal
background, confirmed by screenshot both initially and after two
resizes plus two full minimize/restore cycles (with the texture
correctly re-rendered every time and never recreated), closes cleanly.
**Zero validation errors or warnings** — nothing needed fixing during
M8E's development; image memory binding, buffer/image usage flags,
copy synchronization, the layout transition sequence, and object
destruction were all correct on the first real hardware run.

- Texture dimensions/format: **64×64, `VK_FORMAT_R8G8B8A8_SRGB`**
  (16,384 bytes, tightly packed RGBA8)
- Sampler settings: **`VK_FILTER_LINEAR`** (mag/min),
  **`VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`** (U/V/W),
  **anisotropy disabled**, **`maxLod = 0`** (no mipmaps)
- Descriptor layout/binding: **set 0, binding 0,
  `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`, fragment stage only**
- Image layout transition sequence: **`UNDEFINED` →
  `TRANSFER_DST_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL`**

### What's Deferred to M8F+

No PNG/JPEG/stb_image decoding, no glTF, no `MeshAsset`, no material
system, no normal maps, no mipmaps, no anisotropic filtering, no depth
buffer, no camera, no transforms sent to the GPU, no Scene rendering,
no PBR, no lighting, no OpenXR, no VMA, no bindless descriptors, no
descriptor indexing, no texture streaming. `VulkanImage`/
`VulkanSampler`/the descriptor infrastructure remain Vulkan-private,
same reasoning as `VulkanGraphicsPipeline` (M8C) and `VulkanBuffer`
(M8D) — one texture's worth of evidence is not enough to design a
generic Texture/Material abstraction from.

No other architectural issues were discovered — image creation, staging
upload, layout transitions, descriptor binding, and textured indexed
drawing all worked correctly on the first real hardware run, with zero
validation warnings to investigate.

## 21. M8F Implementation Notes

M8F moves AREngine from flat 2D demo geometry into genuine 3D: real
`Vec3` vertex positions, a fixed camera, a Vulkan-conforming perspective
projection, a depth buffer, and depth testing — proven by two
overlapping quads at different depths, the nearer one submitted first,
where correct occlusion can only happen if the depth buffer (not draw
order) is deciding visibility.

### Vec3 Vertex Layout (M8F)

`Vertex::position` changed from `Vec2` to `Vec3`
(`VulkanVertex.hpp/.cpp`); `color` and `uv` are unchanged. Attribute 0's
format changed from `VK_FORMAT_R32G32_SFLOAT` to
`VK_FORMAT_R32G32B32_SFLOAT` accordingly; `color`/`uv`'s offsets shift
automatically since they're computed via `offsetof`, not hand-counted —
the same reason `offsetof` was used from the start back in M8D. New
stride: 32 bytes (`3+3+2` floats × 4 bytes). Both the binding-stride and
attribute-format/offset assertions are covered by updated pure-logic
tests in `tests/vulkan_tests.cpp`.

### Camera Convention (M8F)

One hard-coded camera for the whole demo: positioned at `(0, 0, 3)`,
looking at the world origin — i.e. looking down world `-Z`, exactly
AREngine's `Forward` convention (see `docs/WORLD_CONVENTIONS.md`). No
keyboard/mouse control, no `Camera` component, no `Input` integration —
all explicitly deferred to M8G, which will need to answer what a real
camera abstraction looks like with actual evidence from a movable one,
not guessed at here.

### View Matrix Convention (M8F)

`Core::Math::LookAtRH(eye, target, up)` (new,
`engine/core/include/AREngine/Core/Math/ViewProjection.hpp` — see
"Core/Vulkan Clip-Space Split" below for why this file is not named
`Camera.hpp`) builds a standard right-handed "look at" view matrix,
following the same `+Y` up / `-Z` forward convention every other piece
of Core math already uses. This is **not** a Vulkan-specific function —
the view matrix's job (expressing world points relative to the camera)
is identical regardless of target graphics API; only the *projection*
step differs between APIs (see below). Added to Core (not
Vulkan-private) specifically so it's directly unit-tested
(`tests/core_tests.cpp`, `TestLookAtRH`) without needing a GPU, and so
it's available to any future backend without duplication.

### Vulkan Projection Convention (M8F)

**Updated shortly after M8F's approval — see "Core/Vulkan Clip-Space
Split" immediately below for the full reasoning; this subsection
describes the current, final design, not M8F's original one.**

`Core::Math::PerspectiveRH_ZO(fovYRadians, aspect, nearZ, farZ)`
(`ViewProjection.hpp`) builds a right-handed perspective projection
matrix with NDC depth in `[0, 1]` ("ZO") rather than OpenGL's
`[-1, 1]` — the convention shared by Vulkan, Direct3D, and Metal. This
depth-range choice is genuinely backend-neutral math (OpenGL is the
outlier, not Vulkan), so it lives in Core, on the same footing as
`LookAtRH`. Verified by `TestPerspectiveRH_ZO` in `tests/core_tests.cpp`,
which checks that view-space `z = -near` maps to NDC depth `0` and
`z = -far` maps to NDC depth `1` numerically, not just "it compiles."

**The Y flip is a separate, Vulkan-layer step, not part of this
function.** Vulkan's NDC `+Y` points down the framebuffer, opposite
AREngine's `+Y`-up world convention — but that mapping is a property of
Vulkan's specific screen-space convention, not of the RH/ZO depth
convention itself (OpenGL needs no such flip despite also supporting a
right-handed view). `AREngine::Rendering::Vulkan::ApplyVulkanYFlip`
(`engine/rendering/src/vulkan/VulkanClipSpace.hpp/.cpp`) is where
AREngine actually performs it: negates the projection matrix's Y-scale
term, applied by the demo as
`ApplyVulkanYFlip(PerspectiveRH_ZO(...))`. AREngine deliberately does
**not** use the alternative (a negative-height viewport, available via
`VK_KHR_maintenance1`/core 1.1): baking the flip into one small,
explicit Vulkan-layer function keeps the correction in exactly one
place, doesn't depend on any extra feature check, and is proven correct
directly by `tests/vulkan_tests.cpp`'s
`TestApplyVulkanYFlipComposesWithProjection` (a world-up point gets
negative NDC y after the flip) — not merely inferred from a
non-upside-down screenshot.

Both the depth-range and Y-flip corrections, composed together, are
proven end to end by `TestModelViewProjectionComposition`
(`tests/core_tests.cpp`, using Core's unflipped `PerspectiveRH_ZO`) and
`TestApplyVulkanYFlipComposesWithProjection`
(`tests/vulkan_tests.cpp`, proving the Vulkan-layer flip). Composing
`Projection * View * Model` end to end and checking the result lands
centered on screen with a valid depth needed `Mat4::operator*(Vec4)`
(new in M8F, `Mat4.hpp`) — `TransformPoint`'s existing w=1-assuming
shortcut is not valid for a projection matrix's output.

### Core/Vulkan Clip-Space Split

M8F's original `PerspectiveVulkanRH` baked both the depth-range
conversion *and* the Vulkan-specific Y-flip into one Core function —
approved and shipped that way, but flagged immediately afterward as a
layering violation: **Core must remain graphics-backend independent**,
and a function literally named after Vulkan, living in Core, was a
concrete instance of that boundary leaking. Fixed as a dedicated
cleanup before M8G:

- `PerspectiveVulkanRH` → **`PerspectiveRH_ZO`**, named after the
  mathematical convention it implements (right-handed, zero-to-one
  depth) rather than a graphics API. It no longer performs the Y flip.
- The Y flip moved to a new, small, Vulkan-private function,
  `ApplyVulkanYFlip` (`engine/rendering/src/vulkan/VulkanClipSpace.hpp/.cpp`)
  — pure logic (no Vulkan API calls), directly unit-tested. The demo
  now calls `ApplyVulkanYFlip(PerspectiveRH_ZO(...))` instead of a
  single combined call; the resulting matrix, and the demo's visible
  output, are bit-for-bit identical to before.
- `Core/Math/Camera.hpp` → **`Core/Math/ViewProjection.hpp`**. Core does
  not own (and this file does not implement) a `Camera` system —
  `Camera.hpp` implied more than two free functions actually provide.
  `ViewProjection.hpp` names exactly what's inside: a view-matrix
  helper and a projection-matrix helper.

Restated, for reference, the conventions this boundary now keeps
cleanly separated:

- **AREngine world coordinates** (`Core`, all of them): right-handed,
  `+Y` up, `-Z` forward — see `docs/WORLD_CONVENTIONS.md`. Fixed,
  engine-wide, and what `LookAtRH`/`PerspectiveRH_ZO` both honor.
- **Projection depth convention used by the current renderer**: `0..1`
  (Vulkan's NDC depth range) — a choice `PerspectiveRH_ZO` makes
  explicit in its name, but which is itself graphics-API-adjacent
  (shared by Vulkan/D3D/Metal, not OpenGL) rather than a pure world
  convention; it stays in Core because the *math* for it is backend-
  neutral, not because Core is Vulkan-aware.
- **Vulkan-specific framebuffer/rendering choices** (the Y flip, and
  anything like it in the future): stay in
  `engine/rendering/src/vulkan/`, never in `Core`.

No complicated clip-space abstraction (no `GraphicsAPI` enum, no
per-backend projection factory) was introduced — the split is exactly
two small functions in two files, chosen to be the minimal change that
restores the boundary. Rebuilt, re-tested (`/W4 /WX` clean, `ctest`
9/9, `ARENGINE_ENABLE_VULKAN=OFF` still 8/8), and the manual Vulkan demo
re-run and re-screenshotted to confirm the visible output — including
the M8F depth-testing proof — is unchanged, with zero validation
errors.

### Depth Format Selection (M8F)

`FindSupportedDepthFormat(physicalDevice)`
(`VulkanDepthFormat.hpp/.cpp`) queries AREngine's preferred order —
`VK_FORMAT_D32_SFLOAT`, `VK_FORMAT_D32_SFLOAT_S8_UINT`,
`VK_FORMAT_D24_UNORM_S8_UINT` — against real device format properties,
and hands the results to `SelectDepthFormat`, which picks the first
candidate whose optimal-tiling features include
`VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` — pure logic over
already-queried data, directly unit-tested with synthetic
`VkFormatProperties` (including a case proving a candidate lacking the
feature is correctly skipped). M8F only requires depth capability; a
selected format that happens to carry a stencil component (both
fallback candidates do) is never used for stencil operations anywhere
in this backend. **On the development machine's GPU (NVIDIA GeForce RTX
3060 Laptop), the first preference, `VK_FORMAT_D32_SFLOAT`, was
selected directly** (format value `126`).

### Depth Image Ownership (M8F)

The depth image reuses `VulkanImage` (M8E) rather than a new,
near-duplicate class — it already owns exactly the right triple
(`VkImage` + `VkDeviceMemory` + `VkImageView`), and the only real
differences a depth image needs are its format, usage
(`VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`), and view aspect mask.
`VulkanImage`'s constructor gained an `aspectMask` parameter
(defaulting to `VK_IMAGE_ASPECT_COLOR_BIT`, so every M8E texture call
site is unaffected) — the demo passes `VK_IMAGE_ASPECT_DEPTH_BIT`
explicitly for the depth image. `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`,
one mip level, one array layer — no depth data ever needs CPU access,
so no staging/upload path applies here at all.

### Depth Lifetime (M8F)

**The depth image IS swapchain/extent-dependent — unlike a texture.**
Extending the swapchain-dependent/not-dependent split from M8D/M8E:

- **Swapchain-dependent** (recreated on every resize): `VulkanSwapchain`
  (images/views), the depth image (`VulkanImage`, sized to the current
  extent), `VulkanFramebuffers` (which wraps both).
- **Not swapchain-dependent** (created once, survive every resize): the
  vertex buffer, the index buffer, `VulkanGraphicsPipeline`,
  `VulkanRenderPass` (its depth *format* is fixed at startup, same as
  the color format — only the depth *image itself* is extent-bound),
  the texture image/view/sampler, the descriptor pool/set.

`recreateSwapchain()` in `vulkan_present_demo.cpp` destroys, in order,
`framebuffers` → `depthImage` → `swapchain` (framebuffers first,
since it references both the other two's views), then reconstructs
`swapchain` → `depthImage` (at the new extent) → `framebuffers` — the
same destroy-before-construct policy every swapchain-dependent resource
in this backend already follows.

### Render-Pass Depth Attachment (M8F)

`VulkanRenderPass` gained a second constructor parameter, `depthFormat`,
and a second `VkAttachmentDescription` (attachment index 1): `loadOp =
CLEAR`, `storeOp = DONT_CARE` (nothing needs the previous frame's depth
contents), no stencil load/store regardless of whether the chosen
format happens to carry a stencil component, `initialLayout =
UNDEFINED`, `finalLayout = DEPTH_STENCIL_ATTACHMENT_OPTIMAL`. The
subpass's `pDepthStencilAttachment` references it. The existing subpass
dependency was extended to also gate on
`VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT` (where the depth clear/
test/write actually happens) and
`VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT`, for the same reason the
original color-only dependency existed: the implicit layout transition
must not be allowed to happen before the depth image is genuinely ready
to be written to.

### Framebuffers With Depth (M8F)

`VulkanFramebuffers` gained a `depthImageView` parameter: every
framebuffer now attaches `{colorView, depthImageView}` in that order
(matching `VulkanRenderPass`'s attachment order exactly). Unlike the
color view (one per swapchain image), the **same single depth image
view is shared across every framebuffer** — only one frame is ever
being rasterized at a time, so one depth image suffices regardless of
swapchain image count.

### Depth Compare / Clear Values (M8F)

Standard, deliberate pairing: clear depth **1.0** (the farthest
possible value) with `depthCompareOp = VK_COMPARE_OP_LESS`. A fragment
passes the depth test only if its depth is *less than* what's currently
stored — so the first thing drawn at a pixel always passes (anything is
less than the 1.0 it was cleared to), and anything drawn later at that
same pixel only overwrites it if it's genuinely closer. No depth bounds
test, no stencil test — neither is needed. `depthWriteEnable = true`,
so every passing fragment updates the stored depth, which is what lets
the *next* draw call correctly test against it.

### Transform Upload Method (M8F)

**Push constants**, not a uniform buffer — `MvpPushConstants`
(`VulkanPushConstants.hpp`): one `Mat4 mvp` (already multiplied down on
the CPU: `Projection * View * Model`) plus one `Vec4 tint`, 80 bytes
total, safely under the 128-byte minimum every Vulkan implementation
guarantees for push constants. Chosen because it keeps M8F
significantly simpler than a uniform buffer would (no additional
descriptor binding, no buffer allocation/update dance for data that's
recomputed every draw anyway) while still being fully correct for
exactly what this milestone needs: two draws, two different (Model,
tint) pairs, updated via `vkCmdPushConstants` immediately before each
`vkCmdDrawIndexed`. **This is deliberately temporary** — real per-object
model transforms and per-frame camera data will very likely eventually
need a uniform buffer (Model changes per-object far more often, and per-
frame camera data is naturally shared across many draws, neither of
which push constants scale well to), but that design wants real
evidence from more than two fixed objects to get right, the same
reasoning M8D/M8E already applied to `BufferDesc`/`TextureDesc`. The
pipeline layout gained one `VkPushConstantRange`
(`VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT`, since the
vertex shader reads `mvp` and the fragment shader reads `tint`).

### Resize Behavior (M8F)

Projection is **recomputed every frame** from the swapchain's *current*
extent (`aspect = extent.width / extent.height`, inside the render
loop) — there is no separate "update the stored projection" step to
forget after a resize; the very next frame's projection is simply
already correct, by construction. The view matrix and both quads'
model matrices, in contrast, never change (the camera and the two
objects are all fixed for the whole demo). Depth-dependent resources
(the depth image, the framebuffers) are destroyed and rebuilt inside
`recreateSwapchain()`, at the new extent, exactly like the swapchain
itself. Minimize handling is unchanged from M8B — the existing wait-
for-nonzero-extent loop, which already runs before any swapchain (and
now depth image) recreation, means a zero-sized depth image is never
attempted; nothing new was needed for this.

### Exact Visual Proof That Depth Testing Works (M8F)

Two copies of the same shared quad (same vertex/index buffers, same
texture), each placed by its own Model matrix:

- **Near**: translated to `(0, 0, -1.5)`, tint `(1, 1, 1, 1)`
  (untinted), submitted **first**.
- **Far**: translated to `(0.4, 0.4, -2.0)`, tint `(1, 0.35, 0.35, 1)`
  (red), submitted **second**.

The diagonal `(0.4, 0.4)` offset on the far quad is deliberate, not
incidental: an earlier version of this demo placed both quads at the
same `(x, y)`, differing only in `z`. That version's far quad ended up
*entirely* hidden behind the (larger, since closer) near quad's
on-screen footprint — a technically valid but visually ambiguous proof,
since "nothing red-tinted is visible" is a much weaker signal to a
human viewer than "the near quad's own color is visible everywhere the
two overlap, and the far quad's red tint is independently visible where
they don't." The diagonal offset guarantees a genuine partial overlap:
each quad has both a shared region and its own independent, non-
overlapping region, confirmed by screenshot — the lower-left region
shows the near quad's untinted checkerboard/color gradient (including
across the entire overlap area, where the far quad's red tint is
correctly absent), and the upper-right region shows the far quad's
red-tinted checkerboard on its own. Without depth testing, submitting
near first and far second would let far's later draw paint its red tint
over near in the overlap region; with depth testing (LESS, write-
enabled), near's already-written, smaller depth values correctly reject
far's fragments there instead — exactly what the screenshot shows, both
before and after two resizes and two full minimize/restore cycles.

### Culling (M8F)

Unchanged from M8C: `cullMode = VK_CULL_MODE_NONE`. The brief explicitly
allowed leaving culling disabled if it would complicate the depth
proof; since M8F's actual goal is proving depth testing (not winding
order), and introducing culling risks a winding-order bug masquerading
as (or obscuring) a depth-testing bug, it stays off. No winding-order
verification was performed this milestone as a result — deferred to
whichever future milestone actually turns culling on.

### NullRenderDevice / Runtime / Generic API

All untouched, exactly as the brief required. `RenderingTests` still
pass unchanged; `AREngineSandbox.exe` still runs on `NullRenderDevice`.
No generic depth-texture abstraction was added to the public
`Rendering` API — the depth attachment is Vulkan rendering
infrastructure specific to this backend's render pass, not an asset
texture a `TextureDesc`/`CreateTexture` caller would ever request
directly, and no evidence from M8F suggests otherwise.

### Validation Results (M8F)

`ARENGINE_ENABLE_VULKAN=ON` (default): full `/W4 /WX` clean build,
`ctest` 9/9 (`VulkanTests` gained 2 new depth-format-selection checks
and an updated position-attribute-format check; `CoreTests` gained 3
new checks — `TestLookAtRH`, `TestPerspectiveRH_ZO` (renamed from
`TestPerspectiveVulkanRH` in the post-M8F Core/Vulkan clip-space split
— see that section above), `TestModelViewProjectionComposition`).
`ARENGINE_ENABLE_VULKAN=OFF`: full build succeeds, no Vulkan/shader
targets present, `ctest` 8/8 (no `VulkanTests`; `CoreTests`, which is
not Vulkan-gated, still includes and passes the new camera-math checks).

`arengine_vulkan_present_demo` run against real hardware (NVIDIA
GeForce RTX 3060 Laptop GPU): window opens, two overlapping quads at
different depths render with the near quad correctly in front
everywhere the two overlap (see "Exact Visual Proof" above), confirmed
by screenshot both initially and after two resizes plus two full
minimize/restore cycles (with the depth proof still holding correctly
after every recreation, and the texture unaffected), closes cleanly.
**Zero validation errors or warnings** — nothing needed fixing during
M8F's development; depth image memory binding, the render pass's
depth-attachment layout transitions, the depth-compare/write pipeline
state, and object destruction order were all correct on the first real
hardware run (after the visual-clarity adjustment to the depth-proof
geometry described above, which was a scene-design change, not a
validation fix — the original nested-overlap version also produced zero
validation errors).

- Selected depth format: **`VK_FORMAT_D32_SFLOAT`** (value 126)
- Transform upload method: **push constants** (`MvpPushConstants`: one
  `Mat4` + one `Vec4`, 80 bytes)
- Near/far/FOV: **near = 0.1, far = 10.0, vertical FOV = 60°**
  (`π/3` radians)

### What's Deferred to M8G+

No movable camera, no `Input` camera control, no `Scene` integration,
no `MeshAsset`, no model loading, no glTF, no lighting, no normals, no
PBR, no shadow mapping, no OpenXR, no MSAA, no mipmaps, no anisotropy,
no material system, no render graph. `Core::Math::LookAtRH`/
`PerspectiveRH_ZO` are genuinely general-purpose (not Vulkan-private)
and are expected to be reused as-is by a future camera abstraction;
`ApplyVulkanYFlip` and everything else new this milestone
(`VulkanDepthFormat`, `MvpPushConstants`, the depth-testing pipeline
state) stays Vulkan-private, same reasoning as every other
backend-specific addition
since M8C — two fixed objects and one fixed camera is not enough
evidence to design a generic camera/transform-upload system from.

No other architectural issues were discovered — depth format selection,
depth image creation, the render pass's depth attachment, depth-tested
rendering, and push-constant transform upload all worked correctly on
the first real hardware run, with zero validation warnings to
investigate.

## 22. M8G Implementation Notes

M8G introduces AREngine's first real `Camera` abstraction and uses it,
via a small demo-private controller, to move through and look around
the M8F 3D scene with WASD + click-drag mouse look — no `SceneRenderer`,
no general mesh renderer, no model loading, and no OpenXR yet.

### Camera Ownership and Module Placement (M8G)

`Scene::Camera` (`engine/scene/include/AREngine/Scene/Camera.hpp`) is a
plain, backend-independent struct: `verticalFovRadians`, `nearZ`,
`farZ`, and a private `m_aspectRatio` behind `SetAspectRatio`/
`GetAspectRatio`. It contains **no position or orientation of its
own** — those live on a separate `Scene::Transform`, passed into
`GetViewMatrix(transform)` as a parameter — and **no Vulkan types
anywhere** (no `VkDevice`, `VkBuffer`, `VkDescriptorSet`). It lives in
`Scene`, not `Core`: `Core` stays limited to math primitives/helpers
(`Mat4`, `Quaternion`, `LookAtRH`, `PerspectiveRH_ZO`), and a `Camera`
is a scene-level *concept* built out of those primitives, not a
primitive itself — the same reasoning that already keeps `Transform`
out of `Core`. There is no `VulkanCamera`; the Vulkan demo owns a plain
`Scene::Camera` value directly.

### Camera vs Transform (M8G)

In simple English: **`Transform` is where the camera is and which way
it's facing; `Camera` is what kind of lens it has.** Moving or rotating
the camera means changing a `Transform` (`position`, `rotation`) —
`Camera` itself never moves. `Camera` only holds the handful of
numbers that describe the "lens": how wide a field of view it sees
(`verticalFovRadians`), how close/far it can see (`nearZ`/`farZ`), and
the aspect ratio of the screen it's rendering to. This split exists
because a `Transform` is a generic, reusable "where and which way"
building block already used for ordinary scene objects (the floor and
quads in this same demo use one) — a `Camera` reuses it instead of
duplicating position/orientation fields, so there is exactly one way
to represent "where something is and which way it's facing" in the
engine.

### View Matrix Generation (M8G)

`Camera::GetViewMatrix(transform)` does not implement its own
quaternion-to-view-matrix formula. It instead computes a synthetic
look-at target — `transform.position + transform.GetForward()` — and
calls the existing `Core::Math::LookAtRH(eye, target, up)` from M8F,
with `up = Core::Math::kWorldUp`. `Transform::GetForward()` (new this
milestone, alongside `GetRight()`/`GetUp()`, in
`engine/scene/include/AREngine/Scene/Transform.hpp`) is
`Core::Math::Rotate(rotation, kWorldForward)` — the world-forward axis
rotated by the transform's orientation. In simple English: **the view
matrix answers "what does the world look like from here, facing this
way?"** by re-expressing every point in the scene relative to the
camera's position and facing direction, so that after the view
transform, the camera is effectively sitting at the origin looking down
its own forward axis — the same job `LookAtRH` already did for M8F's
fixed camera, just fed a target computed from a rotating `Transform`
instead of a hard-coded point.

### Projection Ownership (M8G)

`Camera::GetProjectionMatrix()` returns
`Core::Math::PerspectiveRH_ZO(verticalFovRadians, m_aspectRatio, nearZ,
farZ)` **unflipped** — the same generic, backend-neutral projection
helper from the M8F Core/Vulkan clip-space split (see section 21
above), not a Vulkan-named function, and `Camera` does not know Vulkan
exists. The Vulkan-specific Y flip is applied only in the demo layer:
`ApplyVulkanYFlip(camera.GetProjectionMatrix())`, exactly mirroring how
M8F already called it on `PerspectiveRH_ZO`'s result directly. This
keeps the same boundary the M8F cleanup established: `Camera` (like
`Core`) stays graphics-API-agnostic; only the Vulkan demo composes it
with a Vulkan-specific correction.

### Resize Behavior (M8G)

`SetAspectRatio` is called once per frame in the demo's render loop,
from the swapchain's current extent (`aspect = extent.width /
extent.height`) — the same "recomputed every frame from current state,
nothing to remember to update after a resize" pattern M8F already used
for projection. `GetProjectionMatrix()` then already reflects the
correct aspect ratio on the very next frame after any resize, with no
separate resize-handling code path.

### Quaternion Composition and Rotation (M8G)

Two small, genuinely-needed additions to
`engine/core/include/AREngine/Core/Math/Quaternion.hpp`, both
`constexpr`, both previously explicitly deferred since M5 pending an
actual need:

- `operator*(Quaternion, Quaternion)` — Hamilton product, `a * b`
  meaning "`b` applied first, then `a`", matching `Mat4`'s existing
  TRS composition convention.
- `Rotate(Quaternion, Vec3)` — rotates a vector by a quaternion, using
  the optimized `q * (0, v) * q⁻¹` expansion
  (`t = Cross(qv, v) * 2; return v + t*q.w + Cross(qv, t);`) rather
  than a naive full quaternion-quaternion multiply.

`Transform::GetForward/GetRight/GetUp` and the demo controller's
yaw/pitch-to-orientation composition (below) are what actually needed
these; nothing was added speculatively.

### Movement Controls and Speed (M8G)

- **W/A/S/D** — move along the camera's **current full orientation**
  (including pitch), not always world `-Z`: pressing W moves toward
  wherever the camera is currently looking, even after turning or
  looking up/down. This is the specific, explicitly-tested behavior
  the milestone called out (see `TestComputeNewPositionFollowsRotatedOrientation`,
  `tests/demo_camera_controller_tests.cpp`).
- **Space / Left Ctrl** — move along **world** `+Y`/`-Y` regardless of
  the camera's current pitch (verified by
  `TestComputeNewPositionUpUsesWorldUpRegardlessOfPitch`) — the
  standard "editor fly camera" convention, distinct from an
  FPS-style character controller where up/down might instead be
  jump/crouch.
- Diagonal input (e.g. W+D held together) is normalized so combined
  movement speed never exceeds `moveSpeedMetersPerSecond`
  (`TestComputeNewPositionDiagonalIsNormalized`).
- **`moveSpeedMetersPerSecond = 4.0`** — movement is
  `position += direction * speed * deltaTimeSeconds`, so speed is
  frame-rate independent by construction (`TestComputeNewPositionNoInputIsStationary`
  confirms zero movement when no key is held, even at `dt = 1s`).

### Mouse Look: Click-Drag, Not Cursor Capture (M8G)

Look is driven only while the **right mouse button is held** (queried
via `InputSystem::IsMouseButtonDown(MouseButton::Right)`), using
`InputSystem::GetMouseDelta()` while held. AREngine's `Platform` layer
does not currently support raw input or cursor capture/confinement
cleanly, and the milestone explicitly permitted falling back to
click-drag look rather than expanding `Platform` into raw-input
territory for this — that expansion was explicitly out of scope unless
genuinely necessary, and click-drag look is sufficient to prove camera
movement/look-around works. **`lookSensitivityRadiansPerPixel =
0.0025`.**

**Sign convention** (`DemoCameraController::ApplyLook`):
`yawRadians -= mouseDeltaX * sensitivity;` and `pitchRadians -=
mouseDeltaY * sensitivity;`. Both are subtractions, not additions —
derived from the already-proven fact (`core_tests.cpp`) that rotating
world `Right` (`+X`) by `+90°` around `Up` lands on `Forward` (`-Z`).
Making mouse-right feel like "the camera turns right" therefore
requires *decreasing* yaw as `mouseDeltaX` increases; since
`Platform`'s mouse-delta convention is `+Y = down`, making mouse-down
feel like "look down" likewise requires *decreasing* pitch as
`mouseDeltaY` increases. Verified both by reasoning from first
principles and empirically during manual GPU validation (dragging the
mouse right visibly turned the view right; dragging down visibly
looked down).

**Pitch is clamped to ±89°** (`kPitchLimitRadians ≈ 1.55334303f`,
just under `π/2`) to avoid the camera flipping past straight up/down —
verified by `TestApplyLookClampsPitch` with deliberately extreme
(1,000,000-unit) deltas, checking the result lands exactly at the
limit, not beyond it.

### Quaternion Storage / Yaw-Pitch Composition (M8G)

`Quaternion` remains the engine's one canonical rotation
representation — `Transform::rotation` is a `Quaternion`, never a
yaw/pitch pair. `yawRadians`/`pitchRadians` exist **only** as private
construction inputs inside the demo-private `DemoCameraController`,
converted to a `Quaternion` on demand via
`GetOrientation()`:
`Quaternion::FromAxisAngle(kWorldUp, yawRadians) *
Quaternion::FromAxisAngle(kWorldRight, pitchRadians)` — pitch applied
first (locally), yaw applied last (in world space), the standard
FPS-camera composition. This composition cannot introduce roll by
construction, which mattered when diagnosing an otherwise-unexpected
diagonal line during manual testing (see "Validation Results" below).

### Delta Time (M8G)

The Vulkan demo is not built on `Runtime`/`Frame::FrameDriver` (it's a
standalone manual-test executable, like the rest of the M8B+ demos),
so per the milestone's own guidance to use "the smallest temporary
integration" rather than restructuring `Runtime` just to satisfy the
demo, `Platform::SteadyClock` is used directly:
`clock.Tick()` once per loop iteration, and the resulting
`deltaTimeSeconds` is passed straight into
`DemoCameraController::Update`. No new abstraction was introduced.

### Input Integration (M8G)

The demo reuses `InputSystem` exactly the way `Runtime.cpp` already
does — no direct Win32 queries, no `GetAsyncKeyState`, no duplicate
Win32 input system in the demo:

- `Input::InputSystem inputSystem;` is declared **first** in `main()`,
  before `Window`, so it is destroyed **last** — the same ordering M7
  established after discovering a late `WM_KILLFOCUS` during
  `~WindowsWindow` could otherwise call `OnEvent` on an
  already-destroyed `InputSystem`.
- Every window event is forwarded unconditionally to
  `inputSystem.OnEvent(event)` in the event callback, including
  focus-loss events — `InputSystem` already clears all held keys on
  focus loss (M7 behavior, untouched by M8G).
- `inputSystem.BeginFrame()` is called before `PollEvents()` every
  loop iteration, matching `Runtime.cpp`'s ordering.

No reusable event-routing helper was extracted from `Runtime` — the
few lines involved were small enough that copying the established
pattern directly was the smaller, clearer change; extracting a shared
helper was judged not genuinely needed for one demo call site.

### Demo Scene (M8G)

A small, hard-coded `SceneObject{ Scene::Transform transform; Vec4
tint; }` list (`BuildDemoScene()`, `tests/vulkan_present_demo.cpp`),
all sharing the same quad vertex/index buffer and texture from M8D/M8E
— **temporary test content only**, not `MeshAsset`/model loading or a
general world/level system:

- **Floor**: one large quad, laid flat (rotated to lie in the
  `X`/`Z` plane), tinted white.
- **Four upright quads** at different distances and lateral offsets
  from the camera's start position, each with a distinct tint, so
  movement and look-around visibly change occlusion/relative size/
  perspective between them as the camera passes through the scene.

Camera starts at `position = (0, 1, 5)`, identity orientation (looking
down world `-Z`, toward the quads), `nearZ = 0.1`, `farZ = 100.0`,
default `verticalFovRadians` (60°, unchanged from `Camera`'s default).

### Push-Constant Review (M8G)

Still `MvpPushConstants { Mat4 mvp; Vec4 tint; }`, 80 bytes total,
computed per-object on the CPU (`viewProjection *
object.transform.ToMatrix()`) and pushed immediately before each
object's draw call — unchanged in shape from M8F, just now looped over
five objects instead of two, and fed a per-frame `viewProjection` that
changes as the camera moves instead of a fixed one. Still safely under
the 128-byte minimum every Vulkan implementation guarantees. Per the
milestone's requirement to review against the device's actual limit
(not just the guaranteed minimum), an `AR_ASSERT_MSG` checks
`physicalDevice.properties.limits.maxPushConstantsSize >=
sizeof(MvpPushConstants)` at startup — on the development machine's
GPU this limit is far larger than 80 bytes, so the assert is
effectively a documented safety margin, not a live constraint at this
object count. No uniform-buffer migration was made — five fixed
objects and one camera is still not enough evidence to design that,
same reasoning M8F already applied.

### Temporary Demo-Controller Decisions (M8G)

`ARDemo::DemoCameraController`
(`tests/DemoCameraController.hpp`, demo-private, zero Vulkan
dependency, only `Core`/`Platform`/`Scene`/`Input`) intentionally
stays **separate from `Scene::Camera`** rather than folding its
yaw/pitch/WASD logic into the permanent `Camera` class — `Camera` is
meant to stay small and reusable as pure lens data
(`GetViewMatrix()`, `GetProjectionMatrix()`, `SetAspectRatio(...)`,
explicitly no more than that this milestone: no frustum culling, no
orthographic mode, no jitter, no stereo views, no camera stack, no
post-processing settings), while `DemoCameraController` mixes in
`Input` *policy* (which keys mean what, mouse sensitivity, movement
convention) that is specific to this one manual-test demo and has no
reason to constrain how a future gameplay/editor camera controller —
or an XR head-pose-driven "camera" — might want to move instead. Its
pure-logic pieces (`ApplyLook`, `GetOrientation`, `ComputeNewPosition`)
deliberately take no `InputSystem` parameter, so they're directly
unit-testable without a real window or GPU; only `Update` touches
`InputSystem`, translating raw input into calls to the pure methods.

### Future XR Head Pose vs. Desktop Camera (M8G)

Documented distinction, not implemented this milestone: today, the
desktop `Camera`'s paired `Transform` **is** the viewer's pose — moved
by `DemoCameraController` reading keyboard/mouse. A future OpenXR
integration will **not** work this way: head pose will come from
`XRFrameDriver`/OpenXR's own per-frame tracked pose, not from any
keyboard/mouse controller pretending to be a headset. `Camera` was
deliberately kept free of any assumption that its `Transform` is
always driven by `DemoCameraController` specifically — it only ever
consumes whatever `Transform` it's given. The eventual render path
should be capable of receiving view matrices **from a `FrameDriver`**
(desktop or XR) rather than *always* deriving them from a scene
`Camera` object; this routing is not built yet (no `SceneRenderer`
exists to route into), and `Camera`/`DemoCameraController` were not
contorted to simulate it prematurely.

### Validation Results (M8G)

`ARENGINE_ENABLE_VULKAN=ON` (default): full `/W4 /WX` clean build,
`ctest` 10/10 (new `DemoCameraControllerTests` — 8 pure-logic checks
covering default orientation, yaw/pitch sign convention, pitch
clamping, stationary-when-idle, forward movement at default and
rotated orientation, diagonal-movement normalization, and
pitch-independent world-up movement; `CoreTests` gained
`TestQuaternionMultiplication`/`TestQuaternionRotate`; `SceneTests`
gained `TestTransformDefaultForwardRightUp`,
`TestTransformForwardAfterYaw`, `TestCameraDefaults`,
`TestCameraSetAspectRatio`, `TestCameraViewMatrixFromTransform`).
`ARENGINE_ENABLE_VULKAN=OFF`: full build succeeds, no Vulkan/shader
targets present, `ctest` **9/9** (no `VulkanTests`;
`DemoCameraControllerTests` is registered unconditionally in
`tests/CMakeLists.txt`, not gated behind `ARENGINE_ENABLE_VULKAN`,
since `DemoCameraController` itself has no Vulkan dependency — this is
why the no-Vulkan total grew from M8F's 8/8 to 9/9 rather than staying
the same).

`arengine_vulkan_present_demo` (title: "AREngine M8G Vulkan Camera
Demo") run against real hardware (NVIDIA GeForce RTX 3060 Laptop
GPU): window opens showing the floor and four quads with correct
perspective/depth from the starting position; holding **W** moves the
camera forward into the scene (objects grow larger/closer, a farther
quad becomes visible) with movement visibly proportional to elapsed
time, not frame count; holding the **right mouse button** and dragging
correctly turns the view (drag right → view turns right; drag down →
view looks down) with pitch never overshooting past-vertical; window
resize, minimize, and restore all correctly recompute the aspect ratio
and preserve the camera's current position/orientation, with the
depth-tested scene continuing to render correctly at the new size (no
distortion, no stale swapchain state). **Zero validation errors or
warnings** throughout every test — mouse look, movement, resize, and
minimize/restore all validated clean on the first real-hardware run.

**Focus-loss stuck-key scenario explicitly tested and confirmed
correct**: W held down, focus deliberately stolen to another window
mid-press (triggering `WM_KILLFOCUS` while W was still logically
"held"), W then released while the demo window did not have focus,
focus returned to the demo, and two screenshots taken one second apart
after regaining focus were pixel-identical — proving the camera did
not drift forward either while unfocused or after regaining focus,
i.e. M7's existing focus-loss key-clear behavior continues to prevent
a stuck-movement-key bug with the new camera controller layered on
top, with no changes needed to `InputSystem` itself.

**One non-bug observation worth recording**: during the mouse-look
test, after a combined yaw+pitch drag, the floor's far edge appeared as
a diagonal line on screen rather than staying horizontal. Reasoned
through without any code change: yaw-then-pitch quaternion composition
cannot introduce roll by construction (verified above), and the floor
is a *finite* rectangular quad, not an infinite ground plane — viewing
a finite rectangle's straight edge at an oblique combined yaw+pitch
angle correctly produces a diagonal line in screen space under
perspective projection. This is expected, correct projected geometry,
not a rendering or rotation bug.

- Movement speed: **4.0 m/s** (`moveSpeedMetersPerSecond`)
- Look sensitivity: **0.0025 radians/pixel** (`lookSensitivityRadiansPerPixel`)
- Pitch clamp: **±89°** (`kPitchLimitRadians ≈ 1.55334303` rad)
- Push-constant size: **80 bytes** (`MvpPushConstants`: `Mat4` + `Vec4`),
  asserted against the device's actual `maxPushConstantsSize`, not just
  the 128-byte guaranteed minimum

### What's Deferred to M8H+

No `SceneRenderer`, no general mesh renderer, no model loading, no
glTF, no lighting, no PBR, no shadow maps, no physics, no collision, no
OpenXR, no cursor raw-input/capture system, no camera component
registry, no ECS, no frustum culling, no uniform-buffer architecture.
`Scene::Camera` and `Transform::GetForward/GetRight/GetUp` are
genuinely general-purpose and expected to be reused as-is by future
milestones; `DemoCameraController` stays demo-private, same reasoning
M8B–M8F already applied to every other manual-test-only piece of this
demo.

No architectural issues were discovered — camera math, view/projection
composition, input integration, delta-time-based movement, resize
handling, and the focus-loss key-clear interaction with the new
controller all worked correctly on the first real-hardware run, with
zero validation warnings to investigate.

## 23. M8H Implementation Notes

M8H introduces AREngine's first reusable mesh representation: CPU-side
geometry (`Rendering::MeshData`) separated from Vulkan GPU mesh
resources (`Vulkan::VulkanMesh`), so geometry can be defined once and
drawn many times with different Model transforms — replacing the
hard-coded "one shared quad" the Vulkan demo had used since M8D with a
1-meter cube uploaded once and drawn six times (a floor plus five
instances).

### CPU Mesh Data Placement (M8H)

`Rendering::MeshVertex`/`MeshData`
(`engine/rendering/include/AREngine/Rendering/MeshData.hpp`) live in
`Rendering`, not `Assets`. The deciding factor: this is purely runtime
geometry data with no file format, no `AssetId`, and no loading step —
exactly what `Assets` (`AssetId`, `TextAsset`/`BinaryAsset`, caching by
path) is not about. `Rendering` was already the layer that had (until
now, privately) needed exactly this shape of data
(`Vulkan::Vertex` — see "Vertex Format Review" below), and placing the
now-generic version in the same module it was extracted from, rather
than in a module that has never touched geometry, keeps the layering
simple: `Rendering` depends only on `Core`, same as before, and gains
no new dependency. `MeshData` has zero Vulkan types, zero Scene
dependency, and zero `AssetId` requirement — a future `MeshAsset`
(loaded from glTF/OBJ) would most likely *produce* a `MeshData` as its
in-memory representation, not replace it.

### Vertex Format Review (M8H)

Through M8G, `Vulkan::Vertex` (`src/vulkan/VulkanVertex.hpp`) was a
Vulkan-private struct: `Vec3 position; Vec3 color; Vec2 uv;`. M8H's
`Rendering::MeshVertex` needed exactly the same three fields — the
same shape independently required twice is the actual evidence this
milestone needed. Rather than keep two identical structs and convert
between them (copying every vertex into a throwaway Vulkan-private
array on every mesh upload, for zero benefit), `Vulkan::Vertex` was
retired: `VulkanVertex.hpp/.cpp` now exports two free functions,
`GetVertexBindingDescription()`/`GetVertexAttributeDescriptions()`,
that operate directly on `Rendering::MeshVertex` via `offsetof`. No
`VkFormat` or byte offset is exposed on any public `Rendering`
header — `MeshVertex` itself stays plain engine data; only these two
Vulkan-private functions (and the shaders' `layout(location = ...)`
declarations, which they must keep matching) know Vulkan's vertex-input
description shape. `VulkanGraphicsPipeline` was updated to call the
renamed free functions instead of `Vertex::GetBindingDescription()`/
`Vertex::GetAttributeDescriptions()`.

### Mesh Validation (M8H)

`MeshData::IsValid()` (inline, in `MeshData.hpp`) is deliberately
minimal — not a general mesh validator (no degenerate-triangle checks,
no duplicate-vertex detection, no winding/manifold checks): just
`vertices` non-empty, `indices` non-empty, and every index `<
vertices.size()`. `Vulkan::VulkanMesh`'s constructor asserts this
(`AR_ASSERT_MSG`) before touching the data — invalid geometry is a
programmer error to catch immediately, not a runtime condition an
upload path should try to recover from.

### Procedural Test Meshes (M8H)

`Rendering::CreateQuadMesh()`/`CreateCubeMesh()`
(`engine/rendering/include/AREngine/Rendering/ProceduralMesh.hpp`,
implemented in `src/ProceduralMesh.cpp`, built unconditionally — no
Vulkan dependency) generate geometry in-memory; no downloaded models,
no glTF/OBJ, no `MeshAsset`. `CreateQuadMesh()` reproduces the exact
1x1m quad (4 vertices, 6 indices, same UV mapping) the Vulkan demo
hard-coded from M8D through M8G — now reusable and backend-independent
instead of demo-private. `CreateCubeMesh()` builds a 1x1x1 meter cube
(see `docs/WORLD_CONVENTIONS.md` for "1 unit = 1 meter"), centered on
the origin (`x`/`y`/`z` each spanning exactly `-0.5..+0.5`), with **24
vertices, not 8**: each of the 6 faces gets its own 4 corners, because
sharing corner vertices across faces would force one shared UV per
corner, which cannot correctly map a texture independently onto the 2
or 3 faces meeting at that corner. 36 indices (6 faces x 2 triangles x
3). Every vertex color is white (`(1,1,1)`) — these are neutral,
reusable primitives; the M8D-era per-vertex color gradient was specific
to that one now-retired demo quad, proven separately, and unrelated to
these generators. Every face is built with a consistent right-handed
`R x U = N` corner ordering (`R`/`U` are the face's local right/up
axes, `N` its outward normal), matching `CreateQuadMesh`'s own +Z-face
winding exactly — see "Back-Face Culling" below for why this specific
convention matters.

### VulkanMesh Ownership (M8H)

`Vulkan::VulkanMesh` (`src/vulkan/VulkanMesh.hpp/.cpp`) owns exactly
two `VulkanBuffer`s (vertex, index — reusing the existing class, not a
new one) plus the index count needed to draw. No `VkBuffer` is exposed
on any public `Rendering` header — reached only through Rendering's
private `src/vulkan/` implementation, same as `VulkanBuffer`/
`VulkanImage` before it. Not copyable or movable, same discipline as
every other owned Vulkan resource in this backend. The conceptual
pipeline this milestone introduces:

```
MeshData (CPU, backend-independent)
    |  upload (CreateVulkanMesh)
    v
VulkanMesh (GPU, Vulkan-private)
    |
    v
Bind() + Draw()
```

`Bind()` issues `vkCmdBindVertexBuffers`/`vkCmdBindIndexBuffer`;
`Draw()` issues one `vkCmdDrawIndexed` covering the mesh's full index
range. Splitting these (rather than one combined `Draw()`) is what
lets the demo bind once per frame and draw many times — see "Multiple
Instances" below.

### Upload Flow (M8H)

`CreateVulkanMesh(physicalDevice, device, commandPool, queue,
meshData)` is a thin factory mirroring `CreateDeviceLocalBuffer`/
`CreateTextureFromPixels`'s existing shape: validate, upload, return an
owned resource via `std::unique_ptr` (`VulkanMesh` is non-movable, same
reasoning as those). `VulkanMesh`'s constructor does the actual work:
asserts `meshData.IsValid()`, then calls the existing
`CreateDeviceLocalBuffer` (M8D's staging-buffer upload path,
unchanged) twice — once for vertices, once for indices. Synchronous,
like every other upload in this backend; no async upload was added.

### Multiple Instances (M8H)

The whole point of this milestone: **one** `MeshData` → **one**
`CreateVulkanMesh` call → **one** GPU-resident vertex/index buffer
pair → **many** draws, each with its own Model matrix (and tint) via
push constants. The demo's render loop calls `cubeMesh->Bind()` once
per frame (binding the vertex/index buffers and the texture's
descriptor set is redundant to repeat per-object when every object is
the same mesh), then loops over `sceneObjects`, pushing a fresh
`MvpPushConstants{viewProjection * object.transform.ToMatrix(),
object.tint}` and calling `object.mesh->Draw()` for each. Confirmed via
the manual demo's log: `Cube mesh: 24 vertices, 36 indices, uploaded
once (GPU mesh uploads: 1)` appears exactly once per run, regardless of
resize/minimize/restore cycles (see "Validation Results" below) —
proving the mesh is genuinely uploaded once, not once per object and
not re-uploaded on any subsequent event.

### Back-Face Culling (M8H)

Enabled: `cullMode = VK_CULL_MODE_BACK_BIT`, `frontFace =
VK_FRONT_FACE_CLOCKWISE` (unchanged since M8C — was inert with
`cullMode = NONE` through M8G). The winding convention was derived,
not guessed: AREngine's view space for an identity-orientation camera
keeps world axes unchanged (only translated), and `PerspectiveRH_ZO`
does not flip X or Y — only `ApplyVulkanYFlip` (the Vulkan-layer Y
flip applied to the projection matrix, see section 21's "Core/Vulkan
Clip-Space Split") does. Flipping one screen-space axis reverses
apparent winding, so a triangle wound counter-clockwise in view space
(as seen from the outward-normal side, matching every
`ProceduralMesh.cpp` face by construction) becomes clockwise in actual
Vulkan screen space after the Y flip — exactly matching
`VK_FRONT_FACE_CLOCKWISE`. This was verified two ways: (1) the existing
quad geometry (vertex order `0,1,2,2,3,0` at `(-0.5,-0.5)`,
`(0.5,-0.5)`, `(0.5,0.5)`, `(-0.5,0.5)`) is CCW as seen from `+Z`,
matching this exact derivation, and had already been rendering
correctly (front-facing, undisturbed by culling being off) since M8D;
(2) `CreateCubeMesh`'s six faces were built with the identical `R x U
= N` corner-ordering rule applied consistently to every face, not
independently guessed per face. No winding or projection convention
was changed to "fix" a disappearing-face problem — culling was enabled
once the derivation held, not adjusted reactively.

### Normals (M8H)

Deliberately not added. There is no lighting yet, so no shader stage
would consume a normal attribute — adding one now would be an unused
vertex attribute carried purely in anticipation of a future milestone,
which the project's stated style explicitly avoids. Normals belong to
whichever future milestone introduces lighting.

### Generic RenderDevice Review (M8H)

Reviewed against real evidence from this milestone (buffer uploads,
vertex/index usage, repeated-geometry reuse, indexed draws) — and
concluded the current split is still correct, so nothing was changed:

- **Should `Mesh` be a generic `RenderDevice` concept?** Not yet.
  `RenderDevice::CreateBuffer` already exists and is sufficient in
  principle; the missing piece M8D/M8E's reviews already found (no way
  to supply initial data to a generic buffer/texture) is still
  unaddressed, and M8H doesn't add new evidence about what that upload
  API should look like — it's still exactly one demo's worth of
  evidence (one Vulkan backend, one mesh shape) for a generic upload
  API that would need to work for buffers, textures, *and* meshes
  consistently.
- **Should `RenderDevice` expose `CreateMesh`?** Not yet, for the same
  reason M4 deferred a pipeline/shader API: a generic `RenderDevice`
  method needs evidence from more than one backend's real requirements
  to shape correctly, and Vulkan is still the only backend with real
  geometry. Bundling vertex+index buffers into one handle is a
  reasonable-looking shape, but "reasonable-looking" isn't the bar this
  project has held itself to elsewhere (see M8D/M8E's buffer/texture
  reviews) — a second backend or a real multi-mesh scene (M8I/M8J)
  would be the actual evidence needed.
- **Conclusion**: mesh upload stays a higher-level, Vulkan-private
  concern (`VulkanMesh`/`CreateVulkanMesh`), built *on top of* the
  existing private `VulkanBuffer`/`CreateDeviceLocalBuffer` — exactly
  the same layering `VulkanImage`'s texture upload already uses. `Mesh`
  is a genuinely evidence-based *generic Rendering concept* now (see
  "CPU Mesh Data Placement" above) — that's new. It is **not yet** a
  `RenderDevice` concept — that remains deferred, unchanged from M4's
  original reasoning, now with one more milestone's worth of evidence
  still pointing the same direction rather than a new direction.

### Scene Separation (M8H)

Unchanged from the milestone's explicit requirement: `Scene` does not
depend on `Mesh` or `Rendering`. No `MeshComponent`, no
`RenderableComponent`, no ECS. The Vulkan demo's temporary
`DemoObject{ Scene::Transform transform; Core::Math::Vec4 tint; const
VulkanMesh* mesh; }` (non-owning pointer — the demo's `cubeMesh`
`unique_ptr` is the sole owner) is demo-only, matching the exact shape
the milestone suggested, kept only because it lets the render loop
demonstrate mesh reuse (`object.mesh->Draw(...)`) without introducing
any permanent Scene/Rendering coupling.

### Resource Lifetime (M8H)

The cube mesh's GPU buffers are **not** swapchain-dependent: created
once (alongside the command pool, before the render loop), survive
every `recreateSwapchain()` call untouched, and are destroyed only at
demo shutdown (via `vkDeviceWaitIdle` followed by automatic
destruction in reverse construction order) — the exact same lifetime
category M8D/M8E's vertex/index/texture resources already occupied.
Resize/minimize/restore never rebuilds mesh buffers; confirmed
directly by the manual run's log (see "Validation Results" below).

### Validation Results (M8H)

`ARENGINE_ENABLE_VULKAN=ON` (default): full `/W4 /WX` clean build,
`ctest` **11/11** (new `MeshTests` — 10 pure-logic checks covering
`MeshData::IsValid()`'s four validation rules plus valid-input
acceptance, `CreateQuadMesh`'s vertex/index counts, and
`CreateCubeMesh`'s vertex/index counts, index-range check, and exact
1-meter centered bounds; `VulkanTests`' vertex tests updated to call
the renamed `GetVertexBindingDescription`/`GetVertexAttributeDescriptions`
against `Rendering::MeshVertex`, unchanged in what they verify).
`ARENGINE_ENABLE_VULKAN=OFF`: full build succeeds, no Vulkan/shader
targets present, `ctest` **10/10** (no `VulkanTests`; `MeshTests` is
registered unconditionally in `tests/CMakeLists.txt`, not gated behind
`ARENGINE_ENABLE_VULKAN`, since neither `MeshData` nor
`ProceduralMesh` depend on Vulkan — this is why the no-Vulkan total
grew from M8G's 9/9 to 10/10).

`arengine_vulkan_present_demo` (title: "AREngine M8H Vulkan Mesh
Demo") run against real hardware (NVIDIA GeForce RTX 3060 Laptop
GPU): startup log confirms `Cube mesh: 24 vertices, 36 indices,
uploaded once (GPU mesh uploads: 1)` and `Demo scene: 6 objects (1
floor + 5 cube instances, all sharing 1 mesh)` — matching
`CreateCubeMesh`'s expected 24/36 counts exactly. A resize/minimize/
restore cycle (640x480 -> 1000x700 -> minimized -> restored) produced
six `Swapchain recreated` log lines with **zero validation errors or
warnings**, and — critically — the `Cube mesh:` upload line appears
exactly **once** in the full log, never repeated across any of those
events, directly confirming the mesh buffers are not rebuilt on
resize. The demo closed cleanly via its window's close button both
times it was run, logging `Vulkan presentation demo complete -
shutting down` with no hang and no leftover process.

**Visual on-screen confirmation (cube visibility at different depths,
texture mapping, culling correctness, camera movement/depth from
multiple viewpoints) could not be captured this session**: the
development machine had another application running in exclusive
fullscreen for the full duration of manual validation, which — unlike
a normal windowed application — renders directly to the display and
cannot be screenshotted or brought forward via the usual
window-capture approach without forcibly interrupting that foreground
application. Rather than force focus away from it, visual verification
was skipped in favor of the log-based evidence above (exact expected
vertex/index/object counts, single mesh upload confirmed both at
startup and held steady across every resize/minimize/restore event,
zero validation-layer errors throughout). The geometry and culling
convention were independently derived mathematically (see "Back-Face
Culling" above) rather than tuned to match a screenshot, which reduces
but does not eliminate the risk of an on-screen defect the log
wouldn't reveal (e.g., a texture appearing upside-down, though UV
mapping is unchanged from M8D/M8E's already-visually-confirmed
convention). A follow-up visual pass is recommended once the display
is free, before treating M8H as fully closed out.

- CPU mesh data module: **`AREngine::Rendering`**
  (`MeshData.hpp`/`ProceduralMesh.hpp`)
- Cube vertex count: **24** (6 faces x 4, not shared across faces)
- Cube index count: **36** (6 faces x 2 triangles x 3)
- GPU mesh uploads: **1** (confirmed via log, held steady across
  resize/minimize/restore)
- Cube instances drawn: **5** (plus 1 floor instance, all 6 sharing the
  same uploaded mesh)
- Culling: **`VK_CULL_MODE_BACK_BIT`**, front face
  **`VK_FRONT_FACE_CLOCKWISE`** (unchanged value, now actually enabled)

### What's Deferred to M8I+

No glTF, no OBJ, no model loading, no `MeshAsset`, no normals, no
lighting, no material system, no uniform-buffer architecture, no
`SceneRenderer`, no Scene integration, no ECS, no OpenXR, no physics,
no instancing API (`vkCmdDrawIndexed`'s instance count stays 1 — the
"multiple instances" this milestone proves is multiple *draw calls*
reusing one mesh, not GPU instancing), no indirect draws, no batching,
no GPU-driven rendering. `Rendering::MeshData`/`ProceduralMesh` are
genuinely general-purpose and expected to be reused as-is by future
milestones; `Vulkan::VulkanMesh` stays Vulkan-private, same reasoning
every other backend-specific addition since M8C has followed.

No architectural issues were discovered in the implementation itself —
the CPU/GPU mesh split, upload path, multi-instance draw loop, and
culling all matched their derivations exactly, with zero validation
warnings on the log-confirmed hardware run. The one open item is the
visual-confirmation gap noted above, which is an environmental
limitation of this session, not a defect found in the code.

## 24. M9A Implementation Notes

M9A introduces OpenXR into AREngine for the first time — bring-up only:
discover the loader, create an `XrInstance`, enumerate API layers and
instance extensions, request a head-mounted-display-class `XrSystemId`,
inspect its properties, and shut down cleanly. No `XrSession`, no
graphics binding, no swapchain, no frame loop, no Vulkan/OpenXR
connection — all deferred to later M9 sub-milestones.

### OpenXR Build Option (M9A)

`ARENGINE_ENABLE_OPENXR` (top-level CMake option, **default OFF**) gates
all of OpenXR, mirroring `ARENGINE_ENABLE_VULKAN`'s existing pattern
exactly: when OFF, no OpenXR headers/loader are required anywhere, and
`engine/xr` is just the M0-era placeholder it always was (`XR.cpp`'s
`ModuleName()`). Default OFF (unlike Vulkan's default ON) because,
unlike Vulkan, there is no OpenXR SDK pre-installed on this development
machine, and turning OpenXR on requires network access on first
configure (see below) — a non-XR contributor should never be forced
into that by default. See "Validation Results" below for confirmation
that `ARENGINE_ENABLE_OPENXR=OFF` configures/builds/tests exactly as
before this milestone.

### OpenXR Loader / SDK Acquisition (M9A)

Unlike Vulkan, there is no widely standardized system-wide "OpenXR
SDK" installer on Windows (no `VULKAN_SDK`-style environment variable
convention to rely on) — confirmed by checking this development
machine, which has the Vulkan SDK installed but no OpenXR SDK, no
vcpkg, and nothing under a conventional install path. Given that, and
the brief's explicit instructions to prefer CMake package discovery
and never hard-code a machine-specific path or hand-vendor headers,
`engine/xr/CMakeLists.txt` does:

1. `find_package(OpenXR CONFIG QUIET)` first — if a system or package-
   manager-provided OpenXR SDK is available (e.g. via vcpkg, which
   defines an `OpenXR::openxr_loader` CMake target), it's used as-is.
2. If `OpenXR::openxr_loader` still isn't a target, CMake `FetchContent`
   pulls the official Khronos `OpenXR-SDK` repository (not
   `OpenXR-SDK-Source` — the release repo ships pre-generated headers
   and loader source, confirmed by inspecting it directly, so no Python
   code-generation step is needed), pinned to the exact tagged release
   `release-1.1.62` for reproducibility. `BUILD_API_LAYERS`,
   `BUILD_TESTS`, and `BUILD_CONFORMANCE_TESTS` are all forced OFF
   before fetching — only `BUILD_LOADER` (ON) is needed, so this never
   pulls in Catch2 or the conformance test suite.

Nothing is vendored into this repository — CMake downloads and builds
the loader the same way it would build any other subdirectory, and
nothing under `_deps/` is checked into git. This does mean the first
`ARENGINE_ENABLE_OPENXR=ON` configure requires network access (a `git
clone` of the OpenXR-SDK repository); documented here as the one
Windows-specific toolchain limitation the brief asked to call out if
unavoidable — there was no clean way around it without either vendoring
headers (explicitly disallowed) or requiring a pre-installed SDK this
machine doesn't have.

One incidental toolchain note: the fetched OpenXR-SDK's own CMake
optionally detects this machine's installed Vulkan SDK
(`if(Vulkan_FOUND)`) and includes Vulkan's headers privately inside its
own loader build, for its own internal interop declarations — this is
the *loader's* business, not AREngine's, and was confirmed not to
create any actual coupling: `ARENGINE_ENABLE_OPENXR=ON` with
`ARENGINE_ENABLE_VULKAN=OFF` still configures, builds, and passes every
non-Vulkan test cleanly (see "Validation Results" below) — AREngine's
own `Rendering` module builds zero Vulkan code in that configuration,
and AREngine's `XR` module never references Vulkan at all.

### Requested API Version (M9A)

`AREngine::XR::OpenXR::kTargetApiVersion` (`OpenXRVersion.hpp`) targets
**OpenXR 1.0 core** (`XR_API_VERSION_1_0`), not whatever the newest
fetched header happens to define (1.1.x) — the same "broad compatibility
over the newest available" reasoning M8A already applied when choosing
Vulkan 1.2 over a newer target. Essentially every OpenXR runtime
supports at least 1.0 core; requesting 1.1 blindly would risk failing
instance creation on any runtime that doesn't yet implement it. One
surprising detail worth documenting: `XR_API_VERSION_1_0`'s *patch*
component (as `openxr.h` defines it) tracks whichever header patch
version the code was built against, rather than being pinned to 0 - so
`FormatXrVersion(kTargetApiVersion)` prints `"1.0.62"` (matching the
fetched 1.1.62 header's own patch number), not `"1.0.0"`. Only
major/minor are meaningful; `DecodeXrVersion`/`FormatXrVersion`
(`OpenXRVersion.hpp/.cpp`) mirror `VulkanVersionParts`/
`DecodeVulkanVersion`/`FormatVulkanVersion` exactly, and are pure bit
manipulation over the `XR_VERSION_*` macros — directly unit-tested
without a loader, runtime, or headset.

### API Layer Enumeration (M9A)

`EnumerateApiLayers()` (`OpenXRInstance.hpp/.cpp`) calls
`xrEnumerateApiLayerProperties` — a loader-level query, no `XrInstance`
required — and returns every layer found, for the demo to log. **No
layer is ever enabled**, matching the brief's explicit "do not enable
arbitrary API layers." The standard OpenXR core validation layer
(`XR_APILAYER_LUNARG_core_validation`) is specifically checked for and
reported if present, but still never enabled this milestone: M9A
creates no session, swapchain, or frame loop, so there is very little
for a validation layer to usefully catch yet, and normal execution
never requires it to be installed. On this development machine, zero
layers were found (confirmed by the manual run — see "Validation
Results"), which the demo reports plainly rather than treating as an
error.

### Instance Extension Enumeration (M9A)

`EnumerateInstanceExtensions()` similarly calls
`xrEnumerateInstanceExtensionProperties(nullptr, ...)` (also no
instance required) and logs whatever the active runtime supports. M9A
enables zero extensions — no graphics-binding extension (deferred to
M9C, which will need `xrGetVulkanGraphicsRequirementsKHR` and friends),
no hand tracking, no eye tracking, no passthrough, no spatial anchors.
Unlike the API-layer query, this call genuinely does need a runtime to
answer — on this development machine (no runtime installed), it fails
with `XR_ERROR_RUNTIME_UNAVAILABLE`, confirmed directly by the manual
run's log. Both enumeration functions handle their own failure
gracefully (`AR_LOG_WARNING` + return an empty vector), not an assert —
enumeration failing before an instance even exists is exactly the kind
of "no runtime" scenario this milestone must not crash on.

### Instance Ownership (M9A)

`OpenXRInstance` (`OpenXRInstance.hpp/.cpp`) owns one `XrInstance`,
requesting zero API layers and zero extensions, with an
`XrApplicationInfo` carrying AREngine's name/version (`"AREngine OpenXR
Demo"` / `"AREngine"`, both packed as `major*10000 + minor*100 + patch`
= `100` for 0.1.0 — plain `uint32_t` fields OpenXR does not interpret,
unlike `apiVersion`, which is the real `XrVersion`-typed
`kTargetApiVersion`). Not copyable or movable, destroyed via
`xrDestroyInstance` exactly once, by this object alone — the same
discipline every other owned bring-up handle in this engine follows
(`VulkanInstance`, `VulkanDevice`, etc.).

### Instance Creation Failure Handling (M9A)

**This is the one deliberate departure from every prior bring-up
wrapper's policy.** `VulkanInstance`/`VulkanDevice`/etc. all assert
(via `CheckVkResult`) on any creation failure, because M8A treated "no
Vulkan-capable GPU" as an environment the engine doesn't need to run
in gracefully. OpenXR is different: **"no OpenXR runtime
installed/active" is the *normal* state of most desktop dev machines**
(confirmed directly — this development machine has none), not a
programmer error. `OpenXRInstance`'s constructor therefore does not
assert on `xrCreateInstance` failure at all — it records the raw
`XrResult` (`CreationResult()`) and leaves `IsValid()` false, and it is
entirely up to the caller (the bring-up demo) to decide what that
means and how to report it. `CheckXrResult`
(`OpenXRResult.hpp/.cpp`, mirroring `CheckVkResult`) remains available
and *is* still used for calls made **after** a valid instance/system
already exists, where failure would be genuinely unexpected
(`xrGetInstanceProperties`, `xrGetSystemProperties`) — the fatal-on-
bring-up-failure policy isn't abandoned, just scoped to where it
actually applies.

### System Selection (M9A)

`TryGetHmdSystem(XrInstance)` (`OpenXRSystem.hpp/.cpp`) calls
`xrGetSystem` with `XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY` and, like
`OpenXRInstance`, **does not assert on failure** — unlike Vulkan's
`SelectPhysicalDevice` (M8A), "no HMD connected" is an entirely normal
outcome even when a runtime is active and healthy. It returns a
`SystemRequestResult{found, systemId, rawResult}` so the caller can
inspect exactly why `found` is false.
`IsFormFactorUnavailable(XrResult)` is a small, `constexpr`, directly
unit-tested pure-logic helper that recognizes the two result codes
specifically meaning "this form factor genuinely isn't available right
now" (`XR_ERROR_FORM_FACTOR_UNAVAILABLE`/`_UNSUPPORTED`) as distinct
from any other, more surprising `xrGetSystem` failure.

### System Properties (M9A)

Once `TryGetHmdSystem` succeeds, `xrGetSystemProperties` is called
directly from the demo (no wrapper class needed — `XrSystemId` is not a
resource OpenXR owns, so there's nothing to give RAII to; see "RAII /
Destruction" below) and logs exactly what M9A's brief asked for from
core `XrSystemProperties`, with no extension-specific property chains:
system name, vendor ID, max swapchain image width/height, max
composition layer count, and the two core tracking-capability flags
(`orientationTracking`/`positionTracking`).

### Error Handling (M9A)

`XrResultToReadableString(XrInstance, XrResult)` (`OpenXRResult.hpp/.cpp`,
mirroring `VkResultToString`) is best-effort: if a valid `XrInstance`
is available, it defers to the real `xrResultToString` (which covers
every `XrResult`, including vendor/extension-defined ones, far more
completely than a hand-written switch could); if not (most notably,
when `xrCreateInstance` itself is the failing call, so no instance
exists to ask), it falls back to a numeric `"XrResult(-51)"`-style
representation — directly unit-tested for exactly that no-instance
case. `CheckXrResult` builds on this the same way `CheckVkResult` does,
for the calls where asserting is still correct (see "Instance Creation
Failure Handling" above for the calls where it deliberately is not
used).

### RAII / Destruction (M9A)

M9A owns exactly one real resource: the `XrInstance`, via
`OpenXRInstance`. `XrSystemId` is never separately destroyed — per the
OpenXR spec, it is an opaque identifier valid for the instance's
lifetime, not a resource with its own creation/destruction calls, so
`OpenXRSystem.hpp` deliberately has no `~OpenXRSystem`/RAII class at
all, just a small result struct and a free function. No global XR
singleton and no global mutable `XrInstance` were introduced -
`OpenXRInstance` lives on the demo's stack, exactly like `VulkanInstance`
in `vulkan_demo.cpp`.

### Dependency Trim (M9A)

`engine/xr`'s public link libraries were trimmed from `{Core, Platform,
Frame}` down to **`{Core}`** only. `Platform` and `Frame` were both
linked since the M0 stub, in anticipation of needs that hadn't arrived
yet; M9A is the first real evidence of what XR actually needs, and it
is neither of those — no window/native-handle access (`Platform`) and
no `FrameDriver` implementation yet (`Frame` — `XRFrameDriver` is
explicitly deferred, see "What's Deferred to M9B+" below). This matches
the brief's explicit expected dependency direction (`XR -> Core`, `XR
-> Frame` only "if genuinely required later") and the project's
established evidence-over-speculation pattern (the same reasoning
M8D/M8E/M8H's `RenderDevice` reviews already applied). `runtime/CMakeLists.txt`
still links `AREngine::Platform`/`AREngine::Frame` directly itself
(unrelated to XR's own link graph), so nothing downstream broke — see
"Validation Results" below. `XR` still does not depend on `Scene`,
`Rendering`, `Runtime`, `Assets`, `Input`, or `Editor`, and `Core`
remains entirely unaware that OpenXR exists — no OpenXR type appears
anywhere under `engine/core`.

### No OpenXR Types in Public Engine APIs (M9A)

`AREngine/XR/XR.hpp` (the module's only public header) is completely
unchanged by M9A — it still only declares the M0-era placeholder
`ModuleName()`. Every OpenXR type (`XrInstance`, `XrSystemId`,
`XrResult`, `XrSystemProperties`, etc.) is confined to
`engine/xr/src/openxr/*.hpp`, private implementation headers in
exactly the same sense `engine/rendering/src/vulkan/*.hpp` already are
for Vulkan — reached only by this module's own `.cpp` files and by the
manual bring-up demo/tests, which `#include` them directly by relative
path (`"openxr/OpenXRInstance.hpp"`), never through any public
`AREngine::XR` header.

### Headset Absent Case (M9A)

The manual demo (`tests/openxr_demo.cpp`) explicitly distinguishes
three outcomes, per the brief, rather than treating every failure
identically:

- **A. No OpenXR runtime available** — `OpenXRInstance::IsValid()` is
  false and `CreationResult() == XR_ERROR_RUNTIME_UNAVAILABLE`.
- **B. Runtime exists, no HMD system** — instance creation succeeds,
  but `TryGetHmdSystem`'s result has `found == false` and
  `IsFormFactorUnavailable(rawResult)` is true.
- **C. Runtime + HMD system available** — both succeed; system
  properties are logged.

Each produces a distinct, understandable log message (see
`openxr_demo.cpp`), and in every case the demo returns `0` — no crash,
no assert, no non-zero exit code for what are, on most machines,
completely ordinary states. Case A was directly confirmed on this
development machine, three separate times (default OpenXR-enabled
build, the `/W4 /WX` build, and the OpenXR-ON/Vulkan-OFF build) — see
"Validation Results". Cases B and C could not be exercised on this
machine (no OpenXR runtime is installed to even reach case B, let alone
a real headset for case C) — their code paths were verified by careful
inspection against the exact `XrResult` values the OpenXR 1.1.62
specification defines, not against real hardware. This is a known,
honestly-reported gap, the same kind M8H's session already flagged for
its own visual-verification limitation.

### Automated Tests (M9A)

`tests/openxr_tests.cpp` (`OpenXRTests`, gated behind
`ARENGINE_ENABLE_OPENXR` exactly like `VulkanTests` is gated behind
`ARENGINE_ENABLE_VULKAN`) makes **zero real OpenXR API calls** — only
`DecodeXrVersion`/`FormatXrVersion`, `XrResultToReadableString`'s
no-instance numeric fallback, and `IsFormFactorUnavailable`, all pure
logic over OpenXR's plain C structs/enums as synthetic data. Runs on
any machine with the OpenXR headers available at compile time, without
needing a real loader, runtime, or headset — the same guarantee
`VulkanTests` already makes for Vulkan. Real bring-up (instance/system
creation against a real loader/runtime) is exercised only by the
separate, manual `arengine_openxr_demo`.

### Validation Results (M9A)

`ARENGINE_ENABLE_OPENXR=OFF` (default): unchanged from before this
milestone — full build, `ctest` **11/11**, no OpenXR headers/libs
required anywhere (confirmed: `engine/xr` builds only `XR.cpp`, nothing
under `src/openxr/` is even added to the target).

`ARENGINE_ENABLE_OPENXR=ON` (default `ARENGINE_ENABLE_VULKAN=ON`): the
OpenXR-SDK loader fetches and configures cleanly (pre-generated files
used, no Python required), full `/W4 /WX` clean build (one real issue
found and fixed during development: `XrApplicationInfo::applicationVersion`/
`engineVersion` are plain `uint32_t` fields, not `XrVersion` — using
`XR_MAKE_VERSION` for them, as an initial draft mistakenly did,
silently truncates a 64-bit value into a 32-bit field, caught by `/W4`
as `C4305`/`C4309` and fixed by packing a plain `uint32_t` instead),
`ctest` **12/12** (new `OpenXRTests`, all others unchanged).

`ARENGINE_ENABLE_OPENXR=ON` + `ARENGINE_ENABLE_VULKAN=OFF`: configures,
builds (zero Vulkan targets present, confirming AREngine's own
`Rendering` module built no Vulkan code), and passes all **11**
non-Vulkan tests (`OpenXRTests` included) — directly confirming M9A is
not coupled to Vulkan, per the brief's explicit requirement.

`arengine_openxr_demo` run on this development machine (which has no
OpenXR runtime installed) three times, once per configuration above:
identical, correct Case A behavior every time — API layers enumerate
successfully (0 found, loader-level query, no runtime needed);
instance extension enumeration fails with `XR_ERROR_RUNTIME_UNAVAILABLE`
(logged as a warning, not a crash); instance creation fails with the
same `XR_ERROR_RUNTIME_UNAVAILABLE`, correctly recognized and reported
as "no runtime installed - a normal, expected outcome," not a generic
error; the demo exits with code `0`. The OpenXR loader's own internal
diagnostic logging (printed directly to stderr by the loader itself,
not by AREngine) is visible alongside AREngine's own log lines and is
expected, normal loader behavior when no runtime is registered.

- Active OpenXR runtime: **none installed on this development
  machine** (Case A) — not reportable this session; see "Headset
  Absent Case" above.
- Requested OpenXR API version: **1.0** (`XR_API_VERSION_1_0`, formats
  as `"1.0.62"` — see "Requested API Version" for why the patch
  component isn't 0)
- Header version at build time: **1.1.62** (`XR_CURRENT_API_VERSION`,
  from the fetched `release-1.1.62` OpenXR-SDK)
- Available API layers on this machine: **0**
- Available instance extensions on this machine: **not queryable** (no
  runtime - `XR_ERROR_RUNTIME_UNAVAILABLE`)
- HMD `XrSystemId` / system name / max swapchain dimensions / max
  layer count: **not obtainable this session** (bring-up never reaches
  instance/system creation without a runtime) - the code paths that
  would report these are implemented and unit-tested where separable,
  but not exercised against real hardware; see "Headset Absent Case"

### What's Deferred to M9B+

No `XrSession`, no graphics binding, no Vulkan/OpenXR bridge
(`xrGetVulkanGraphicsRequirementsKHR`/`xrCreateVulkanInstanceKHR`/
`xrCreateVulkanDeviceKHR` and friends - all M9C), no XR swapchain, no
`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`/`xrLocateViews`, no reference
spaces, no head tracking, no controllers, no hand/eye tracking, no
passthrough, no anchors, no XR actions, no `XRFrameDriver`, no Scene
rendering, no stereo rendering, no headset presentation. The existing
`FrameDriver` abstraction (`Frame` module) is completely untouched by
this milestone, and `engine/xr` does not link `Frame` at all right now
(see "Dependency Trim" above) - it will come back once
`XRFrameDriver` is real enough to need it. M8's Vulkan desktop path is
completely unchanged - `tests/vulkan_present_demo.cpp` and everything
under `engine/rendering` were not touched by this milestone.

No architectural issues were discovered — the loader acquisition
strategy, instance/system bring-up sequence, and graceful-failure
handling all worked exactly as designed against the one real machine
state available to test (no runtime installed), with the only real
bug caught being the `applicationVersion`/`engineVersion` type
mismatch, caught immediately by `/W4 /WX` before it could reach
production. The one open item, consistent with M8H's own honestly-
reported gap, is that Cases B and C of the headset-absent handling are
verified by specification-level code review rather than real hardware,
since no OpenXR runtime or headset was available this session.

## 25. M9B: Runtime/Simulator Development Environment (Planning Only)

M9B is a **planning/review milestone with zero engine code changes** —
it exists because of a roadmap correction discovered right after M9A:
a normal (non-headless) OpenXR session requires a graphics binding at
creation time, so `XrSession` cannot be implemented before Vulkan/
OpenXR graphics integration exists (see "Why Graphics Binding Must
Precede a Graphics XrSession" below). M9B's job is narrower than that:
confirm this development machine's current OpenXR runtime state, and
recommend (not install) a suitable runtime/simulator so M9C+ can
eventually be validated against something real. No files under
`engine/` changed in this milestone.

### Revised M9 Sub-Milestone Order

M9A's original roadmap entry ("OpenXR integration") is now split more
finely, in this conceptual order:

- **M9A** — OpenXR instance/system discovery (complete).
- **M9B** — runtime/simulator development environment (this section).
- **M9C** — Vulkan/OpenXR graphics requirements and graphics binding.
- **M9D** — `XrSession` + session-state handling + reference spaces.
- **M9E** — XR swapchains + frame lifecycle.
- **M9F** — `xrLocateViews` + stereo rendering.
- **M9G** — head-tracked AREngine demo.

See `docs/ROADMAP.md` for the corresponding table rows.

### Why Graphics Binding Must Precede a Graphics XrSession

`xrCreateSession`'s `XrSessionCreateInfo` has a `next` chain, and for
any *normal* (non-headless) session, that chain **must** contain a
graphics-API-specific binding struct — for Vulkan, `XrGraphicsBindingVulkanKHR`,
carrying an already-created `VkInstance`, `VkPhysicalDevice`,
`VkDevice`, and the queue family/index AREngine intends to render
with. Without it, there is no valid `XrSessionCreateInfo` to
construct at all for a graphics session — this isn't a missing
optional feature, it's a required input the struct has no sensible
default for. (A session-less/graphics-less mode does exist in OpenXR —
the `XR_MND_headless` extension — but that is an optional,
vendor-adjacent extension for non-rendering use cases, not the
"normal" graphics session this roadmap's M9D is aiming for.)

Beyond just *having* Vulkan objects, the OpenXR spec requires the app
to *negotiate* them with the runtime first, specifically so the
runtime's compositor and the app agree on exactly which physical GPU
and Vulkan API version are in play (a machine can have multiple GPUs;
the runtime needs the app rendering on the same one driving the
headset's output):

1. `xrGetVulkanGraphicsRequirementsKHR` — asks the runtime what
   Vulkan API version range it supports, *before* creating a
   `VkInstance`.
2. `xrGetVulkanGraphicsDeviceKHR` (or the runtime-driven
   `xrCreateVulkanInstanceKHR`/`xrCreateVulkanDeviceKHR` path) — asks
   the runtime which physical device to use, rather than AREngine's
   own `SelectPhysicalDeviceForPresentation` (M8B) picking one
   independently, which could disagree with the headset's actual
   output device.

Only once this negotiation produces a `VkInstance`/`VkPhysicalDevice`/
`VkDevice`/queue tuple the runtime has agreed to is there anything
valid to place in `XrGraphicsBindingVulkanKHR` and hand to
`xrCreateSession`. This is exactly why M9C (graphics requirements +
graphics binding) is sequenced immediately before M9D (`XrSession`) and
never the other way around — attempting `XrSession` first, as M9A's
original unsplit roadmap entry implicitly suggested, would have hit
this requirement with nothing to satisfy it.

### Environment Inspection (M9B)

Checked directly on this development machine (Windows 11, build
26200):

- **`XR_RUNTIME_JSON` environment variable** (the loader's absolute
  override, checked first before any registry lookup): **not set**, at
  process, user, or machine scope.
- **`HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime`**
  (the standard Windows runtime-registration key the loader falls back
  to): **the key does not exist at all** — not merely empty. Confirmed
  by direct registry inspection; only `HKLM\SOFTWARE\Khronos\Vulkan`
  exists under `Khronos` (created by the Vulkan SDK installer), with
  no sibling `OpenXR` key.
- **Known OpenXR runtime software** (SteamVR, Meta/Oculus, Varjo Base,
  Windows Mixed Reality, ALVR, Monado): checked via installed-program
  registry entries and known install paths — **none found**.
- **Windows Mixed Reality specifically**: not installed, and not
  expected to be viable even if reinstalled — Microsoft retired the
  WMR platform (including its OpenXR runtime) starting with Windows 11
  24H2; this machine's build (26200) postdates that removal.

This precisely explains M9A's own observed behavior: `xrCreateInstance`
failing with `XR_ERROR_RUNTIME_UNAVAILABLE`, and the OpenXR loader's
own diagnostic log ("`RuntimeManifestFile::FindManifestFiles - failed
to find active runtime file in registry`") — the loader's Windows
runtime lookup found nothing because there is, genuinely, nothing
registered.

### How the OpenXR Loader Discovers Runtimes (M9B)

Confirmed by direct inspection of the fetched OpenXR-SDK loader's own
source (`src/loader/manifest_file.cpp`,
`RuntimeManifestFile::FindManifestFiles`), not just general knowledge —
the exact order is:

1. **`XR_RUNTIME_JSON` environment variable** — if set (and points at
   a real, readable file), it is used unconditionally, bypassing the
   registry entirely. This is the standard way to force a specific
   runtime for local testing without touching the registry.
2. **Windows registry**, `HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\<major-version>\ActiveRuntime`
   (a `REG_SZ` value naming an absolute path to a runtime manifest
   JSON file) — `HKEY_LOCAL_MACHINE` only; unlike API-layer discovery
   (which also checks `HKEY_CURRENT_USER`), the *active runtime* key is
   HKLM-only in this loader version. Setting it requires installing (or
   manually registering) a runtime with administrator privileges - a
   per-user override is not part of the standard runtime lookup.
3. If neither yields a valid manifest, `xrCreateInstance` (and any
   loader-level call that needs to talk to a runtime, such as
   `xrEnumerateInstanceExtensionProperties`) fails with
   `XR_ERROR_RUNTIME_UNAVAILABLE` — exactly the case M9A's demo
   exercised and this section's environment inspection explains.

Note this is entirely separate from **API layer** discovery (also
registry-based, under a sibling `ApiLayers\Implicit`/`ApiLayers\Explicit`
key structure, and does check `HKEY_CURRENT_USER` as a fallback) — M9A
already confirmed 0 layers present on this machine, consistent with no
runtime (many API layers ship alongside a runtime installation) having
ever been installed here either.

### Recommended Runtime/Simulator (M9B)

**Recommendation: SteamVR's OpenXR runtime, run with a null/simulated
HMD driver (no physical headset required).** Reasoning against the
brief's own criteria:

- **Conformant**: SteamVR's OpenXR runtime is on Khronos's official
  Adopters/Conformant Products list — satisfies "prefer a conformant
  OpenXR runtime" directly, rather than an experimental or
  partially-compliant implementation.
- **No custom AR glasses required**: SteamVR supports running without
  a real headset via its built-in null/simulated HMD driver
  (`driver_null`) — well-documented, widely used by the OpenXR/OpenVR
  developer community specifically for headless CI and no-hardware
  development. This would let AREngine's future M9D+ session/swapchain/
  frame-loop code be exercised against a real, conformant runtime's
  actual state machine (session states, frame timing) without needing
  physical AR/VR hardware.
- **Free and easy to install**: distributed via Steam (itself free),
  no purchase required to install and register the runtime.
- **No AREngine coupling**: AREngine's own code has zero
  runtime-specific logic today — no vendor extensions requested, no
  SteamVR-specific assumptions anywhere in `engine/xr`. Whichever
  runtime is "active" is an OS/environment-level choice (the registry
  key above), completely outside AREngine's code. Recommending SteamVR
  for *this developer's local testing* does not create any code-level
  dependency on Valve — a different developer, or a future CI machine,
  could just as validly point `XR_RUNTIME_JSON` at a different
  conformant runtime with zero AREngine changes.

**Alternative considered**: **Meta XR Simulator** (Meta's own
standalone, headset-free OpenXR runtime/simulator, free to download,
does not require owning a Quest) is a legitimate secondary option,
particularly if future milestones want to exercise Meta-specific
extensions (hand tracking, passthrough) — but it is more feature-heavy
and Quest-content-oriented than M9C–M9E's actual near-term needs
(graphics binding, session states, basic stereo rendering), so SteamVR
remains the primary recommendation for now.

**Ruled out**: Windows Mixed Reality (platform retired on this
Windows build, confirmed above); Meta/Oculus native runtime and Varjo
Base (both require owning the actual respective hardware, contradicting
"without requiring custom AR glasses"); Monado (Windows support is
limited/experimental compared to its Linux-first target, a weaker fit
for a smooth Windows dev loop than SteamVR).

**Nothing has been installed.** Per the brief, this is a
recommendation only, reported before any installation, awaiting
explicit approval to proceed.

### What's Deferred to M9C+

No Vulkan/OpenXR graphics requirements query, no graphics binding, no
`XrSession`, no session-state handling, no reference spaces, no XR
swapchains, no frame lifecycle (`xrWaitFrame`/`xrBeginFrame`/
`xrEndFrame`), no `xrLocateViews`, no stereo rendering, no head-tracked
demo, and (per this milestone) no runtime/simulator actually installed
yet. `engine/xr`'s M9A-established structure (`OpenXRInstance`,
`OpenXRSystem`, `OpenXRResult`, `OpenXRVersion`, all private under
`src/openxr/`) is completely unchanged.

No architectural issues were discovered — this was a documentation/
investigation-only milestone with no code surface to introduce them.

## 26. M9C Implementation Notes

M9C integrates OpenXR with Vulkan far enough to construct every Vulkan
object a future graphics `XrSession` will need — but stops short of
creating that session. It proves the whole chain: confirm
`XR_KHR_vulkan_enable2` is supported → enable it on instance creation →
query Vulkan graphics requirements → create an XR-compatible
`VkInstance` (through OpenXR, not the ordinary desktop path) → ask
OpenXR which `VkPhysicalDevice` to use → find a graphics queue family →
create an XR-compatible `VkDevice` → retrieve the queue → assemble the
data a future `XrGraphicsBindingVulkan2KHR` needs. No `XrSession`, no
swapchain, no Windows presentation surface anywhere in this milestone.

### XR-Vulkan Integration Placement (M9C)

```
OpenXR
    |  requirements/device selection
    v
XR-Vulkan integration   (engine/xr/src/openxr/OpenXRVulkan*.hpp/.cpp)
    |
    v
Vulkan instance/device/queue
```

This integration code lives in `engine/xr/src/openxr/`, **not**
`engine/rendering/src/vulkan/`. The deciding factor: every one of
M9C's calls that actually produces the Vulkan objects
(`xrCreateVulkanInstanceKHR`, `xrGetVulkanGraphicsDevice2KHR`,
`xrCreateVulkanDeviceKHR`) is itself an **OpenXR** API call — OpenXR is
the API surface driving this work, even though Vulkan types come out
the other end. Placing it in `Rendering` would make Rendering aware of
OpenXR (the brief's explicit "avoid" case); placing it in `XR` keeps
that awareness contained to one small, clearly-marked integration
boundary, private like every other OpenXR implementation file, and
never exposed through `XR.hpp` or any other public header.

`engine/xr/CMakeLists.txt` only compiles this boundary (`OpenXRVulkanRequirements.*`,
`OpenXRVulkanGraphicsBinding.*`) and links `Vulkan::Vulkan` **when both**
`ARENGINE_ENABLE_OPENXR` and `ARENGINE_ENABLE_VULKAN` are `ON` — nested
inside the existing OpenXR-only block, so `OPENXR=ON`/`VULKAN=OFF`
still builds `arengine_xr` exactly as M9A left it, just without this
one file pair. `XR` therefore never depends on the entire `Rendering`
module to get here — only on the raw Vulkan SDK, the same relationship
`Rendering` itself already has with Vulkan. `Rendering`'s own
`CMakeLists.txt` and every desktop Vulkan file are completely
untouched by M9C.

### XR_KHR_vulkan_enable2 Selection (M9C)

The M9C demo enumerates instance extensions (reusing M9A's
`EnumerateInstanceExtensions()`) and calls the new
`IsExtensionSupported(extensions, name)` helper
(`OpenXRInstance.hpp` — pure logic over already-queried data, directly
unit-tested) to explicitly verify `XR_KHR_vulkan_enable2` before ever
requesting it. **If unsupported, the demo logs a clear message and
stops** — it does not fall back to the older `XR_KHR_vulkan_enable`
(the non-"2" extension) or invent another path, per the brief. On this
development machine's runtime (SteamVR/OpenXR 2.16.7),
`XR_KHR_vulkan_enable2` is supported, so this path was never actually
exercised — see "Validation Results" below.

`enable2` (not `enable1`) was chosen because it is architecturally
simpler for exactly the reason this milestone needs: the app builds a
normal `VkInstanceCreateInfo`/`VkDeviceCreateInfo` itself and hands it
to OpenXR, which augments it internally with whatever it needs — no
separate `xrGetVulkanInstanceExtensionsKHR`/`xrGetVulkanDeviceExtensionsKHR`
query-and-merge dance `enable1` requires. `enable1`'s functions were
not implemented at all; there was no real need to after `enable2`
proved supported.

`OpenXRInstance` (M9A's class) was extended, not replaced: its
constructor now takes an optional `std::span<const char* const>` of
extension names to enable, defaulting to empty — M9A's own demo still
default-constructs it unchanged, requesting zero extensions exactly as
before. The extension-support check stays the *caller's*
responsibility (the demo, before construction) rather than something
`OpenXRInstance` silently validates or degrades on its own — matching
the brief's "report clearly and stop" instruction, which is a decision
belonging to the caller with context about what "stop" should mean,
not to a generic instance-creation wrapper.

### Function Pointer Loading (M9C)

`xrCreateVulkanInstanceKHR`, `xrGetVulkanGraphicsRequirements2KHR`,
`xrGetVulkanGraphicsDevice2KHR`, and `xrCreateVulkanDeviceKHR` are
extension functions — not part of the loader's static import table —
so none of them are assumed directly linkable. `LoadOpenXRVulkanFunctions(instance)`
(`OpenXRVulkanRequirements.hpp/.cpp`) resolves all four via
`xrGetInstanceProcAddr` and returns `std::nullopt` (not a
partially-filled struct) if even one fails to load. `OpenXRVulkanGraphicsBinding`'s
constructor asserts on `std::nullopt` — by the time this class is
constructed, `XR_KHR_vulkan_enable2` is already confirmed enabled, so
any function actually failing to load would be a genuine bug/runtime
inconsistency, not an ordinary outcome.

### Vulkan Graphics Requirements (M9C)

`xrGetVulkanGraphicsRequirements2KHR` reports `minApiVersionSupported`/
`maxApiVersionSupported` as `XrVersion` values — **not** Vulkan's own
`VkVersion` encoding, despite representing a Vulkan version. This is a
genuinely non-obvious gotcha: `XrVersion` packs major/minor/patch as
16/16/32 bits (`XR_VERSION_MAJOR`/`MINOR`/`PATCH`), while Vulkan's
`VK_MAKE_API_VERSION` packs them as 7/10/12 bits — the two are **not**
bit-reinterpretable. `XrVersionToVkApiVersion` (`OpenXRVulkanRequirements.hpp`)
decodes via the OpenXR macros (correctly recovering the intended
Vulkan major/minor, since the runtime encoded them with OpenXR's own
`XR_MAKE_VERSION` in the first place) and re-encodes via
`VK_MAKE_API_VERSION` — pure bit manipulation, directly unit-tested
with a synthetic value chosen specifically to fail if the conversion
were a naive reinterpret instead. Patch is intentionally dropped on
both ends (every `VK_API_VERSION_1_x` constant already hard-codes
patch as 0; Vulkan version comparisons are conventionally
major.minor-only).

**On this development machine (NVIDIA RTX 3060 Laptop, SteamVR/OpenXR
2.16.7): reported range 1.0.0 – 1.2.0.**

### Vulkan Version Selection (M9C)

`SelectVulkanApiVersion(preferred, range)` (pure logic, unit-tested)
keeps AREngine's desktop-matching preferred version (Vulkan 1.2 — see
below for why this is redeclared locally rather than included from
Rendering) if it falls inside the runtime's reported range, otherwise
falls back to the runtime's own minimum — never silently requesting a
version outside what the runtime declared. **1.2 fell inside the
reported 1.0–1.2 range on this machine, so 1.2 was kept**, per the
brief's explicit preference. `kPreferredVulkanApiVersion` (`VK_API_VERSION_1_2`)
is redeclared as a small local constant in `OpenXRVulkanGraphicsBinding.cpp`
rather than `#include`d from Rendering's private `VulkanVersion.hpp` —
one duplicated constant is a far smaller cost than a cross-module
header dependency between `XR` and `Rendering`'s private
implementation.

**Investigated further in M9D** — see Section 27, "Vulkan Device
Feature Requirement Discovered in M9D" and its addendum: M9C never
created an `XrSession`, so this 1.2 selection was never actually
exercised against the runtime's own session-creation code path. M9D's
real session creation initially hit a genuine device-feature conflict
at 1.2, briefly worked around with a static 1.1 cap — then, on
review, that cap was found to be unnecessary once the underlying
feature-enabling approach was corrected (avoiding the specific
struct that conflicted, rather than avoiding the API version). The
version actually requested today is still exactly this section's
`SelectVulkanApiVersion` output, unmodified — **1.2 is genuinely
selected and works**, with no exception logic anywhere in the code.

### XR-Controlled VkInstance Creation (M9C)

The XR-compatible `VkInstance` is created via `xrCreateVulkanInstanceKHR`
(`XrVulkanInstanceCreateInfoKHR{ systemId, vkGetInstanceProcAddr, &vulkanCreateInfo,
vulkanAllocator=nullptr }`) — **not** the ordinary `vkCreateInstance`
desktop path (`VulkanInstance.cpp`), and returns **both** an `XrResult`
(did OpenXR itself process the request) and a `VkResult` (did the
`vkCreateInstance` call OpenXR performed internally succeed) — both are
checked. `vulkanCreateInfo` is a completely ordinary `VkInstanceCreateInfo`
AREngine builds itself: an application info block with the selected
Vulkan API version, and — for `enable2` specifically — no instance
extensions/layers are *required* by the runtime (unlike `enable1`,
which needs the app to query and merge the runtime's own required
extension list first; this is exactly the simplification that made
`enable2` the right choice here). The only extension/layer AREngine
adds is `VK_LAYER_KHRONOS_validation` + `VK_EXT_debug_utils`, and only
when validation is genuinely available.

### Vulkan Validation (M9C)

**Preserved, not lost, when OpenXR participates in instance creation.**
The same debug-build-only, availability-gated pattern
`VulkanInstance.cpp` already uses (`vkEnumerateInstanceLayerProperties`
→ enable `VK_LAYER_KHRONOS_validation` + `VK_EXT_debug_utils` if
present) is re-implemented (not shared/imported, for the same
decoupling reasoning as everywhere else in this integration boundary)
inside `OpenXRVulkanGraphicsBinding`'s constructor, and a debug
messenger is attached to the resulting `VkInstance` exactly as the
desktop path attaches one to its own — logging through the same
`AR_LOG_ERROR`/`WARNING`/`INFO` calls, prefixed `[OpenXR/Vulkan]`
instead of `[Vulkan]` so the two paths' output stays distinguishable
in a shared log. **Confirmed working on the manual run**: the log shows
`VK_LAYER_KHRONOS_validation` genuinely inserted into the XR-created
instance's layer callstack, with zero validation errors or warnings
for the entire bring-up.

### XR-Controlled Physical Device Selection (M9C)

The `VkPhysicalDevice` comes from `xrGetVulkanGraphicsDevice2KHR`
(`XrVulkanGraphicsDeviceGetInfoKHR{ systemId, vulkanInstance }`) — this
is **authoritative**, not `Rendering::Vulkan`'s own desktop ranking
algorithm (`SelectPhysicalDevice`/`RankPhysicalDeviceType`), which is
never called anywhere in this path. On a machine with multiple GPUs,
only OpenXR knows which one is actually driving the headset's output;
AREngine's own "prefer discrete over integrated" heuristic has no way
to know that and must not second-guess it. **On this development
machine, OpenXR selected the same NVIDIA GeForce RTX 3060 Laptop GPU
the desktop path already selects** — a useful diagnostic confirmation
(this machine only has one real GPU, so agreement was expected), but
the code never requires or checks for this agreement; a machine with a
dedicated XR-only GPU alongside a desktop GPU would legitimately see
them differ.

### Queue Family Selection (M9C)

`FindGraphicsQueueFamily` (`OpenXRVulkanGraphicsBinding.hpp/.cpp`) is a
fresh, self-contained copy of the same ~10 lines `Rendering::Vulkan::FindGraphicsQueueFamily`
(`VulkanPhysicalDevice.hpp`) already implements — deliberately
duplicated, not imported, for the same cross-module decoupling
reasoning as everywhere else in this file. No `VkSurfaceKHR`/
presentation support is queried at all (unlike the desktop's
`FindPresentQueueFamily`) — there is no Windows surface anywhere in
this XR path, only a plain graphics-capable queue family.
`queueIndex` is always `0` — no concrete reason on this hardware to
request anything else. **Selected on this machine: queue family index
0, queue index 0** — confirmed by actually retrieving the `VkQueue`
handle via `vkGetDeviceQueue` (non-null), not just asserting an index
looked plausible.

### XR-Controlled VkDevice Creation (M9C)

Created via `xrCreateVulkanDeviceKHR` (`XrVulkanDeviceCreateInfoKHR{
systemId, vkGetInstanceProcAddr, vulkanPhysicalDevice, &vulkanCreateInfo }`)
— not `vkCreateDevice`. `vulkanCreateInfo` requests **one queue, one
family, zero device extensions, and no `pEnabledFeatures`** — in
particular, deliberately **no `VK_KHR_swapchain`**, even though the
desktop `VulkanDevice` enables it: OpenXR owns its own swapchains
starting at M9E, and a Windows presentation swapchain extension has no
relevance to a device this XR path will never present to a
`VkSurfaceKHR` with. No optional GPU feature is requested speculatively
either — nothing in M9C needs one.

### Graphics Binding Data (M9C)

`VulkanGraphicsBindingData` (`OpenXRVulkanGraphicsBinding.hpp`) is a
plain struct mirroring `XrGraphicsBindingVulkan2KHR`'s real fields
exactly (`instance`, `physicalDevice`, `device`, `queueFamilyIndex`,
`queueIndex` — minus `type`/`next`), plus an `IsValid()` predicate
(non-null instance/physicalDevice/device) that is pure logic and
directly unit-tested (including with fake-but-non-null handle values,
proving the check genuinely requires all three, not just one). M9D can
construct `XrGraphicsBindingVulkan2KHR` directly from this data with no
translation step. **In simple English**: this struct is the exact
paperwork OpenXR will ask for when it's finally time to say "render
into this headset using this Vulkan setup" — M9C proves every field on
that paperwork is filled in correctly, without actually submitting it
yet.

### Ownership / Destruction Order (M9C)

- **`XrInstance` / `XrSystemId`**: borrowed, not owned, by
  `OpenXRVulkanGraphicsBinding`. `XrSystemId` is never destroyed by
  anyone (it is an opaque identifier, not a resource — unchanged from
  M9A). The caller (the demo) must keep its `OpenXRInstance` alive for
  `OpenXRVulkanGraphicsBinding`'s entire lifetime; this is guaranteed
  by **explicit declaration order**, not left as fragile happenstance:
  the demo declares `OpenXRInstance instance` before constructing
  `OpenXRVulkanGraphicsBinding binding`, so C++'s reverse-local-
  destruction-order rule destroys `binding` first and `instance` last
  — documented with an inline comment at both the class declaration and
  the demo's own construction site, not left implicit.
- **`VkInstance` / `VkDevice` / debug messenger**: owned by
  `OpenXRVulkanGraphicsBinding`, destroyed in its destructor in the
  order debug messenger → `VkDevice` → `VkInstance` (a device must
  never outlive the instance it was created from).
- **`VkPhysicalDevice`**: never destroyed by an application at all
  (Vulkan physical devices are driver-owned, enumerated/queried
  handles) — `OpenXRVulkanGraphicsBinding` never attempts to.
  **`VkQueue`**: owned implicitly by the `VkDevice` that created it;
  also never separately destroyed.

### Why the M8 Desktop Vulkan Objects Are Not Reused (M9C)

The M8 desktop `VulkanInstance`/`VulkanDevice` (created via ordinary
`vkCreateInstance`/`vkCreateDevice`, driving the M8 window/swapchain
path) are **completely separate** from M9C's XR-compatible objects —
never handed to OpenXR, never shared. Per the OpenXR spec,
`XR_KHR_vulkan_enable2` requires the app's Vulkan instance/device to be
created *through* OpenXR's own functions specifically so the runtime
can guarantee compatibility with whatever compositor/driver path it
uses internally — an instance created independently via plain
`vkCreateInstance` has no such guarantee and is not a valid substitute,
regardless of which extensions/layers it happens to enable. M9C
therefore builds a second, independent Vulkan instance/device pair.
Whether and how much of this eventually gets unified with the desktop
path is deliberately left undecided — "we can decide how much code to
unify later after real evidence" (the brief's own words) — one working
XR path and one working desktop path is not yet evidence for what a
shared abstraction should look like. The M8 desktop demo
(`arengine_vulkan_present_demo`) is completely unmodified and continues
working independently.

### Why the Desktop VkSurfaceKHR/Swapchain Is Absent (M9C)

No `VkSurfaceKHR`, no Win32 Vulkan surface, no `VK_KHR_swapchain`, no
present queue, anywhere in this integration boundary. Those are
Windows *desktop presentation* concepts — a `VkSurfaceKHR` represents
a native window's drawable surface, and `VK_KHR_swapchain` is how an
app presents rendered images to that surface. **OpenXR presentation
works completely differently**: the runtime's compositor owns its own
swapchains (`xrCreateSwapchain`, arriving in M9E) backed by images the
runtime itself allocates and hands to the app — there is no window,
and no Windows presentation surface of any kind in the loop. Requesting
`VK_KHR_swapchain` on this device would be requesting a capability this
XR path has no use for and no window to ever use it with.

### Build Options (M9C)

M9C's new sources (`OpenXRVulkanRequirements.*`, `OpenXRVulkanGraphicsBinding.*`
in `engine/xr/`; `openxr_vulkan_tests.cpp`, `openxr_vulkan_demo.cpp` in
`tests/`) are gated behind **both** `ARENGINE_ENABLE_OPENXR=ON` **and**
`ARENGINE_ENABLE_VULKAN=ON` — nested `if(ARENGINE_ENABLE_VULKAN)` blocks
inside the existing `if(ARENGINE_ENABLE_OPENXR)` blocks in both
`engine/xr/CMakeLists.txt` and `tests/CMakeLists.txt`. All four
combinations were verified this milestone (see "Validation Results"):
`OPENXR=ON`/`VULKAN=ON` (13/13 tests, full M9C path), `OPENXR=OFF`/`VULKAN=ON`
(11/11, unaffected), `OPENXR=ON`/`VULKAN=OFF` (11/11, no Vulkan
coupling — `arengine_xr` builds without this file pair at all), and
`OPENXR=OFF`/`VULKAN=OFF` (10/10, unaffected). No link errors in any
combination.

### Validation Results (M9C)

`ARENGINE_ENABLE_OPENXR=ON`/`ARENGINE_ENABLE_VULKAN=ON`: full `/W4 /WX`
clean build (including the fetched OpenXR-SDK loader and all M9C
sources), `ctest` **13/13** (new `OpenXRVulkanTests` — 10 pure-logic
checks covering `XrVersionToVkApiVersion`'s conversion correctness,
`FormatVkApiVersion`, `IsVulkanApiVersionSupported`/`SelectVulkanApiVersion`'s
range logic, `FindGraphicsQueueFamily`, and `VulkanGraphicsBindingData::IsValid()`;
`OpenXRTests` gained `TestIsExtensionSupported`).

`arengine_openxr_vulkan_demo` run against the live SteamVR/OpenXR 2.16.7
null-driver runtime (see `docs/ARCHITECTURE.md` Section 25):

```
XR_KHR_vulkan_enable2: supported, enabled
Active runtime: SteamVR/OpenXR (version 2.16.7)
XrSystemId: 1152951414759096766, system name: "SteamVR/OpenXR : null"
Vulkan API version range supported: 1.0.0 - 1.2.0
Vulkan API version selected: 1.2.0 (AREngine's desktop preference - in range, so kept)
Vulkan validation layer: enabled (VK_LAYER_KHRONOS_validation genuinely
    inserted into the XR-created instance's layer callstack)
OpenXR-selected GPU: NVIDIA GeForce RTX 3060 Laptop GPU (discrete GPU)
OpenXR-selected GPU's own Vulkan API version: 1.4.325
Graphics queue family index: 0, queue index: 0, VkQueue: non-null
Graphics-binding data valid: true
```

**Zero Vulkan validation errors or warnings** — confirmed both visually
(the log contains only `[OpenXR/Vulkan]`-prefixed `INFO`-level layer
setup messages) and programmatically (grepped for
warn/error/fail — no matches). Exit code 0 on every run. No `XrSession`
was created (confirmed by code inspection — `xrCreateSession` does not
appear anywhere in this milestone's source) and no `VkSurfaceKHR`/
desktop swapchain exists anywhere in the XR path (confirmed the same
way).

- Files changed: see the file list in the final chat report (kept out
  of this document to avoid duplicating a list that changes with every
  commit).

### What's Deferred to M9D+

No `XrSession`, no `xrCreateSession`, no session-state handling, no
reference spaces, no XR swapchains, no `xrEnumerateSwapchainFormats`,
no `xrCreateSwapchain`, no frame lifecycle
(`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`), no `xrLocateViews`, no
stereo rendering, no `XRFrameDriver`, no controllers/actions, no hand/
eye tracking, no passthrough, no spatial anchors, no Scene rendering,
no headset frame submission. `OpenXRVulkanGraphicsBinding`'s
`VulkanGraphicsBindingData` is genuinely ready to be handed directly
into an `XrGraphicsBindingVulkan2KHR` the moment M9D needs one — that
translation is the only thing M9D adds on the graphics-binding side;
everything else about M9D (session creation itself, state-change
handling via `xrPollEvent`, reference spaces) is new work.

No architectural issues were discovered — the extension-support check,
function-pointer loading, requirements query, XR-controlled instance/
device creation, physical-device/queue selection, and validation-layer
preservation all worked correctly on the first real-runtime run, with
zero validation warnings to investigate.

## 27. M9D Implementation Notes

M9D creates AREngine's first real `XrSession`, using the M9C Vulkan
graphics binding unchanged (no second Vulkan device), tracks
`XrSessionState` through a real event-polling loop, calls
`xrBeginSession`/`xrEndSession` at the correct points, enumerates view
configurations and reference spaces, and creates the reference spaces
the runtime supports. No swapchain, no frame loop, no view location —
those remain M9E/M9F.

### XrSession Ownership (M9D)

`OpenXRSession` (`engine/xr/src/openxr/OpenXRSession.hpp/.cpp`) owns
one `XrSession`, created via `xrCreateSession` with an
`XrGraphicsBindingVulkan2KHR` — built directly from the M9C
`VulkanGraphicsBindingData` — chained through
`XrSessionCreateInfo::next`. `createFlags` stays zero. Destructor calls
`xrDestroySession`. Not copyable or movable, same discipline as every
other owned OpenXR/Vulkan handle in this engine. This is the **one**
piece of M9D's session lifecycle that is Vulkan-coupled — everything
else (`OpenXRSessionState.hpp`, `OpenXRViewConfiguration.hpp`,
`OpenXRReferenceSpace.hpp`) has zero Vulkan dependency, since session
*state*, view configuration, and reference spaces are pure OpenXR
concepts independent of which graphics API backs the session.
`engine/xr/CMakeLists.txt` reflects this exactly: `OpenXRSession.*` is
nested inside the same `if(ARENGINE_ENABLE_VULKAN)` block as M9C's
files; the other three are in the unconditional OpenXR-only block.

### Graphics-Binding Relationship (M9D)

`XrGraphicsBindingVulkan2KHR`'s five fields (`instance`,
`physicalDevice`, `device`, `queueFamilyIndex`, `queueIndex`) are
copied directly from `OpenXRVulkanGraphicsBinding::GetBindingData()` —
the exact same struct M9C validated. **No second Vulkan instance/
device is created for M9D.** In simple English: the graphics binding is
the paperwork OpenXR needs before it will let an app open a real
session — "here is the Vulkan instance, the GPU, the logical device,
and which queue to use." M9C proved every field on that paperwork was
valid; M9D is the first milestone that actually submits it.

### Session-State Lifecycle (M9D)

`XrSessionState` is never treated as a boolean anywhere in this
engine. `FormatSessionState` (`OpenXRSessionState.hpp/.cpp`) gives
every state a readable name for logging (with a numeric fallback for
anything unrecognized, mirroring `XrResultToReadableString`'s own
discipline). Three small `constexpr` predicates, all pure logic and
unit-tested:

- `ShouldBeginSession(state)` — true only for `READY`.
- `ShouldEndSession(state, sessionRunning)` — true only for `STOPPING`
  **and** an already-running session; `sessionRunning` is a separate
  parameter, not inferred from `state` (see "Session Running Flag"
  below).
- `ShouldStopMainLoop(state)` — true for `EXITING` or `LOSS_PENDING`.

**In simple English, session states answer "what stage of the
session's life am I in right now, and what am I allowed to do about
it?"** `IDLE` is "created, not yet ready." `READY` is "the runtime says
go ahead and begin." `SYNCHRONIZED`/`VISIBLE`/`FOCUSED` describe how
much the running session is actually being displayed/interacted with
(least to most). `STOPPING` is "wind down now." `EXITING` is "done,
leave cleanly." `LOSS_PENDING` is "the whole runtime/system is about to
disappear out from under you."

### Event Polling (M9D)

`PollSessionEvents(instance)` (`OpenXRSession.hpp/.cpp`) drains every
pending event in one call: `xrPollEvent` in a loop, each iteration
using a freshly zero-initialized `XrEventDataBuffer` (as OpenXR
requires), until `XR_EVENT_UNAVAILABLE`. Only two event types are
actually handled — `XrEventDataSessionStateChanged` (read via the
standard OpenXR polymorphic-event `reinterpret_cast`, matching every
Khronos sample) and `XrEventDataInstanceLossPending` (logged, treated
as a stop signal); `XrEventDataEventsLost` is logged as a warning
(queue overflow, not fatal); everything else is silently ignored — a
deliberately small, non-generic event-dispatch surface, per the brief.
If multiple session-state-changed events arrive in one poll cycle, only
the last one's state is kept — it is the current, authoritative one.

### Observed SteamVR/Null State Sequence (M9D)

**Exactly**: `IDLE` is skipped in the demo's own printed sequence
(only *changes* are logged, and the very first observed change already
carries the runtime past creation) —

```
XR_SESSION_STATE_READY -> XR_SESSION_STATE_STOPPING -> XR_SESSION_STATE_EXITING
```

**Critically, `SYNCHRONIZED`/`VISIBLE`/`FOCUSED` were never reached.**
This was investigated, not assumed away: per the OpenXR spec,
`SYNCHRONIZED` is reached once the application participates in the
frame loop (`xrWaitFrame`) — which M9D explicitly must not call. With
no frame loop at all, a real, spec-conformant runtime has no reason to
ever advance past `READY`/running; SteamVR's null driver behaves
exactly this way in practice, confirmed by direct observation (the
demo's first implementation waited for `FOCUSED` before requesting
exit and simply hung until a 30-second safety timeout, every time).
The demo now requests exit immediately once the session starts
running, which correctly matches what M9D can actually reach on its
own. **M9E's frame loop is expected to be the reason `SYNCHRONIZED`+
become observable at all** — worth confirming directly once that
milestone exists, not assumed here.

### READY → xrBeginSession Behavior (M9D)

`ShouldBeginSession(state)` fires exactly once, from inside the
event-poll loop, immediately after a `READY` transition is observed.
`OpenXRSession::BeginSession(primaryViewConfigurationType)` calls
`xrBeginSession` and sets its internal running flag. Confirmed via the
manual run: this happened exactly once, in direct response to the
single `READY` event SteamVR's null driver sent.

### STOPPING → xrEndSession Behavior (M9D)

`ShouldEndSession(state, sessionRunning)` fires only when the tracked
state is `STOPPING` **and** the session is still marked running —
calling `xrEndSession` from any other state is invalid per spec and
returns `XR_ERROR_SESSION_NOT_RUNNING`. **This was found the hard way**:
an early version of this demo's post-loop fallback path called
`EndSession()` unconditionally whenever `IsRunning()` was true,
regardless of the actual tracked state — this crashed
(`AR_ASSERT_MSG` fired) the first time the safety timeout fired before
`STOPPING` had ever arrived. Fixed by gating that fallback on
`ShouldEndSession` too, exactly like the main loop's own handling; if a
session is still running but genuinely never reached `STOPPING`, the
correct action is to destroy it directly (spec-legal — the runtime
must clean up a still-running session on `xrDestroySession`), not to
force an invalid `xrEndSession` call.

### EXITING Behavior (M9D)

`ShouldStopMainLoop` returns true, the demo logs a plain informational
message ("not an error") and breaks its loop. No crash, no special
handling beyond that — confirmed on the manual run: `EXITING` arrived
immediately after `xrEndSession` succeeded, and the demo exited cleanly
with code 0.

### LOSS_PENDING Behavior (M9D)

Also handled by `ShouldStopMainLoop` (session-state `LOSS_PENDING`) —
the demo stops using the session and exits, with no recreation
attempted, per the brief. Separately, `XrEventDataInstanceLossPending`
(a distinct, rarer event about the whole *instance* becoming invalid,
not just the session) is checked explicitly in the poll loop and also
treated as a clean-stop signal. Neither was actually observed against
SteamVR's null driver this session — both are implemented and covered
by the loop's structure, but this specific path remains unexercised
against a real runtime; a genuine, honestly-reported gap, consistent
with this project's practice of not claiming untested paths as
verified.

### Selected Primary View Configuration (M9D)

`EnumerateViewConfigurationTypes` reported exactly one supported type
on this runtime/system: `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO`.
`SelectPrimaryViewConfigurationType` (pure logic, unit-tested)
deliberately returns `std::nullopt` — not a silent fallback to
`PRIMARY_MONO` or anything else — when stereo isn't present, mirroring
M9C's own "report clearly and stop, don't invent a fallback" handling
of `XR_KHR_vulkan_enable2`. Not exercised on this runtime, since stereo
was supported.

### View Configuration Properties (M9D)

`xrGetViewConfigurationProperties(PRIMARY_STEREO)` reported
`fovMutable = true` on this runtime — logged, not acted on (nothing in
M9D adjusts FOV).

### Recommended Per-View Dimensions/Sample Counts (M9D)

`xrEnumerateViewConfigurationViews(PRIMARY_STEREO)` reported **2
views** (one per eye), each: recommended **1852x2056**, max
**8192x8192**, recommended sample count **1**, max sample count **1**.
Purely diagnostic in M9D — no swapchain or Vulkan image is allocated
from this data; M9E will be the first milestone to actually use these
numbers.

### Supported Reference-Space Types (M9D)

`xrEnumerateReferenceSpaces` reported all three core types on this
runtime: `VIEW`, `LOCAL`, `STAGE` (SteamVR's null driver, unlike some
minimal runtimes, does support `STAGE`). `SelectReferenceSpacesToCreate`
(pure logic, unit-tested with both an all-present and a
STAGE-absent synthetic input, since the brief explicitly warns "do not
assume STAGE exists") always includes `VIEW`/`LOCAL` when supported and
includes `STAGE` too, for diagnostics only — AREngine does not require
it.

### Which Reference Spaces Are Created (M9D)

All three: `OpenXRReferenceSpace` instances for `VIEW`, `LOCAL`, and
`STAGE`, each with `poseInReferenceSpace = IdentityPose()` (no rotation,
no offset — no reason yet to use anything else). Held in a
`std::vector<std::unique_ptr<OpenXRReferenceSpace>>`, declared after
`session` in the demo so they are destroyed before it automatically
(see "Destruction Order" below).

### Space Semantics (M9D)

In simple English:

- **VIEW** moves with the user's head/viewer — its origin *is*
  wherever the viewer currently is and however they're currently
  facing. Useful for things that should always be positioned relative
  to the eyes (e.g. a HUD).
- **LOCAL** is a stable local tracking origin appropriate for seated/
  standing local experiences — it does not move once established, even
  as the user moves around; think "the point in space where tracking
  started."
- **STAGE** is a room-scale/play-area-style reference frame when the
  runtime supports it — typically the floor-level center of a
  calibrated play area. AREngine does not require it to exist; it is
  created here only when the runtime reports supporting it, for
  diagnostics.

AREngine does not yet build any final world-origin policy on top of
these — M9D only proves the spaces themselves can be created and
destroyed correctly. Deciding which of these (if any) becomes the
engine's actual default rendering origin needs real pose/location
evidence (`xrLocateSpace`), which M9D explicitly does not call.

### Destruction Order (M9D)

M9D's demo declares, in this exact order: `instance` → `binding`
(M9C's `OpenXRVulkanGraphicsBinding`) → `session` (`OpenXRSession`) →
`referenceSpaces` (a vector of `OpenXRReferenceSpace`). C++'s
reverse-local-destruction-order rule therefore destroys everything in
exactly the opposite order automatically: reference spaces first (each
`xrDestroySpace`), then the session (`xrDestroySession`), then the
Vulkan graphics binding (its `VkDevice`, then `VkInstance`), then the
OpenXR instance (`xrDestroyInstance`) last — satisfying every ordering
requirement the brief lists (spaces before session; Vulkan device/
instance not destroyed while the session still references them;
instance outliving everything that depends on it). This is documented
explicitly with a comment at the declaration site, not left as an
implicit consequence of declaration order a future reader would have
to rediscover.

### Vulkan Device Feature Requirement Discovered in M9D

**The most significant real finding this milestone produced.** M9C's
Vulkan device was created and validated, but never actually used to
back a real session — `xrCreateSession` is where a runtime's own
compositor first does real work against the app's device. The very
first real session-creation attempt against SteamVR produced multiple
rounds of genuine Vulkan validation errors, each investigated and
resolved with real evidence, not guessed at:

1. **`vkCreateShaderModule` failed**: SteamVR's compositor shaders use
   the SPIR-V `ShaderViewportIndexLayer` capability, requiring Vulkan
   1.2's `shaderOutputViewportIndex`/`shaderOutputLayer` device
   features (or the pre-1.2 `VK_EXT_shader_viewport_index_layer`
   extension) — M9C's device enabled neither (deliberately minimal, no
   speculative features).
2. Enabling those two features via a `VkPhysicalDeviceVulkan12Features`
   struct fixed that error, but revealed a **second**: `vkCreateDevice`
   failed because SteamVR's own `xrCreateVulkanDeviceKHR`
   implementation unconditionally appends its own
   `VkPhysicalDeviceFeatures2`-wrapped `VkPhysicalDeviceTimelineSemaphoreFeatures`
   onto whatever `pNext` chain the app supplies — which Vulkan
   validation correctly rejects the moment the app's own chain *also*
   contains a `VkPhysicalDeviceVulkan12Features` (both describe
   `timelineSemaphore`; the spec forbids the ambiguity of two structs
   potentially disagreeing).
3. Several chain-structuring variations were tried directly against
   the real runtime (a bare `VkPhysicalDeviceVulkan12Features`; the
   same struct pre-populated with every supported 1.2 feature,
   including `timelineSemaphore`; wrapping it in an app-provided
   `VkPhysicalDeviceFeatures2` instead of attaching it directly) — **all
   still conflicted**, since SteamVR's runtime appends its own
   `VkPhysicalDeviceFeatures2`/`VkPhysicalDeviceTimelineSemaphoreFeatures`
   pair unconditionally, regardless of what the app's chain already
   contains.
4. **Resolution**: `shaderOutputViewportIndex`/`shaderOutputLayer` are
   satisfied via the plain `VK_EXT_shader_viewport_index_layer`
   **device extension string** (checked for availability first, never
   assumed) instead of the `VkPhysicalDeviceVulkan12Features` feature
   bits — this extension has no feature struct of its own, so it has
   nothing for the runtime's own injected structs to conflict with, at
   any Vulkan version. A separate, confirmed real requirement —
   `VkPhysicalDeviceFeatures::geometryShader`, used by a different
   SteamVR compositor shader variant — is satisfied by querying
   `vkGetPhysicalDeviceFeatures` and passing the full supported set via
   `pEnabledFeatures` (a plain core-1.0 struct, never in conflict with
   anything the runtime injects).
5. This device-feature/extension logic applies **only** to the
   XR-compatible device (`OpenXRVulkanGraphicsBinding`) — the M8
   desktop device (`Rendering::Vulkan::VulkanDevice`) is completely
   untouched and keeps its deliberately minimal, zero-optional-feature
   M8A-established policy. The reasoning for enabling "everything the
   physical device reports supporting" on the XR device specifically
   (rather than a hand-picked minimal set) is that a third-party
   runtime's compositor uses this device for shaders AREngine has no
   visibility into or control over; nothing is enabled that the
   hardware doesn't already genuinely support, so this remains
   evidence-based rather than speculative.

Confirmed fixed end to end: the final manual run produced **zero**
Vulkan validation errors or warnings, with the Vulkan API version
logged as **`1.2.0`** — AREngine's genuine, unmodified desktop-matching
preference, working against SteamVR without any exception. See the
addendum immediately below for how an initial, broader fix (a static
1.1 version cap) was caught and replaced with this narrower one before
M9E began.

### M9D Addendum: Vulkan Version Compatibility Review

**Requested and performed immediately after M9D's own approval, before
M9E began**, specifically to check that the fix above hadn't
overreached. The concern: M9D's first working fix capped
`OpenXRVulkanGraphicsBinding`'s selected Vulkan API version to 1.1
*unconditionally* whenever `SelectVulkanApiVersion` would otherwise
have picked 1.2+ — regardless of which OpenXR runtime was actually
active. That is a broader change than the evidence justified: it would
have silently downgraded every future runtime to 1.1, including fully
conformant ones with no trace of SteamVR's specific chain-conflict
behavior, for a problem that was never actually inherent to Vulkan 1.2
itself.

Re-examining the fix's own final shape (item 4 above) showed the cap
was no longer even doing any real work: the actual device-creation code
never chains `VkPhysicalDeviceVulkan12Features` (or any
`VkPhysicalDeviceFeatures2` wrapper) at all — `shaderOutputViewportIndex`/
`shaderOutputLayer` are satisfied through the plain extension string
instead. Since the confirmed conflict was specifically "app-provided
`VkPhysicalDeviceVulkan12Features` vs. runtime-injected
`VkPhysicalDeviceTimelineSemaphoreFeatures`," and the app's chain no
longer contains the former under *any* circumstance, **the version cap
had nothing left to protect against.** It was removed, and 1.2 was
re-tested directly against SteamVR: zero validation errors, full
session lifecycle correct, exactly as before — confirming the cap was
never actually load-bearing once the extension-string approach existed.

Checked directly against each concern raised:

1. **Desktop Vulkan renderer remains 1.2** — unchanged; `Rendering::Vulkan`'s
   `VulkanVersion.hpp`/`VulkanInstance`/`VulkanDevice` were never
   touched by any part of this fix.
2. **Runtime's reported min/max range is still queried** — unchanged;
   `xrGetVulkanGraphicsRequirements2KHR` and `DecodeVulkanVersionRange`
   are exactly as M9C left them.
3. **XR Vulkan API version is selected through capability/runtime
   logic** — yes, and more purely than before: `m_selectedVulkanApiVersion`
   is now *exactly* `SelectVulkanApiVersion(kPreferredVulkanApiVersion,
   m_supportedVersionRange)`'s own output, with no post-hoc
   modification of any kind.
4. **The SteamVR-specific cap is isolated as narrowly as practical** —
   more than that: it no longer exists. There was nothing narrower to
   isolate it to once it was confirmed unnecessary.
5. **No SteamVR path, driver name, or Valve-specific code leaks into
   generic AREngine APIs** — confirmed by inspection: `OpenXRVulkanGraphicsBinding.cpp`
   contains no string comparison against a runtime/driver name and no
   branch conditioned on which runtime is active. "SteamVR" appears
   only in comments/documentation describing what was *observed* during
   manual validation, never in a runtime code path.
6. **A future runtime can use Vulkan 1.2 or another supported version
   without redesigning the XR module** — yes: version selection is
   `SelectVulkanApiVersion`'s pure preference-vs-range logic, unmodified
   since M9C, with zero runtime-specific exceptions anywhere in the
   codebase today.
7. **Compatibility reason documented clearly** — this addendum, plus
   the corrected "Vulkan Device Feature Requirement Discovered in M9D"
   section above.

**Result: this is not merely a narrower fix than the one M9D shipped
with — it is a strictly better one.** The XR path now requests the
exact same Vulkan version the desktop renderer does, against the one
real runtime available for testing, with no exception logic left in
the codebase at all. `docs/ARCHITECTURE.md` Section 26 ("Vulkan Version
Selection (M9C)") was updated to point here rather than to a stale
description of the 1.1 cap.

### Validation Results (M9D)

`ARENGINE_ENABLE_OPENXR=ON`/`ARENGINE_ENABLE_VULKAN=ON`: full `/W4 /WX`
clean build, `ctest` **14/14** (new `OpenXRSessionTests` — 12
pure-logic checks covering `FormatSessionState`'s known values and
numeric fallback, `ShouldBeginSession`/`ShouldEndSession`/
`ShouldStopMainLoop`'s decision logic, view-configuration support/
selection logic, reference-space support/selection logic including the
explicit "STAGE absent" case, and `IdentityPose`).

`arengine_openxr_session_demo` run against the live SteamVR/OpenXR
2.16.7 null-driver runtime:

```
XrSession created successfully
Supported view configurations: 1 (PRIMARY_STEREO) - selected
View count: 2, each 1852x2056 recommended (8192x8192 max), sample count 1
Supported reference spaces: VIEW, LOCAL, STAGE (all created)
Observed state sequence: READY -> STOPPING -> EXITING
xrBeginSession: called once, exactly on READY
xrEndSession: called once, exactly on STOPPING
Vulkan API version selected: 1.2.0 (see the M9D Addendum above - re-verified after removing an initial, overly-broad 1.1 cap)
```

**Zero Vulkan validation errors or warnings** on the final run
(confirmed both by reading the full log and by grepping for warn/error/
fail — no matches) — a real improvement over the multiple genuine
errors hit and fixed during this milestone's own development, not a
claim that was true from the first attempt. Exit code 0. Re-verified
against a freshly rebuilt `arengine_openxr_vulkan_demo` (M9C's demo)
too, confirming the shared `OpenXRVulkanGraphicsBinding` fix didn't
regress M9C's own scenario. All four build-option combinations
(`OPENXR`×`VULKAN`, all four `ON`/`OFF` pairs) build and pass their
respective test suites with no link errors.

- Files changed: see the file list in the final chat report (kept out
  of this document to avoid duplicating a list that changes with every
  commit).

### What's Deferred to M9E+

No XR swapchains, no `xrCreateSwapchain`, no `xrEnumerateSwapchainFormats`,
no frame lifecycle (`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`), no
`xrLocateViews`, no `xrLocateSpace` for tracking, no stereo rendering,
no `XRFrameDriver`, no controllers/actions, no hand/eye tracking, no
passthrough, no spatial anchors, no Scene integration, no headset image
submission. `OpenXRSession`, `OpenXRReferenceSpace`, and the view-
configuration data M9D already gathered (view count, recommended/max
dimensions, sample counts) are all genuinely ready for M9E to consume
directly — that translation is expected to be the bulk of what M9E
adds on top, alongside the frame loop itself.

No architectural issues were discovered in the session-lifecycle logic
itself (state tracking, event polling, reference-space creation, and
destruction ordering all worked correctly on the first attempt once
constructed). The one genuine issue found and fixed was the Vulkan
device feature/extension requirement above — a real, evidence-based
discovery this milestone's own real-runtime testing was specifically
designed to surface, not a design flaw in M9D's own session-lifecycle
code.

## 28. M9E Implementation Notes

M9E gives AREngine its first real OpenXR frame lifecycle: XR
swapchains created from the runtime's own recommended dimensions,
`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`, per-eye swapchain-image
acquire/wait/release with a minimal Vulkan clear proving the
OpenXR-owned images are genuinely usable, an environment blend mode
selected from what the runtime actually reports, and zero composition
layers submitted (real view pose/FOV data via `xrLocateViews` is
formally deferred to M9F). No stereo scene rendering, no
`XRFrameDriver`, no head tracking — see "What's Deferred to M9F+" below.

### XR Swapchain Topology (M9E)

**One `XrSwapchain` per view (two total for `PRIMARY_STEREO`), not one
array swapchain with `arraySize = viewCount`.** Chosen for simplicity,
not because of any SteamVR-specific quirk: two independent swapchains
give two independent image lists, which maps directly onto M9E's own
"one distinct clear color per eye" proof-of-life (`kEyeClearColors` in
the demo) with no array-layer indexing to reason about, and defers the
real efficiency argument for a single array swapchain (fewer runtime
objects, natural fit for multiview rendering) to whichever future
milestone (M9F+) actually implements real stereo rendering and has
concrete evidence for which approach the renderer needs. This is a
general simplicity/evidence argument that would apply against any
conformant runtime, not a workaround for anything SteamVR-specific.

### Swapchain Color Format Selection (M9E)

`EnumerateSwapchainFormats`/`SelectSwapchainColorFormat`
(`OpenXRSwapchain.hpp/.cpp`) — the `int64_t` values
`xrEnumerateSwapchainFormats` returns are, for a Vulkan-backed session,
directly `VkFormat` values (an OpenXR-spec-defined mapping, not a
guess). `SelectSwapchainColorFormat` (pure logic, unit-tested) prefers
`VK_FORMAT_B8G8R8A8_SRGB`, then `VK_FORMAT_R8G8B8A8_SRGB`, then falls
back to whichever format the runtime lists first — **never** assumes
`VK_FORMAT_B8G8R8A8_SRGB` is present, per the brief. Against SteamVR's
null driver: 10 formats reported; `VK_FORMAT_B8G8R8A8_SRGB` was present
and selected.

### Recommended Dimensions/Sample Count (M9E)

Swapchain `width`/`height`/`sampleCount` come directly from each
view's own `XrViewConfigurationView` (`recommendedImageRectWidth`/
`recommendedImageRectHeight`/`recommendedSwapchainSampleCount`,
enumerated in M9D) — never hard-coded. Against SteamVR: both views
1852x2056, sample count 1, confirmed identical to M9D's own
observation of the same data. `faceCount`/`mipCount` are always 1 (one
view per swapchain, no cubemap, no mipmapping for a compositor
target).

### VkImage / VkImageView Ownership (M9E)

`OpenXRSwapchain` enumerates its images via `xrEnumerateSwapchainImages`
into `XrSwapchainImageVulkan2KHR` (the two-call idiom, same pattern as
every other `xrEnumerate*` wrapper in this codebase), then copies out
just the `VkImage` handles into a `std::vector<VkImage>`. **These
`VkImage`s are OpenXR-owned**: never destroyed by `OpenXRSwapchain` or
the demo, never backed by application-allocated `VkDeviceMemory` (no
`vkAllocateMemory`/`vkBindImageMemory` call exists anywhere in M9E's
code for them — the runtime already did that itself before exposing
the handles). **M9E creates zero `VkImageView`s**: `vkCmdClearColorImage`
operates directly on a `VkImage`, so no view was needed for this
milestone's minimal proof-of-life. If/when a future milestone needs a
render-pass/framebuffer-based render path over these images, that is
exactly where AREngine-owned `VkImageView`s would be created (and, per
the brief, destroyed before the owning `XrSwapchain`) — not before,
since M9E has no actual use for one yet.

### Frame Lifecycle: Acquire / Wait / Clear / Release (M9E)

Per rendered frame, per swapchain: `xrAcquireSwapchainImage` (returns
an index into the swapchain's own image list) → `xrWaitSwapchainImage`
(`timeout = XR_INFINITE_DURATION`) → [record Vulkan commands against
that index's `VkImage`] → `xrReleaseSwapchainImage`. The Vulkan work
itself: one shared, reusable command buffer (allocated once, `vkReset`+
re-recorded every frame) records, for each acquired image in turn, a
layout transition `UNDEFINED → TRANSFER_DST_OPTIMAL`, a
`vkCmdClearColorImage` to that eye's own distinct solid color
(`kEyeClearColors`: warm red for view 0, cool blue for view 1 — proof
each acquired image is genuinely independent, not the same underlying
image returned twice by mistake), then a second transition
`TRANSFER_DST_OPTIMAL → COLOR_ATTACHMENT_OPTIMAL` (the conventional
layout a color swapchain image is expected to be left in for the
compositor, matching its `COLOR_ATTACHMENT_BIT` usage flag, even though
M9E's own zero-layer `xrEndFrame` means nothing reads it this frame).

### Synchronization Before Release (M9E)

**`xrReleaseSwapchainImage` is never called while GPU work targeting
that image is still in flight.** One `VkFence` (created once, reused
every frame) is `vkQueueSubmit`-attached to the frame's single command
buffer submission; the demo then `vkWaitForFences` on it (infinite
timeout) **before** calling `xrReleaseSwapchainImage` for either
swapchain. A fence was used rather than `vkQueueWaitIdle` specifically
because the brief asked for one where practical — it waits for exactly
this frame's work, not the entire queue. This is fully synchronous (one
frame's GPU work completes before the next frame's command buffer is
even recorded, no multi-frame-in-flight pipelining) — adequate for
M9E's diagnostic clear, and explicitly **temporary**: a future renderer
doing real per-frame work will want double/triple-buffered command
buffers and fences instead of stalling every frame on GPU completion.

### `xrWaitFrame` Timing / `predictedDisplayTime` / `shouldRender` (M9E)

**In simple English:** `xrWaitFrame` is OpenXR's frame-pacing
heartbeat — the app calls it once per frame, it blocks until the
runtime decides "now is a good time to start preparing the next
frame," and hands back a `predictedDisplayTime` (when this frame's
image is expected to actually reach the user's eyes — not "now") plus
`shouldRender` (whether the runtime actually wants new content this
frame, or would rather the app skip rendering — e.g. because nothing
is currently visible to a user). `XrTimeToSeconds`
(`OpenXRFrameTiming.hpp`, pure, unit-tested) converts the raw `XrTime`
into seconds using the OpenXR spec's own fixed definition (a 64-bit
count of nanoseconds) — exact arithmetic, not a guess; the epoch itself
is runtime-defined and only meaningful relative to other `XrTime`
values from the same runtime, never as an absolute wall-clock reading
(this is why the logged values are large, runtime-internal numbers, not
"seconds since app start").

Observed against SteamVR's null driver: `shouldRender = true` on frame
1 only, then `false` for every subsequent frame observed (50, 100, 150,
200). This is a plausible, spec-consistent outcome, not a bug: the
runtime never reported `VISIBLE`/`FOCUSED` during this run (see below),
and `shouldRender` genuinely means "the runtime wants content right
now" — a null/simulated HMD with no real display and no user attention
signal has little reason to keep asking for new frames once its
initial diagnostic frame is captured.

### `xrWaitFrame` Requires A Running Session (M9E)

**A real, empirically-discovered behavior correction, found the hard
way — not assumed from the spec.** The first version of this loop
called `xrWaitFrame` unconditionally every iteration, including before
the session was ever running, on the (spec-plausible) theory that
participating in frame timing as early as possible helps the runtime
pace the session toward `SYNCHRONIZED`. This ran without error right
up through 200 successful frames and `xrRequestExitSession` — but the
very next `xrWaitFrame` call, made immediately after `STOPPING` →
`xrEndSession` had already succeeded, failed with
`XR_ERROR_SESSION_NOT_RUNNING` and crashed the demo (`AR_ASSERT_MSG`).
**Fixed** by only calling `xrWaitFrame` (and everything downstream of
it) while `session.IsRunning()` is true — checked *before* the call,
not after. This also resolved a latent architectural gap: the "not
running" branch now sleeps 16ms per iteration (matching M9D's own
polling cadence) since it is no longer paced by a blocking OpenXR call.
Re-run twice after the fix: both runs completed all 200 frames and
exited cleanly (code 0), with no further `xrWaitFrame`-related errors.
This demo never actually exercises "call `xrWaitFrame` before the
*first* `xrBeginSession`" either, since on this runtime `READY` (and
therefore `BeginSession`) arrives within the very first loop iteration
— that specific pre-first-begin case remains an honestly-reported,
unexercised gap, same practice as M9D's `LOSS_PENDING` gap.

### Environment Blend Mode Selection (M9E)

`EnumerateEnvironmentBlendModes`/`SelectEnvironmentBlendMode`
(`OpenXREnvironmentBlendMode.hpp/.cpp`, pure logic where it matters,
unit-tested) prefers `OPAQUE`, then `ALPHA_BLEND`, then `ADDITIVE`,
never hard-coded without checking what the runtime actually reports.
`OPAQUE` is preferred because AREngine has no real passthrough camera
pipeline yet — an opaque background is the safest, most universally
correct default for a runtime that could be a VR headset, a
passthrough-capable AR headset, or (as in this milestone's actual test
environment) a simulated/null HMD with no real-world view to blend
with at all. Against SteamVR: `OPAQUE` was the only mode reported and
was selected.

### Zero Composition Layers Submitted (M9E)

**Deliberate, and central to keeping M9E's scope honest.** A valid
`XrCompositionLayerProjection` requires real per-view pose/FOV data
from `xrLocateViews` against a real `XrSpace` — M9E does not call
`xrLocateViews` (formally deferred to M9F) and will not fabricate pose/
FOV data to manufacture a layer that looks complete but isn't. Every
`xrEndFrame` this milestone calls therefore uses `layerCount = 0`,
`layers = nullptr` — spec-legal, and means "the compositor shows
nothing new from this application this frame." The frame lifecycle
mechanics above (wait/begin/acquire/wait/clear/release/end) are fully
exercised regardless of layer count; only the actual on-screen
composition is deferred. A consequence worth noting for M9F: **no
`XrSpace`/reference space is created anywhere in M9E** (unlike M9D's
demo) — with zero layers submitted, nothing in the frame loop currently
needs one; M9F's `xrLocateViews` call will be the first thing in this
codebase that actually needs a real `XrSpace` again.

### Observed Session-State Sequence (M9E)

```
XR_SESSION_STATE_READY -> XR_SESSION_STATE_SYNCHRONIZED -> XR_SESSION_STATE_STOPPING -> XR_SESSION_STATE_EXITING
```

**`SYNCHRONIZED` was reached for the first time in this project's
history**, directly confirming M9D's own prediction ("M9E's frame loop
is expected to be the reason `SYNCHRONIZED`+ become observable at all —
worth confirming directly once that milestone exists"). Confirmed, not
merely plausible: it happened consistently across repeated manual runs,
always immediately after frame 1's `xrEndFrame`. **`VISIBLE`/`FOCUSED`
were never reached**, across every run performed. This is reported as
an honest, real limitation of the test environment, not silently
assumed away: SteamVR's null/simulated HMD driver has no actual display
surface and nothing resembling real user attention, and `VISIBLE`/
`FOCUSED` describe exactly that kind of runtime-decided "is a real user
currently looking at this" signal — a genuine headset (or a
runtime/simulator that models user attention) would be needed to
observe them, which is outside what M9E's development environment
(established in M9B) can provide. This is not treated as a defect in
M9E's own frame-loop implementation, since every state this environment
is capable of granting was in fact reached.

### SteamVR-Internal Vulkan Validation Noise (M9E)

**Real validation-layer output was observed during every manual run —
traced to its source, not dismissed, and confirmed to originate
entirely from SteamVR's own compositor internals, not from any AREngine
Vulkan call.** Two distinct error classes appeared, both tagged
`[OpenXR/Vulkan]` (the runtime's own logging prefix, same convention
established in M9C/M9D):

1. During/after `xrBeginSession`, before AREngine's own first
   `vkQueueSubmit`: `vkCreateImage()` complaining about a
   `VkExternalMemoryImageCreateInfo` (`VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT`)
   paired with `initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED`, and
   `vkQueueSubmit()` complaining about an image debug-named
   `BlankEyeBuffer` being in the wrong layout.
2. During shutdown (after every OpenXR/Vulkan object this demo owns had
   already been destroyed cleanly): `vkFreeMemory()` complaining a
   `VkDeviceMemory` was still in use by an image that was still in use
   by a command buffer.

**Conclusive evidence this is not AREngine's code**: the image name
(`BlankEyeBuffer`) was never set by any AREngine call — nothing in
`OpenXRSwapchain` or the demo ever calls
`vkSetDebugUtilsObjectNameEXT`. The command buffer handles named in
these errors (multiple different addresses across a single run) never
match the demo's own single, reused command buffer (allocated once,
logged nowhere near these values). AREngine's code never calls
`vkCreateImage`, `vkAllocateMemory`, or `vkFreeMemory` for *any* image
in M9E — every image involved is either OpenXR-owned (the swapchain
images) or entirely internal to SteamVR's own compositor
(`BlankEyeBuffer`, evidently its own internal placeholder image shown
in place of application content — plausibly exercised specifically
*because* M9E submits zero composition layers, though confirming that
hypothesis is left to M9F, once this codebase actually submits a real
layer and can observe whether the behavior changes). This is the same
category of finding as M9D's `ShaderViewportIndexLayerEXT` errors: a
pre-existing quirk in SteamVR's own Vulkan usage, surfaced only because
validation is enabled, not caused by or fixable through any change to
AREngine's own Vulkan calls. **AREngine's own Vulkan usage in M9E
produced zero validation errors or warnings** — confirmed by tracing
every reported error to a handle this codebase never created.

### `FrameDriver` Fit Evaluation (M9E)

**Required by the brief to happen after the raw OpenXR lifecycle was
proven out, not before — performed now, with real evidence in hand.**
`Frame::FrameDriver` (`engine/frame/include/AREngine/Frame/FrameDriver.hpp`)
is three methods: `WaitForNextFrame() -> FrameTiming`,
`GetViews() -> std::vector<ViewInfo>`, `SubmitFrame()`. Mapped against
what M9E actually built:

- **`WaitForNextFrame()` maps cleanly onto `xrWaitFrame`.** `XrFrameState`'s
  `predictedDisplayTime` converts losslessly into
  `FrameTiming::predictedDisplayTimeSeconds` via `XrTimeToSeconds`
  (exactly the field `FrameTiming.hpp`'s own comment already anticipated:
  "the XR module will translate OpenXR's time representation into this
  later"). `shouldRender`, however, has **no home in `FrameTiming`
  today** — `FrameDriver`'s interface has no way for a caller to learn
  "the driver would prefer you skip rendering this frame." A future
  `XRFrameDriver` could route around this (e.g. return an empty
  `GetViews()` when `shouldRender` is false, or extend `FrameTiming`
  with a `bool shouldRender` field), but that is a real, concrete gap,
  not a clean fit.
- **`GetViews()` does not yet have a real implementation to evaluate.**
  M9E deliberately does not call `xrLocateViews` (M9F's job), so there
  is no real `ViewInfo` data to compare against `Frame::ViewInfo`'s
  shape (`position`/`orientation`/`projection`) yet. The shape itself
  looks plausible for what `xrLocateViews` (`XrView`: `XrPosef` +
  `XrFovf`) will eventually provide, converted through world-convention
  math — but this is deferred to M9F with real evidence, not confirmed
  here.
- **`SubmitFrame()` does not map onto a single OpenXR call cleanly.**
  M9E's real per-frame body is *acquire → wait → [render] → release →
  xrEndFrame* — a sequence with a natural midpoint (the acquired
  `VkImage`s must be rendered into by the *caller*, between `GetViews()`
  and `SubmitFrame()`, in whatever `FrameDriver`-based design eventually
  exists) that `FrameDriver`'s current three-method shape has no explicit
  place for. `DesktopFrameDriver` doesn't hit this problem because
  desktop's swapchain-image acquire/present is handled entirely inside
  `Runtime`'s existing render step, outside `FrameDriver` itself, in a
  way M9E's OpenXR path cannot replicate (OpenXR's acquire/wait/release
  happens *per swapchain*, not once per frame, and must interleave
  with the caller's own render commands, not merely bookend them).

**Conclusion**: `FrameDriver`'s three-method shape does **not** cleanly
fit OpenXR's real frame lifecycle as built in M9E, for two concrete,
evidence-based reasons — no home for `shouldRender`, and no explicit
seam for interleaving per-swapchain acquire/render/release with
`SubmitFrame()`'s single call. Per the brief, this is reported as a
finding, not silently patched over with a premature `XRFrameDriver`
implementation or a `FrameDriver` interface redesign — that decision is
left to whichever future milestone actually needs `XRFrameDriver` to
exist, once `xrLocateViews`-backed `GetViews()` data (M9F) makes the
full shape of the problem clear. No production code in `engine/frame/`
or `runtime/` was touched by M9E.

### Desktop Path Unaffected (M9E)

M9E touched only `engine/xr/` and `tests/`. No file under
`engine/rendering/`, `engine/frame/`, or `runtime/` was changed.
`RenderingTests`/`RuntimeTests`/`FrameTests` all continued passing
across every build-option combination below, and
`arengine_vulkan_present_demo`/`NullRenderDevice`/`AREngineSandbox` all
still build unchanged.

### Validation Results (M9E)

All four `ARENGINE_ENABLE_OPENXR` × `ARENGINE_ENABLE_VULKAN` combinations:

- **ON/ON**: full build succeeds, `ctest` **14/14** (new pure-logic
  checks: 6 environment-blend-mode/`XrTimeToSeconds` checks added to
  `OpenXRSessionTests`, 4 swapchain-color-format-selection checks added
  to `OpenXRVulkanTests`).
- **ON/ON, `/EHsc /W4 /WX`**: zero warnings, zero errors, across
  `arengine_xr` and every OpenXR test/demo target.
- **ON/OFF**: builds correctly — `arengine_openxr_frame_demo` and the
  swapchain-format tests are correctly absent (Vulkan-coupled),
  `OpenXRSessionTests` (Vulkan-independent, includes the new blend-mode/
  timing checks) still builds and passes, `ctest` **12/12**.
- **OFF/ON** (default `build/`): builds correctly, entirely unaffected,
  `ctest` **11/11**.

`arengine_openxr_frame_demo` run against the live SteamVR/OpenXR 2.16.7
null-driver runtime (repeated twice after the `xrWaitFrame` fix below,
both clean):

```
Selected swapchain format: VK_FORMAT_B8G8R8A8_SRGB
Created swapchain for view 0: 1852x2056, 3 image(s)
Created swapchain for view 1: 1852x2056, 3 image(s)
Selected environment blend mode: OPAQUE
Observed session state sequence: XR_SESSION_STATE_READY -> XR_SESSION_STATE_SYNCHRONIZED -> XR_SESSION_STATE_STOPPING -> XR_SESSION_STATE_EXITING
Total completed frames: 200
```

Exit code 0 both times. **Zero validation errors or warnings traced to
AREngine's own Vulkan usage** (see "SteamVR-Internal Vulkan Validation
Noise" above for the SteamVR-internal errors that were observed, traced
to their source, and confirmed unrelated). An earlier run (before the
`xrWaitFrame`-requires-running fix) crashed via `AR_ASSERT_MSG`
immediately after the 200th frame and `xrRequestExitSession` — a real
bug, found by real testing, fixed, and re-verified, not swept under a
"known issue" label.

### What's Deferred to M9F+

`xrLocateViews`, real per-view pose/FOV data, a real `XrSpace` for
locating against, `XrCompositionLayerProjection` submission with actual
content, stereo scene rendering, any `VkImageView`/render-pass/
framebuffer path over the swapchain images, resolving the `shouldRender`/
`FrameDriver` gap identified above, resolving the "does zero-layer
submission cause the SteamVR `BlankEyeBuffer` errors" open hypothesis,
`XRFrameDriver` itself, controllers/actions, hand/eye tracking,
passthrough, spatial anchors, physics, PBR shading, Scene integration.
`OpenXRSwapchain`, the environment-blend-mode selection, and the
now-working `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame` lifecycle are all
genuinely ready for M9F to build on directly.

## 29. M9E.5 Implementation Notes

M9E.5 is a focused architecture milestone: redesign `Frame::FrameDriver`
using the real evidence M9E gathered, refactor `DesktopFrameDriver`/
`Runtime` onto the new interface without regressing desktop behavior,
and introduce an initial `XRFrameDriver` that wraps only `xrWaitFrame`/
`xrBeginFrame`/`xrEndFrame` — not swapchain image acquisition, not
`xrLocateViews`, not rendering. No stereo rendering, no Scene
integration; those remain M9F+.

### Why The Old FrameDriver Was Insufficient (M9E.5)

The original interface (`WaitForNextFrame() -> FrameTiming`,
`GetViews() -> vector<ViewInfo>`, `SubmitFrame()`) was designed in M1,
before any real backend existed. M9E's own "FrameDriver Fit Evaluation"
(Section 28) found four concrete mismatches once a real OpenXR frame
lifecycle actually existed to compare against:

1. **No representation of `shouldRender`.** `XrFrameState::shouldRender`
   is real, load-bearing data (SteamVR's null driver reported it false
   on the large majority of M9E's 200 frames) with nowhere to go.
2. **No explicit begin-frame seam.** `WaitForNextFrame()` conflated
   "block for timing" with everything that follows; OpenXR's real
   sequence has a distinct `xrBeginFrame` step between waiting and
   acquiring/rendering.
3. **`SubmitFrame()` too coarse.** OpenXR's real per-frame body is
   acquire → wait → [render] → release → end — a single opaque call
   has no seam for the caller's own render work in the middle.
4. **A session-running precondition the old interface had no way to
   express.** M9E discovered, by crashing and fixing it, that
   `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame` all require the session to
   be running - calling `xrWaitFrame` right after `STOPPING ->
   xrEndSession` fails with `XR_ERROR_SESSION_NOT_RUNNING`. The old
   interface had no concept of "no frame lifecycle at all this tick."

### New Lifecycle API (M9E.5)

`FrameDriver` (`engine/frame/include/AREngine/Frame/FrameDriver.hpp`):

```cpp
class FrameDriver {
public:
    virtual ~FrameDriver() = default;
    virtual FrameContext PrepareFrame() = 0; // was WaitForNextFrame()
    virtual void BeginFrame() = 0;           // NEW
    virtual std::vector<ViewInfo> GetViews() = 0; // unchanged shape
    virtual void EndFrame() = 0;             // was SubmitFrame()
};
```

`FrameContext` (new, `FrameContext.hpp`): `{ FrameTiming timing;
FrameStatus status = FrameStatus::Continue; }` — bundled together
because they are always produced and consumed together, mirroring how
`XrFrameState` itself bundles `predictedDisplayTime`/
`predictedDisplayPeriod`/`shouldRender` into one struct.

`FrameStatus` (new, `FrameStatus.hpp`): `enum class FrameStatus {
Continue, Idle, Stop };`

- **`Continue`** — a full frame lifecycle should happen this tick:
  `BeginFrame()`, then (only if `timing.shouldRender`) the caller's own
  render work, then `EndFrame()` — regardless of `shouldRender`.
- **`Idle`** — no `BeginFrame()`/`GetViews()`/`EndFrame()` call should
  happen at all this tick, not even an "empty" one. Desktop never
  returns this; XR returns it while the session exists but is not
  currently running (not yet begun, or between `STOPPING` and
  `EXITING`) — `xrBeginFrame`/`xrEndFrame` are not legal to call in
  that window at all, not merely "skip the content." (Named `Idle`, not
  the earlier-drafted `SkipRendering` — renamed specifically to avoid
  reading as a synonym for `shouldRender=false`, which is a genuinely
  different situation; see "shouldRender Semantics" below.)
- **`Stop`** — the frame source can no longer produce frames; the
  caller should end its own loop. Desktop never returns this; XR
  returns it once the session reaches `EXITING`/`LOSS_PENDING` (via the
  already-tested `ShouldStopMainLoop`).

**Contract** (documented on `FrameDriver` itself):
`BeginFrame()`/`GetViews()`/`EndFrame()` must only be called after
`PrepareFrame()` returns `FrameStatus::Continue`. `GetViews()` is only
valid after `BeginFrame()`. `EndFrame()` must be called exactly once
per `BeginFrame()`, regardless of `shouldRender` — OpenXR requires a
matching `xrBeginFrame`/`xrEndFrame` pair either way.

### shouldRender Semantics (M9E.5)

`FrameTiming` gained exactly one new field: `bool shouldRender = true`.
Deliberately a different axis from `FrameStatus::Idle` — the two answer
different questions:

- `FrameStatus` answers "is a `Begin`/`End` pair even legal to call
  this tick at all."
- `FrameTiming::shouldRender` answers "given a legal `Begin`/`End`
  pair, should its content actually be rendered."

OpenXR's real lifecycle needs both, and they are not interchangeable:
`xrBeginFrame`/`xrEndFrame` require a running session (a `FrameStatus`
question — see the crash in Section 28), while a *running* session's
own `xrWaitFrame` can independently report `shouldRender=false` (a
`FrameTiming` question — this is what actually happened on the large
majority of M9E's 200 frames; `xrBeginFrame`/`xrEndFrame` remained
fully legal and were called every time). Collapsing these into one
concept would either reintroduce the `xrWaitFrame`-after-`xrEndSession`
crash risk (by making `Idle` and `shouldRender=false` the same signal)
or require hiding the running/not-running distinction inside no-op
`BeginFrame`/`EndFrame` bodies, which would remove the pure-logic
testability `DetermineFrameStatus` (below) depends on. `shouldRender`
defaults `true` so any default-constructed `FrameTiming` behaves
sanely; no `predictedDisplayPeriodSeconds` field was added — nothing
would consume it yet.

### Frame Timing Semantics (M9E.5)

Unchanged in kind, only in source: `deltaTimeSeconds`/
`totalTimeSeconds` still come from a monotonic wall clock (Desktop's
`Platform::SteadyClock`; `XRFrameDriver` owns its own self-contained
`std::chrono::steady_clock`-based timer rather than depending on
`Platform` — see "Why `engine/xr` Does Not Depend On `Platform`"
below). `predictedDisplayTimeSeconds` is real for the first time on the
XR path: `XrTimeToSeconds` (M9E, `OpenXRFrameTiming.hpp`) converts
`XrFrameState::predictedDisplayTime` using the OpenXR spec's own fixed
nanoseconds-per-tick definition.

### Render-Target Acquisition Ownership (M9E.5)

**Deliberately excluded from `FrameDriver`**: GPU render-target
acquisition (OpenXR's `xrAcquireSwapchainImage`/
`xrWaitSwapchainImage`/`xrReleaseSwapchainImage`) stays entirely on
`OpenXRSwapchain` (M9E), coordinated by whichever layer actually owns
rendering — currently the manual frame demo, since no dedicated
Renderer/XR-integration module exists yet. Confirmed against the real
M9E code, not just argued abstractly: in the demo, the acquire/wait/
Vulkan-clear/release block sits *between* `xrBeginFrame` and
`xrEndFrame`, gated on `shouldRender` — resource-lifecycle work,
sharing the same envelope as frame timing but not itself a frame-timing
concern. Putting it inside `XRFrameDriver` would also contradict the
"generic Frame module describes WHEN a frame happens, not GPU resource
ownership" principle this redesign is built on: swapchain images,
`VkImageView`s, and Vulkan handles must never appear in `Frame`'s
public API. `XRFrameDriver` wraps exactly `xrWaitFrame`/`xrBeginFrame`/
`xrEndFrame` and nothing else.

### Desktop Mapping (M9E.5)

`DesktopFrameDriver::PrepareFrame()` ticks `Platform::SteadyClock`
exactly as before, returns `FrameContext{timing,
FrameStatus::Continue}` with `timing.shouldRender = true` always.
`BeginFrame()`/`EndFrame()` are both empty bodies — nothing to do yet,
same as the old `SubmitFrame()`'s empty body. `GetViews()` is
unchanged (one placeholder `ViewInfo`). No new complexity was added
just to make Desktop resemble XR — it never returns `Idle` or `Stop`,
and its `shouldRender` is never false.

### XR Mapping (M9E.5)

`XRFrameDriver` (new, `engine/xr/src/openxr/XRFrameDriver.hpp/.cpp`,
namespace `AREngine::XR::OpenXR`) implements `Frame::FrameDriver`.
Constructor: `XRFrameDriver(XrInstance instance, OpenXRSession&
session, XrViewConfigurationType primaryViewConfigurationType,
XrEnvironmentBlendMode environmentBlendMode)` — borrows `instance`/
`session` (same discipline as every other OpenXR wrapper in this
codebase); the blend mode is pre-selected by the caller via M9E's
`OpenXREnvironmentBlendMode.hpp` functions, not enumerated/selected by
this class itself.

- **`PrepareFrame()`**: polls session-state events (see "Ordered
  Session-State Processing" below), reacts to every transition
  observed (in order), then either returns `FrameStatus::Stop`
  (terminal state reached), `FrameStatus::Idle` (session not currently
  running — no real `xrWaitFrame` call is made; see "CPU Behavior While
  Idle" below), or calls the real `xrWaitFrame` and returns
  `FrameStatus::Continue` with real `deltaTimeSeconds`/
  `totalTimeSeconds`/`predictedDisplayTimeSeconds`/`shouldRender`.
- **`BeginFrame()`**: calls `xrBeginFrame`. Only valid after `Continue`
  (contract, not defensively re-checked — same trust-the-caller
  discipline the rest of this codebase already applies to internal
  contracts).
- **`GetViews()`**: always returns an empty vector. No real view data
  exists yet — `xrLocateViews` is M9F's job. Empty is spec-legal per
  `ViewInfo`'s own "a frame may need zero, one, or several of these"
  documentation.
- **`EndFrame()`**: calls `xrEndFrame` with zero composition layers —
  the same decision M9E made (no real per-view pose/FOV data exists yet
  to build a valid `XrCompositionLayerProjection`, and this class does
  not fabricate one), using the `displayTime` stashed from
  `PrepareFrame()`'s `xrWaitFrame` call.
- **`RequestExit()`**: not part of `FrameDriver` — forwards to
  `OpenXRSession::RequestExit()`. Requesting a session exit is an
  application-level decision (e.g. "I've run N diagnostic frames"), not
  something a generic frame-pacing driver should decide on its own.
  Only reachable through a concrete `XRFrameDriver` reference, never
  through a `Frame::FrameDriver*` pointer.

### Ordered Session-State Processing (M9E.5)

**A real correctness fix, caught during design review before any code
was written, not discovered by accident.** `SessionEventPollResult`
(M9D, `OpenXRSession.hpp`) originally reported only the *last*
`XrEventDataSessionStateChanged` observed in one `xrPollEvent` draining
cycle — acceptable for M9D's own demo (which only logs/reacts to "the
current state"), but wrong for `XRFrameDriver`: a runtime can
legitimately deliver more than one session-state-changed event within
a single cycle (e.g. `READY` immediately followed by `STOPPING`).
Reacting only to the last one would see `STOPPING` with
`sessionRunning` still `false` (since `READY`'s `xrBeginSession` call
was never made), produce no action at all, and leave the session
permanently stuck — never begun, never ended, with no way to recover.

**Fix** (additive, backward-compatible — M9D's own demo is unaffected):
`SessionEventPollResult` gained `std::vector<XrSessionState>
sessionStateSequence`, holding every state-changed event observed that
cycle, in the exact order `xrPollEvent` returned them
(`sessionStateSequence.back() == newSessionState` always holds when
`sessionStateChanged` is true). A new pure function,
`DetermineSessionLifecycleActions(sequence, initiallyRunning) ->
vector<SessionLifecycleAction>` (`OpenXRSessionState.hpp`), computes
the full `Begin`/`End`/`None` action plan for the whole sequence
up front (tracking the running flag exactly as `OpenXRSession` itself
would after each real call), and `XRFrameDriver::PrepareFrame()`
applies it in order, updating `m_currentState` and logging each
transition alongside each action. Directly unit-tested, including the
exact READY-then-STOPPING-same-cycle scenario described above
(`TestDetermineSessionLifecycleActionsReadyThenStoppingSameCycle`,
`tests/openxr_session_tests.cpp`) — confirms two actions (`Begin`, then
`End`) are produced, not the one-or-zero a last-state-only approach
would produce. **Confirmed genuinely exercised, not just theoretically
possible**: a real manual run against SteamVR (see "Validation Results"
below) observed an extra `IDLE` transition between `STOPPING` and
`EXITING` in the same poll cycle that would previously have been
silently absorbed — now correctly logged and processed as its own
`None`-action step.

### CPU Behavior While Idle (M9E.5)

**Verified, not assumed.** Because `xrWaitFrame` cannot legally be
called while the session isn't running, `PrepareFrame()`'s
`FrameStatus::Idle` branch has no blocking OpenXR call to pace it
(unlike the `Continue` path, which blocks inside the real
`xrWaitFrame`). Without a guard, a caller looping on `Idle` while
waiting for `READY` (or for `EXITING` after `STOPPING`) would busy-spin
`PollSessionEvents` at full CPU. **Fix, implementation-private to
`XRFrameDriver`, not part of the generic `Frame` API** (per the brief:
"do not make timing/sleep policy part of the generic Frame API"): a
16ms `std::this_thread::sleep_for` inside `PrepareFrame()`'s own `Idle`
branch, matching the polling cadence already established by M9D's/
M9E's own demos. `Frame::FrameStatus`/`FrameContext`/`FrameDriver`
contain no timing/sleep concept whatsoever — Desktop's `PrepareFrame()`
has nothing analogous, since it never returns `Idle`.

### Why `engine/xr` Does Not Depend On `Platform` (M9E.5)

`XRFrameDriver` needs a monotonic delta/total-time clock, same as
`Platform::SteadyClock` already provides — but `engine/xr` does not
gain a new dependency on `engine/platform` just for ~10 lines of
`std::chrono` wrapping. `XRFrameDriver` owns a private, self-contained
`std::chrono::steady_clock`-based timer instead, matching the
"duplicate small self-contained logic rather than add a cross-module
dependency" precedent already established for
`FindGraphicsQueueFamily`/`TransitionImageLayout` elsewhere in this
module.

### Where DetermineFrameStatus Lives (M9E.5)

`DetermineFrameStatus(currentState, sessionRunning) ->
Frame::FrameStatus` and `DetermineSessionLifecycleActions` both live in
`OpenXRSessionState.hpp/.cpp` — **not** co-located with `XRFrameDriver`
— specifically so they (and their tests) stay buildable/testable in
the *narrowest* config. `OpenXRSessionState.hpp` is Vulkan-independent
(unconditional `ARENGINE_ENABLE_OPENXR` block); `XRFrameDriver.hpp` is
not — it has a hard *build* dependency on `OpenXRSession`, which only
exists when `ARENGINE_ENABLE_VULKAN=ON` (it needs
`XrGraphicsBindingVulkan2KHR`). So `XRFrameDriver.hpp/.cpp` are nested
inside `if(ARENGINE_ENABLE_VULKAN)` in `engine/xr/CMakeLists.txt`,
alongside `OpenXRSession`/`OpenXRSwapchain`, even though `XRFrameDriver`
itself makes no direct Vulkan API call. `tests/openxr_session_tests.cpp`
gained the `DetermineFrameStatus`/`DetermineSessionLifecycleActions`
tests directly (no new test executable) — it already covers this
file's other pure-logic functions and already builds Vulkan-independent.

### Why Frame Came Back (M9E.5)

`engine/xr/CMakeLists.txt` gains `target_link_libraries(arengine_xr
PRIVATE AREngine::Frame)` — a real reversal of M9A's deliberate trim
("Frame ... deferred until a real XRFrameDriver is built... can come
back the moment a later milestone gives a genuine reason"), documented
as such at the trim's own comment site, not silently. The link is at
the *outer* `if(ARENGINE_ENABLE_OPENXR)` level (not nested in the
Vulkan block) because `OpenXRSessionState.cpp` needs
`Frame::FrameStatus` regardless of `ARENGINE_ENABLE_VULKAN`. `Platform`
remains untouched — nothing in `engine/xr` needs window/native-handle
access yet.

### Runtime Loop Changes (M9E.5)

```cpp
while (true) {
    m_inputSystem.BeginFrame();
    m_window->PollEvents();
    if (m_window->ShouldClose()) break;           // unchanged
    // ... existing input-logging block, unchanged ...

    const Frame::FrameContext frameContext = m_frameDriver->PrepareFrame();
    if (frameContext.status == Frame::FrameStatus::Stop) break;
    if (frameContext.status == Frame::FrameStatus::Idle) continue;

    m_frameDriver->BeginFrame();
    if (frameContext.timing.shouldRender) {
        const std::vector<Frame::ViewInfo> views = m_frameDriver->GetViews();
        (void)views;
        m_renderDevice->BeginRendering();
        /* existing dummy draw, unchanged */
        m_renderDevice->EndRendering();
    }
    // fps accounting - unconditional, only render work is gated
    m_frameDriver->EndFrame();
}
```

Preserves the M7 event order exactly: Input.BeginFrame → Platform
messages → Close check → **Frame lifecycle** (`PrepareFrame` +
early-outs + `BeginFrame`) → **Application/update/render** (the
`shouldRender`-gated block) → **Frame completion** (`EndFrame`). The
window-close signal stays entirely on `Window::ShouldClose()`, untouched
— `FrameStatus::Stop` is for driver-level termination (XR), not desktop
window close; Desktop never returns it. `Runtime` itself gained no new
XR-specific concept — it only ever sees the generic `FrameContext`/
`FrameStatus`/`shouldRender`.

### Deferred View/Stereo Design (M9E.5)

`Frame::ViewInfo` was **not** redesigned around OpenXR — M9F will
provide real evidence via `xrLocateViews`. The only structural change
this milestone made anywhere near views was moving `GetViews()` after
`BeginFrame()` in the interface's documented contract (a consequence of
the `BeginFrame()` seam existing at all, not a `ViewInfo`-specific
decision). `XRFrameDriver::GetViews()` returns an empty vector, not a
placeholder XR-shaped view — an honest "no data yet," not a fabricated
one.

### Test-Coverage Scoping (M9E.5)

"Runtime skips render work when shouldRender=false" and "Runtime can
stop when frame source indicates termination" are verified at the
`FrameContext`/`DummyFrameDriver` unit level
(`tests/frame_tests.cpp`), not by literally driving `Runtime::Run()`
with a fake driver — `Runtime::m_frameDriver` is constructed internally
with no dependency-injection seam, and adding one is out of scope for
this milestone. `tests/runtime_tests.cpp`'s `DesktopFrameDriver` test
confirms the real desktop driver always reports `Continue`/
`shouldRender=true` across 5 iterations, exercising `BeginFrame()`/
`EndFrame()` for the first time. `tests/openxr_session_tests.cpp`
gained 8 new pure-logic checks (`DetermineFrameStatus`'s 3 outcomes,
`DetermineSessionLifecycleActions`'s single-action/ordered-multi-
action/non-actionable/empty cases) — zero real OpenXR calls, no
SteamVR required.

### Validation Results (M9E.5)

All four `ARENGINE_ENABLE_OPENXR` × `ARENGINE_ENABLE_VULKAN`
combinations: full build succeeds, `/EHsc /W4 /WX` clean (verified on
ON/ON), `ctest` green on every combination (ON/ON: 14/14 — includes the
new `FrameTests`/`RuntimeTests`/`OpenXRSessionTests` checks; ON/OFF:
12/12; OFF/ON default: 11/11).

`arengine_openxr_frame_demo`, rewired through `XRFrameDriver`, run
twice against the live SteamVR/OpenXR runtime — both clean (exit code
0), 200/200 frames completed both times:

```
Session state changed -> XR_SESSION_STATE_IDLE
Session state changed -> XR_SESSION_STATE_READY
xrBeginSession succeeded - session is now running
Completed frame 1 (shouldRender=true)
Session state changed -> XR_SESSION_STATE_SYNCHRONIZED
Completed frame 50/100/150/200 (shouldRender=false)
Reached target frame count (200) - requesting a clean session exit...
Session state changed -> XR_SESSION_STATE_STOPPING
xrEndSession succeeded - session is no longer running
Session state changed -> XR_SESSION_STATE_IDLE
Session state changed -> XR_SESSION_STATE_EXITING
Frame driver reports FrameStatus::Stop - stopping the main loop cleanly (not an error)
Total completed frames: 200
```

Note the `IDLE` transition observed between `STOPPING` and `EXITING` in
both runs — exactly the kind of multi-transition-in-one-cycle detail
the ordered-processing fix above was designed to surface correctly (as
a harmless `None` action) rather than silently absorb. Same category of
SteamVR-internal Vulkan validation noise as M9E (`BlankEyeBuffer`,
command-buffer handles this codebase never allocated) was observed
again, unchanged in kind — zero validation errors traced to AREngine's
own code. `AREngineSandbox` and `arengine_vulkan_present_demo` were
both launched and confirmed to start and run without crashing (process
stayed alive under manual inspection; full log capture is limited by
stdio buffering under a forced kill, not a regression - unrelated to
this milestone's changes, since neither binary's own code was touched).

- Files changed: `engine/frame/include/AREngine/Frame/{FrameDriver,
  FrameTiming,Frame}.hpp`, new `FrameStatus.hpp`/`FrameContext.hpp`,
  `engine/frame/CMakeLists.txt`; `runtime/src/{Runtime,
  DesktopFrameDriver}.cpp`, `runtime/include/AREngine/Runtime/
  DesktopFrameDriver.hpp`; new `engine/xr/src/openxr/
  XRFrameDriver.hpp/.cpp`; `engine/xr/src/openxr/
  OpenXRSession.hpp/.cpp`, `OpenXRSessionState.hpp`;
  `engine/xr/CMakeLists.txt`; `tests/openxr_frame_demo.cpp`,
  `tests/frame_tests.cpp`, `tests/runtime_tests.cpp`,
  `tests/openxr_session_tests.cpp`, `tests/CMakeLists.txt`.

### Architectural Issues Found (M9E.5)

None beyond what this milestone itself set out to fix. The one design
question genuinely worth flagging forward: `XRFrameDriver::GetViews()`
returning an empty vector today means `Runtime`'s existing `(void)
views;` no-op is trivially satisfied — M9F's real `xrLocateViews`
integration will be the first actual test of whether `ViewInfo`'s
current shape (`position`/`orientation`/`projection`) is sufficient, or
whether *that* milestone surfaces its own concrete mismatch the way
M9E did for `FrameDriver`. Not fixed here, not assumed away — flagged
for M9F to discover with its own real evidence.

## 30. M9F Implementation Notes

M9F integrates real OpenXR view location: `xrLocateViews` is called at
the current frame's predicted display time, the runtime's real per-view
pose and asymmetric FOV are converted into generic `Frame::ViewInfo`,
and a genuine `XrCompositionLayerProjection` — built from that real
data — is submitted to the compositor. No scene geometry is rendered;
the swapchain images still just receive M9E's solid-color clear. The
goal is proving real view data flows correctly end to end and is
accepted by the compositor, not drawing anything.

A design-review pass (before implementation) independently re-derived
the asymmetric-projection matrix math from first principles and
confirmed it correct with no sign errors. A second, later review pass
(also before implementation) caught that the original draft let
`XRFrameDriver` grow to own swapchain topology and composition-layer
metadata directly — reworked into a smaller, separate
`OpenXRProjectionLayer` helper instead; see "Why OpenXRProjectionLayer
Is Separate From XRFrameDriver" below.

### ViewInfo Architecture Decision (M9F)

**Old shape** (unchanged since M1):
```cpp
struct ViewInfo {
    Core::Math::Vec3 position;
    Core::Math::Quaternion orientation = Core::Math::Quaternion::Identity();
    Core::Math::Mat4 projection = Core::Math::Mat4::Identity();
};
```

**Evaluated against real `xrLocateViews` evidence and found already
sufficient — zero field changes.** `position`/`orientation` already
represent exactly what `xrLocateViews` provides: the view's pose in the
reference space used to locate it (see "View-Pose Semantics" below) —
not a view matrix, and `projection` was already a plain `Mat4`, capable
of holding a real asymmetric projection without any structural change
once a function to build one existed (`PerspectiveOffCenterRH_ZO`,
below).

One field was seriously considered and explicitly **rejected**: a
precomputed `viewMatrix` (the milestone's own brief offered this as "a
possible direction"). Rejected because nothing in M9F renders anything
— no code anywhere in this engine yet consumes a view matrix — and
`Scene::Camera` already establishes the relevant precedent:
`Camera::GetViewMatrix(transform)` computes `viewFromWorld` from a
separately-stored pose on demand, rather than storing both the pose and
its derived matrix redundantly ("Camera does NOT own or duplicate
position/rotation... the same way every other piece of AREngine avoids
storing the same data twice" — `Camera.hpp`'s own doc comment).
Deriving `viewFromWorld` from `ViewInfo`'s pose the same way is a real
design decision, correctly deferred to M9G, when a real renderer first
needs one and can supply genuine evidence for its exact shape — not
invented speculatively here. Only `ViewInfo.hpp`'s doc comment changed,
clarifying this reasoning and that `projection` still carries no Vulkan
Y-flip (same policy as `Camera`).

### xrLocateViews Lifecycle Position (M9F)

Called from `XRFrameDriver::GetViews()`, which is only ever called by a
caller (the demo) after `BeginFrame()` has succeeded — consistent with
`FrameDriver`'s own documented contract since M9E.5. Only called when
`shouldRender` is true: `xrLocateViews` is real runtime work with no
benefit when nothing will be drawn this frame, and the milestone brief
explicitly asks not to call it unnecessarily. Uses the standard
two-call idiom (`xrLocateViews` with `viewCapacityInput=0` to query the
count, then again with a sized buffer) — the same idiom every other
`xrEnumerate*`-shaped call in this codebase already uses.

### Predicted Display Time Usage (M9F)

`XrViewLocateInfo::displayTime` is set to
`m_lastPredictedDisplayTime` — the exact value `PrepareFrame()`'s own
`xrWaitFrame` call stashed for *this* frame, the same value `EndFrame()`
uses for `XrFrameEndInfo::displayTime`. Never CPU wall-clock time, never
a previous frame's time — the located views correspond exactly to the
frame actually being rendered/submitted, per the milestone's explicit
requirement. No new state was needed for this: `m_lastPredictedDisplayTime`
already existed (M9E.5), used for `xrEndFrame`'s `displayTime`; M9F
simply reads the same field.

### LOCAL Reference-Space Use (M9F)

Used as both `xrLocateViews`'s `space` parameter and
`XrCompositionLayerProjection::space`. M9D's `OpenXRReferenceSpace`
class (unused since M9E.5, when zero composition layers meant nothing
needed an `XrSpace`) is brought back — the demo creates one `LOCAL`
space, declared after `session` (destroyed before it, per M9D's
established ordering rule), and passes it by reference into
`XRFrameDriver`'s constructor. Deliberately **not** AREngine's final AR
world-origin policy: `STAGE` and future spatial anchors remain separate
concerns, entirely unaddressed by this milestone.

### OpenXR/AREngine Coordinate Compatibility (M9F)

**Verified before writing any conversion code, not assumed.** Per the
OpenXR specification's own coordinate-system definition, OpenXR
reference spaces are right-handed, +Y up, -Z forward, in meters —
identical, field for field, to AREngine's own world convention
(`docs/WORLD_CONVENTIONS.md`: 1 unit = 1 meter, right-handed, +X right,
+Y up, -Z forward). **No axis flip is introduced anywhere in this
milestone's conversion code** — `ConvertXrPosition` is a direct,
unmodified field copy (`OpenXRViewConversion.hpp`). This is
deliberately documented as a verified fact, not a silent assumption —
see that file's own comment and `TestConvertXrPositionTranslated`
(`tests/openxr_view_tests.cpp`), which checks a position with a
distinct, non-zero value on every axis specifically so a sign error on
any one of them would be caught.

### Quaternion Conversion (M9F)

**Confirmed against both types' actual definitions before writing the
conversion, not assumed.** `XrQuaternionf` stores `{x, y, z, w}`.
`AREngine::Core::Math::Quaternion` is Hamilton `{w, x, y, z}` storage,
with constructor `Quaternion(float w, float x, float y, float z)`. The
conversion (`ConvertXrOrientation`, `OpenXRViewConversion.hpp`) is
therefore `Quaternion(xr.w, xr.x, xr.y, xr.z)` — a component
**reorder**, not a coordinate-system conversion (no axis flip is
involved, per the previous section). Tested with a deliberately
per-component-asymmetric quaternion (`{x=0.1, y=0.2, z=0.3, w=0.9}`),
not just identity — an identity-only test could pass even with the
fields swapped into the wrong slots; this one actually catches a
reorder bug (`TestConvertXrOrientationFieldReorder`).

### View-Pose Semantics (M9F)

Documented precisely, per the milestone's own explicit warning:
`ViewInfo::position`/`orientation` is the view's pose **in the
reference space it was located against** (LOCAL, for M9F) —
conceptually "worldFromView": where the view is and how it's facing,
expressed in that space. It is **not** a view matrix, and **not**
`viewFromWorld` (its inverse) either. A renderer that needs a
conventional view matrix computes `viewFromWorld` from this pose on
demand — see "ViewInfo Architecture Decision" above for why that
computation doesn't exist yet.

### Asymmetric FOV / New Core Helper (M9F)

OpenXR's `XrFovf` provides four **independent** angles
(`angleLeft`/`angleRight`/`angleUp`/`angleDown`) — a stereo headset's
per-eye frustum is not generally symmetric around the view's forward
axis, and the existing `PerspectiveRH_ZO` (one symmetric `fovY` +
`aspect`) cannot represent that. New `Core::Math::PerspectiveOffCenterRH_ZO`
(`ViewProjection.hpp`) is the general off-center case
`PerspectiveRH_ZO`'s formula is a special case of:

```cpp
Mat4 PerspectiveOffCenterRH_ZO(
    float angleLeftRadians, float angleRightRadians,
    float angleUpRadians, float angleDownRadians,
    float nearZ, float farZ)
{
    const float tanLeft = std::tan(angleLeftRadians);
    const float tanRight = std::tan(angleRightRadians);
    const float tanUp = std::tan(angleUpRadians);
    const float tanDown = std::tan(angleDownRadians);
    Mat4 result;
    result.Set(0, 0, 2.0f / (tanRight - tanLeft));
    result.Set(0, 2, (tanRight + tanLeft) / (tanRight - tanLeft));
    result.Set(1, 1, 2.0f / (tanUp - tanDown));
    result.Set(1, 2, (tanUp + tanDown) / (tanUp - tanDown));
    result.Set(2, 2, farZ / (nearZ - farZ));
    result.Set(2, 3, (farZ * nearZ) / (nearZ - farZ));
    result.Set(3, 2, -1.0f);
    return result;
}
```

**Derivation**: at the near plane (view-space `z = -nearZ`), the
visible x-range is `[nearZ·tanLeft, nearZ·tanRight]` and must map to
NDC `x ∈ [-1, +1]`; solving that linear system for the two boundary
conditions gives `m00`/`m02` (same derivation, transposed, for
`m11`/`m12` with up/down). The depth terms (`m22`/`m23`/`m32`) are
exactly `PerspectiveRH_ZO`'s own — off-center shear affects only X/Y,
never Z/W. **Independently re-derived by a design-review pass** before
any code was written (concrete numeric case: `angleLeft=-30°,
angleRight=+60°` → `m00=0.86603, m02=0.5`, verified against both
frustum edges by hand) — confirmed correct, no sign error.
**Symmetric-case reduction verified both algebraically and by test**
(`TestPerspectiveOffCenterRH_ZOSymmetricCaseMatchesPerspectiveRH_ZO`,
`tests/core_tests.cpp`): substituting `angleLeft=-angleRight`,
`angleDown=-angleUp` collapses `m02`/`m12` to exactly 0 and
`m00`/`m11` to exactly `PerspectiveRH_ZO`'s own `focalLength/aspect`
and `focalLength`. Asymmetric-boundary tests
(`TestPerspectiveOffCenterRH_ZOAsymmetricFrustumBoundaries`) transform
each of the four frustum-edge points through the matrix and confirm
each lands exactly on its corresponding NDC boundary — the "a point on
the left frustum boundary should map to the left clip boundary" check
the milestone brief explicitly asked for, for all four sides.

Same policy as `PerspectiveRH_ZO`/`Camera`: **no Vulkan Y-flip baked
in**. That correction stays a downstream, Vulkan-renderer-layer
concern (`Rendering::Vulkan::ApplyVulkanYFlip`, M8F) — `engine/xr` does
not include Rendering's private headers (same established boundary as
M9C's `FindGraphicsQueueFamily` duplication), and more fundamentally,
nothing renders yet in M9F for the flip to matter to. When a real
XR-Vulkan renderer exists (M9G+), the flip is a Vulkan-API-level NDC
convention correction — not swapchain-topology-specific — so
`ApplyVulkanYFlip` (or an equivalent) applies to XR render targets
exactly as it already does to desktop ones; this was reasoned through
explicitly, not assumed, but implementing it is out of scope until a
real XR renderer exists to apply it.

### Near/Far Policy (M9F)

OpenXR's `XrFovf` provides angular FOV only — never near/far clip
distances. `nearZ`/`farZ` are therefore explicit
`XRFrameDriver` constructor parameters (defaults `0.05f`/`100.0f`),
not hard-coded inside any conversion function — application/engine
policy, kept easy to change, clearly separated from the
runtime-provided FOV data flowing through `ConvertXrViewToViewInfo`.

### View Validity Handling (M9F)

`XrViewState::viewStateFlags` is inspected via
`IsViewStateValid(flags)` (`OpenXRViewConversion.hpp`): requires
**both** `XR_VIEW_STATE_ORIENTATION_VALID_BIT` and
`XR_VIEW_STATE_POSITION_VALID_BIT` — per the OpenXR spec, pose data
must not be used at all when either is unset. The `*_TRACKED_BIT`
flags are deliberately **not** required: the spec permits a runtime to
report valid-but-untracked (a "last known good" pose during a brief
tracking interruption), which remains legitimate, usable data, not a
hard failure. When invalid, `GetViews()` returns an empty vector — **no
identity head pose is ever fabricated to hide the condition** — and
`EndFrame()` correctly submits zero composition layers as a
consequence (see "Composition-Layer Seam" below). Logging is
transition-only (`m_lastViewStateValid` tracks the last-observed
validity; a log line fires only when it flips), avoiding per-frame
spam while still surfacing the condition clearly when it actually
changes. Not exercised against SteamVR's null driver this session —
pose data was valid on every observed frame; a genuine, honestly-
reported gap, consistent with this project's established practice.

### Why OpenXRProjectionLayer Is Separate From XRFrameDriver (M9F)

**Caught and corrected during design review, before implementation.**
The first draft added swapchain sub-image metadata directly to
`XRFrameDriver`'s constructor. Reworked: M9E.5 established
`XRFrameDriver` as a frame-*lifecycle* object (session events,
`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`, and now `xrLocateViews` →
generic `ViewInfo` conversion) — it does not own swapchain topology or
permanent render-target metadata, which is a genuinely separate
resource-lifecycle concern (same reasoning M9E.5 already applied to
swapchain image acquisition staying on `OpenXRSwapchain`, not
`XRFrameDriver`). `OpenXRProjectionLayer` (new,
`engine/xr/src/openxr/OpenXRProjectionLayer.hpp/.cpp`) is the smallest
dedicated place for that: it owns per-view `XrSwapchainSubImage`
metadata (fixed for the session's lifetime) and, each frame, rebuilds
an `XrCompositionLayerProjectionView` array + `XrCompositionLayerProjection`
from that metadata plus the frame's real located `XrView` data.
Vulkan-independent (its own types —
`XrSwapchainSubImage`/`XrCompositionLayerProjection*` — are all core
`openxr.h` types, not `openxr_platform.h` Vulkan-flavored ones), so it
lives in the unconditional OpenXR CMake block, same tier as
`OpenXRSessionState.hpp`, and its `Prepare()` logic (count-mismatch
handling, view-array construction) is directly pure-logic-testable
with synthetic data — no Vulkan or real OpenXR call needed
(`tests/openxr_view_tests.cpp`).

**Borrowed-lifetime discipline, documented explicitly**: the
`XrSwapchainSubImage`s `OpenXRProjectionLayer` holds contain raw
`XrSwapchain` handles it does not own — `XrSwapchain` is an opaque
OpenXR handle, not a C++ object with its own lifetime tracking; the
actual owning C++ object is `OpenXRSwapchain` (M9E), and it is
`OpenXRSwapchain`'s destructor (`xrDestroySwapchain`) that these
borrowed handles must not outlive. The demo declares `swapchains`
before `projectionLayer`, guaranteeing correct destruction order.

### No XrView/ViewInfo Round-Trip (M9F)

`Frame::ViewInfo`'s generic shape does not, and should not, carry
every OpenXR-specific field `XrCompositionLayerProjectionView` needs
(raw `XrPosef`/`XrFovf`, not AREngine math types). Round-tripping
`XrView → ViewInfo → reconstructed XrView` would be both lossy
(`ViewInfo` doesn't need to preserve OpenXR's exact struct layout) and
pointless extra work. Instead, `XRFrameDriver` retains the current
frame's raw `XrView` data privately (`m_lastLocatedViews`, populated
inside `GetViews()`'s own `xrLocateViews` call) and exposes it via a
narrow, XR-only accessor — `GetLastLocatedXrViews() const -> const
std::vector<XrView>&` — not part of the generic `FrameDriver`
interface, consumed directly by `OpenXRProjectionLayer::Prepare()`.

### Composition-Layer Seam (M9F)

**`Frame::FrameDriver::EndFrame()` gained zero new parameters and
stays a completely generic, OpenXR-free override.** The minimal
XR-private seam: `XRFrameDriver::SetPendingProjectionLayer(const
XrCompositionLayerProjection* layer)` — not part of `FrameDriver`,
called by the demo (after `OpenXRProjectionLayer::Prepare()` succeeds)
right before `EndFrame()`. `EndFrame()` builds `XrFrameEndInfo::layers`
from whatever was pending (zero or one layer), submits it, then
**resets the pending pointer to `nullptr`** regardless of whether it
was used — the actual fix for a real bug caught during an earlier
review pass of this same design (an earlier draft required a separate,
easy-to-forget clear call elsewhere; the "always reset after
consuming" rule means a tick that never calls
`SetPendingProjectionLayer()` at all — `shouldRender` was false, or no
valid views were located — safely defaults to zero layers with no
other bookkeeping required).

### Swapchain/View-Count Validation (M9F)

**A genuine runtime check, not a debug-only assertion** — per the
milestone's explicit requirement that a Release build compiling out an
`AR_ASSERT_MSG` must not silently remove the check entirely.
`OpenXRProjectionLayer::Prepare(views)` compares `views.size()`
(this frame's real `xrLocateViews` result) against `m_subImages.size()`
(fixed at construction, one per swapchain): on a mismatch, logs a
clear `AR_LOG_ERROR` and returns `false` (submitting zero layers this
frame) rather than indexing either array out of bounds. `views.empty()`
(shouldRender was false, or view state was invalid) is handled
identically but is not itself an error — no log, just "no layer this
frame." Both paths tested directly with synthetic data
(`TestOpenXRProjectionLayerCountMismatchProducesNoLayer`,
`TestOpenXRProjectionLayerEmptyViewsProducesNoLayer`).

### Composition Submission (M9F)

When a valid layer was prepared: one `XrCompositionLayerProjection`
(`space = LOCAL`, `layerFlags = 0`) containing one
`XrCompositionLayerProjectionView` per located view (`pose`/`fov`
straight from the real `XrView`, `subImage` from the pre-built,
real-swapchain-derived metadata), submitted via the standard
`const XrCompositionLayerBaseHeader* const*` pointer-array +
`reinterpret_cast` idiom `XrFrameEndInfo::layers` requires. Confirmed
accepted by the compositor on the one frame it was attempted (frame 1,
the only frame `shouldRender` was true on this run — see below): no
`CheckXrResult` failure, no crash, exit code 0.

### Static/Moving Pose Findings (M9F)

Observed against SteamVR's null driver, twice, both runs identical:
```
View 0: pos=(0.0315, 0.0000, 0.0000) orient=(w=0.0000, x=0.0000, y=0.0000, z=-1.0000) fov(L/R/U/D deg)=(-45.00, 45.00, 45.00, -45.00)
View 1: pos=(-0.0315, 0.0000, 0.0000) orient=(w=0.0000, x=0.0000, y=0.0000, z=-1.0000) fov(L/R/U/D deg)=(-45.00, 45.00, 45.00, -45.00)
```
**FOV wording, precise**: these ±45° values are symmetric
(`angleLeft = -angleRight`, `angleDown = -angleUp`) on both eyes, every
observed run. AREngine's support for genuinely asymmetric FOV is
real and proven — by the pure mathematical/conversion tests
(`PerspectiveOffCenterRH_ZO`'s frustum-boundary tests,
`tests/core_tests.cpp`; `ConvertXrViewToViewInfo`'s tests,
`tests/openxr_view_tests.cpp`), not by this live run. SteamVR's null
driver simply happened to report a symmetric FOV every time it was
observed; this is a fact about the runtime, not a limitation of
AREngine's conversion pipeline (which carries through whatever FOV
values it is given, symmetric or not, without collapsing anything -
see the tests above). Do not read "asymmetric FOV preserved end to
end" claims elsewhere in this document as claiming the *live* runtime
demonstrated asymmetry - only the tests do; see "M9F Diagnostic Review
Addendum" below for the review that caught and corrected this exact
wording ambiguity.

The ±0.0315 m X-separation between the two views is a plausible,
realistic human IPD (≈63 mm total) — evidence the position conversion
is genuinely correct, not coincidentally-plausible garbage. The
orientation `(w=0, x=0, y=0, z=-1)` is a valid unit quaternion (not a
bug) representing a 180° rotation about Z — not the identity a
"forward-facing, upright" default might be expected to be; this
appears to be the null driver's own fixed default, not a bug in
AREngine's conversion pipeline (independently re-verified twice: the
position conversion's IPD sanity, and the field-reorder logic's own
unit test). **Whether poses change over time could not be observed in
this run**: `shouldRender` was `true` only on frame 1 (the same
SteamVR/null-driver behavior already documented in M9E — see Section
28, "`xrWaitFrame` Timing / `predictedDisplayTime` / `shouldRender`
(M9E)"), and `GetViews()` is deliberately only called when
`shouldRender` is true (per this milestone's own "don't call
`xrLocateViews` unnecessarily" requirement) — so only one real sample
was gathered per run. Reported honestly as a real limitation of this
test environment, not papered over; a runtime/simulator that reports
`shouldRender=true` more often, or genuine head-tracking hardware,
would be needed to observe pose changes across frames. No SteamVR-
specific null-driver pose-injection mechanism was investigated — out
of scope, and the brief explicitly warns against adding vendor-specific
AREngine code for it regardless.

### Validation Results (M9F)

All four `ARENGINE_ENABLE_OPENXR` × `ARENGINE_ENABLE_VULKAN`
combinations: full build succeeds, `/EHsc /W4 /WX` clean (verified on
ON/ON), `ctest` green on every combination (ON/ON: 15/15 — includes
the new `OpenXRViewTests`, 15 pure-logic checks; ON/OFF: 13/13;
OFF/ON default: 11/11).

`arengine_openxr_frame_demo` run twice against the live SteamVR/OpenXR
runtime — both clean (exit code 0), 200/200 frames completed both
times, identical view/pose data both runs (see above). Session-state
sequence: `IDLE → READY → SYNCHRONIZED → STOPPING → IDLE → EXITING`.
Same category of SteamVR-internal Vulkan validation noise as M9E/
M9E.5 (`BlankEyeBuffer`, command-buffer handles this codebase never
allocated) — zero validation errors traced to AREngine's own code.
Sandbox and the desktop Vulkan present demo both launched and ran
without crashing (neither `engine/rendering/` nor `runtime/` code was
touched by this milestone).

- Files changed: `engine/core/include/AREngine/Core/Math/ViewProjection.hpp`;
  `engine/frame/include/AREngine/Frame/ViewInfo.hpp` (docs only); new
  `engine/xr/src/openxr/OpenXRViewConversion.hpp`,
  `OpenXRProjectionLayer.hpp/.cpp`; `engine/xr/src/openxr/
  XRFrameDriver.hpp/.cpp`; `engine/xr/CMakeLists.txt`;
  `tests/openxr_frame_demo.cpp`, new `tests/openxr_view_tests.cpp`,
  `tests/core_tests.cpp`, `tests/CMakeLists.txt`.

### Architectural Issues Found (M9F)

None beyond what design review already caught and corrected before
implementation (the swapchain-metadata-ownership question, resolved by
introducing `OpenXRProjectionLayer`). The one item flagged forward for
M9G: `ViewInfo`'s pose-only shape (no view matrix) has now gone two
milestones without a real consumer exercising it for rendering — M9G
(head-tracked AREngine demo) will be the first genuine test of whether
that remains the right shape, or whether a `viewFromWorld`-producing
helper (on `ViewInfo` itself, or elsewhere) is the smallest correct
addition once a real renderer exists to need one.

### M9F Diagnostic Review Addendum

Requested and performed immediately after M9F's own architectural
approval, before any commit, in response to two specific concerns
raised about the delivered report.

**Issue 1 — PRIMARY_STEREO view order / eye positions.** The original
report showed `View 0 = (+0.0315, 0, 0)`, `View 1 = (-0.0315, 0, 0)`,
which looks reversed against the usual index-0-is-left,
+X-is-right expectation. Investigated directly rather than "fixed" by
swapping or negating anything (which the review explicitly, and
correctly, warned against — that would have hidden real runtime
behavior behind a guess). The demo's diagnostic logging was extended to
print the **raw** `xrLocateViews` output (`XrView[i].pose.position`,
straight from the runtime, before any AREngine code touches it)
alongside the **converted** `Frame::ViewInfo[i].position`, for the same
index `i`, side by side — then rerun twice against live SteamVR:

```
View 0: RAW xrLocateViews pos=(0.0315, 0.0000, 0.0000)
View 0: CONVERTED ViewInfo pos=(0.0315, 0.0000, 0.0000) ...
View 1: RAW xrLocateViews pos=(-0.0315, 0.0000, 0.0000)
View 1: CONVERTED ViewInfo pos=(-0.0315, 0.0000, 0.0000) ...
```

Raw and converted are identical for both indices, both runs — proving
`ConvertXrPosition` genuinely performs only a direct field copy (no
reversal, no sign flip) and that no reordering happens anywhere between
`xrLocateViews`'s output and `ViewInfo`'s consumer-facing values. Every
other requested check was confirmed by direct code inspection, not
merely asserted:
- `GetViews()` builds its result vector with a single `for (const
  XrView& view : views) result.push_back(...)` loop, in order — no
  reversal, and `m_lastLocatedViews = views` is a straight copy of the
  same array, same order.
- The demo's logging loop uses the same index `i` for `views[i]`
  (converted) and `rawViews[i]` (raw) - no mislabeling.
- `swapchains[i]` is built from `viewConfigViews[i]` (M9D's own
  index-preserving `EnumerateViewConfigurationViews`), and
  `projectionSubImages[i]` is built by iterating `swapchains` in the
  same order - confirmed both by code inspection and by new explicit
  log output (`projectionSubImages[0] <- swapchains[0] (1852x2056)`,
  `projectionSubImages[1] <- swapchains[1] (1852x2056)`) added during
  this review. `xrEnumerateViewConfigurationViews` and `xrLocateViews`
  are both indexed consistently by the runtime for a given
  `XrViewConfigurationType`, per spec - there is no seam anywhere in
  this pipeline where index `i` could silently become index `j`.

**Conclusion: the raw `xrLocateViews` output itself, before any
AREngine code runs, already reports index 0 at +X and index 1 at -X.**
This is SteamVR null-driver runtime behavior, observed directly, not an
AREngine bug - reported as exactly that, per the review's own
instruction, with **no runtime-specific workaround introduced**.
AREngine's view ordering and axis convention are preserved exactly as
the runtime reports them; swapping or negating anything here would have
been AREngine silently disagreeing with its own graphics runtime about
whose eye is whose, which is a strictly worse outcome than surfacing
the raw fact. **No code change was required to fix a bug**, because
there was no bug to fix - the two logging additions above (raw-vs-
converted position, sub-image index correspondence) exist purely to
make this conclusion independently verifiable from the log output
itself, not just from this document's own claims.

**Issue 2 — FOV wording precision.** Corrected in this document (see
"Static/Moving Pose Findings (M9F)" above) and in `AGENTS.md`: AREngine
supports and preserves genuinely asymmetric OpenXR FOV, proven by pure
mathematical/conversion tests, not by the live runtime, which reported
a symmetric ±45° FOV in every observed run. Prior wording in
`AGENTS.md` that listed "asymmetric FOV preserved end to end" as
something "verified... against live SteamVR" conflated the two and has
been corrected.

**Regression validation after this review**: `/EHsc /W4 /WX` clean,
`ctest` 15/15 (ON/ON - the only combination affected, since the change
is confined to `tests/openxr_frame_demo.cpp`'s logging inside the
already-Vulkan-gated code path; no header, library, or test-suite
source changed). `arengine_openxr_frame_demo` rerun twice against live
SteamVR: raw/converted positions identical and stable across both runs
(see above), composition layer still accepted (no `CheckXrResult`
failure, no crash), 200/200 frames both runs, exit code 0 both times.

## 31. M9G Implementation Notes

M9G renders AREngine's first real 3D geometry — a cube, reusing M8H's
exact mesh/pipeline infrastructure unchanged — into each eye's real
OpenXR-owned Vulkan swapchain image, using the real per-view pose
`xrLocateViews` produces (inverted into a proper view matrix) and the
real asymmetric per-view projection M9F already produces, submitted
through the existing `XrCompositionLayerProjection` path. This is the
low-level XR render path proven end to end, deliberately before any
`SceneRenderer`/`Scene` integration exists.

### ViewInfo Sufficiency, Reconfirmed (M9G)

M9F already concluded `Frame::ViewInfo` needed zero structural changes
and explicitly deferred the "does a real renderer need more?" question
to whenever one first existed. M9G is that renderer, and the answer is
still no: `position`/`orientation` (a pose) is exactly what
`ViewMatrixFromPoseRH` (below) needs, and `projection` is exactly what
`ApplyVulkanYFlip` needs. Consistent with `Scene::Camera`'s own
"derive on demand, don't duplicate stored data" precedent, the new
capability is a free function (`Core::Math::ViewMatrixFromPoseRH`), not
a field — `ViewInfo` itself is untouched by this milestone.

### View-Pose Semantics and the Rigid-Transform-Inverse View Matrix (M9G)

`ViewInfo.position`/`.orientation` represent `worldFromView =
Translation(position) * Rotation(orientation)` — the pose that places
the eye in the world, **not** the matrix rendering needs. Rendering
needs `viewFromWorld`, its inverse. Because this is a rigid transform
(pure rotation + translation, `orientation` guaranteed unit-length),
the inverse has a closed form and does **not** require a general 4×4
matrix inverse:

```cpp
// Quaternion.hpp
[[nodiscard]] constexpr Quaternion Conjugate(const Quaternion& q) {
    return Quaternion(q.w, -q.x, -q.y, -q.z);
}

// ViewProjection.hpp
[[nodiscard]] inline Mat4 ViewMatrixFromPoseRH(const Vec3& position, const Quaternion& orientation) {
    return Mat4::Rotation(Conjugate(orientation)) * Mat4::Translation(-position);
}
```

`Conjugate(q) = q^-1` for any unit quaternion — a standard identity,
used here (rather than a general matrix inverse) precisely because
`orientation` is always unit-length by construction. Verified three
independent ways before implementation: (1) term-by-term, confirming
`Rotation(Conjugate(q))` is exactly `Rotation(q)`'s transpose, which
equals its inverse for a pure rotation; (2) a worked numeric example —
eye at world `(1,0,0)`, identity orientation; world point `(1,0,-2)`
transforms to view-space `(0,0,-2)` exactly, matching intuition (the
eye's own local -Z axis, at the expected distance) — now a permanent
regression test (`TestViewMatrixFromPoseRH`); (3) propagated through
M9F's actual observed null-driver pose (`orientation =
(w=0,x=0,y=0,z=-1)`, a 180°-about-Z rotation) to confirm a world-space
cube at `(0,0,-2)` genuinely lands at negative view-space Z (visible,
per `PerspectiveOffCenterRH_ZO`'s `w=-z` convention) under this
runtime's specific default pose — the cube's placement (below) is a
verified choice, not a guess.

Naming is deliberately backend-neutral (`ViewMatrixFromPoseRH`, not
`OpenXRViewMatrix`) and lives in `Core::Math` — `Core` must not know
OpenXR exists, and this is exactly the same rigid-pose-to-matrix
operation `Scene::Camera::GetViewMatrix` already performs for desktop,
just extracted so both call sites share it in spirit (not literally
merged — `Camera` composes from a `Scene::Transform`, `ViewInfo` from a
raw pose — but the underlying math and its correctness argument are
identical).

### The ApplyVulkanYFlip Row-1 Bug (found and fixed, M9G)

Before this milestone, `ApplyVulkanYFlip` (`VulkanClipSpace.cpp`, M8F)
negated exactly one matrix element:

```cpp
projection.Set(1, 1, -projection.At(1, 1));   // old
```

This was only ever correct because every projection built before M9G
was symmetric — `PerspectiveRH_ZO`'s off-center skew term (`m12`) is
always exactly 0 for a symmetric frustum. M9F's
`PerspectiveOffCenterRH_ZO` produces a genuinely nonzero `m12` whenever
`angleUp ≠ -angleDown` (the general case for a real HMD's per-eye FOV).
Since `clip.y = m10*x + m11*y + m12*z + m13*w` for any matrix with a
nonzero row-1, negating only `m11` produces `clip.y = -m11*y + m12*z`
instead of the required `-(m10*x + m11*y + m12*z + m13*w)` — a real,
previously-latent bug, caught by design review (independently
re-deriving the math from `Mat4::operator*(Mat4,Vec4)` and
`PerspectiveOffCenterRH_ZO`'s actual source) before it ever ran against
real asymmetric data. Fixed by negating the entire row:

```cpp
projection.Set(1, 0, -projection.At(1, 0));
projection.Set(1, 1, -projection.At(1, 1));
projection.Set(1, 2, -projection.At(1, 2));
projection.Set(1, 3, -projection.At(1, 3));
```

Backward-compatible **by construction**: every desktop call site builds
a symmetric projection, where `m10`/`m12`/`m13` are already 0 — negating
0 is 0, so the output is bit-for-bit identical to the old single-element
version for every existing caller. `tests/vulkan_tests.cpp` gained a
regression test proving exactly that
(`TestApplyVulkanYFlipSymmetricInputUnchanged`), plus two tests that
would have caught the bug directly: `TestApplyVulkanYFlipNegatesOffCenterSkewToo`
(a synthetic all-nonzero row-1 input, confirming all four terms get
negated) and `TestApplyVulkanYFlipComposesWithAsymmetricProjection`
(an end-to-end check against a real `PerspectiveOffCenterRH_ZO` matrix,
confirming `flipped.y == -original.y` exactly).

**Culling/front-face needs no change.** `VK_CULL_MODE_BACK_BIT`/
`VK_FRONT_FACE_CLOCKWISE` (M8H, baked into `VulkanGraphicsPipeline`) was
re-examined, not just assumed safe: the off-center shear terms
(`m02`/`m12`) are additive per-vertex-Z offsets, not functions of the
local `(x,y)` Jacobian that determines 2D screen-space winding — they
cannot flip winding. Only the row-1 negation affects winding, and that
operation is unchanged in *kind* (now just correctly complete) from
what M8F already established as compatible with `VK_FRONT_FACE_CLOCKWISE`.
This reasoning depends on the row-1 fix above being applied — the old
half-flip's effect on winding was not cleanly characterizable and was
not relied upon.

### The VulkanRenderPass colorFinalLayout Bug (found and fixed during live validation, M9G)

Not part of the original design — found only once the cube demo
actually ran against live SteamVR. `VulkanRenderPass` (M8C) hard-coded
its color attachment's `finalLayout` to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`,
correct for every desktop caller (`tests/vulkan_present_demo.cpp`),
whose swapchain images are eventually handed to `vkQueuePresentKHR`.
OpenXR-owned swapchain images are **never** presented that way —
`VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` requires `VK_KHR_swapchain`-semantics
this compositor path doesn't have, and the validation layer rejected it
outright (`vkCreateRenderPass(): pCreateInfo->pAttachments[0].finalLayout
... requires the extension VK_KHR_swapchain`), with the resulting layout
mismatch then failing every subsequent `vkQueueSubmit` for that image.

Generalized rather than duplicated (mirroring the same reuse-first
philosophy already applied to `OpenXRSwapchain`/`VulkanFramebuffers`):
`VulkanRenderPass` gained a fourth constructor parameter,
`VkImageLayout colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` —
the default preserves every existing desktop call site exactly (both
`vulkan_present_demo.cpp` and every other caller omit the new
parameter, unchanged). `openxr_cube_demo.cpp` passes
`VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` explicitly — correct because
nothing downstream of AREngine's own render pass needs a specific final
layout for an OpenXR-owned image; the compositor consumes it directly.
After this fix, the render pass creates cleanly and every subsequent
frame's `vkQueueSubmit` and `xrReleaseSwapchainImage` succeed with no
AREngine-attributable validation error.

### Depth Strategy and Framebuffer Granularity (M9G)

OpenXR provides no depth image (`XR_KHR_composition_layer_depth` is not
enabled or needed here) — AREngine owns one depth `VulkanImage` per eye,
sized to that eye's own swapchain extent (found supported format via
the existing `FindSupportedDepthFormat`, M8F, reused unchanged).

Framebuffers are **two independent `VulkanFramebuffers` instances**, not
one shared instance — `VulkanFramebuffers` is built around one shared
extent/depth-view per instance (matching one desktop swapchain, whose
images are always uniform size); the two eyes are independent
`OpenXRSwapchain`s that could in principle report different extents
(never assumed equal here, even though both happened to be 1852×2056 on
the test runtime). `VulkanRenderPass`/`VulkanGraphicsPipeline`/
`VulkanDescriptorSetLayout`, by contrast, depend only on *format*, not
extent, and are safely shared across both eyes — one instance of each,
reused for both framebuffers.

### OpenXRSwapchain Gains AREngine-Owned Image Views (M9G)

`OpenXRSwapchain`'s constructor gained a required `VkDevice device`
parameter; after enumerating the runtime's own `VkImage`s (unchanged),
it now creates one `VkImageView` per image — `VK_IMAGE_VIEW_TYPE_2D`,
this swapchain's own format, `VK_IMAGE_ASPECT_COLOR_BIT`, 1 mip, 1
layer, identity swizzle — mirroring `VulkanSwapchain.cpp`'s own
image-view creation exactly. New `GetImageViews() ->
const std::vector<VkImageView>&`. The destructor destroys these views
(AREngine-owned) **before** `xrDestroySwapchain` (which owns the
`VkImage`s/`XrSwapchain` itself) — the same "OpenXR owns `VkImage`,
AREngine may own `VkImageView`" split already documented for M9E's
swapchain, now actually exercised.

### Vulkan Command Recording and Synchronization (M9G)

One command buffer, one fence, fully synchronous — the same
"correctness first," deliberately temporary strategy M9E already
established and documented, reused rather than redesigned. Per
rendering frame: acquire + wait both swapchain images first; bind
pipeline/mesh/descriptor-set **once**, before the first eye's render
pass (confirmed via Vulkan-spec review that bound state survives
`vkCmdEndRenderPass` → `vkCmdBeginRenderPass` within one command
buffer, so no per-eye rebinding is needed); then per eye: begin render
pass, set dynamic viewport/scissor from that eye's own extent, push
`MvpPushConstants{ApplyVulkanYFlip(views[i].projection) *
ViewMatrixFromPoseRH(views[i].position, views[i].orientation) * model,
tint}`, draw, end render pass. One submit, one fence wait, then release
both images — `xrReleaseSwapchainImage` never runs while GPU work
targeting that image could still be in flight.

At shutdown, a `vkDeviceWaitIdle` was added before tearing down any
Vulkan resource (matching `vulkan_present_demo.cpp`'s own shutdown
discipline) — AREngine's own submitted work is already known complete
by the per-frame fence wait, but the shared `VkDevice` (handed to the
runtime via `XR_KHR_vulkan_enable2`) may still have SteamVR's own
in-flight compositor work on it. See "SteamVR-Internal Vulkan
Validation Noise, Continued (M9G)" below for what this did and didn't
change.

### One Model, Multiple Views (M9G)

One `Scene::Transform cubeTransform` (position `(0,0,-2)`, scale `0.5`,
identity rotation — LOCAL space, stationary), one `model =
cubeTransform.ToMatrix()` computed once per frame — never once per eye,
never one cube instance per eye. Each eye's MVP is computed as
`projection_i * view_i * model` (never the reverse order — a dedicated
test, `TestMvpMultiplicationOrderMatters`, confirms `model * view *
proj` gives a different, wrong result for the same inputs), and a
second test (`TestTwoEyePosesProduceDifferentMvps`) confirms the same
shared `model` genuinely produces two different MVPs when combined with
two different eye poses — proving the "one world object, N views"
pipeline is wired correctly, not just assumed. View order is preserved
exactly as the runtime provides it (`view[0]` → `swapchains[0]`,
`view[1]` → `swapchains[1]`, matching the same discipline M9F's
diagnostic review already established for this codebase) — no
sign-based or identity-based reordering anywhere.

### Render-Gate Reuse, Not a New Check (M9G)

Rather than adding an independent view-count-vs-swapchain-count check,
the demo reuses `OpenXRProjectionLayer::Prepare(const
std::vector<XrView>&) -> bool` (M9F) directly as the render gate:
`if (projectionLayer.Prepare(frameDriver.GetLastLocatedXrViews()) &&
views.size() == swapchains.size())`. `Prepare` already logs a clear
error and returns `false` safely on a count mismatch without indexing
blindly; `views` and the raw located `XrView`s are always the same size
by construction (both derived from the same `xrLocateViews` call inside
`XRFrameDriver::GetViews()`), so no redundant second check was added.
When `shouldRender` is false, or `Prepare` fails: no acquire, no GPU
submission, zero composition layers, `EndFrame()` still runs — unchanged
from M9E/M9F.

### Vulkan Reuse Report (M9G)

Reused **completely unchanged**: `VulkanMesh`/`CreateVulkanMesh`,
`VulkanImage` (for both the depth images and the existing M8E
checkerboard texture), `VulkanDescriptorSetLayout`/`VulkanDescriptorPool`/
`VulkanSampler`, `VulkanCommandPool`, `MvpPushConstants`,
`GenerateCheckerboardRGBA8`/`CreateTextureFromPixels`,
`FindSupportedDepthFormat`, `VulkanGraphicsPipeline`.

Reused with a **small, backward-compatible generalization**:
`VulkanRenderPass` (new `colorFinalLayout` parameter, default-preserved
— see above), `OpenXRSwapchain` (new `VkDevice` parameter + cached
image views — see above). `VulkanFramebuffers` itself needed **no**
change — it was already generic (`device, renderPass,
vector<VkImageView>, depthView, extent`), just instantiated twice
instead of once.

**New, not reused**: `Core::Math::Conjugate`/`ViewMatrixFromPoseRH` —
Core math, not XR-specific, following the same "no OpenXR types leak
into Core" rule already established.

### New Demo: openxr_cube_demo.cpp, Not Extending openxr_frame_demo.cpp (M9G)

A new, self-contained `tests/openxr_cube_demo.cpp` (not part of
`ctest`, same manual-validation convention as every other OpenXR demo)
rather than extending `openxr_frame_demo.cpp`: the latter is already a
complete, coherent frame-lifecycle/view-location diagnostic at roughly
650 lines; adding a full render pass/pipeline/depth/descriptor-set/mesh
setup on top would have doubled it and mixed two concerns that this
codebase otherwise keeps as separate, self-contained leaf demos (the
same pattern `vulkan_present_demo.cpp` and `openxr_frame_demo.cpp`
already establish independently of each other). `openxr_cube_demo.cpp`
reaches directly into both `engine/xr/src/openxr/*` and
`engine/rendering/src/vulkan/*` private headers — the first demo to
cross both boundaries in one file, explicitly permitted by this
milestone's brief ("the M9G demo may coordinate these systems
directly"). Neither `engine/xr` nor `engine/rendering` gained a new
*library-level* dependency on each other — only this leaf demo target
links both.

### SteamVR-Internal Vulkan Validation Noise, Continued (M9G)

The same category of noise M9E/M9E.5/M9F already traced conclusively to
SteamVR's own compositor internals (not AREngine's code) reappears here,
now with more objects — expected, since M9G is the first milestone to
submit real rendering work on the shared `VkDevice`, giving SteamVR's
compositor correspondingly more of its own internal work to do:
`vkCreateImage`/`PREINITIALIZED` warnings during session start, and at
shutdown, a `vkDestroyDevice` "N leaked objects" validation error
listing several `VkCommandBuffer`/`VkFence`/`VkDeviceMemory` handles
(one explicitly tagged `[BlankEyeBuffer]`). Traced the same way M9E
established: this demo allocates exactly one command buffer (freed via
`VulkanCommandPool`'s destructor) and one fence (`vkDestroyFence`,
explicit, before `vkDeviceWaitIdle`'s caller returns) — the leaked list
names three *different* command-buffer addresses and four different
fences across a single run, structurally ruling out AREngine's own
single, reused handles. Adding `vkDeviceWaitIdle` before any teardown
(new in M9G, matching `vulkan_present_demo.cpp`'s own shutdown
discipline) is still correct hygiene and was kept, but — consistent
with this being SteamVR's own internal bookkeeping rather than a
synchronization gap in AREngine's own code — it did not make the
message disappear, which is itself further evidence for, not against,
the M9E-established conclusion. **AREngine's own Vulkan usage in M9G
produced zero validation errors or warnings traced to its own handles.**

### Spatial Stability and Visual Verification Limits (M9G)

`shouldRender` was `true` only on frame 1 of this 200-frame run — the
same SteamVR/null-driver behavior M9E and M9F already documented, not a
regression introduced here. Consequently only frame 1 actually
rendered (`renderedFrameCount == 1`, 2 draw calls), and no
frame-to-frame pose comparison is possible on this runtime — the same
honest limitation M9F already reported, reconfirmed rather than
re-litigated. The cube's position and both eyes' real, distinct
per-view poses (`(0.0315,0,0)` / `(-0.0315,0,0)`, a plausible ≈63mm IPD
split) were logged on the one frame that did render, confirming real
per-view data reached the render call. SteamVR's null driver has no
window or visual output in this environment, so **this milestone
reports code-path correctness (the right matrices, in the right order,
from the right data, submitted through the right composition layer) —
it does not and cannot claim a human visually confirmed a stereo cube**,
consistent with the brief's own instruction not to claim visual
verification that didn't happen. Optional cube rotation (to make
motion visible independent of head tracking) was deliberately not
implemented, per the brief's stated preference for a stationary cube
first and because this runtime offers no way to visually confirm
rotation either way.

### Validation (M9G)

All four `ARENGINE_ENABLE_OPENXR` × `ARENGINE_ENABLE_VULKAN`
combinations: full build succeeds, `/EHsc /W4 /WX` clean (verified on
ON/ON), `ctest` green on every combination (ON/ON: 15/15 — 5 new pure-
logic tests over M9F's count: `TestQuaternionConjugate`,
`TestViewMatrixFromPoseRH`, `TestViewMatrixFromPoseRHRotated`,
`TestMvpMultiplicationOrderMatters`, `TestTwoEyePosesProduceDifferentMvps`,
plus 3 revised/new `ApplyVulkanYFlip` tests in `vulkan_tests.cpp`;
ON/OFF: 13/13; OFF/ON default: 11/11; the `/W4 /WX` combination also
15/15).

`arengine_openxr_cube_demo` run against live SteamVR: 200/200 frames
completed, clean session-state sequence (`IDLE → READY →
SYNCHRONIZED → STOPPING → IDLE → EXITING`, identical shape to M9E/M9F),
exit code 0, no crash. Real per-view poses and the real asymmetric
projection path confirmed in use (by direct code inspection —
`views[i].projection` flows straight from `Frame::ViewInfo`, never
`Scene::Camera` or a symmetric fallback), `view[0]`/`swapchains[0]`
correspondence preserved with no reordering, a real
`XrCompositionLayerProjection` referencing the actually-rendered
swapchains submitted via `SetPendingProjectionLayer`. Sandbox and
`vulkan_present_demo` both launched and ran without crashing (neither
was touched by this milestone; `ApplyVulkanYFlip`'s fix is behavior-
preserving for every desktop call site, confirmed by the new regression
test, not just assumed).

### Architectural Issues Found (M9G)

Two real, previously-latent bugs were found and fixed, both before or
during this milestone's own validation (not carried over from an
earlier milestone): the `ApplyVulkanYFlip` row-1 truncation (found by
design review, before ever running against real hardware) and the
`VulkanRenderPass` hard-coded `colorFinalLayout` (found only once the
cube demo actually ran against live SteamVR — the first real GPU
submission this codebase has made into an OpenXR-owned image). Both are
documented above with their fixes. No other architectural issues were
found — `Frame::ViewInfo`, `OpenXRProjectionLayer`, and
`VulkanFramebuffers` all needed zero or trivial changes to serve as a
real rendering consumer, consistent with the "prove the seam with the
minimal thing first" philosophy this project has followed since M0.

### Not Implemented (M9G, by design)

No `SceneRenderer`/`Scene` integration, no gameplay/networking/physics/
audio systems, no controller input or OpenXR actions, no hand/eye
tracking, no passthrough/anchors, no PBR/lighting/shadows/post-
processing, no custom AR glasses integration, no arena calibration —
all explicitly out of scope for M9G, deferred to M10 or later per
`docs/ROADMAP.md`.

- Files changed: `engine/rendering/src/vulkan/VulkanClipSpace.cpp/.hpp`
  (row-1 flip fix), `VulkanRenderPass.hpp/.cpp` (`colorFinalLayout`
  parameter); `engine/core/include/AREngine/Core/Math/Quaternion.hpp`
  (`Conjugate`), `ViewProjection.hpp` (`ViewMatrixFromPoseRH`);
  `engine/xr/src/openxr/OpenXRSwapchain.hpp/.cpp` (`VkDevice` + cached
  image views); new `tests/openxr_cube_demo.cpp`; `tests/core_tests.cpp`
  (5 new tests), `tests/vulkan_tests.cpp` (3 new/revised tests),
  `tests/CMakeLists.txt` (`arengine_openxr_cube_demo` target);
  `tests/openxr_frame_demo.cpp` (one line, updated `OpenXRSwapchain`
  constructor call to match the new signature).

## 32. M9H Implementation Notes

M9H is a cleanup/hardening pass over M9G's XR render path — no new
rendering capability, no Scene integration, no gameplay. Goal: extract
the genuinely reusable pieces of M9G's demo into small, narrow,
well-documented components; make per-view resource ownership and
command/fence reuse explicit and correct across many frames, not just
the one frame M9G's own run happened to render; validate the generic
frame loop over a much longer run; and investigate (without adding any
vendor-specific code) whether continuous `shouldRender=true` frames are
achievable in this environment.

### M9G Demo Audit (M9H)

Performed first, before any refactoring, per the milestone's own
requirement. Every piece of `tests/openxr_cube_demo.cpp` as it stood
after M9G, categorized:

**A. Reusable engine infrastructure** (extracted): the per-view depth-
image + framebuffer-set construction loop (one real ownership unit,
duplicated by a `for` loop across eyes, with real lifetime rules worth
naming) and the per-view render-pass-begin/viewport/scissor/push-
constants/draw/end sequence (a single, reusable "record one view's
draw commands" operation). Both extracted into new
`tests/OpenXRVulkanViewTarget.hpp/.cpp` — see "Reusable Helper
Extraction (M9H)" below for why this landed at the `tests/` leaf level
rather than inside `engine/xr` or `engine/rendering`.

**B. Demo-only orchestration** (kept in the demo, unchanged in kind):
the entire OpenXR/Vulkan bring-up sequence (instance, system, view
config, blend mode, graphics binding, session, LOCAL space, swapchain-
format selection) — identical in substance to `openxr_frame_demo.cpp`
by established, deliberate precedent (no shared demo-setup helper
exists between any two demos in this codebase); the cube's own
`Scene::Transform` definition and the checkerboard texture setup (this
demo's entire "scene," never promoted to a real asset/Scene concept);
main-loop control (target frame count, exit-request timing, safety
timeout, logging cadence); the small file-local helpers
(`CheckVkResultHere`, `SwapchainFormatToString`, `kEyeClearColors`) —
each duplicated per file by this codebase's own established
convention, not accidentally.

**C. Accidental duplication of existing Vulkan helpers**: none found.
The per-view depth/framebuffer loop (category A above) is new resource-
ownership logic, not a duplicate of any existing class — `VulkanImage`
and `VulkanFramebuffers` are correctly reused *inside* it, unchanged.
`CheckVkResultHere`'s per-file duplication is deliberate, established
convention (see M9E/M9F/M9G), not an oversight.

**D. Temporary one-frame assumptions** — the audit's most important
finding: the entire real-rendering code path (command-buffer reset/
re-record, fence reset/wait/release, the per-view draw sequence) had,
as of M9G, been exercised by **exactly one frame** in every manual run
performed so far (`shouldRender` was `true` only on frame 1 in every
M9E/M9F/M9G run). The code was *written* to be repeat-frame-safe (no
per-frame allocation anywhere in the loop; the same command buffer and
fence reused throughout) but this had never actually been *proven*
against repeated real GPU work — a real, if unavoidable (see
"Continuous shouldRender Investigation" below), gap between "designed
to be correct across many frames" and "observed to be correct across
many frames." M9H's 1000-iteration validation run (below) closes the
"many frames of the generic loop" half of this gap; the "many frames of
real GPU render work" half remains unclosed, honestly, for the reason
documented below. A second, smaller finding: the acquire/release loop
had no explicit note that nothing is ever acquired unless the frame's
full render-and-release contract can be completed (see "Error Recovery
(M9H)" below) — now documented explicitly, not just true by accident of
control flow.

### Reusable Helper Extraction (M9H)

New `tests/OpenXRVulkanViewTarget.hpp/.cpp`: a class,
`OpenXRVulkanViewTarget` (owns one view's depth image + framebuffer
set, borrows its `OpenXRSwapchain`), and a free function,
`RecordOpenXRViewRenderPass` (records one view's render pass into an
already-open command buffer, given an already-built MVP matrix — no
`Frame::ViewInfo` dependency, kept Vulkan-only). Deliberately **not**
the milestone brief's own example shape
(`OpenXRVulkanViewRenderer::RenderView(OpenXRSwapchain&, viewIndex,
Frame::ViewInfo, Mat4)`) — that shape bundles view-matrix derivation
into the renderer; M9H keeps that math in the demo (already a tested,
working call site: `Math::ViewMatrixFromPoseRH` /
`ApplyVulkanYFlip`), and passes the *already-computed* MVP in, which is
what let `RecordOpenXRViewRenderPass` avoid depending on `engine/frame`
at all.

**Placement — deliberately NOT `engine/xr` or `engine/rendering`.**
This is the audit's second major architectural finding. A view-target
class needs `OpenXRSwapchain` (XR-private) and `VulkanImage`/
`VulkanFramebuffers` (Rendering-private) simultaneously. Checked against
actual `CMakeLists.txt` link lines: `arengine_xr` links raw
`Vulkan::Vulkan` but **not** `AREngine::Rendering`; `arengine_rendering`
does not link `AREngine::XR` at all. Promoting this class into either
module would require one of them to gain a brand-new dependency on the
other — exactly the boundary M9C, M9F ("`engine/xr` does not include
Rendering's private headers... same established boundary as M9C's
`FindGraphicsQueueFamily` duplication"), and M9G ("`engine/xr` itself
gains NO new dependency on `engine/rendering`") have each independently,
repeatedly chosen not to cross. M9H did not silently violate that
boundary to get a cleaner-looking class location: the helper stays at
the same leaf level (`tests/`) the boundary-crossing already
legitimately happens at, exactly where `openxr_cube_demo.cpp` itself
already lives. This is a real architectural constraint, not a
stylistic preference — reported here explicitly rather than only
implied by file placement.

### Per-View Resource Model (M9H)

Unchanged from M9G, now named and owned by `OpenXRVulkanViewTarget`
instead of two parallel loop-built vectors: per view — one
`OpenXRSwapchain` (borrowed), N OpenXR-owned color `VkImage`s, N
AREngine-owned color `VkImageView`s (cached on the swapchain itself,
M9G), one AREngine-owned depth `VulkanImage`, one AREngine-owned
`VulkanFramebuffers` set (N framebuffers, one per swapchain image).

**Depth model: option A (one depth image reused sequentially for all
of a view's swapchain images), confirmed, not changed.** Re-examined
against the actual evidence, not just carried forward: `VulkanFramebuffers`
itself only accepts a single `depthImageView` parameter for its whole
image-view set (see `VulkanFramebuffers.hpp`) — option B (one depth
image per swapchain image) isn't even representable without changing
that class. Correct under the current fully-synchronous,
one-frame-in-flight synchronization model: only one framebuffer is ever
being rasterized into at a time (the single command buffer/fence pair
is reset, recorded, submitted, and waited on to completion before the
next acquire), so no two images ever contend for the same depth buffer
concurrently.

### Framebuffer Model (M9H)

No per-frame framebuffer creation — confirmed by construction
(`OpenXRVulkanViewTarget`'s framebuffers are built once, in its
constructor, called exactly `swapchains.size()` times total, before the
frame loop begins) and by the 1000-frame validation run's own log
(`"Created 2 per-view render target(s)..."` logged exactly once, not
once per frame). `RecordOpenXRViewRenderPass` only *selects* an
already-built framebuffer by acquired image index
(`viewTarget.GetFramebuffer(acquiredImageIndex)`) — it never creates
one. Reuses M9G's generalized `VulkanFramebuffers` unchanged.

### Command-Buffer and Fence Reuse Model (M9H)

Confirmed unchanged from M9G and now exercised across 1000 loop
iterations (not just one): one `VkCommandBuffer` and one `VkFence`,
allocated/created exactly once (before the frame loop begins), reused
every rendered frame via `vkResetCommandBuffer` + `vkBeginCommandBuffer`
and `vkResetFences` + `vkWaitForFences` — never reallocated,
re-created, or freed inside the loop. No complex command scheduler was
introduced (none was needed): `vkResetCommandBuffer` on a single
primary command buffer, called only from a single thread, is already
the minimal correct reuse mechanism for this demo's fully-synchronous
model. The 1000-frame run's clean exit (`vkDestroyDevice`'s "leaked
objects" list — see below — still names only handles this demo never
allocated) is direct evidence this reuse is correct, not just written
to look correct.

### Fence/Synchronization Model (M9H)

Kept exactly as M9G documented it — correctness first: reset fence,
record, submit with fence, wait for the fence, only then
`xrReleaseSwapchainImage`. Still explicitly temporary (no multiple
frames in flight), still not optimized, per the brief's own explicit
instruction not to. `vkDeviceWaitIdle` before teardown (added in M9G)
is unchanged.

### Important Release Rule, Reconfirmed (M9H)

`xrReleaseSwapchainImage` is still never called until the render
fence has signaled — `vkWaitForFences` unconditionally precedes the
release loop, both in the code and in every observed run's ordering.
No change was needed; this invariant was already correct in M9G and
the refactor preserves it verbatim (`RecordOpenXRViewRenderPass` never
touches acquire/wait/release at all — that stays the frame loop's own
responsibility, per the milestone's explicit "should not own... swapchain
acquire/release" instruction for any extracted helper).

### Layout Lifecycle (M9H)

Unchanged from M9G, restated for completeness: color images are
acquired in whatever layout the runtime last left them (never assumed
by AREngine); the render pass's `loadOp=CLEAR` means the incoming
layout genuinely doesn't matter (`initialLayout = VK_IMAGE_LAYOUT_UNDEFINED`);
during rendering the subpass uses `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`
(the attachment reference's layout); the render pass's `finalLayout` is
`VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` (M9G's `colorFinalLayout`
fix) — never `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`, which is meaningless
for an OpenXR-owned image and was the real bug M9G found and fixed. The
1000-frame run reproduced zero layout-mismatch errors of the kind M9G
found and fixed (the only remaining `vkCreateImage`/`vkQueueSubmit`
errors are the already-documented `BlankEyeBuffer` category — see
below), confirming the fix holds under repeated use, not just the one
run that discovered it.

### Depth, Reconfirmed Internal-Only (M9H)

No `XR_KHR_composition_layer_depth`, not considered, per the brief.
Depth exists solely so AREngine's own 3D rendering (occlusion between
the cube's own faces) is correct — never submitted to or read by the
compositor.

### Pipeline and Mesh Reuse, Reconfirmed (M9H)

Unchanged from M9G, now explicitly re-verified as part of this
milestone's own audit rather than only inherited: one
`VulkanGraphicsPipeline`, built once, bound once per rendered frame
(not once per view — the bind-once-per-frame, draw-many-views pattern
M9G established), reused across both eyes via different per-view MVP
push constants. One `VulkanMesh` (`Rendering::CreateCubeMesh()`),
uploaded once via `CreateVulkanMesh`, bound once per rendered frame,
drawn once per view. No new class or code path was needed to preserve
either — both were already correct in M9G and untouched by this
milestone's refactor.

### Model State (M9H)

Still one shared `Scene::Transform` for the whole cube — never one
per eye. **New in M9H**: slow, constant-rate rotation
(`kCubeRotationRadiansPerSecond = 0.5` rad/s about world Up), driven by
`FrameTiming::totalTimeSeconds` (the frame driver's own elapsed clock,
recomputed fresh each rendered frame — never a per-frame delta
accumulator, which would drift under variable frame timing), added
specifically so a repeated-frame run has *some* independently
observable evidence of change across frames, since this runtime's real
head poses remain static (M9F/M9G, reconfirmed below). This is
motion added for repeated-frame verification, explicitly not a
stand-in for head tracking — the cube's own rotation and the (still
static) camera pose are never conflated in this document or the demo's
own logging.

### Performance Diagnostics (M9H)

New, lightweight, `std::chrono`-only (`FrameDiagnostics`, a plain
struct of running sums/counts, no profiler framework): frames
attempted, frames with `shouldRender=true`, frames rendered, views
rendered, draw calls, average CPU frame-preparation time (view
location + command recording, measured from just after `BeginFrame()`
plus the render gate to just before `vkQueueSubmit`), average Vulkan
submit-to-fence-signal time. From the validation run (below): 1000
frames attempted, 1 frame with `shouldRender=true`, 1 frame rendered, 2
views rendered, 2 draw calls, average CPU prep time 1.75ms, average
Vulkan submit time 0.56ms — consistent with a single simple textured
cube drawn twice, not a meaningful FPS benchmark (the brief's own
stated goal for this instrumentation is catching lifetime/stale-state
bugs, not measuring performance, and it's treated that way here).

### Continuous shouldRender Investigation (M9H)

Investigated as an **environment configuration question**, per the
brief's explicit instruction that AREngine's own source code must stay
standard OpenXR/Vulkan with zero vendor-specific logic — no code was
added or considered for this.

- **SteamVR null-driver settings inspected directly**: both
  `drivers/null/resources/settings/default.vrsettings` (the driver's
  own shipped defaults: `windowWidth`/`windowHeight`/`renderWidth`/
  `renderHeight`/`displayFrequency` — nothing related to focus or
  continuous rendering) and the active
  `config/steamvr.vrsettings` (`driver_null.enable: true`,
  `steamvr.forcedDriver: "null"`, `steamvr.requireHmd: false`) were
  read. Neither exposes any setting that changes whether the
  compositor keeps issuing `shouldRender=true` to a background
  application.
- **Root-cause hypothesis, evidence-backed**: every session-state trace
  captured across M9E, M9F, M9G, and this milestone's own 1000-frame
  run shows the identical sequence `IDLE → READY → SYNCHRONIZED →
  STOPPING → IDLE → EXITING` — the session **never reaches
  `XR_SESSION_STATE_VISIBLE` or `XR_SESSION_STATE_FOCUSED`** in any
  observed run. Per the OpenXR spec's session-state model, a runtime is
  expected to keep issuing meaningful render opportunities to the
  *focused* scene application; SteamVR's compositor most plausibly
  treats this console-only, no-window OpenXR client as never becoming
  the focused application (there is no dashboard interaction, no real
  display surface, and nothing in this environment that would cause
  SteamVR to hand it focus) — `shouldRender=true` on frame 1 only is
  consistent with "the runtime offers one opportunistic frame, then
  stops asking for more from a background/unfocused client." This is a
  hypothesis about the *runtime's* compositor logic, not a claim about
  AREngine's own code, and is reported as a hypothesis, not a proven
  fact — but it is the same category of "traced to its actual source,
  not just observed" standard M9E/M9F/M9G held every other finding to.
- **No standards-based alternative already installed.** M9B's own
  investigation (Section 25) already evaluated the realistic options on
  this machine and concluded SteamVR's null driver was the best fit;
  the only legitimate secondary option it identified, **Meta XR
  Simulator**, is still not installed and was not installed for this
  milestone either, per the brief's explicit "do not install another
  runtime automatically" instruction. If continuous-render validation
  becomes a hard requirement for a future milestone, Meta XR Simulator
  remains the recommended next thing to evaluate — reported here as the
  option to pursue, not acted on.
- **Conclusion, honestly reported**: no continuous-render option is
  available in this environment without new software. M9H proceeded
  with the current runtime and validated repeated frame-lifecycle
  behavior as thoroughly as it allows (1000 iterations of the generic
  loop; 1 frame of real GPU render work) rather than fabricating or
  simulating continuous rendering evidence that didn't happen.

### Stale-Frame Protections (M9H)

Confirmed by direct code inspection (same verification standard as
M9F/M9G — every claim below was checked against the actual current
source, not assumed carried over):
- `XRFrameDriver::GetViews()` (`XRFrameDriver.cpp`) clears
  `m_lastLocatedViews` unconditionally at the very start of every call,
  before doing anything else, and only repopulates it if
  `IsViewStateValid` returns true; on an invalid-view frame it returns
  early with `m_lastLocatedViews` left empty (never stale data from a
  previous valid frame).
- `XRFrameDriver::EndFrame()` unconditionally resets
  `m_pendingProjectionLayer = nullptr` after every `xrEndFrame` call,
  regardless of whether `SetPendingProjectionLayer` was called that
  tick — a `shouldRender=false` (or invalid-view, or count-mismatch)
  frame that never calls `SetPendingProjectionLayer` submits zero
  layers this tick and cannot possibly submit a previous frame's
  pointer next tick, because that pointer no longer exists by the time
  the next `EndFrame()` runs.
  `TestOpenXRProjectionLayerRepreparingReplacesPreviousLayer`
  (`tests/openxr_view_tests.cpp`, M9F) already covers the equivalent
  guarantee one layer lower, at `OpenXRProjectionLayer::Prepare`/`Get`
  itself.
- The acquired-image-index vector (`acquiredIndices`) is declared fresh
  inside the render branch on every iteration — there is no member
  variable or static state an index could leak through between frames.
- No acquire ever happens unless `Prepare(...)` and the view/swapchain
  count check both succeed first (see "Error Recovery" below) — so an
  invalid or mismatched frame never reaches the acquire loop at all,
  and therefore can never leave an unreleased image behind.

`XRFrameDriver` itself needed **zero** changes for any of this — every
guarantee above was already correct as of M9E.5/M9F/M9G; M9H's
contribution is re-verifying and explicitly documenting it, plus
confirming (by construction) that the newly-extracted
`OpenXRVulkanViewTarget` introduces no new place for staleness to hide
(it holds no per-frame state at all — only the depth image/framebuffers
built once at construction).

### Error Recovery (M9H)

Two genuinely different situations, handled differently, both reported
honestly:

1. **Expected render-skip conditions** (`shouldRender=false`, invalid
   view state, view/swapchain count mismatch) — already handled
   correctly, by construction: the `Prepare(...) && views.size() ==
   swapchains.size()` gate runs *before* any `xrAcquireSwapchainImage`
   call, so on any of these conditions nothing is ever acquired, there
   is nothing to release, zero composition layers are submitted, and
   the loop continues cleanly to the next `PrepareFrame()`. This *is*
   the error-recovery story for every condition the brief lists as
   "expected" (count mismatch, invalid views) — no additional code was
   needed because the existing gate already produces exactly the
   required outcome.
2. **Unexpected mid-frame API failures** (e.g. `vkQueueSubmit` or
   `xrAcquireSwapchainImage` itself returning an unexpected error after
   a prior view's image was already successfully acquired) — genuinely
   **not** recoverable under this codebase's own established
   `CheckVkResultHere`/`CheckXrResult` idiom, which calls
   `AR_ASSERT_MSG` → `std::abort()` immediately at the point of failure
   (confirmed by reading `engine/core/src/Assert.cpp`: no exception is
   thrown, so there is no `catch` block opportunity to run cleanup code
   afterward — the process terminates at the exact line that detected
   the failure). This fail-fast policy is deliberate and used
   identically by every Vulkan/OpenXR demo in this codebase, not a
   gap introduced by M9H. Building a partial-release-then-continue path
   around it would mean replacing that codebase-wide idiom — explicitly
   out of scope ("do not build a full exception/recovery framework").
   Reported here as a known, deliberate limitation rather than silently
   left undocumented: an unexpected failure between a successful
   partial acquire and the matching release will still abort without
   releasing the already-acquired image(s), exactly as every other
   unexpected Vulkan/OpenXR API failure in this codebase already does.

### Runtime-Independent Architecture, Reconfirmed (M9H)

Grepped `engine/xr` and `engine/rendering` for any of `SteamVR`,
`Valve`, `driver_null`, `Meta`, `Oculus`, or a vendor registry path —
zero matches, unchanged from every prior XR milestone. The
`shouldRender` investigation above was conducted entirely by reading
files *outside* the repository (SteamVR's own installed config) and
reasoning about the OpenXR spec's session-state model — nothing it
found was translated into engine code.

### Validation (M9H)

All five practically-distinct `ARENGINE_ENABLE_OPENXR` ×
`ARENGINE_ENABLE_VULKAN` configurations built and `ctest`-passed:
ON/ON (`/EHsc /W4 /WX` clean) 15/15; ON/ON with `/EHsc /W4 /WX`
explicitly forced via `CMAKE_CXX_FLAGS` 15/15 (same count, confirming
the refactor introduces no new warning); ON/OFF 13/13; OFF/ON (the
project's default configuration) 11/11; OFF/OFF 10/10. No existing test
needed to change to keep passing — the M9F/M9G suite already covers
stale-layer reset, count-mismatch handling, and MVP/view-matrix/
Y-flip math (see "Tests" below for the explicit per-checklist-item
mapping).

`arengine_openxr_cube_demo` run against live SteamVR for 1000 completed
frame-lifecycle iterations (up from M9G's 200): clean exit (code 0),
clean session-state sequence identical in shape to every prior run
(`IDLE → READY → SYNCHRONIZED → STOPPING → IDLE → EXITING`), 2 per-view
render targets logged as created exactly once (not per frame), 1 frame
rendered (`shouldRender=true` on frame 1 only, matching the
already-documented runtime limitation), 2 views rendered, 2 draw calls,
diagnostics reported as above. The `vkDestroyDevice` "14 leaked
objects" validation error at shutdown reappeared, structurally
unchanged from M9G (different `VkCommandBuffer`/`VkFence` handle
addresses than this demo's own single reused pair, same
`[BlankEyeBuffer]`-tagged memory) — traced the same way M9E originally
established, confirming this refactor changed nothing about that
already-understood, non-AREngine-attributable category. Sandbox and
`arengine_vulkan_present_demo` both launched and ran without crashing
(neither was touched by this milestone).

**Resource-growth findings**: no per-frame `VkImageView`, framebuffer,
or mesh-upload call exists anywhere in the loop (confirmed by
construction — `OpenXRVulkanViewTarget` construction and
`CreateVulkanMesh`/`CreateTextureFromPixels` all happen before the loop
begins, and the "Created 2 per-view render target(s)" log line, which
would print once per construction, printed exactly once across the
1000-iteration run); the command buffer and fence are the same two
handles for the entire run (allocated/created once, only
reset/re-recorded, never reallocated); `acquiredIndices` is a small,
bounded (`swapchains.size()`-length) vector, freshly constructed and
immediately discarded each render, not an accumulating container.
Since only one frame in this run actually exercised the real render
path, this is a structural (by-construction) resource-growth argument,
not an observed-over-many-real-renders one — honestly distinguished
from a claim that repeated *real* rendering was proven leak-free, which
would require the continuous-render environment this milestone
established isn't available here.

### Tests (M9H)

Per the checklist's own 9 categories, the existing M9F/M9G suite was
checked item by item rather than assumed sufficient:

1. Stale projection-layer reset —
   `TestOpenXRProjectionLayerRepreparingReplacesPreviousLayer` (M9F).
2. Render-target bookkeeping — not unit-testable without a real
   `VkDevice`/`OpenXRSwapchain` (same category as `OpenXRSwapchain`'s
   own image-view creation, M9G: "a real Vulkan API call... not
   unit-tested, only exercised by the manual demo"); validated by the
   live 1000-frame run instead (constant render-target count, above).
3. Acquired/released state transitions — same category as (2), same
   reasoning.
4. Repeated frame state — validated by the live 1000-frame run (no
   crash, no hang, no session-lifecycle error across 1000 iterations),
   not separable into a pure-logic unit.
5. Count-mismatch behavior —
   `TestOpenXRProjectionLayerCountMismatchProducesNoLayer` (M9F).
6. No previous-frame data reuse — confirmed by code inspection (see
   "Stale-Frame Protections" above) plus (1)/(5)'s existing coverage.
7. Existing Vulkan Y-flip regression —
   `TestApplyVulkanYFlipSymmetricInputUnchanged`/
   `TestApplyVulkanYFlipNegatesOffCenterSkewToo`/
   `TestApplyVulkanYFlipComposesWithAsymmetricProjection` (M9G) — all
   still pass unchanged, confirming M9H's refactor didn't disturb them.
8. Render-pass final-layout behavior — `VulkanRenderPass` only exposes
   an opaque `VkRenderPass` handle (no accessor for its internal
   `VkAttachmentDescription`s), so the `colorFinalLayout` value itself
   isn't pure-logic-testable without either changing that class's
   public surface (not justified by this milestone) or GPU-side
   validation-layer capture; validated live instead (the 1000-frame
   run reproduced zero layout-mismatch errors of the kind M9G's fix
   addressed).
9. View/model/MVP math — `TestViewMatrixFromPoseRH`/
   `TestViewMatrixFromPoseRHRotated`/`TestMvpMultiplicationOrderMatters`/
   `TestTwoEyePosesProduceDifferentMvps` (M9G), all still passing.

No new `ctest` entries were added: every checklist category maps to
either existing, still-passing test coverage or a GPU-coupled
concern that this codebase's own established convention (M9G, and
`OpenXRSwapchain` before it) already treats as manually-validated
rather than unit-tested. Adding a test that only re-asserts what an
existing test already proves, or that mocks out real GPU/OpenXR state
to force a unit-testable shape, was judged not to be "useful" in the
brief's own sense — consistent with "do not overbuild."

### Architectural Issues Found (M9H)

One real, load-bearing finding: a genuinely reusable per-view render-
resource class cannot be promoted into either `engine/xr` or
`engine/rendering` under the current module layering without one of
them gaining a new dependency on the other — see "Reusable Helper
Extraction (M9H)" above. This is not a defect to fix; it is a
constraint this milestone respected rather than worked around, and is
reported explicitly so a future milestone (most plausibly M10, once
`SceneRenderer` needs to drive XR rendering for real) can make an
informed, deliberate decision about whether to finally cross that
boundary, rather than rediscovering the same tension from scratch. No
other architectural issues were found — `XRFrameDriver`,
`OpenXRProjectionLayer`, `Frame::ViewInfo`, `VulkanFramebuffers`,
`VulkanRenderPass`, `VulkanGraphicsPipeline`, and `VulkanMesh` all
needed zero changes to support this milestone's refactor.

### Not Implemented (M9H, by design)

No `SceneRenderer`/`Scene` integration, no gameplay/player/weapon/
physics/audio/networking, no controller input or OpenXR actions, no
hand/eye tracking, no passthrough/anchors, no AR arena calibration, no
PBR/lighting/shadows/post-processing, no custom AR glasses integration
— all explicitly out of scope for M9H, deferred to M10 or later per
`docs/ROADMAP.md`. `XR_KHR_composition_layer_depth` was deliberately
not added. No new "XRRenderer"/"RenderGraph"/"FrameGraph"/"Renderer2"
class was created — the audit's own evidence justified exactly two
small, narrow additions (`OpenXRVulkanViewTarget`,
`RecordOpenXRViewRenderPass`), nothing larger.

- Files changed: new `tests/OpenXRVulkanViewTarget.hpp/.cpp`;
  `tests/openxr_cube_demo.cpp` (refactored to use the new helper,
  raised target frame count 200 → 1000, added slow cube rotation,
  added `FrameDiagnostics`); `tests/CMakeLists.txt`
  (`OpenXRVulkanViewTarget.cpp` added to the `arengine_openxr_cube_demo`
  target). No `engine/` source changed — this milestone's scope was
  entirely the demo layer and its own newly-extracted helper.

## 33. M10 Implementation Notes

M10 gives AREngine its first OpenXR action/input foundation: one
`XrActionSet`, four actions (a shared pose action plus boolean/float/
vector2f actions, each spanning both hands via subaction paths),
bindings suggested for the KHR simple_controller interaction profile,
the action set attached once, per-hand pose action spaces, and a
per-frame `xrSyncActions` + state-query path that converts real OpenXR
action state into a small, new generic vocabulary in `AREngine::Input`
— entirely separate from, and never corrupting, M7's existing keyboard/
mouse `KeyCode`/`MouseButton` model. No gameplay, no controller-driven
Scene/rendering integration, no hand tracking, no haptics.

### InputSystem Audit (M10)

Performed first, before any code, per the milestone's own requirement -
`engine/input/include/AREngine/Input/Input.hpp` and `Input.cpp` were
read in full, not skimmed:

1. **Reusable unchanged, as a *pattern*, not literal code**: `ButtonState`'s
   down/wasPressed/wasReleased semantics (M7) - "held across frames,
   pressed/released true for exactly the transition frame, never
   re-triggered while held" - is exactly the digital semantics M10
   needs for OpenXR boolean actions, and M10's `ConvertActionStateBoolean`
   deliberately mirrors it edge-for-edge (see below). `Core::Math::Vec2`
   (already used for mouse position/delta) is directly reused, unchanged,
   for 2D vector action state - no new 2D type was needed.
2. **Inherently keyboard/mouse-specific** (left untouched): `KeyCode`,
   `MouseButton`, `BindActionKey`/`BindActionMouseButton`,
   `GetMousePosition`/`GetMouseDelta`, and the `m_keys`/`m_mouseButtons`/
   `m_actions` storage keyed by those two enums. None of these gained a
   new case, a new enum value, or a new field for any XR concept - see
   "Do Not Put Tracked Pose Into KeyCode" below.
3. **Generic concepts missing for XR**: an explicit "active" bit (no
   keyboard/mouse equivalent - a physical key is always either present
   or absent; an OpenXR action can be bound but temporarily inactive),
   a float value, a 2D value distinct from mouse position (normalized
   thumbstick space, not client-area pixels), and a tracked pose
   (position + orientation + independent validity flags) - nothing
   resembling this existed anywhere in `Input`.
4. **Changes actually necessary in M10**: exactly one new header,
   `AREngine/Input/ActionState.hpp` (four small structs - see "Generic
   Input Extension" below). Zero changes to `Input.hpp`/`Input.cpp`
   themselves.
5. **Deferred**: wiring a live `Input::InputSystem` instance to actually
   receive OpenXR-sourced events (M10's demo has no `InputSystem`
   instance at all - there is nothing for it to feed events to yet,
   since no gameplay consumes them); a string-keyed named-action lookup
   layer for XR actions analogous to `InputSystem::m_actions` (M10's
   `OpenXRActionSystem` exposes typed accessors - `GetSelectState`,
   `GetTriggerState`, etc. - not a string-name API, since exactly four
   actions exist and typed accessors are simpler and cannot typo a
   name); merging KeyCode-based and OpenXR-based action names into one
   namespace.

### Generic Input Extension (M10)

New `AREngine/Input/ActionState.hpp`: `DigitalActionState`
(down/pressed/released/active), `AnalogActionState` (value/active),
`Vector2ActionState` (value/active), `PoseActionState`
(position/orientation/positionValid/orientationValid/active) - plain
data, zero OpenXR headers included, zero dependency on `Input.hpp`'s
existing `InputSystem`/`KeyCode`/`MouseButton` types. Matches the
milestone brief's own example shape closely, adjusted only for real
XR evidence (`AnalogActionState`'s `value` deliberately not clamped to
0..1 - the OpenXR spec does not guarantee every float component is
normalized). No `Input.hpp` public API leaks any OpenXR type - confirmed
by direct inspection (`ActionState.hpp` includes only `Core::Math`
headers) and structurally guaranteed by `engine/input`'s own
`CMakeLists.txt`, which links only `AREngine::Core`/`AREngine::Platform`
and never `AREngine::XR`.

### Do Not Put Tracked Pose Into KeyCode (M10)

`KeyCode`, `MouseButton`, and `InputSystem` itself are byte-for-byte
unchanged by this milestone. No controller pose, trigger value, or
thumbstick value is modeled as a synthetic `KeyCode` enumerator or
routed through `BindActionKey`/`IsActionDown` - confirmed by diffing
`Input.hpp`/`Input.cpp` against their pre-M10 state (zero lines
changed) and by grep (`KeyCode`, `MouseButton` never appear in any M10
file).

### Dependency Direction (M10)

`engine/xr` gained a new dependency on `AREngine::Input` (private, at
the same outer/Vulkan-independent tier as its existing `AREngine::Frame`
dependency - see `engine/xr/CMakeLists.txt`), mirroring the exact
precedent `OpenXRViewConversion.hpp` already set for `Frame::ViewInfo`
in M9F: a generic, backend-independent data-type module that a leaf-
adjacent OpenXR conversion file produces instances of. `AREngine::Input`
itself gained **no** new dependency - it still links only
`AREngine::Core`/`AREngine::Platform`, unaware OpenXR exists. No cycle:
the dependency runs XR → Input, never Input → XR, satisfying the
brief's explicit "Do NOT make InputSystem call OpenXR directly" and
"Generic Input must remain usable when `ARENGINE_ENABLE_OPENXR=OFF`"
requirements - confirmed by the OFF/OFF and OFF/ON build-matrix
configurations both building and passing `InputTests` unchanged (see
Validation below).

### Action System Placement / Vulkan-Independence (M10)

All seven new `engine/xr/src/openxr/` files
(`OpenXRActionPaths`, `OpenXRActionSet`, `OpenXRAction`,
`OpenXRActionSpace`, `OpenXRSimpleControllerBindings`,
`OpenXRActionStateConversion`, `OpenXRActionSystem`) were placed in the
OUTER, Vulkan-independent `target_sources` block in
`engine/xr/CMakeLists.txt` - not nested inside the
`if(ARENGINE_ENABLE_VULKAN)` block alongside `OpenXRSession`/
`OpenXRSwapchain`/`XRFrameDriver`. Justification, not assumption: every
OpenXR function this code calls (`xrCreateActionSet`, `xrCreateAction`,
`xrStringToPath`, `xrSuggestInteractionProfileBindings`,
`xrAttachSessionActionSets`, `xrCreateActionSpace`, `xrSyncActions`,
`xrGetActionState*`, `xrLocateSpace`) operates on `XrInstance`/
`XrSession`/`XrActionSet`/`XrAction`/`XrSpace`/`XrPath` - all core
`openxr.h` types, declared without `openxr_platform.h` - and
`OpenXRReferenceSpace.hpp` (M9D) already proves `XrSession`-consuming
code doesn't require Vulkan headers (it creates an `XrSpace` from a
session with zero Vulkan dependency, in this exact outer block).
Confirmed empirically, not just by inspection: the whole action-system
library builds and its pure-logic tests pass under `ARENGINE_ENABLE_OPENXR=ON`/
`ARENGINE_ENABLE_VULKAN=OFF` (see Validation below) - only the demo
executable (which needs a real `XrSession`, itself Vulkan-coupled in
this codebase's current session-creation path) is Vulkan-gated.

### Action Set (M10)

One action set, `"gameplay"` / `"Gameplay"`, priority 0 - no evidence
justifies more than one for M10's four actions. `OpenXRActionSet`
(new) owns the `XrActionSet` and exposes `Attach(instance, session)`
for `xrAttachSessionActionSets`.

### Subaction Paths (M10)

`/user/hand/left` and `/user/hand/right`, resolved once via
`xrStringToPath` (`OpenXRActionPaths.hpp`'s `ResolveHandSubactionPaths`)
- never a hard-coded numeric `XrPath`, per the spec's own guidance that
`XrPath` values are runtime-defined and not guaranteed stable across
runtimes or invocations.

### Actions To Create / Left-Right Hand Representation (M10)

Four actions, each created ONCE with BOTH hand paths passed as its
`subactionPaths` (the "shared action, distinguished by which subaction
path is passed at query time" model the brief calls for, matching
OpenXR's own subaction design) - never `left_select`/`right_select`
duplicate actions:

| Action | Type | Concept |
|---|---|---|
| `aim_pose` | `XR_ACTION_TYPE_POSE_INPUT` | tracked controller aim pose |
| `select` | `XR_ACTION_TYPE_BOOLEAN_INPUT` | primary digital select |
| `trigger` | `XR_ACTION_TYPE_FLOAT_INPUT` | analog trigger |
| `move` | `XR_ACTION_TYPE_VECTOR2F_INPUT` | 2D thumbstick/trackpad |

No tenth controller button was added - four actions is the minimal,
representative set the brief asked for, covering one of each OpenXR
action-type category.

### Interaction Profile / Suggested Bindings (M10)

**Only `/interaction_profiles/khr/simple_controller` is targeted**,
per the milestone's own explicit priority. Verified against the OpenXR
1.0 specification's standard interaction-profile table, not guessed:
simple_controller defines exactly `/input/select/click`,
`/input/menu/click`, `/input/grip/pose`, `/input/aim/pose`, and
`/output/haptic` per top-level user path - **no trigger and no
thumbstick/2D component exists on this profile at all**.
`SuggestSimpleControllerBindings` (new) therefore binds only `select`
→ `select/click` and `aim_pose` → `aim/pose`, for both hands;
`trigger`/`move` are deliberately left unbound under this profile -
they remain valid, created actions that correctly report
`isActive=false` while simple_controller is the active profile, rather
than being bound to a component path that doesn't exist (which the
runtime would reject, or which would silently mismap a physical input
that isn't really a trigger). A second, richer profile (e.g. one with
real trigger/thumbstick components) was deliberately NOT also targeted:
this environment's SteamVR null driver exposes no real controller
regardless of which profile(s) are suggested (confirmed - see "Runtime/
Controller Availability Findings" below), so adding a second profile
speculatively, with no evidence it would change observed behavior here,
was judged not justified by the brief's own "only if runtime support/
requirements justify it" standard. Deferred, not forgotten - see "What
Remains Deferred" below.

### Action Set / Action / Binding / Attach / Action-Space Ordering (M10)

`OpenXRActionSystem`'s constructor performs, in this exact order, all
in one place:

1. Resolve hand subaction paths.
2. Create the action set.
3. Create all four actions (each needs the action set to already
   exist).
4. Suggest simple_controller bindings (`xrSuggestInteractionProfileBindings`)
   - must precede attach, per the OpenXR spec.
5. Attach the action set to the session (`xrAttachSessionActionSets`) -
   **exactly once**, never repeated (no per-frame or per-sync call
   anywhere in this codebase).
6. Create the left/right `aim_pose` action spaces - after attach,
   matching established best practice (the OpenXR spec does not
   strictly require this ordering for `xrCreateActionSpace` itself, but
   every widely-used reference sample follows it, and no evidence in
   this milestone justified deviating).

Steps 1-4 are constructed via the member initializer list (in
declaration order); steps 5-6 happen in the constructor body, since
they are ordered actions on already-constructed members, not
sub-object construction themselves - the two pose-action spaces are
`std::unique_ptr<OpenXRActionSpace>` members specifically so their
construction can be deferred past attach (see the class's own header
comment).

### xrSyncActions Placement (M10)

Called once per `Frame::FrameStatus::Continue` tick (i.e. every tick
`PrepareFrame()` actually calls the real `xrWaitFrame`), immediately
after `BeginFrame()` and before any `GetXState()` call - `Continue`
itself is only ever returned once the session has reached RUNNING
(`PrepareFrame()`'s real `xrWaitFrame` call requires a running session
to succeed at all), so this placement satisfies "after the session is
running" by construction, not by a separate check. Deliberately **not**
gated on `shouldRender`: `xrSyncActions` and rendering-opportunity
throttling are unrelated OpenXR concepts (input responsiveness has no
principled reason to wait for a render opportunity), so this demo syncs
and queries every running frame regardless of `shouldRender` - confirmed
in the live run (500/500 frames synced, matching 500/500 frames
completed, even though - as expected - `shouldRender` was true on only
one of them, the same SteamVR/null-driver behavior every prior XR
milestone already documented).

### Digital/Analog/Vector2/Pose State Semantics (M10)

New `OpenXRActionStateConversion.hpp` (header-only, pure logic):

- **Digital** (`ConvertActionStateBoolean`): computed from an explicit
  up→down/down→up EDGE against the caller's own tracked `previousDown`
  - deliberately **not** derived from `XrActionStateBoolean::changedSinceLastSync`,
  which the brief itself warns is a related-but-different concept
  (it means "differs from the previous sync," not "the caller has
  observed and reacted to a press/release edge"). Mirrors M7's
  `ButtonState` model exactly: `pressed`/`released` true for exactly
  one call, `down` true for every call in between, never re-triggered
  while held. When `isActive` is false, the action is treated as
  not-held regardless of `currentState`, and if it WAS held on the
  previous call, a `released` transition is still synthesized -
  mirroring M7's `WindowFocusLostEvent` handling, which similarly never
  leaves a key/button stuck Down once the signal that would release it
  stops arriving. `OpenXRActionSystem` tracks `previousDown` per hand
  (`m_previousSelectDown`), updated every `GetSelectState` call.
- **Analog** (`ConvertActionStateFloat`): `value` passes through
  unchanged while active (never blindly clamped - the spec does not
  guarantee a 0..1 range for every float component), zero when
  inactive - never a stale previous reading.
- **Vector2** (`ConvertActionStateVector2f`): same "zero when inactive,
  pass through while active" policy, componentwise.
- **Pose** (`ConvertActionStatePose`): two-step query, exactly as the
  brief specifies - `xrGetActionStatePose` first (the action's own
  activity); only if active, `xrLocateSpace` against the caller-supplied
  base space/time. `positionValid`/`orientationValid` reflect the
  located space's own `XR_SPACE_LOCATION_POSITION_VALID_BIT`/
  `ORIENTATION_VALID_BIT` independently - position/orientation are only
  populated when their own bit is set, never fabricated otherwise.
  Reuses `ConvertXrPosition`/`ConvertXrOrientation`
  (`OpenXRViewConversion.hpp`, M9F) directly rather than duplicating -
  the same `XrVector3f`/`XrQuaternionf` → `Core::Math` conversion is
  correct for any `XrPosef`, not specific to view poses.

### Pose Validity Vs. Tracked (M10)

`XR_SPACE_LOCATION_POSITION_TRACKED_BIT`/`ORIENTATION_TRACKED_BIT` were
read and understood (a stricter condition than VALID - a runtime may
report a "last known good" pose as VALID-but-not-TRACKED during a brief
interruption, the same distinction M9F already established for view
poses) but were deliberately **not** added as a fifth/sixth field to
the generic `PoseActionState` - `positionValid`/`orientationValid` +
`active` already honestly represent "position/orientation valid does
not necessarily imply actively tracked" (an inactive action never
reports position/orientation as valid regardless of the underlying
space's own flags), and no concrete M10 consumer needs the finer TRACKED
distinction yet. Deferred, not overlooked - see "What Remains Deferred."

### Predicted Display Time For Poses (M10)

`XRFrameDriver` gained one new accessor, `GetLastPredictedDisplayTime() -> XrTime`
(mirroring the existing `GetLastLocatedXrViews()` "not part of the
generic `FrameDriver` interface" pattern) - the exact same
`XrTime` value `GetViews()`'s own `xrLocateViews` call already uses,
never wall-clock time, never a value this demo invents. `XrTime` itself
is never exposed to `AREngine::Input` - `OpenXRActionSystem::GetAimPoseState`
takes it as a parameter and consumes it internally; no generic
`Input::PoseActionState` field carries a timestamp at all (the brief's
own "convert to seconds or defer" instruction - deferred, since no
concrete consumer needs one yet).

### No Head Pose Duplication (M10)

Controller `aim_pose` action spaces are never used as, or confused
with, the head/view pose - `xrLocateViews` (M9F, `XRFrameDriver::GetViews()`)
remains the sole source of head/view poses; `OpenXRActionSystem::GetAimPoseState`
is a completely separate code path (`xrGetActionStatePose`/`xrLocateSpace`
against an action space, never a view). Both happen to use the same
LOCAL reference space and the same current predicted display time - a
deliberate, correct sharing of context (both need "where/when is this
frame"), not a conflation of what they represent.

### Stale/Inactive-State Behavior (M10)

Verified for each generic state type, not just digital: an action
that goes inactive never leaves `down=true`, a nonzero `value`, or a
nonzero `value.x`/`value.y` behind (see the conversion functions above
- each explicitly zeros/clears its own "value" field when `active` is
false), and a pose action that goes inactive reports
`positionValid=false`/`orientationValid=false` (via the early-return
`Input::PoseActionState{}` in `GetAimPoseState` when
`xrGetActionStatePose` reports `isActive=false` - the action's space is
never even located in that case, let alone trusted). Mirrors M7's own
focus-loss protection against stuck keys, generalized to XR's richer
state shapes.

### Runtime/Controller Availability Findings (M10)

**SteamVR's null driver exposes no real controller bound to any
interaction profile in this environment.** Confirmed by the live
500-frame run: every one of the four actions, on both hands, reported
`active=false` for the entire run - logged explicitly at the end
("No action was ever active on either hand this run..."), not silently
passed over. This is honest, expected behavior per the milestone's own
explicit allowance ("SteamVR null driver may expose no real controllers.
That is acceptable"), not a bug: the action set/actions/bindings/attach/
spaces/sync/queries all completed without error on every one of 500
frames - the *mechanism* is proven correct; only real physical input
was unavailable to exercise it further. No controller activity was
fabricated or worked around anywhere in this milestone's code.

**What would be needed for real interactive validation**: real
controller hardware paired with SteamVR (or another OpenXR runtime that
exposes a real or simulated controller bound to a supported interaction
profile) - not investigated further or installed, per the brief's
explicit "do not install another runtime automatically" instruction.
M9B's existing recommendation (Meta XR Simulator, Section 25) remains
the most plausible standards-based alternative already identified, if a
future milestone needs this; nothing was installed for M10.

### No Vendor Coupling, Reconfirmed (M10)

Grepped every new file for `SteamVR`, `Valve`, `driver_null`, `Meta`,
`Oculus`, and any vendor registry path - zero matches. The one
interaction profile targeted (`khr/simple_controller`) is a standard,
Khronos-defined OpenXR path, not vendor-specific.

### Validation (M10)

All five practically-distinct `ARENGINE_ENABLE_OPENXR` ×
`ARENGINE_ENABLE_VULKAN` configurations built and `ctest`-passed:
ON/ON (`/EHsc /W4 /WX` clean) 16/16 (one new suite,
`OpenXRActionTests`, over M9H's 15); ON/ON with `/EHsc /W4 /WX`
explicitly forced 16/16; ON/OFF 14/14 (confirming the action-system
library itself needs no Vulkan - see "Action System Placement" above);
OFF/ON (default) 11/11; OFF/OFF 10/10. `InputTests` passed unchanged in
every single configuration - the desktop keyboard/mouse regression the
brief asked for is confirmed, not assumed.

`arengine_openxr_input_demo` run against live SteamVR: 500/500 frames
completed, 500/500 synced (`xrSyncActions` called every running frame),
clean session-state sequence (`IDLE → READY → SYNCHRONIZED → STOPPING →
IDLE → EXITING`, identical shape to every prior XR milestone), exit
code 0, no crash. The `vkDestroyDevice` "14 leaked objects" validation
error (SteamVR-internal `BlankEyeBuffer`-tagged compositor bookkeeping,
same category M9E first traced) reappeared - **this time with even
stronger evidence it is not AREngine's**: this demo allocates zero
`VkCommandBuffer`/`VkFence`/`VkSemaphore`/`VkImage`/`VkDeviceMemory`
objects of its own at all (no command pool, no rendering, nothing but
the OpenXR action/space objects `OpenXRActionSystem` owns), yet the
same category and quantity of leaked handles appeared regardless -
conclusively SteamVR's own internal work, unrelated to anything this
demo's code allocates.

### Architectural Issues Found (M10)

None. `Frame::ViewInfo`, `XRFrameDriver` (beyond the one additive
accessor), `OpenXRSession`, and `AREngine::Input`'s existing
`InputSystem`/`KeyCode`/`MouseButton` all needed zero or trivially
additive changes to support this milestone - consistent with the "prove
the seam with the minimal thing first" philosophy this project has
followed since M0. The one new cross-module dependency
(`engine/xr` → `AREngine::Input`) was evaluated against this
project's established layering rules before being added and found
consistent with existing precedent (`engine/xr` → `AREngine::Frame`,
M9F), not a violation of anything.

### What Remains Deferred (M10)

A second, richer interaction profile (real trigger/thumbstick
components) - no evidence in this environment would exercise it
differently than simple_controller alone. `XR_SPACE_LOCATION_*_TRACKED_BIT`
as a distinct generic field. A live bridge from `OpenXRActionSystem`
into a running `Input::InputSystem` instance (no gameplay exists yet to
consume one). A string-keyed named-action lookup layer for XR actions.
`XR_EXT_hand_tracking`, `XR_ACTION_TYPE_VIBRATION_OUTPUT` (haptics),
eye tracking, passthrough, spatial anchors, arena calibration - all
explicitly out of scope per the brief, deferred to later milestones per
`docs/ROADMAP.md`.

- Files changed: new `engine/input/include/AREngine/Input/ActionState.hpp`;
  `engine/input/CMakeLists.txt` (one new header); new
  `engine/xr/src/openxr/OpenXRActionPaths.hpp/.cpp`,
  `OpenXRActionSet.hpp/.cpp`, `OpenXRAction.hpp/.cpp`,
  `OpenXRActionSpace.hpp/.cpp`, `OpenXRSimpleControllerBindings.hpp/.cpp`,
  `OpenXRActionStateConversion.hpp`, `OpenXRActionSystem.hpp/.cpp`;
  `engine/xr/src/openxr/XRFrameDriver.hpp` (one new accessor,
  `GetLastPredictedDisplayTime()`); `engine/xr/CMakeLists.txt` (new
  sources + `AREngine::Input` dependency); new `tests/openxr_action_tests.cpp`
  (13 new pure-logic tests), new `tests/openxr_input_demo.cpp`;
  `tests/CMakeLists.txt` (`OpenXRActionTests`/`arengine_openxr_input_demo`
  targets). `engine/input/include/AREngine/Input/Input.hpp`/`.cpp`
  themselves: unchanged.

## 34. M10.5 Implementation Notes

M10.5 builds AREngine's first INTEGRATED XR demo - one process, one
`XrSession`, one frame loop, that exercises the render path (M9G/M9H)
and the action/input path (M10) together, rendering multiple objects
into multiple views while syncing/querying controller state every
running frame. No new OpenXR/Vulkan capability was added - the goal was
proving these already-proven-independently systems coexist correctly,
per the milestone's own stated question: "does AREngine now have a
coherent XR application path?"

### Existing-Demo Audit (M10.5)

Performed first, per the milestone's own requirement. All six prior
OpenXR demos (`openxr_demo`, `openxr_vulkan_demo`, `openxr_session_demo`,
`openxr_frame_demo`, `openxr_cube_demo`, `openxr_input_demo`) were
inspected:

**A. Duplicated bring-up**: real and substantial - instance/system/
view-configuration/blend-mode/graphics-binding/session/reference-space
creation is reimplemented, near-verbatim, in every one of the six
demos (each later demo a superset of the previous one's bring-up).
Confirmed directly, not estimated: `tests/xr_demo.cpp` itself is a
seventh near-identical repetition of this exact sequence.

**B. Reusable engine functionality** (already properly factored into
`engine/xr`/`engine/rendering`, never duplicated - only the demo-level
orchestration around them is): `OpenXRInstance`, `OpenXRSystem`,
`OpenXRSession`, `OpenXRReferenceSpace`, `OpenXRSwapchain`,
`OpenXRProjectionLayer`, `XRFrameDriver`, `OpenXRActionSystem`
(M10), `OpenXRVulkanViewTarget` (M9H, `tests/`-level), every
`VulkanRenderPass`/`VulkanGraphicsPipeline`/`VulkanMesh`/etc class.

**C. Still demo-only** (legitimately different per demo, correctly NOT
shared): which of these resources each demo actually constructs
(`openxr_session_demo` has no swapchain at all; `openxr_input_demo` has
actions but no rendering; `openxr_cube_demo` has one object, no
actions); each demo's own stop conditions/log messages; small
file-local helpers (`CheckVkResultHere`, `SwapchainFormatToString`) -
deliberately duplicated per this codebase's own long-standing,
explicitly-documented convention (first stated in M9C/M9D, reaffirmed
at every subsequent demo including this one).

**D. Missing a clean integration boundary** - the audit's real
finding, and the one this milestone actually acted on: **no prior demo
exercised rendering and the action system in the same session**, and
**no prior demo rendered more than one object per view**. This is not
a code-duplication problem (category A) - it's an untested-integration
gap, exactly matching the milestone's own stated purpose. Two small,
targeted extensions closed it - see below.

**Decision on category A (bring-up duplication)**: deliberately left
UNCHANGED. This codebase's own long-standing convention (no shared
demo-setup helper exists between any two demos, restated explicitly in
M9G/M9H/M10's own documentation) was re-evaluated here, not just
inherited - bring-up decisions (which extension to request, what to log
at each stage, what counts as a graceful-stop condition) are
legitimately application-level judgment calls a real, different
application might make differently; consolidating them into a shared
helper for a SEVENTH demo would be premature abstraction for a
still-single-consumer concern, and would not have reduced any real
duplication this milestone actually needed to solve (the earlier six
demos are explicitly left untouched per the brief - "do not delete the
earlier bring-up demos... remain focused regression tools").

### Multi-Object Render Split (M10.5)

The one concrete extraction the audit justified: `OpenXRVulkanViewTarget.hpp/.cpp`'s
M9H/M9G-era `RecordOpenXRViewRenderPass` (one call = begin render pass +
one object's push-constants/draw + end render pass) was split into
three narrower functions - `BeginOpenXRViewRenderPass`,
`DrawOpenXRViewObject`, `EndOpenXRViewRenderPass` - so a view's render
pass can draw an arbitrary number of objects, each possibly using a
DIFFERENT `VulkanMesh` (M10.5 draws both a cube mesh and a floor quad
mesh into the same view). `RecordOpenXRViewRenderPass` itself is kept,
now implemented as `Begin` + one `DrawOpenXRViewObject` + `End` -
`tests/openxr_cube_demo.cpp` calls it unchanged and was rebuilt and
confirmed to still compile and behave identically (bit-for-bit the same
sequence of Vulkan calls for its single-object case). One deliberate
behavior change from M9G/M9H: `DrawOpenXRViewObject` now calls
`mesh.Bind()` itself (per-object), rather than assuming the caller
bound the (single, shared) mesh once outside the loop - necessary now
that different objects can use different meshes; harmless for
`openxr_cube_demo.cpp`'s still-present external bind call (rebinding
identical vertex/index buffers is idempotent, not a correctness issue -
confirmed by reading the Vulkan spec, not assumed). Pipeline and
descriptor-set binding remain once-per-frame (unchanged from M9G/M9H) -
only mesh identity varies per object, not the shared pipeline/texture.

### XR/Rendering Boundary, Reconfirmed (M10.5)

M9H already found and documented the real constraint: a genuinely
reusable per-view render-resource class needs both `OpenXRSwapchain`
(XR-private) and `VulkanImage`/`VulkanFramebuffers` (Rendering-private)
simultaneously, and promoting it into either `engine/xr` or
`engine/rendering` would require one to gain a new dependency on the
other - a boundary M9C/M9F/M9G each already deliberately avoided
crossing. M10.5 re-evaluated this with fresh evidence (a second,
harder case: multiple meshes, multiple objects, a genuine input-system
integration in the same file) and reached the **same conclusion**: the
extended `OpenXRVulkanViewTarget.hpp/.cpp` stays exactly where M9H put
it (`tests/`), and the new `xr_demo.cpp` itself stays a leaf that
crosses both boundaries directly - explicitly permitted by this
milestone's own brief ("if the cleanest solution still belongs in a
leaf/demo integration layer, that is acceptable. Do not force a new
engine module just to make it official"). No new engine module was
created. `engine/xr` gained no new dependency on `engine/rendering`;
`engine/rendering` gained no new dependency on `engine/xr` - confirmed
by inspecting both `CMakeLists.txt` files, unchanged by this milestone.

### No God-Object Coordinator (M10.5)

`xr_demo.cpp`'s `main()` directly owns every resource (instance,
binding, session, localSpace, actionSystem, swapchains, viewTargets,
renderPass/pipeline/meshes/texture, frameDriver) - exactly the same
shape every prior XR demo already uses, not a new
`XRApplicationThatOwnsEverything` wrapper class. `main()` itself is the
"small coordinator" the brief allows, not a class with its own
lifecycle - consistent with "avoid making one giant god object" and
"prefer a small coordinator if justified" (justified here by exactly
the same evidence-based reasoning as every prior demo: `main()`'s own
declaration order already gives correct RAII destruction, and no
runtime polymorphism or reuse-across-multiple-instances need exists to
justify wrapping it in a class).

### Scene Integration Evaluated And Declined (M10.5)

`Scene::Scene` (M5) was read in full and evaluated honestly against
this milestone's actual scene content (five flat, non-hierarchical
objects: a floor and four cubes, none parented to any other), not
declined by default. `Scene::Scene` provides entity identity +
`Transform` + parent/child hierarchy + `GetWorldMatrix()` (which walks
the parent chain) - for a scene with **zero** parent/child
relationships, `GetWorldMatrix(entity)` degenerates to exactly
`Transform::ToMatrix()`, which M10.5 already calls directly. `Scene`
also has **no concept of "which mesh/tint an entity uses"** - there is
no `MeshRenderer`-equivalent component - so using `Scene::Scene` here
would mean creating five entities via `CreateEntity()`/
`GetTransform()` purely to store the same transforms `SceneObject`
already stores directly, while STILL maintaining a separate, manual
`entity -> mesh/tint` mapping outside `Scene` (since `Scene` has
nowhere to put it). This provides zero real value over the demo's own
flat `std::vector<SceneObject>` for this specific scene shape -
confirmed by evaluation, not assumed. Deferred, not rejected outright:
a scene with real parent/child relationships, or a future
`MeshRenderer`-equivalent component, would change this calculus - see
"Recommended Next Milestone" below.

### No Scene::Camera For XR Eyes, Reconfirmed (M10.5)

Unchanged from M9G: XR view poses come exclusively from
`XRFrameDriver::GetViews()` (real `xrLocateViews` data); no
`Scene::Camera` was created for either eye. `Scene::Camera` remains
appropriate only for desktop rendering (`vulkan_present_demo.cpp`'s own
established use) - the two concepts are kept distinct, confirmed by
`xr_demo.cpp` never including `Scene/Camera.hpp` at all.

### Input Update Order (M10.5)

Per running (`FrameStatus::Continue`) tick, in this order:
`PrepareFrame()` (polls session events, calls the real `xrWaitFrame`) →
`BeginFrame()` (`xrBeginFrame`) → `actionSystem.SyncActions()`
(`xrSyncActions`, unconditional) → query select/trigger/move/aim-pose
for both hands (unconditional) → **if `shouldRender`**: `GetViews()`
(`xrLocateViews`) → render all five objects into each view → prepare
the projection layer → `EndFrame()` (`xrEndFrame`, always). Verified
against the OpenXR spec, not copied blindly from the brief's own
suggested pseudocode: `xrSyncActions`/`xrGetActionState*` have no
`shouldRender` precondition anywhere in the spec, so syncing/querying
input every running frame (not just renderable ones) is legal and
correct - confirmed by the live run, where `framesSynced` (1000)
matched `framesAttempted` (1000) exactly, independent of
`framesWithShouldRenderTrue` (1). This is the same placement M10's own
`openxr_input_demo.cpp` already established, now proven correct
alongside real rendering in the same tick, not just in isolation.

### Multi-Object/Multi-View Render Flow (M10.5)

For each rendered frame: acquire+wait both swapchain images → bind
pipeline+descriptor set once → for each view: `BeginOpenXRViewRenderPass`
→ for each of the five objects: compute
`MVP = Projection(view) * ViewMatrix(view) * Model(object)`
(`object.transform.ToMatrix()`, computed fresh each rendered frame -
same shared `Scene::Transform` used by every view, never a per-eye
copy) → `DrawOpenXRViewObject` → `EndOpenXRViewRenderPass` → submit
once → wait fence → release both images. **Draw count, observed and
matching the exact prediction**: 2 views × 5 objects = 10 draws on the
one frame that rendered (`viewsRendered=2`, `objectsRendered=10`,
`drawCalls=10`) - confirmed from the actual live-run log, not just
computed in the abstract. No instancing/batching was added (explicitly
out of scope).

### Ownership/Lifetime (M10.5)

Verified against actual OpenXR object-ownership rules, not assumed:
`OpenXRVulkanViewTarget`s (owns depth image + framebuffers) destroyed
before `swapchains` (which they borrow image views from) and before
`renderPass`/`pipeline` (which they were built against); `swapchains`
destroyed before `session` (each swapchain is session-scoped);
`actionSystem` (destroys its two pose action spaces, then its four
actions, then its action set) destroyed before `session` (action
spaces/actions/the action set are all session- and instance-scoped,
per the OpenXR spec's own object model - an `XrActionSet`'s actions are
implicitly destroyed with it, but this codebase explicitly destroys
each `OpenXRAction` first regardless, matching its own established
"every wrapper destroys its own handle" discipline); `localSpace`
destroyed before `session`; `session` destroyed before `binding`
(`VkDevice`, then `VkInstance`); `binding`/`session` both destroyed
before `instance` (`xrDestroyInstance` last). All of this follows
directly from `main()`'s own declaration order (C++ reverse-local-
destruction), documented explicitly in the function's own trailing
comment - not left implicit, per the brief's own explicit requirement.

### Performance Diagnostics (M10.5)

Carried forward from M9H/M10 (`FrameDiagnostics`, `std::chrono`-only,
extended with `objectsRendered`): from the live run - 1000 frames
attempted, 1000 frames synced, 1 frame with `shouldRender=true`, 1
frame rendered, 2 views rendered, 10 objects rendered, 10 draw calls,
average CPU frame-prep time 5.08ms, average Vulkan submit-to-fence time
0.72ms (both measured over the one rendered frame - higher than M9H's
single-object 1.75ms/0.56ms, consistent with 10 draws + 2 mesh rebinds
instead of 2 draws with one shared mesh, not a regression).

### SteamVR/Null Limitations, Reconfirmed (M10.5)

Both already-documented limitations reappeared, unchanged, run
together for the first time: `shouldRender=true` on frame 1 only (same
root cause M9E/M9H already investigated - the session never reaches
`XR_SESSION_STATE_FOCUSED` in this environment), and zero real
controller activity on either hand for the entire run (same finding
M10 already established). Neither was worked around, faked, or
special-cased - both are reported honestly in the run's own final
summary log lines. `arengine_xr_demo` ran 1000/1000 lifecycle
iterations cleanly regardless, proving the full integrated system
(session, actions, frame driver, render targets) remains stable across
a long run even though only one of those iterations exercised real
rendering.

### Validation (M10.5)

All five practically-distinct `ARENGINE_ENABLE_OPENXR` ×
`ARENGINE_ENABLE_VULKAN` configurations built and `ctest`-passed: ON/ON
(`/EHsc /W4 /WX` clean) 16/16 (one new pure-logic test added,
`TestQuaternionRotateFloorOrientationMapsLocalZToWorldUp`, verifying
the hand-derived floor-orientation math this demo relies on); ON/ON
with `/EHsc /W4 /WX` explicitly forced 16/16; ON/OFF 14/14; OFF/ON
(default) 11/11; OFF/OFF 10/10. `InputTests` passed unchanged in every
configuration.

`arengine_xr_demo` run against live SteamVR: 1000/1000 frames
completed, 1000/1000 synced, clean session-state sequence (`IDLE →
READY → SYNCHRONIZED → STOPPING → IDLE → EXITING`), exit code 0, no
crash. The `vkDestroyDevice` "14 leaked objects" validation error
(SteamVR-internal `[BlankEyeBuffer]`-tagged compositor bookkeeping,
same category traced since M9E) reappeared, structurally unchanged -
still three different `VkCommandBuffer`/four different `VkFence`
handles than this demo's own single reused pair. Sandbox and
`arengine_vulkan_present_demo` both launched and ran without crashing.

### Architectural Issues Found (M10.5)

None new. The XR/Rendering boundary tension M9H first found was
re-evaluated with harder evidence (multi-object, multi-mesh, combined
render+input) and reached the same conclusion, not a new one - see "XR/
Rendering Boundary, Reconfirmed" above. `Frame::ViewInfo`, `FrameDriver`,
`OpenXRActionSystem`, and `Input::ActionState` all needed zero changes
to support this integration (`FrameDriver` and `ViewInfo` were
explicitly re-checked against this milestone's own new evidence per the
brief's instruction not to speculatively refactor them, and neither
needed to change).

### What Is Now Proven As An Integrated Engine Path (M10.5)

One `XrSession`, one frame loop, running the real OpenXR frame
lifecycle (M9E.5), real per-view pose/projection data (M9F), multiple
objects with independent transforms/tints rendered into multiple real
XR swapchain images using M9H's hardened per-view resource model, and
the full OpenXR action/input system (M10) synced and queried every
running frame - all in the same process, the same session, the same
loop, for 1000 consecutive lifecycle iterations, with zero AREngine-
attributable errors and a clean session-state sequence both times
(bring-up and shutdown). This is genuinely new evidence, not a
restatement of what M9G/M9H/M10 already proved separately: those
milestones each proved their own subsystem in isolation; this milestone
proves none of them interfere with each other when combined.

### Recommended Next Milestone (M10.5)

Based on this milestone's own evidence, not a general roadmap
restatement: the render and input paths are now proven integration-
ready, but nothing today connects a *changing* application state
(gameplay logic, a real Scene with parented/animated entities, an
actual input-driven mapping) to what gets rendered - `xr_demo.cpp`'s
scene is still a static, hand-written object list, and its action
queries are logged, never acted on. The most evidence-justified next
step is **not** a `SceneRenderer` (no concrete rendering-side gap
justifies one yet - `OpenXRVulkanViewTarget`'s Begin/Draw/End split
already handles "many objects, many views" correctly) but rather the
first small, deliberate use of queried input to affect *something*
already on screen (e.g. driving the reference cube's rotation from
`select`/`trigger` instead of a hard-coded clock, if a continuous-
render-capable runtime becomes available to actually observe it) -
still not gameplay, but the first genuine "input affects state affects
render" loop, which is the concrete gap this milestone's own audit
surfaced. Continuing to investigate a continuous-`shouldRender`-capable
runtime/simulator (Meta XR Simulator remains the standing,
not-yet-installed recommendation from M9B) would make any such future
milestone's own validation far more conclusive than this environment
currently allows.

- Files changed: `tests/OpenXRVulkanViewTarget.hpp/.cpp` (Begin/Draw/End
  split, `RecordOpenXRViewRenderPass` kept as a convenience wrapper);
  new `tests/xr_demo.cpp`; `tests/core_tests.cpp` (one new pure-logic
  test, floor-orientation math); `tests/CMakeLists.txt`
  (`arengine_xr_demo` target). No other `engine/` source changed -
  every subsystem this milestone integrates (`OpenXRActionSystem`,
  `XRFrameDriver`, `OpenXRProjectionLayer`, `OpenXRSwapchain`,
  `Scene::Transform`, `Rendering::CreateCubeMesh`/`CreateQuadMesh`) was
  reused completely unchanged.

## 35. M10.6 Implementation Notes

M10.6 closes the exact gap M10.5's own "Recommended Next Milestone"
section identified: input was queried and logged, but never affected
anything. M10.6 connects real, queried OpenXR action state (M10),
through the existing generic `Input::*ActionState` types, through a new
small pure interaction layer, to visible object transform/tint changes
in the M10.5 integrated demo's shared scene - proving the generic input
state is actually useful to engine logic, not just observable.

### Audit Of The Current Integrated Demo (M10.6)

Performed first, per the milestone's own requirement:

1. **Where OpenXR action states are currently queried**: entirely
   inside `tests/xr_demo.cpp`'s main loop, via
   `OpenXRActionSystem::GetSelectState`/`GetTriggerState`/`GetMoveState`/
   `GetAimPoseState` - four calls per hand, per running frame, already
   established since M10.5.
2. **Where generic `Input::*ActionState` values exist**: as the direct,
   immediate return values of those four calls - `DigitalActionState`/
   `AnalogActionState`/`Vector2ActionState`/`PoseActionState` local
   variables, previously consumed only by `LogHandState` (diagnostic
   logging - no effect on rendering).
3. **The smallest clean place to apply those values to engine state**:
   right where the values already exist, immediately after they're
   queried and logged - one small function call per action type,
   mutating one shared `XRInteractionState` local variable, applied to
   the already-existing `sceneObjects` list a few lines later (in the
   same per-object loop that already updates the reference cube's
   rotation).
4. **Whether a small reusable interaction helper is justified**: yes -
   four small, independently-testable pure functions
   (`ApplyDigitalToggle`/`ApplyAnalogScale`/`ApplyVectorOffset`/
   `ApplyPoseMarker`), not a monolithic "update everything" function,
   matching the milestone's own per-category test list exactly. See
   "Where Interaction Logic Lives" below for why this stays a `tests/`-
   level helper, not an engine module.
5. **What must remain demo-only**: the exact `XRInteractionState` field
   shapes and their mapping to this specific scene's five objects
   (which cube gets highlighted, which gets offset, what the marker
   looks like) - genuinely specific to this one demo's own
   visualization choices, not a generic concept.

### Input-To-State Mapping (M10.6)

New `tests/XRInteractionState.hpp/.cpp`:

```cpp
struct XRInteractionState {
    bool highlightEnabled = false;
    float scaleFactor = 1.0f;
    Core::Math::Vec2 moveOffset;
    bool poseMarkerVisible = false;
    Core::Math::Vec3 poseMarkerPosition;
    Core::Math::Quaternion poseMarkerOrientation = Core::Math::Quaternion::Identity();
};
```

Deliberately close to, but not copied blindly from, the milestone
brief's own example - trimmed to exactly what this demo's chosen
visualization needs (no unused fields). Four pure functions mutate one
field-group each: `ApplyDigitalToggle`, `ApplyAnalogScale`,
`ApplyVectorOffset`, `ApplyPoseMarker` - see their own header comments
in `XRInteractionState.hpp` for the exact policy each one implements
(reproduced below per action type). This header includes ONLY
`AREngine::Input`/`AREngine::Core::Math` - confirmed by its own include
list, no `<openxr/openxr.h>` anywhere - satisfying the milestone's
"rendering must not know `XrAction`/`XrPath`" requirement one level
more strictly: the INTERACTION LOGIC itself doesn't know either, not
just the renderer.

**Which hand drives the interaction**: the right hand only - a simple,
deliberate choice (no two-hand combination policy has any evidence to
justify its shape yet), applied inside the existing per-hand query loop
(`if (hand == Hand::Right) { Apply*(...); }`), immediately after that
hand's four states are queried and logged.

### Digital Mapping (M10.6)

`ApplyDigitalToggle(select, state)`: `if (select.pressed) { state.highlightEnabled = !state.highlightEnabled; }`
- reacts to `pressed` (the up→down edge, computed by M10's own
`ConvertActionStateBoolean`), never `down` - a held select cannot
re-toggle every frame, confirmed by a dedicated test
(`TestDigitalHeldWithoutPressedDoesNotToggleAgain`) that calls the
function twice in a row with `down=true, pressed=false` and asserts no
change. An inactive `DigitalActionState` always has `pressed=false` by
construction (M10's own conversion guarantee), so no separate
active-state check was needed in this function - confirmed explicitly
by `TestDigitalInactiveDoesNotSpuriouslyToggle`, not just assumed to
follow from the other tests.

### Analog Mapping (M10.6)

`ApplyAnalogScale(trigger, state)`: `state.scaleFactor = trigger.active ? (1.0f + trigger.value * kScaleFactorRange) : 1.0f`
(`kScaleFactorRange = 0.8`) - trigger 0.0 → 1.0x (base scale), trigger
1.0 → 1.8x. Inactive always resets to exactly 1.0, never a stale prior
active reading - confirmed by
`TestAnalogStaleValueNotReusedAfterInactive`, which sets an active
max-scale reading first, then an inactive one with the SAME
`.value=1.0` still present (as a real runtime might report), and
asserts the neutral default wins.

### Vector Mapping (M10.6)

`ApplyVectorOffset(move, state)`: `state.moveOffset = clamp(move.value * kMoveOffsetMetersPerUnit, ±kMaxMoveOffsetMeters)`
while active, `(0,0)` while inactive - a direct, unflipped, per-
component linear mapping (positive `move.x` → positive `offset.x`,
same for `y` - no axis inversion anywhere in this function, confirmed
by dedicated positive/negative-X tests and a positive-Y test). No
deadzone was added - no current input evidence (this runtime never
reports an active move action at all) justifies one; deferred, not
overlooked. Deliberately NOT integrated over delta time - `moveOffset`
is a direct function of the CURRENT `move.value` each time it's called,
never accumulated - this is a visualization of the current input
state, not a player-controller movement system (the brief's own
explicit "do NOT use delta-time movement like WASD gameplay"
instruction, honored by construction: the function has no delta-time
parameter to even accept).

### Pose Mapping (M10.6)

`ApplyPoseMarker(pose, state)`: the marker is visible only when
`pose.active && pose.positionValid && pose.orientationValid` - **the
conservative "both must be valid" policy**, chosen deliberately over a
"partially valid" alternative (e.g. showing the marker at a valid
position with a defaulted orientation) because a marker with only half
its transform genuinely reflecting reality is more likely to mislead a
reader than an honestly-hidden one; documented here as an explicit
choice, per the brief's own "according to chosen policy" instruction,
not left implicit. When visible, `poseMarkerPosition`/`Orientation` are
copied directly from the generic `PoseActionState` (already in LOCAL
space, the same space every `Scene::Transform` in this demo already
uses - no conversion needed); when not visible, both reset to their
type defaults (identity/zero), never a stale previous pose - confirmed
by `TestPoseHiddenResetsToDefaultNotStalePose`. Orientation-copy
correctness is verified with a per-component-asymmetric test quaternion
(`TestPoseOrientationCopiedCorrectly`), not identity-only, so a
component-reorder bug would actually be caught.

### Where Interaction Logic Lives (M10.6)

Kept at the `tests/` leaf (`XRInteractionState.hpp/.cpp`), **not**
promoted into `engine/input`. Evaluated explicitly, not assumed: the
underlying MECHANISMS (toggle-on-press, linear analog-to-range mapping,
bounded vector offset, valid-pose gating) are each only a few lines,
and the STRUCT's own fields (`highlightEnabled`, a single
`scaleFactor`, one `moveOffset`, one pose marker) are tailored to this
specific demo's own five-object scene and its own visualization
choices - not a shape any other application would reuse verbatim. Built
unconditionally (not gated behind `ARENGINE_ENABLE_OPENXR`/
`ARENGINE_ENABLE_VULKAN` - confirmed by building and testing it under
OFF/OFF), demonstrating it could technically live in a generic engine
location, but "could" is not "should" without a second real consumer -
promoting it now would be "engine API to make the milestone look
larger," which the brief explicitly warns against.

### InputSystem Bridge Question (M10.6)

**Not needed, and not added.** M10's own `OpenXRActionSystem`/
`OpenXRActionStateConversion.hpp` already IS the "OpenXR → generic
Input types" bridge the brief describes - M10.6 needed nothing new
here, only a consumer for the values that bridge already produces.
`Input::InputSystem` itself (the keyboard/mouse class) was not touched,
and no live bridge feeds OpenXR-sourced events into a running
`InputSystem` instance - `tests/xr_demo.cpp` still has no
`InputSystem` instance at all, since nothing in this demo needs
keyboard/mouse-style action-name lookup. This matches M10's own
explicit deferral ("M10 intentionally stopped before a full live
bridge... M10.6 should only add a bridge if the integrated demo finds a
concrete need") - it didn't.

### Scene Question, Reconfirmed (M10.6)

Still declined, unchanged from M10.5's own evaluation - the interaction
state's effects are applied directly to the existing hand-owned
`std::vector<SceneObject>`'s `Transform`/tint fields, not through
`Scene::Scene`. Nothing about adding input-driven mutation changed the
underlying calculus (still a flat, non-hierarchical, small object
list) - re-confirmed, not just carried over without re-checking.

### Update Ordering (M10.6)

Verified against the current architecture, not copied blindly from the
brief's own suggested pseudocode: `PrepareFrame()` → (`Stop`/`Idle`
handling, unchanged) → `BeginFrame()` → `xrSyncActions` (unconditional)
→ query + log + **apply interaction state** for both hands (right hand
only feeds `Apply*`) → **if `shouldRender`**: `GetViews()` → apply the
now-current `interactionState` to `sceneObjects` (tint/scale/offset/
marker, alongside the existing rotation update) → render all visible
objects into each view → prepare the projection layer → `EndFrame()`
(always). The interaction-state COMPUTATION happens every running
frame (cheap, pure logic); its APPLICATION to the renderable
`SceneObject` transforms happens only inside the render-gated block,
since those fields are only ever read during rendering - a deliberate
efficiency choice, not a correctness requirement (applying it
unconditionally would also have been correct, just wasted work on
non-rendering frames).

### shouldRender=false Behavior, Reconfirmed (M10.6)

Input synchronization and `interactionState` computation both continue
on every running frame regardless of `shouldRender` - unchanged from
M10.5's own already-documented decision, re-confirmed here with the
concrete new evidence that the interaction state genuinely does keep
updating: the live run's `framesSynced` (1000) equals `framesAttempted`
(1000), independent of `framesWithShouldRenderTrue` (1). This is
desirable, not incidental: engine-owned interaction state should not
freeze just because the runtime skipped a visual-render opportunity.

### Shared World State, Reconfirmed (M10.6)

`interactionState` is computed exactly ONCE per frame (not once per
view) and applied to `sceneObjects` exactly once, before the per-view
render loop begins - every view reads the SAME already-updated
`Transform`/tint values, never a per-eye recomputation. Confirmed
structurally (no per-view field or parameter exists anywhere in
`XRInteractionState` or the four `Apply*` functions) and by a dedicated
test, `TestInteractionStateIsIndependentOfViewCount`.

### No Controller Rendering System, Reconfirmed (M10.6)

The pose marker reuses the existing persistent cube mesh at a small
scale (`0.15`) and a distinct tint - no `ControllerRenderer`/
`HandRenderer`/`WeaponRenderer` was created, and none was justified by
anything this milestone found.

### Test-vs-Live Evidence Distinction (M10.6)

Reported explicitly, per the brief's own "do not merge those claims"
instruction:

**A. Generic input-to-engine-state behavior, proven by pure unit
tests** (`tests/xr_interaction_tests.cpp`, 20 tests, synthetic
`Input::*ActionState` values, zero OpenXR/Vulkan dependency): digital
toggle (press-only, held-does-not-retrigger, inactive-never-toggles),
analog scaling (inactive/0/0.5/1.0/stale-value-not-reused), vector
offset (inactive/positive-X/negative-X/positive-Y/clamping), pose
marker (inactive-disabled/active-but-invalid-disabled/active-valid-
shows-correct-pose/orientation-copied-correctly/hidden-resets-to-
default), and shared-state-independent-of-view-count. All 20 pass.

**B. Real OpenXR action synchronization/querying, proven against
SteamVR/null**: `xrSyncActions` succeeded on all 1000/1000 running
frames; `GetSelectState`/`GetTriggerState`/`GetMoveState`/
`GetAimPoseState` were called (and their results fed into the real
`Apply*` functions, not synthetic stand-ins) on every one of those
frames without error; the resulting `interactionState` reached the end
of the run in its neutral/default form (`highlightEnabled=false`,
`scaleFactor=1.00`, `moveOffset=(0.00,0.00)`, `poseMarkerVisible=false`)
- exactly the value a correctly-wired, correctly-inactive pipeline
should produce.

**C. Real physical controller-driven visual change - NOT yet proven**,
because no active controller was available in this environment for the
entire run. This is reported as a real, current limitation, not
glossed over or implied to be equivalent to (A) or (B).

### Real Active-Input Limitation / Recommended Test Environment (M10.6)

Unchanged from M10/M10.5's own already-reported finding: SteamVR's
null driver exposes no real controller bound to any interaction
profile in this environment. For real active-controller validation, the
recommended next test environment remains M9B's original,
not-yet-installed recommendation - **Meta XR Simulator** - or a real
Quest/SteamVR controller/headset. Nothing was installed for this
milestone, per the brief's explicit instruction.

### Validation (M10.6)

All five practically-distinct `ARENGINE_ENABLE_OPENXR` ×
`ARENGINE_ENABLE_VULKAN` configurations built and `ctest`-passed: ON/ON
(`/EHsc /W4 /WX` clean) 17/17 (one new suite, `XRInteractionTests`, 20
pure-logic tests, over M10.5's 16); ON/ON with `/EHsc /W4 /WX`
explicitly forced 17/17; ON/OFF 15/15; OFF/ON (default) 12/12; OFF/OFF
11/11 - `XRInteractionTests` present and passing in **every**
configuration, including OFF/OFF, confirming the interaction logic
genuinely has zero OpenXR/Vulkan dependency. `InputTests` passed
unchanged in every configuration - `Input::InputSystem` itself was not
touched by this milestone.

`arengine_xr_demo` run against live SteamVR: 1000/1000 frames
completed, 1000/1000 synced, clean session-state sequence (`IDLE →
READY → SYNCHRONIZED → STOPPING → IDLE → EXITING`), exit code 0, no
crash. Scene now 6 objects (5 base + 1 pose marker, hidden by default);
the one rendered frame drew 2 views × 5 visible objects = 10 draws
(the marker excluded, matching its default-hidden state exactly). The
same already-traced SteamVR-internal `[BlankEyeBuffer]` validation
noise reappeared, structurally unchanged. Sandbox and
`arengine_vulkan_present_demo` both launched and ran without crashing.

### Architectural Issues Found (M10.6)

None. No engine module was added or changed - `Input::InputSystem`,
`OpenXRActionSystem`, `Frame::ViewInfo`, and `FrameDriver` all remained
exactly as M10/M10.5 left them.

### Recommended Next Milestone (M10.6)

Based on this milestone's own evidence: the full chain from real OpenXR
input, through generic `Input::*ActionState`, through pure interaction
logic, to a rendered visual result is now proven correct end to end -
proven by unit test for the logic itself, proven live for the OpenXR
plumbing around it, honestly reported as unproven for the one piece
(a real active controller) this environment cannot supply. The most
evidence-justified next step is **not** more input mapping (four action
types across one demo scene is already representative, per the
brief's own "minimal but representative" instruction) but rather
acquiring or investigating a genuinely continuous-render/active-input-
capable test environment (Meta XR Simulator remains the standing
recommendation) so that a future milestone's own live validation can
finally exercise the code paths this milestone proved only by unit
test - `ApplyDigitalToggle`/`ApplyAnalogScale`/`ApplyVectorOffset`/
`ApplyPoseMarker` are all fully exercised in isolation, but none has
ever been driven by a real, changing, physically-sourced value.

- Files changed: new `tests/XRInteractionState.hpp/.cpp`; new
  `tests/xr_interaction_tests.cpp` (20 pure-logic tests); `tests/xr_demo.cpp`
  (interaction state wired into the existing per-hand query loop and
  per-object update loop; scene grew from 5 to 6 objects - one new pose
  marker, hidden by default; visibility check added to the draw loop);
  `tests/CMakeLists.txt` (`XRInteractionTests`/`arengine_xr_interaction_tests`
  target, unconditional; `XRInteractionState.cpp` added to
  `arengine_xr_demo`'s sources). No `engine/` source changed at all -
  `Input::InputSystem` and every OpenXR/XR wrapper class were reused
  completely unchanged.

## M11 - Environment Research and Timeline Semaphore Negotiation

### Meta XR Simulator (M11/M11.1, research and installation only)

M11 was a research-only milestone investigating a headset-free OpenXR
development environment. It recommended Meta XR Simulator (v205.0,
official Meta distribution) as the primary candidate: a real, conformant
OpenXR runtime supporting `XR_KHR_vulkan_enable2`, with simulated
head/controller input, distinct from SteamVR's null driver. M11.1
installed and validated it: activated via Meta's own official
`activate_simulator.ps1`/`deactivate_simulator.ps1` toggle scripts (never
a manual registry edit), which preserve the prior `ActiveRuntime` into a
`PreviousActiveRuntime` registry value and restore it on deactivation -
confirmed by reading both scripts' source directly, not merely by
documentation.

Validated against the real simulator (system name "Meta Quest 3",
`XrSystemId` reported, `XR_KHR_vulkan_enable2` present): the full OpenXR
session-state sequence (`IDLE`→`READY`→`SYNCHRONIZED`→`VISIBLE`→`FOCUSED`)
is reached, and `shouldRender` becomes **continuously true** across
hundreds of frames - a genuine improvement over SteamVR/null, which
never sustains that (see M9H). Real per-view stereo poses (~30mm IPD)
and differing per-eye asymmetric FOV are reported. The 1000-frame
integrated demo (`arengine_xr_demo`, M10.5/M10.6) rendered all 1000
frames successfully with real M10.6 interaction state wired, but the
process crashed during clean session exit - see "Clean-Exit Crash"
below, which M11.1 found and M11.2 investigated further.

### Timeline Semaphore Device-Feature Negotiation (M11.2)

**Finding**: Meta XR Simulator's compositor uses a
`VK_SEMAPHORE_TYPE_TIMELINE` semaphore on AREngine's shared XR `VkDevice`
and fails Vulkan validation
(`VUID-VkSemaphoreTypeCreateInfo-timelineSemaphore-03252`) unless the
`timelineSemaphore` feature is enabled at device creation.
`OpenXRVulkanGraphicsBinding`'s device only enabled core Vulkan 1.0
features via `pEnabledFeatures` (see "Vulkan Device Feature Requirement
Discovered in M9D" above) - deliberately, because that section's own
history found a real conflict when the app supplied a
`VkPhysicalDeviceVulkan12Features` struct: SteamVR's own
`xrCreateVulkanDeviceKHR` unconditionally injects its own
`VkPhysicalDeviceFeatures2`-wrapped `VkPhysicalDeviceTimelineSemaphoreFeatures`,
and Vulkan's spec forbids an app-supplied `VkPhysicalDeviceVulkan12Features`
co-existing with any of its individually-promoted structs (including
`VkPhysicalDeviceTimelineSemaphoreFeatures`) in the same `pNext` chain.
Re-auditing that history in M11.2 found every variation tried in M9D
used the *aggregate* `VkPhysicalDeviceVulkan12Features` struct - the
*narrow* `VkPhysicalDeviceTimelineSemaphoreFeatures` struct alone,
without the aggregate, had never actually been tried.

**Fix**: `OpenXRVulkanGraphicsBinding.cpp` now queries
`vkGetPhysicalDeviceFeatures2`/`VkPhysicalDeviceTimelineSemaphoreFeatures`
capability directly (gated on the selected Vulkan API version being
>= 1.1, since `vkGetPhysicalDeviceFeatures2` is core-1.1) and decides
whether to enable the feature via a new pure-logic helper,
`SelectTimelineSemaphoreSupport` (`OpenXRVulkanRequirements.hpp`):
purely capability-driven (physical-device support + selected API
version + `VK_KHR_timeline_semaphore` extension availability below
1.2), with **zero runtime-name checks anywhere**. When the feature is
supported, the device is created via `VkPhysicalDeviceFeatures2` (chaining
only the narrow `VkPhysicalDeviceTimelineSemaphoreFeatures`, never the
aggregate `VkPhysicalDeviceVulkan12Features`) with the existing core-1.0
`enabledFeatures` moved into `VkPhysicalDeviceFeatures2::features`
unchanged; when unsupported, device creation is byte-for-byte the same
`pEnabledFeatures` path as before M11.2. Below Vulkan 1.2 with the
feature supported, `VK_KHR_timeline_semaphore` is added to the device
extension list; at 1.2+, it's core and the extension is never requested.
Desktop Vulkan device creation (`engine/rendering/src/vulkan/`) is
completely untouched.

**Result on Meta XR Simulator**: the physical device (NVIDIA RTX 3060
Laptop GPU) reports `timelineSemaphore` support; Vulkan 1.2.0 selected
(core, no extension needed); the original
`VUID-VkSemaphoreTypeCreateInfo-timelineSemaphore-03252` error is
**completely gone** across every post-fix run of `arengine_openxr_vulkan_demo`,
`arengine_openxr_session_demo`, `arengine_openxr_frame_demo`, and
`arengine_xr_demo`.

**Result on SteamVR**: device creation itself (`arengine_openxr_vulkan_demo`,
`arengine_openxr_session_demo`) remains completely clean - zero
validation errors or warnings, confirming the narrow
`VkPhysicalDeviceTimelineSemaphoreFeatures` struct does **not**
reintroduce the M9D aggregate-struct conflict. `timelineSemaphore` is
now genuinely requested by AREngine on SteamVR too (previously it
worked only because SteamVR's own unconditional injection supplied it
independently of anything AREngine asked for).

### M11.2 Result Summary: Three Separate Problems

```
Problem A - Timeline semaphore used without the feature enabled -> FIXED
Problem B - Meta XR Simulator crashes during teardown           -> STILL OPEN
Problem C - SteamVR shows new teardown validation noise once
            timeline semaphores are genuinely enabled            -> NEW EVIDENCE, UNPROVEN CAUSE
```

M11.2 fixed Problem A only. Problems B and C are separate, unresolved
findings for a future milestone (see below) - M11.2 must not be read as
having solved "the Meta validation problem" as a whole.

### Clean-Exit Crash (found M11.1, persists after M11.2's fix - unresolved)

Both `arengine_openxr_frame_demo` (M9E, 200-frame target) and the
1000-frame `arengine_xr_demo` (M10.5/M10.6) crash on Meta XR Simulator
during clean session exit, after their real frame-loop work otherwise
completes (`STATUS_ACCESS_VIOLATION`/`0xC0000005` on one run,
`STATUS_FATAL_APP_EXIT`/`0x40000015` on another - not deterministic).
`vkDestroyDevice` reports a variable number of leaked objects (a
`VkSemaphore`, swapchain `VkImage`s, `VkDeviceMemory`, a `VkCommandPool`,
ranging from 1 to 34 objects across runs) immediately before the crash,
and one observed run additionally showed a genuine cross-thread race
(`vkDestroyDevice` on one thread concurrent with
`vkFreeMemory`/`vkDestroySemaphore` on another).

**Ownership of these objects/threads is NOT yet established and must
not be assumed.** `xrCreateVulkanDeviceKHR` gives the runtime visibility
into the same device creation and lets it create device children
internally, so the leaked/racing handles could belong to either side -
concluding "the runtime's own compositor, not AREngine" from AREngine's
own code being single-threaded is not valid evidence; it requires actual
handle/lifetime tracking to confirm. Similarly, the absence of
AREngine's "Diagnostics:"/"complete - shutting down" log lines in a
crashed run's captured stdout is **not** strong evidence those lines
never printed - C stdio buffering can lose already-written-but-unflushed
output when a process is killed abruptly, especially under
`Start-Process`-style redirection to a file. Whether AREngine's own
destructors (which already call `vkDeviceWaitIdle` before manual cleanup
- the standard Vulkan mechanism for ensuring the app's own submitted
work has completed before device destruction, though not necessarily
sufficient for work the runtime itself submitted independently) ever
began running is genuinely unknown from the evidence gathered so far.

The M11.2 fix eliminated the timeline-semaphore validation *error* but
did not eliminate this crash. One plausible (not confirmed) explanation:
properly enabling `timelineSemaphore` lets the runtime's compositor
engage real timeline-semaphore-based synchronization internally,
exposing a pre-existing thread-safety bug in that path that a
silently-degraded fallback previously avoided. Meta's own v205 release
notes reportedly describe Vulkan support in XR Simulator as limited and
list several simulator crash/thread-safety fixes including teardown/
stability work, making a simulator-side bug entirely plausible - but
this must be proven with direct evidence, not assumed from precedent.
Per M11.2's own explicit instruction, no teardown-order change was
attempted to chase this.

**M11.3 should instrument teardown with unbuffered/immediately-flushed
logging (e.g. a distinct stream, or explicit flush after each line)
around every individual destruction step** (fence, command pool,
framebuffers, swapchains + their cached image views, action system,
local space, session, graphics-binding device/instance, XR instance),
so a crashed run's true last-completed step - and therefore whether
AREngine's own destructors started, and which one was in progress - can
be established directly, rather than inferred from potentially-lost
buffered output.

A related but distinct finding: SteamVR's `arengine_openxr_frame_demo`
and `arengine_xr_demo` runs (200 and 1000 frames respectively) both now
complete and exit cleanly (code 0, all frames attempted/synced, full
diagnostics printed) but show new Vulkan validation errors during
teardown - objects named `BlankEyeBuffer` and associated command
buffers/fences reported as leaked or freed-while-in-use at
`vkDestroyDevice`, which were not present in M9D's original "zero
validation errors" baseline. Functionally nothing regressed (frame
counts, session-state sequence, and exit codes are all unchanged), but
this is new validation-layer noise. A shared root cause with the Meta
crash (the runtime's own compositor engaging real timeline-semaphore
sync now that the feature is genuinely enabled) is plausible but
**unproven** - not investigated further in M11.2, per its own scope, and
subject to the same ownership caveat as Problem B above.

### Validation Results (M11.2)

Pure-logic tests: 5 new checks in `tests/openxr_vulkan_tests.cpp`
covering `SelectTimelineSemaphoreSupport` (Vulkan >= 1.2 core case;
pre-1.2 with extension available; unsupported by the physical device;
pre-1.2 without the extension available; no runtime-name dependency) -
zero API calls, zero GPU/runtime required. All four
`ARENGINE_ENABLE_OPENXR`×`ARENGINE_ENABLE_VULKAN` combinations configure,
build `/W4 /WX` clean, and pass 100% of their CTest suites. Manual
validation against both Meta XR Simulator and SteamVR as described
above.

- Files changed: `engine/xr/src/openxr/OpenXRVulkanRequirements.hpp`
  (new `TimelineSemaphoreSelection`/`SelectTimelineSemaphoreSupport`);
  `engine/xr/src/openxr/OpenXRVulkanGraphicsBinding.cpp` (capability
  query + conditional `VkPhysicalDeviceFeatures2`/pNext device-creation
  path); `tests/openxr_vulkan_tests.cpp` (5 new pure-logic tests). No
  other `engine/` or `tests/` files changed; `engine/rendering/` (desktop
  Vulkan) untouched.

## M11.3 - Teardown Investigation (blocked partway through by environment)

### Goal and scope

Investigate the Meta clean-exit crash left open by M11.2, without
guessing: document the real ownership graph, instrument teardown with
crash-survivable logging, classify every leaked/racing handle by actual
creator (not by inference from validation-message text alone), and only
then classify root cause as AREngine/runtime/interop/unknown. No
teardown-order changes, no workarounds, no runtime-name branching.

### Step 1: Ownership graph (`tests/xr_demo.cpp`, verified against each
wrapper class's actual constructor/destructor source, not milestone
prose)

Declaration order in `main()` (destruction is exact reverse, per C++'s
local-variable rule):

| Object | Creates | Destroys via | Depends on |
|---|---|---|---|
| `instance` (`OpenXRInstance`) | `XrInstance` | `xrDestroyInstance` | nothing |
| `binding` (`OpenXRVulkanGraphicsBinding`) | `VkInstance`, `VkPhysicalDevice` (borrowed from driver), `VkDevice`, `VkQueue` (borrowed from device) | debug messenger, then `vkDestroyDevice`, then `vkDestroyInstance` | `instance` |
| `session` (`OpenXRSession`) | `XrSession` | `xrDestroySession` | `instance`, `binding` |
| `localSpace` (`OpenXRReferenceSpace`) | `XrSpace` | `xrDestroySpace` | `instance`, `session` |
| `actionSystem` (`OpenXRActionSystem`) | `XrActionSet`, 4x `XrAction`, 2x `XrSpace` (action spaces) | action spaces (`xrDestroySpace`) → actions (`xrDestroyAction`) → action set (`xrDestroyActionSet`), in class-member reverse order | `instance`, `session` |
| `swapchains` (N x `OpenXRSwapchain`) | N x `XrSwapchain`; AREngine-owned `VkImageView`s over runtime-owned `VkImage`s | per swapchain: AREngine's `VkImageView`s (`vkDestroyImageView`) first, then `xrDestroySwapchain` | `instance`, `session`, `binding.device` |
| `projectionLayer` (`OpenXRProjectionLayer`) | nothing owned (borrows swapchain handles) | trivial | `swapchains`, `localSpace` |
| `renderPass` (`VulkanRenderPass`) | `VkRenderPass` | `vkDestroyRenderPass` | `binding.device` |
| `descriptorSetLayout` (`VulkanDescriptorSetLayout`) | `VkDescriptorSetLayout` | `vkDestroyDescriptorSetLayout` | `binding.device` |
| `pipeline` (`VulkanGraphicsPipeline`) | `VkPipeline`, `VkPipelineLayout` | `vkDestroyPipeline`, `vkDestroyPipelineLayout` | `binding.device`, `renderPass`, `descriptorSetLayout` |
| `commandPool` (`VulkanCommandPool`) | `VkCommandPool` (implicitly owns `commandBuffer`) | `vkDestroyCommandPool` | `binding.device` |
| `cubeMesh`/`floorMesh` (`VulkanMesh`) | 2x `VulkanBuffer` each (vertex+index → `VkBuffer`+`VkDeviceMemory`) | buffer before memory, per `VulkanBuffer`'s own destructor | `binding.device` |
| `texture` (`VulkanImage`) | `VkImageView`, `VkImage`, `VkDeviceMemory` | view → image → memory | `binding.device` |
| `sampler` (`VulkanSampler`) | `VkSampler` | `vkDestroySampler` | `binding.device` |
| `descriptorPool` (`VulkanDescriptorPool`) | `VkDescriptorPool` (implicitly frees `descriptorSet`) | `vkDestroyDescriptorPool` | `binding.device` |
| `viewTargets` (N x `OpenXRVulkanViewTarget`) | per view: depth `VulkanImage`, `VulkanFramebuffers` | framebuffers, then depth image (class-member reverse order) | `binding.device`, `renderPass`, `swapchains[i]` |
| `sceneObjects` | nothing (non-owning mesh pointers) | trivial | nothing |
| `commandBuffer` (raw `VkCommandBuffer`) | nothing separately | freed implicitly by `commandPool`'s own destruction | `commandPool` |
| `renderFence` (raw `VkFence`) | `VkFence` | explicit `vkDestroyFence`, called manually BEFORE the RAII chain even starts (see below) | `binding.device` |
| `frameDriver` (`XRFrameDriver`) | nothing owned (borrows `session`/`localSpace`) | trivial | `session`, `localSpace` |

Explicit (non-RAII) shutdown sequence, in the code, before any automatic
destructor runs: `vkDeviceWaitIdle(binding.device)` → `vkDestroyFence(renderFence)`
→ `AR_LOG_INFO("...shutting down")` → `return 0` → automatic destructor
chain begins (`frameDriver` → ... → `instance`, in the exact reverse
order of the table above).

**Cross-checked against real OpenXR object-hierarchy rules (Step 6),
not old milestone prose**: `XrSwapchain`/`XrSpace` (session children)
destroyed before `XrSession` - correct. `XrActionSet`/`XrAction` are
actually `XrInstance` children, not `XrSession` children, per spec - so
destroying them before `session` (as AREngine does) is conservative,
not required, and not a violation either way. `XrSession` destroyed
before the shared `VkDevice` (via `binding`'s destructor) - correct,
matches `XR_KHR_vulkan_enable2`'s implied lifetime (the runtime's
compositor may still reference the device through the session).
`VkDevice` destroyed before `VkInstance`, `XrInstance` destroyed last of
all - correct. **No destruction-order violation found anywhere in this
graph.**

`openxr_session_demo.cpp` and `openxr_frame_demo.cpp` have their own,
much smaller graphs (`instance`→`binding`→`session`→reference
spaces/swapchains, no rendering resources beyond frame_demo's own single
`VkCommandPool`+`VkFence`) - see the source for the exact tables; not
repeated here.

### Step 3: `vkDeviceWaitIdle` usage - a real inconsistency found

- `arengine_xr_demo` (`tests/xr_demo.cpp`): calls `vkDeviceWaitIdle`
  immediately before its own explicit `vkDestroyFence`, with a comment
  already noting the runtime's compositor may have independent in-flight
  work on the shared device.
- `arengine_openxr_frame_demo` (`tests/openxr_frame_demo.cpp`): creates
  its own `VkCommandPool`+`VkFence` but **never calls `vkDeviceWaitIdle`**
  before destroying them.
- `arengine_openxr_session_demo`: creates zero Vulkan resources of its
  own (see Step 5), so there is nothing of AREngine's own to wait on -
  not calling it here is not itself a bug, but it also provides no
  guarantee about the runtime's own independent work.

This inconsistency is real and now documented, but **not acted on** -
`xr_demo.cpp` already calls `vkDeviceWaitIdle` and still crashes, so
adding it to `frame_demo.cpp` on a guess would not be evidence-driven per
M11.3's own explicit instruction. Left as a candidate fix pending live
confirmation (blocked - see below).

### Steps 4/5: Handle ownership classification (from source, cross-checked
against the M11.2 crash-log evidence already on record)

Definitive, source-grounded findings - not inferred from validation
message text alone:

- **`grep` for `vkCreateSemaphore` across the entire `engine/` tree and
  every `tests/*.cpp` XR demo returns zero matches.** AREngine creates
  no `VkSemaphore` anywhere in this codebase. Therefore every
  `VkSemaphore` reported "leaked" in every M11.2 crash log (Meta and
  SteamVR alike) is **conclusively not AREngine's** - including the one
  most directly tied to Problem A/the timeline semaphore itself.
- **`grep` for `vkCreatePipelineCache`/`VkPipelineCache` across the same
  scope also returns zero matches.** The `VkPipelineCache` reported
  leaked in M11.2's Meta session-demo run is therefore also conclusively
  not AREngine's.
- **`openxr_session_demo.cpp` makes zero `vkCreate*` calls of any kind**
  (confirmed by `grep -c "vkCreate" tests/openxr_session_demo.cpp` → 0).
  The `VkCommandPool` residual reported leaked in every M11.1/M11.2 run
  of this demo is therefore **conclusively not AREngine's** - this
  directly answers M11.3's own flagged question about that specific
  residual.
- **`openxr_frame_demo.cpp` allocates exactly one `VkCommandBuffer` and
  creates exactly one `VkFence`, zero `VkSemaphore`s** (confirmed by
  `grep -n "vkAllocateCommandBuffers\|vkCreateFence\|vkCreateSemaphore"`).
  M11.2's SteamVR frame-demo crash log showed **three** distinct
  `VkCommandBuffer` handles and **four** distinct `VkFence` handles
  leaked - both counts exceed what this demo could possibly have created
  itself, so the excess (at minimum 2 command buffers, 3 fences) is
  conclusively runtime-owned.
- **Swapchain `VkImage`s**: every M11.2 crash log's leaked `VkImage`
  entries carry the runtime's own debug name, e.g.
  `[XRSim][Client][1680x1760] Image (Swapchain Image 2/3)` - matching
  `OpenXRSwapchain.hpp`'s own documented, source-verified ownership
  split ("Does NOT own the VkImages returned by
  xrEnumerateSwapchainImages - these belong to the OpenXR runtime's
  compositor"). AREngine never calls `vkCreateImage` for these; they
  come from `xrEnumerateSwapchainImages` and are the exact resource
  `OpenXRSwapchain`'s own class comment already documents as
  runtime-owned. Conclusively not AREngine's.
- **`VkDeviceMemory` entries** in the same crash logs are most plausibly
  the backing memory for those same runtime-owned swapchain images (or
  other runtime-internal allocations, e.g. SteamVR's own `BlankEyeBuffer`)
  - AREngine never allocates memory for a swapchain image (same
  ownership-split reasoning). Not conclusively provable purely from
  `grep` the way the object-count arguments above are, but consistent
  with every other finding.

**Net result: every object type observed as leaked or racing in every
M11.1/M11.2 crash log, across both runtimes and all three demos, is
either something AREngine's source never creates at all, or exceeds the
count AREngine could have created - none of the leaked/racing handles
gathered so far are provably AREngine's own.** This is strong,
source-grounded evidence pointing away from an AREngine ownership/lifetime
bug and toward the runtime side - but it does not yet identify the exact
crashing call/thread, which requires the live re-instrumented runs below
that the environment currently blocks.

### Step 2: Teardown instrumentation (implemented, not yet exercised live)

Added to `tests/xr_demo.cpp`, `tests/openxr_frame_demo.cpp`, and
`tests/openxr_session_demo.cpp`: a small `TeardownMarker` RAII sentinel
(diagnostic-only, temporary) placed immediately after each real object's
declaration - since C++ destroys local objects in exact reverse
declaration order, each sentinel's own destructor fires immediately
before the real object it follows is destroyed, giving a precise
"reached this point in teardown" trace with zero changes to any engine
class's own destructor. Writes to `std::cerr` (unit-buffered per the
C++ standard, plus an explicit `.flush()`) rather than `AR_LOG_INFO`'s
`std::cout`, specifically because `std::cout` redirected to a file is
typically fully block-buffered and can silently lose already-produced
output when a process is killed by an unhandled exception - exactly the
ambiguity that made M11.2's crash evidence inconclusive about whether
AREngine's own destructors ever started running. Also added: `VK_EXT_debug_utils`
object naming (`NameVulkanObject`, no-ops safely if unavailable) on
AREngine's own `commandPool`/`renderFence` in `xr_demo.cpp` and
`frame_demo.cpp`, so future validation output can name-match these
specific objects directly rather than relying on count-based inference.

### Blocker: Windows Smart App Control (Steps 8-11 not run)

While preparing to re-run the instrumented demos against Meta XR
Simulator, **every executable in the build directory - including
binaries built and successfully run earlier in this same session -
became blocked from launching**, with PowerShell reporting "An
Application Control policy has blocked this file." Read-only inspection
of the `Microsoft-Windows-CodeIntegrity/Operational` event log confirmed
the exact cause: Windows **Smart App Control** (`VerifiedAndReputablePolicyState: 1`,
i.e. enforcing) began blocking unsigned AREngine build binaries at
13:29:48 on the day of this investigation, logging "Code Integrity
determined that a process (bash.exe) attempted to load
arengine_openxr_demo.exe that did not meet the Enterprise signing level
requirements or violated code integrity policy." This is a machine-wide
policy change, not something caused by M11.3's own code changes (it
blocks binaries that ran successfully hours earlier in the same session)
and not something this investigation attempts to work around - modifying
security policy is out of scope. Per Microsoft's own documentation, Smart
App Control cannot be turned back off once switched from "Evaluation" to
"On" without a clean Windows reinstall, so this is a real, potentially
long-lived environment constraint, not a transient glitch.

**As a result, Steps 8 (repeat Meta test with instrumentation), 9**
**(Khronos hello_xr isolation test), 10 (crash-reproduction matrix across**
**demos), and 11 (timeline-feature A/B diagnostic) could not be**
**performed this milestone.** The instrumentation and `VK_EXT_debug_utils`
naming described above are implemented and built successfully, but have
not yet produced live evidence.

### Step 7: Session exit sequence (verified by code structure)

`tests/xr_demo.cpp`'s frame loop calls `PrepareFrame()` at the top of
every iteration and checks for `FrameStatus::Stop` **before** calling
`BeginFrame()` or doing any rendering that tick - so a `Stop` status can
only ever be observed at the start of a fresh iteration, never partway
through an acquire/render/release sequence. Every tick that renders
acquires, waits, and releases **every** swapchain image within that same
tick, before `EndFrame()` runs. By construction, this means no
`XrSwapchain` image can be left acquired when the loop exits - proven by
code structure, not yet independently confirmed by a live instrumented
run (blocked, see above). `openxr_frame_demo.cpp` follows the identical
acquire-render-release-within-one-tick pattern.

### Step 12: Root cause classification (preliminary - not final)

Per M11.3's own explicit instruction not to assign blame early: this is
**not** a final classification. Available evidence:

- No destruction-order violation found in AREngine's own code (Step 6).
- Every leaked/racing handle observed so far is either something
  AREngine's source never creates, or exceeds the count AREngine could
  have created (Steps 4/5) - none are provably AREngine's.
- A real inconsistency exists (`frame_demo.cpp` never calls
  `vkDeviceWaitIdle`), but `xr_demo.cpp` already calls it and still
  crashes, so this alone cannot be the root cause.
- The exact crashing call and thread remain unknown - this requires the
  live re-instrumented runs and, ideally, the official Khronos `hello_xr`
  isolation test that Steps 8-9 would have provided.

**Best-supported classification given available evidence: leans toward
"runtime bug or interop issue," not "AREngine bug"** - but this remains
unconfirmed without the live evidence Steps 8-11 would have produced,
and must not be treated as a final determination.
