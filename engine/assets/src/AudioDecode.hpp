#pragma once
#include "AREngine/Assets/AudioAsset.hpp"
#include <optional>
#include <span>
#include <cstddef>

namespace AREngine::Assets
{
    // Narrow RIFF/WAVE PCM16 mono/stereo reader; rejects truncated chunks.
    std::optional<AudioAsset> DecodeWav(std::span<const std::byte> bytes);
}
