#pragma once
#include "AREngine/Core/Math/Quaternion.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace AREngine::Audio
{
    // Context-local, non-recycled identities. Never pass ids between contexts.
    struct AudioClipId
    {
        std::uint64_t value = 0;
        bool operator==(const AudioClipId&) const = default;
    };
    struct AudioSourceId
    {
        std::uint64_t value = 0;
        bool operator==(const AudioSourceId&) const = default;
    };
    struct ListenerState
    {
        Core::Math::Vec3 position;
        Core::Math::Quaternion orientation;
    };
    struct SourceState
    {
        Core::Math::Vec3 position;
        float gain = 1.0f; // Linear, nonnegative.
        float pitch = 1.0f; // Playback-rate multiplier, [0.125, 8].
        bool looping = false;
        bool spatial = true;
    };
    enum class AudioOutput { Device, Offline };

    // Available when ARENGINE_ENABLE_AUDIO is ON. One owning application thread
    // calls this API. The backend owns its device callback thread. No frame/view
    // dependency: update ONE listener from a camera or head pose outside eye loops.
    class AudioContext
    {
    public:
        // Device failure is normal: nullptr, never silently a null device.
        static std::unique_ptr<AudioContext> Create(AudioOutput output = AudioOutput::Device);
        ~AudioContext();
        AudioContext(const AudioContext&) = delete;
        AudioContext& operator=(const AudioContext&) = delete;

        // Copies resident PCM once. Caller retains/reuses the returned clip id.
        // Invalid content returns zero. No file I/O or decoding in this module.
        AudioClipId CreateClip(std::span<const float> samples, unsigned channels, unsigned sampleRate);
        AudioSourceId CreateSource(AudioClipId clip, const SourceState& state = {});
        bool DestroySource(AudioSourceId id);
        bool DestroyClip(AudioClipId id); // Refuses while any source references it.
        bool SetSourceState(AudioSourceId id, const SourceState& state);
        bool SetListener(const ListenerState& state); // Rejects non-unit/nonfinite poses.
        bool Play(AudioSourceId id); // Restart at frame zero, including after EOF.
        bool Stop(AudioSourceId id);
        bool IsPlaying(AudioSourceId id) const;
        std::size_t ClipCount() const;
        std::size_t SourceCount() const;
        // Offline only: renders interleaved stereo float PCM at 48000 Hz using
        // the real backend. Caller supplies storage; never opens a device.
        bool RenderOffline(std::span<float> stereoSamples);

    private:
        AudioContext();
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
