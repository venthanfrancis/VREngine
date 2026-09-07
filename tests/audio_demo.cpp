#include "AREngine/Assets/Assets.hpp"
#include "AREngine/Audio/AudioContext.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    using namespace AREngine;
    Assets::AssetManager assets(AR_AUDIO_ASSETS_ROOT);
    const auto asset = assets.LoadAudio("audio/tone.wav");
    if (!asset) return 1;
    auto audio = Audio::AudioContext::Create();
    if (!audio) { std::fprintf(stderr, "No audio playback device could be initialized.\n"); return 2; }
    const auto& pcm = assets.GetAudio(*asset);
    const auto clip = audio->CreateClip(pcm.samples, pcm.channels, pcm.sampleRate);
    Audio::SourceState state;
    state.gain = 0.4f;
    state.position = {-2,0,-1};
    const auto source = audio->CreateSource(clip, state);
    if (!source.value || !audio->Play(source)) return 1;
    std::puts("Device initialized: one clip, one source, one listener. One-shot left.");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (audio->IsPlaying(source)) return 1;
    state.looping = true;
    if (!audio->SetSourceState(source, state) || !audio->Play(source)) return 1;
    std::puts("Loop sweeps left to right; pitch rises from 1 to 2. No content loads in loop.");
    for (int frame = 0; frame < 180; ++frame)
    {
        state.position.x = -2 + 4 * static_cast<float>(frame) / 179;
        state.pitch = 1 + static_cast<float>(frame) / 179;
        if (!audio->SetSourceState(source, state)) return 1;
        // Integration point: replace with camera/head world pose once per frame,
        // before rendering any views. Never feed an individual eye pose.
        if (!audio->SetListener({})) return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    if (!audio->Stop(source) || audio->IsPlaying(source)) return 1;
    std::printf("180 world updates, clips=%zu sources=%zu. Stopped; clean teardown.\n", audio->ClipCount(), audio->SourceCount());
    return 0;
}
