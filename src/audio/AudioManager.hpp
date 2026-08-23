#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <vector>

namespace galaxian {

// Sound effects (docs/game_spec.md §13, Stage 20). Gameplay code only
// ever names these ids — never file paths, never devices.
enum class SoundId {
    PlayerFire,
    EnemyFire,
    EnemyDestroyed,
    PlayerDestroyed,
    WaveStart,
    GameStart,
    GameOver,
    Count,
};
inline constexpr int kSoundIdCount = static_cast<int>(SoundId::Count);

// Short upper-case label for logs/debug displays.
const char* soundIdName(SoundId id);

// Looping background music (optional per the plan; one gameplay track).
enum class MusicId {
    Gameplay,
    Count,
};

// The audio subsystem (docs/architecture.md §3.10, Stage 20).
//
// Design notes:
//   * **Procedural dev audio**: like DevArt's textures, the clips are
//     tiny synthesized chiptune-style buffers created at initialize() —
//     no binary assets, no extra library (plain SDL2 audio, no
//     SDL_mixer). Stage 24 can swap real files in behind the same ids.
//   * **Silent fallback**: if opening a device fails (or setSilent(true)
//     was requested — used by headless tests), the manager runs muted but
//     fully functional: playSound/playMusic keep counting requests so
//     callers and tests stay identical either way. The game NEVER crashes
//     on a missing device.
//   * **Bounded voices**: a fixed pool of kMaxVoices mixing slots with
//     round-robin reuse — rapid firing cannot grow memory or corrupt the
//     mix, and overlapping effects are allowed by construction.
//   * **Thread safety**: the SDL callback runs on its own thread;
//     mutations are guarded with SDL_LockAudioDevice.
class AudioManager {
public:
    // Mixing voice pool capacity.
    static constexpr int kMaxVoices = 8;

    AudioManager() = default;
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Test hook: force the silent path even when a device could be opened.
    void setSilent(bool silent) { forceSilent_ = silent; }

    // Opens the audio device (unless silenced) and synthesizes the clip
    // buffers. Safe to call again after shutdown().
    bool initialize();
    void shutdown();
    bool initialized() const { return initialized_; }
    bool available() const { return device_ != 0; }

    // Fire-and-forget playback. Always counted; audible only when a device
    // is open. Unknown ids are ignored safely.
    void playSound(SoundId id);
    void playMusic(MusicId id);
    void stopMusic();

    // Diagnostics/tests: how many times each id reached the manager.
    int playCount(SoundId id) const
    {
        return playCounts_[static_cast<int>(id)];
    }

    // ---- Deterministic mixer test hooks (no device needed) ----

    // Starts one voice for `id` bypassing the device state (test only).
    void debugTriggerVoice(SoundId id);
    // Runs the mixer for `samples` frames over the CURRENT voices and
    // returns the mono S16 output (test only).
    std::vector<Sint16> mixForTest(int samples);
    int activeVoicesForTest() const;

private:
    struct Buffer {
        std::vector<Sint16> samples;   // final mix-ready samples
        std::vector<float> fsamples;   // synthesis staging (cleared after)
        bool looping = false;
    };
    struct Voice {
        const Buffer* buffer = nullptr;
        int position = 0;   // sample index within the buffer
        bool active = false;
    };

    void generateClips();
    static void mixCallback(void* userdata, Uint8* stream, int len);

    bool forceSilent_ = false;
    bool initialized_ = false;
    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};

    std::vector<Buffer> soundBuffers_;
    Buffer musicBuffer_;

    Voice musicVoice_;  // dedicated looping channel
    std::array<Voice, kMaxVoices> voices_{};
    int nextVoice_ = 0;

    std::array<int, kSoundIdCount> playCounts_{{}};
};

}  // namespace galaxian
