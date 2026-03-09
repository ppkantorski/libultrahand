/********************************************************************************
 * File: audio.cpp
 * Author: ppkantorski
 * Description:
 *   Render-thread-safe audio with a single shared DMA playback buffer.
 *   Key design:
 *   - rawBuf     : only per-sound allocation — compact native-channel 16-bit PCM,
 *                  native sample rate, no volume. Kept for re-render on vol/dock change.
 *   - m_playBuf  : single shared DMA-ready buffer, sized to the largest sound's
 *                  48 kHz stereo output. All sounds share this one allocation.
 *   - renderToPlayBuf() runs inside playSound(): resamples to 48 kHz (linear interp),
 *                  expands mono → L+R, and applies volume in one pass.
 *   - No per-sound stereo copy is kept — memory cost per sound is rawBuf only.
 *   - m_playBuf is safe to reuse because audout is always drained before writing.
 *   - Volume and dock state are read live at render time — no stale tracking needed.
 *   - Supports any WAV sample rate ≤ 48 kHz (8/11025/16000/22050/32000/44100/48000 Hz).
 *     Source rates > 48 kHz are rejected at load time.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2025-2026 ppkantorski
 ********************************************************************************/

#include "audio.hpp"

namespace ult {

    // ── Static member definitions ─────────────────────────────────────────────
    bool                          Audio::m_initialized     = false;
    std::atomic<bool>             Audio::m_enabled{true};
    std::atomic<int32_t>          Audio::m_masterVolumeFixed{154};  // 0.6 * 256 ≈ 154
    bool                          Audio::m_lastDockedState  = false;
    std::vector<Audio::CachedSound> Audio::m_cachedSounds;
    std::mutex                    Audio::m_audioMutex;
    void*                         Audio::m_playBuf          = nullptr;
    uint32_t                      Audio::m_playBufCap       = 0;
    AudioOutBuffer                Audio::m_audoutBuf        = {};

    // 4 KB — required by Switch audout DMA
    static constexpr uint32_t AUDIO_ALIGN  = 0x1000;
    static constexpr uint32_t TARGET_RATE  = 48000;

    // ── initialize ────────────────────────────────────────────────────────────
    bool Audio::initialize() {
        std::lock_guard<std::mutex> lock(m_audioMutex);

        if (m_initialized) return true;

        if (R_FAILED(audoutInitialize()) || R_FAILED(audoutStartAudioOut())) {
            audoutExit();
            return false;
        }

        m_initialized = true;
        m_cachedSounds.resize(static_cast<uint32_t>(SoundType::Count));
        m_lastDockedState = ult::consoleIsDocked();

        reloadAllSounds();

        return true;
    }

    // ── exit ──────────────────────────────────────────────────────────────────
    void Audio::exit() {
        std::lock_guard<std::mutex> lock(m_audioMutex);

        for (auto& s : m_cachedSounds) {
            free(s.rawBuf);
            s = CachedSound{};
        }

        free(m_playBuf);
        m_playBuf    = nullptr;
        m_playBufCap = 0;
        m_audoutBuf  = {};

        if (m_initialized) {
            audoutStopAudioOut();
            audoutExit();
            m_initialized = false;
        }
    }

    // ── reloadAllSounds ───────────────────────────────────────────────────────
    void Audio::reloadAllSounds() {
        for (uint32_t i = 0; i < static_cast<uint32_t>(SoundType::Count); ++i)
            loadSoundFromWav(static_cast<SoundType>(i), m_soundPaths[i]);
        // growPlayBuf() is called inside loadSoundFromWav after each load,
        // so m_playBuf is always sized to the current high-water mark.
    }

    // ── unloadAllSounds ───────────────────────────────────────────────────────
    void Audio::unloadAllSounds(const std::initializer_list<SoundType>& excludeSounds) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        if (!m_initialized) return;

        for (uint32_t i = 0; i < m_cachedSounds.size(); ++i) {
            const SoundType cur = static_cast<SoundType>(i);
            if (std::find(excludeSounds.begin(), excludeSounds.end(), cur) != excludeSounds.end())
                continue;

            CachedSound& s = m_cachedSounds[i];
            free(s.rawBuf);
            s = CachedSound{};
        }
        // m_playBuf stays allocated — it will be reused when any remaining sound plays.
    }

    // ── reloadIfDockedChanged ─────────────────────────────────────────────────
    // Volume and dock state are read live in renderToPlayBuf(), so no stale
    // marking is needed here — just update m_lastDockedState.
    bool Audio::reloadIfDockedChanged() {
        if (!m_initialized) return false;

        const bool currentDocked = ult::consoleIsDocked();
        if (currentDocked == m_lastDockedState) return false;

        std::lock_guard<std::mutex> lock(m_audioMutex);
        m_lastDockedState = currentDocked;
        return true;
    }

    // ── growPlayBuf ───────────────────────────────────────────────────────────
    // Computes the 48 kHz stereo output size needed for every currently loaded
    // sound and grows m_playBuf to cover the largest one.
    // Called after each loadSoundFromWav. Must hold m_audioMutex.
    void Audio::growPlayBuf() {
        uint32_t maxNeeded = 0;

        for (const auto& s : m_cachedSounds) {
            if (!s.rawBuf || s.rawSize == 0) continue;

            const uint32_t srcPerChan = s.isMono
                ? (s.rawSize / sizeof(s16))
                : (s.rawSize / sizeof(s16)) / 2;

            const uint32_t outPerChan = (s.sampleRate == TARGET_RATE || s.sampleRate == 0)
                ? srcPerChan
                : static_cast<uint32_t>(
                    ((uint64_t)srcPerChan * TARGET_RATE + s.sampleRate - 1) / s.sampleRate);

            const uint32_t stereoBytes = outPerChan * 2 * sizeof(s16);
            const uint32_t needed      = (stereoBytes + AUDIO_ALIGN - 1) & ~(AUDIO_ALIGN - 1);
            if (needed > maxNeeded) maxNeeded = needed;
        }

        if (maxNeeded <= m_playBufCap) return;  // already large enough

        free(m_playBuf);
        m_playBuf = aligned_alloc(AUDIO_ALIGN, maxNeeded);
        if (m_playBuf) {
            m_playBufCap = maxNeeded;
        } else {
            m_playBufCap = 0;
        }
    }

    // ── renderToPlayBuf ───────────────────────────────────────────────────────
    // Writes rawBuf → m_playBuf: resample to 48 kHz (linear interp if needed),
    // expand mono → L+R, apply current volume and dock attenuation.
    // Returns actual output byte count written, or 0 on error.
    // Must be called under m_audioMutex.
    uint32_t Audio::renderToPlayBuf(const CachedSound& s) {
        if (!s.rawBuf || s.rawSize == 0 || !m_playBuf) return 0;

        const uint32_t srcSamples = s.rawSize / sizeof(s16);
        const uint32_t srcPerChan = s.isMono ? srcSamples : srcSamples / 2;

        const uint32_t outPerChan = (s.sampleRate == TARGET_RATE || s.sampleRate == 0)
            ? srcPerChan
            : static_cast<uint32_t>(
                ((uint64_t)srcPerChan * TARGET_RATE + s.sampleRate - 1) / s.sampleRate);

        const uint32_t stereoBytes = outPerChan * 2 * sizeof(s16);
        const uint32_t needed      = (stereoBytes + AUDIO_ALIGN - 1) & ~(AUDIO_ALIGN - 1);

        if (needed > m_playBufCap) return 0;  // shouldn't happen after growPlayBuf()

        // Effective volume: master * 0.5 when docked (TV speaker protection).
        // Fixed-point: 0–256 where 256 == 1.0.
        int32_t vol = m_masterVolumeFixed.load(std::memory_order_relaxed);
        if (m_lastDockedState) vol >>= 1;

        const s16* src = static_cast<const s16*>(s.rawBuf);
        s16*       dst = static_cast<s16*>(m_playBuf);

        const bool needsResample = (s.sampleRate != TARGET_RATE && s.sampleRate != 0);

        if (!needsResample) {
            // ── Fast path: native 48 kHz — no resampling ─────────────────────
            if (s.isMono) {
                for (uint32_t i = 0; i < srcSamples; ++i) {
                    const s16 v = static_cast<s16>((static_cast<int32_t>(src[i]) * vol) >> 8);
                    *dst++ = v;  // L
                    *dst++ = v;  // R
                }
            } else {
                for (uint32_t i = 0; i < srcSamples; ++i)
                    *dst++ = static_cast<s16>((static_cast<int32_t>(src[i]) * vol) >> 8);
            }
        } else {
            // ── Resample path: linear interpolation to 48 kHz ────────────────
            // step is source frames per output frame in 16.16 fixed-point.
            // For all rates ≤ 48 kHz this is < 1.0 (upsampling).
            const uint64_t step = ((uint64_t)s.sampleRate << 16) / TARGET_RATE;
            uint64_t srcFixed = 0;

            if (s.isMono) {
                for (uint32_t i = 0; i < outPerChan; ++i) {
                    const uint32_t i0   = static_cast<uint32_t>(srcFixed >> 16);
                    const uint32_t i1   = (i0 + 1 < srcPerChan) ? i0 + 1 : i0;
                    const int32_t  frac = static_cast<int32_t>(srcFixed & 0xFFFF);
                    const int32_t  s0   = src[i0], s1 = src[i1];
                    const s16 v = static_cast<s16>(
                        ((s0 + (((s1 - s0) * frac) >> 16)) * vol) >> 8);
                    *dst++ = v;  // L
                    *dst++ = v;  // R
                    srcFixed += step;
                }
            } else {
                // Stereo interleaved: index [frame*2+0] = L, [frame*2+1] = R
                for (uint32_t i = 0; i < outPerChan; ++i) {
                    const uint32_t i0   = static_cast<uint32_t>(srcFixed >> 16);
                    const uint32_t i1   = (i0 + 1 < srcPerChan) ? i0 + 1 : i0;
                    const int32_t  frac = static_cast<int32_t>(srcFixed & 0xFFFF);
                    const int32_t  l0 = src[i0*2],     l1 = src[i1*2];
                    const int32_t  r0 = src[i0*2 + 1], r1 = src[i1*2 + 1];
                    *dst++ = static_cast<s16>(((l0 + (((l1-l0)*frac)>>16)) * vol) >> 8);
                    *dst++ = static_cast<s16>(((r0 + (((r1-r0)*frac)>>16)) * vol) >> 8);
                    srcFixed += step;
                }
            }
        }

        // Zero-fill alignment padding
        if (stereoBytes < needed)
            memset(static_cast<u8*>(m_playBuf) + stereoBytes, 0, needed - stereoBytes);

        return stereoBytes;
    }

    // ── loadSoundFromWav ──────────────────────────────────────────────────────
    // Reads WAV → rawBuf (16-bit, native channels, no volume applied).
    // Rejects source rates > 48 kHz. Calls growPlayBuf() after a successful load.
    // Must be called under m_audioMutex.
    bool Audio::loadSoundFromWav(SoundType type, const char* path) {
        const uint32_t idx = static_cast<uint32_t>(type);
        if (!m_initialized || idx >= static_cast<uint32_t>(SoundType::Count)) return false;

        CachedSound& s = m_cachedSounds[idx];

        free(s.rawBuf);
        s = CachedSound{};  // reset all fields

        FILE* f = fopen(path, "rb");
        if (!f) return false;

        // ── RIFF/WAVE header ──────────────────────────────────────────────────
        char hdr[12];
        if (fread(hdr, 1, 12, f) != 12 ||
            memcmp(hdr,     "RIFF", 4) ||
            memcmp(hdr + 8, "WAVE", 4)) {
            fclose(f); return false;
        }

        u16  fmt = 0, ch = 0, bits = 0;
        u32  rate = 0, dSize = 0;
        long dPos = 0;

        // ── Chunk scan ────────────────────────────────────────────────────────
        while (fread(hdr, 1, 8, f) == 8) {
            const u32 sz = *reinterpret_cast<const u32*>(hdr + 4);
            if (!memcmp(hdr, "fmt ", 4)) {
                fread(&fmt,  2, 1, f);
                fread(&ch,   2, 1, f);
                fread(&rate, 4, 1, f);
                fseek(f, 6, SEEK_CUR);  // skip byte rate + block align
                fread(&bits, 2, 1, f);
                fseek(f, (long)sz - 16, SEEK_CUR);
            } else if (!memcmp(hdr, "data", 4)) {
                dSize = sz;
                dPos  = ftell(f);
                break;
            } else {
                fseek(f, sz, SEEK_CUR);
            }
        }

        // ── Validate ──────────────────────────────────────────────────────────
        // Reject rates above 48 kHz — downsampling would expand rawBuf beyond its
        // target-rate output and defeat the purpose of small source files.
        if (!dSize || fmt != 1 || ch == 0 || ch > 2 ||
            (bits != 8 && bits != 16) || rate == 0 || rate > TARGET_RATE) {
            fclose(f); return false;
        }

        const uint32_t inSamples = dSize / (bits / 8);
        const uint32_t rawBytes  = inSamples * sizeof(s16);  // normalised to 16-bit
        const uint32_t rawCap    = (rawBytes + AUDIO_ALIGN - 1) & ~(AUDIO_ALIGN - 1);

        void* buf = aligned_alloc(AUDIO_ALIGN, rawCap);
        if (!buf) { fclose(f); return false; }

        fseek(f, dPos, SEEK_SET);
        s16*     out       = static_cast<s16*>(buf);
        uint32_t remaining = inSamples;
        uint32_t outIdx    = 0;

        // ── Chunked read + bit-depth normalisation ────────────────────────────
        constexpr uint32_t CHUNK = 512;

        if (bits == 8) {
            u8 chunk[CHUNK];
            while (remaining > 0) {
                const uint32_t toRead = std::min(remaining, CHUNK);
                if (fread(chunk, 1, toRead, f) != toRead) {
                    free(buf); fclose(f); return false;
                }
                for (uint32_t i = 0; i < toRead; ++i)
                    out[outIdx++] = static_cast<s16>((static_cast<int32_t>(chunk[i]) - 128) << 8);
                remaining -= toRead;
            }
        } else {
            s16 chunk[CHUNK];
            while (remaining > 0) {
                const uint32_t toRead = std::min(remaining, CHUNK);
                if (fread(chunk, sizeof(s16), toRead, f) != toRead) {
                    free(buf); fclose(f); return false;
                }
                memcpy(out + outIdx, chunk, toRead * sizeof(s16));
                outIdx    += toRead;
                remaining -= toRead;
            }
        }

        fclose(f);

        if (rawBytes < rawCap)
            memset(static_cast<u8*>(buf) + rawBytes, 0, rawCap - rawBytes);

        s.rawBuf     = buf;
        s.rawSize    = rawBytes;
        s.rawCap     = rawCap;
        s.sampleRate = rate;
        s.isMono     = (ch == 1);

        // Grow shared play buffer if this sound's 48 kHz output would exceed it.
        growPlayBuf();
        return (m_playBuf != nullptr);
    }

    // ── playSound ─────────────────────────────────────────────────────────────
    // Drains the audout queue, renders the sound into the shared play buffer,
    // then submits. Volume and dock attenuation are applied live inside render.
    void Audio::playSound(SoundType type) {
        if (!m_enabled.load(std::memory_order_relaxed)) return;

        const uint32_t idx = static_cast<uint32_t>(type);
        if (idx >= static_cast<uint32_t>(SoundType::Count)) return;

        std::lock_guard<std::mutex> lock(m_audioMutex);
        if (!m_initialized || !m_playBuf) return;

        const CachedSound& s = m_cachedSounds[idx];
        if (!s.rawBuf) return;  // sound file not loaded

        // Drain finished buffers so audout's queue stays healthy and so we know
        // the shared buffer is no longer in use by a previous submission.
        AudioOutBuffer* released = nullptr;
        u32 releasedCount = 0;
        audoutGetReleasedAudioOutBuffer(&released, &releasedCount);

        const uint32_t outBytes = renderToPlayBuf(s);
        if (outBytes == 0) return;

        const uint32_t bufCap = (outBytes + AUDIO_ALIGN - 1) & ~(AUDIO_ALIGN - 1);

        m_audoutBuf              = {};
        m_audoutBuf.buffer       = m_playBuf;
        m_audoutBuf.buffer_size  = bufCap;
        m_audoutBuf.data_size    = outBytes;
        m_audoutBuf.data_offset  = 0;
        m_audoutBuf.next         = nullptr;

        AudioOutBuffer* rel = nullptr;
        audoutPlayBuffer(&m_audoutBuf, &rel);
    }

    // ── Volume / enable accessors ─────────────────────────────────────────────

    // Volume is read live in renderToPlayBuf() — no stale marking needed.
    void Audio::setMasterVolume(float v) {
        const int32_t fixed = static_cast<int32_t>(std::clamp(v, 0.0f, 1.0f) * 256.0f);
        m_masterVolumeFixed.store(fixed, std::memory_order_relaxed);
    }

    void Audio::setEnabled(bool e) {
        m_enabled.store(e, std::memory_order_relaxed);
    }

    bool Audio::isEnabled() {
        return m_enabled.load(std::memory_order_relaxed);
    }

} // namespace ult