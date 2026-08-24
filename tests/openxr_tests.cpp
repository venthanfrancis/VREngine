// M9A automated tests for AREngine::XR::OpenXR's pure-logic helpers:
// version formatting, result-to-string fallback logic, and
// system-selection helper logic. Deliberately calls ZERO real OpenXR
// API functions (no xrCreateInstance, no xrGetSystem, ...) — only uses
// OpenXR's plain C structs/enums as synthetic test data, so this runs
// on any machine with the OpenXR headers available at compile time,
// without needing a real OpenXR runtime or headset at runtime. See
// docs/ARCHITECTURE.md, "M9A Implementation Notes".
//
// Real OpenXR bring-up (instance/system creation against a real
// loader/runtime) is exercised only by the separate, manual
// arengine_openxr_demo — not part of this suite, since CTest must not
// depend on an XR runtime or headset being present.

#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"

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

    using namespace AREngine::XR::OpenXR;

    void TestDecodeXrVersion()
    {
        const XrVersion packed = XR_MAKE_VERSION(1, 2, 3);
        const XrVersionParts parts = DecodeXrVersion(packed);
        Check(parts.major == 1, "DecodeXrVersion extracts the major version");
        Check(parts.minor == 2, "DecodeXrVersion extracts the minor version");
        Check(parts.patch == 3, "DecodeXrVersion extracts the patch version");
    }

    void TestFormatXrVersion()
    {
        // kTargetApiVersion's exact patch component tracks whichever
        // OpenXR header patch version this was built against (see
        // XR_API_VERSION_1_0's definition in openxr.h) - only major/
        // minor are meaningful to assert on here, not a hardcoded
        // patch number that would break on a future header bump.
        const XrVersionParts targetParts = DecodeXrVersion(kTargetApiVersion);
        Check(targetParts.major == 1 && targetParts.minor == 0,
              "AREngine's target OpenXR API version is 1.0.x (major.minor)");
        Check(FormatXrVersion(XR_MAKE_VERSION(1, 4, 9)) == "1.4.9",
              "FormatXrVersion formats an arbitrary version correctly");
    }

    void TestXrResultToReadableStringNumericFallback()
    {
        // No instance available - xrResultToString cannot be called, so
        // this must fall back to a numeric representation rather than
        // crashing or returning an empty string.
        const std::string readable = XrResultToReadableString(XR_NULL_HANDLE, XR_ERROR_RUNTIME_UNAVAILABLE);
        Check(readable == "XrResult(-51)",
              "XrResultToReadableString falls back to a numeric representation with no XrInstance available");
    }

    void TestXrResultToReadableStringSuccessFallback()
    {
        const std::string readable = XrResultToReadableString(XR_NULL_HANDLE, XR_SUCCESS);
        Check(readable == "XrResult(0)",
              "XrResultToReadableString's numeric fallback works for XR_SUCCESS (0) too, not just negative codes");
    }

    void TestIsFormFactorUnavailable()
    {
        Check(IsFormFactorUnavailable(XR_ERROR_FORM_FACTOR_UNAVAILABLE),
              "XR_ERROR_FORM_FACTOR_UNAVAILABLE is recognized as a form-factor-unavailable result");
        Check(IsFormFactorUnavailable(XR_ERROR_FORM_FACTOR_UNSUPPORTED),
              "XR_ERROR_FORM_FACTOR_UNSUPPORTED is recognized as a form-factor-unavailable result");
        Check(!IsFormFactorUnavailable(XR_ERROR_RUNTIME_UNAVAILABLE),
              "An unrelated error code (e.g. runtime unavailable) is NOT reported as form-factor-unavailable");
        Check(!IsFormFactorUnavailable(XR_SUCCESS),
              "XR_SUCCESS is not reported as a form-factor-unavailable failure");
    }

    XrExtensionProperties MakeExtension(const char* name)
    {
        XrExtensionProperties extension{XR_TYPE_EXTENSION_PROPERTIES};
        std::snprintf(extension.extensionName, sizeof(extension.extensionName), "%s", name);
        return extension;
    }

    void TestIsExtensionSupported()
    {
        const std::vector<XrExtensionProperties> extensions{
            MakeExtension("XR_KHR_composition_layer_depth"),
            MakeExtension("XR_KHR_vulkan_enable2"),
            MakeExtension("XR_EXT_hand_tracking"),
        };
        Check(IsExtensionSupported(extensions, "XR_KHR_vulkan_enable2"),
              "IsExtensionSupported finds an extension present in the list");
        Check(!IsExtensionSupported(extensions, "XR_KHR_vulkan_enable"),
              "IsExtensionSupported does not match a similarly-named but different extension (no substring match)");
        Check(!IsExtensionSupported({}, "XR_KHR_vulkan_enable2"),
              "IsExtensionSupported on an empty list returns false, not a crash");
    }
}

int main()
{
    TestDecodeXrVersion();
    TestFormatXrVersion();
    TestXrResultToReadableStringNumericFallback();
    TestXrResultToReadableStringSuccessFallback();
    TestIsFormFactorUnavailable();
    TestIsExtensionSupported();

    if (g_failureCount == 0)
    {
        std::printf("All OpenXR (pure-logic) M9A checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
