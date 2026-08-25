// M9F automated tests for AREngine::XR::OpenXR's real-view-location
// pure-logic helpers: XrPosef/XrFovf -> Frame::ViewInfo conversion
// (OpenXRViewConversion.hpp) and composition-layer preparation
// (OpenXRProjectionLayer.hpp). Deliberately calls ZERO real OpenXR API
// functions (no xrLocateViews, no xrCreateSession, ...) and has ZERO
// Vulkan dependency (XrPosef/XrFovf/XrView/XrCompositionLayerProjection*/
// XrSwapchainSubImage are all core openxr.h types, not Vulkan-flavored
// ones), so this runs on any machine with the OpenXR headers available
// at compile time, without needing a real OpenXR runtime, headset, GPU,
// or even Vulkan enabled.
//
// Real xrLocateViews / composition-layer submission against a real
// loader/runtime is exercised only by the separate, manual
// arengine_openxr_frame_demo — not part of this suite, since CTest must
// not depend on an XR runtime or headset being present.

#include "openxr/OpenXRProjectionLayer.hpp"
#include "openxr/OpenXRViewConversion.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    void CheckNearlyEqual(float actual, float expected, const char* description)
    {
        constexpr float kEpsilon = 0.0001f;
        Check(std::abs(actual - expected) < kEpsilon, description);
    }

    using namespace AREngine::XR::OpenXR;
    using namespace AREngine::Core::Math;

    // --- ConvertXrPosition / ConvertXrOrientation ---

    void TestConvertXrPositionIdentity()
    {
        const Vec3 result = ConvertXrPosition(XrVector3f{0.0f, 0.0f, 0.0f});
        Check(result == Vec3(0.0f, 0.0f, 0.0f), "ConvertXrPosition: origin maps to origin");
    }

    void TestConvertXrPositionTranslated()
    {
        // No axis flip anywhere - OpenXR and AREngine share the same
        // right-handed, +Y-up, -Z-forward, meters convention (verified
        // against the OpenXR specification's own coordinate-system
        // definition before writing this function - see
        // OpenXRViewConversion.hpp). A straight field-for-field copy
        // must therefore preserve every component's sign exactly.
        const Vec3 result = ConvertXrPosition(XrVector3f{1.5f, 2.5f, -3.5f});
        Check(result == Vec3(1.5f, 2.5f, -3.5f), "ConvertXrPosition: translated position preserves every component's sign (+X right, +Y up, -Z forward, no axis flip)");
    }

    void TestConvertXrOrientationIdentity()
    {
        const Quaternion result = ConvertXrOrientation(XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f});
        Check(result == Quaternion::Identity(), "ConvertXrOrientation: XrQuaternionf identity (0,0,0,1) maps to Quaternion identity");
    }

    void TestConvertXrOrientationFieldReorder()
    {
        // Deliberately per-component-asymmetric (every component a
        // different value) so a field-reorder bug would actually be
        // caught - an identity-only test could pass even with x/y/z/w
        // swapped in the wrong place. XrQuaternionf stores {x,y,z,w};
        // AREngine's Quaternion constructor takes (w,x,y,z) - this test
        // confirms ConvertXrOrientation performs exactly that reorder,
        // not a coordinate-system conversion (see OpenXRViewConversion.hpp).
        const XrQuaternionf xr{0.1f, 0.2f, 0.3f, 0.9f}; // x=0.1, y=0.2, z=0.3, w=0.9
        const Quaternion result = ConvertXrOrientation(xr);
        CheckNearlyEqual(result.w, 0.9f, "ConvertXrOrientation: XrQuaternionf.w maps to Quaternion.w");
        CheckNearlyEqual(result.x, 0.1f, "ConvertXrOrientation: XrQuaternionf.x maps to Quaternion.x");
        CheckNearlyEqual(result.y, 0.2f, "ConvertXrOrientation: XrQuaternionf.y maps to Quaternion.y");
        CheckNearlyEqual(result.z, 0.3f, "ConvertXrOrientation: XrQuaternionf.z maps to Quaternion.z");
    }

    // --- ConvertXrViewToViewInfo ---

    void TestConvertXrViewToViewInfoPoseAndProjection()
    {
        XrView view{};
        view.type = XR_TYPE_VIEW;
        view.pose.position = XrVector3f{0.0f, 1.6f, 0.0f}; // roughly standing head height
        view.pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f}; // identity
        view.fov.angleLeft = -0.5f;
        view.fov.angleRight = 0.6f;
        view.fov.angleUp = 0.45f;
        view.fov.angleDown = -0.4f;

        const auto info = ConvertXrViewToViewInfo(view, 0.1f, 100.0f);
        Check(info.position == Vec3(0.0f, 1.6f, 0.0f), "ConvertXrViewToViewInfo: position carried through unchanged");
        Check(info.orientation == Quaternion::Identity(), "ConvertXrViewToViewInfo: identity orientation carried through unchanged");

        // The projection must be the genuinely asymmetric one built
        // from this view's own FOV, not a symmetric approximation -
        // confirmed by checking the off-center skew terms are non-zero
        // (matches PerspectiveOffCenterRH_ZO's own tests in
        // core_tests.cpp; this test just confirms the real FOV values
        // actually reach the projection, unmodified).
        Check(info.projection.At(0, 2) != 0.0f, "ConvertXrViewToViewInfo: asymmetric horizontal FOV reaches the projection matrix");
        Check(info.projection.At(1, 2) != 0.0f, "ConvertXrViewToViewInfo: asymmetric vertical FOV reaches the projection matrix");
    }

    // --- IsViewStateValid ---

    void TestIsViewStateValidBothBitsSet()
    {
        constexpr XrViewStateFlags flags = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        Check(IsViewStateValid(flags), "IsViewStateValid: both required bits set is valid");
    }

    void TestIsViewStateValidMissingOrientation()
    {
        constexpr XrViewStateFlags flags = XR_VIEW_STATE_POSITION_VALID_BIT;
        Check(!IsViewStateValid(flags), "IsViewStateValid: missing ORIENTATION_VALID_BIT is invalid");
    }

    void TestIsViewStateValidMissingPosition()
    {
        constexpr XrViewStateFlags flags = XR_VIEW_STATE_ORIENTATION_VALID_BIT;
        Check(!IsViewStateValid(flags), "IsViewStateValid: missing POSITION_VALID_BIT is invalid");
    }

    void TestIsViewStateValidTrackedBitsIrrelevant()
    {
        // VALID-but-not-TRACKED is legitimate per the OpenXR spec (a
        // "last known good" pose during a brief tracking interruption)
        // - TRACKED bits must not be required.
        constexpr XrViewStateFlags flags = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        Check(IsViewStateValid(flags), "IsViewStateValid: valid without tracked bits is still valid");
    }

    void TestIsViewStateValidEmpty()
    {
        Check(!IsViewStateValid(0), "IsViewStateValid: no flags set is invalid");
    }

    // --- OpenXRProjectionLayer ---

    XrSwapchainSubImage MakeSubImage(std::int32_t width, std::int32_t height)
    {
        XrSwapchainSubImage subImage{};
        subImage.swapchain = reinterpret_cast<XrSwapchain>(0x1234); // fake, never dereferenced by pure logic
        subImage.imageRect.offset = {0, 0};
        subImage.imageRect.extent = {width, height};
        subImage.imageArrayIndex = 0;
        return subImage;
    }

    XrView MakeView(float x)
    {
        XrView view{};
        view.type = XR_TYPE_VIEW;
        view.pose.position = XrVector3f{x, 0.0f, 0.0f};
        view.pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};
        view.fov = XrFovf{-0.5f, 0.5f, 0.5f, -0.5f};
        return view;
    }

    void TestOpenXRProjectionLayerEmptyViewsProducesNoLayer()
    {
        std::vector<XrSwapchainSubImage> subImages{MakeSubImage(100, 100), MakeSubImage(100, 100)};
        OpenXRProjectionLayer layer(subImages, reinterpret_cast<XrSpace>(0x5678));

        Check(!layer.Prepare({}), "OpenXRProjectionLayer::Prepare: empty views returns false");
        Check(layer.Get() == nullptr, "OpenXRProjectionLayer::Get: nullptr after an empty-views Prepare()");
    }

    void TestOpenXRProjectionLayerCountMismatchProducesNoLayer()
    {
        // Exactly the runtime-inconsistency scenario the brief requires
        // a clear diagnostic (not a compiled-out debug assertion) for -
        // one sub-image configured, two views located.
        std::vector<XrSwapchainSubImage> subImages{MakeSubImage(100, 100)};
        OpenXRProjectionLayer layer(subImages, reinterpret_cast<XrSpace>(0x5678));

        const std::vector<XrView> views{MakeView(-0.1f), MakeView(0.1f)};
        Check(!layer.Prepare(views), "OpenXRProjectionLayer::Prepare: view/sub-image count mismatch returns false, not a crash");
        Check(layer.Get() == nullptr, "OpenXRProjectionLayer::Get: nullptr after a mismatched Prepare()");
    }

    void TestOpenXRProjectionLayerMatchingCountProducesValidLayer()
    {
        std::vector<XrSwapchainSubImage> subImages{MakeSubImage(200, 300), MakeSubImage(200, 300)};
        const XrSpace space = reinterpret_cast<XrSpace>(0x5678);
        OpenXRProjectionLayer layer(subImages, space);

        const std::vector<XrView> views{MakeView(-0.1f), MakeView(0.1f)};
        Check(layer.Prepare(views), "OpenXRProjectionLayer::Prepare: matching view/sub-image count returns true");

        const XrCompositionLayerProjection* result = layer.Get();
        Check(result != nullptr, "OpenXRProjectionLayer::Get: non-null after a successful Prepare()");
        if (result != nullptr)
        {
            Check(result->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION, "OpenXRProjectionLayer: layer has the correct XrStructureType");
            Check(result->space == space, "OpenXRProjectionLayer: layer uses the configured XrSpace");
            Check(result->viewCount == 2, "OpenXRProjectionLayer: layer's viewCount matches the located view count");
            Check(result->views != nullptr, "OpenXRProjectionLayer: layer's views pointer is populated");
            if (result->views != nullptr)
            {
                Check(result->views[0].pose.position.x == -0.1f, "OpenXRProjectionLayer: view 0's pose comes from the real located view, not fabricated");
                Check(result->views[1].pose.position.x == 0.1f, "OpenXRProjectionLayer: view 1's pose comes from the real located view, not fabricated");
                Check(result->views[0].subImage.imageRect.extent.width == 200, "OpenXRProjectionLayer: view 0's subImage comes from the configured sub-image metadata");
            }
        }
    }

    void TestOpenXRProjectionLayerRepreparingReplacesPreviousLayer()
    {
        // A mismatched/empty Prepare() after a previously successful
        // one must clear the stale layer, not leave Get() returning
        // last frame's data.
        std::vector<XrSwapchainSubImage> subImages{MakeSubImage(100, 100)};
        OpenXRProjectionLayer layer(subImages, reinterpret_cast<XrSpace>(0x5678));

        Check(layer.Prepare({MakeView(0.0f)}), "OpenXRProjectionLayer::Prepare: first call with a matching view succeeds");
        Check(layer.Get() != nullptr, "OpenXRProjectionLayer::Get: non-null after the first successful Prepare()");

        Check(!layer.Prepare({}), "OpenXRProjectionLayer::Prepare: second call with no views fails");
        Check(layer.Get() == nullptr, "OpenXRProjectionLayer::Get: nullptr after the second, empty-views Prepare() - stale layer not reused");
    }
}

int main()
{
    TestConvertXrPositionIdentity();
    TestConvertXrPositionTranslated();
    TestConvertXrOrientationIdentity();
    TestConvertXrOrientationFieldReorder();

    TestConvertXrViewToViewInfoPoseAndProjection();

    TestIsViewStateValidBothBitsSet();
    TestIsViewStateValidMissingOrientation();
    TestIsViewStateValidMissingPosition();
    TestIsViewStateValidTrackedBitsIrrelevant();
    TestIsViewStateValidEmpty();

    TestOpenXRProjectionLayerEmptyViewsProducesNoLayer();
    TestOpenXRProjectionLayerCountMismatchProducesNoLayer();
    TestOpenXRProjectionLayerMatchingCountProducesValidLayer();
    TestOpenXRProjectionLayerRepreparingReplacesPreviousLayer();

    if (g_failureCount == 0)
    {
        std::printf("All OpenXR view (pure-logic) M9F checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
