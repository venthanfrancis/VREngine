#pragma once

#include "AREngine/Assets/AssetId.hpp"
#include <filesystem>
#include <vector>

namespace AREngine::Assets
{
    // Fully decoded interleaved PCM. Independent of playback/backend lifetime.
    struct AudioAsset
    {
        AssetId id;
        std::filesystem::path path;
        unsigned sampleRate = 0;
        unsigned channels = 0;
        std::vector<float> samples;
    };
}
