#include "audio_player.hpp"

bool AudioPlayer::m_initialized = false;
bool AudioPlayer::m_enabled = true;
float AudioPlayer::m_masterVolume = 0.6f;
std::vector<AudioPlayer::CachedSound> AudioPlayer::m_cachedSounds;

// -------------------- Initialization --------------------

bool AudioPlayer::initialize() {
    if (m_initialized) return true;

    Result rc = audoutInitialize();
    if (R_SUCCEEDED(rc)) {
        rc = audoutStartAudioOut();
        if (R_SUCCEEDED(rc)) {
            m_initialized = true;
            initializeDefaultConfigs();
            return true;
        }
        audoutExit();
    }
    return false;
}

void AudioPlayer::exit() {
    for (auto& cached : m_cachedSounds) {
        if (cached.buffer) {
            free(cached.buffer);
            cached.buffer = nullptr;
        }
    }
    m_cachedSounds.clear();

    if (m_initialized) {
        audoutStopAudioOut();
        audoutExit();
        m_initialized = false;
    }
}

// -------------------- WAV Loading --------------------

bool AudioPlayer::loadSoundFromWav(SoundType type, const char* path) {
    if (!m_initialized) return false;

    const int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(SoundType::Count)) return false;

    // Free old buffer if it exists
    if (m_cachedSounds[index].buffer) {
        free(m_cachedSounds[index].buffer);
        m_cachedSounds[index] = {nullptr, 0, false};
    }

    FILE* file = fopen(path, "rb");
    if (!file) return false;

    char riff[4], wave[4];
    u32 fileSize;
    if (fread(riff, 4, 1, file) != 1 || fread(&fileSize, 4, 1, file) != 1 || fread(wave, 4, 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        fclose(file);
        return false;
    }

    u16 audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    u32 sampleRate = 0, byteRate = 0, dataSize = 0;
    u16 blockAlign = 0;
    long dataChunkPos = 0;

    while (!feof(file)) {
        char chunkId[4];
        u32 chunkSize;
        if (fread(chunkId, 4, 1, file) != 1 || fread(&chunkSize, 4, 1, file) != 1) break;

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            fread(&audioFormat, 2, 1, file);
            fread(&numChannels, 2, 1, file);
            fread(&sampleRate, 4, 1, file);
            fread(&byteRate, 4, 1, file);
            fread(&blockAlign, 2, 1, file);
            fread(&bitsPerSample, 2, 1, file);
            fseek(file, chunkSize - 16, SEEK_CUR);
        } else if (strncmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            dataChunkPos = ftell(file);
            break;
        } else {
            fseek(file, chunkSize, SEEK_CUR);
        }
    }

    if (dataSize == 0 || audioFormat != 1 || bitsPerSample != 16 || numChannels == 0 || numChannels > 2) {
        fclose(file);
        return false;
    }

    const bool isMono = (numChannels == 1);
    const size_t inputSamples = dataSize / sizeof(s16);
    const size_t outputSamples = isMono ? inputSamples * 2 : inputSamples;
    const size_t outputDataSize = outputSamples * sizeof(s16);

    // Use smaller alignment for small sounds to save memory
    const size_t alignment = outputDataSize < 16384 ? 0x100 : 0x1000;
    const size_t bufferSize = (outputDataSize + (alignment - 1)) & ~(alignment - 1);

    void* buffer = aligned_alloc(alignment, bufferSize);
    if (!buffer) {
        fclose(file);
        return false;
    }

    fseek(file, dataChunkPos, SEEK_SET);
    s16* outputSamples16 = static_cast<s16*>(buffer);
    const float scale = std::clamp(m_masterVolume, 0.0f, 1.0f);

    if (isMono) {
        // Read mono and expand to stereo in-place
        std::vector<s16> monoSamples(inputSamples);
        if (fread(monoSamples.data(), sizeof(s16), inputSamples, file) != inputSamples) {
            free(buffer);
            fclose(file);
            return false;
        }

        for (size_t i = 0; i < inputSamples; ++i) {
            const int32_t val = static_cast<int32_t>(monoSamples[i] * scale);
            const s16 scaledSample = static_cast<s16>(
                val < -32768 ? -32768 : (val > 32767 ? 32767 : val)
            );

            outputSamples16[i * 2]     = scaledSample; // Left
            outputSamples16[i * 2 + 1] = scaledSample; // Right
        }
    } else {
        // Read stereo directly and apply volume
        if (fread(buffer, 1, dataSize, file) != dataSize) {
            free(buffer);
            fclose(file);
            return false;
        }
        for (size_t i = 0; i < inputSamples; ++i) {
            const int32_t val = static_cast<int32_t>(outputSamples16[i] * scale);
            outputSamples16[i] = static_cast<s16>(
                val < -32768 ? -32768 : (val > 32767 ? 32767 : val)
            );
        }
    }

    fclose(file);

    // Only zero padding if actually needed
    if (outputDataSize < bufferSize) {
        memset(static_cast<u8*>(buffer) + outputDataSize, 0, bufferSize - outputDataSize);
    }

    m_cachedSounds[index] = {buffer, bufferSize, false};
    return true;
}

// -------------------- Default Sound Config --------------------

void AudioPlayer::initializeDefaultConfigs() {
    const size_t soundCount = static_cast<size_t>(SoundType::Count);
    m_cachedSounds.resize(soundCount);

    // Try loading WAVs
    loadSoundFromWav(SoundType::Navigate, "sdmc:/config/ultrahand/sounds/tick.wav");
    loadSoundFromWav(SoundType::Enter,    "sdmc:/config/ultrahand/sounds/enter.wav");
    loadSoundFromWav(SoundType::Exit,     "sdmc:/config/ultrahand/sounds/exit.wav");
    loadSoundFromWav(SoundType::Wall,     "sdmc:/config/ultrahand/sounds/wall.wav");
    loadSoundFromWav(SoundType::On,       "sdmc:/config/ultrahand/sounds/on.wav");
    loadSoundFromWav(SoundType::Off,      "sdmc:/config/ultrahand/sounds/off.wav");
}

// -------------------- Playback --------------------

void AudioPlayer::playSound(SoundType type) {
    if (!m_initialized || !m_enabled) return;
    
    const int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(SoundType::Count)) return;

    auto& cached = m_cachedSounds[index];
    if (!cached.buffer) return;

    // Simple fire-and-forget playback
    AudioOutBuffer aBuf = {};
    aBuf.buffer = cached.buffer;
    aBuf.buffer_size = static_cast<u32>(cached.bufferSize);
    aBuf.data_size = static_cast<u32>(cached.bufferSize);
    aBuf.next = nullptr;

    AudioOutBuffer* released = nullptr;
    audoutPlayBuffer(&aBuf, &released);
}

void AudioPlayer::playAudioBuffer(void* buffer, size_t bufferSize) {
    if (!m_initialized || !m_enabled || !buffer) return;

    AudioOutBuffer aBuf = {};
    aBuf.buffer = buffer;
    aBuf.buffer_size = static_cast<u32>(bufferSize);
    aBuf.data_size = static_cast<u32>(bufferSize);
    aBuf.next = nullptr;

    AudioOutBuffer* released = nullptr;
    audoutPlayBuffer(&aBuf, &released);
}

// -------------------- Settings --------------------

void AudioPlayer::setMasterVolume(float v) {
    m_masterVolume = std::max(0.0f, std::min(1.0f, v));
}

void AudioPlayer::setEnabled(bool e) { 
    m_enabled = e; 
}

bool AudioPlayer::isEnabled() { 
    return m_enabled; 
}

// -------------------- Tesla Compatibility --------------------

void AudioPlayer::playNavigateSound() { 
    playSound(SoundType::Navigate); 
}

void AudioPlayer::playEnterSound() { 
    playSound(SoundType::Enter); 
}

void AudioPlayer::playExitSound() { 
    playSound(SoundType::Exit); 
}

void AudioPlayer::playWallSound() { 
    playSound(SoundType::Wall); 
}

void AudioPlayer::playOnSound() { 
    playSound(SoundType::On); 
}

void AudioPlayer::playOffSound() { 
    playSound(SoundType::Off); 
}
