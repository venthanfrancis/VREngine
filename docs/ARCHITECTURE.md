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
