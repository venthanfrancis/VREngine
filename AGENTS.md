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

**M0 and M1 complete.** `Core` (math: Vec2/Vec3/Vec4/Quaternion/Mat4,
logging, assertions, the `Event` base type) and `Frame`
(`FrameTiming`/`ViewInfo`/`FrameDriver`) are implemented. Everything else
(`Platform`, `Rendering`, `Scene`, `Assets`, `Input`, `XR`, `Runtime`,
`Editor`, `Sandbox`) is still an M0-style stub with no functionality.
Next up is M2 (`Platform`, Windows). See `docs/ROADMAP.md` for the full
plan.

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
