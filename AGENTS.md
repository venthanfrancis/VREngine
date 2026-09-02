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

**M0 through M7 complete; M8A through M8H (Vulkan bring-up + presentation + first triangle + vertex/index buffers + textures + genuine 3D/depth + a movable camera + a reusable mesh representation) complete; M9A (OpenXR bring-up) complete; M9B (runtime/simulator environment planning — no engine code changes) complete; M9C (OpenXR/Vulkan graphics binding bring-up) complete; M9D (first real XrSession + session-state lifecycle) complete; M9E (XR swapchains + frame lifecycle) complete; M9E.5 (generic FrameDriver redesign from real OpenXR evidence) complete; M9F (real xrLocateViews view location + composition-layer submission) complete; M9G (first real 3D rendering into OpenXR — a cube, real per-view poses/projections, rendered into each eye's swapchain image) complete; M9H (XR render-path hardening — reusable per-view render-resource helper extracted, 1000-frame repeated-lifecycle validation, lightweight diagnostics, continuous-shouldRender environment limitation investigated honestly) complete; M10 (OpenXR action/input foundation — one XrActionSet, four actions spanning both hands, KHR simple_controller bindings, xrSyncActions, new generic Input::*ActionState vocabulary) complete; M10.5 (first integrated XR demo — render path + action/input path combined in one session/frame loop, multiple objects into multiple views, 1000-iteration validation) complete; M10.6 (input-driven interaction state — real OpenXR action state now visibly affects the integrated demo's scene via a small unit-tested pure interaction layer) complete; M11 (XR development test environment research — recommended Meta XR Simulator, no engine changes) complete; M11.1 (Meta XR Simulator installed and validated through Step 4 — real gains found: sustained shouldRender=true, FOCUSED reached, real stereo poses; a timeline-semaphore Vulkan validation error and a clean-exit crash also found) complete through Step 4, Steps 5–10 deferred; M11.2 (Vulkan timeline-semaphore device-feature negotiation) PARTIAL — the feature-negotiation fix itself is complete (capability-driven, no runtime-name branching, SteamVR's M9D conflict not reintroduced), but the Meta clean-exit crash from M11.1 is still open and unresolved, and new non-fatal SteamVR teardown validation noise was newly discovered — see `docs/ARCHITECTURE.md`, "M11.2 Result Summary: Three Separate Problems". M11.3A (Smart App Control diagnosis) COMPLETE — Smart App Control enforcement confirmed through Code Integrity events; the blocker was hash/execution history under enforcement, not file origin/ACLs/Mark-of-the-Web; a clean relink produced a new hash and restored execution; full CTest returned to 17/17 with Smart App Control left enabled throughout, no security settings changed — see `docs/ARCHITECTURE.md`, "M11.3A - Smart App Control Diagnosis". M11.3 (XR teardown/lifetime investigation) COMPLETE — AREngine's destruction order and Vulkan/OpenXR ownership were exhaustively audited (no AREngine-owned `VkDevice` child ever survives to `vkDestroyDevice`, no acquired `XrSwapchain` image remains at shutdown, `vkDeviceWaitIdle` succeeds in the integrated path); the crash reproduces inside/after `OpenXRVulkanGraphicsBinding`'s own teardown, around `vkDestroyDevice`, only under Meta XR Simulator (never SteamVR, running identical AREngine code); best-supported classification is a Meta runtime bug or Vulkan/OpenXR interop issue, not an AREngine defect; exact crash stack/module remains unknown (no debugger/minidump used); no AREngine-side fix is justified by current evidence — see `docs/ARCHITECTURE.md`, "M11.3 Resumed - Teardown Investigation Conclusion". M11.1 Steps 5–10 remain a judgment call, not a technical blocker: AREngine's own code is cleared, but any sufficiently long Meta session still ends in this crash. M11.1B (active head/controller validation) PARTIAL — Meta's actual reported interaction profile was discovered to depend on which profiles AREngine suggests (khr/simple_controller only, until a new `oculus/touch_controller` binding was added alongside it, never replacing it); with that binding in place, select/trigger/move/aim_pose all become genuinely active on both hands, generic `Input::*ActionState` conversion works against the real profile, and the M10.6 pose marker becomes visible from real runtime state; SteamVR regression clean, full build matrix and CTest green. Still open: driven select press/release, nonzero trigger/thumbstick values, controller pose movement, simulated head movement, and the resulting visible highlight/scale/move changes — blocked by an inability to reliably drive Meta XR Simulator's GUI input through the current automation environment, not a code defect — see `docs/ARCHITECTURE.md`, "M11.1B - Active Head + Controller Validation". M11.1 is not yet marked complete. M11.1C (Meta XR Operator setup) PARTIAL — Meta XR Operator v205.1 (standalone/native package, no Unity required) validated as an OpenXR API layer sitting above Meta XR Simulator: `arengine_openxr_demo`/`arengine_openxr_vulkan_demo` confirm AREngine still uses Meta XR Simulator normally with the layer enabled, Vulkan/`timelineSemaphore` config is byte-for-byte unchanged, zero new pre-teardown OpenXR/Vulkan errors, and no AREngine source dependency on Meta XR Operator was introduced (zero engine files changed). Pending: MCP agent connection, programmatic head movement, driven select press/release, nonzero trigger/thumbstick input, moving controller pose, and the resulting visible M10.6 interaction — blocked specifically because the `claude` CLI needed for `claude mcp add` isn't reachable from this session's own tool sandbox; registering the MCP proxy requires the user's normal terminal — see `docs/ARCHITECTURE.md`, "M11.1C - Meta XR Operator Input-Driving Validation". M11.1 is still not yet marked complete.** `Core`
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
1.0.0–1.2.0, OpenXR-selected GPU matched the desktop's own (NVIDIA RTX
3060 Laptop), zero validation errors, clean shutdown, no `XrSession`
created. See `docs/ARCHITECTURE.md` Section 26 for full details. (M9D's
real session creation briefly revealed an incompatibility at 1.2 that
mere instance/device creation couldn't have caught, and briefly capped
this device's selected version to 1.1 as a fix — a follow-up
compatibility review found the cap unnecessary once the underlying
fix was corrected; this device genuinely selects **1.2** today, same
as the desktop renderer. See below.)

**M9D creates AREngine's first real `XrSession`**, reusing the M9C
Vulkan graphics binding unchanged (no second Vulkan device):
`XrGraphicsBindingVulkan2KHR` built from `VulkanGraphicsBindingData`
and chained through `XrSessionCreateInfo::next` via the new
`OpenXRSession` class (`engine/xr/src/openxr/OpenXRSession.hpp/.cpp`,
Vulkan-gated, alongside M9C's files). `XrSessionState` is tracked
through a real `xrPollEvent` loop (`PollSessionEvents`, draining every
pending event each cycle) and three small `constexpr` decision
functions (`OpenXRSessionState.hpp` — Vulkan-independent, like
`OpenXRViewConfiguration.hpp`/`OpenXRReferenceSpace.hpp`, since session
state/view configuration/reference spaces are pure OpenXR concepts):
`ShouldBeginSession` (fires only on `READY`), `ShouldEndSession` (fires
only on `STOPPING` while already running — `sessionRunning` tracked
separately from `XrSessionState`, never inferred from it), and
`ShouldStopMainLoop` (`EXITING`/`LOSS_PENDING`). View configurations
were enumerated (`PRIMARY_STEREO` was the only, and thus selected,
type; 2 views, each 1852x2056 recommended/8192x8192 max), and all three
reference space types the runtime supports (`VIEW`, `LOCAL`, `STAGE`)
were created via `OpenXRReferenceSpace`. **The observed SteamVR/null
state sequence was `READY -> STOPPING -> EXITING`** —
`SYNCHRONIZED`/`VISIBLE`/`FOCUSED` were never reached, confirmed to be
because those require frame-loop participation (`xrWaitFrame`) that
M9D explicitly must not call; the demo now requests a clean exit
immediately once the session starts running rather than waiting for an
unreachable `FOCUSED`.

Real session creation against SteamVR surfaced a genuine Vulkan
device-feature requirement M9C's narrower testing (instance/device
creation only, no session) couldn't have caught: SteamVR's own
compositor shaders need `shaderOutputViewportIndex`/`shaderOutputLayer`
and `geometryShader`, and SteamVR's `xrCreateVulkanDeviceKHR`
unconditionally injects its own `VkPhysicalDeviceTimelineSemaphoreFeatures`
into the device's `pNext` chain in a way that conflicts with any
app-provided `VkPhysicalDeviceVulkan12Features`. **M9D's first fix
capped the XR device's selected Vulkan version to 1.1 whenever 1.2+
would otherwise be picked — a compatibility review immediately after
M9D's approval, before M9E began, caught that this cap was broader
than the evidence justified** (it would have silently downgraded
every future runtime, not just SteamVR) **and found it was no longer
even necessary**: the corrected fix satisfies both shader capabilities
via the plain `VK_EXT_shader_viewport_index_layer` device extension
(no feature struct to conflict with anything) and `geometryShader` via
`pEnabledFeatures`, neither of which touches the struct that actually
conflicted — so **the version cap was removed entirely**, re-tested,
and confirmed working: the XR device now genuinely selects **1.2**
(matching the desktop renderer, with zero exception logic anywhere in
the code) against the same real runtime, still with zero Vulkan
validation errors. `SelectVulkanApiVersion`'s output is used
unmodified. Applies only to the XR-compatible device — the M8 desktop
device keeps its unchanged, deliberately minimal feature set. See
`docs/ARCHITECTURE.md` Section 27, "Vulkan Device Feature Requirement
Discovered in M9D," for the full investigation.

**M9E gives AREngine its first real OpenXR frame lifecycle**: one
`XrSwapchain` per view (`OpenXRSwapchain.hpp/.cpp`, Vulkan-gated,
alongside M9C/M9D's files), sized from M9D's own recommended
dimensions/sample count, color format selected from what the runtime
actually reports (`VK_FORMAT_B8G8R8A8_SRGB` preferred, never assumed).
A real `xrWaitFrame`→`xrBeginFrame`→(acquire/wait/clear/release per
swapchain when `shouldRender`)→`xrEndFrame` loop runs for 200 frames
against SteamVR before requesting exit — the per-eye clear
(`vkCmdClearColorImage` to a distinct color per eye, synchronized with
a `VkFence` before releasing) proves the OpenXR-owned `VkImage`s are
genuinely usable, without rendering any real scene content. Zero
composition layers are submitted (`xrEndFrame`'s `layerCount = 0`) —
real per-view pose/FOV data via `xrLocateViews` is formally deferred to
M9F, and M9E deliberately does not fabricate it. Environment blend mode
is enumerated and selected (`OpenXREnvironmentBlendMode.hpp/.cpp`,
Vulkan-independent; `OPAQUE` preferred and selected against SteamVR).
**`SYNCHRONIZED` was reached for the first time in this project's
history** (`READY -> SYNCHRONIZED -> STOPPING -> EXITING`), confirming
M9D's own prediction that frame-loop participation is what unlocks it —
`VISIBLE`/`FOCUSED` were never reached, an honest limitation of
SteamVR's null/simulated-HMD test environment (no real display, no
real user-attention signal), not a defect in M9E's own logic. A real
bug was found and fixed via manual testing: calling `xrWaitFrame` after
`xrEndSession` had already run (this runtime requires the session to
be running for `xrWaitFrame`, confirmed empirically) crashed the demo;
fixed by only calling it while `session.IsRunning()`. Real Vulkan
validation noise was observed and traced to SteamVR's own internal
compositor objects (a debug-named `BlankEyeBuffer`, command buffers
this codebase never allocated) — zero validation errors were traced to
AREngine's own Vulkan calls. The existing `Frame::FrameDriver`
interface (`WaitForNextFrame`/`GetViews`/`SubmitFrame`) was evaluated
against this real lifecycle and found **not** to fit cleanly: no home
for `shouldRender`, no explicit seam for interleaving per-swapchain
acquire/render/release with a single `SubmitFrame()` call — reported as
a finding, not patched over with a premature `XRFrameDriver` at the
time. See `docs/ARCHITECTURE.md` Section 28 for the full investigation.

**M9E.5 redesigns `FrameDriver` using that evidence**:
`PrepareFrame() -> FrameContext` (was `WaitForNextFrame`), a new
`BeginFrame()`, `GetViews()` (unchanged shape), `EndFrame()` (was
`SubmitFrame`). `FrameContext` bundles `FrameTiming` (which gained one
new field, `bool shouldRender`) with a new `FrameStatus` (`Continue`/
`Idle`/`Stop`) — `Idle` means "no Begin/End call at all this tick"
(e.g. the XR session isn't currently running), a deliberately different
axis from `shouldRender=false` ("a Begin/End pair is happening, but
skip the content"); collapsing the two would have reintroduced the
`xrWaitFrame`-after-`xrEndSession` crash risk. `DesktopFrameDriver` and
`Runtime::Run()`'s loop were both refactored onto the new interface
with no behavior change (`Runtime` preserves the exact M7 event order:
Input.BeginFrame → Platform messages → Close check → Frame lifecycle →
Application/update/render → Frame completion). An initial
`XRFrameDriver` (`engine/xr/src/openxr/XRFrameDriver.hpp/.cpp`,
Vulkan-gated - needs `OpenXRSession`) now exists, wrapping exactly
`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame` and the session-state event
loop M9D/M9E already built — it deliberately does **not** own swapchain
image acquisition (that stays on `OpenXRSwapchain`, coordinated by
whichever layer actually renders - currently the manual frame demo),
does not call `xrLocateViews`, and does not render anything.
`engine/xr` regained a dependency on `AREngine::Frame` as a result - a
real, documented reversal of M9A's earlier trim, not a silent one. A
second real bug was caught and fixed during this milestone's own
design review (before implementation): `SessionEventPollResult`
previously collapsed multiple session-state transitions observed in one
poll cycle down to just the last one, which could silently skip a
required `xrBeginSession`/`xrEndSession` call - fixed by processing
every transition in order via a new pure, unit-tested
`DetermineSessionLifecycleActions` function, and confirmed genuinely
exercised (not just theoretical) against real SteamVR runs. See
`docs/ARCHITECTURE.md` Section 29 for the full design and validation.

**M9F integrates real OpenXR view location.** `XRFrameDriver::GetViews()`
now calls real `xrLocateViews` at the current frame's own predicted
display time (the same value stashed for `xrEndFrame`, never wall-clock
time), against the LOCAL reference space (M9D's `OpenXRReferenceSpace`,
unused since M9E.5, brought back), and converts the runtime's real
per-view pose and FOV into generic `Frame::ViewInfo` — the FOV
conversion genuinely supports and preserves independent per-eye
left/right/up/down angles (never collapsed to one symmetric value; see
below for what the live runtime actually reported) — via new
`OpenXRViewConversion.hpp` helpers — `ConvertXrPosition`
(straight field copy: OpenXR and AREngine share the same right-handed,
+Y-up, -Z-forward convention, verified against the OpenXR spec before
writing any conversion code, no axis flip anywhere), `ConvertXrOrientation`
(a component **reorder** only — `XrQuaternionf{x,y,z,w}` →
`Quaternion(w,x,y,z)`, confirmed against both types' actual definitions,
not assumed), and a new `Core::Math::PerspectiveOffCenterRH_ZO`
(`ViewProjection.hpp`) — the general off-center case
`PerspectiveRH_ZO`'s formula is a special case of, preserving OpenXR's
independent left/right/up/down angles rather than collapsing them to a
symmetric approximation; independently re-derived and verified during
design review (no sign error), with tests confirming each frustum edge
maps to its correct NDC boundary. `ViewInfo` itself needed **zero**
structural changes — `position`/`orientation` already represented
exactly what `xrLocateViews` provides (a pose, not a view matrix); a
precomputed `viewMatrix` field was seriously considered and explicitly
rejected as premature (mirrors `Scene::Camera`'s own "compute on demand,
don't duplicate stored data" precedent — nothing renders yet to need
one). A design-review pass caught that an earlier draft let
`XRFrameDriver` grow to own swapchain topology directly; reworked into
a small, separate `OpenXRProjectionLayer` (new,
`engine/xr/src/openxr/OpenXRProjectionLayer.hpp/.cpp`, Vulkan-independent,
directly pure-logic-testable) that owns per-view `XrSwapchainSubImage`
metadata and builds each frame's real `XrCompositionLayerProjection`
from it plus `XRFrameDriver`'s raw located `XrView` data (exposed via a
narrow `GetLastLocatedXrViews()` accessor — no `XrView`→`ViewInfo`→
reconstructed-`XrView` round-trip) — `FrameDriver::EndFrame()` itself
stays a completely generic, OpenXR-free override; a new minimal
XR-only seam (`XRFrameDriver::SetPendingProjectionLayer`) hands the
prepared layer across, consumed and reset to "none" every `EndFrame()`
call so a `shouldRender=false` tick safely defaults to zero layers.
View-state validity (`XR_VIEW_STATE_ORIENTATION_VALID_BIT`/
`POSITION_VALID_BIT`) is checked before ever using pose data — no
identity head pose is fabricated to hide invalid tracking. A
view-count/swapchain-count mismatch is a genuine runtime check (logged,
zero layers submitted), not a debug-only assertion that a Release build
could compile away. **Asymmetric-FOV support is proven by pure
math/conversion tests** (`PerspectiveOffCenterRH_ZO`'s frustum-boundary
tests, `tests/core_tests.cpp`), not by the live runtime — SteamVR's
null driver happened to report a symmetric ±45° FOV in every observed
run, so the live run demonstrates the conversion pipeline correctly
carries through whatever FOV the runtime provides, not that the
runtime itself exercised genuine asymmetry (a diagnostic review after
this milestone's initial approval corrected this exact overclaim). What
*was* verified twice against live SteamVR: a real, plausible IPD-scale
per-eye position offset (≈63mm) at the raw `xrLocateViews` level, a
real composition layer accepted by the compositor with no error, and
200/200 frames both runs. Poses could not be compared
across frames in this run — `shouldRender` was true only on frame 1
(the same SteamVR/null-driver behavior M9E already documented), and
`xrLocateViews` is deliberately not called on frames that don't render
— reported honestly as a real test-environment limitation, not papered
over. See `docs/ARCHITECTURE.md` Section 30 for the full design,
derivation, and validation.

**M9G renders AREngine's first real geometry into OpenXR** — a cube
(M8H's exact mesh/pipeline infrastructure, reused unchanged) drawn into
each eye's real OpenXR-owned Vulkan swapchain image, using a real view
matrix derived from `xrLocateViews`' own pose (new
`Core::Math::ViewMatrixFromPoseRH`/`Conjugate` — a closed-form rigid-
transform inverse, since a unit-quaternion orientation's inverse is
just its conjugate, not a general matrix inverse) and M9F's real
asymmetric per-view projection, submitted through the existing
`XrCompositionLayerProjection` path (`OpenXRProjectionLayer`, reused
with no changes to its own logic). `ViewInfo` needed **zero** further
changes — M9F already established it as sufficient, and M9G (the first
real renderer to consume it) confirms that conclusion rather than
overturning it. Two real, previously-latent bugs were found and fixed:
`ApplyVulkanYFlip` (M8F) only ever negated one matrix element, which
was silently correct only because every projection before M9G was
symmetric — M9F's genuinely asymmetric projections have a nonzero
off-center skew term that the old code left un-flipped; fixed by
negating the whole row (bit-for-bit unchanged for every existing
symmetric caller, confirmed by a new regression test). Separately,
found only once real rendering ran against live SteamVR:
`VulkanRenderPass` hard-coded a `PRESENT_SRC_KHR` final layout that's
meaningless for an OpenXR-owned (never `vkQueuePresentKHR`-ed) image
and was rejected by validation — generalized into an optional
constructor parameter, defaulted to the old value so every desktop call
site is unaffected. New `tests/openxr_cube_demo.cpp` (a separate demo
from `openxr_frame_demo.cpp`, keeping frame-lifecycle diagnostics and
actual rendering as distinct, coherently-sized concerns) ran 200/200
frames clean against live SteamVR; `shouldRender` was true only on
frame 1 (the same SteamVR/null-driver behavior M9E/M9F already
documented — not a regression), so only one frame actually rendered,
and this runtime offers no visual window to confirm the result by eye —
reported as exactly that limitation, not glossed over. See
`docs/ARCHITECTURE.md` Section 31 for the full design, both bugs' fixes,
and validation.

**M9H hardens M9G's render path — a cleanup milestone, no new rendering
capability.** M9G's demo code was audited first (per the milestone's own
requirement) into reusable-infrastructure/demo-only/duplicated/
one-frame-assumption categories; the one genuinely reusable piece (per-
view depth image + framebuffer ownership, and the per-view render-pass
recording sequence) was extracted into a new, narrow
`OpenXRVulkanViewTarget` class + `RecordOpenXRViewRenderPass` free
function (`tests/OpenXRVulkanViewTarget.hpp/.cpp`) — deliberately kept
at the `tests/` leaf level, **not** promoted into `engine/xr` or
`engine/rendering`, because doing so would require one of those modules
to gain a brand-new dependency on the other (confirmed by reading their
actual `CMakeLists.txt` link lines), which M9C/M9F/M9G each already
deliberately chose not to do. `arengine_openxr_cube_demo`'s target
frame count was raised from M9G's 200 to 1000 to validate the generic
frame loop (`PrepareFrame`/`BeginFrame`/`GetViews`/`EndFrame`,
session-state handling, command-buffer/fence reuse) over a much longer
run — 1000/1000 frames completed cleanly against live SteamVR, same
session-state sequence and same already-documented SteamVR-internal
validation noise as every prior XR milestone, no new AREngine-
attributable error. `shouldRender` was still `true` on frame 1 only;
this was investigated as an environment-configuration question (SteamVR
null-driver settings files read directly, zero related toggle found;
the session never reaches `XR_SESSION_STATE_FOCUSED` in any observed
run, the most plausible explanation) without adding any vendor-specific
code to the engine, per the milestone's explicit instruction — reported
as an honest, unresolved environment limitation, not worked around.
Lightweight `std::chrono`-only performance diagnostics (frames
attempted/rendered, views rendered, draw calls, average CPU prep/Vulkan
submit time) were added; a slow cube rotation was added for repeated-
frame verification, explicitly not a stand-in for head tracking. See
`docs/ARCHITECTURE.md` Section 32 for the full audit, the module-
boundary finding, and validation.

**M10 gives AREngine its first OpenXR action/input foundation** - one
`"gameplay"` `XrActionSet`, four actions (`aim_pose`/`select`/`trigger`/
`move` - pose/boolean/float/vector2f, one of each OpenXR action-type
category), each spanning both hands via shared `/user/hand/left`|
`right` subaction paths rather than duplicated left_x/right_x actions.
KHR simple_controller bindings suggested for `select`/`aim_pose` only -
verified against the OpenXR spec that this profile has no trigger or
thumbstick component at all, so `trigger`/`move` are deliberately left
unbound (they still exist and correctly report inactive) rather than
guessing a nonexistent component path. Action set attached once;
per-hand `aim_pose` action spaces created after attach; `xrSyncActions`
called every running frame, independent of `shouldRender`. An
InputSystem audit (required before any code) found M7's `ButtonState`/
`KeyCode`/`MouseButton` model has no home for float/vector2/pose/
"active" state and correctly left it untouched - a new, small generic
vocabulary (`AREngine::Input::DigitalActionState`/`AnalogActionState`/
`Vector2ActionState`/`PoseActionState`, `AREngine/Input/ActionState.hpp`)
was added instead, with zero OpenXR dependency; `engine/xr` gained a
new private dependency on `AREngine::Input` to produce these (mirroring
the exact precedent `Frame::ViewInfo` already set in M9F), never the
reverse. Digital press/release is computed from an explicit edge
against the action system's own tracked previous state - deliberately
not equated with `changedSinceLastSync` - and every state type clears
to inactive/zero rather than leaving stale "held" values behind when an
action goes inactive (mirrors M7's focus-loss key-release handling).
New `tests/openxr_input_demo.cpp` (no swapchain, no rendering - only
the frame lifecycle needed to keep the session running) ran 500/500
frames clean against live SteamVR, all synced; every action on both
hands reported inactive for the entire run, honestly reported as this
environment's SteamVR null driver exposing no real controller, not
worked around or fabricated. See `docs/ARCHITECTURE.md` Section 33 for
the full design, the InputSystem audit, and validation.

**M10.5 builds AREngine's first INTEGRATED XR demo** - one process, one
`XrSession`, combining the render path (M9G/M9H) and the action/input
path (M10) in the same frame loop, proving they don't interfere with
each other. An audit of all six prior OpenXR demos found the real gap:
none rendered more than one object per view, and none exercised
rendering and the action system together. Closed with the smallest
justified extraction - `OpenXRVulkanViewTarget`'s single-object
`RecordOpenXRViewRenderPass` (M9H) was split into
`BeginOpenXRViewRenderPass`/`DrawOpenXRViewObject`/`EndOpenXRViewRenderPass`
so a view's render pass can draw multiple objects across multiple
meshes (a floor quad + four cubes) - `openxr_cube_demo.cpp` still
calls the original single-object wrapper unchanged. `main()` in the new
`tests/xr_demo.cpp` directly owns every resource, same shape as every
prior demo - no `XRApplicationThatOwnsEverything` coordinator class was
introduced. `Scene::Scene` was evaluated (not skipped by default) and
declined: for a flat, non-hierarchical five-object scene, its
`GetWorldMatrix()` degenerates to exactly the `Transform::ToMatrix()`
call already used, and it has no mesh/tint concept at all, so it would
add indirection with zero real value here. Every running frame calls
`xrSyncActions` and queries controller state unconditionally, then
renders (if `shouldRender`) - verified against the spec that input
sync has no `shouldRender` precondition, confirmed live
(`framesSynced=1000` vs. `framesWithShouldRenderTrue=1`). Ran 1000/1000
lifecycle iterations clean against live SteamVR: 2 views x 5 objects =
10 draws on the one frame that rendered, exactly as predicted; both
already-known environment limitations (render-opportunity-once,
no real controller) reappeared and were reported honestly, not worked
around. See `docs/ARCHITECTURE.md` Section 34 for the full audit,
boundary re-confirmation, and validation.

**M10.6 closes the exact gap M10.5 identified: input now visibly
affects the scene.** A new, small pure layer
(`tests/XRInteractionState.hpp/.cpp` - generic `Input`/`Core::Math`
types only, zero OpenXR/Vulkan dependency, builds and passes even under
`ARENGINE_ENABLE_OPENXR=OFF`/`ARENGINE_ENABLE_VULKAN=OFF`) converts the
right hand's already-queried `Input::DigitalActionState`/
`AnalogActionState`/`Vector2ActionState`/`PoseActionState` into a
shared `XRInteractionState`: `select.pressed` (never `.down` - a held
button must not re-toggle every frame) toggles the reference cube's
highlight tint; `trigger.value` scales it (1.0x..1.8x, resetting to
1.0x the instant the action goes inactive - never a stale reading);
`move.value` offsets one diagnostic cube directly (bounded, never
delta-time-integrated - a visualization of current input, not a WASD-
style controller); `aim_pose` shows/hides/positions a small marker cube
only when the action is active AND both position/orientation are
valid (the deliberately conservative policy chosen and documented, not
left implicit). 20 new pure-logic tests
(`tests/xr_interaction_tests.cpp`) cover every mapping with synthetic
`Input::*ActionState` values - legitimate in a unit test, never fed
into the live demo in place of real queried OpenXR state.
`Input::InputSystem` itself was not touched - M10's own
`OpenXRActionSystem` already was the "OpenXR → generic Input types"
bridge the milestone needed, and no live bridge into a running
`InputSystem` instance was added (still no concrete need for one).
`arengine_xr_demo` ran 1000/1000 frames clean against live SteamVR, all
synced; the interaction state reached the end of the run in its
neutral/default form, exactly as a correctly-wired, correctly-inactive
pipeline should - proven distinctly from, and never conflated with,
the (not yet available) case of a real controller actually driving a
visible change. See `docs/ARCHITECTURE.md` Section 35 for the full
design and the explicit test-vs-live evidence split.

There is still no real image loading (PNG/JPEG/stb_image), no uniform
buffers, no `SceneRenderer`/Scene integration, no `MeshAsset`/glTF/OBJ
loading, no normals/lighting, no frame limiting, no ECS/component
system, no gameplay, and no proof of a REAL controller driving a
visible change (only unit-tested logic and live-but-inactive OpenXR
plumbing so far) — see Sections 11–15. `Editor` is still an M0-style
stub with no functionality. Next up, per M10.6's own evidence-based
recommendation: acquiring or investigating a genuinely continuous-
render/active-input-capable test environment (Meta XR Simulator remains
the standing recommendation) so a future milestone can finally exercise
the input-to-state code paths with real, changing, physically-sourced
values - or M8I+ (Scene integration, still pending within M8) — see
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
   for what's still pending within M8.
3a. **OpenXR instance/system discovery + Vulkan graphics-binding +
   XrSession/session-state/reference-space bring-up + XR swapchains/
   frame lifecycle + generic FrameDriver redesign + real view location/
   composition-layer submission + multiple objects rendered into each
   eye's swapchain image (hardened for repeated-frame reuse) + an
   OpenXR action/input foundation (one action set, four actions
   spanning both hands, KHR simple_controller bindings, xrSyncActions,
   controller pose action spaces located via xrLocateSpace) + these two
   paths proven to coexist in one integrated session/frame loop + real
   queried input now visibly driving object tint/scale/offset/marker
   state through a small unit-tested pure interaction layer so far
   (M9A–M10.6)**: no hand tracking (`XR_EXT_hand_tracking`) or eye
   tracking, no haptics (`XR_ACTION_TYPE_VIBRATION_OUTPUT`), no
   passthrough, no anchors, no `SceneRenderer`/`Scene` integration
   driving what's rendered into XR eyes or consuming controller actions
   (the scene is still a hand-written `std::vector`, not
   `Scene::Scene` - evaluated and declined twice now, see
   `docs/ARCHITECTURE.md`; controller action state feeds
   `tests/XRInteractionState.hpp`'s pure functions directly, never a
   live `Input::InputSystem` instance - still no concrete need for one),
   no `Scene::Camera` driving XR eyes, no XR head/`viewFromWorld` camera
   abstraction beyond the free-function `Core::Math::ViewMatrixFromPoseRH`
   M9G added, no swapchain-image acquisition inside `XRFrameDriver`
   (that stays on `OpenXRSwapchain`, coordinated outside
   `Frame`/`XRFrameDriver`), no rendering responsibility added to
   `XRFrameDriver` for any of this, and no proof yet of a REAL
   controller driving a visible change (only unit-tested logic and
   live-but-inactive OpenXR plumbing - see
   `docs/ARCHITECTURE.md`'s explicit test-vs-live evidence split). See
   `docs/ROADMAP.md`'s M11 row.
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
8. **OpenXR/Vulkan handle ownership must be proven by creation source,
   never inferred from validation-layer output.** A `vkDestroyDevice`
   "leaked object" message names a handle, not its creator - before
   treating one as an AREngine bug, check whether AREngine's own code
   actually calls the corresponding `vkCreate*`/`xrCreate*` function
   (and how many times), or whether the object is documented as
   runtime/OpenXR-owned (e.g. swapchain `VkImage`s - see
   `OpenXRSwapchain.hpp`). See `docs/ARCHITECTURE.md`, "M11.3 - Teardown
   Investigation" for the concrete method (grep for the creating call,
   compare counts) this uncovered.
9. **Instrument XR/Vulkan teardown before reordering any destructor or
   adding synchronization calls on a guess.** A crash or validation
   error during shutdown is not evidence of *where* the code went wrong
   until the exact last-completed step is captured with crash-survivable
   logging (`std::cerr`, unit-buffered plus explicit flush - `AR_LOG_INFO`'s
   `std::cout` can lose buffered-but-unflushed output when a process is
   killed by an unhandled exception). See `docs/ARCHITECTURE.md`'s
   `TeardownMarker` pattern in `tests/xr_demo.cpp`/`openxr_frame_demo.cpp`/
   `openxr_session_demo.cpp`.
10. **When Windows Smart App Control blocks a local AREngine test binary,
    prefer a clean relink/rebuild and verify Code Integrity logs before
    changing security settings. Do not disable SAC, Defender, or related
    protections as a first response.** See `docs/ARCHITECTURE.md`,
    "M11.3A - Smart App Control Diagnosis" - a blocked binary's hash
    simply hadn't run yet under enforcement; a clean rebuild resolved
    every case observed, with Smart App Control left enabled throughout.
11. **Meta XR Simulator v205 has a known teardown instability with
    AREngine's valid Vulkan/OpenXR path** - the integrated demo's own
    1000-frame session (real render loop, real controller sync, clean
    `vkDeviceWaitIdle`) completes successfully every time, then the
    process crashes inside `OpenXRVulkanGraphicsBinding`'s destructor,
    around `vkDestroyDevice`, only under Meta - never under SteamVR
    running the identical AREngine code. Longer/busier sessions reproduce
    it more reliably (session_demo: ~12%; the 1000-frame integrated demo:
    100%). Exhaustively investigated in `docs/ARCHITECTURE.md`, "M11.3
    Resumed - Teardown Investigation Conclusion": no AREngine-owned
    `VkDevice` child ever survives to the failure point, and no acquired
    `XrSwapchain` image remains at shutdown, in any observed run - **if
    all AREngine-owned `VkDevice` children are already destroyed and
    swapchain images already released, do not invent a teardown-order
    fix for runtime-owned objects** on a future run of this same
    investigation; the evidence already rules that out (see rules 8-9
    above for the ownership-proof and instrument-first discipline that
    produced this conclusion). **Do not add runtime-name workarounds,
    sleeps, intentional leaks, or validation suppression** to work around
    this - none are justified by the evidence, and this milestone
    explicitly forbade them. **SteamVR remains the stable regression
    runtime for teardown behavior** - use it to confirm any future
    XR-path change hasn't regressed, independent of whatever Meta's own
    crash does.
12. **OpenXR interaction profile resolution depends on the profiles/bindings
    the application suggests - do not assume the runtime's profile before
    suggestion.** M11.1B found Meta XR Simulator (system name "Meta Quest
    3") reports exactly `khr/simple_controller` when that's the only
    profile AREngine suggests, and switches to `oculus/touch_controller`
    the moment AREngine also suggests that one - the runtime never
    reports a profile the application didn't offer a binding for, however
    capable the underlying (real or simulated) hardware actually is. See
    `docs/ARCHITECTURE.md`, "M11.1B - Active Head + Controller Validation".
13. **Preserve multiple standards-based interaction-profile bindings side
    by side. Do not replace `khr/simple_controller` when adding
    Touch-style profiles** - `OpenXRActionSystem`'s constructor suggests
    both unconditionally, with no runtime-name check deciding which to
    use; the runtime alone resolves which one matches. See
    `OpenXRTouchControllerBindings.hpp`/`OpenXRSimpleControllerBindings.hpp`.
14. **Bind Vector2f actions only to valid aggregate vector2 components**
    (e.g. `/input/thumbstick`), never a scalar `/x` or `/y` sub-path alone.
15. **Treat `active=true` with neutral/default values as successful
    action *activation*, not evidence of driven user input.** M11.1B's
    touch_controller binding made select/trigger/move/aim_pose all
    genuinely active on both hands and made the M10.6 pose marker
    visible - a real result - but every value stayed at its idle default
    (no press, zero trigger, zero thumbstick, static pose), because no
    live input was actually driven that session.
16. **Never claim live interaction is proven until actual value
    transitions are observed** - a press/release cycle, a nonzero
    analog/thumbstick reading, or a changed pose. Activation alone
    (rule 15) is not sufficient evidence, however genuine it is on its
    own terms.
17. **If a third-party XR runtime DLL is blocked by Windows Smart App
    Control, diagnose signature/reputation and Code Integrity events
    first.**
18. **Do not weaken Smart App Control, Defender, or Code Integrity to
    make an XR development runtime load.**
19. **Treat vendor runtime signing/reputation failures as
    environment/toolchain issues, not AREngine defects.**
20. **Scene owns world/renderable state, never GPU resources.**
21. **Renderable data must remain backend-neutral.**
22. **Resolve MeshId or future render-resource identifiers outside
    Scene.**
23. **Extract scene renderables once, then consume them across any
    number of views.**
24. **XR eyes are runtime-driven views, not Scene camera entities.**
25. **Do not introduce transform/extraction caching without profiling
    evidence.**
26. **Avoid fragile positional aggregate initialization for
    Scene::Renderable as the structure evolves. Prefer explicit field
    assignment/designated-style construction supported by the project's
    C++ conventions when it improves correctness. Adding a new render-
    resource field must not silently change visibility/tint/material
    semantics.**

## Build

```
cmake -S . -B build
cmake --build build
```
