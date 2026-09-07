#include "AudioDecode.hpp"
#include <cstdint>
#include <cstring>

namespace AREngine::Assets
{
    std::optional<AudioAsset> DecodeWav(std::span<const std::byte> bytes)
    {
        const auto u16 = [&](std::size_t p) {
            return std::to_integer<unsigned>(bytes[p]) | (std::to_integer<unsigned>(bytes[p + 1]) << 8);
        };
        const auto u32 = [&](std::size_t p) {
            return static_cast<std::uint32_t>(u16(p)) | (static_cast<std::uint32_t>(u16(p + 2)) << 16);
        };
        const auto tag = [&](std::size_t p, const char* s) { return std::memcmp(bytes.data() + p, s, 4) == 0; };
        if (bytes.size() < 12 || !tag(0, "RIFF") || !tag(8, "WAVE")) return std::nullopt;
        const std::uint64_t end64 = static_cast<std::uint64_t>(u32(4)) + 8;
        if (end64 != bytes.size()) return std::nullopt;
        AudioAsset result;
        std::span<const std::byte> pcm;
        bool format = false;
        bool data = false;
        for (std::size_t p = 12; p < bytes.size();)
        {
            if (bytes.size() - p < 8) return std::nullopt;
            const std::size_t size = u32(p + 4);
            const std::size_t start = p + 8;
            if (size > bytes.size() - start) return std::nullopt;
            if (tag(p, "fmt "))
            {
                if (format || size < 16 || u16(start) != 1 || u16(start + 14) != 16) return std::nullopt;
                result.channels = u16(start + 2);
                result.sampleRate = u32(start + 4);
                if ((result.channels != 1 && result.channels != 2) || result.sampleRate < 8000 ||
                    result.sampleRate > 192000 || u16(start + 12) != result.channels * 2 ||
                    u32(start + 8) != result.sampleRate * result.channels * 2) return std::nullopt;
                format = true;
            }
            else if (tag(p, "data"))
            {
                if (data) return std::nullopt;
                pcm = bytes.subspan(start, size);
                data = true;
            }
            const std::size_t padding = size & 1;
            if (padding > bytes.size() - start - size) return std::nullopt;
            p = start + size + padding;
        }
        if (!format || !data || pcm.empty() || pcm.size() % (result.channels * 2) != 0) return std::nullopt;
        result.samples.reserve(pcm.size() / 2);
        for (std::size_t p = 0; p < pcm.size(); p += 2)
        {
            const unsigned value = std::to_integer<unsigned>(pcm[p]) | (std::to_integer<unsigned>(pcm[p + 1]) << 8);
            const int signedValue = value >= 32768 ? static_cast<int>(value) - 65536 : static_cast<int>(value);
            result.samples.push_back(static_cast<float>(signedValue) / 32768.0f);
        }
        return result;
    }
}
