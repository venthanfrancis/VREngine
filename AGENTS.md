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

**M0 through M7 complete; M8A through M8H (Vulkan bring-up + presentation + first triangle + vertex/index buffers + textures + genuine 3D/depth + a movable camera + a reusable mesh representation) complete; M9A (OpenXR bring-up) complete; M9B (runtime/simulator environment planning — no engine code changes) complete; M9C (OpenXR/Vulkan graphics binding bring-up) complete.** `Core`
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

**M9A introduces OpenXR bring-up** — the `XR` module's first real
content, gated behind a new top-level CMake option,
`ARENGINE_ENABLE_OPENXR` (default **OFF**, mirroring
`ARENGINE_ENABLE_VULKAN`'s pattern). When ON, `engine/xr/CMakeLists.txt`
first tries `find_package(OpenXR CONFIG QUIET)`, then falls back to
`FetchContent`-fetching the official Khronos `OpenXR-SDK` repository
(pinned to `release-1.1.62`, pre-generated headers/loader — no Python
needed, nothing vendored into this repo) if no system package is
found — there is no Vulkan-SDK-style pre-installed system package for
OpenXR on Windows. `engine/xr/src/openxr/` (private, never exposed
through `AREngine/XR/XR.hpp`) gained `OpenXRVersion` (targets OpenXR
1.0 core, the same "broad compatibility over the newest header"
reasoning M8A used for Vulkan 1.2), `OpenXRResult` (`CheckXrResult`/
`XrResultToReadableString`, mirroring `CheckVkResult`/`VkResultToString`),
`OpenXRInstance` (owns one `XrInstance`, zero layers/extensions
requested — but deliberately does **not** assert on
`xrCreateInstance` failure, unlike every prior bring-up wrapper: "no
OpenXR runtime installed" is a normal desktop-machine state, not a
bug), and `OpenXRSystem` (`TryGetHmdSystem` requests an
`XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY` system, also without asserting on
failure — "no HMD connected" is likewise normal). `engine/xr`'s public
link libraries were trimmed from `{Core, Platform, Frame}` down to
`{Core}` only — neither Platform nor Frame is genuinely needed yet
(no window, no `XRFrameDriver`), matching M9A's explicit minimal-
dependency requirement; both can return once a later milestone gives a
real reason. `tests/openxr_demo.cpp` (manual, gated behind
`ARENGINE_ENABLE_OPENXR`, not part of `ctest`) proves the whole
sequence end to end and explicitly distinguishes three outcomes rather
than one generic failure: no runtime installed, runtime-but-no-HMD, and
runtime-plus-HMD. On this development machine (no OpenXR runtime
installed), the first case was confirmed three times — default OpenXR
build, `/W4 /WX` build, and an `ARENGINE_ENABLE_OPENXR=ON` +
`ARENGINE_ENABLE_VULKAN=OFF` build (proving M9A is not coupled to
Vulkan) — all exiting cleanly with code 0, zero crashes. The other two
cases could not be exercised on this machine and were verified by
specification-level code review instead — an honestly-reported gap,
same as M8H's own visual-verification gap. See `docs/ARCHITECTURE.md`
Section 24 for full details, including the one real `/W4`-caught bug
(`XrApplicationInfo::applicationVersion`/`engineVersion` are plain
`uint32_t`, not `XrVersion` — an initial draft's use of
`XR_MAKE_VERSION` for them silently truncated a 64-bit value, fixed
before this milestone shipped).

**M9B is a planning/review milestone only — zero engine code changed.**
Triggered by a roadmap correction found right after M9A: a normal
(non-headless) `XrSession` requires a graphics binding at creation time
(`XrGraphicsBindingVulkanKHR`'s `VkInstance`/`VkPhysicalDevice`/
`VkDevice`/queue, negotiated via `xrGetVulkanGraphicsRequirementsKHR`
first), so `XrSession` cannot come before Vulkan/OpenXR graphics
integration exists — M9's sub-milestones were re-ordered accordingly
(M9C graphics binding now precedes M9D `XrSession`; see
`docs/ROADMAP.md` and `docs/ARCHITECTURE.md` Section 25, "Why Graphics
Binding Must Precede a Graphics XrSession," for the full reasoning).
M9B inspected this development machine directly: no `XR_RUNTIME_JSON`
override, and `HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` doesn't
exist at all (confirmed via the registry, not just inference) — no
OpenXR runtime has ever been registered here, explaining M9A's observed
`XR_ERROR_RUNTIME_UNAVAILABLE` precisely. The OpenXR loader's own
fetched source was inspected to confirm its exact Windows discovery
order: `XR_RUNTIME_JSON` env var first, then
`HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\<major>\ActiveRuntime`
(HKLM only — no `HKEY_CURRENT_USER` fallback for the runtime itself,
unlike API-layer discovery). **Recommendation (not installed yet)**:
SteamVR's OpenXR runtime, run with its built-in null/simulated-HMD
driver — Khronos-conformant, free, requires no physical headset,
and creates zero code-level coupling (AREngine's `engine/xr` requests
no vendor extensions and has no runtime-specific logic; whichever
runtime is "active" is purely an OS/registry-level choice). Meta XR
Simulator was considered as a secondary option; Windows Mixed Reality
was ruled out (platform retired on this Windows build). See
`docs/ARCHITECTURE.md` Section 25 for the full investigation and
reasoning. Installation awaits explicit approval — nothing was
installed this milestone.

**M9C integrates OpenXR with Vulkan** far enough to construct every
Vulkan object a future graphics `XrSession` will need, but stops short
of creating that session. `XR_KHR_vulkan_enable2` support is explicitly
verified (`OpenXRInstance::IsExtensionSupported`, new this milestone)
before being enabled on instance creation — if unsupported, the demo
reports that clearly and stops rather than falling back to the older
`XR_KHR_vulkan_enable`. A new private integration boundary,
`engine/xr/src/openxr/OpenXRVulkanRequirements.*`/`OpenXRVulkanGraphicsBinding.*`,
resolves the four required `XR_KHR_vulkan_enable2` functions via
`xrGetInstanceProcAddr` (never assumed linkable), queries the runtime's
Vulkan API version range and converts it correctly from `XrVersion`'s
16/16/32-bit packing to Vulkan's own 7/10/12-bit `VkVersion` encoding
(the two are not bit-reinterpretable — a real gotcha this milestone
documents carefully), creates an XR-compatible `VkInstance`/`VkDevice`
pair through OpenXR's own creation functions (`xrCreateVulkanInstanceKHR`/
`xrCreateVulkanDeviceKHR` — **not** the ordinary desktop
`vkCreateInstance`/`vkCreateDevice` path), asks OpenXR which
`VkPhysicalDevice` to use (authoritative, not `Rendering`'s own desktop
ranking algorithm), finds a graphics queue family (no `VkSurfaceKHR`/
presentation involved — there is no Windows surface anywhere in this
XR path), and assembles a `VulkanGraphicsBindingData` struct ready to
become a real `XrGraphicsBindingVulkan2KHR` in M9D. Vulkan validation
is preserved through the XR-created instance (confirmed on the real
run: `VK_LAYER_KHRONOS_validation` genuinely inserted, zero validation
errors/warnings). This integration code lives in `engine/xr`, not
`engine/rendering` — every call driving it is itself an OpenXR call,
and this keeps `Rendering` completely unaware OpenXR exists; `arengine_xr`
only links Vulkan when **both** `ARENGINE_ENABLE_OPENXR` and
`ARENGINE_ENABLE_VULKAN` are `ON`, verified not to create link errors
in any of the four build-option combinations. The M8 desktop Vulkan
objects are never reused or handed to OpenXR — a second, independent
Vulkan instance/device pair is created, since the spec requires
XR-bound Vulkan objects to be created *through* OpenXR's own functions
for compositor compatibility. Run against the live SteamVR/OpenXR
2.16.7 null-driver runtime (see Section 25): reported Vulkan range
1.0.0–1.2.0, selected 1.2.0 (AREngine's desktop preference, in range),
OpenXR-selected GPU matched the desktop's own (NVIDIA RTX 3060 Laptop),
zero validation errors, clean shutdown, no `XrSession` created. See
`docs/ARCHITECTURE.md` Section 26 for full details.

There is still no real image loading (PNG/JPEG/stb_image), no uniform
buffers, no `SceneRenderer`/Scene integration, no `MeshAsset`/glTF/OBJ
loading, no normals/lighting, no frame limiting, no ECS/component
system, and no `XrSession`/session-state handling/reference spaces/XR
swapchain/frame loop/head tracking/controllers — see Sections 11–15.
`Editor` is still an M0-style stub with no functionality. Next up is
M9D (`XrSession` + session-state handling + reference spaces) or M8I+
(Scene integration, still pending within M8) — see `docs/ROADMAP.md`
for the full plan.

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
   for what's still pending within M8.
3a. **OpenXR instance/system discovery + Vulkan graphics-binding
   bring-up only so far (M9A–M9C)**: no `XrSession`, no session-state
   handling, no reference spaces, no XR swapchain, no frame loop
   (`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`/`xrLocateViews`), no
   head/hand/eye tracking, no passthrough, no anchors, no
   `XRFrameDriver`. `XrSession` may now be implemented (M9C's graphics
   binding exists) — see `docs/ROADMAP.md`'s M9D row.
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
