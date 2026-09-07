#pragma once

// Private Jolt integration detail - never included outside src/jolt/ and
// PhysicsWorld.cpp. The ONLY place AREngine<->Jolt type conversion
// happens. See docs/ARCHITECTURE.md, "M18 - Physics Foundation",
// "Coordinate System" / "Quaternion Conversion".
//
// AREngine and Jolt are BOTH right-handed, +Y up - confirmed no axis
// flip is needed anywhere here, only an explicit quaternion component
// reorder: AREngine::Core::Math::Quaternion is stored Hamilton (w,x,y,z);
// Jolt's JPH::Quat is (x,y,z,w). Never memcpy/reinterpret_cast between
// them - always go through these named accessors.

#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>

namespace AREngine::Physics::Jolt
{
    [[nodiscard]] inline JPH::Vec3 ToJolt(const Core::Math::Vec3& v)
    {
        return JPH::Vec3(v.x, v.y, v.z);
    }

    // Also handles JPH::RVec3, Jolt's "real" (world-space) position
    // type: RVec3 is a type alias for Vec3 unless JPH_DOUBLE_PRECISION
    // is defined, which this codebase never defines (M18's narrow scope
    // has no need for double-precision large-world support) - a
    // separate FromJolt(const JPH::RVec3&) overload would therefore be
    // a duplicate definition of this exact function, not a distinct one.
    // If double precision is ever enabled later, this call site's
    // GetX()/GetY()/GetZ() results would need an explicit
    // static_cast<float> narrowing that isn't needed today.
    [[nodiscard]] inline Core::Math::Vec3 FromJolt(const JPH::Vec3& v)
    {
        return Core::Math::Vec3(v.GetX(), v.GetY(), v.GetZ());
    }

    [[nodiscard]] inline JPH::RVec3 ToJoltR(const Core::Math::Vec3& v)
    {
        return JPH::RVec3(v.x, v.y, v.z);
    }

    // Explicit component reorder - AREngine (w,x,y,z) -> Jolt (x,y,z,w).
    [[nodiscard]] inline JPH::Quat ToJolt(const Core::Math::Quaternion& q)
    {
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    // Explicit component reorder - Jolt (x,y,z,w) -> AREngine (w,x,y,z).
    [[nodiscard]] inline Core::Math::Quaternion FromJolt(const JPH::Quat& q)
    {
        return Core::Math::Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }
}
