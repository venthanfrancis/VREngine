#pragma once

#include <cstddef>

namespace AREngine::Rendering
{
    // What a buffer is for. Deliberately just the three categories
    // actually needed so far — not every Vulkan buffer usage flag; more
    // are added when a real backend needs them.
    enum class BufferUsage
    {
        Vertex,
        Index,
        Uniform,
    };

    struct BufferDesc
    {
        std::size_t sizeBytes = 0;
        BufferUsage usage = BufferUsage::Vertex;
    };
}
