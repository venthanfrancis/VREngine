#pragma once

#include "AREngine/Assets/AssetId.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace AREngine::Assets
{
    // A decoded image, loaded and normalized by AssetManager::LoadTexture
    // from a real image file on disk (M14). Built on top of the same raw
    // bytes BinaryAsset would give you - see docs/ARCHITECTURE.md, "M6 -
    // No Common Asset Base Type", which already anticipated this exact
    // type - but as its own independent struct, not derived from
    // BinaryAsset, matching TextAsset/BinaryAsset's own precedent.
    //
    // Always normalized to RGBA8 regardless of the source file's actual
    // channel count/format: 4 channels, 8 bits/channel, row-major,
    // tightly packed (width * height * 4 bytes total), top-left source
    // orientation. `pixels` is std::uint8_t, not std::byte like
    // BinaryAsset::bytes - this is decoded, channel-typed pixel data
    // (matching Rendering::Vulkan's own GenerateCheckerboardRGBA8/
    // CreateTextureFromPixels pixel-data convention), not opaque raw
    // file bytes, so the type deliberately diverges from BinaryAsset
    // here. No Vulkan/OpenXR/Rendering type anywhere in this file.
    struct TextureAsset
    {
        AssetId id;
        std::filesystem::path path; // the relative path it was loaded from
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t channels = 4; // always 4 (RGBA8) after normalization
        std::vector<std::uint8_t> pixels;
    };
}
