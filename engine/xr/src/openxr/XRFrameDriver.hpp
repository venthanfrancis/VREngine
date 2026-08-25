#pragma once

// Private OpenXR/Vulkan integration boundary - see
// OpenXRVulkanGraphicsBinding.hpp for the vulkan.h/openxr_platform.h
// include-order requirement this file also depends on transitively
// (through OpenXRSession.hpp).
//
// AREngine's first real Frame::FrameDriver implementation for OpenXR
// (M9E.5) - wraps xrWaitFrame/xrBeginFrame/xrEndFrame, the session-
// state event loop M9D/M9E already built, and (M9F) real xrLocateViews
// -> generic Frame::ViewInfo conversion. Deliberately does NOT own XR
// swapchain image acquisition (xrAcquireSwapchainImage/
// xrWaitSwapchainImage/xrReleaseSwapchainImage stay on OpenXRSwapchain,
// coordinated by whichever Renderer/XR-integration layer eventually
// owns real rendering - currently the manual frame demo plays that
// role) and does NOT own swapchain topology or composition-layer
// render-target metadata either (that lives in OpenXRProjectionLayer,
// M9F - kept deliberately separate so this class stays focused on
// frame pacing/timing/view-location, not resource bookkeeping; see
// docs/ARCHITECTURE.md, "Why OpenXRProjectionLayer Is Separate From
// XRFrameDriver (M9F)"). Still does NOT render anything.
//
// Lives here (not in engine/frame/ or runtime/): needs OpenXRSession,
// which is only compiled when ARENGINE_ENABLE_VULKAN is also ON (it
// needs XrGraphicsBindingVulkan2KHR) - see engine/xr/CMakeLists.txt,
// this file is nested inside that same block alongside OpenXRSession/
// OpenXRSwapchain, even though XRFrameDriver itself makes no direct
// Vulkan API call.

#include "OpenXRSession.hpp"
#include "OpenXRReferenceSpace.hpp"

#include "AREngine/Frame/FrameDriver.hpp"

#include <chrono>

namespace AREngine::XR::OpenXR
{
    // Not copyable or movable: exactly one XRFrameDriver per session,
    // same discipline as every other OpenXR wrapper in this codebase.
    class XRFrameDriver final : public Frame::FrameDriver
    {
    public:
        // Does not own `instance`/`session`/`localSpace` - the caller
        // must keep all three alive for this object's entire lifetime,
        // same borrowing discipline as every other OpenXR wrapper here.
        // `environmentBlendMode` is pre-selected by the caller (via
        // OpenXREnvironmentBlendMode.hpp's EnumerateEnvironmentBlendModes/
        // SelectEnvironmentBlendMode) - this class does not enumerate or
        // select one itself. `localSpace` is used as xrLocateViews's own
        // `space` parameter (M9F uses LOCAL only - see
        // docs/ARCHITECTURE.md, "LOCAL Reference-Space Use (M9F)"; this
        // is not AREngine's final AR world-origin policy). `nearZ`/
        // `farZ` are engine/application clip-distance policy - OpenXR's
        // XrFovf provides angular FOV only, never near/far distances.
        XRFrameDriver(XrInstance instance, OpenXRSession& session, OpenXRReferenceSpace& localSpace,
                      XrViewConfigurationType primaryViewConfigurationType,
                      XrEnvironmentBlendMode environmentBlendMode,
                      float nearZ = 0.05f, float farZ = 100.0f);

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

        // Calls xrLocateViews at the CURRENT frame's predicted display
        // time (the value this same frame's PrepareFrame() stashed from
        // its own xrWaitFrame call - never wall-clock time, never a
        // previous frame's time), using `m_localSpace`. Converts each
        // located view into a generic Frame::ViewInfo via
        // OpenXRViewConversion.hpp - never a fabricated/placeholder
        // pose or FOV. If the runtime reports required pose data as
        // invalid (XrViewState's ORIENTATION_VALID_BIT/POSITION_VALID_BIT
        // - see IsViewStateValid), returns an empty vector rather than
        // inventing an identity head pose to hide the condition (logged
        // only on a valid<->invalid transition, not every frame - see
        // the .cpp). Also stashes the raw XrView data privately (see
        // GetLastLocatedXrViews() below) for XR-specific composition-
        // layer construction, which does not round-trip back through
        // ViewInfo. Only valid after PrepareFrame() returned
        // FrameStatus::Continue and BeginFrame() has been called.
        std::vector<Frame::ViewInfo> GetViews() override;

        // Calls xrEndFrame, submitting whatever layer (if any) the most
        // recent SetPendingProjectionLayer() call configured - the
        // pending layer is consumed and reset to none by this call, so
        // a tick that never calls SetPendingProjectionLayer() (e.g.
        // shouldRender was false, or GetViews() found no valid views)
        // safely submits zero layers by default. Only valid after
        // PrepareFrame() returned FrameStatus::Continue.
        void EndFrame() override;

        // Not part of the generic FrameDriver interface - requesting a
        // session exit is an application-level decision (e.g. "I've run
        // N diagnostic frames"), not something a generic frame-pacing
        // driver should decide on its own. Only reachable through a
        // concrete XRFrameDriver reference, never through a
        // Frame::FrameDriver* pointer.
        void RequestExit();

        // Not part of the generic FrameDriver interface. The raw
        // XrView data (real runtime pose/FOV) from the most recent
        // GetViews() call - empty if GetViews() has not been called
        // this frame, or found no valid views. Exists specifically so
        // XR-only composition-layer construction (OpenXRProjectionLayer)
        // can use the exact runtime data GetViews() already located,
        // without round-tripping XrView -> ViewInfo -> reconstructed
        // XrView (ViewInfo's generic shape does not, and should not,
        // carry every OpenXR-specific field XrCompositionLayerProjectionView
        // needs). See docs/ARCHITECTURE.md, "No XrView/ViewInfo
        // Round-Trip (M9F)".
        [[nodiscard]] const std::vector<XrView>& GetLastLocatedXrViews() const { return m_lastLocatedViews; }

        // Not part of the generic FrameDriver interface. Configures the
        // composition layer this frame's EndFrame() call should submit
        // - `layer` may be nullptr (submit zero layers this frame,
        // e.g. no valid views were located). Does not take ownership;
        // the pointee must remain valid only until the EndFrame() call
        // that consumes it, after which the pending layer is reset to
        // nullptr regardless. Keeps FrameDriver::EndFrame() itself
        // free of any OpenXR type - this is the "minimal XR-private
        // seam" the XR integration layer uses to hand prepared
        // composition state to a generic, unchanged EndFrame() call.
        // See docs/ARCHITECTURE.md, "Composition-Layer Seam (M9F)".
        void SetPendingProjectionLayer(const XrCompositionLayerProjection* layer) { m_pendingProjectionLayer = layer; }

    private:
        XrInstance m_instance = XR_NULL_HANDLE; // borrowed, not owned
        OpenXRSession& m_session; // borrowed, not owned
        OpenXRReferenceSpace& m_localSpace; // borrowed, not owned
        XrViewConfigurationType m_primaryViewConfigurationType;
        XrEnvironmentBlendMode m_environmentBlendMode;
        float m_nearZ;
        float m_farZ;

        XrSessionState m_currentState = XR_SESSION_STATE_UNKNOWN;
        XrTime m_lastPredictedDisplayTime = 0;

        std::vector<XrView> m_lastLocatedViews; // see GetLastLocatedXrViews()
        bool m_lastViewStateValid = true; // for log-on-transition-only, not every frame
        const XrCompositionLayerProjection* m_pendingProjectionLayer = nullptr; // borrowed, see SetPendingProjectionLayer()

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
