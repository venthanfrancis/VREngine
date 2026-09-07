#pragma once

// Private Jolt integration detail. Minimal 2-layer collision setup
// (NON_MOVING/MOVING) - exactly per Jolt's own official HelloWorld
// example. No gameplay collision channels yet - see
// docs/ARCHITECTURE.md, "M18 - Physics Foundation", "Jolt Layers".

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace AREngine::Physics::Jolt
{
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::uint NUM_LAYERS = 2;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS = 2;
    }

    // Maps each JPH::ObjectLayer to a JPH::BroadPhaseLayer - a direct
    // 1:1 mapping for this milestone's minimal 2-layer scheme.
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl();

        [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override;
        [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

    private:
        JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
    };

    // NON_MOVING only needs to be tested against MOVING broadphase
    // objects (a static body never needs to be tested against another
    // static body); MOVING is tested against everything.
    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broadPhase) const override;
    };

    // Same rule at the object-layer level: NON_MOVING only collides with
    // MOVING; MOVING collides with everything.
    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
    {
    public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
    };
}
