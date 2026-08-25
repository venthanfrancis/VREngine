#include "OpenXRProjectionLayer.hpp"

#include "AREngine/Core/Log.hpp"

#include <format>

namespace AREngine::XR::OpenXR
{
    OpenXRProjectionLayer::OpenXRProjectionLayer(std::vector<XrSwapchainSubImage> subImages, XrSpace space)
        : m_subImages(std::move(subImages))
        , m_space(space)
    {
    }

    bool OpenXRProjectionLayer::Prepare(const std::vector<XrView>& views)
    {
        m_hasLayer = false;

        if (views.empty())
        {
            return false;
        }

        // Runtime data, checked every frame - not a debug-only
        // assertion, since a Release build could compile that out
        // entirely and leave no check at all. A mismatch fails this
        // frame safely (zero layers) rather than indexing either array
        // out of bounds.
        if (views.size() != m_subImages.size())
        {
            AR_LOG_ERROR(std::format(
                "OpenXRProjectionLayer: located view count ({}) does not match the configured "
                "swapchain/sub-image count ({}) - submitting zero composition layers this frame",
                views.size(), m_subImages.size()));
            return false;
        }

        m_projectionViews.clear();
        m_projectionViews.reserve(views.size());
        for (std::size_t i = 0; i < views.size(); ++i)
        {
            XrCompositionLayerProjectionView projectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionView.pose = views[i].pose;
            projectionView.fov = views[i].fov;
            projectionView.subImage = m_subImages[i];
            m_projectionViews.push_back(projectionView);
        }

        m_projectionLayer = XrCompositionLayerProjection{};
        m_projectionLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        m_projectionLayer.layerFlags = 0;
        m_projectionLayer.space = m_space;
        m_projectionLayer.viewCount = static_cast<std::uint32_t>(m_projectionViews.size());
        m_projectionLayer.views = m_projectionViews.data();

        m_hasLayer = true;
        return true;
    }

    const XrCompositionLayerProjection* OpenXRProjectionLayer::Get() const
    {
        return m_hasLayer ? &m_projectionLayer : nullptr;
    }
}
