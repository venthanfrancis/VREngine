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
| — M9F | `xrLocateViews` + stereo rendering. | Not started | |
| — M9G | Head-tracked AREngine demo. | Not started | Fulfills M9's own goal, ahead of M10's full AR/XR demo |
| M10 | **Simple AR/XR demo**: a minimal scene, rendered stereo through Vulkan, driven by real head tracking | Not started | Fulfills the project's primary goal chain |
| M11 | `Physics`: minimal implementation | Deprioritized | Starts only after M10 |
| M12 | `Audio`: minimal implementation | Deprioritized | Starts only after M10 |
| M13 | `Editor` skeleton | Deprioritized | After the core chain is proven |
| M14 | Android/Linux `Platform` backend | Later | When actually needed |
| M15 | Custom AR hardware bring-up | Long-term | The project's ultimate goal |

## Rules while working through the roadmap

- No third-party dependencies until a milestone explicitly calls for one.
- No Vulkan code before M8. No OpenXR code before M9.
- No custom container types — use `std::` directly.
- Respect the dependency layering in `docs/ARCHITECTURE.md`.
