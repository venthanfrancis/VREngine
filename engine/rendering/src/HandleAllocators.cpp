#include "HandleAllocators.hpp"

namespace AREngine::Rendering
{
    MeshHandle MeshHandleAllocator::GetOrCreate(Assets::AssetId assetId)
    {
        const auto existing = m_handlesByAsset.find(assetId);
        if (existing != m_handlesByAsset.end())
        {
            return existing->second;
        }

        const MeshHandle handle{m_next++};
        m_handlesByAsset.emplace(assetId, handle);
        return handle;
    }

    MeshHandle MeshHandleAllocator::CreateNew()
    {
        const MeshHandle handle{m_next++};
        ++m_proceduralCount;
        return handle;
    }

    MaterialHandle MaterialHandleAllocator::CreateNew()
    {
        return MaterialHandle{m_next++};
    }
}
