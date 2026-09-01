# AREngine Roadmap

Guiding idea: prove every architectural seam with a fake/minimal
implementation before paying for the expensive real one (Vulkan, OpenXR,
real hardware). The primary goal chain for this project is:

```
C++ Engine → Vulkan → OpenXR → Head tracking → Simple AR/XR demo
```

Editor, advanced Physics, advanced Audio, and custom glasses hardware
all come after that chain is proven.

## Milestones

| # | Milestone | Status | Notes |
|---|---|---|---|
| M0 | Repository structure + CMake scaffolding | **Complete** | Stub targets only, no functionality |
| M1 | `Core`: math (built to the world conventions), logging, assertions, `Frame`'s `FrameDriver`/`FrameTiming`/`ViewInfo` types. `std::` containers throughout. | **Complete** | See `docs/ARCHITECTURE.md` Section 9 |
| M2 | `Platform` (Windows): window, raw keyboard/mouse, timing, file I/O | **Complete** | Window + SteadyClock done; raw keyboard/mouse deferred to M7 (Input); file I/O deferred to a milestone with a real consumer — see `docs/ARCHITECTURE.md` Section 10 |
| M3 | Minimal `Runtime`: main loop written against `FrameDriver`, backed by `DesktopFrameDriver`. Opens a window, prints FPS. | **Complete** | Proves the loop doesn't hard-code desktop timing assumptions — see `docs/ARCHITECTURE.md` Section 11 |
| M4 | `Rendering`: minimal RHI (create buffer/texture, draw) + a "null" backend | **Complete** | RHI kept deliberately small; no pipeline/shader API and no `Present()` yet — see `docs/ARCHITECTURE.md` Section 12 |
| M5 | `Scene`: entities, transforms, hierarchy | **Complete** | Confirms world-convention math end-to-end; not yet wired into Runtime/Rendering — see `docs/ARCHITECTURE.md` Section 13 |
| M6 | `Assets`: asset root, AssetId, TextAsset/BinaryAsset loading, caching | **Complete** | Real mesh/texture formats deferred — see `docs/ARCHITECTURE.md` Section 14 |
| M7 | `Input`: desktop keyboard/mouse state + action-mapping over raw Platform input | **Complete** | Desktop-only (no hand/controller/XR input yet) — see `docs/ARCHITECTURE.md` Section 15 |
| M8 | **Vulkan backend** implementing the RHI: triangle, then a textured mesh | In progress | First external dependency, isolated to `Rendering`. Split into sub-milestones — see below |
| — M8A | Vulkan bring-up: instance, validation, physical/logical device, graphics queue. No swapchain, no rendering. | **Complete** | See `docs/ARCHITECTURE.md` Section 16 |
| — M8B | Window → `VkSurfaceKHR` → presentation-capable device → swapchain → acquire/clear/present loop, resize + minimize handling. No triangle. | **Complete** | See `docs/ARCHITECTURE.md` Section 17 |
| — M8C | First triangle: render pass, graphics pipeline, GLSL→SPIR-V shaders, `gl_VertexIndex`-generated vertices (no vertex buffer yet). | **Complete** | See `docs/ARCHITECTURE.md` Section 18 |
| — M8D | Real GPU geometry: `VulkanBuffer`, staging uploads, vertex + index buffers, indexed quad draw. | **Complete** | See `docs/ARCHITECTURE.md` Section 19 |
| — M8E | Textured quad: `VulkanImage`/`VulkanSampler`, procedural checkerboard, descriptor set, UV sampling. | **Complete** | See `docs/ARCHITECTURE.md` Section 20 |
| — M8F | Genuine 3D: Vec3 positions, fixed camera, Vulkan-conforming perspective, depth buffer/testing, push-constant MVP. | **Complete** | See `docs/ARCHITECTURE.md` Section 21 |
| — M8G | Backend-independent `Scene::Camera` abstraction, driven by a demo-private controller using `Input`: WASD move, click-drag mouse look, delta-time-based movement over a small hard-coded scene. | **Complete** | See `docs/ARCHITECTURE.md` Section 22 |
| — M8H | Reusable mesh representation: backend-independent `Rendering::MeshData`, Vulkan-private `VulkanMesh`, one cube mesh uploaded once and drawn as multiple instances via different Model transforms. Back-face culling enabled. | **Complete** | See `docs/ARCHITECTURE.md` Section 23 |
| — M8I+ | Scene integration | Not started | |
| M9 | `XR` module: OpenXR integration, `XRFrameDriver` implementing wait/predict/submit | In progress | `Runtime`'s main loop requires no changes — only the `FrameDriver` swaps. Split into sub-milestones — see below |
| — M9A | OpenXR instance/system discovery: loader discovery (fetched via CMake), `XrInstance`, API layer/instance extension enumeration, HMD-class `XrSystemId` request + system properties, graceful no-runtime/no-headset handling. No session, no swapchain, no Vulkan/OpenXR bridge. | **Complete** | See `docs/ARCHITECTURE.md` Section 24 |
| — M9B | Runtime/simulator development environment: planning/review only — inspect this machine's OpenXR runtime registration, confirm how the loader discovers runtimes, recommend a conformant, vendor-neutral-to-AREngine runtime/simulator for headset-free development. No engine code changes; no installation performed yet. | **Complete** | See `docs/ARCHITECTURE.md` Section 25 |
| — M9C | Vulkan/OpenXR graphics requirements and graphics binding: `XR_KHR_vulkan_enable2` enabled, Vulkan graphics requirements queried, XR-controlled `VkInstance`/`VkPhysicalDevice`/`VkDevice`/queue created via OpenXR's own Vulkan functions, `XrGraphicsBindingVulkan2KHR`-ready data assembled. Still no session. | **Complete** | See `docs/ARCHITECTURE.md` Section 26 |
| — M9D | `XrSession` created from the M9C graphics binding; `XrSessionState` tracked via `xrPollEvent`/`XrEventDataSessionStateChanged`; `xrBeginSession`/`xrEndSession` at the correct states; view configurations and reference spaces (VIEW/LOCAL/STAGE) enumerated and created. Still no swapchain, no frame loop. | **Complete** | See `docs/ARCHITECTURE.md` Section 27 |
| — M9E | XR swapchains + frame lifecycle (`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`). | **Complete** | See `docs/ARCHITECTURE.md` Section 28 |
| — M9E.5 | Generic `Frame::FrameDriver` redesign using M9E's real evidence: `PrepareFrame`/`BeginFrame`/`GetViews`/`EndFrame`, `FrameStatus`/`shouldRender`, initial `XRFrameDriver` (wait/begin/end only). `DesktopFrameDriver`/`Runtime` refactored onto it, behavior unchanged. | **Complete** | See `docs/ARCHITECTURE.md` Section 29 |
| — M9F | Real `xrLocateViews` view location: real per-view pose/asymmetric FOV converted into generic `Frame::ViewInfo`, real `XrCompositionLayerProjection` submitted (`OpenXRProjectionLayer`). Still no scene rendering. | **Complete** | See `docs/ARCHITECTURE.md` Section 30 |
| — M9G | First real 3D rendering into OpenXR: a cube, using real per-view poses (inverted into proper view matrices) and real asymmetric per-view projections, rendered into each eye's OpenXR swapchain image and submitted via the existing `XrCompositionLayerProjection` path. Still no Scene/SceneRenderer integration. | **Complete** | See `docs/ARCHITECTURE.md` Section 31 |
| — M9H | XR render-path hardening: M9G's demo code audited and its reusable per-view render-resource ownership extracted into a small, narrow helper (`OpenXRVulkanViewTarget`); repeated frame-lifecycle behavior validated over 1000 iterations; lightweight performance diagnostics added; continuous-`shouldRender` environment limitation investigated and reported (no vendor-specific code added). Still no Scene/SceneRenderer integration. | **Complete** | See `docs/ARCHITECTURE.md` Section 32 |
| M10 | OpenXR action/input foundation: one `XrActionSet`, four actions (pose/boolean/float/vector2f, each spanning both hands via shared subaction paths), KHR simple_controller bindings, action-set attach, per-hand pose action spaces, `xrSyncActions`, and a new generic `Input::*ActionState` vocabulary (`Input` module unchanged, no OpenXR type leaks). No gameplay, no Scene/rendering integration, no hand tracking, no haptics. | **Complete** | See `docs/ARCHITECTURE.md` Section 33 |
| M10.5 | **Integrated XR demo**: the render path (M9G/M9H) and the action/input path (M10) combined in one session/frame loop - multiple objects (floor + 4 cubes) rendered into multiple views, `xrSyncActions`/controller state queried every running frame, all validated over 1000 lifecycle iterations. Existing-demo audit found and closed one real gap (no prior demo rendered >1 object/view or exercised actions+rendering together); Scene integration evaluated and deliberately declined (no real value for a flat, non-hierarchical scene). Still no gameplay, no SceneRenderer, no input-driven state. | **Complete** | See `docs/ARCHITECTURE.md` Section 34 |
| M10.6 | Input-driven interaction state: real queried OpenXR action state (M10), through generic `Input::*ActionState`, through a new small pure interaction layer (`XRInteractionState`, 20 unit tests, zero OpenXR/Vulkan dependency), to visible tint/scale/offset/pose-marker changes in the M10.5 scene. Digital reacts to `pressed` not `down`; analog/vector reset to neutral when inactive, never stale; pose marker hidden unless active+fully valid. Proven live against SteamVR/null (1000/1000 frames synced, neutral end state, honestly reported - no real controller available to drive a visible change yet). No gameplay, no InputSystem/Scene changes. | **Complete** | See `docs/ARCHITECTURE.md` Section 35 |
| M11 | XR development test environment: research-only survey of headset-free OpenXR dev environments (Meta XR Simulator, Monado, others), no installation, no engine changes. | **Complete** | Recommended Meta XR Simulator as primary candidate |
| — M11.1 | Install and validate Meta XR Simulator (v205.0) against AREngine's existing OpenXR/Vulkan demos, through Step 4 (continuous frame validation). Found real gains (sustained `shouldRender=true`, `FOCUSED` reached, real stereo poses) and a real Vulkan validation error (timeline semaphore) plus a clean-exit crash. No engine code changed. | **Complete (through Step 4)** | Steps 5-10 (controller/interaction-profile work) deferred pending M11.2's crash finding |
| — M11.2 | Vulkan timeline-semaphore device-feature negotiation: root-caused and fixed the Step 4 timeline-semaphore validation error (capability-driven, no runtime-name branching, narrow `VkPhysicalDeviceTimelineSemaphoreFeatures` struct, SteamVR's M9D conflict not reintroduced). The Meta clean-exit crash from M11.1 **persists** (root cause not yet proven - ownership of the leaked/racing objects between AREngine, Meta, and SteamVR is not yet established), and new (non-fatal) SteamVR teardown validation noise was newly discovered. | **Partial** | Feature-negotiation fix: **complete**. Meta teardown crash investigation: **open** - see `docs/ARCHITECTURE.md`, "M11.2 Result Summary: Three Separate Problems". M11.1 Steps 5-10 remain blocked until resolved. |
| — M11.3A | **Smart App Control diagnosis: COMPLETE.** Findings: Smart App Control enforcement confirmed through Code Integrity events; affected binaries were unsigned local compiler outputs with no Mark-of-the-Web; the differentiator was hash/execution history under SAC enforcement, not file origin, ACLs, or MOTW; clean relink produced a new hash and restored execution; full CTest returned to 17/17 with Smart App Control still enabled; no Defender/SmartScreen/firewall/security weakening required. | **Complete** | Development rule: if a locally-built test executable is unexpectedly blocked under SAC, relink/rebuild it before changing Windows security settings. See `docs/ARCHITECTURE.md`, "M11.3A - Smart App Control Diagnosis". |
| — M11.3 | **XR teardown/lifetime investigation: COMPLETE.** Conclusion: AREngine destruction order and Vulkan/OpenXR ownership audited; no AREngine-owned Vulkan device child survives to `VkDevice` destruction; no acquired `XrSwapchain` image remains at shutdown; `vkDeviceWaitIdle` succeeds in the integrated path; crash reproduces inside/after `OpenXRVulkanGraphicsBinding` teardown, around `vkDestroyDevice`, only under Meta XR Simulator; SteamVR remains functionally stable; best-supported classification: Meta runtime bug or Vulkan/OpenXR interop issue; no AREngine-side fix justified by current evidence. Known environment limitation: Meta XR Simulator v205 may crash the client process during Vulkan/OpenXR teardown - longer sessions reproduce more reliably. | **Complete** | See `docs/ARCHITECTURE.md`, "M11.3 Resumed - Teardown Investigation Conclusion". Root cause: runtime bug/interop issue (not AREngine). Exact crash stack/module still unknown - no debugger/minidump was used. |
| — M11.1B | **Active head/controller validation: PARTIAL.** Completed: Meta runtime health validated; actual interaction profile discovered; `oculus/touch_controller` binding support added; `simple_controller` preserved; select/trigger/move/aim_pose all become active on both hands; generic `ActionState` conversion works with the real profile; pose action becomes active + position/orientation valid; M10.6 pose marker becomes visible from real runtime state; sustained rendering + active input coexist; SteamVR regression clean; full build matrix and CTest green. Still open: driven select press/release; nonzero trigger values; nonzero thumbstick values; controller pose movement; simulated head movement; visible highlight/scale/move changes from driven input. | **Partial** | Blocker: Meta XR Simulator GUI input could not be reliably driven through the current automation environment. See `docs/ARCHITECTURE.md`, "M11.1B - Active Head + Controller Validation". M11.1 not yet marked complete. |
| — M11.1C | **Meta XR Operator setup: PARTIAL.** Completed: Meta XR Operator v205.1 standalone/native package validated; Unity not required; OpenXR API layer loads successfully; AREngine still uses Meta XR Simulator normally; Vulkan/OpenXR path remains healthy; zero new pre-teardown OpenXR/Vulkan errors; no AREngine source dependency on Meta XR Operator. Pending: MCP agent connection; programmatic head movement; driven select press/release; nonzero trigger input; nonzero thumbstick input; moving controller pose; visible M10.6 interaction from driven values. | **Partial** | Blocker: Meta XR Operator MCP proxy must be registered from the user's normal terminal where the Claude CLI is available. See `docs/ARCHITECTURE.md`, "M11.1C - Meta XR Operator Input-Driving Validation". M11.1 not yet marked complete. |
| — M11.1D-B | **Smart App Control / Meta runtime load diagnosis: COMPLETE.** Conclusion: Windows/Defender/network health verified; Smart App Control enforcement confirmed healthy and unchanged; Meta XR Simulator v205 `SIMULATOR.dll` is an authentic official file but is not Authenticode-signed; Smart App Control blocks the DLL under `VerifiedAndReputableDesktop` before the OpenXR runtime can load; no AREngine defect is involved; no supported per-file Smart App Control exception exists. Durable fixes: (1) Meta ships a validly signed simulator runtime, or (2) Microsoft establishes reputation/trust for the file through the supported submission process. | **Complete** | M11.1 live-driven validation remains externally blocked by the current Meta runtime package under Smart App Control. See `docs/ARCHITECTURE.md`, "M11.1D - Live Input Validation Attempt" and "M11.1D-B - Smart App Control / Meta Runtime Load Diagnosis". |
| M16 | `Physics`: minimal implementation | Deprioritized | Starts only after M10.6 |
| M17 | `Audio`: minimal implementation | Deprioritized | Starts only after M10.6 |
| M18 | `Editor` skeleton | Deprioritized | After the core chain is proven |
| M19 | Android/Linux `Platform` backend | Later | When actually needed |
| M20 | Custom AR hardware bring-up | Long-term | The project's ultimate goal |

### M11 closeout

```
M11 — XR development validation: COMPLETE WITH EXTERNAL LIMITATION

Engine work: COMPLETE
Environment validation: COMPLETE
Live driven input: PARTIALLY VERIFIED / EXTERNALLY BLOCKED

Proven:
- continuous XR rendering
- FOCUSED session state
- asymmetric runtime-driven XR views
- OpenXR/Vulkan device path
- XR frame lifecycle
- OpenXR actions
- simple-controller profile support
- Touch-controller profile support
- real action activation
- valid controller pose state
- generic ActionState conversion
- input-to-engine-state plumbing
- XR/Vulkan ownership and teardown correctness on AREngine side
- SteamVR regression path
- Meta XR Operator API-layer compatibility

Not yet proven:
- actively moved simulated head pose
- real select press/release from simulator
- nonzero trigger value
- nonzero thumbstick value
- actively moved controller pose

External limitation:
Meta XR Simulator v205 SIMULATOR.dll is unsigned and is blocked by Windows
Smart App Control on this development machine. No AREngine workaround is
appropriate.
```

## Rules while working through the roadmap

- No third-party dependencies until a milestone explicitly calls for one.
- No Vulkan code before M8. No OpenXR code before M9.
- No custom container types — use `std::` directly.
- Respect the dependency layering in `docs/ARCHITECTURE.md`.
