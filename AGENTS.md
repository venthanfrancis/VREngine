# AGENTS.md — AREngine

This file orients any AI coding agent (or human) working in this
repository.

## What this project is

AREngine is a custom, from-scratch C++20 game engine built specifically
for augmented reality. It is **not** built on Unity or Unreal. Long-term
it targets custom AR glasses hardware; near-term it targets Windows
desktop, later adding Vulkan and OpenXR.

Full context lives in `docs/`:
- `docs/ARCHITECTURE.md` — module layout, layering rules, key design
  decisions (FrameDriver abstraction, deferred RHI presentation, etc.)
- `docs/ROADMAP.md` — milestone plan and current status
- `docs/WORLD_CONVENTIONS.md` — units and coordinate system

## Current status

**M0 through M5 complete.** `Core` (math: Vec2/Vec3/Vec4/Quaternion
(incl. `FromAxisAngle`)/Mat4 (incl. `Translation`/`Scale`/`Rotation`/
`TRS`), logging, assertions, the `Event` base type), `Frame`
(`FrameTiming`/`ViewInfo`/`FrameDriver`), `Platform`'s `Window`
abstraction + Windows/Win32 backend + `SteadyClock`, `Runtime` (owns
`Window` + a `FrameDriver` + a `RenderDevice`, runs the main loop) with
`DesktopFrameDriver`, `Rendering`'s minimal RHI
(`RenderDevice`/`BufferHandle`/`TextureHandle`/`DrawCommand`) with its
`NullRenderDevice` backend, and `Scene` (`EntityId`/`Transform`/
parent-child hierarchy/`GetWorldMatrix`) are all implemented.
`AREngineSandbox.exe` opens a window, runs a real frame loop, submits
one hard-coded temporary dummy draw per frame through the Null backend
(nothing visibly renders — that's expected, see
`docs/ARCHITECTURE.md` Section 12), logs FPS once per second, and shuts
down cleanly on close. Raw keyboard/mouse input and file I/O were
deliberately NOT added in M2 — see Section 10. **`Scene` is not yet
wired into `Runtime`** — nothing consumes it yet (no `Scene`→`Rendering`
bridge exists), so it's tested entirely headlessly; see Section 13 for
why that's the correct M5 scope, not a gap. There is still no real
graphics backend, no frame limiting, no pipeline/shader API (deferred
until Vulkan, M8, per Section 12), and no ECS/component system (Section
13) — `DesktopFrameDriver` produces one placeholder identity `ViewInfo`
per frame and `SubmitFrame()` is a no-op — see Section 11. Everything
else (`Assets`, `Input`, `XR`, `Editor`) is still an M0-style stub with
no functionality. Next up is M6 (`Assets`). See `docs/ROADMAP.md` for
the full plan.

## Hard rules — do not violate without the project owner's explicit approval

1. **No implementation ahead of the current milestone.** Check
   `docs/ROADMAP.md` before adding functionality to a module.
2. **No third-party dependencies** until a milestone explicitly calls for
   one.
3. **No Vulkan code** before milestone M8. **No OpenXR code** before
   milestone M9.
4. **No custom container types.** Use `std::` containers directly unless
   there is a measured (profiled) reason to do otherwise.
5. **Respect the dependency layering** in `docs/ARCHITECTURE.md` — a
   module may only depend on modules below it. In particular:
   - `Core` depends on nothing.
   - `Frame` depends only on `Core` (this is where `FrameDriver`,
     `FrameTiming`, and `ViewInfo` live — not in `Core`).
   - `Rendering` (the RHI) is GPU operations only; it does not own frame
     lifecycle or presentation.
   - Nothing under `engine/` or `runtime/` may depend on `editor/` or
     `sandbox/`.
6. **World conventions are fixed**: 1 unit = 1 meter, right-handed,
   +Y up, -Z forward, +X right. See `docs/WORLD_CONVENTIONS.md`.
7. **C++20**, CMake-based build, Windows-first for now.

## Build

```
cmake -S . -B build
cmake --build build
```
