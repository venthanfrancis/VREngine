#include "OpenXRVersion.hpp"

#include <format>

namespace AREngine::XR::OpenXR
{
    std::string FormatXrVersion(XrVersion packedVersion)
    {
        const XrVersionParts parts = DecodeXrVersion(packedVersion);
        return std::format("{}.{}.{}", parts.major, parts.minor, parts.patch);
    }
}
