#include "AREngine/Assets/Assets.hpp"
#include "AudioDecode.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>

namespace
{
    int failures = 0;
    void Check(bool ok, const char* name) { if (!ok) { ++failures; std::fprintf(stderr, "FAILED: %s\n", name); } }
}
int main()
{
    using namespace AREngine::Assets;
    AssetManager assets(AR_AUDIO_ASSETS_ROOT);
    const auto id = assets.LoadAudio("audio/tone.wav");
    Check(id.has_value(), "file-backed WAV loads");
    if (!id) return 1;
    Check(assets.LoadAudio("audio/../audio/tone.wav") == id, "normalized path cache reuse");
    Check(assets.IsValid(*id), "audio participates in asset validity");
    Check(!assets.IsValid({}), "invalid asset id");
    const auto binary = assets.LoadBinary("audio/tone.wav");
    Check(binary && *binary != *id, "same-path distinct content type identity");
    const auto& wav = assets.GetAudio(*id);
    Check(wav.channels == 1 && wav.sampleRate == 48000 && wav.samples.size() == 12000, "PCM metadata and frame count");
    Check(wav.samples[0] == 0 && wav.samples[27] > 0.18f, "signed PCM decoding amplitude");
    Check(wav.samples[82] < -0.18f, "negative signed PCM decoding");
    Check(!assets.LoadAudio("missing.wav") && !assets.LoadAudio("hello.txt") &&
          !assets.LoadAudio("../outside.wav"), "missing corrupt and escaping loads fail");
    auto bytes = assets.GetBinary(*binary).bytes;
    for (std::size_t size = 0; size < bytes.size(); ++size)
        Check(!DecodeWav(std::span(bytes).first(size)), "every truncation rejected");
    auto bad = bytes;
    bad[20] = std::byte{3};
    Check(!DecodeWav(bad), "unsupported WAV encoding");
    bad = bytes; bad[22] = std::byte{0};
    Check(!DecodeWav(bad), "zero channels rejected");
    bad = bytes; bad[32] = std::byte{1};
    Check(!DecodeWav(bad), "inconsistent block alignment rejected");
    bad = bytes; bad[40] = std::byte{0xff}; bad[41] = std::byte{0xff}; bad[42] = std::byte{0xff}; bad[43] = std::byte{0xff};
    Check(!DecodeWav(bad), "oversized data chunk rejected");
    // Insert an odd-sized unknown chunk with padding, preserving valid RIFF size.
    bad = bytes;
    const std::byte junk[]{std::byte{'J'},std::byte{'U'},std::byte{'N'},std::byte{'K'},std::byte{1},std::byte{0},std::byte{0},std::byte{0},std::byte{7},std::byte{0}};
    bad.insert(bad.begin() + 12, std::begin(junk), std::end(junk));
    const auto set32 = [](auto& b, std::size_t p, unsigned v) { for (unsigned i = 0; i < 4; ++i) b[p+i] = static_cast<std::byte>((v >> (8*i)) & 255); };
    set32(bad, 4, static_cast<unsigned>(bad.size() - 8));
    Check(DecodeWav(bad).has_value(), "unknown padded chunks skipped");
    // Same PCM interpreted as stereo frames, with consistent metadata.
    bad = bytes; bad[22] = std::byte{2}; bad[32] = std::byte{4}; set32(bad, 28, 192000);
    const auto stereo = DecodeWav(bad);
    Check(stereo && stereo->channels == 2 && stereo->samples.size() == 12000, "stereo PCM supported");
    std::printf("Audio asset failures: %d\n", failures);
    return failures ? 1 : 0;
}
