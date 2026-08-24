#include "OpenXRReferenceSpace.hpp"

#include "OpenXRResult.hpp"

#include <cstdint>

namespace AREngine::XR::OpenXR
{
    std::vector<XrReferenceSpaceType> EnumerateReferenceSpaceTypes(XrInstance instance, XrSession session)
    {
        std::uint32_t count = 0;
        CheckXrResult(instance,
            xrEnumerateReferenceSpaces(session, 0, &count, nullptr),
            "xrEnumerateReferenceSpaces (count query)");

        std::vector<XrReferenceSpaceType> types(count);
        if (count == 0)
        {
            return types;
        }

        CheckXrResult(instance,
            xrEnumerateReferenceSpaces(session, count, &count, types.data()),
            "xrEnumerateReferenceSpaces (data query)");
        return types;
    }

    bool IsReferenceSpaceTypeSupported(const std::vector<XrReferenceSpaceType>& supported, XrReferenceSpaceType type)
    {
        for (const XrReferenceSpaceType candidate : supported)
        {
            if (candidate == type)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<XrReferenceSpaceType> SelectReferenceSpacesToCreate(const std::vector<XrReferenceSpaceType>& supported)
    {
        std::vector<XrReferenceSpaceType> result;
        if (IsReferenceSpaceTypeSupported(supported, XR_REFERENCE_SPACE_TYPE_VIEW))
        {
            result.push_back(XR_REFERENCE_SPACE_TYPE_VIEW);
        }
        if (IsReferenceSpaceTypeSupported(supported, XR_REFERENCE_SPACE_TYPE_LOCAL))
        {
            result.push_back(XR_REFERENCE_SPACE_TYPE_LOCAL);
        }
        if (IsReferenceSpaceTypeSupported(supported, XR_REFERENCE_SPACE_TYPE_STAGE))
        {
            result.push_back(XR_REFERENCE_SPACE_TYPE_STAGE);
        }
        return result;
    }

    OpenXRReferenceSpace::OpenXRReferenceSpace(XrInstance instance, XrSession session, XrReferenceSpaceType type, XrPosef poseInReferenceSpace)
        : m_type(type)
    {
        XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        createInfo.referenceSpaceType = type;
        createInfo.poseInReferenceSpace = poseInReferenceSpace;
        CheckXrResult(instance, xrCreateReferenceSpace(session, &createInfo, &m_space), "xrCreateReferenceSpace");
    }

    OpenXRReferenceSpace::~OpenXRReferenceSpace()
    {
        if (m_space != XR_NULL_HANDLE)
        {
            xrDestroySpace(m_space);
            m_space = XR_NULL_HANDLE;
        }
    }
}
