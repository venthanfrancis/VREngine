#include "JoltLayers.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Physics::Jolt
{
    BPLayerInterfaceImpl::BPLayerInterfaceImpl()
    {
        m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    JPH::uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
    {
        AR_ASSERT_MSG(layer < Layers::NUM_LAYERS, "Unknown ObjectLayer");
        return m_objectToBroadPhase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
    {
        switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
        {
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING): return "NON_MOVING";
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):     return "MOVING";
            default:                                                                    return "INVALID";
        }
    }
#endif

    bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broadPhase) const
    {
        if (object == Layers::NON_MOVING)
        {
            return broadPhase == BroadPhaseLayers::MOVING;
        }
        return true; // MOVING collides with everything
    }

    bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
    {
        if (a == Layers::NON_MOVING)
        {
            return b == Layers::MOVING;
        }
        return true; // MOVING collides with everything
    }
}
