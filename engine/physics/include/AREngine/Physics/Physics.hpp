#pragma once

// Umbrella header for AREngine::Physics's unconditional, backend-neutral
// public API - the types buildable regardless of ARENGINE_ENABLE_PHYSICS.
// PhysicsWorld/PhysicsSceneBridge (which require a real backend) are
// intentionally NOT included here - include
// "AREngine/Physics/PhysicsWorld.hpp"/"PhysicsSceneBridge.hpp" directly
// where the backend is actually needed. See docs/ARCHITECTURE.md, "M18 -
// Physics Foundation".

#include "AREngine/Physics/ColliderDesc.hpp"
#include "AREngine/Physics/FixedTimestepAccumulator.hpp"
#include "AREngine/Physics/PhysicsBodyId.hpp"
#include "AREngine/Physics/PhysicsPose.hpp"
#include "AREngine/Physics/RigidBodyDesc.hpp"
