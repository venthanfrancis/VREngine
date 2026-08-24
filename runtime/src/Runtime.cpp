#include "AREngine/Runtime/Runtime.hpp"

#include "AREngine/Core/Core.hpp"
#include "AREngine/Platform/WindowCloseEvent.hpp"
#include "AREngine/Platform/WindowResizeEvent.hpp"
#include "AREngine/Rendering/NullRenderDevice.hpp"
#include "AREngine/Runtime/DesktopFrameDriver.hpp"

#include <format>

namespace AREngine::Runtime
{
    Runtime::Runtime(const RuntimeConfig& config)
    {
        Platform::WindowDesc windowDesc;
        windowDesc.title = config.windowTitle;
        windowDesc.width = config.windowWidth;
        windowDesc.height = config.windowHeight;

        m_window = Platform::CreateAppWindow(windowDesc);

        // Window::ShouldClose() — checked directly in Run() — is the
        // single authoritative signal for ending the loop; this
        // callback never drives control flow itself, so there is
        // exactly one source of truth for "should the app end," not two
        // that could disagree.
        //
        // Every event is also forwarded to m_inputSystem.OnEvent()
        // unconditionally — this is the one place Platform's events
        // reach InputSystem. InputSystem itself never listens to
        // *m_window directly; Runtime is the "higher layer" the M7
        // brief describes deciding who consumes each event. OnEvent
        // silently ignores anything that isn't an input event (Close/
        // Resize included), so this ordering relative to the logging
        // below doesn't matter — see docs/ARCHITECTURE.md, "Event
        // Routing".
        m_window->SetEventCallback([this](Core::Event& event)
        {
            m_inputSystem.OnEvent(event);

            if (dynamic_cast<Platform::WindowCloseEvent*>(&event) != nullptr)
            {
                AR_LOG_INFO("Window close requested");
            }
            else if (auto* resize = dynamic_cast<Platform::WindowResizeEvent*>(&event))
            {
                AR_LOG_INFO(std::format("Window resized to {}x{}", resize->width, resize->height));
            }
        });

        // DesktopFrameDriver is constructed directly here rather than
        // through a factory: unlike Platform::Window (which hides
        // WindowsWindow behind CreateAppWindow specifically to keep
        // Win32 out of public headers), DesktopFrameDriver already lives
        // in this same module and leaks no platform-specific types, so
        // there is nothing for an extra layer of indirection to hide.
        // Swapping this one line for an XRFrameDriver later is the
        // entire cost of adding XR support to Runtime — Run()'s loop
        // below only ever calls through the Frame::FrameDriver
        // interface, so it would not need to change at all.
        m_frameDriver = std::make_unique<DesktopFrameDriver>(*m_window);

        // Same reasoning as DesktopFrameDriver above: constructed
        // directly via its factory (CreateNullRenderDevice, declared
        // alongside the concrete NullRenderDevice), stored as the
        // abstract Rendering::RenderDevice interface. Nothing past this
        // line ever names NullRenderDevice or touches its internals —
        // see docs/ARCHITECTURE.md, "M4 Implementation Notes".
        m_renderDevice = Rendering::CreateNullRenderDevice();

        // TEMPORARY (M4 validation only): a placeholder vertex buffer,
        // just large enough to be a valid buffer, that exists purely so
        // Run()'s dummy draw call has something valid to reference. Its
        // content is meaningless — nothing reads it, since there is no
        // real backend to read anything yet. Removed once Scene (M5)
        // provides real geometry.
        Rendering::BufferDesc dummyBufferDesc;
        dummyBufferDesc.sizeBytes = 12; // arbitrary non-zero size
        dummyBufferDesc.usage = Rendering::BufferUsage::Vertex;
        m_dummyVertexBuffer = m_renderDevice->CreateBuffer(dummyBufferDesc);

        // M7 validation: one action ("Select") with two independent
        // bindings, demonstrating that either can trigger the same
        // query — see docs/ARCHITECTURE.md, "Action Mapping
        // Semantics". Not gameplay architecture; just proof the seam
        // works.
        m_inputSystem.BindActionKey("Select", Core::KeyCode::Space);
        m_inputSystem.BindActionMouseButton("Select", Core::MouseButton::Left);

        AR_LOG_INFO("Runtime initialized");
    }

    Runtime::~Runtime()
    {
        // Explicit, while m_renderDevice is still alive, to exercise the
        // destroy path rather than relying only on NullRenderDevice's
        // own destructor to clean up. Harmless either way today (no real
        // GPU resource exists), but establishes the habit for when it
        // won't be.
        m_renderDevice->DestroyBuffer(m_dummyVertexBuffer);

        // Everything else is a std::unique_ptr and cleans itself up;
        // this destructor exists mainly to log the moment of shutdown.
        // Destruction order (render device, then frame driver, then
        // window) is guaranteed correct by member declaration order —
        // see the comment in Runtime.hpp.
        AR_LOG_INFO("Runtime shutting down");
    }

    void Runtime::Run()
    {
        AR_LOG_INFO("Runtime loop starting");

        // FPS is accumulated over roughly one second rather than logged
        // every frame, which would flood the console at hundreds or
        // thousands of frames per second.
        double fpsAccumulatedSeconds = 0.0;
        int fpsFrameCount = 0;

        while (true)
        {
            // InputSystem::BeginFrame() clears last frame's transient
            // Pressed/Released flags. It must run before PollEvents()
            // delivers this frame's new events — otherwise a flag this
            // frame's own event just set would be wiped out again
            // immediately, and nothing would ever observe it. Doing it
            // here means Pressed/Released set by the events processed
            // just below remain observable for the rest of this frame's
            // body (the demo logging, and everything after it), not
            // cleared until next iteration — see docs/ARCHITECTURE.md,
            // "Frame Lifecycle for Transient State".
            m_inputSystem.BeginFrame();

            // Process OS messages, then check ShouldClose() immediately
            // afterward — before doing any frame work. If a new frame
            // were started first and ShouldClose() only checked
            // afterward, closing the window would still cost one more
            // full frame of (currently pointless, since nothing renders
            // yet) work before the loop noticed. Checking right here
            // means a close request is acted on the moment it's known.
            m_window->PollEvents();
            if (m_window->ShouldClose())
            {
                break;
            }

            // M7 validation: minimal, deliberately non-flooding input
            // logging. WasKeyPressed/WasKeyReleased are true for
            // exactly one frame per physical transition (OS key-repeat
            // while holding Space does not retrigger this), which is
            // exactly what proves the frame lifecycle works — not raw
            // event logging, which would show repeat spam instead.
            if (m_inputSystem.WasKeyPressed(Core::KeyCode::Space))
            {
                AR_LOG_INFO("Space pressed");
            }
            if (m_inputSystem.WasKeyReleased(Core::KeyCode::Space))
            {
                AR_LOG_INFO("Space released");
            }
            if (m_inputSystem.WasMouseButtonPressed(Core::MouseButton::Left))
            {
                AR_LOG_INFO("Left mouse button pressed");
            }
            if (m_inputSystem.WasActionPressed("Select"))
            {
                AR_LOG_INFO("Select action pressed (Space or Left Mouse)");
            }

            // Frame lifecycle: PrepareFrame() blocks (if the driver
            // needs to) until it's time to prepare the next frame, and
            // reports whether a frame lifecycle should even happen this
            // tick at all (FrameStatus::Idle — e.g. an XR session that
            // isn't currently running — skips BeginFrame/GetViews/
            // EndFrame entirely, not just the render work below) or
            // whether the frame source can no longer produce frames at
            // all (FrameStatus::Stop). See docs/ARCHITECTURE.md, "Runtime
            // Loop Changes (M9E.5)".
            const Frame::FrameContext frameContext = m_frameDriver->PrepareFrame();
            if (frameContext.status == Frame::FrameStatus::Stop)
            {
                break;
            }
            if (frameContext.status == Frame::FrameStatus::Idle)
            {
                continue;
            }

            m_frameDriver->BeginFrame();

            // Application/update/render: gated on shouldRender, not on
            // FrameStatus — a Continue frame can still legitimately ask
            // to skip rendering its content (OpenXR's own
            // shouldRender=false while the session is running but not
            // currently visible/focused), while BeginFrame()/EndFrame()
            // still bracket it either way.
            if (frameContext.timing.shouldRender)
            {
                const std::vector<Frame::ViewInfo> views = m_frameDriver->GetViews();
                (void)views; // not consumed by rendering yet — no camera/Scene until M5+

                // "Update runtime/application state" belongs here once
                // there is any state to update (Scene, M5+).

                // TEMPORARY (M4 validation only): a hard-coded dummy draw
                // proving the Rendering seam works end to end (CreateBuffer
                // -> BeginRendering -> SubmitDraw -> EndRendering), with
                // nothing actually appearing on screen since NullRenderDevice
                // does no real graphics work. Scene (M5) replaces this with
                // real geometry submission driven by actual scene content.
                m_renderDevice->BeginRendering();
                Rendering::DrawCommand dummyDraw;
                dummyDraw.vertexBuffer = m_dummyVertexBuffer;
                dummyDraw.count = 3; // pretend one triangle
                m_renderDevice->SubmitDraw(dummyDraw);
                m_renderDevice->EndRendering();
            }

            // Timing accounting is unconditional — only the render work
            // above is gated on shouldRender.
            fpsAccumulatedSeconds += frameContext.timing.deltaTimeSeconds;
            ++fpsFrameCount;
            if (fpsAccumulatedSeconds >= 1.0)
            {
                const double fps = static_cast<double>(fpsFrameCount) / fpsAccumulatedSeconds;
                AR_LOG_INFO(std::format("FPS: {:.1f} ({} frames / {:.3f}s)",
                                        fps, fpsFrameCount, fpsAccumulatedSeconds));
                fpsAccumulatedSeconds = 0.0;
                fpsFrameCount = 0;
            }

            // Frame completion: called once per BeginFrame(), regardless
            // of shouldRender — some backends require a matching Begin/
            // End pair either way (OpenXR's xrBeginFrame/xrEndFrame).
            m_frameDriver->EndFrame();
        }

        AR_LOG_INFO("Runtime loop stopped");
    }
}
