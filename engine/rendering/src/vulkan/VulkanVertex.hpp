#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "AREngine/Rendering/MeshData.hpp"

#include <vulkan/vulkan.h>

#include <array>

namespace AREngine::Rendering::Vulkan
{
    // Through M8G this file defined its own private `Vertex` struct
    // (position/color/uv) — a near-duplicate of what M8H's
    // Rendering::MeshVertex now is. As of M8H, this IS that evidence:
    // the exact same three fields were independently needed twice (the
    // old hard-coded demo geometry, and now generic mesh data), which
    // is what justifies operating directly on the generic type here
    // instead of converting a std::vector<MeshVertex> into a
    // Vulkan-private duplicate on every mesh upload. See
    // docs/ARCHITECTURE.md, "Vertex Format Review (M8H)".
    //
    // No VkFormat or byte offset is exposed on any public Rendering
    // header — MeshVertex itself (Rendering/MeshData.hpp) stays plain
    // engine data; only these two Vulkan-private functions know how to
    // turn it into Vulkan's vertex-input description structs.
    //
    // One binding (binding 0), one vertex per draw call - not per
    // instance (no instancing yet). See docs/ARCHITECTURE.md,
    // "Vertex Input Layout (M8D)".
    [[nodiscard]] VkVertexInputBindingDescription GetVertexBindingDescription();

    // location 0 = position (vec3 -> VK_FORMAT_R32G32B32_SFLOAT),
    // location 1 = color (vec3 -> VK_FORMAT_R32G32B32_SFLOAT), location
    // 2 = uv (vec2 -> VK_FORMAT_R32G32_SFLOAT) - matches triangle.vert's
    // `layout(location = 0/1/2) in ...` declarations. Offsets computed
    // via offsetof, not hand-counted.
    [[nodiscard]] std::array<VkVertexInputAttributeDescription, 3> GetVertexAttributeDescriptions();
}
