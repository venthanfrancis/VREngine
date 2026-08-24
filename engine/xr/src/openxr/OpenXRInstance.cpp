#include "OpenXRInstance.hpp"

#include "OpenXRResult.hpp"
#include "OpenXRVersion.hpp"

#include "AREngine/Core/Log.hpp"

#include <cstdio>
#include <cstring>

namespace AREngine::XR::OpenXR
{
    std::vector<XrApiLayerProperties> EnumerateApiLayers()
    {
        std::uint32_t count = 0;
        XrResult result = xrEnumerateApiLayerProperties(0, &count, nullptr);
        if (XR_FAILED(result))
        {
            AR_LOG_WARNING("xrEnumerateApiLayerProperties (count query) failed: "
                            + XrResultToReadableString(XR_NULL_HANDLE, result));
            return {};
        }

        std::vector<XrApiLayerProperties> layers(count, XrApiLayerProperties{XR_TYPE_API_LAYER_PROPERTIES});
        if (count == 0)
        {
            return layers;
        }

        result = xrEnumerateApiLayerProperties(count, &count, layers.data());
        if (XR_FAILED(result))
        {
            AR_LOG_WARNING("xrEnumerateApiLayerProperties (data query) failed: "
                            + XrResultToReadableString(XR_NULL_HANDLE, result));
            return {};
        }
        return layers;
    }

    std::vector<XrExtensionProperties> EnumerateInstanceExtensions()
    {
        std::uint32_t count = 0;
        XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr);
        if (XR_FAILED(result))
        {
            AR_LOG_WARNING("xrEnumerateInstanceExtensionProperties (count query) failed: "
                            + XrResultToReadableString(XR_NULL_HANDLE, result));
            return {};
        }

        std::vector<XrExtensionProperties> extensions(count, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
        if (count == 0)
        {
            return extensions;
        }

        result = xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data());
        if (XR_FAILED(result))
        {
            AR_LOG_WARNING("xrEnumerateInstanceExtensionProperties (data query) failed: "
                            + XrResultToReadableString(XR_NULL_HANDLE, result));
            return {};
        }
        return extensions;
    }

    bool IsExtensionSupported(const std::vector<XrExtensionProperties>& extensions, const char* name)
    {
        for (const XrExtensionProperties& extension : extensions)
        {
            if (std::strcmp(extension.extensionName, name) == 0)
            {
                return true;
            }
        }
        return false;
    }

    namespace
    {
        // applicationVersion/engineVersion (below) are plain,
        // application-defined uint32_t values - unlike apiVersion,
        // OpenXR does not interpret them (XR_MAKE_VERSION produces a
        // 64-bit XrVersion, which is the wrong type for these two
        // fields). Packed here as major*10000 + minor*100 + patch,
        // matching AREngine's 0.1.0 project version, purely for
        // diagnostic/logging purposes on the runtime side.
        constexpr std::uint32_t kAppEngineVersion = 0 * 10000 + 1 * 100 + 0; // 0.1.0
    }

    OpenXRInstance::OpenXRInstance(std::span<const char* const> requestedExtensions)
    {
        XrApplicationInfo appInfo{};
        std::snprintf(appInfo.applicationName, sizeof(appInfo.applicationName), "AREngine OpenXR Demo");
        appInfo.applicationVersion = kAppEngineVersion;
        std::snprintf(appInfo.engineName, sizeof(appInfo.engineName), "AREngine");
        appInfo.engineVersion = kAppEngineVersion;
        appInfo.apiVersion = kTargetApiVersion;

        XrInstanceCreateInfo createInfo{};
        createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
        createInfo.applicationInfo = appInfo;
        // enabledApiLayerCount left at zero - see the class comment in
        // OpenXRInstance.hpp. enabledExtensionNames/Count come directly
        // from the caller's already-verified list (M9A's default: none).
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requestedExtensions.size());
        createInfo.enabledExtensionNames = requestedExtensions.data();

        m_creationResult = xrCreateInstance(&createInfo, &m_instance);
        if (XR_FAILED(m_creationResult))
        {
            // Deliberately not logged/asserted here - see
            // docs/ARCHITECTURE.md, "Instance Creation Failure Handling
            // (M9A)": the caller has the context to know whether this
            // is expected (no runtime installed) or surprising, and to
            // produce the right user-facing message.
            m_instance = XR_NULL_HANDLE;
        }
    }

    OpenXRInstance::~OpenXRInstance()
    {
        if (m_instance != XR_NULL_HANDLE)
        {
            xrDestroyInstance(m_instance);
            m_instance = XR_NULL_HANDLE;
        }
    }
}
