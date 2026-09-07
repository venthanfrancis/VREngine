#include "AREngine/Audio/AudioContext.hpp"
#include "AREngine/Assets/Assets.hpp"
#include <cstdio>
#include <limits>
#include <numbers>
#include <type_traits>
#include <vector>

using namespace AREngine;
using namespace Audio;
namespace
{
    int failures = 0;
    void Check(bool ok, const char* name) { if (!ok) { ++failures; std::fprintf(stderr, "FAILED: %s\n", name); } }
    double Energy(const std::vector<float>& samples, std::size_t channel)
    {
        double sum = 0;
        for (std::size_t i = channel; i < samples.size(); i += 2) sum += samples[i] * samples[i];
        return sum;
    }
    std::vector<float> Mix(AudioContext& context, std::size_t frames)
    {
        std::vector<float> output(frames * 2);
        Check(context.RenderOffline(output), "real backend offline mix succeeds");
        return output;
    }
}
int main()
{
    static_assert(!std::is_same_v<Assets::AssetId, AudioClipId>);
    static_assert(!std::is_same_v<AudioClipId, AudioSourceId>);
    static_assert(!std::is_copy_constructible_v<AudioContext>);
    auto context = AudioContext::Create(AudioOutput::Offline);
    Check(context != nullptr, "no-device engine creation");
    if (!context) return 1;
    auto& c = *context;
    AudioClipId clip;
    { // Assets and caller PCM die before playback begins.
        Assets::AssetManager assets(AR_AUDIO_ASSETS_ROOT);
        auto id = assets.LoadAudio("audio/tone.wav");
        if (!id) return 1;
        const auto& wav = assets.GetAudio(*id);
        clip = c.CreateClip(wav.samples, wav.channels, wav.sampleRate);
    }
    Check(clip.value != 0 && c.ClipCount() == 1, "context owns PCM copy");
    Check(c.CreateClip({}, 1, 48000).value == 0, "empty PCM rejected");
    const float invalid[] = {std::numeric_limits<float>::quiet_NaN()};
    Check(c.CreateClip(invalid, 1, 48000).value == 0, "nonfinite PCM rejected");
    Check(c.CreateSource({}).value == 0 && !c.Play({}) && !c.Stop({}) &&
          !c.DestroySource({}) && !c.DestroyClip({}) && !c.IsPlaying({}), "invalid ids rejected");
    SourceState state;
    state.spatial = false;
    auto source = c.CreateSource(clip, state);
    auto second = c.CreateSource(clip, state);
    Check(source.value && second != source && c.ClipCount() == 1 && c.SourceCount() == 2, "shared clip independent sources");
    Check(!c.DestroyClip(clip), "live source prevents clip destruction");
    Check(c.Play(source), "one-shot start");
    auto normal = Mix(c, 4096);
    Check(Energy(normal, 0) > 1, "audible PCM generated after asset owner destruction");
    Check(!c.IsPlaying(second), "second source cursor independent");
    Mix(c, 16000);
    Check(!c.IsPlaying(source), "one-shot reaches EOF");
    Check(c.Play(source), "replay from EOF");
    Check(Energy(Mix(c, 4096), 0) > 1, "replay produces PCM");
    Check(c.Stop(source), "stop succeeds");
    Mix(c, 1024); // Drain backend's existing processing block.
    Check(Energy(Mix(c, 4096), 0) == 0, "stop produces silence");
    state.looping = true;
    state.gain = 0.5f;
    Check(c.SetSourceState(source, state) && c.Play(source), "gain and loop update");
    Mix(c, 1024);
    auto quiet = Mix(c, 4096);
    Check(Energy(quiet, 0) / Energy(normal, 0) > 0.20 && Energy(quiet, 0) / Energy(normal, 0) < 0.30, "half gain yields quarter energy");
    Mix(c, 30000);
    Check(c.IsPlaying(source) && Energy(Mix(c, 4096), 0) > 1, "loop survives multiple clip durations");
    state.gain = 1; state.pitch = 2;
    Check(c.SetSourceState(source, state), "pitch update");
    Mix(c, 2048);
    auto fast = Mix(c, 4800);
    unsigned crossings = 0;
    for (std::size_t i = 2; i < fast.size(); i += 2) if (fast[i-2] <= 0 && fast[i] > 0) ++crossings;
    Check(crossings > 80 && crossings < 96, "double pitch actually doubles frequency to 880Hz");
    state.pitch = 1; state.spatial = true; state.position = {-2, 0, -1};
    Check(c.SetSourceState(source, state), "left spatial source");
    Mix(c, 2048);
    auto left = Mix(c, 4096);
    state.position = {2, 0, -1}; c.SetSourceState(source, state); Mix(c, 2048);
    auto right = Mix(c, 4096);
    Check(Energy(left, 0) > Energy(left, 1) * 1.5 && Energy(right, 1) > Energy(right, 0) * 1.5, "world X pans to correct stereo channel");
    ListenerState listener;
    listener.orientation = Core::Math::Quaternion::FromAxisAngle({0,1,0}, std::numbers::pi_v<float>);
    Check(c.SetListener(listener), "rotated listener"); Mix(c, 2048);
    auto rotated = Mix(c, 4096);
    Check(Energy(rotated, 0) > Energy(rotated, 1) * 1.5, "head rotation reverses relative pan");
    listener = {}; listener.position = {0,0,20}; c.SetListener(listener); Mix(c, 2048);
    auto far = Mix(c, 4096);
    Check(Energy(far, 0) + Energy(far, 1) < (Energy(right, 0) + Energy(right, 1)) * 0.1, "listener distance attenuates source");
    state.pitch = 0; Check(!c.SetSourceState(source, state), "invalid pitch rejected");
    state.pitch = 1; state.gain = -1; Check(!c.SetSourceState(source, state), "invalid gain rejected");
    listener.orientation = {0,0,0,0}; Check(!c.SetListener(listener), "invalid orientation rejected");
    float odd[3]{}; Check(!c.RenderOffline(odd), "partial stereo frame rejected");
    Check(c.DestroySource(source) && !c.Play(source) && !c.DestroySource(source), "destroyed source stays invalid");
    Check(c.DestroySource(second) && c.DestroyClip(clip) && !c.DestroyClip(clip), "final source releases clip ownership");
    Check(c.ClipCount() == 0 && c.SourceCount() == 0, "all resources released");
    for (int i = 0; i < 10; ++i)
    {
        auto life = AudioContext::Create(AudioOutput::Offline);
        if (!life) { Check(false, "repeated context init"); break; }
        const float pcm[]{0.2f, -0.2f};
        auto id = life->CreateClip(pcm, 1, 48000);
        SourceState loop; loop.looping = true;
        auto s = life->CreateSource(id, loop);
        life->Play(s); Mix(*life, 256); // Destruction while playing.
    }
    std::printf("Audio backend failures: %d\n", failures);
    return failures ? 1 : 0;
}
