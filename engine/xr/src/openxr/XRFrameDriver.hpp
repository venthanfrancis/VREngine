#pragma once

// Private OpenXR/Vulkan integration boundary - see
// OpenXRVulkanGraphicsBinding.hpp for the vulkan.h/openxr_platform.h
// include-order requirement this file also depends on transitively
// (through OpenXRSession.hpp).
//
// AREngine's first real Frame::FrameDriver implementation for OpenXR
// (M9E.5) - wraps xrWaitFrame/xrBeginFrame/xrEndFrame and the session-
// state event loop M9D/M9E already built. Deliberately does NOT own
// XR swapchain image acquisition (xrAcquireSwapchainImage/
// xrWaitSwapchainImage/xrReleaseSwapchainImage stay on OpenXRSwapchain,
// coordinated by whichever Renderer/XR-integration layer eventually
// owns real rendering - currently the manual frame demo plays that
// role), does NOT call xrLocateViews (GetViews() returns empty - real
// view data is M9F's job), and does NOT render anything. See
// docs/ARCHITECTURE.md, "XR Mapping (M9E.5)" and "Render-Target
// Acquisition Ownership (M9E.5)".
//
// Lives here (not in engine/frame/ or runtime/): needs OpenXRSession,
// which is only compiled when ARENGINE_ENABLE_VULKAN is also ON (it
// needs XrGraphicsBindingVulkan2KHR) - see engine/xr/CMakeLists.txt,
// this file is nested inside that same block alongside OpenXRSession/
// OpenXRSwapchain, even though XRFrameDriver itself makes no direct
// Vulkan API call.

#include "OpenXRSession.hpp"

#include "AREngine/Frame/FrameDriver.hpp"

#include <chrono>

namespace AREngine::XR::OpenXR
{
    // Not copyable or movable: exactly one XRFrameDriver per session,
    // same discipline as every other OpenXR wrapper in this codebase.
    class XRFrameDriver final : public Frame::FrameDriver
    {
    public:
        // Does not own `instance`/`session` - the caller must keep both
        // alive for this object's entire lifetime, same borrowing
        // discipline as every other OpenXR wrapper here.
        // `environmentBlendMode` is pre-selected by the caller (via
        // OpenXREnvironmentBlendMode.hpp's EnumerateEnvironmentBlendModes/
        // SelectEnvironmentBlendMode) - this class does not enumerate or
        // select one itself.
        XRFrameDriver(XrInstance instance, OpenXRSession& session,
                      XrViewConfigurationType primaryViewConfigurationType,
                      XrEnvironmentBlendMode environmentBlendMode);

        XRFrameDriver(const XRFrameDriver&) = delete;
        XRFrameDriver& operator=(const XRFrameDriver&) = delete;
        XRFrameDriver(XRFrameDriver&&) = delete;
        XRFrameDriver& operator=(XRFrameDriver&&) = delete;

        // Polls session-state events (reacting to every transition
        // observed this cycle, in order - see
        // OpenXRSessionState.hpp's DetermineSessionLifecycleActions),
        // then either returns FrameStatus::Stop (session reached a
        // terminal state), FrameStatus::Idle (session not currently
        // running - no real xrWaitFrame call is made; see the .cpp for
        // why this branch also sleeps briefly), or calls the real
        // xrWaitFrame and returns FrameStatus::Continue with real
        // timing/shouldRender data.
        Frame::FrameContext PrepareFrame() override;

        // Calls xrBeginFrame. Only valid after PrepareFrame() returned
        // FrameStatus::Continue (contract documented on FrameDriver
        // itself).
        void BeginFrame() override;

        // No real view data yet - xrLocateViews is deferred to M9F.
        // Always returns an empty vector, which is spec-legal per
        // Frame::ViewInfo's own "a frame may need zero, one, or several
        // of these" documentation.
        std::vector<Frame::ViewInfo> GetViews() override;

        // Calls xrEndFrame with zero composition layers (same decision
        // M9E made - no real per-view pose/FOV data exists yet to build
        // a valid XrCompositionLayerProjection, and this class does not
        // fabricate one). Only valid after PrepareFrame() returned
        // FrameStatus::Continue.
        void EndFrame() override;

        // Not part of the generic FrameDriver interface - requesting a
        // session exit is an application-level decision (e.g. "I've run
        // N diagnostic frames"), not something a generic frame-pacing
        // driver should decide on its own. Only reachable through a
        // concrete XRFrameDriver reference, never through a
        // Frame::FrameDriver* pointer.
        void RequestExit();

    private:
        XrInstance m_instance = XR_NULL_HANDLE; // borrowed, not owned
        OpenXRSession& m_session; // borrowed, not owned
        XrViewConfigurationType m_primaryViewConfigurationType;
        XrEnvironmentBlendMode m_environmentBlendMode;

        XrSessionState m_currentState = XR_SESSION_STATE_UNKNOWN;
        XrTime m_lastPredictedDisplayTime = 0;

        // Self-contained std::chrono timer, not Platform::SteadyClock -
        // engine/xr does not (and should not, just for ~10 lines of
        // std::chrono wrapping) depend on engine/platform. Same
        // "duplicate small self-contained logic rather than add a
        // cross-module dependency" precedent already used for
        // FindGraphicsQueueFamily/TransitionImageLayout elsewhere in
        // this module.
        std::chrono::steady_clock::time_point m_clockStart = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point m_lastTick = m_clockStart;
    };
}
