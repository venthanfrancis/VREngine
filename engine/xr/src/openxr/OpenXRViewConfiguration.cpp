#include "OpenXRViewConfiguration.hpp"

#include "OpenXRResult.hpp"

#include <cstdint>

namespace AREngine::XR::OpenXR
{
    std::vector<XrViewConfigurationType> EnumerateViewConfigurationTypes(XrInstance instance, XrSystemId systemId)
    {
        std::uint32_t count = 0;
        CheckXrResult(instance,
            xrEnumerateViewConfigurations(instance, systemId, 0, &count, nullptr),
            "xrEnumerateViewConfigurations (count query)");

        std::vector<XrViewConfigurationType> types(count);
        if (count == 0)
        {
            return types;
        }

        CheckXrResult(instance,
            xrEnumerateViewConfigurations(instance, systemId, count, &count, types.data()),
            "xrEnumerateViewConfigurations (data query)");
        return types;
    }

    bool IsViewConfigurationTypeSupported(const std::vector<XrViewConfigurationType>& supported, XrViewConfigurationType type)
    {
        for (const XrViewConfigurationType candidate : supported)
        {
            if (candidate == type)
            {
                return true;
            }
        }
        return false;
    }

    std::optional<XrViewConfigurationType> SelectPrimaryViewConfigurationType(const std::vector<XrViewConfigurationType>& supported)
    {
        if (IsViewConfigurationTypeSupported(supported, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO))
        {
            return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        }
        return std::nullopt;
    }

    XrViewConfigurationProperties GetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType type)
    {
        XrViewConfigurationProperties properties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
        CheckXrResult(instance,
            xrGetViewConfigurationProperties(instance, systemId, type, &properties),
            "xrGetViewConfigurationProperties");
        return properties;
    }

    std::vector<XrViewConfigurationView> EnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType type)
    {
        std::uint32_t count = 0;
        CheckXrResult(instance,
            xrEnumerateViewConfigurationViews(instance, systemId, type, 0, &count, nullptr),
            "xrEnumerateViewConfigurationViews (count query)");

        std::vector<XrViewConfigurationView> views(count, XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW});
        if (count == 0)
        {
            return views;
        }

        CheckXrResult(instance,
            xrEnumerateViewConfigurationViews(instance, systemId, type, count, &count, views.data()),
            "xrEnumerateViewConfigurationViews (data query)");
        return views;
    }
}
