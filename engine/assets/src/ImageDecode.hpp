#pragma once

// M14: the ONLY point in this codebase that knows stb_image exists.
// Deliberately kept private (engine/assets/src/, not include/) - not
// part of AssetManager's public API, no stb type or pointer ever
// crosses out of ImageDecode.cpp. See docs/ARCHITECTURE.md, "M14 -
// Asset-Backed Texture & Material Loading Foundation".

#include "AREngine/Assets/TextureAsset.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace AREngine::Assets
{
    // Decodes `fileBytes` (the raw contents of an image file, e.g. PNG)
    // into RGBA8 pixel data. Returns std::nullopt on any decode failure
    // (corrupt file, unsupported format, zero-size input) - never
    // crashes through a null decoder pointer. The returned TextureAsset
    // has `id`/`path` left default-constructed; the caller (AssetManager)
    // fills those in.
    [[nodiscard]] std::optional<TextureAsset> DecodeImageRGBA8(std::span<const std::byte> fileBytes);
}
