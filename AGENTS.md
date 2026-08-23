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

**M0 through M7 complete; M8A through M8H (Vulkan bring-up + presentation + first triangle + vertex/index buffers + textures + genuine 3D/depth + a movable camera + a reusable mesh representation) complete.** `Core`
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

**M8D adds real GPU geometry** on top of M8C: Rendering's Vulkan backend
gained `VulkanBuffer` (owns one `VkBuffer`+`VkDeviceMemory`, no VMA),
`VulkanMemory::FindMemoryType` (pure-logic memory-type selection,
unit-tested), `VulkanOneTimeCommands` (allocate/begin/submit/wait/free
helper for one-shot command buffers), and `VulkanVertex::Vertex`
(`Vec2` position + `Vec3` color, with binding/attribute descriptions).
`triangle.vert` now reads real per-vertex data (`layout(location = 0/1)
in ...`) instead of generating positions from `gl_VertexIndex`.
Uploads go CPU → `HOST_VISIBLE|HOST_COHERENT` staging buffer →
`vkCmdCopyBuffer` → `DEVICE_LOCAL` destination buffer
(`CreateDeviceLocalBuffer`), synchronously (`vkQueueWaitIdle` after the
copy — documented as a deliberate simplification, not built to scale to
per-frame uploads). `tests/vulkan_present_demo.cpp` now renders a
colored quad (4 vertices, 6 indices, `VK_INDEX_TYPE_UINT32`) via
`vkCmdBindVertexBuffers`/`vkCmdBindIndexBuffer`/`vkCmdDrawIndexed` —
neither buffer is swapchain-dependent, so `recreateSwapchain()` never
touches them. M4's `BufferDesc`/`BufferHandle`/`RenderDevice::CreateBuffer`
were reviewed against this evidence (a real gap was found: no way to
supply initial data) but deliberately left unchanged — M8D's demo
bypasses the generic API entirely, so one non-generic Vulkan buffer
isn't enough evidence to pick the right generic upload shape yet. Zero
validation errors on the final run — see `docs/ARCHITECTURE.md`
Section 19 for full details.

**M8E adds real texturing** on top of M8D: Rendering's Vulkan backend
gained `VulkanImage` (owns `VkImage`+`VkDeviceMemory`+`VkImageView`),
`VulkanSampler` (kept separate from `VulkanImage` — a sampler is
commonly shared across textures), `VulkanImageLayoutTransition`
(a narrow helper for exactly the two transitions a texture upload
needs — `UNDEFINED`→`TRANSFER_DST_OPTIMAL`→`SHADER_READ_ONLY_OPTIMAL`;
not a revival of M8B's deleted generic `VulkanImageBarrier`),
`VulkanDescriptorSetLayout`/`VulkanDescriptorPool` (one combined-image-
sampler binding, one pool, one set — no descriptor recycling/caching),
and `VulkanCheckerboard::GenerateCheckerboardRGBA8` (a pure-logic,
unit-tested procedural test texture — no PNG/JPEG/stb_image). `Vertex`
gained a `uv` field (location 2). `triangle.vert`/`.frag` now pass UVs
through and sample `layout(set = 0, binding = 0) uniform sampler2D
uTexture`, multiplying the sample by the M8D vertex-color gradient.
Texture upload reuses M8D's staging-buffer pattern
(`CreateTextureFromPixels`): CPU pixels → `HOST_VISIBLE|HOST_COHERENT`
staging buffer → `vkCmdCopyBufferToImage` → `DEVICE_LOCAL` image,
synchronously. `tests/vulkan_present_demo.cpp` now renders a 64×64
checkerboard-textured quad — none of the texture/sampler/descriptor
resources are swapchain-dependent, so `recreateSwapchain()` never
touches them. M4's `TextureDesc`/`TextureHandle` were reviewed against
this evidence (the same initial-data gap `BufferDesc` had, now proven
twice, plus a newly found format gap — `TextureFormat` can't express
sRGB vs. linear encoding) but deliberately left unchanged, for the same
reason as M8D's `BufferDesc` review. Zero validation errors on the
final run — see `docs/ARCHITECTURE.md` Section 20 for full details.

**M8F adds genuine 3D** on top of M8E: `Vertex::position` changed from
`Vec2` to `Vec3`. `Core::Math` gained `LookAtRH` (a general-purpose,
non-Vulkan-specific right-handed view matrix) and `PerspectiveVulkanRH`
(Vulkan's `[0,1]`-depth, Y-flipped clip-space convention, baked into
one matrix as AREngine's single explicit place for that correction —
see `docs/ARCHITECTURE.md` Section 21, "Vulkan Projection Convention
(M8F)"), plus `Mat4::operator*(Vec4)` for full 4-component transforms.
Rendering's Vulkan backend gained `VulkanDepthFormat::FindSupportedDepthFormat`
(prefers `D32_SFLOAT` → `D32_SFLOAT_S8_UINT` → `D24_UNORM_S8_UINT`,
pure-logic-tested selection), a depth image (reusing `VulkanImage` with
a new `aspectMask` parameter rather than a near-duplicate class),
`VulkanRenderPass`/`VulkanFramebuffers` extended with a depth
attachment, depth testing enabled in the pipeline
(`depthCompareOp = LESS`, clear depth 1.0), and `VulkanPushConstants::MvpPushConstants`
(one `Mat4` MVP + one `Vec4` tint, 80 bytes — the deliberately temporary
transform-upload mechanism for M8F's two fixed objects). One fixed
camera at `(0,0,3)` looking at the origin — no movable camera, no
`Input` integration, no `Camera` component. `tests/vulkan_present_demo.cpp`
now draws two overlapping quads at different depths (offset diagonally
so each has both a shared and an independent screen region), the nearer
one submitted first, proving the depth buffer — not draw order —
decides visibility. **The depth image IS swapchain/extent-dependent
(unlike a texture)** and is recreated alongside the swapchain on
resize; `recreateSwapchain()` was extended accordingly. Zero validation
errors on the final run — see `docs/ARCHITECTURE.md` Section 21 for
full details, including the exact visual proof and why an initial
same-XY, Z-only version of the two quads was revised (the far quad was
fully hidden behind the near one, making the proof visually
ambiguous).

**A Core/Vulkan clip-space cleanup landed right after M8F, before M8G**:
M8F's original `Core::Math::PerspectiveVulkanRH` baked Vulkan's NDC
Y-flip into a Core function — a graphics-backend concept leaking into
`Core`, which must stay backend-independent. Fixed by splitting it:
`Core::Math::PerspectiveRH_ZO` (renamed, `Camera.hpp` → `ViewProjection.hpp`
— Core doesn't own a `Camera` system) now builds only the backend-
neutral right-handed/zero-to-one-depth matrix, with no Y-flip; the flip
itself moved to a new, small, unit-tested Vulkan-private function,
`AREngine::Rendering::Vulkan::ApplyVulkanYFlip`
(`engine/rendering/src/vulkan/VulkanClipSpace.hpp/.cpp`), which the demo
now calls explicitly. The resulting matrix — and the demo's visible
output, including the M8F depth-testing proof — are unchanged; only the
internal organization moved. See `docs/ARCHITECTURE.md` Section 21,
"Core/Vulkan Clip-Space Split", for the full reasoning.

**M8G adds AREngine's first `Camera` abstraction** on top of M8F:
`Scene::Camera` (`engine/scene/include/AREngine/Scene/Camera.hpp`) —
FOV/near/far/aspect only, no position/orientation of its own (reuses
`Scene::Transform` for that), no Vulkan types anywhere, and no
`VulkanCamera`. `GetViewMatrix(transform)` reuses M8F's `LookAtRH` via
a synthetic look-at target; `GetProjectionMatrix()` returns the
*unflipped* `PerspectiveRH_ZO`, with `ApplyVulkanYFlip` applied only in
the demo layer. `Core::Math::Quaternion` gained `operator*` (Hamilton
product) and `Rotate(Quaternion, Vec3)` — both genuinely needed now,
previously deferred since M5. `Transform` gained
`GetForward()`/`GetRight()`/`GetUp()`. A small, demo-private
`ARDemo::DemoCameraController` (`tests/DemoCameraController.hpp`, zero
Vulkan dependency) drives WASD movement (following the camera's full
current orientation, not always world `-Z`), Space/Ctrl world-space up/
down, and click-drag (right-mouse-held) mouse look — chosen over cursor
capture since `Platform` has no raw-input support yet, and expanding it
wasn't judged necessary. Movement is delta-time-based
(`Platform::SteadyClock`, the smallest temporary integration since the
demo isn't built on `Runtime`). Input reuses `InputSystem` exactly as
`Runtime.cpp` does — no direct Win32 queries, and M7's focus-loss
key-clear behavior was explicitly re-verified to still prevent stuck
movement keys. `tests/vulkan_present_demo.cpp` now renders a small
hard-coded scene (a floor + four upright quads) navigable in real time,
still depth-tested per M8F. Zero validation errors on the final run —
see `docs/ARCHITECTURE.md` Section 22 for full details, including the
exact yaw/pitch sign convention and why a diagonal floor edge seen
during mouse-look testing is correct perspective, not a bug.

**M8H adds AREngine's first reusable mesh representation** on top of
M8G: `Rendering::MeshData`/`MeshVertex`
(`engine/rendering/include/AREngine/Rendering/MeshData.hpp`) — a
backend-independent CPU geometry description (no Vulkan types, no
Scene dependency, no `AssetId`) — plus `Rendering::CreateQuadMesh`/
`CreateCubeMesh` (`ProceduralMesh.hpp`, unconditionally built, no
Vulkan dependency) for in-memory test geometry. Vulkan's old private
`Vertex` struct was retired in favor of operating directly on
`Rendering::MeshVertex` (`VulkanVertex.hpp/.cpp` now export free
functions, `GetVertexBindingDescription`/`GetVertexAttributeDescriptions`,
rather than a duplicate struct) — the same three fields being needed
twice independently was the evidence that justified merging them.
Rendering's Vulkan backend gained `Vulkan::VulkanMesh`
(`src/vulkan/VulkanMesh.hpp/.cpp`): owns one vertex + one index
`VulkanBuffer` pair plus an index count, uploaded once via
`CreateVulkanMesh` (mirroring `CreateDeviceLocalBuffer`/
`CreateTextureFromPixels`'s existing factory shape), with `Bind()`/
`Draw()` methods so one mesh can be bound once per frame and drawn many
times with different push-constant Model transforms. `tests/vulkan_present_demo.cpp`
now uploads exactly one `CreateCubeMesh()` (a 1x1x1m cube, 24 vertices/
36 indices — 4 per face, not shared, so each face's UVs map
independently) and draws it 6 times (a flattened floor instance plus 5
upright cube instances) — replacing the old hard-coded shared quad.
Back-face culling was enabled (`VK_CULL_MODE_BACK_BIT`, `frontFace`
unchanged at `VK_FRONT_FACE_CLOCKWISE`) after deriving, not guessing,
that every `ProceduralMesh` face's construction-time winding already
matches that front-face convention once the existing Vulkan Y-flip is
applied. The generic `RenderDevice` API was reviewed again with this
milestone's real evidence and deliberately left unchanged — mesh
upload stays a `Rendering`-private, Vulkan-only concern layered on the
existing private buffer/upload path, not yet a `RenderDevice` concept;
see `docs/ARCHITECTURE.md` Section 23, "Generic RenderDevice Review
(M8H)" for the full reasoning. Zero validation errors on the final
run, and the log confirms the mesh is uploaded exactly once and never
rebuilt across a resize/minimize/restore cycle — see
`docs/ARCHITECTURE.md` Section 23 for full details, including a noted
gap: this session's manual validation could not capture on-screen
visual confirmation (another application was in exclusive fullscreen
on the development machine throughout), so the CPU/GPU counts and
zero-validation-error result are log-confirmed, but a visual pass is
still recommended before treating M8H as fully closed out.

There is still no real image loading (PNG/JPEG/stb_image), no uniform
buffers, no `SceneRenderer`/Scene integration, no `MeshAsset`/glTF/OBJ
loading, no normals/lighting, no frame limiting, no ECS/component
system, and no analog/XR/controller input — see Sections 11–15.
Everything else (`XR`, `Editor`) is still an M0-style stub with no
functionality. Next up is M8I+ (Scene integration). See
`docs/ROADMAP.md` for the full plan.

## Hard rules — do not violate without the project owner's explicit approval

1. **No implementation ahead of the current milestone.** Check
   `docs/ROADMAP.md` before adding functionality to a module.
2. **No third-party dependencies** until a milestone explicitly calls for
   one.
3. **Vulkan bring-up + presentation + first triangle + vertex/index
   buffers + textures + genuine 3D/depth + a movable camera + a
   reusable mesh representation only (M8A–M8H)**: no real image
   loading, no uniform buffers, no `SceneRenderer`/Scene integration,
   no `MeshAsset`/model loading yet — see `docs/ROADMAP.md`'s M8I+ row
   for what's still pending within M8. **No OpenXR code** before
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
