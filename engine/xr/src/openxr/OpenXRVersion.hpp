#pragma once

// Private OpenXR bring-up implementation — see OpenXRInstance.hpp.

#include <openxr/openxr.h>

#include <cstdint>
#include <string>

namespace AREngine::XR::OpenXR
{
    // AREngine's target OpenXR API version: 1.0 core (major.minor - the
    // only two components a runtime actually compatibility-checks
    // against). The installed/fetched headers are newer (1.1.x as of
    // M9A), but requesting whatever the newest header happens to
    // define would risk failing on any runtime that only implements
    // 1.0 core - the same "broad compatibility over the newest
    // available" reasoning M8A already applied when choosing Vulkan 1.2
    // over a newer target. See docs/ARCHITECTURE.md, "API Version
    // (M9A)".
    //
    // Note: XR_API_VERSION_1_0's patch component (as defined in
    // openxr.h) tracks whichever header patch version this was built
    // against, rather than being pinned to 0 - so FormatXrVersion(
    // kTargetApiVersion) prints e.g. "1.0.62", not "1.0.0". Only
    // major/minor are meaningful here; do not depend on the exact
    // patch value.
    inline constexpr XrVersion kTargetApiVersion = XR_API_VERSION_1_0;

    struct XrVersionParts
    {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint32_t patch = 0;
    };

    // Decodes a packed XrVersion (as returned by
    // XrInstanceProperties::runtimeVersion, or used to build
    // XrApplicationInfo::apiVersion) into its components. Pure bit
    // manipulation over the XR_VERSION_* macros - no OpenXR API calls,
    // so this is directly unit-testable without a loader, runtime, or
    // headset.
    [[nodiscard]] constexpr XrVersionParts DecodeXrVersion(XrVersion packedVersion)
    {
        return XrVersionParts{
            XR_VERSION_MAJOR(packedVersion),
            XR_VERSION_MINOR(packedVersion),
            XR_VERSION_PATCH(packedVersion)
        };
    }

    [[nodiscard]] std::string FormatXrVersion(XrVersion packedVersion);
}
