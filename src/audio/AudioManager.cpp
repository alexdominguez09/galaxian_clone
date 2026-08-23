#include "AudioManager.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace galaxian {

namespace {

// Dev-audio format: mono S16 at a modest rate keeps the synthesized
// buffers small and the mixer trivial.
constexpr int kSampleRate = 22050;
// Post-mix output gain: with per-clip peak normalization x0.92 this puts
// a single effect at exactly the loudness auditioned in the sound chooser.
constexpr float kOutputGain = 0.8f;

const char* kSoundIdNames[kSoundIdCount] = {
    "PLAYER FIRE",  "ENEMY FIRE",   "ENEMY DESTROYED", "PLAYER DESTROYED",
    "WAVE START",   "GAME START",   "GAME OVER",
};

// ---- synthesis toolkit (mirrors tools/SoundChooser.cpp 1:1) ------------

enum class Wave { Square, Saw, Triangle, Sine, Noise };

struct Ctx {
    std::vector<float> samples;
    float phase = 0.f;
    unsigned noiseState = 0x1234567u;
};

void addTone(Ctx& c, Wave w, float f0, float f1, double seconds, float vol,
             float decayExp, int echoMs = 0, float echoGain = 0.f,
             float vibHz = 0.f, float vibDepth = 0.f)
{
    const int n = static_cast<int>(seconds * kSampleRate);
    const size_t start = c.samples.size();
    c.samples.resize(start + static_cast<size_t>(n), 0.f);
    const unsigned echoDelay =
        static_cast<unsigned>(echoMs * kSampleRate / 1000);
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / n;
        float freq = f0 + (f1 - f0) * t;
        if (vibHz > 0.f) {
            freq += std::sin(2.f * 3.14159265f * vibHz * i / kSampleRate) *
                    vibDepth * freq;
        }
        c.phase += freq / kSampleRate;
        c.phase -= std::floor(c.phase);
        float s;
        if (w == Wave::Noise) {
            // Deterministic per-position seed: identical clips synthesize
            // identically across manager instances.
            c.noiseState = 0x9E3779B9u ^
                           (static_cast<unsigned>(start + i) * 2654435761u);
            s = static_cast<float>(c.noiseState >> 16) / 32768.f - 1.f;
        } else if (w == Wave::Square) {
            s = c.phase < .5f ? 1.f : -1.f;
        } else if (w == Wave::Saw) {
            s = 2.f * c.phase - 1.f;
        } else if (w == Wave::Triangle) {
            s = 1.f - 4.f * std::fabs(c.phase - .5f);
        } else {
            s = std::sin(2.f * 3.14159265f * c.phase);
        }
        const float env = std::pow(1.f - t, decayExp);
        float v = s * env * vol;
        if (echoDelay > 0 && i >= static_cast<int>(echoDelay)) {
            v += c.samples[start + i - echoDelay] * echoGain;
        }
        c.samples[start + static_cast<size_t>(i)] += v;
    }
}

}  // namespace

const char* soundIdName(SoundId id)
{
    const int index = static_cast<int>(id);
    if (index >= 0 && index < kSoundIdCount) {
        return kSoundIdNames[index];
    }
    return "?";
}

AudioManager::~AudioManager()
{
    shutdown();
}

void AudioManager::generateClips()
{
    // ---- Stage 20 sound set: HUMAN-CHOSEN variants -------------------
    //
    // These generators mirror tools/SoundChooser.cpp EXACTLY (same DSP),
    // using the picks recorded by the human in
    // assets/audio/sound_choices.txt:
    //   player_fire=d      low heavy pew
    //   enemy_fire=a       short blip
    //   enemy_destroyed=c  zip-crash
    //   player_destroyed=b deep sub rumble
    //   wave_start=c       rising sweep
    //   game_start=c       jingle + held high note
    //   game_over=c        descend into low drone
    //   music_gameplay=a   authentic silence (soft space-hum drone)
    //
    // Each finished buffer is peak-normalized x0.92 -- the same gain the
    // chooser applied at playback -- so in-game loudness equals what was
    // auditioned. If a future re-choice happens: rerun ./build/sound_chooser
    // and port the picked generator here.
    soundBuffers_.clear();
    soundBuffers_.resize(kSoundIdCount);
    musicBuffer_ = Buffer{};

    auto normalizeToS16 = [](Buffer& buf) {
        float peak = 1e-6f;
        for (float v : buf.fsamples) {
            peak = std::max(peak, std::fabs(v));
        }
        const float gain = 0.92f / peak;
        buf.samples.clear();
        buf.samples.reserve(buf.fsamples.size());
        for (float v : buf.fsamples) {
            v *= gain;
            if (v > 1.f) v = 1.f;
            if (v < -1.f) v = -1.f;
            buf.samples.push_back(
                static_cast<Sint16>(v * 32767.f));
        }
        buf.fsamples.clear();
    };

    // --- player fire (d): low heavy pew ---
    {
        Ctx c;
        addTone(c, Wave::Square, 800, 150, .10f, .7f, 2.5f);
        soundBuffers_[static_cast<int>(SoundId::PlayerFire)].fsamples =
            std::move(c.samples);
        normalizeToS16(soundBuffers_[static_cast<int>(SoundId::PlayerFire)]);
    }
    // --- enemy fire (a): short blip ---
    {
        Ctx c;
        addTone(c, Wave::Square, 420, 210, .07f, .6f, 2.5f);
        soundBuffers_[static_cast<int>(SoundId::EnemyFire)].fsamples =
            std::move(c.samples);
        normalizeToS16(soundBuffers_[static_cast<int>(SoundId::EnemyFire)]);
    }
    // --- enemy destroyed (c): zip-crash ---
    {
        Ctx c;
        addTone(c, Wave::Square, 1300, 200, .08f, .5f, 2.f);
        addTone(c, Wave::Noise, 0, 0, .14f, .6f, 2.f);
        soundBuffers_[static_cast<int>(SoundId::EnemyDestroyed)].fsamples =
            std::move(c.samples);
        normalizeToS16(
            soundBuffers_[static_cast<int>(SoundId::EnemyDestroyed)]);
    }
    // --- player destroyed (b): deep sub rumble ---
    {
        Ctx c;
        addTone(c, Wave::Sine, 90, 38, .90f, .95f, .9f);
        addTone(c, Wave::Noise, 0, 0, .60f, .5f, 1.6f);
        soundBuffers_[static_cast<int>(SoundId::PlayerDestroyed)].fsamples =
            std::move(c.samples);
        normalizeToS16(
            soundBuffers_[static_cast<int>(SoundId::PlayerDestroyed)]);
    }
    // --- wave start (c): rising sweep ---
    {
        Ctx c;
        addTone(c, Wave::Square, 200, 1250, .42f, .55f, .7f);
        soundBuffers_[static_cast<int>(SoundId::WaveStart)].fsamples =
            std::move(c.samples);
        normalizeToS16(soundBuffers_[static_cast<int>(SoundId::WaveStart)]);
    }
    // --- game start (c): jingle + held high note ---
    {
        Ctx c;
        const float up[] = {392, 494, 587, 659, 784, 988};
        for (float f : up) {
            addTone(c, Wave::Square, f, f, .055f, .5f, 1.2f);
        }
        addTone(c, Wave::Square, 1175, 1175, .22f, .55f, .9f);
        soundBuffers_[static_cast<int>(SoundId::GameStart)].fsamples =
            std::move(c.samples);
        normalizeToS16(soundBuffers_[static_cast<int>(SoundId::GameStart)]);
    }
    // --- game over (c): descend into low drone ---
    {
        Ctx c;
        const float seq[] = {587, 494, 392};
        for (float f : seq) {
            addTone(c, Wave::Square, f, f, .14f, .5f, 1.f);
        }
        addTone(c, Wave::Triangle, 98, 98, .70f, .6f, .6f);
        soundBuffers_[static_cast<int>(SoundId::GameOver)].fsamples =
            std::move(c.samples);
        normalizeToS16(soundBuffers_[static_cast<int>(SoundId::GameOver)]);
    }
    // --- music (a): soft space-hum drone, looping ---
    {
        Ctx c;
        addTone(c, Wave::Triangle, 55, 55, 3.2f, .18f, .2f, 0, 0, .3f, .1f);
        musicBuffer_.fsamples = std::move(c.samples);
        normalizeToS16(musicBuffer_);
        musicBuffer_.looping = true;
    }
}

bool AudioManager::initialize()
{
    if (initialized_) {
        return true;
    }
    generateClips();

    // CRITICAL: the renderer only initializes SDL_INIT_VIDEO. The audio
    // SUBSYSTEM must be brought up explicitly or SDL_OpenAudioDevice fails
    // with "Audio subsystem is not initialized" (SDL does not lazy-init
    // it) -- which silently muted the whole game until Stage 20's fix.
    if (!forceSilent_) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::fprintf(stderr,
                         "galaxian: audio subsystem unavailable (%s), "
                         "running silent\n",
                         SDL_GetError());
        }
    }

    if (SDL_WasInit(SDL_INIT_AUDIO) != 0 && !forceSilent_) {
        SDL_AudioSpec want{};
        want.freq = kSampleRate;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = 1024;
        want.callback = &AudioManager::mixCallback;
        want.userdata = this;

        device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
        if (device_ == 0) {
            // Silent fallback (docs/test_plan.md Stage 20): the game runs
            // fine without a device.
            std::fprintf(stderr,
                         "galaxian: audio device unavailable (%s), "
                         "running silent\n",
                         SDL_GetError());
        } else {
            SDL_PauseAudioDevice(device_, 0);
            std::printf("galaxian: audio opened (%d Hz, %d ch)\n",
                        spec_.freq, spec_.channels);
        }
    }

    initialized_ = true;
    return true;
}

void AudioManager::shutdown()
{
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    if (initialized_) {
        // Release our reference to the audio subsystem.
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    initialized_ = false;
}

void AudioManager::playSound(SoundId id)
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= kSoundIdCount) {
        return;
    }
    ++playCounts_[index];

    if (device_ == 0) {
        return;  // silent path: counted but not voiced
    }
    SDL_LockAudioDevice(device_);
    Voice& v = voices_[nextVoice_];
    nextVoice_ = (nextVoice_ + 1) % kMaxVoices;  // round-robin reuse
    v.buffer = &soundBuffers_[index];
    v.position = 0;
    v.active = true;
    SDL_UnlockAudioDevice(device_);
}

void AudioManager::playMusic(MusicId id)
{
    (void)id;  // one track for now
    if (device_ == 0) {
        return;
    }
    SDL_LockAudioDevice(device_);
    musicVoice_.buffer = &musicBuffer_;
    musicVoice_.position = 0;
    musicVoice_.active = true;
    SDL_UnlockAudioDevice(device_);
}

void AudioManager::stopMusic()
{
    if (device_ == 0) {
        return;
    }
    SDL_LockAudioDevice(device_);
    musicVoice_ = Voice{};
    SDL_UnlockAudioDevice(device_);
}

void AudioManager::debugTriggerVoice(SoundId id)
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= kSoundIdCount ||
        soundBuffers_[index].samples.empty()) {
        return;
    }
    Voice& v = voices_[nextVoice_];
    nextVoice_ = (nextVoice_ + 1) % kMaxVoices;
    v.buffer = &soundBuffers_[index];
    v.position = 0;
    v.active = true;
}

std::vector<Sint16> AudioManager::mixForTest(int samples)
{
    std::vector<Sint16> out(static_cast<size_t>(samples > 0 ? samples : 0),
                            0);
    if (samples <= 0) {
        return out;
    }
    // One synchronous pass over every active voice (the same arithmetic
    // the real callback uses).
    for (int s = 0; s < samples; ++s) {
        std::int32_t mix = 0;
        for (Voice& v : voices_) {
            if (!v.active || v.buffer == nullptr) {
                continue;
            }
            mix += v.buffer->samples[static_cast<size_t>(v.position)];
            ++v.position;
            if (v.position >= static_cast<int>(v.buffer->samples.size())) {
                if (v.buffer->looping) {
                    v.position = 0;
                } else {
                    v.active = false;
                    v.buffer = nullptr;
                }
            }
        }
        if (musicVoice_.active && musicVoice_.buffer != nullptr &&
            !musicVoice_.buffer->samples.empty()) {
            mix += musicVoice_.buffer
                       ->samples[static_cast<size_t>(musicVoice_.position)];
            ++musicVoice_.position;
            if (musicVoice_.position >=
                static_cast<int>(musicVoice_.buffer->samples.size())) {
                musicVoice_.position = 0;  // loops forever
            }
        }
        // Clamp the summed mix back into S16 (post-gain).
        mix = static_cast<std::int32_t>(
            static_cast<float>(mix) * kOutputGain);
        if (mix > 32767) {
            mix = 32767;
        }
        if (mix < -32768) {
            mix = -32768;
        }
        out[static_cast<size_t>(s)] = static_cast<Sint16>(mix);
    }
    return out;
}

int AudioManager::activeVoicesForTest() const
{
    int count = 0;
    for (const Voice& v : voices_) {
        if (v.active) {
            ++count;
        }
    }
    return count;
}

void AudioManager::mixCallback(void* userdata, Uint8* stream, int len)
{
    auto* self = static_cast<AudioManager*>(userdata);
    auto* out = reinterpret_cast<Sint16*>(stream);
    const int frames = len / static_cast<int>(sizeof(Sint16));

    for (int s = 0; s < frames; ++s) {
        std::int32_t mix = 0;
        for (Voice& v : self->voices_) {
            if (!v.active || v.buffer == nullptr) {
                continue;
            }
            mix += v.buffer->samples[static_cast<size_t>(v.position)];
            ++v.position;
            if (v.position >= static_cast<int>(v.buffer->samples.size())) {
                if (v.buffer->looping) {
                    v.position = 0;
                } else {
                    v.active = false;
                    v.buffer = nullptr;
                }
            }
        }
        if (self->musicVoice_.active && self->musicVoice_.buffer != nullptr &&
            !self->musicVoice_.buffer->samples.empty()) {
            mix += self->musicVoice_.buffer
                       ->samples[static_cast<size_t>(
                           self->musicVoice_.position)];
            ++self->musicVoice_.position;
            if (self->musicVoice_.position >=
                static_cast<int>(self->musicVoice_.buffer->samples.size())) {
                self->musicVoice_.position = 0;
            }
        }
        mix = static_cast<std::int32_t>(
            static_cast<float>(mix) * kOutputGain);
        if (mix > 32767) {
            mix = 32767;
        }
        if (mix < -32768) {
            mix = -32768;
        }
        out[s] = static_cast<Sint16>(mix);
    }
}

}  // namespace galaxian
