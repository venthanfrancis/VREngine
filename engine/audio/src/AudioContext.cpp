#include "AREngine/Audio/AudioContext.hpp"
#include <miniaudio.h>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace AREngine::Audio
{
    namespace
    {
        bool Finite(Core::Math::Vec3 p)
        {
            return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
        }
        bool Valid(const SourceState& s)
        {
            return Finite(s.position) && std::isfinite(s.gain) && s.gain >= 0 &&
                std::isfinite(s.pitch) && s.pitch >= 0.125f && s.pitch <= 8;
        }
    }
    struct AudioContext::Impl
    {
        struct Clip
        {
            std::vector<float> samples;
            unsigned channels;
            unsigned sampleRate;
        };
        struct Source
        {
            AudioClipId clip;
            ma_audio_buffer buffer{};
            ma_sound sound{};
            bool bufferReady = false;
            bool soundReady = false;
            ~Source()
            {
                if (soundReady) ma_sound_uninit(&sound);
                if (bufferReady) ma_audio_buffer_uninit(&buffer);
            }
        };
        ma_engine engine{};
        bool ready = false;
        AudioOutput output = AudioOutput::Device;
        std::uint64_t nextClip = 1;
        std::uint64_t nextSource = 1;
        std::unordered_map<std::uint64_t, Clip> clips;
        std::unordered_map<std::uint64_t, std::unique_ptr<Source>> sources;
        ~Impl()
        {
            if (ready) ma_engine_stop(&engine);
            sources.clear(); // Disconnect sound nodes, then their cursors.
            clips.clear();   // PCM remains alive until every cursor is gone.
            if (ready) ma_engine_uninit(&engine);
        }
    };

    AudioContext::AudioContext() : m_impl(std::make_unique<Impl>()) {}
    AudioContext::~AudioContext() = default;

    std::unique_ptr<AudioContext> AudioContext::Create(AudioOutput output)
    {
        if (output != AudioOutput::Device && output != AudioOutput::Offline) return nullptr;
        auto context = std::unique_ptr<AudioContext>(new AudioContext);
        auto config = ma_engine_config_init();
        config.listenerCount = 1;
        config.channels = 2;
        config.sampleRate = 48000;
        config.noDevice = output == AudioOutput::Offline;
        if (ma_engine_init(&config, &context->m_impl->engine) != MA_SUCCESS) return nullptr;
        context->m_impl->ready = true;
        context->m_impl->output = output;
        context->SetListener({});
        return context;
    }

    AudioClipId AudioContext::CreateClip(std::span<const float> samples, unsigned channels, unsigned sampleRate)
    {
        if ((channels != 1 && channels != 2) || sampleRate < 8000 || sampleRate > 192000 ||
            samples.empty() || samples.size() % channels != 0 ||
            !std::all_of(samples.begin(), samples.end(), [](float x) { return std::isfinite(x) && std::abs(x) <= 1; })) return {};
        const AudioClipId id{m_impl->nextClip++};
        m_impl->clips.emplace(id.value, Impl::Clip{std::vector<float>(samples.begin(), samples.end()), channels, sampleRate});
        return id;
    }

    AudioSourceId AudioContext::CreateSource(AudioClipId clip, const SourceState& state)
    {
        const auto it = m_impl->clips.find(clip.value);
        if (it == m_impl->clips.end() || !Valid(state)) return {};
        auto source = std::make_unique<Impl::Source>();
        source->clip = clip;
        auto config = ma_audio_buffer_config_init(ma_format_f32, it->second.channels,
            it->second.samples.size() / it->second.channels, it->second.samples.data(), nullptr);
        config.sampleRate = it->second.sampleRate;
        if (ma_audio_buffer_init(&config, &source->buffer) != MA_SUCCESS) return {};
        source->bufferReady = true;
        if (ma_sound_init_from_data_source(&m_impl->engine, &source->buffer, 0, nullptr, &source->sound) != MA_SUCCESS) return {};
        source->soundReady = true;
        ma_sound_set_pinned_listener_index(&source->sound, 0);
        ma_sound_set_attenuation_model(&source->sound, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(&source->sound, 1);
        ma_sound_set_max_distance(&source->sound, 1000);
        ma_sound_set_rolloff(&source->sound, 1);
        const AudioSourceId id{m_impl->nextSource++};
        m_impl->sources.emplace(id.value, std::move(source));
        SetSourceState(id, state);
        return id;
    }

    bool AudioContext::DestroySource(AudioSourceId id) { return m_impl->sources.erase(id.value) != 0; }
    bool AudioContext::DestroyClip(AudioClipId id)
    {
        for (const auto& [key, source] : m_impl->sources)
        {
            (void)key;
            if (source->clip == id) return false;
        }
        return m_impl->clips.erase(id.value) != 0;
    }
    bool AudioContext::SetSourceState(AudioSourceId id, const SourceState& state)
    {
        const auto it = m_impl->sources.find(id.value);
        if (it == m_impl->sources.end() || !Valid(state)) return false;
        auto* sound = &it->second->sound;
        ma_sound_set_volume(sound, state.gain);
        ma_sound_set_pitch(sound, state.pitch);
        ma_sound_set_looping(sound, state.looping);
        ma_sound_set_spatialization_enabled(sound, state.spatial);
        ma_sound_set_position(sound, state.position.x, state.position.y, state.position.z);
        return true;
    }
    bool AudioContext::SetListener(const ListenerState& state)
    {
        using namespace Core::Math;
        const auto q = state.orientation;
        const float norm = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
        if (!Finite(state.position) || !std::isfinite(norm) || std::abs(norm - 1) > 0.001f) return false;
        const auto forward = Rotate(q, kWorldForward);
        const auto up = Rotate(q, kWorldUp);
        ma_engine_listener_set_position(&m_impl->engine, 0, state.position.x, state.position.y, state.position.z);
        ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
        return true;
    }
    bool AudioContext::Play(AudioSourceId id)
    {
        const auto it = m_impl->sources.find(id.value);
        if (it == m_impl->sources.end()) return false;
        auto* sound = &it->second->sound;
        return ma_sound_stop(sound) == MA_SUCCESS && ma_sound_seek_to_pcm_frame(sound, 0) == MA_SUCCESS && ma_sound_start(sound) == MA_SUCCESS;
    }
    bool AudioContext::Stop(AudioSourceId id)
    {
        const auto it = m_impl->sources.find(id.value);
        return it != m_impl->sources.end() && ma_sound_stop(&it->second->sound) == MA_SUCCESS;
    }
    bool AudioContext::IsPlaying(AudioSourceId id) const
    {
        const auto it = m_impl->sources.find(id.value);
        return it != m_impl->sources.end() && ma_sound_is_playing(&it->second->sound);
    }
    std::size_t AudioContext::ClipCount() const { return m_impl->clips.size(); }
    std::size_t AudioContext::SourceCount() const { return m_impl->sources.size(); }
    bool AudioContext::RenderOffline(std::span<float> stereoSamples)
    {
        if (m_impl->output != AudioOutput::Offline || stereoSamples.size() % 2 != 0) return false;
        if (stereoSamples.empty()) return true;
        return ma_engine_read_pcm_frames(&m_impl->engine, stereoSamples.data(), stereoSamples.size() / 2, nullptr) == MA_SUCCESS;
    }
}
