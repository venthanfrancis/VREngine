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

**M0 through M7 complete; M8A, M8B, and M8C (Vulkan bring-up + presentation + first triangle) complete.** `Core`
(math, logging, assertions, `Event`, `KeyCode`/`MouseButton`), `Frame`
(`FrameTiming`/`ViewInfo`/`FrameDriver`), `Platform` (`Window` +
Windows/Win32 backend + `SteadyClock` + keyboard/mouse/focus events),
`Runtime` (owns `Window` + `FrameDriver` + `RenderDevice` +
`InputSystem`, runs the main loop) with `DesktopFrameDriver`,
`Rendering`'s minimal RHI with its `NullRenderDevice` backend, `Scene`,
`Assets`, and `Input` are all implemented as described in
`docs/ARCHITECTURE.md` Sections 9–15. `AREngineSandbox.exe` still runs
entirely on `NullRenderDevice` — nothing about M8A changed its behavior,
confirmed by an unchanged manual run.

**M8A adds real Vulkan** (`engine/rendering/src/vulkan/`, private, not
exposed through any public `Rendering` header): `VulkanInstance`
(instance + debug-build validation via `VK_EXT_debug_utils`),
`SelectPhysicalDevice` (enumerates, ranks discrete > integrated >
other, requires a graphics queue family and API ≥ 1.2), and
`VulkanDevice` (logical device + graphics queue). Targets Vulkan API
1.2. Exercised only by `tests/vulkan_demo.cpp` (manual, not part of
`ctest` — requires a real Vulkan-capable GPU) and unit-tested where the
logic is GPU-independent (`tests/vulkan_tests.cpp` — device ranking,
queue-family selection, version decoding, all pure logic with zero real
Vulkan calls). No `VulkanRenderDevice` exists yet — see
`docs/ARCHITECTURE.md` Section 16 for why that was deliberately not
built this milestone. No surface, no swapchain, no shaders, no
triangle — bring-up only.

**M8B adds real presentation** on top of M8A: `Window` gained
`GetNativeHandle()` (a deliberately narrow `NativeWindowHandle` escape
hatch — see `docs/ARCHITECTURE.md` Section 17), and Rendering's Vulkan
backend gained `VulkanSurface`, `VulkanSwapchain`,
`VulkanSwapchainSupport` (format/present-mode/extent/image-count
policy), `VulkanQueueFamilies` (graphics vs. present, possibly
different), `VulkanCommandPool`, and `VulkanImageBarrier`. The manual
`tests/vulkan_present_demo.cpp` opens a real AREngine window, creates a
swapchain, and clears each acquired image to a visible color every
frame until the window closes — handling resize and minimize along the
way. Still **no shaders, no pipeline, no triangle** — that's M8C. Three
real validation-layer errors were found and fixed during M8B
(image-view usage flags, per-image vs. per-frame semaphore indexing,
and a minimize-transition race) — see `docs/ARCHITECTURE.md` Section 17
for details; the final run has zero validation errors/warnings.

**M8C adds AREngine's first triangle** on top of M8B: Rendering's
Vulkan backend gained `VulkanShaderModule` (loads a compiled SPIR-V
`.spv` into a `VkShaderModule`), `VulkanRenderPass` (one color
attachment, cleared on load, presentable on completion — no depth, no
MSAA), `VulkanFramebuffers` (one per swapchain image view,
recreated alongside the swapchain), and `VulkanGraphicsPipeline` (empty
pipeline layout, dynamic viewport/scissor, no vertex input state). Two
GLSL shaders (`engine/rendering/src/vulkan/shaders/triangle.vert/.frag`)
are compiled to SPIR-V by CMake via `glslc` automatically as part of
the build — nothing manual, no hard-coded SDK path. The vertex shader
generates its 3 positions from `gl_VertexIndex`; **there is still no
vertex buffer** — that's M8D. `VulkanImageBarrier`, M8B's manual
clear-image machinery, was deleted (the render pass now performs the
same clear/layout-transition work). `tests/vulkan_present_demo.cpp` now
draws one RGB-interpolated triangle over the same teal background every
frame, still surviving resize/minimize exactly as in M8B. Zero
validation errors on the final run — see `docs/ARCHITECTURE.md` Section
18 for full details, including why a traditional `VkRenderPass` was
chosen over Vulkan 1.3 dynamic rendering (AREngine targets 1.2).

There is still no vertex/index buffer, no mesh abstraction, no texture,
no descriptor sets, no uniform buffers, no camera, no depth buffer, no
frame limiting, no ECS/component system, no real mesh/texture/image
format parsing, and no analog/XR/controller input — see Sections 11–15.
Everything else (`XR`, `Editor`) is still an M0-style stub with no
functionality. Next up is M8D+ (vertex/index buffers, then a textured
mesh). See `docs/ROADMAP.md` for the full plan.

## Hard rules — do not violate without the project owner's explicit approval

1. **No implementation ahead of the current milestone.** Check
   `docs/ROADMAP.md` before adding functionality to a module.
2. **No third-party dependencies** until a milestone explicitly calls for
   one.
3. **Vulkan bring-up + presentation + first triangle only (M8A/M8B/
   M8C)**: no vertex/index buffers, no mesh, no texture, no camera yet
   — see `docs/ROADMAP.md`'s M8D+ row for what's still pending within
   M8. **No OpenXR code** before milestone M9.
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
