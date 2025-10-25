#include "audio_player.hpp"

bool AudioPlayer::m_initialized = false;
bool AudioPlayer::m_enabled = true;
float AudioPlayer::m_masterVolume = 0.6f;
std::vector<AudioPlayer::CachedSound> AudioPlayer::m_cachedSounds;

bool AudioPlayer::initialize() {
    if (m_initialized) return true;
    
    if (R_FAILED(audoutInitialize()) || R_FAILED(audoutStartAudioOut())) {
        audoutExit();
        return false;
    }
    
    m_initialized = true;
    m_cachedSounds.resize(static_cast<uint32_t>(SoundType::Count));
    
    // Load sounds
    static const char* paths[] = {
        "sdmc:/config/ultrahand/sounds/tick.wav",
        "sdmc:/config/ultrahand/sounds/enter.wav",
        "sdmc:/config/ultrahand/sounds/exit.wav",
        "sdmc:/config/ultrahand/sounds/wall.wav",
        "sdmc:/config/ultrahand/sounds/on.wav",
        "sdmc:/config/ultrahand/sounds/off.wav",
        "sdmc:/config/ultrahand/sounds/settings.wav",
        "sdmc:/config/ultrahand/sounds/move.wav"
    };
    
    for (uint32_t i = 0; i < static_cast<uint32_t>(SoundType::Count); ++i)
        loadSoundFromWav(static_cast<SoundType>(i), paths[i]);
    
    return true;
}

void AudioPlayer::exit() {
    for (auto& c : m_cachedSounds)
        free(c.buffer);
    m_cachedSounds.clear();
    
    if (m_initialized) {
        audoutStopAudioOut();
        audoutExit();
        m_initialized = false;
    }
}

bool AudioPlayer::loadSoundFromWav(SoundType type, const char* path) {
    const uint32_t idx = static_cast<uint32_t>(type);
    if (!m_initialized || idx >= static_cast<uint32_t>(SoundType::Count)) return false;

    // Free old buffer
    free(m_cachedSounds[idx].buffer);
    m_cachedSounds[idx] = { nullptr, 0, 0 };

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    // Quick RIFF/WAVE header check
    char hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        fclose(f);
        return false;
    }

    u16 fmt = 0, ch = 0, bits = 0;
    u32 rate = 0, dSize = 0;
    long dPos = 0;

    // Locate "fmt " and "data" chunks
    while (fread(hdr, 1, 8, f) == 8) {
        const u32 sz = *(u32*)(hdr + 4);
        if (!memcmp(hdr, "fmt ", 4)) {
            fread(&fmt, 2, 1, f);
            fread(&ch, 2, 1, f);
            fread(&rate, 4, 1, f);
            fseek(f, 6, SEEK_CUR);
            fread(&bits, 2, 1, f);
            fseek(f, sz - 16, SEEK_CUR);
        } else if (!memcmp(hdr, "data", 4)) {
            dSize = sz;
            dPos = ftell(f);
            break;
        } else {
            fseek(f, sz, SEEK_CUR);
        }
    }

    if (!dSize || fmt != 1 || ch == 0 || ch > 2 || (bits != 8 && bits != 16)) {
        fclose(f);
        return false;
    }

    const bool mono = (ch == 1);
    const uint32_t inSamp = dSize / (bits / 8);
    const uint32_t outSamp = mono ? inSamp * 2 : inSamp;
    const uint32_t outSize = outSamp * 2;
    const uint32_t align = outSize < 16384 ? 0x100 : 0x1000;
    const uint32_t bufSize = (outSize + align - 1) & ~(align - 1);

    void* buf = aligned_alloc(align, bufSize);
    if (!buf) {
        fclose(f);
        return false;
    }

    fseek(f, dPos, SEEK_SET);
    s16* out = (s16*)buf;
    const float scale = std::clamp(m_masterVolume, 0.0f, 1.0f);

    if (bits == 8) {
        std::vector<u8> tmp(inSamp);
        if (fread(tmp.data(), 1, inSamp, f) != inSamp) {
            free(buf);
            fclose(f);
            return false;
        }
    
        // Precompute scale as 32-bit integer multiplier
        const int32_t scaleInt = static_cast<int32_t>(scale * 256.0f); // scale in 0..256 range
    
        for (uint32_t i = 0; i < inSamp; ++i) {
            s16 v = static_cast<s16>((tmp[i] - 128) * scaleInt);
            if (mono) {
                out[i * 2] = out[i * 2 + 1] = v;
            } else {
                out[i] = v;
            }
        }
    } else {
        std::vector<s16> tmp(inSamp);
        if (fread(tmp.data(), sizeof(s16), inSamp, f) != inSamp) {
            free(buf);
            fclose(f);
            return false;
        }

        for (uint32_t i = 0; i < inSamp; ++i) {
            s16 v = (s16)(tmp[i] * scale);
            out[mono ? i * 2 : i] = v;
            if (mono) out[i * 2 + 1] = v;
        }
    }

    fclose(f);

    if (outSize < bufSize)
        memset((u8*)buf + outSize, 0, bufSize - outSize);

    m_cachedSounds[idx] = { buf, bufSize, outSize };
    return true;
}

void AudioPlayer::playSound(SoundType type) {
    if (!m_initialized || !m_enabled) return;

    const uint32_t idx = static_cast<uint32_t>(type);
    if (idx >= static_cast<uint32_t>(SoundType::Count)) return;

    auto& cached = m_cachedSounds[idx];
    if (!cached.buffer) return;

    // Static buffer per sound
    static AudioOutBuffer ab[static_cast<uint32_t>(SoundType::Count)] = {};

    ab[idx] = {};                  // Reset previous values
    ab[idx].buffer = cached.buffer;
    ab[idx].buffer_size = cached.bufferSize;
    ab[idx].data_size = cached.dataSize;  // Use actual audio data
    ab[idx].data_offset = 0;
    ab[idx].next = nullptr;

    // Play buffer; rel is optional and can be ignored
    AudioOutBuffer* rel = nullptr;
    audoutPlayBuffer(&ab[idx], &rel);
}

void AudioPlayer::playAudioBuffer(void* buffer, uint32_t sz) {
    if (m_initialized && m_enabled && buffer) {
        AudioOutBuffer ab = {};
        ab.next = nullptr;
        ab.buffer = buffer;
        ab.buffer_size = (u32)sz;
        ab.data_size = (u32)sz;
        ab.data_offset = 0;
        AudioOutBuffer* rel = nullptr;
        audoutPlayBuffer(&ab, &rel);
    }
}

void AudioPlayer::setMasterVolume(float v) { m_masterVolume = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
void AudioPlayer::setEnabled(bool e) { m_enabled = e; }
bool AudioPlayer::isEnabled() { return m_enabled; }

void AudioPlayer::playNavigateSound() { playSound(SoundType::Navigate); }
void AudioPlayer::playEnterSound() { playSound(SoundType::Enter); }
void AudioPlayer::playExitSound() { playSound(SoundType::Exit); }
void AudioPlayer::playWallSound() { playSound(SoundType::Wall); }
void AudioPlayer::playOnSound() { playSound(SoundType::On); }
void AudioPlayer::playOffSound() { playSound(SoundType::Off); }
void AudioPlayer::playSettingsSound() { playSound(SoundType::Settings); }
void AudioPlayer::playMoveSound() { playSound(SoundType::Move); }