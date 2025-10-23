#include "audio_player.hpp"
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool AudioPlayer::m_initialized = false;
bool AudioPlayer::m_enabled = true;
float AudioPlayer::m_masterVolume = 0.6f;
//std::vector<AudioPlayer::SoundConfig> AudioPlayer::m_soundConfigs;
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
    //m_soundConfigs.clear();

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

    // Only free old buffer if not playing
    if (m_cachedSounds[index].buffer && !m_cachedSounds[index].playing) {
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
    u32 dataSize = 0;
    long dataChunkPos = 0;

    while (!feof(file)) {
        char chunkId[4];
        u32 chunkSize;
        if (fread(chunkId, 4, 1, file) != 1 || fread(&chunkSize, 4, 1, file) != 1) break;

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            fread(&audioFormat, 2, 1, file);
            fread(&numChannels, 2, 1, file);
            u32 sampleRate; fread(&sampleRate, 4, 1, file);
            u32 byteRate; fread(&byteRate, 4, 1, file);
            u16 blockAlign; fread(&blockAlign, 2, 1, file);
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

    const size_t bufferSize = (dataSize + 0xFFF) & ~0xFFF;
    void* buffer = aligned_alloc(0x1000, bufferSize);
    if (!buffer) {
        fclose(file);
        return false;
    }

    fseek(file, dataChunkPos, SEEK_SET);
    fread(buffer, 1, dataSize, file);
    fclose(file);

    // Apply master volume scaling
    s16* samples = static_cast<s16*>(buffer);
    const size_t sampleCount = dataSize / sizeof(s16);
    const float scale = std::clamp(m_masterVolume, 0.0f, 1.0f);
    for (size_t i = 0; i < sampleCount; ++i) {
        const float val = static_cast<float>(samples[i]) * scale;
        samples[i] = static_cast<s16>(std::max(-32768.0f, std::min(32767.0f, val)));
    }
    memset((u8*)buffer + dataSize, 0, bufferSize - dataSize);

    // Assign buffer only if old one not playing
    if (!m_cachedSounds[index].playing)
        m_cachedSounds[index] = {buffer, bufferSize, false};

    return true;
}

// -------------------- Default Sound Config --------------------

void AudioPlayer::initializeDefaultConfigs() {
    const size_t soundCount = static_cast<size_t>(SoundType::Count);
    //m_soundConfigs.resize(soundCount);
    m_cachedSounds.resize(soundCount);

    // Navigate (tick)
    //m_soundConfigs[static_cast<int>(SoundType::Navigate)] = {
    //    6000.0f, 0.007f, 0.45f, 0.0005f, 0.002f, true, "Navigate"
    //};
    //
    //// Enter (confirmation)
    //m_soundConfigs[static_cast<int>(SoundType::Enter)] = {
    //    4000.0f, 0.015f, 0.55f, 0.0008f, 0.004f, true, "Enter"
    //};
    //
    //// Exit (closing/back)
    //m_soundConfigs[static_cast<int>(SoundType::Exit)] = {
    //    3000.0f, 0.012f, 0.5f, 0.0008f, 0.004f, true, "Exit"
    //};

    // Try loading WAVs; fall back to generated tones
    loadSoundFromWav(SoundType::Navigate, "sdmc:/config/ultrahand/sounds/tick.wav");
    loadSoundFromWav(SoundType::Enter,    "sdmc:/config/ultrahand/sounds/enter.wav");
    loadSoundFromWav(SoundType::Exit,     "sdmc:/config/ultrahand/sounds/exit.wav");

    //if (!tickLoaded || !enterLoaded || !exitLoaded)
    //    pregenerateSoundBuffers();
}

// -------------------- Sound Generation --------------------

//void AudioPlayer::pregenerateSoundBuffers() {
//    if (!m_initialized) return;
//
//    const u32 sampleRate = audoutGetSampleRate();
//    const u32 channelCount = audoutGetChannelCount();
//
//    for (size_t i = 0; i < m_soundConfigs.size(); ++i) {
//        if (m_cachedSounds[i].buffer) continue;
//
//        const auto& config = m_soundConfigs[i];
//        if (config.duration == 0.0f) continue;
//
//        size_t neededSamples = static_cast<size_t>(sampleRate * config.duration);
//        size_t neededSize = neededSamples * channelCount * sizeof(s16);
//
//        size_t bufferSize = (neededSize + 0xFFF) & ~0xFFF;
//        bufferSize = std::max(bufferSize, static_cast<size_t>(0x1000));
//        bufferSize = std::min(bufferSize, static_cast<size_t>(0x4000));
//
//        void* buffer = aligned_alloc(0x1000, bufferSize);
//        if (buffer) {
//            generateTone(buffer, bufferSize, config);
//            m_cachedSounds[i] = {buffer, bufferSize, false};
//        }
//    }
//}

//void AudioPlayer::applyEnvelope(float* sample, float time, float duration, float attack, float decay) {
//    float env = 1.0f;
//    if (time < attack) env = time / attack;
//    else if (time > duration - decay) env = (duration - time) / decay;
//    env = std::max(0.0f, std::min(1.0f, env));
//    *sample *= env;
//}
//
//void AudioPlayer::generateTone(void* buffer, size_t size, const SoundConfig& config) {
//    const u32 sr = audoutGetSampleRate();
//    const u32 ch = audoutGetChannelCount();
//    const size_t totalSamples = size / (ch * sizeof(s16));
//    const size_t samplesToGenerate = static_cast<size_t>(sr * config.duration);
//    const size_t actualSamples = std::min(samplesToGenerate, totalSamples);
//
//    s16* samples = static_cast<s16*>(buffer);
//    const float amp = config.volume * m_masterVolume * 32767.0f * 1.8f;
//    const float srInv = 1.0f / sr;
//
//    std::memset(buffer, 0, size);
//
//    for (size_t i = 0; i < actualSamples; ++i) {
//        float t = static_cast<float>(i) * srInv;
//        float val = ((float)rand() / RAND_MAX - 0.5f) * 1.6f;
//        if (i > 0) {
//            float prev = samples[(i - 1) * ch] / 32767.0f;
//            val = val * 0.4f + prev * 0.6f;
//        }
//        float decay = std::exp(-200.0f * t);
//        val *= amp * decay;
//        if (t < 0.0005f) val *= (t / 0.0005f);
//        if (i > 4) val += 0.06f * samples[(i - 4) * ch] / 32767.0f;
//        if (config.useEnvelope)
//            applyEnvelope(&val, t, config.duration, config.attack, config.decay);
//        s16 s = static_cast<s16>(std::max(-32768.0f, std::min(32767.0f, val)));
//        for (u32 c = 0; c < ch; ++c)
//            samples[i * ch + c] = s;
//    }
//}

// -------------------- Playback --------------------

void AudioPlayer::playSound(SoundType type) {
    if (!m_initialized || !m_enabled) return;
    const int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(SoundType::Count)) return;

    auto& cached = m_cachedSounds[index];
    if (!cached.buffer) return;

    // Prevent overwriting buffer while playing
    if (cached.playing) return;

    AudioOutBuffer aBuf = {};
    aBuf.buffer = cached.buffer;
    aBuf.buffer_size = static_cast<u32>(cached.bufferSize);
    aBuf.data_size = static_cast<u32>(cached.bufferSize);
    aBuf.next = nullptr;

    cached.playing = true;
    AudioOutBuffer* released = nullptr;
    audoutPlayBuffer(&aBuf, &released);

    if (released == &aBuf) cached.playing = false;
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

void AudioPlayer::setEnabled(bool e) { m_enabled = e; }
bool AudioPlayer::isEnabled() { return m_enabled; }

// -------------------- Tesla Compatibility --------------------

void AudioPlayer::playNavigateSound() { playSound(SoundType::Navigate); }
void AudioPlayer::playEnterSound()    { playSound(SoundType::Enter); }
void AudioPlayer::playExitSound()     { playSound(SoundType::Exit); }