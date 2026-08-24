#include "OpenXRSystem.hpp"

namespace AREngine::XR::OpenXR
{
    SystemRequestResult TryGetHmdSystem(XrInstance instance)
    {
        XrSystemGetInfo getInfo{};
        getInfo.type = XR_TYPE_SYSTEM_GET_INFO;
        getInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        SystemRequestResult result;
        result.rawResult = xrGetSystem(instance, &getInfo, &result.systemId);
        result.found = (result.rawResult == XR_SUCCESS);
        if (!result.found)
        {
            result.systemId = XR_NULL_SYSTEM_ID;
        }
        return result;
    }
}
