/********************************************************************************
 * File: animated.cpp
 * Author: ppkantorski
 * Description: 
 *   'animated.cpp' implements animated wallpapers (.uaw) for the Ultrahand Overlay
 *   project: streaming the frame records from the SD card on a low-priority reader
 *   thread and decoding them into the wallpaper buffer on the render thread.
 *   The file format is documented in extra/make_uaw.py.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository:
 *   GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *   Note: This notice is integral to the project's documentation and must not be 
 *   altered or removed.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2026 ppkantorski
 ********************************************************************************/

#include <animated.hpp>
#include <tsl_utils.hpp>
#include <glass.hpp>
#include <image_fx.hpp>
#include <cstdio>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <arm_neon.h>

namespace ult::anim {

    std::atomic<bool> s_active{false};
    u32 s_ringCapLimit = 0;

    namespace {

        // ── Format and playback state ──────────────────────────────────────────
        constexpr u32 kW = 448, kH = 720, kBW = kW / 4, kBH = kH / 4, kBlocks = kBW * kBH;
        constexpr u32 kBlockBytes = 10;
        constexpr u32 kKeyBytes = 1 + kBlocks * kBlockBytes;      // a KEY record's payload
        constexpr u32 kWrap = 0xFFFFFFFFu;                         // ring marker: continue at 0

        FILE*  s_file = nullptr;
        u32    s_frames = 0, s_first = 0, s_maxRec = 0;
        u64    s_periodNs = 0;                                     // one video frame

        // Ring of upcoming records: [u32 len][payload], back to back (kWrap = go to 0).
        // One producer (the reader thread) and one consumer (the render thread).
        u8*    s_ring = nullptr;
        u32    s_ringSize = 0;
        std::atomic<u64> s_wPos{0}, s_rPos{0};                     // byte positions, never wrap

        Thread s_thread{};
        bool   s_threadUp = false;
        std::atomic<bool> s_quit{false};
        UEvent s_space;                                            // consumer -> producer: room freed
        bool   s_eventInit = false;

        // Where the reader starts (set by startFromPath)
        u32    s_readIdx = 0, s_readLen = 0;

        // Playback position for resuming in the next overlay: the next record to show and
        // its file offset. It moves as records are shown, not as they are read ahead.
        u32    s_playIdx = 0;
        u64    s_playOff = 0;

        std::string s_path;                                        // the playing .uaw (state check)
        u64    s_fileSize = 0;
        u64    s_nextDueNs = 0;                                    // when the next frame is due

    #if USING_LIVE_LOGGING_DIRECTIVE
        u32    s_statFrames = 0, s_statUnder = 0;
        u64    s_statDecNs = 0, s_statT0 = 0;
    #endif


        // ── Reader thread ──────────────────────────────────────────────────────

        // Waits until `bytes` are free in the ring. False when quitting.
        bool waitFree(u64 bytes) {
            while (s_ringSize - (s_wPos.load(std::memory_order_relaxed) - s_rPos.load(std::memory_order_acquire)) < bytes) {
                if (s_quit.load(std::memory_order_acquire)) return false;

                // No timeout: pop() and closeAll() both signal, and the event stays set until
                // waited on, so a signal between the check above and this wait isn't lost.
                // With a full ring (overlay hidden, playback held) the thread just sleeps.
                waitSingle(waiterForUEvent(&s_space), UINT64_MAX);
            }

            return !s_quit.load(std::memory_order_acquire);
        }

        // Reads record after record into the ring, from where startFromPath() stopped. After
        // the last one it seeks back to record 0 (a KEY). Each record is one fread: its
        // payload plus the next record's length.
        void readerMain(void*) {
            u32 idx = s_readIdx, len = s_readLen;
            bool ok = true;

            while (ok && !s_quit.load(std::memory_order_acquire)) {
                if (len == 0 || len > s_maxRec) {
                    ok = false;
                    break;
                }

                const bool last = idx + 1 == s_frames;
                const u32 need = 4 + len + (last ? 0 : 4);         // + the next record's length
                u64 w = s_wPos.load(std::memory_order_relaxed);
                u32 off = static_cast<u32>(w % s_ringSize);

                // Doesn't fit before the end of the ring: wrap around
                if (s_ringSize - off < need) {
                    const u32 tail = s_ringSize - off;
                    if (!waitFree(tail)) break;
                    if (tail >= 4) memcpy(s_ring + off, &kWrap, 4);
                    w += tail;
                    s_wPos.store(w, std::memory_order_release);
                    off = 0;
                }

                if (!waitFree(need)) break;

                u8* const dst = s_ring + off;
                memcpy(dst, &len, 4);
                if (fread(dst + 4, 1, len + (last ? 0 : 4), s_file) != len + (last ? 0 : 4)) {
                    ok = false;
                    break;
                }

                u32 nextLen = 0;
                if (!last) memcpy(&nextLen, dst + 4 + len, 4);      // (unaligned: memcpy)
                s_wPos.store(w + 4 + len, std::memory_order_release);

                if (last) {
                    // Loop: back to record 0 (a KEY)
                    idx = 0;
                    ok = fseek(s_file, s_first, SEEK_SET) == 0 && fread(&len, 4, 1, s_file) == 1;
                } else {
                    ++idx;
                    len = nextLen;
                }
            }

        #if USING_LIVE_LOGGING_DIRECTIVE
            if (!ok && ult::glassLog) glass::live::log("anim: read error, playback holds the last frame");
        #endif
        }


        // ── Decoding (render thread) ───────────────────────────────────────────
        // A block record is c0 (RGB), c1 (RGB), then 16 x 2-bit palette indices (pixel 0 in
        // the low bits, rows top to bottom). Palette: c0, c1, (2*c0 + c1) / 3, (c0 + 2*c1) / 3.
        // It's all NEON: the palette is built in one vector, the 16 indices are expanded to
        // bytes at once, and every block row is one table lookup and one store.

        // Palette as 12 bytes, R,G,B x {c0, c1, mix1, mix2} (lanes 12..15 unused)
        inline uint8x16_t paletteRGB(const u8* rec) {
            const uint8x8_t v  = vld1_u8(rec);                       // r0 g0 b0 r1 g1 b1 i0 i1
            const uint8x8_t c1 = vext_u8(v, v, 3);                   // r1 g1 b1 ...
            const uint16x8_t s1 = vaddq_u16(vshll_n_u8(v, 1), vmovl_u8(c1));   // 2*c0 + c1 (<= 765)
            const uint16x8_t s2 = vaddq_u16(vmovl_u8(v), vshll_n_u8(c1, 1));   // c0 + 2*c1

            // x / 3 for x <= 765: (x * 21846) >> 16, exact over that range
            auto div3 = [](uint16x8_t x) {
                const uint32x4_t lo = vmull_n_u16(vget_low_u16(x), 21846);
                const uint32x4_t hi = vmull_n_u16(vget_high_u16(x), 21846);
                return vmovn_u16(vcombine_u16(vshrn_n_u32(lo, 16), vshrn_n_u32(hi, 16)));
            };

            const uint8x16_t t = vcombine_u8(v, div3(s1));             // 0..5 = c0 c1, 8..10 = mix1
            const uint8x16_t u = vcombine_u8(div3(s2), vdup_n_u8(0));   // 0..2 = mix2
            static const uint8x16_t kPick = { 0, 1, 2, 3, 4, 5, 8, 9, 10, 16, 17, 18, 0, 0, 0, 0 };

            return vqtbl2q_u8(uint8x16x2_t{{ t, u }}, kPick);
        }

        // The 16 indices (2 bits each, pixel 0 in the low bits) as bytes, in pixel order
        // (lane = y*4 + x)
        inline uint8x16_t indices(const u8* src) {
            u32 bits;
            memcpy(&bits, src, 4);

            static const uint8x16_t kByte  = { 0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3 };
            static const int8x16_t  kShift = { 0,-2,-4,-6, 0,-2,-4,-6, 0,-2,-4,-6, 0,-2,-4,-6 };
            const uint8x16_t b = vqtbl1q_u8(vreinterpretq_u8_u32(vdupq_n_u32(bits)), kByte);

            return vandq_u8(vshlq_u8(b, kShift), vdupq_n_u8(3));
        }

        // The wallpaper adjustments on a palette (12 bytes, R,G,B x 4 colours). Every pixel of
        // the block is one of these colours, so this is the same as adjusting all 16 pixels.
        inline uint8x16_t adjustPalette(uint8x16_t pal, const WallAdj& adj) {
            static const uint8x8_t kR = { 0, 3, 6, 9, 0, 0, 0, 0 }, kG = { 1, 4, 7, 10, 0, 0, 0, 0 },
                                   kB = { 2, 5, 8, 11, 0, 0, 0, 0 };
            uint8x8x3_t c{{ vqtbl1_u8(pal, kR), vqtbl1_u8(pal, kG), vqtbl1_u8(pal, kB) }};

            wallAdjLow4(adj, c);

            static const uint8x16_t kBack = { 0, 8, 16, 1, 9, 17, 2, 10, 18, 3, 11, 19, 0, 0, 0, 0 };
            return vqtbl2q_u8(uint8x16x2_t{{ vcombine_u8(c.val[0], c.val[1]), vcombine_u8(c.val[2], c.val[2]) }}, kBack);
        }

        // Stores one block from its palette and indices. Full colour: pal = R,G,B x 4 colours
        // (12 bytes, pre-multiplied RGB, 3 B/px; opaque, so plain RGB).
        inline void storeFull(u8* wall, u32 b, uint8x16_t pal, uint8x16_t idx) {
            // Byte k of the block's 48 output bytes = palette[3 * idx[k / 3] + k % 3]
            static const uint8x16_t kPix[3] = {
                { 0,0,0, 1,1,1, 2,2,2, 3,3,3, 4,4,4, 5 },
                { 5,5, 6,6,6, 7,7,7, 8,8,8, 9,9,9, 10,10 },
                { 10, 11,11,11, 12,12,12, 13,13,13, 14,14,14, 15,15,15 } };
            static const uint8x16_t kCh[3] = {
                { 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0 },
                { 1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1 },
                { 2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2 } };

            const u32 bx = b % kBW, by = b / kBW;
            const uint8x16_t tri = vmulq_u8(idx, vdupq_n_u8(3));
            alignas(16) u8 out[48];
            for (int k = 0; k < 3; ++k)
                vst1q_u8(out + 16 * k, vqtbl1q_u8(pal, vaddq_u8(vqtbl1q_u8(tri, kPix[k]), kCh[k])));

            u8* p = wall + (size_t(by) * 4 * kW + bx * 4) * 3;
            for (int y = 0; y < 4; ++y, p += kW * 3) memcpy(p, out + 12 * y, 12);
        }

        // RGBA4444 (2 B/px: r|g, b|a nibbles): tab = the 4 colours x 2 bytes
        inline void store4444(u8* wall, u32 b, uint8x8_t tab, uint8x16_t idx) {
            const u32 bx = b % kBW, by = b / kBW;
            const uint8x16_t i2 = vshlq_n_u8(idx, 1);                   // byte offset of each pixel
            const uint8x16x2_t z = vzipq_u8(i2, vaddq_u8(i2, vdupq_n_u8(1)));   // lo/hi byte per pixel
            const uint8x16_t r01 = vqtbl1q_u8(vcombine_u8(tab, tab), z.val[0]); // rows 0, 1
            const uint8x16_t r23 = vqtbl1q_u8(vcombine_u8(tab, tab), z.val[1]); // rows 2, 3

            u8* p = wall + (size_t(by) * 4 * kW + bx * 4) * 2;
            vst1_u8(p,              vget_low_u8(r01));
            vst1_u8(p + kW * 2,     vget_high_u8(r01));
            vst1_u8(p + kW * 4,     vget_low_u8(r23));
            vst1_u8(p + kW * 6,     vget_high_u8(r23));
        }

        // Writes one block (a 10-byte record) into the wallpaper buffer, RGBA4444 or full
        // colour. adj: the adjustments to apply, or nullptr for none.
        template <bool kFull>
        inline void putBlock(u8* wall, u32 b, const u8* rec, const WallAdj* adj) {
            uint8x16_t pal = paletteRGB(rec);
            if (adj) pal = adjustPalette(pal, *adj);                  // (8-bit, before any 4-bit cut)

            const uint8x16_t idx = indices(rec + 6);

            if constexpr (kFull) {
                storeFull(wall, b, pal, idx);
            } else {
                // 4444 palette: byte 2i = (r & F0) | (g >> 4), byte 2i+1 = (b & F0) | 0x0F
                static const uint8x8_t kR = { 0, 3, 6, 9, 0, 0, 0, 0 }, kG = { 1, 4, 7, 10, 0, 0, 0, 0 },
                                       kB = { 2, 5, 8, 11, 0, 0, 0, 0 };
                const uint8x8_t hiMask = vdup_n_u8(0xF0);
                const uint8x8_t lo = vorr_u8(vand_u8(vqtbl1_u8(pal, kR), hiMask), vshr_n_u8(vqtbl1_u8(pal, kG), 4));
                const uint8x8_t hi = vorr_u8(vand_u8(vqtbl1_u8(pal, kB), hiMask), vdup_n_u8(0x0F));
                store4444(wall, b, vzip1_u8(lo, hi), idx);              // 4 pixels x 2 bytes
            }
        }

        // Shifts the whole picture by (dx, dy) in place. Pixels shifted in keep their value.
        void shiftPicture(u8* wall, int dx, int dy, u32 bpp) {
            const int w = int(kW) - (dx < 0 ? -dx : dx), h = int(kH) - (dy < 0 ? -dy : dy);
            if (w <= 0 || h <= 0) return;

            const int sx = dx < 0 ? -dx : 0, tx = dx > 0 ? dx : 0, sy = dy < 0 ? -dy : 0;
            auto row = [&](int y) {
                memmove(wall + (size_t(y + dy) * kW + tx) * bpp, wall + (size_t(y) * kW + sx) * bpp, size_t(w) * bpp);
            };

            // Moving down: bottom row first
            if (dy > 0) for (int y = sy + h - 1; y >= sy; --y) row(y);
            else        for (int y = sy; y < sy + h; ++y) row(y);
        }

        // Decodes one record (payload p, len bytes) into wall, adjusted with adj (nullptr:
        // none). A shift moves pixels that are already adjusted, so it needs nothing.
        // False if the record is malformed.
        template <bool kFull>
        bool decodeRecord(u8* wall, const u8* p, u32 len, const WallAdj* adj) {
            const u8* const end = p + len;

            switch (p[0]) {
                case 0: return true;                                         // REPEAT

                case 1:                                                      // KEY
                    if (len != kKeyBytes) return false;
                    for (u32 b = 0; b < kBlocks; ++b) putBlock<kFull>(wall, b, p + 1 + b * kBlockBytes, adj);
                    return true;

                case 2: case 3: {                                            // DELTA
                    if (len < 3) return false;

                    const int dx = static_cast<s8>(p[1]), dy = static_cast<s8>(p[2]);
                    if (dx || dy) shiftPicture(wall, dx, dy, kFull ? 3 : 2);

                    if (p[0] == 2) {
                        // Changed blocks as a bitmap
                        const u8* const bm = p + 3;
                        constexpr u32 kBmBytes = (kBlocks + 7) / 8;          // 2520 = 315 x 8
                        const u8* q = bm + kBmBytes;
                        if (q > end) return false;

                        for (u32 i = 0; i < kBmBytes; i += 8) {             // 64 blocks per step
                            u64 m;
                            memcpy(&m, bm + i, 8);
                            while (m) {
                                const u32 b = i * 8 + static_cast<u32>(__builtin_ctzll(m));
                                m &= m - 1;
                                if (b >= kBlocks || q + kBlockBytes > end) return false;
                                putBlock<kFull>(wall, b, q, adj);
                                q += kBlockBytes;
                            }
                        }
                    } else {
                        // Changed blocks as runs: (skip, count) pairs
                        if (len < 5) return false;

                        u16 nr;
                        memcpy(&nr, p + 3, 2);
                        const u8* runs = p + 5;
                        const u8* q = runs + size_t(nr) * 4;
                        if (q > end) return false;

                        u32 pos = 0;
                        for (u32 k = 0; k < nr; ++k) {
                            u16 sk, cnt;
                            memcpy(&sk, runs + k * 4, 2);
                            memcpy(&cnt, runs + k * 4 + 2, 2);
                            pos += sk;
                            if (pos + cnt > kBlocks || q + size_t(cnt) * kBlockBytes > end) return false;

                            for (u32 j = 0; j < cnt; ++j) {
                                putBlock<kFull>(wall, pos++, q, adj);
                                q += kBlockBytes;
                            }
                        }
                    }
                    return true;
                }
            }

            return false;
        }


        // ── Saved picture (animated.cache) ─────────────────────────────────────
        // A decoded block is one of 4 colours (its record's palette), so the cache keeps it that
        // way. Per band of 112 blocks (4 rows): a 14-byte mask (bit set = packed), then each block
        // packed (4 colours + the 2-bit indices: 16 B full colour, 12 B RGBA4444) or as it is
        // (48 / 32 B; a shift can leave more colours in a block). The picture comes back exactly.
        constexpr u32 kMaskBytes = kBW / 8;
        constexpr u32 kBandMax = kMaskBytes + kBW * 48;           // a band at its largest

        // Copies one 4-pixel row (12 or 8 bytes)
        inline void copyRow(u8* dst, const u8* src, bool full) {
            if (full) memcpy(dst, src, 12);
            else      memcpy(dst, src, 8);
        }

        // Packs band `by` of the picture into out. Returns the bytes written.
        u32 packBand(u8* out, const u8* pic, u32 by, bool full) {
            const u32 bpp = full ? 3 : 2;
            u8* const mask = out;
            u8* q = out + kMaskBytes;
            memset(mask, 0, kMaskBytes);

            for (u32 bx = 0; bx < kBW; ++bx) {
                const u8* const src = pic + (size_t(by) * 4 * kW + bx * 4) * bpp;
                u32 pal[4] = {}, n = 0, bits = 0;
                bool fits = true;

                for (u32 i = 0; i < 16; ++i) {
                    const u8* const px = src + (size_t(i >> 2) * kW + (i & 3)) * bpp;
                    const u32 c = px[0] | px[1] << 8 | (full ? px[2] << 16 : 0);
                    u32 k = 0;
                    while (k < n && pal[k] != c) ++k;
                    if (k == n) {
                        if (n == 4) { fits = false; break; }
                        pal[n++] = c;
                    }
                    bits |= k << (2 * i);
                }

                if (fits) {
                    mask[bx >> 3] |= u8(1u << (bx & 7));
                    for (u32 k = 0; k < 4; ++k, q += bpp) memcpy(q, &pal[k], 4);   // (spare bytes are overwritten next)
                    memcpy(q, &bits, 4);
                    q += 4;
                } else {
                    for (u32 y = 0; y < 4; ++y, q += 4 * bpp) copyRow(q, src + size_t(y) * kW * bpp, full);
                }
            }

            return static_cast<u32>(q - out);
        }

        // The size of the band at p (its mask is read)
        u32 bandBytes(const u8* p, bool full) {
            u32 packed = 0;
            for (u32 i = 0; i < kMaskBytes; ++i) packed += __builtin_popcount(p[i]);
            return kMaskBytes + packed * (full ? 16 : 12) + (kBW - packed) * (full ? 48 : 32);
        }

        // Writes band `by` from p (bandBytes long) into the picture
        void unpackBand(u8* pic, u32 by, const u8* p, bool full) {
            const u32 bpp = full ? 3 : 2;
            const u8* const mask = p;
            p += kMaskBytes;

            for (u32 bx = 0; bx < kBW; ++bx) {
                const u32 b = by * kBW + bx;
                if (mask[bx >> 3] & (1u << (bx & 7))) {
                    // (the 16-byte load takes the indices too; only the 12 palette bytes are used)
                    if (full) { storeFull(pic, b, vld1q_u8(p), indices(p + 12)); p += 16; }
                    else      { store4444(pic, b, vld1_u8(p), indices(p + 8));   p += 12; }
                } else {
                    u8* const dst = pic + (size_t(by) * 4 * kW + bx * 4) * bpp;
                    for (u32 y = 0; y < 4; ++y, p += 4 * bpp) copyRow(dst + size_t(y) * kW * bpp, p, full);
                }
            }
        }

        // Writes the picture to f through buf (cap bytes), band by band
        bool writePicture(FILE* f, const u8* pic, u8* buf, u32 cap, bool full) {
            u32 n = 0;
            for (u32 by = 0; by < kBH; ++by) {
                if (cap - n < kBandMax) {
                    if (fwrite(buf, 1, n, f) != n) return false;
                    n = 0;
                }
                n += packBand(buf + n, pic, by, full);
            }
            return fwrite(buf, 1, n, f) == n;
        }

        // Reads the picture from f through buf (cap bytes), band by band
        bool readPicture(FILE* f, u8* pic, u8* buf, u32 cap, bool full) {
            u32 have = 0, pos = 0;
            for (u32 by = 0; by < kBH; ++by) {
                if (have - pos < kBandMax) {
                    memmove(buf, buf + pos, have - pos);
                    have -= pos;
                    pos = 0;
                    have += static_cast<u32>(fread(buf + have, 1, cap - have, f));
                }
                if (have - pos < kMaskBytes) return false;

                const u32 len = bandBytes(buf + pos, full);
                if (have - pos < len) return false;
                unpackBand(pic, by, buf + pos, full);
                pos += len;
            }
            return true;
        }


        // ── Ring access (render thread) ────────────────────────────────────────

        // The next record in the ring, or nullptr if none is buffered yet
        const u8* peek(u32& len) {
            for (;;) {
                const u64 r = s_rPos.load(std::memory_order_relaxed);
                if (r == s_wPos.load(std::memory_order_acquire)) return nullptr;

                const u32 off = static_cast<u32>(r % s_ringSize), tail = s_ringSize - off;
                u32 l = kWrap;
                if (tail >= 4) memcpy(&l, s_ring + off, 4);
                if (l == kWrap) {
                    s_rPos.store(r + tail, std::memory_order_release);
                    // The reader may be waiting for exactly this room (a large record after the
                    // wrap, with the ring's end left unused): wake it, as pop() does.
                    ueventSignal(&s_space);
                    continue;
                }

                len = l;
                return s_ring + off + 4;
            }
        }

        void pop(u32 len) {
            s_rPos.store(s_rPos.load(std::memory_order_relaxed) + 4 + len, std::memory_order_release);
            ueventSignal(&s_space);
        }

        // Stops the reader thread. It runs at the lowest priority, so it gets ours first:
        // on a busy core it could otherwise wait a long time to reach its exit.
        void quitReader() {
            if (!s_threadUp) return;
            s_quit.store(true, std::memory_order_release);
            ueventSignal(&s_space);
            s32 prio = 0x2C;
            svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
            svcSetThreadPriority(s_thread.handle, prio);
            threadWaitForExit(&s_thread);
            threadClose(&s_thread);
            s_threadUp = false;
        }

        void closeAll() {
            quitReader();

            if (s_file) {
                fclose(s_file);
                s_file = nullptr;
            }

            glass::sharedFree(s_ring);
            s_ring = nullptr;
            s_ringSize = 0;
        }
    }


    // ── Resume state ──────────────────────────────────────────────────────────────
    // config/ultrahand/cache/animated.cache, written when an overlay exits: the picture on
    // screen (the wallpaper buffer), the adjustments baked into it and the playback position.
    // Little-endian: "UAWS", u32 version 3, u64 .uaw size, u32 frames, u32 next record,
    // u64 its file offset, u16 path length, s8 x WA_COUNT adjustments, u32 picture bytes,
    // path, picture (packed in bands, see packBand).

    // The folder for the state: Ultrahand's cache folder, or for an overlay's own .uaw
    // (WALLPAPER_ANIM_OVERRIDE, e.g. wallpaper.uaw in its UI override folder), a cache/
    // folder next to it. Worked out on each use: an overlay may change the override.
    static std::string stateDir() {
        if (WALLPAPER_ANIM_OVERRIDE.empty()) return CACHE_PATH;
        const size_t slash = WALLPAPER_ANIM_OVERRIDE.rfind('/');
        return WALLPAPER_ANIM_OVERRIDE.substr(0, slash == std::string::npos ? 0 : slash + 1) + "cache/";
    }

    static std::string statePath() {
        return stateDir() + "animated.cache";
    }

    void saveState() {
        if (!s_active.load(std::memory_order_acquire) || s_path.empty() || !s_ring) return;

        // The overlay is closing: no more reading ahead, so the write has the card to itself
        quitReader();

    #if USING_LIVE_LOGGING_DIRECTIVE
        const u64 t0 = ult::glassLog ? armGetSystemTick() : 0;
    #endif

        // An existing state file is overwritten in place, so its clusters are reused and the
        // card's allocation table isn't rewritten on every close (truncating and growing a
        // ~1 MB file again is the slow, stall-prone part). A shorter state leaves old bytes
        // past its end, which loadState() never reads. The magic is zeroed first and written
        // last, so an interrupted write is never taken for a state.
        FILE* f = fopen(statePath().c_str(), "r+b");
    #if USING_LIVE_LOGGING_DIRECTIVE
        const bool inPlace = f != nullptr;
    #endif
        if (!f) f = fopen(statePath().c_str(), "wb");

        // Only on the first save: the cache folder doesn't exist yet
        if (!f) {
            mkdir(stateDir().c_str(), 0777);
            f = fopen(statePath().c_str(), "wb");
        }
        if (!f) return;

        setvbuf(f, nullptr, _IONBF, 0);

        const u16 pathLen = static_cast<u16>(s_path.size() < 1024 ? s_path.size() : 0);
        if (!pathLen) {
            fclose(f);
            std::remove(statePath().c_str());
            return;
        }

        // Header, picture size and path
        u8 hdr[34 + WA_COUNT + 4 + 1024];
        const u32 ver = 3, bytes = static_cast<u32>(wallpaperData.size());
        memset(hdr, 0, 4);
        memcpy(hdr + 4, &ver, 4);
        memcpy(hdr + 8, &s_fileSize, 8);
        memcpy(hdr + 16, &s_frames, 4);
        memcpy(hdr + 20, &s_playIdx, 4);
        memcpy(hdr + 24, &s_playOff, 8);
        memcpy(hdr + 32, &pathLen, 2);
        memcpy(hdr + 34, wallpaperBaked, WA_COUNT);
        memcpy(hdr + 34 + WA_COUNT, &bytes, 4);
        memcpy(hdr + 38 + WA_COUNT, s_path.data(), pathLen);
        const size_t hdrLen = 38 + WA_COUNT + pathLen;

        // The picture is packed through the ring (the reader is done with it)
        const bool full = bytes == size_t(kW) * kH * 3;
        bool ok = fwrite(hdr, 1, hdrLen, f) == hdrLen &&            // (magic still zero)
                  writePicture(f, wallpaperData.data(), s_ring, s_ringSize, full) &&
                  fseek(f, 0, SEEK_SET) == 0 && fwrite("UAWS", 1, 4, f) == 4;
        ok = (fclose(f) == 0) && ok;

        // Never leave a partial state behind
        if (!ok) std::remove(statePath().c_str());

    #if USING_LIVE_LOGGING_DIRECTIVE
        if (ult::glassLog) glass::live::log("anim: state saved at record %u in %lu us (%s)", s_playIdx,
                                            (unsigned long)(armTicksToNs(armGetSystemTick() - t0) / 1000),
                                            inPlace ? "in place" : "new file");
    #endif
    }

    void clearState() {
        std::remove(statePath().c_str());
    }

    namespace {
        // Restores the picture and position saved by saveState() if they belong to this .uaw
        // (same path and size) and this colour format, and the picture has the adjustments
        // `want` baked in (or none, then they are applied here). Sets the reader's start.
        bool loadState(bool full, const s8 (&want)[WA_COUNT]) {
            FILE* f = fopen(statePath().c_str(), "rb");
            if (!f) return false;

            setvbuf(f, nullptr, _IONBF, 0);

            u8 hdr[34 + WA_COUNT];
            u32 bytes = 0;
            bool ok = fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr) && fread(&bytes, 4, 1, f) == 1 && memcmp(hdr, "UAWS", 4) == 0;

            u32 ver = 0, frames = 0, idx = 0;
            u64 size = 0, off = 0;
            u16 pathLen = 0;
            s8 baked[WA_COUNT] = {};
            bool plain = true;

            if (ok) {
                memcpy(&ver, hdr + 4, 4);
                memcpy(&size, hdr + 8, 8);
                memcpy(&frames, hdr + 16, 4);
                memcpy(&idx, hdr + 20, 4);
                memcpy(&off, hdr + 24, 8);
                memcpy(&pathLen, hdr + 32, 2);
                memcpy(baked, hdr + 34, WA_COUNT);
                for (u32 i = 0; i < WA_COUNT; ++i) plain &= baked[i] == 0;

                ok = ver == 3 && (plain || memcmp(baked, want, WA_COUNT) == 0) && size == s_fileSize && frames == s_frames && idx < frames &&
                     (idx == 0 ? off == s_first : off > s_first) && off + 4 <= size && pathLen == s_path.size() &&
                     bytes == size_t(kW) * kH * (full ? 3 : 2);
            }

            if (ok) {
                std::string p(pathLen, '\0');
                // (through the ring: the reader isn't running yet)
                ok = fread(p.data(), 1, pathLen, f) == pathLen && p == s_path &&
                     readPicture(f, wallpaperData.data(), s_ring, s_ringSize, full);
            }

            fclose(f);
            if (!ok) return false;

            // Saved plain (the sliders were in use): apply the adjustments now
            if (plain) {
                WallAdj a;
                wallAdjSetup(a, want);
                if (full) wallAdjBuffer(a, wallpaperData.data(), size_t(kW) * kH);
                else      wallAdjBuffer4444(a, wallpaperData.data(), size_t(kW) * kH);
            }

            // The reader starts at record idx
            u32 len = 0;
            if (fseek(s_file, static_cast<long>(off), SEEK_SET) != 0 || fread(&len, 4, 1, s_file) != 1) return false;

            s_readIdx = idx;
            s_readLen = len;
            s_playIdx = idx;
            s_playOff = off;
            return true;
        }
    }


    // ── Playback ──────────────────────────────────────────────────────────────────

    void stop() {
        if (!s_active.exchange(false, std::memory_order_acq_rel) && !s_file && !s_ring) return;
        closeAll();
    }

    void stopForPowerOff() {
        // frame() stops decoding. The ring stays allocated: the render thread may be reading it.
        s_active.store(false, std::memory_order_release);
        quitReader();

        if (s_file) {
            fclose(s_file);
            s_file = nullptr;
        }
    }

    bool startFromPath(const std::string& path, bool resume) {
        stop();
        if (path.empty()) return false;

        s_file = fopen(path.c_str(), "rb");
        if (!s_file) return false;

        setvbuf(s_file, nullptr, _IONBF, 0);                       // whole records per read
        s_path = path;

        if (fseek(s_file, 0, SEEK_END) != 0) {
            closeAll();
            return false;
        }
        s_fileSize = static_cast<u64>(ftell(s_file));
        rewind(s_file);

        // File header
        u8 hdr[32];
        u16 w, h, num, den;
        if (fread(hdr, 1, 32, s_file) != 32 || memcmp(hdr, "UAW1", 4) != 0) {
            closeAll();
            return false;
        }
        memcpy(&w, hdr + 4, 2);
        memcpy(&h, hdr + 6, 2);
        memcpy(&num, hdr + 8, 2);
        memcpy(&den, hdr + 10, 2);
        memcpy(&s_frames, hdr + 12, 4);
        memcpy(&s_maxRec, hdr + 16, 4);
        memcpy(&s_first, hdr + 20, 4);
        if (w != kW || h != kH || !num || !den || !s_frames || s_maxRec < kKeyBytes || s_maxRec > (1u << 20)) {
            closeAll();
            return false;
        }
        s_periodNs = 1'000'000'000ULL * den / num;

        // Ring: about 0.5 s of frames at the clip's average size, so an SD read that stalls
        // for a moment (20-100 ms is normal) doesn't hold up playback. At least 256 KB and the
        // largest record (+ length words); at most 1 MB above 8 MB of heap, 768 KB at 8 MB and
        // 512 KB at 6 MB.
        {
            const u64 avgRec = (s_fileSize - s_first) / s_frames;
            const u64 halfSec = avgRec * num / den / 2;
            u64 cap = furtherExpandedMemory ? (1024u << 10) : expandedMemory ? (768u << 10) : (512u << 10);
            if (s_ringCapLimit && s_ringCapLimit < cap) cap = s_ringCapLimit;   // an overlay short of room

            u64 want = halfSec < cap ? halfSec : cap;
            if (want < 256u * 1024u) want = 256u * 1024u;
            if (want < u64(s_maxRec) + 8) want = u64(s_maxRec) + 8;
            s_ringSize = static_cast<u32>((want + 0xFFFFu) & ~u64(0xFFFFu));
        }

        s_ring = static_cast<u8*>(glass::sharedAlloc(glass::PoolUser::Wallpaper, s_ringSize));   // the block shared with the glass
        if (!s_ring) {
            closeAll();
            return false;
        }
        s_wPos.store(0);
        s_rPos.store(0);

        // Resume where the last overlay left off if its state matches. Otherwise decode the
        // first frame (a KEY) right now, so the wallpaper is complete from the start, and the
        // reader carries on from record 1 (read here together with this one's payload).
        const bool full = fullColorWallpaper;
        if (!wallpaperData.resize(size_t(kW) * kH * (full ? 3 : 2))) {   // no memory: no wallpaper
            closeAll();
            return false;
        }

        // The adjustments are baked into every decoded block (image_fx.hpp)
        s8 want[WA_COUNT];
        wallpaperAdjustTarget(want);
        setWallpaperBaked(want);
        const WallAdj* const adj = wallpaperBakedAdj.on ? &wallpaperBakedAdj : nullptr;

        const bool resumed = resume && s_frames > 1 && loadState(full, want);
        if (!resumed) {
            u32 len = 0;
            const u32 extra = s_frames > 1 ? 4 : 0;                   // + record 1's length
            if (fseek(s_file, s_first, SEEK_SET) != 0 || fread(&len, 4, 1, s_file) != 1 ||
                len != kKeyBytes || fread(s_ring, 1, len + extra, s_file) != len + extra ||
                !(full ? decodeRecord<true>(wallpaperData.data(), s_ring, len, adj)
                       : decodeRecord<false>(wallpaperData.data(), s_ring, len, adj))) {
                closeAll();
                wallpaperData.release();
                setWallpaperBaked(kNoWallAdjust);
                return false;
            }

            s_readIdx = 1;
            if (s_frames > 1) memcpy(&s_readLen, s_ring + len, 4);
            s_playIdx = 1;
            s_playOff = s_first + 4 + len;
        }
        wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel);

        // A single frame is a still: no reader, nothing to play
        if (s_frames == 1) {
            closeAll();
            return true;
        }

        if (!s_eventInit) {
            ueventCreate(&s_space, true);
            s_eventInit = true;
        }
        s_quit.store(false, std::memory_order_release);

        // Lowest priority, preferring core 2 like the glass worker: it mostly waits on the
        // SD card, and the ring holds a few hundred ms of frames.
        bool created = R_SUCCEEDED(threadCreate(&s_thread, readerMain, nullptr, nullptr, 0x4000, 0x3F, 2));
        if (created) svcSetThreadCoreMask(s_thread.handle, 2, 0xF);
        else created = R_SUCCEEDED(threadCreate(&s_thread, readerMain, nullptr, nullptr, 0x4000, 0x3F, -2));

        if (!created || R_FAILED(threadStart(&s_thread))) {
            if (created) threadClose(&s_thread);
            closeAll();
            return false;
        }

        s_threadUp = true;
        s_nextDueNs = 0;                                           // set by the first frame()
        s_active.store(true, std::memory_order_release);

    #if USING_LIVE_LOGGING_DIRECTIVE
        if (ult::glassLog) glass::live::log("anim: playing %s (%u frames, %u/%u fps, ring %u KB), %s record %u",
                                            path.c_str(), s_frames, num, den, s_ringSize / 1024,
                                            resumed ? "resumed at" : "from", resumed ? s_playIdx : 0u);
    #endif
        return true;
    }

    void frame() {
        if (!s_active.load(std::memory_order_acquire) || refreshWallpaper.load(std::memory_order_acquire)) return;

        // A reload owns the wallpaper: skip this frame
        std::unique_lock<std::mutex> lock(wallpaperMutex, std::try_to_lock);
        if (!lock.owns_lock()) return;

        const bool full = fullColorWallpaper;
        if (wallpaperData.size() != size_t(kW) * kH * (full ? 3 : 2)) return;

        // The adjustments baked into the buffer go into every decoded block (none while the
        // sliders are in use: the renderer applies them then)
        const WallAdj* const adj = wallpaperBakedAdj.on ? &wallpaperBakedAdj : nullptr;

        const u64 now = nowNs();

        // The picture set by startFromPath (frame 0, or the resumed one) stays up for one period
        // from the first call. Back after a pause (overlay hidden, a stall): carry on from now
        // instead of racing through the missed frames.
        if (s_nextDueNs == 0) s_nextDueNs = now + s_periodNs;
        else if (now > s_nextDueNs + 250'000'000ULL) s_nextDueNs = now;

        // Half a display frame of slack: a frame due just after this refresh is shown now, so
        // 30 fps lands on every second 60 Hz refresh instead of drifting.
        constexpr u64 kSlackNs = 8'000'000ULL;

    #if USING_LIVE_LOGGING_DIRECTIVE
        const u64 t0 = ult::glassLog ? armGetSystemTick() : 0;
    #endif
        bool changed = false;
        u32 decoded = 0;

        while (now + kSlackNs >= s_nextDueNs && decoded < 3) {
            const s64 err = static_cast<s64>(now) - static_cast<s64>(s_nextDueNs);   // + late, - early

            u32 len;
            const u8* const rec = peek(len);

            // The reader is behind: hold this frame
            if (!rec) {
            #if USING_LIVE_LOGGING_DIRECTIVE
                ++s_statUnder;
            #endif
                break;
            }

            if (rec[0] != 0) {
                const bool ok = full ? decodeRecord<true>(wallpaperData.data(), rec, len, adj)
                                     : decodeRecord<false>(wallpaperData.data(), rec, len, adj);

                // Malformed: keep the last frame
                if (!ok) {
                #if USING_LIVE_LOGGING_DIRECTIVE
                    if (ult::glassLog) glass::live::log("anim: bad record, playback stopped");
                #endif
                    s_active.store(false, std::memory_order_release);
                    closeAll();
                    return;
                }
                changed = true;
            }

            pop(len);

            // The position for a resume
            s_playOff += 4 + len;
            if (++s_playIdx == s_frames) {
                s_playIdx = 0;
                s_playOff = s_first;
            }

            // Late by more than half a period (the reader was behind, a slow frame): carry on
            // from here without catching up, which would skip pictures right after a freeze.
            // Otherwise follow the render loop, feeding back 1/8 of the error so the due times
            // stay centred on the loop's frame times. (The display's 60 Hz and the system clock
            // drift apart; without this a 30 fps clip slowly slides to the edge of the slack
            // and then shows frames 1 and 3 refreshes apart.)
            if (err > static_cast<s64>(s_periodNs / 2)) s_nextDueNs = now + s_periodNs;
            else s_nextDueNs += s_periodNs + err / 8;

            ++decoded;
        #if USING_LIVE_LOGGING_DIRECTIVE
            ++s_statFrames;
        #endif
        }

        // Full colour: compose again
        if (changed) wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel);

    #if USING_LIVE_LOGGING_DIRECTIVE
        // One line every ~5 s
        if (ult::glassLog) {
            s_statDecNs += armTicksToNs(armGetSystemTick() - t0);
            if (!s_statT0) s_statT0 = now;
            else if (now - s_statT0 >= 5'000'000'000ULL) {
                glass::live::log("anim: %u frames in %lu ms, decode %lu us per frame, %u underruns",
                                 s_statFrames, (unsigned long)((now - s_statT0) / 1'000'000ULL),
                                 (unsigned long)(s_statDecNs / 1000 / (s_statFrames ? s_statFrames : 1)), s_statUnder);
                s_statFrames = s_statUnder = 0;
                s_statDecNs = 0;
                s_statT0 = now;
            }
        }
    #endif
    }

} // namespace ult::anim
