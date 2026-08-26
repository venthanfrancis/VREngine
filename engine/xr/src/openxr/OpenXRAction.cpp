#include "OpenXRAction.hpp"

#include "OpenXRResult.hpp"

#include <cstdio>

namespace AREngine::XR::OpenXR
{
    namespace
    {
        // Same bounded-copy discipline as OpenXRActionSet.cpp - see that
        // file's own comment for why std::snprintf, not strcpy/sprintf.
        template <std::size_t N>
        void CopyBoundedString(char (&dest)[N], const char* src)
        {
            std::snprintf(dest, N, "%s", src);
        }
    }

    OpenXRAction::OpenXRAction(XrInstance instance, XrActionSet actionSet, const char* name, const char* localizedName,
                                XrActionType type, const std::vector<XrPath>& subactionPaths)
    {
        XrActionCreateInfo createInfo{XR_TYPE_ACTION_CREATE_INFO};
        CopyBoundedString(createInfo.actionName, name);
        CopyBoundedString(createInfo.localizedActionName, localizedName);
        createInfo.actionType = type;
        createInfo.countSubactionPaths = static_cast<std::uint32_t>(subactionPaths.size());
        createInfo.subactionPaths = subactionPaths.empty() ? nullptr : subactionPaths.data();
        CheckXrResult(instance, xrCreateAction(actionSet, &createInfo, &m_action), "xrCreateAction");
    }

    OpenXRAction::~OpenXRAction()
    {
        if (m_action != XR_NULL_HANDLE)
        {
            xrDestroyAction(m_action);
            m_action = XR_NULL_HANDLE;
        }
    }
}
