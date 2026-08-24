#include "OpenXREnvironmentBlendMode.hpp"

#include "OpenXRResult.hpp"

#include <cstdint>

namespace AREngine::XR::OpenXR
{
    std::vector<XrEnvironmentBlendMode> EnumerateEnvironmentBlendModes(
        XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType)
    {
        std::uint32_t count = 0;
        CheckXrResult(instance,
            xrEnumerateEnvironmentBlendModes(instance, systemId, viewConfigurationType, 0, &count, nullptr),
            "xrEnumerateEnvironmentBlendModes (count query)");

        std::vector<XrEnvironmentBlendMode> modes(count);
        if (count == 0)
        {
            return modes;
        }

        CheckXrResult(instance,
            xrEnumerateEnvironmentBlendModes(instance, systemId, viewConfigurationType, count, &count, modes.data()),
            "xrEnumerateEnvironmentBlendModes (data query)");
        return modes;
    }

    bool IsEnvironmentBlendModeSupported(const std::vector<XrEnvironmentBlendMode>& supported, XrEnvironmentBlendMode mode)
    {
        for (const XrEnvironmentBlendMode candidate : supported)
        {
            if (candidate == mode)
            {
                return true;
            }
        }
        return false;
    }

    std::optional<XrEnvironmentBlendMode> SelectEnvironmentBlendMode(const std::vector<XrEnvironmentBlendMode>& supported)
    {
        if (IsEnvironmentBlendModeSupported(supported, XR_ENVIRONMENT_BLEND_MODE_OPAQUE))
        {
            return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        }
        if (IsEnvironmentBlendModeSupported(supported, XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND))
        {
            return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
        }
        if (IsEnvironmentBlendModeSupported(supported, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE))
        {
            return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
        }
        return std::nullopt;
    }

    const char* EnvironmentBlendModeToString(XrEnvironmentBlendMode mode)
    {
        switch (mode)
        {
            case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:      return "OPAQUE";
            case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:    return "ADDITIVE";
            case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND: return "ALPHA_BLEND";
            default:                                    return "OTHER";
        }
    }
}
