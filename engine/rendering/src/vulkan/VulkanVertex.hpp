#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "AREngine/Core/Math/Vec2.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

#include <vulkan/vulkan.h>

#include <array>

namespace AREngine::Rendering::Vulkan
{
    // A minimal Vulkan-demo vertex: a real 3D position, an RGB color,
    // and a 2D texture coordinate. Deliberately private to Rendering's
    // Vulkan backend and its manual demo, not a generic Mesh/
    // VertexFormat abstraction — one vertex shape with no evidence of
    // what a second one would need is not enough to design that
    // abstraction from. See docs/ARCHITECTURE.md, "Vertex Structure
    // (M8D)", "Vertex Format (M8E)", and "Vec3 Vertex Layout (M8F)"
    // (position changed from Vec2 to Vec3 in M8F for genuine 3D).
    struct Vertex
    {
        AREngine::Core::Math::Vec3 position;
        AREngine::Core::Math::Vec3 color;
        AREngine::Core::Math::Vec2 uv;

        // One binding (binding 0), one vertex per draw call - not per
        // instance (no instancing yet). See docs/ARCHITECTURE.md,
        // "Vertex Input Layout (M8D)".
        [[nodiscard]] static VkVertexInputBindingDescription GetBindingDescription();

        // location 0 = position (vec3 -> VK_FORMAT_R32G32B32_SFLOAT,
        // as of M8F), location 1 = color (vec3 -> VK_FORMAT_R32G32B32_SFLOAT),
        // location 2 = uv (vec2 -> VK_FORMAT_R32G32_SFLOAT) - matches
        // triangle.vert's `layout(location = 0/1/2) in ...`
        // declarations. Offsets computed via offsetof, not hand-counted.
        [[nodiscard]] static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
    };
}
