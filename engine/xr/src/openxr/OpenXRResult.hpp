#pragma once

// Private OpenXR bring-up implementation — see OpenXRInstance.hpp.

#include <openxr/openxr.h>

#include <string>

namespace AREngine::XR::OpenXR
{
    // A human-readable name for an XrResult (e.g.
    // "XR_ERROR_RUNTIME_UNAVAILABLE"), for logging. `instance` may be
    // XR_NULL_HANDLE - xrResultToString itself requires a valid
    // instance to call, so before one exists (most notably, when
    // xrCreateInstance itself is the call that failed) this falls back
    // to a numeric representation instead. Once an instance exists,
    // this defers to the real xrResultToString, which covers every
    // XrResult (including vendor/extension-defined ones) far more
    // completely than a hand-written switch could. See
    // docs/ARCHITECTURE.md, "Error Handling (M9A)".
    [[nodiscard]] std::string XrResultToReadableString(XrInstance instance, XrResult result);

    // Logs which operation failed and why (via AR_LOG_ERROR), then
    // asserts - the same fatal-on-bring-up-failure policy M8A's
    // CheckVkResult already established, for calls where failure truly
    // is unexpected (e.g. xrGetInstanceProperties failing right after a
    // successful xrCreateInstance). Deliberately NOT used for
    // xrCreateInstance or xrGetSystem, whose failure can be a
    // completely normal outcome (no runtime installed, no HMD
    // connected) rather than a bug - see OpenXRInstance.hpp and
    // OpenXRSystem.hpp, and docs/ARCHITECTURE.md, "Instance Creation
    // Failure Handling (M9A)" / "System Selection (M9A)".
    void CheckXrResult(XrInstance instance, XrResult result, const char* operationDescription);
}
