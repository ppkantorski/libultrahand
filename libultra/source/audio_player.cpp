#include "audio_player.hpp"

namespace ult {
    bool AudioPlayer::m_initialized = false;
    std::atomic<bool> AudioPlayer::m_enabled{true};  // <- atomic initialization
    float AudioPlayer::m_masterVolume = 0.6f;
    bool AudioPlayer::m_lastDockedState = false;
    std::vector<AudioPlayer::CachedSound> AudioPlayer::m_cachedSounds;
    std::mutex AudioPlayer::m_audioMutex;
    
    bool AudioPlayer::initialize() {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        if (m_initialized) return true;
        
        if (R_FAILED(audoutInitialize()) || R_FAILED(audoutStartAudioOut())) {
            audoutExit();
            return false;
        }
        
        m_initialized = true;
        m_cachedSounds.resize(static_cast<uint32_t>(SoundType::Count));
        m_lastDockedState = isDocked();
        reloadAllSounds();
        
        return true;
    }
    
    void AudioPlayer::exit() {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        for (auto& c : m_cachedSounds)
            free(c.buffer);
        m_cachedSounds.clear();
        
        if (m_initialized) {
            audoutStopAudioOut();
            audoutExit();
            m_initialized = false;
        }
    }
    
    void AudioPlayer::reloadAllSounds() {
        for (uint32_t i = 0; i < static_cast<uint32_t>(SoundType::Count); ++i) {
            loadSoundFromWav(static_cast<SoundType>(i), m_soundPaths[i]);
        }
    }
    
    bool AudioPlayer::reloadIfDockedChanged() {
        if (!m_initialized) return false;
        
        const bool currentDocked = isDocked();
        if (currentDocked == m_lastDockedState) return false;
        
        std::lock_guard<std::mutex> lock(m_audioMutex);
        m_lastDockedState = currentDocked;
        reloadAllSounds();
        
        return true;
    }
    
    bool AudioPlayer::loadSoundFromWav(SoundType type, const char* path) {
        const uint32_t idx = static_cast<uint32_t>(type);
        if (!m_initialized || idx >= static_cast<uint32_t>(SoundType::Count)) return false;
    
        free(m_cachedSounds[idx].buffer);
        m_cachedSounds[idx] = { nullptr, 0, 0 };
    
        FILE* f = fopen(path, "rb");
        if (!f) return false;
    
        char hdr[12];
        if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
            fclose(f);
            return false;
        }
    
        u16 fmt = 0, ch = 0, bits = 0;
        u32 rate = 0, dSize = 0;
        long dPos = 0;
    
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
        
        float effectiveVolume = m_masterVolume;
        if (m_lastDockedState) {
            effectiveVolume *= 0.5f;
        }
        const float scale = std::clamp(effectiveVolume, 0.0f, 1.0f);
    
        if (bits == 8) {
            std::vector<u8> tmp(inSamp);
            if (fread(tmp.data(), 1, inSamp, f) != inSamp) {
                free(buf);
                fclose(f);
                return false;
            }
        
            const int32_t scaleInt = static_cast<int32_t>(scale * 256.0f);
        
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
        // Lock-free check - SAFE with atomic
        if (!m_enabled.load(std::memory_order_relaxed)) return;
    
        const uint32_t idx = static_cast<uint32_t>(type);
        if (idx >= static_cast<uint32_t>(SoundType::Count)) return;
    
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        // Check again under lock
        if (!m_initialized) return;
        
        auto& cached = m_cachedSounds[idx];
        if (!cached.buffer) return;
    
        AudioOutBuffer* releasedBuffers = nullptr;
        u32 releasedCount = 0;
        audoutGetReleasedAudioOutBuffer(&releasedBuffers, &releasedCount);
    
        // Your static pattern is SAFE - mutex protects it
        static AudioOutBuffer audioBuffer = {};
        audioBuffer = {};
        audioBuffer.buffer = cached.buffer;
        audioBuffer.buffer_size = cached.bufferSize;
        audioBuffer.data_size = cached.dataSize;
        audioBuffer.data_offset = 0;
        audioBuffer.next = nullptr;
    
        AudioOutBuffer* rel = nullptr;
        audoutPlayBuffer(&audioBuffer, &rel);
    }
    
    //void AudioPlayer::playAudioBuffer(void* buffer, uint32_t sz) {
    //    if (!m_enabled.load(std::memory_order_relaxed) || !buffer) return;
    //    
    //    std::lock_guard<std::mutex> lock(m_audioMutex);
    //    
    //    if (!m_initialized) return;
    //    
    //    AudioOutBuffer* releasedBuffers = nullptr;
    //    u32 releasedCount = 0;
    //    audoutGetReleasedAudioOutBuffer(&releasedBuffers, &releasedCount);
    //    
    //    static AudioOutBuffer audioBuffer = {};
    //    audioBuffer = {};
    //    audioBuffer.next = nullptr;
    //    audioBuffer.buffer = buffer;
    //    audioBuffer.buffer_size = sz;
    //    audioBuffer.data_size = sz;
    //    audioBuffer.data_offset = 0;
    //    
    //    AudioOutBuffer* rel = nullptr;
    //    audoutPlayBuffer(&audioBuffer, &rel);
    //}
    
    void AudioPlayer::setMasterVolume(float v) { 
        std::lock_guard<std::mutex> lock(m_audioMutex);
        m_masterVolume = std::clamp(v, 0.0f, 1.0f);
    }
    
    void AudioPlayer::setEnabled(bool e) { 
        m_enabled.store(e, std::memory_order_relaxed);  // <- atomic, no lock needed
    }
    
    bool AudioPlayer::isEnabled() { 
        return m_enabled.load(std::memory_order_relaxed);  // <- atomic, no lock needed
    }
    
    bool AudioPlayer::isDocked() {
        Result rc = apmInitialize();
        if (R_FAILED(rc)) return false;
        
        ApmPerformanceMode perfMode = ApmPerformanceMode_Invalid;
        rc = apmGetPerformanceMode(&perfMode);
        apmExit();
        
        return R_SUCCEEDED(rc) && (perfMode == ApmPerformanceMode_Boost);
    }
}