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
| M0 | Repository structure + CMake scaffolding | **In progress** | Stub targets only, no functionality |
| M1 | `Core`: math (built to the world conventions), logging, assertions, `Frame`'s `FrameDriver`/`FrameTiming`/`ViewInfo` types. `std::` containers throughout. | Not started | |
| M2 | `Platform` (Windows): window, raw keyboard/mouse, timing, file I/O | Not started | |
| M3 | Minimal `Runtime`: main loop written against `FrameDriver`, backed by `DesktopFrameDriver`. Opens a window, prints FPS. | Not started | Proves the loop doesn't hard-code desktop timing assumptions |
| M4 | `Rendering`: minimal RHI (create buffer/texture/pipeline, draw) + a "null" backend that logs calls | Not started | RHI kept deliberately small; no `Present()` yet — see ARCHITECTURE.md |
| M5 | `Scene`: entities, transforms, hierarchy; Sandbox draws through the null renderer | Not started | Confirms world-convention math end-to-end |
| M6 | `Assets`: minimal file-based mesh/texture loading | Not started | |
| M7 | `Input`: simple action-mapping over raw Platform input | Not started | Desktop-only for now |
| M8 | **Vulkan backend** implementing the RHI: triangle, then a textured mesh | Not started | First external dependency, isolated to `Rendering` |
| M9 | `XR` module: OpenXR integration, `XRFrameDriver` implementing wait/predict/submit | Not started | `Runtime`'s main loop requires no changes — only the `FrameDriver` swaps |
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
