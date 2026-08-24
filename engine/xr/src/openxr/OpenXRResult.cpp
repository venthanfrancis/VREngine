#include "OpenXRResult.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

#include <format>

namespace AREngine::XR::OpenXR
{
    std::string XrResultToReadableString(XrInstance instance, XrResult result)
    {
        if (instance != XR_NULL_HANDLE)
        {
            char buffer[XR_MAX_RESULT_STRING_SIZE] = {};
            if (xrResultToString(instance, result, buffer) == XR_SUCCESS)
            {
                return std::string(buffer);
            }
        }
        return std::format("XrResult({})", static_cast<int>(result));
    }

    void CheckXrResult(XrInstance instance, XrResult result, const char* operationDescription)
    {
        if (XR_FAILED(result))
        {
            const std::string message = std::format(
                "{} failed: {}", operationDescription, XrResultToReadableString(instance, result));
            AR_LOG_ERROR(message);
            AR_ASSERT_MSG(false, message.c_str());
        }
    }
}
