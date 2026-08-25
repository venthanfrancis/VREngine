#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: XrCompositionLayerProjection/
// XrCompositionLayerProjectionView/XrSwapchainSubImage are all core
// openxr.h types (not openxr_platform.h Vulkan-flavored ones) - the
// only Vulkan-adjacent thing this file ever touches is a bare
// XrSwapchain handle, an opaque OpenXR handle, not a Vulkan type.
// Building the composition-layer view array from real located views is
// therefore pure OpenXR logic, independent of which graphics API backs
// the session - see docs/ARCHITECTURE.md, "Projection-Layer
// Construction (M9F)".
//
// The smallest dedicated place for this logic to live: M9E.5
// established that XRFrameDriver stays a frame-lifecycle object (event
// processing, xrWaitFrame/xrBeginFrame/xrEndFrame, generic ViewInfo
// conversion) and does not own swapchain topology or render-target
// metadata. Building an XrCompositionLayerProjection needs exactly
// that render-target metadata (per-view XrSwapchainSubImage) alongside
// this frame's raw located XrView data - genuinely separate
// responsibility from frame pacing, so it lives in its own small class
// here rather than growing XRFrameDriver's scope. See
// docs/ARCHITECTURE.md, "Why OpenXRProjectionLayer Is Separate From
// XRFrameDriver (M9F)".

#include <openxr/openxr.h>

#include <cstdint>
#include <vector>

namespace AREngine::XR::OpenXR
{
    // Owns per-frame XrCompositionLayerProjectionView storage and the
    // XrCompositionLayerProjection built from it, given this frame's
    // raw located XrView data and a fixed, session-lifetime set of
    // per-view swapchain sub-image metadata. Never exposed through any
    // generic Frame API - this class, like XrView/XrFovf/XrPosef
    // themselves, is purely an XR-integration-layer concept.
    //
    // Ownership: does NOT own the XrSwapchain handles inside
    // `subImages` - those are borrowed from the caller's own
    // OpenXRSwapchain objects (M9E), which must outlive this object and
    // every Prepare() call made on it. An XrSwapchain is an opaque
    // OpenXR handle, not a C++ object with its own lifetime tracking -
    // the actual owning C++ object is OpenXRSwapchain, and it is
    // OpenXRSwapchain's destructor (xrDestroySwapchain) that this
    // object's borrowed handles must not outlive.
    class OpenXRProjectionLayer
    {
    public:
        // `subImages` - one per view, in view-index order, each
        // `{swapchain, imageRect covering the swapchain's full real
        // width/height, imageArrayIndex=0}` (M9E's one-swapchain-per-
        // view, arraySize=1 topology - see docs/ARCHITECTURE.md, "XR
        // Swapchain Topology (M9E)"). Fixed for this object's entire
        // lifetime, since the swapchains themselves don't change
        // within a session. `space` - the XrSpace to use as
        // XrCompositionLayerProjection::space (the caller's LOCAL
        // reference space for M9F).
        OpenXRProjectionLayer(std::vector<XrSwapchainSubImage> subImages, XrSpace space);

        // Rebuilds this frame's composition-layer view array from
        // `views` (this frame's real xrLocateViews result - the actual
        // runtime pose/FOV, never fabricated). Returns false (and
        // leaves Get() returning nullptr) if `views` is empty (no
        // rendering was requested, or view state was invalid this
        // frame - not an error) OR if `views.size()` does not match
        // the sub-image count this object was constructed with (a
        // genuine runtime inconsistency - logged as a clear error, not
        // indexed into blindly, and not merely asserted: a debug
        // assertion can be compiled out entirely in a Release build,
        // which would leave no check at all). Returns true only when a
        // valid layer covering every view was actually built.
        [[nodiscard]] bool Prepare(const std::vector<XrView>& views);

        // The layer prepared by the most recent Prepare() call, or
        // nullptr if that call returned false (or Prepare() has never
        // been called). Valid only until the next Prepare() call or
        // this object's destruction - the caller must submit it (via
        // XRFrameDriver::SetPendingProjectionLayer) before doing either.
        [[nodiscard]] const XrCompositionLayerProjection* Get() const;

    private:
        std::vector<XrSwapchainSubImage> m_subImages; // borrowed XrSwapchain handles inside - see class comment
        XrSpace m_space;
        std::vector<XrCompositionLayerProjectionView> m_projectionViews;
        XrCompositionLayerProjection m_projectionLayer{};
        bool m_hasLayer = false;
    };
}
