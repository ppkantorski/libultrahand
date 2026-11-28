/********************************************************************************
 * File: image_fx.hpp
 * Author: ppkantorski
 * Description: 
 *   'image_fx.hpp' holds the wallpaper Light and Color adjustments
 *   (wallpaper.ini, -100..100 each) for the Ultrahand Overlay project: the per-pixel
 *   math, shared by the renderer (live preview), the still-wallpaper load and the
 *   animated-wallpaper decoder, and the rules for where it is applied.
 *
 *   Where the adjustments are applied (full colour only):
 *   - Normally they are baked into the wallpaper buffer: once when a still wallpaper is
 *     loaded, and for an animated one on each block's 4-colour palette as it is decoded
 *     (every pixel of a block is one of those 4 colours and the adjustments are per
 *     pixel, so this gives exactly the same picture for 1/4 of the work, and only for
 *     the blocks that change). Drawing a frame then costs nothing extra.
 *   - While the adjustment sliders are in use (beginWallpaperAdjustEdit) the buffer holds
 *     the plain wallpaper and the renderer applies the current values on every frame, so
 *     the preview follows the sliders. Leaving the page (endWallpaperAdjustEdit) bakes
 *     them back in once.
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

#pragma once

#include <switch.h>
#include <arm_neon.h>
#include <math.h>
#include <atomic>
#include <cstddef>
#include "tsl_utils.hpp"

namespace ult {

    // ── The adjustments ───────────────────────────────────────────────────────────
    // The Light and Color steps of the Ultrahand Wallpaper designer, in its order, on 8-bit
    // RGB:
    //   - exposure, brightness, black point, contrast, warmth and tint fold into one
    //     c' = a*c + o_channel
    //   - highlights / shadows add d(luma) to all channels
    //   - saturation / vibrance scale each channel's distance from the HSL lightness (same
    //     result as the designer's HSL round trip; grey stays grey)
    // Every stage is skipped when its sliders are 0. With all of them 0, `on` is false and
    // nothing runs.
    struct WallAdj {
        float32x4_t a, oR, oG, oB, o, q2, q1, q0, m, vib;
        int16x8_t fa, foR, foG, foB, kSat;
        bool on, fast, satFix, hs, sat, div, vibOn;
    };

    inline void wallAdjSetup(WallAdj& wa, const s8 (&v)[WA_COUNT]) {
        wa = WallAdj{};
        for (u32 i = 0; i < WA_COUNT; ++i) wa.on |= v[i] != 0;
        if (!wa.on) return;

        const float E  = exp2f(1.5f * v[WA_EXPOSURE] / 100.0f);
        const float C  = 1.0f + (v[WA_CONTRAST] < -99 ? -99 : v[WA_CONTRAST]) / 100.0f;   // -100 (an older ini): as -99
        const float o  = (0.6f * v[WA_BRIGHTNESS] - 0.4f * v[WA_BLACK_POINT] - 128.0f) * C + 128.0f;
        const float W  = 0.4f * v[WA_WARMTH], T = 0.4f * v[WA_TINT];
        const float hi = 0.8f * v[WA_HIGHLIGHTS], sh = 0.8f * v[WA_SHADOWS];
        const float m  = 1.0f + v[WA_SATURATION] / 100.0f, vib = v[WA_VIBRANCE] / 100.0f;

        wa.a  = vdupq_n_f32(E * C);
        wa.o  = vdupq_n_f32(o);
        wa.oR = vdupq_n_f32(o + W);
        wa.oG = vdupq_n_f32(o + T);
        wa.oB = vdupq_n_f32(o - W);

        // d = hi*l^2 + sh*(1-l)^2 with l = luma/255, as a polynomial in luma
        wa.q2 = vdupq_n_f32((hi + sh) / 65025.0f);
        wa.q1 = vdupq_n_f32(-2.0f * sh / 255.0f);
        wa.q0 = vdupq_n_f32(sh);

        wa.m = vdupq_n_f32(m);
        wa.vib = vdupq_n_f32(vib);
        wa.hs = hi != 0.0f || sh != 0.0f;
        wa.sat = m != 1.0f || vib != 0.0f;
        wa.vibOn = vib != 0.0f;
        wa.div = wa.vibOn || m > 1.0f;   // m <= 1 alone: the factor is just m

        // Only the folded a*c + o stage in use: 16-bit fixed point instead of floats
        // (a < 8 as Q15 of a/8, offsets in 1/16 steps)
        wa.fast = !wa.hs && !wa.sat;

        // Lowering saturation (m < 1, no vibrance, no highlights / shadows): fixed point too
        wa.satFix = wa.sat && !wa.div && !wa.hs;

        wa.kSat = vdupq_n_s16(static_cast<s16>(lrintf(m * 32767.0f)));   // m as Q15
        wa.fa  = vdupq_n_s16(static_cast<s16>(lrintf(E * C * 4096.0f)));
        wa.foR = vdupq_n_s16(static_cast<s16>(lrintf((o + W) * 16.0f)));
        wa.foG = vdupq_n_s16(static_cast<s16>(lrintf((o + T) * 16.0f)));
        wa.foB = vdupq_n_s16(static_cast<s16>(lrintf((o - W) * 16.0f)));
    }

    // 4 px as floats on the 0..255 scale (all stages)
    __attribute__((always_inline)) inline void wallAdj4f(const WallAdj& wa, float32x4_t& R, float32x4_t& G, float32x4_t& B) {
        const float32x4_t r0 = R, g0 = G, b0 = B;
        R = vfmaq_f32(wa.oR, r0, wa.a);
        G = vfmaq_f32(wa.oG, g0, wa.a);
        B = vfmaq_f32(wa.oB, b0, wa.a);

        // Highlights / shadows
        if (wa.hs) {
            const float32x4_t y = vfmaq_n_f32(vfmaq_n_f32(vmulq_n_f32(r0, 0.299f), g0, 0.587f), b0, 0.114f);
            const float32x4_t lc = vfmaq_f32(wa.o, y, wa.a);          // luma after contrast
            const float32x4_t d = vfmaq_f32(wa.q0, vfmaq_f32(wa.q1, lc, wa.q2), lc);
            R = vaddq_f32(R, d);
            G = vaddq_f32(G, d);
            B = vaddq_f32(B, d);
        }

        // Saturation / vibrance
        if (wa.sat) {
            const float32x4_t z = vdupq_n_f32(0.0f), f255 = vdupq_n_f32(255.0f), one = vdupq_n_f32(1.0f);
            R = vminq_f32(vmaxq_f32(R, z), f255);
            G = vminq_f32(vmaxq_f32(G, z), f255);
            B = vminq_f32(vmaxq_f32(B, z), f255);

            const float32x4_t mx = vmaxq_f32(vmaxq_f32(R, G), B), mn = vminq_f32(vminq_f32(R, G), B);
            const float32x4_t sum = vaddq_f32(mx, mn), diff = vsubq_f32(mx, mn), L = vmulq_n_f32(sum, 0.5f);
            float32x4_t k = wa.m;

            if (wa.div) {
                auto rcp = [](float32x4_t x) {
                    const float32x4_t e = vrecpeq_f32(x);       // one Newton step: ~16 bits,
                    return vmulq_f32(vrecpsq_f32(x, e), e);     // plenty for 8-bit output
                };

                const float32x4_t den = vsubq_f32(f255, vabsq_f32(vsubq_f32(sum, f255)));   // HSL: s = diff / den
                const float32x4_t rDiff = rcp(diff);

                if (wa.vibOn) {
                    float32x4_t ns = vmulq_f32(vmulq_f32(diff, rcp(den)), wa.m);
                    ns = vfmaq_f32(ns, wa.vib, vsubq_f32(one, ns));
                    ns = vminq_f32(vmaxq_f32(ns, z), one);
                    k = vmulq_f32(vmulq_f32(ns, den), rDiff);                // new s / s
                } else {
                    k = vminq_f32(wa.m, vmulq_f32(den, rDiff));             // s capped at 1
                }

                k = vbslq_f32(vcgtq_f32(diff, vdupq_n_f32(1e-4f)), k, z);   // grey: unchanged
            }

            R = vfmaq_f32(L, vsubq_f32(R, L), k);
            G = vfmaq_f32(L, vsubq_f32(G, L), k);
            B = vfmaq_f32(L, vsubq_f32(B, L), k);
        }
    }

    // The a*c + o stage alone: c*128 * (a/8) = c*a*16, + o*16, then /16 rounded to 0..255
    __attribute__((always_inline)) inline uint8x8_t wallAdjFx(const WallAdj& wa, uint8x8_t c, int16x8_t o) {
        return vqrshrun_n_s16(vqaddq_s16(vqrdmulhq_s16(vreinterpretq_s16_u16(vshll_n_u8(c, 7)), wa.fa), o), 4);
    }

    // The a*c + o stage, then saturation lowered by m (wa.satFix), in 16-bit fixed point:
    // 8 px per instruction instead of 4 and no float conversions (~2.7x faster than the
    // float stages).
    //   - stage 1 in 1/16 steps, clamped to 0..255 (the float path clamps before the
    //     saturation stage too)
    //   - L = (max + min) / 2, kept exact as 2L
    //   - c' = L + (c - L) m in 1/32 steps, rounded half to even like the float path
    // Same picture as the float path for pure saturation at m = k/4; otherwise at most one
    // step (of 255) on ~2% of channel values (exhaustive test over all 16.7M colours).
    namespace detail {   // functions, not lambdas: -Os builds would not inline lambdas here
        __attribute__((always_inline)) inline int16x8_t satFixStage1(const WallAdj& wa, uint8x8_t c, int16x8_t o) {
            const int16x8_t v = vqaddq_s16(vqrdmulhq_s16(vreinterpretq_s16_u16(vshll_n_u8(c, 7)), wa.fa), o);
            return vminq_s16(vmaxq_s16(v, vdupq_n_s16(0)), vdupq_n_s16(255 * 16));
        }

        // 2c' = 2L + (2c - 2L) m, then /32 rounded half to even, 0..255
        __attribute__((always_inline)) inline uint8x8_t satFixOut(const WallAdj& wa, int16x8_t c, int16x8_t L2) {
            const int16x8_t x = vaddq_s16(L2, vqrdmulhq_s16(vsubq_s16(vshlq_n_s16(c, 1), L2), wa.kSat));
            const int16x8_t odd = vandq_s16(vshrq_n_s16(x, 5), vdupq_n_s16(1));
            return vqshrun_n_s16(vqaddq_s16(x, vaddq_s16(vdupq_n_s16(15), odd)), 5);
        }
    }

    __attribute__((always_inline)) inline void wallAdjSatFix8(const WallAdj& wa, uint8x8x3_t& w) {
        const int16x8_t R = detail::satFixStage1(wa, w.val[0], wa.foR);
        const int16x8_t G = detail::satFixStage1(wa, w.val[1], wa.foG);
        const int16x8_t B = detail::satFixStage1(wa, w.val[2], wa.foB);
        const int16x8_t L2 = vaddq_s16(vmaxq_s16(vmaxq_s16(R, G), B), vminq_s16(vminq_s16(R, G), B));   // 2L

        w.val[0] = detail::satFixOut(wa, R, L2);
        w.val[1] = detail::satFixOut(wa, G, L2);
        w.val[2] = detail::satFixOut(wa, B, L2);
    }

    // 8 px of 8-bit RGB in place (wa.on must be true)
    __attribute__((always_inline)) inline void wallAdj8(const WallAdj& wa, uint8x8x3_t& w) {
        if (wa.fast) {
            w.val[0] = wallAdjFx(wa, w.val[0], wa.foR);
            w.val[1] = wallAdjFx(wa, w.val[1], wa.foG);
            w.val[2] = wallAdjFx(wa, w.val[2], wa.foB);
            return;
        }

        if (wa.satFix) {
            wallAdjSatFix8(wa, w);
            return;
        }

        float32x4_t c[2][3];
        for (u32 ch = 0; ch < 3; ++ch) {
            const uint16x8_t h = vmovl_u8(w.val[ch]);
            c[0][ch] = vcvtq_f32_u32(vmovl_u16(vget_low_u16(h)));
            c[1][ch] = vcvtq_f32_u32(vmovl_u16(vget_high_u16(h)));
        }

        wallAdj4f(wa, c[0][0], c[0][1], c[0][2]);
        wallAdj4f(wa, c[1][0], c[1][1], c[1][2]);

        // Round and clamp to 0..255
        for (u32 ch = 0; ch < 3; ++ch)
            w.val[ch] = vqmovn_u16(vcombine_u16(vqmovn_u32(vcvtnq_u32_f32(c[0][ch])), vqmovn_u32(vcvtnq_u32_f32(c[1][ch]))));
    }

    // The first 4 px (lanes 0..3) of 8-bit RGB in place, with the same result per pixel as
    // wallAdj8 (wa.on must be true). Lanes 4..7 are undefined afterwards. Used for block
    // palettes.
    __attribute__((always_inline)) inline void wallAdjLow4(const WallAdj& wa, uint8x8x3_t& w) {
        if (wa.fast || wa.satFix) {
            wallAdj8(wa, w);
            return;
        }

        float32x4_t c[3];
        for (u32 ch = 0; ch < 3; ++ch) c[ch] = vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(w.val[ch]))));

        wallAdj4f(wa, c[0], c[1], c[2]);

        for (u32 ch = 0; ch < 3; ++ch)
            w.val[ch] = vqmovn_u16(vcombine_u16(vqmovn_u32(vcvtnq_u32_f32(c[ch])), vdup_n_u16(0)));
    }

    /**
     * @brief Applies wa to px pixels of 8-bit RGB (3 B/px) in place. px must be a multiple of 8.
     */
    void wallAdjBuffer(const WallAdj& wa, u8* rgb, size_t px);

    // RGBA4444 (2 B/px: r|g, b|a nibbles): 8 px expanded to 8 bits (x * 17), adjusted and cut
    // back to 4 bits (>> 4, as a load cuts them). The live preview at 6 / 8 MB draws exactly
    // this, and leaving the page bakes it in place with wallAdjBuffer4444, so nothing jumps.
    __attribute__((always_inline)) inline void wallAdj4444(const WallAdj& wa, uint8x8_t& rg, uint8x8_t& ba) {
        const uint8x8_t m = vdup_n_u8(0xF0);
        const uint8x8_t r = vand_u8(rg, m), g = vshl_n_u8(rg, 4), b = vand_u8(ba, m);
        uint8x8x3_t c{{ vsra_n_u8(r, r, 4), vsra_n_u8(g, g, 4), vsra_n_u8(b, b, 4) }};

        wallAdj8(wa, c);

        rg = vorr_u8(vand_u8(c.val[0], m), vshr_n_u8(c.val[1], 4));
        ba = vorr_u8(vand_u8(c.val[2], m), vand_u8(ba, vdup_n_u8(0x0F)));
    }

    /**
     * @brief The same as wallAdjBuffer for px pixels of RGBA4444, in place.
     */
    void wallAdjBuffer4444(const WallAdj& wa, u8* px4444, size_t px);


    // ── Where they are applied (see the file header) ─────────────────────────────

    // True while the sliders are in use: the buffer is plain and the renderer adjusts it.
    extern std::atomic<bool> wallpaperAdjustLive;

    // The live preview may spread its frames over the render threads when no game was in
    // focus as it started (glass::gameOutOfFocus) and HOME hasn't been pressed since. Same
    // rule as the frosted glass composition: with a game in front the frame stays on one
    // thread.
    extern std::atomic<bool> wallpaperAdjustSpread;

    // The adjustments baked into wallpaperData, as values and set up (wallpaperMutex). The
    // animated wallpaper decoder applies wallpaperBakedAdj to what it decodes.
    extern s8 wallpaperBaked[WA_COUNT];
    inline constexpr s8 kNoWallAdjust[WA_COUNT] = {};
    extern WallAdj wallpaperBakedAdj;

    /**
     * @brief What a wallpaper loaded now gets baked with: the current values, or none while
     * the sliders are in live preview (full colour only).
     */
    void wallpaperAdjustTarget(s8 (&out)[WA_COUNT]);

    void setWallpaperBaked(const s8 (&v)[WA_COUNT]);

    /**
     * @brief Bakes wallpaperAdjustTarget() into the plain wallpaper just loaded into
     * wallpaperData, and records it. The caller holds wallpaperMutex.
     */
    void bakeLoadedWallpaper();

    /**
     * @brief Starts the live preview. Main thread, before the first slider change.
     *
     * Unless nothing is baked in, the plain wallpaper goes back into the buffer first (a
     * still one is read again, an animated one restarts at frame 0). No-op while already live.
     */
    void beginWallpaperAdjustEdit();

    /**
     * @brief Ends the live preview and bakes the current values in once. Main thread, when the
     * adjustments page goes away. The animation carries on from the same frame. No-op unless live.
     */
    void endWallpaperAdjustEdit();

    /**
     * @brief Brings the buffer to wallpaperAdjust[] after it changed outside the live preview.
     * Main thread.
     *
     * In place if nothing was baked in, otherwise by loading the wallpaper again.
     *
     * @param resume False: an animated wallpaper that has to be loaded again starts at frame 0
     *               (4-bit value changes).
     */
    void syncWallpaperAdjust(bool resume = true);

    /**
     * @brief The wallpaper.ini section holding a wallpaper's adjustments in the current colour
     * mode, by its file name (an image and a video may share a name).
     *
     * [<file> - full] in full colour (> 8 MB), [<file> - limited] in RGBA4444 (6 / 8 MB). They
     * are kept apart because 16 levels per channel need different values to look right.
     */
    std::string wallpaperAdjustSection(const std::string& name);

} // namespace ult
