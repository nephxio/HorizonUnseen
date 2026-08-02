#include "Audio/AudioEngine.h"

#include "Audio/WavLoader.h"
#include "Core/Log.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace hu {

namespace {

constexpr const char* kLogCategory = "Audio";

// Minimum gap between two plays of the same sound. Without it a frame that
// resolves forty bullet impacts fires forty copies of one buffer a millisecond
// apart, which sums into a click rather than reading as forty hits.
constexpr float kRetriggerSeconds = 0.035f;

// The manifest is small and fixed-shape, so it is read with a hand-rolled
// scanner rather than pulling in a JSON library for one file. Only the two
// fields the engine needs are extracted; anything else is skipped.
struct ManifestEntry {
    std::string name;
    std::string file;
    float gain = 1.0f;
};

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::string();
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Finds `"section": { ... }` and returns the entries inside it.
std::vector<ManifestEntry> parseSection(const std::string& text, const std::string& section) {
    std::vector<ManifestEntry> entries;

    const std::size_t sectionKey = text.find("\"" + section + "\"");
    if (sectionKey == std::string::npos) {
        return entries;
    }
    const std::size_t open = text.find('{', sectionKey);
    if (open == std::string::npos) {
        return entries;
    }

    // Walk to the matching brace so a later section cannot bleed in.
    int depth = 0;
    std::size_t close = open;
    for (std::size_t i = open; i < text.size(); ++i) {
        if (text[i] == '{') {
            ++depth;
        } else if (text[i] == '}') {
            if (--depth == 0) {
                close = i;
                break;
            }
        }
    }

    std::size_t cursor = open + 1;
    while (cursor < close) {
        const std::size_t nameStart = text.find('"', cursor);
        if (nameStart == std::string::npos || nameStart >= close) {
            break;
        }
        const std::size_t nameEnd = text.find('"', nameStart + 1);
        if (nameEnd == std::string::npos || nameEnd >= close) {
            break;
        }

        ManifestEntry entry;
        entry.name = text.substr(nameStart + 1, nameEnd - nameStart - 1);

        const std::size_t bodyOpen = text.find('{', nameEnd);
        if (bodyOpen == std::string::npos || bodyOpen >= close) {
            break;
        }
        const std::size_t bodyClose = text.find('}', bodyOpen);
        if (bodyClose == std::string::npos || bodyClose > close) {
            break;
        }
        const std::string body = text.substr(bodyOpen, bodyClose - bodyOpen);

        const std::size_t fileKey = body.find("\"file\"");
        if (fileKey != std::string::npos) {
            const std::size_t valueStart = body.find('"', body.find(':', fileKey));
            const std::size_t valueEnd = valueStart == std::string::npos
                                             ? std::string::npos
                                             : body.find('"', valueStart + 1);
            if (valueEnd != std::string::npos) {
                entry.file = body.substr(valueStart + 1, valueEnd - valueStart - 1);
            }
        }

        const std::size_t gainKey = body.find("\"gain\"");
        if (gainKey != std::string::npos) {
            entry.gain = static_cast<float>(std::atof(body.c_str() + body.find(':', gainKey) + 1));
        }

        if (!entry.file.empty()) {
            entries.push_back(entry);
        }
        cursor = bodyClose + 1;
    }

    return entries;
}

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct AudioEngine::Impl {
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;

    std::array<ALuint, SoundIdCount> soundBuffers{};
    std::array<float, SoundIdCount> soundGains{};
    std::array<float, SoundIdCount> lastPlayed{};

    std::array<ALuint, MusicIdCount> musicBuffers{};
    std::array<float, MusicIdCount> musicGains{};

    std::array<ALuint, VoiceCount> voices{};
    ALuint musicSource = 0;

    MusicId currentMusic = MusicId::Count;
    bool musicPlaying = false;

    // Seconds since init, used only for the retrigger throttle. A monotonic
    // counter rather than a wall clock so it cannot jump.
    float clock = 0.0f;
};

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init(const std::string& assetDirectory) {
    if (m_available) {
        return true;
    }

    m_impl = new Impl();
    m_impl->soundBuffers.fill(0);
    m_impl->soundGains.fill(1.0f);
    m_impl->lastPlayed.fill(-1000.0f);
    m_impl->musicBuffers.fill(0);
    m_impl->musicGains.fill(1.0f);
    m_impl->voices.fill(0);

    m_impl->device = alcOpenDevice(nullptr);
    if (m_impl->device == nullptr) {
        // The overwhelmingly common cause is a headless machine. Not an error.
        HU_LOG_WARN(kLogCategory,
                    "No audio device available; the game will run silently");
        shutdown();
        return false;
    }

    m_impl->context = alcCreateContext(m_impl->device, nullptr);
    if (m_impl->context == nullptr || alcMakeContextCurrent(m_impl->context) == ALC_FALSE) {
        HU_LOG_WARN(kLogCategory, "Could not create an audio context; running silently");
        shutdown();
        return false;
    }

    const ALCchar* name = alcGetString(m_impl->device, ALC_ALL_DEVICES_SPECIFIER);
    m_deviceName = name != nullptr ? name : "unknown device";

    alGenSources(static_cast<ALsizei>(VoiceCount), m_impl->voices.data());
    alGenSources(1, &m_impl->musicSource);
    if (alGetError() != AL_NO_ERROR) {
        HU_LOG_WARN(kLogCategory, "Could not allocate audio sources; running silently");
        shutdown();
        return false;
    }

    // Effects are panned by hand via source position, so distance attenuation
    // must not also apply: every sound should be heard at the gain it asks for.
    for (ALuint voice : m_impl->voices) {
        alSourcei(voice, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(voice, AL_ROLLOFF_FACTOR, 0.0f);
    }
    alSourcei(m_impl->musicSource, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(m_impl->musicSource, AL_ROLLOFF_FACTOR, 0.0f);
    alSourcei(m_impl->musicSource, AL_LOOPING, AL_TRUE);

    // --- load the manifest -------------------------------------------------
    const std::string manifestPath = assetDirectory + "/sounds.json";
    const std::string manifest = readFile(manifestPath);
    if (manifest.empty()) {
        HU_LOG_WARN(kLogCategory, "Could not read %s; running silently",
                    manifestPath.c_str());
        shutdown();
        return false;
    }

    auto loadInto = [&](const ManifestEntry& entry, ALuint& buffer) -> bool {
        WavData wav;
        std::string error;
        const std::string path = assetDirectory + "/" + entry.file;
        if (!loadWav(path, wav, &error)) {
            HU_LOG_ERROR(kLogCategory, "Could not load %s: %s", path.c_str(), error.c_str());
            return false;
        }

        alGenBuffers(1, &buffer);
        const ALenum format = wav.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        alBufferData(buffer, format, wav.samples.data(),
                     static_cast<ALsizei>(wav.samples.size()), wav.sampleRate);
        if (alGetError() != AL_NO_ERROR) {
            HU_LOG_ERROR(kLogCategory, "Could not upload %s", path.c_str());
            buffer = 0;
            return false;
        }
        return true;
    };

    const std::vector<ManifestEntry> sounds = parseSection(manifest, "sounds");
    for (std::size_t i = 0; i < SoundIdCount; ++i) {
        const char* wanted = soundName(static_cast<SoundId>(i));
        const auto found = std::find_if(sounds.begin(), sounds.end(),
                                        [&](const ManifestEntry& e) { return e.name == wanted; });
        if (found == sounds.end()) {
            HU_LOG_WARN(kLogCategory, "Sound '%s' is not in the manifest; it will be silent",
                        wanted);
            continue;
        }
        if (loadInto(*found, m_impl->soundBuffers[i])) {
            m_impl->soundGains[i] = found->gain;
            ++m_loadedSounds;
        }
    }

    const std::vector<ManifestEntry> music = parseSection(manifest, "music");
    for (std::size_t i = 0; i < MusicIdCount; ++i) {
        const char* wanted = musicName(static_cast<MusicId>(i));
        const auto found = std::find_if(music.begin(), music.end(),
                                        [&](const ManifestEntry& e) { return e.name == wanted; });
        if (found == music.end()) {
            HU_LOG_WARN(kLogCategory, "Music '%s' is not in the manifest", wanted);
            continue;
        }
        if (loadInto(*found, m_impl->musicBuffers[i])) {
            m_impl->musicGains[i] = found->gain;
            ++m_loadedSounds;
        }
    }

    m_available = true;
    HU_LOG_INFO(kLogCategory, "Audio ready on '%s': %zu buffers, %zu voices",
                m_deviceName.c_str(), m_loadedSounds, VoiceCount);
    return true;
}

void AudioEngine::shutdown() {
    if (m_impl == nullptr) {
        m_available = false;
        return;
    }

    if (m_impl->context != nullptr) {
        stopAll();

        if (m_impl->musicSource != 0) {
            alDeleteSources(1, &m_impl->musicSource);
        }
        if (m_impl->voices[0] != 0) {
            alDeleteSources(static_cast<ALsizei>(VoiceCount), m_impl->voices.data());
        }
        for (ALuint buffer : m_impl->soundBuffers) {
            if (buffer != 0) {
                alDeleteBuffers(1, &buffer);
            }
        }
        for (ALuint buffer : m_impl->musicBuffers) {
            if (buffer != 0) {
                alDeleteBuffers(1, &buffer);
            }
        }

        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_impl->context);
    }
    if (m_impl->device != nullptr) {
        alcCloseDevice(m_impl->device);
    }

    delete m_impl;
    m_impl = nullptr;
    m_available = false;
    m_loadedSounds = 0;

    HU_LOG_INFO(kLogCategory, "Audio shut down");
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void AudioEngine::play(const SoundEvent& event) {
    if (!m_available) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(event.id);
    if (index >= SoundIdCount || m_impl->soundBuffers[index] == 0) {
        return;
    }

    // Advancing here rather than in a separate update() keeps the throttle
    // working without the caller needing to remember to tick anything.
    m_impl->clock += 1.0f / 600.0f;
    if (m_impl->clock - m_impl->lastPlayed[index] < kRetriggerSeconds) {
        return;
    }

    ALuint chosen = 0;
    for (ALuint voice : m_impl->voices) {
        ALint state = 0;
        alGetSourcei(voice, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            chosen = voice;
            break;
        }
    }
    if (chosen == 0) {
        // Everything is busy. Dropping is better than stealing: the stolen
        // voice would click mid-sample, and one missing impact among 32
        // simultaneous ones is inaudible.
        return;
    }

    m_impl->lastPlayed[index] = m_impl->clock;

    const float gain = m_masterVolume * m_sfxVolume * m_impl->soundGains[index] * event.gain;
    const float pan = std::max(-1.0f, std::min(1.0f, event.pan));

    alSourcei(chosen, AL_BUFFER, static_cast<ALint>(m_impl->soundBuffers[index]));
    alSourcef(chosen, AL_GAIN, std::max(0.0f, gain));
    // Placed on the unit circle so panning hard left is no closer than centre;
    // a plain (pan, 0, 0) would also make edge sounds louder.
    alSource3f(chosen, AL_POSITION, pan, 0.0f, -std::sqrt(std::max(0.0f, 1.0f - pan * pan)));
    alSourcePlay(chosen);
}

void AudioEngine::playAll(const std::vector<SoundEvent>& events) {
    for (const SoundEvent& event : events) {
        play(event);
    }
}

void AudioEngine::playMusic(MusicId id) {
    if (!m_available) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= MusicIdCount || m_impl->musicBuffers[index] == 0) {
        return;
    }
    if (m_impl->musicPlaying && m_impl->currentMusic == id) {
        return;   // already playing: do not restart it every frame
    }

    alSourceStop(m_impl->musicSource);
    alSourcei(m_impl->musicSource, AL_BUFFER, static_cast<ALint>(m_impl->musicBuffers[index]));
    m_impl->currentMusic = id;
    m_impl->musicPlaying = true;
    applyMusicGain();
    alSourcePlay(m_impl->musicSource);
}

void AudioEngine::stopMusic() {
    if (!m_available) {
        return;
    }
    alSourceStop(m_impl->musicSource);
    m_impl->musicPlaying = false;
    m_impl->currentMusic = MusicId::Count;
}

void AudioEngine::stopAll() {
    if (m_impl == nullptr || m_impl->context == nullptr) {
        return;
    }
    for (ALuint voice : m_impl->voices) {
        if (voice != 0) {
            alSourceStop(voice);
        }
    }
    if (m_impl->musicSource != 0) {
        alSourceStop(m_impl->musicSource);
    }
    m_impl->musicPlaying = false;
    m_impl->currentMusic = MusicId::Count;
}

// ---------------------------------------------------------------------------
// Mixing
// ---------------------------------------------------------------------------

void AudioEngine::applyMusicGain() {
    if (!m_available || !m_impl->musicPlaying) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(m_impl->currentMusic);
    const float trackGain = index < MusicIdCount ? m_impl->musicGains[index] : 1.0f;
    alSourcef(m_impl->musicSource,
              AL_GAIN,
              std::max(0.0f, m_masterVolume * m_musicVolume * trackGain));
}

void AudioEngine::setMasterVolume(float volume) {
    m_masterVolume = std::max(0.0f, std::min(1.0f, volume));
    applyMusicGain();
}

void AudioEngine::setSfxVolume(float volume) {
    // Only affects sounds started from now on; one-shots are short enough that
    // retroactively regaining playing voices is not worth the bookkeeping.
    m_sfxVolume = std::max(0.0f, std::min(1.0f, volume));
}

void AudioEngine::setMusicVolume(float volume) {
    m_musicVolume = std::max(0.0f, std::min(1.0f, volume));
    applyMusicGain();
}

std::size_t AudioEngine::activeVoiceCount() const {
    if (!m_available) {
        return 0;
    }
    std::size_t count = 0;
    for (ALuint voice : m_impl->voices) {
        ALint state = 0;
        alGetSourcei(voice, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            ++count;
        }
    }
    return count;
}

} // namespace hu
