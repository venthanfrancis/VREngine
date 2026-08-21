#pragma once

#include "AREngine/Assets/AssetId.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace AREngine::Assets
{
    // A file loaded as raw bytes. No parsing, no format — just "the
    // bytes of this file." See TextAsset for the analogous text case
    // and docs/ARCHITECTURE.md for why there's no shared base type.
    struct BinaryAsset
    {
        AssetId id;
        std::filesystem::path path; // the relative path it was loaded from
        std::vector<std::byte> bytes;
    };
}
