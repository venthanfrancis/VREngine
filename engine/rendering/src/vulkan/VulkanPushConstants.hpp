#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Vec4.hpp"

namespace AREngine::Rendering::Vulkan
{
    // M8F's whole transform-upload mechanism: one push constant block,
    // computed fully on the CPU (Model/View/Projection already
    // multiplied down to one Mat4) and a flat tint color, pushed fresh
    // before each draw call. 64 + 16 = 80 bytes, safely under the
    // 128-byte minimum every Vulkan implementation guarantees for push
    // constants.
    //
    // Deliberately temporary and Vulkan-private - not a generic
    // uniform/transform system. Real per-object model transforms and
    // per-frame camera data will eventually need something more
    // structured (very likely a uniform buffer once Scene integration
    // gives real evidence of what "per-object" and "per-frame" actually
    // need to look like); this is the smallest correct mechanism for
    // M8F's fixed-camera, two-object depth proof. See
    // docs/ARCHITECTURE.md, "Transform Upload Method (M8F)".
    //
    // Field order/layout matches std430-style push-constant rules
    // exactly as GLSL expects: mat4 first (16-byte aligned, 64 bytes),
    // then vec4 (16-byte aligned) immediately after - no padding needed
    // on either side.
    struct MvpPushConstants
    {
        AREngine::Core::Math::Mat4 mvp;
        AREngine::Core::Math::Vec4 tint;
    };
}
