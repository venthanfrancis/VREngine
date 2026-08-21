#pragma once

#include "AREngine/Rendering/Handles.hpp"

#include <cstdint>

namespace AREngine::Rendering
{
    // The minimum representation of "draw this geometry." Deliberately
    // has no transform, camera, material, or pipeline reference — those
    // belong to Scene (M5+) and to a real pipeline/shader system, which
    // is deferred entirely until Vulkan reveals what it actually needs
    // (see docs/ARCHITECTURE.md, "M4 Implementation Notes").
    struct DrawCommand
    {
        BufferHandle vertexBuffer;

        // Default-constructed (invalid, id == 0) means "not indexed."
        BufferHandle indexBuffer;

        // Vertex count for a non-indexed draw, or index count for an
        // indexed draw (when indexBuffer.IsValid()). A single field
        // because only one of those two counts is ever meaningful for a
        // given draw — mirroring how real graphics APIs distinguish
        // "draw" from "draw indexed" rather than tracking both counts
        // at once.
        std::uint32_t count = 0;
    };
}
