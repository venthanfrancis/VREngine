#include "OpenXRActionSet.hpp"

#include "OpenXRResult.hpp"

#include <cstdio>

namespace AREngine::XR::OpenXR
{
    namespace
    {
        // XrActionSetCreateInfo's name fields are fixed-size char
        // arrays (XR_MAX_ACTION_SET_NAME_SIZE/XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE),
        // not std::string - std::snprintf truncates safely (never
        // overruns) rather than the unsafe strcpy/sprintf this
        // codebase avoids elsewhere.
        template <std::size_t N>
        void CopyBoundedString(char (&dest)[N], const char* src)
        {
            std::snprintf(dest, N, "%s", src);
        }
    }

    OpenXRActionSet::OpenXRActionSet(XrInstance instance, const char* name, const char* localizedName, std::uint32_t priority)
    {
        XrActionSetCreateInfo createInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        CopyBoundedString(createInfo.actionSetName, name);
        CopyBoundedString(createInfo.localizedActionSetName, localizedName);
        createInfo.priority = priority;
        CheckXrResult(instance, xrCreateActionSet(instance, &createInfo, &m_actionSet), "xrCreateActionSet");
    }

    OpenXRActionSet::~OpenXRActionSet()
    {
        if (m_actionSet != XR_NULL_HANDLE)
        {
            xrDestroyActionSet(m_actionSet);
            m_actionSet = XR_NULL_HANDLE;
        }
    }

    void OpenXRActionSet::Attach(XrInstance instance, XrSession session) const
    {
        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_actionSet;
        CheckXrResult(instance, xrAttachSessionActionSets(session, &attachInfo), "xrAttachSessionActionSets");
    }
}
