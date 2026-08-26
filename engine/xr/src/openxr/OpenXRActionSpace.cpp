#include "OpenXRActionSpace.hpp"

#include "OpenXRResult.hpp"

namespace AREngine::XR::OpenXR
{
    OpenXRActionSpace::OpenXRActionSpace(XrInstance instance, XrSession session, XrAction action, XrPath subactionPath,
                                          XrPosef poseInActionSpace)
    {
        XrActionSpaceCreateInfo createInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        createInfo.action = action;
        createInfo.subactionPath = subactionPath;
        createInfo.poseInActionSpace = poseInActionSpace;
        CheckXrResult(instance, xrCreateActionSpace(session, &createInfo, &m_space), "xrCreateActionSpace");
    }

    OpenXRActionSpace::~OpenXRActionSpace()
    {
        if (m_space != XR_NULL_HANDLE)
        {
            xrDestroySpace(m_space);
            m_space = XR_NULL_HANDLE;
        }
    }
}
