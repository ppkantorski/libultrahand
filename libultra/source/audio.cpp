/********************************************************************************
 * File: audio.cpp
 * Author: ppkantorski
 * Description:
 *   Render-thread-safe audio with pre-baked stereo buffers.
 *   Key design:
 *   - rawBuf  : compact native-channel 16-bit PCM, no volume, kept for rebaking
 *   - sterBuf : pre-baked DMA-ready stereo PCM (mono→L+R + volume applied)
 *   - bakeStereo() runs at load time; reruns only when volume/dock state changes
 *   - sterBuf capacity is fixed at first bake — rebakes always reuse same allocation
 *     (same sound = same sample count = same output size), so no alloc on rebake
 *   - playSound() hot path: drain audout queue + submit — no loops, no allocs
 *   - Each sound owns its AudioOutBuffer descriptor; no shared-buffer DMA race
 *   - setMasterVolume() and reloadIfDockedChanged() mark sounds stale; rebake is
 *     deferred to next playSound() call for that specific sound
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
    bool                     Audio::m_initialized    = false;
    std::atomic<bool>        Audio::m_enabled{true};
    std::atomic<int32_t>     Audio::m_masterVolumeFixed{154};  // 0.6 * 256 = 153.6 ≈ 154
    bool                     Audio::m_lastDockedState = false;
    std::vector<Audio::CachedSound> Audio::m_cachedSounds;
    std::mutex               Audio::m_audioMutex;

    // 4 KB — required by Switch audout DMA
    static constexpr uint32_t AUDIO_ALIGN = 0x1000;

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

        // Load + eagerly bake every sound. After this, every playSound is zero-work
        // until a volume or dock state change occurs.
        reloadAllSounds();

        return true;
    }

    // ── exit ──────────────────────────────────────────────────────────────────
    void Audio::exit() {
        std::lock_guard<std::mutex> lock(m_audioMutex);

        for (auto& s : m_cachedSounds) {
            free(s.rawBuf);
            free(s.sterBuf);
            s = CachedSound{};
        }

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
            free(s.sterBuf);
            s = CachedSound{};
        }
    }

    // ── reloadIfDockedChanged ─────────────────────────────────────────────────
    // Marks all sounds stale — no file I/O, no expansion work.
    // Each sound rebakes from its rawBuf on its next playSound call.
    bool Audio::reloadIfDockedChanged() {
        if (!m_initialized) return false;

        const bool currentDocked = ult::consoleIsDocked();
        if (currentDocked == m_lastDockedState) return false;

        std::lock_guard<std::mutex> lock(m_audioMutex);
        m_lastDockedState = currentDocked;
        for (auto& s : m_cachedSounds)
            s.stale = true;

        return true;
    }

    // ── bakeStereo ────────────────────────────────────────────────────────────
    // Converts rawBuf → sterBuf: mono expanded to L+R, volume applied.
    //
    // sterBuf is sized once at first call (same sound = same sample count forever),
    // so all subsequent rebakes (volume/dock change) overwrite in-place with no
    // allocation. Returns false only if the initial aligned_alloc fails.
    //
    // Must be called under m_audioMutex.
    bool Audio::bakeStereo(CachedSound& s) {
        if (!s.stale) return true;
        if (!s.rawBuf || s.rawSize == 0) return false;

        const uint32_t srcSamples    = s.rawSize / sizeof(s16);
        const uint32_t stereoSamples = s.isMono ? srcSamples * 2 : srcSamples;
        const uint32_t stereoBytes   = stereoSamples * sizeof(s16);
        const uint32_t needed        = (stereoBytes + AUDIO_ALIGN - 1) & ~(AUDIO_ALIGN - 1);

        // First call: allocate. Subsequent calls: reuse (same sound = same size).
        if (needed > s.sterCap) {
            free(s.sterBuf);
            s.sterBuf = aligned_alloc(AUDIO_ALIGN, needed);
            if (!s.sterBuf) { s.sterCap = 0; return false; }
            s.sterCap = needed;
        }

        // Compute effective volume: master * 0.5 when docked (TV speaker protection).
        // Fixed-point: 0–256 where 256 == 1.0.
        int32_t vol = m_masterVolumeFixed.load(std::memory_order_relaxed);
        if (m_lastDockedState) vol >>= 1;

        const s16* src = static_cast<const s16*>(s.rawBuf);
        s16*       dst = static_cast<s16*>(s.sterBuf);

        if (s.isMono) {
            // Expand mono → stereo and apply volume in one pass.
            // (sample * vol) >> 8  ≡  sample * (vol / 256.0)  — integer only.
            for (uint32_t i = 0; i < srcSamples; ++i) {
                const s16 v = static_cast<s16>((static_cast<int32_t>(src[i]) * vol) >> 8);
                *dst++ = v;  // L
                *dst++ = v;  // R
            }
        } else {
            for (uint32_t i = 0; i < srcSamples; ++i)
                *dst++ = static_cast<s16>((static_cast<int32_t>(src[i]) * vol) >> 8);
        }

        // Zero-fill alignment padding
        if (stereoBytes < needed)
            memset(static_cast<u8*>(s.sterBuf) + stereoBytes, 0, needed - stereoBytes);

        s.sterSize = stereoBytes;
        s.stale    = false;
        return true;
    }

    // ── loadSoundFromWav ──────────────────────────────────────────────────────
    // Reads WAV → rawBuf (16-bit, native channel count, no volume applied).
    // Immediately calls bakeStereo so the first playSound is zero-work.
    // Must be called under m_audioMutex.
    bool Audio::loadSoundFromWav(SoundType type, const char* path) {
        const uint32_t idx = static_cast<uint32_t>(type);
        if (!m_initialized || idx >= static_cast<uint32_t>(SoundType::Count)) return false;

        CachedSound& s = m_cachedSounds[idx];

        // Free raw buffer. Keep sterBuf alive — bakeStereo will overwrite it.
        free(s.rawBuf);
        s.rawBuf  = nullptr;
        s.rawSize = 0;
        s.rawCap  = 0;
        s.isMono  = false;
        s.stale   = true;

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
                fseek(f, 6, SEEK_CUR);            // skip byte rate + block align
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
        if (!dSize || fmt != 1 || ch == 0 || ch > 2 || (bits != 8 && bits != 16)) {
            fclose(f); return false;
        }

        const uint32_t inSamples = dSize / (bits / 8);
        const uint32_t rawBytes  = inSamples * sizeof(s16);  // always normalised to 16-bit
        const uint32_t rawCap    = (rawBytes + AUDIO_ALIGN - 1) & ~(AUDIO_ALIGN - 1);

        void* buf = aligned_alloc(AUDIO_ALIGN, rawCap);
        if (!buf) { fclose(f); return false; }

        fseek(f, dPos, SEEK_SET);
        s16*     out       = static_cast<s16*>(buf);
        uint32_t remaining = inSamples;
        uint32_t outIdx    = 0;

        // ── Chunked read + bit-depth normalisation (no volume here) ───────────
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
            // 16-bit: bulk copy per chunk, no per-sample work
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

        s.rawBuf  = buf;
        s.rawSize = rawBytes;
        s.rawCap  = rawCap;
        s.isMono  = (ch == 1);
        s.stale   = true;

        // Eagerly bake so the very first playSound for this sound is zero-work.
        return bakeStereo(s);
    }

    // ── playSound ─────────────────────────────────────────────────────────────
    // Hot path (normal case — no volume change since last play):
    //   lock → drain audout queue (syscall) → submit (syscall) → unlock
    //
    // No expansion loop, no allocation, no per-sample math on this thread.
    // The stale branch only runs after setMasterVolume() or a dock state change,
    // neither of which occurs during active sound-effect use.
    void Audio::playSound(SoundType type) {
        if (!m_enabled.load(std::memory_order_relaxed)) return;

        const uint32_t idx = static_cast<uint32_t>(type);
        if (idx >= static_cast<uint32_t>(SoundType::Count)) return;

        std::lock_guard<std::mutex> lock(m_audioMutex);
        if (!m_initialized) return;

        CachedSound& s = m_cachedSounds[idx];
        if (!s.rawBuf) return;  // sound file not loaded

        // Keep audout's internal queue healthy by draining finished buffers.
        AudioOutBuffer* released = nullptr;
        u32 releasedCount = 0;
        audoutGetReleasedAudioOutBuffer(&released, &releasedCount);

        // Rebake only when stale (volume or dock state changed).
        // On the normal hot path this branch is never taken.
        if (s.stale && !bakeStereo(s)) return;

        // Fill the per-sound descriptor and submit.
        // Using a per-sound audoutBuf (not shared) means there is no DMA race:
        // each sound's sterBuf is only written by bakeStereo under the mutex,
        // and bakeStereo rewrites in-place (no free/realloc after first bake).
        s.audoutBuf              = {};
        s.audoutBuf.buffer       = s.sterBuf;
        s.audoutBuf.buffer_size  = s.sterCap;   // DMA-aligned capacity
        s.audoutBuf.data_size    = s.sterSize;  // actual stereo PCM bytes
        s.audoutBuf.data_offset  = 0;
        s.audoutBuf.next         = nullptr;

        AudioOutBuffer* rel = nullptr;
        audoutPlayBuffer(&s.audoutBuf, &rel);
    }

    // ── Volume / enable accessors ─────────────────────────────────────────────

    // Updates the fixed-point volume and marks all loaded sounds stale.
    // Rebaking is deferred to each sound's next playSound() call — no
    // expansion work happens on this thread.
    void Audio::setMasterVolume(float v) {
        const int32_t fixed = static_cast<int32_t>(std::clamp(v, 0.0f, 1.0f) * 256.0f);
        m_masterVolumeFixed.store(fixed, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(m_audioMutex);
        for (auto& s : m_cachedSounds)
            s.stale = true;
    }

    void Audio::setEnabled(bool e) {
        m_enabled.store(e, std::memory_order_relaxed);
    }

    bool Audio::isEnabled() {
        return m_enabled.load(std::memory_order_relaxed);
    }

} // namespace ult