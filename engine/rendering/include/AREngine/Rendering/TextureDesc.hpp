#pragma once

#include <cstdint>

namespace AREngine::Rendering
{
    // Deliberately one format for now. The real set of formats this
    // engine needs isn't known until a real backend (Vulkan, M8) does —
    // guessing at a full format enum now would just be guessing.
    enum class TextureFormat
    {
        RGBA8Unorm,
    };

    struct TextureDesc
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8Unorm;
    };
}
