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

        // Logging only. Window::ShouldClose() — checked directly in
        // Run() — is the single authoritative signal for ending the
        // loop; this callback never drives control flow, so there is
        // exactly one source of truth for "should the app end," not two
        // that could disagree.
        m_window->SetEventCallback([](Core::Event& event)
        {
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

            const Frame::FrameTiming timing = m_frameDriver->WaitForNextFrame();
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

            fpsAccumulatedSeconds += timing.deltaTimeSeconds;
            ++fpsFrameCount;
            if (fpsAccumulatedSeconds >= 1.0)
            {
                const double fps = static_cast<double>(fpsFrameCount) / fpsAccumulatedSeconds;
                AR_LOG_INFO(std::format("FPS: {:.1f} ({} frames / {:.3f}s)",
                                        fps, fpsFrameCount, fpsAccumulatedSeconds));
                fpsAccumulatedSeconds = 0.0;
                fpsFrameCount = 0;
            }

            m_frameDriver->SubmitFrame();
        }

        AR_LOG_INFO("Runtime loop stopped");
    }
}
