#pragma once

// The audio device.
//
// Sits at the same layer as the renderer: gameplay produces SoundEvents the way
// it produces a DrawList, and the application hands them here. Nothing in
// Gameplay/ or Core/ links this library, which is what keeps husim and the
// headless build free of any OpenAL dependency.
//
// Failure is never fatal. A machine with no sound device -- every CI runner, and
// plenty of real ones -- leaves the engine unavailable and every call becomes a
// no-op. The game must be playable in silence, so nothing here throws and
// nothing returns an error the caller is obliged to handle.

#include "Core/SoundId.h"

#include <cstddef>
#include <string>
#include <vector>

namespace hu {

class AudioEngine {
public:
    // Simultaneous one-shots. Past this, the quietest playing voice is stolen.
    static constexpr std::size_t VoiceCount = 32;

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Opens the device and loads every sound named in <assetDirectory>/sounds.json.
    // Returns false when audio is unavailable; the caller may ignore that and
    // keep calling, which is the point.
    bool init(const std::string& assetDirectory = "assets");
    void shutdown();

    bool isAvailable() const { return m_available; }

    // Name of the device actually opened, for the debug overlay and logs.
    const std::string& deviceName() const { return m_deviceName; }

    // --- playback ----------------------------------------------------------
    void play(const SoundEvent& event);
    void playAll(const std::vector<SoundEvent>& events);

    // Switching to the track already playing is a no-op rather than a restart,
    // so calling this every frame from a state machine is safe.
    void playMusic(MusicId id);
    void stopMusic();
    void stopAll();

    // --- mixing ------------------------------------------------------------
    // All three are clamped to [0, 1]. Master scales both buses.
    void setMasterVolume(float volume);
    void setSfxVolume(float volume);
    void setMusicVolume(float volume);

    float masterVolume() const { return m_masterVolume; }
    float sfxVolume() const { return m_sfxVolume; }
    float musicVolume() const { return m_musicVolume; }

    // --- diagnostics -------------------------------------------------------
    std::size_t loadedSoundCount() const { return m_loadedSounds; }
    std::size_t activeVoiceCount() const;

private:
    struct Impl;

    void applyMusicGain();

    // Pimpl so no OpenAL header leaks into anything that includes this. The
    // UI and application only ever need the interface above.
    Impl* m_impl = nullptr;

    bool m_available = false;
    std::string m_deviceName;
    std::size_t m_loadedSounds = 0;

    float m_masterVolume = 0.8f;
    float m_sfxVolume = 1.0f;
    float m_musicVolume = 0.6f;
};

} // namespace hu
