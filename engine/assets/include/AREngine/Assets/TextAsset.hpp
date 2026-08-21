#pragma once

#include "AREngine/Assets/AssetId.hpp"

#include <filesystem>
#include <string>

namespace AREngine::Assets
{
    // A file loaded as raw text. No parsing, no format — just "the
    // bytes of this file, as a string." Real content types
    // (MeshAsset, TextureAsset, ...) build on top of this and
    // BinaryAsset once there is real format parsing to do; see
    // docs/ARCHITECTURE.md, "Future Content Types".
    //
    // No common base class with BinaryAsset — see docs/ARCHITECTURE.md,
    // "Asset Base Type Decision".
    struct TextAsset
    {
        AssetId id;
        std::filesystem::path path; // the relative path it was loaded from
        std::string contents;
    };
}
