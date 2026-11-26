"""
File: make_sounds.py
Author: ppkantorski
Description:
    This script generates Ultrahand's default sound effects (enter, exit, main_settings,
    move, notification, on, off, settings, tick, wall) as 16-bit mono 48 kHz WAV files.
    Every sound is synthesized from scratch -- sine tones and filtered noise that
    follow pitch and volume curves -- so no recorded audio is used. Each sound is
    scaled to a set perceived loudness. The output is the same on every run.

    Usage: python3 make_sounds.py [output folder]

    For the latest updates and contributions, visit the project's GitHub repository.
    (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)

    Note: Please be aware that this notice cannot be altered or removed. It is a part
    of the project's documentation and must remain intact.

Licensed under GPLv2
Copyright (c) 2026 ppkantorski
"""
import os
import sys
import wave

import numpy as np

SR = 48000                                  # sample rate (Hz)
rng = np.random.default_rng(2026)           # fixed seed: same files every run (add new
                                            # sounds at the END, or later ones' noise changes)


# ---- building blocks ---------------------------------------------------------

def ms(n):
    """Milliseconds -> samples."""
    return int(round(SR * n / 1000))

def curve(points, n, start):
    """Points [(ms, value), ...] -> per-sample values (linear between points)."""
    t = start + np.arange(n) * 1000 / SR
    return np.interp(t, [p[0] for p in points], [p[1] for p in points])

def tone(freq, amp, harm=(0,)):
    """Sine tone (+ harmonics, dB) whose pitch and volume follow point curves (ms, Hz/level).
    Returns (start ms, samples)."""
    t0, t1 = amp[0][0], amp[-1][0]
    n = ms(t1 - t0)
    ph = 2 * np.pi * np.cumsum(curve(freq, n, t0)) / SR
    y = sum(10 ** (db / 20) * np.sin((k + 1) * ph) for k, db in enumerate(harm))
    return t0, y * curve(amp, n, t0)

def biquad(x, b, a):
    """Second-order IIR filter (direct form I)."""
    y = np.zeros(len(x)); x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        y0 = (b[0] * v + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2) / a[0]
        y[i] = y0; x2, x1, y2, y1 = x1, v, y1, y0
    return y

def bandpass(x, f, q):
    """Band-pass around f Hz (RBJ, constant 0 dB peak)."""
    w = 2 * np.pi * f / SR; al = np.sin(w) / (2 * q)
    return biquad(x, (al, 0, -al), (1 + al, -2 * np.cos(w), 1 - al))

def noise(amp, bands, air=0.15):
    """Noise burst whose volume follows `amp`, coloured by band-passes [(Hz, Q, gain)]
    plus a little unfiltered 'air'. Returns (start ms, samples)."""
    t0, t1 = amp[0][0], amp[-1][0]
    n = ms(t1 - t0)
    x = rng.standard_normal(n + ms(4))
    y = sum(g * bandpass(x, f, q) for f, q, g in bands) + air * x
    y = y[ms(4):] / (np.abs(y[ms(4):]).max() + 1e-9)        # skip the filters' start-up
    return t0, y * curve(amp, n, t0)

def resonator(x, f, q_ms):
    """2-pole resonator ringing at f with decay time constant q_ms."""
    r = np.exp(-1 / (SR * q_ms / 1000)); w = 2 * np.pi * f / SR
    return biquad(x, (1, 0, 0), (1, -2 * r * np.cos(w), r * r))

def click(t0, length, body_hz, body_ms, level, bright=0.25):
    """Switch click at t0 ms: a 0.6 ms noise burst striking a small body."""
    x = np.zeros(ms(length)); k = ms(0.6)
    x[:k] = rng.standard_normal(k) * np.hanning(k)
    y = resonator(x, body_hz, body_ms); y /= np.abs(y).max() + 1e-9
    y += bright * x / (np.abs(x).max() + 1e-9)
    return t0, level * y / np.abs(y).max()

def crackle(t0, length, body_hz, dec_ms, level, density=0.5):
    """Noisy tail at t0 ms: sparse micro-clicks and noise ringing a small body, fading."""
    n = ms(length)
    x = rng.standard_normal(n) * (rng.random(n) < density)
    x *= np.exp(-np.arange(n) / (SR * dec_ms / 1000))
    y = resonator(x, body_hz, 0.6) + 0.25 * x
    y[-ms(3):] *= np.linspace(1, 0, ms(3))                # fade out: no click where it ends
    return t0, level * y / (np.abs(y).max() + 1e-9)

def blip(t0, f, att, hold, dec, level, harm=(0,), curve=2.0):
    """Steady note at t0 ms: raised-cosine attack, hold, then a curved decay."""
    e = np.concatenate([0.5 - 0.5 * np.cos(np.pi * np.arange(ms(att)) / max(1, ms(att))),
                        np.ones(ms(hold)), (1 - np.arange(ms(dec)) / max(1, ms(dec))) ** curve])
    ph = 2 * np.pi * f * np.arange(len(e)) / SR
    y = sum(10 ** (db / 20) * np.sin((k + 1) * ph) for k, db in enumerate(harm)) * e
    return t0, level * y / np.abs(y).max()

def mix(total, *parts):
    """Sum (start ms, samples) parts into a buffer of `total` ms."""
    out = np.zeros(ms(total))
    for t0, sig in parts:
        i = ms(t0); n = min(len(sig), len(out) - i)
        out[i:i+n] += sig[:n]
    return out



# ---- output --------------------------------------------------------------------

def lowpass(x, fc=12500.0, q=0.707):
    """Gentle 2-pole low-pass: nothing harsh above ~13 kHz."""
    w = 2 * np.pi * fc / SR; al = np.sin(w) / (2 * q); c = np.cos(w)
    return biquad(x, ((1 - c) / 2, 1 - c, (1 - c) / 2), (1 + al, -2 * c, 1 - al))

def loudness(x):
    """Perceived loudness (dB, ITU-R BS.1770 K-weighting) over the audible part
    (1 ms frames within 40 dB of the peak), so silence between events doesn't count."""
    K = np.tan(np.pi * 1681.974450955533 / SR); Q = 0.7071752369554196
    Vh = 10 ** (3.999843853973347 / 20); Vb = Vh ** 0.4996667741545416; a0 = 1 + K / Q + K * K
    y = biquad(x, ((Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0),
                  (1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0))
    K = np.tan(np.pi * 38.13547087602444 / SR); Q = 0.5003270373238773; a0 = 1 + K / Q + K * K
    y = biquad(y, (1, -2, 1), (1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0))
    h = ms(1); n = len(x) // h * h
    act = np.abs(x[:n].reshape(-1, h)).max(1) > np.abs(x).max() * 0.01
    return -0.691 + 10 * np.log10((y[:n].reshape(-1, h)[act] ** 2).mean())

def finish(y, loud):
    """Low-pass, scale to the perceived loudness `loud` (dB), end on silence."""
    y = lowpass(y)
    y = y * 10 ** ((loud - loudness(y)) / 20)
    assert np.abs(y).max() < 1.0, 'would clip'
    f = min(len(y), ms(1))
    y[-f:] *= np.linspace(1, 0, f) ** 2
    return np.round(y * 32767).astype('<i2')

def write(path, y):
    """Plain RIFF WAV (fmt + data chunks only), 16-bit mono."""
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(y.tobytes())



# ---- the sounds ------------------------------------------------------------------
# Curves are points: (ms, Hz) for pitch, (ms, level) for volume; harm = harmonic levels
# in dB (fundamental first). The last number of each sound is its loudness in dB.

first_blip = tone([(0, 2126), (1, 2160), (3, 2140), (6, 2105), (14, 2097)],
                  [(0, 0), (0.8, 0.41), (1.6, 0.28), (3, 0.33), (4.5, 0.31), (6, 0.25),
                   (8, 0.17), (10, 0.07), (12, 0.02), (13.5, 0)], harm=(0, -52, -25, -61, -40))
sounds = {
    # cursor: a rising swoop, and a shorter one as it lands
    'move': finish(mix(94,
        tone([(0, 340), (3, 369), (5, 462), (7, 562), (9, 673), (11, 814), (13, 972), (15, 1119), (17, 1195)],
             [(0, 0), (1, 0.05), (2, 0.14), (4, 0.21), (5, 0.27), (6, 0.33), (7, 0.36), (10, 0.38),
              (13, 0.42), (14.5, 0.43), (15, 0.38), (15.5, 0.25), (16, 0.12), (16.5, 0)],
             harm=(0, -22, -23, -32, -46)),
        tone([(84, 1000), (86, 1100), (88, 1230), (90, 1360), (92.5, 1460)],
             [(84, 0), (84.5, 0.40), (85, 0.49), (87, 0.48), (89, 0.44), (91, 0.39),
              (91.5, 0.29), (92, 0.15), (92.5, 0)], harm=(0, -40, -36, -26, -47))), -12.1),
    # into / out of a menu: a soft blip, then a higher (in) or lower (out) one
    'enter': finish(mix(55, first_blip,
        tone([(42, 2780), (43.5, 3100), (45, 3030), (46.5, 2955), (50, 2953)],
             [(42, 0), (42.5, 0.22), (43, 0.43), (44, 0.47), (44.5, 0.61), (45, 0.58), (45.5, 0.48),
              (46, 0.37), (46.5, 0.25), (47, 0.15), (47.5, 0.09), (48.5, 0.03), (50, 0)],
             harm=(0, -26, -44, -35, -51))), -12.2),
    'exit': finish(mix(51, first_blip,
        tone([(41.5, 1450), (42.5, 1300), (44, 1325), (50.5, 1320)],
             [(41.5, 0), (42, 0.14), (43, 0.23), (43.5, 0.35), (44, 0.37), (44.5, 0.31), (45.5, 0.21),
              (46.5, 0.14), (47.5, 0.10), (48.5, 0.06), (49.5, 0.03), (50.5, 0)],
             harm=(0, -41, -58, -64))), -15.3),
    # main_settings (Ultrahand's own settings): a soft breath blooming into a warm D major
    # chord (D5, F#5, A5 with rich harmonics; the top note lasts longest, so it lifts),
    # then a breath and a pure D6 swelling in and out an octave above
    'main_settings': finish(mix(118,
        noise([(0, 0), (3, 0.18), (8, 0.14), (14, 0)], [(3800, 1.5, 1.0), (2000, 2, 0.5)], air=0.1),
        blip(2.0, 587.3, 9, 2, 26, 0.60, harm=(0, -6, -10, -16, -20, -26), curve=1.4),
        blip(2.0, 588.4, 10, 2, 26, 0.25, harm=(0, -10), curve=1.4),
        blip(3.0, 740.0, 10, 3, 30, 0.45, harm=(0, -8, -14, -20), curve=1.4),
        blip(4.0, 880.0, 12, 6, 36, 0.50, harm=(0, -10, -16), curve=1.3),
        noise([(55, 0), (62, 0.06), (68, 0.05), (73, 0)], [(3000, 1.5, 1.0)], air=0.1),
        blip(64, 1174.7, 16, 2, 34, 0.62, harm=(0, -14, -14, -27), curve=1.3)), -14.5),
    # toggle on: a small body ringing at 2.4-2.9 kHz, struck three times, ringing out
    'on': finish(mix(34,
        tone([(2, 2400), (5, 2450), (8, 2900), (11, 2950), (14, 2800), (34, 2780)],
             [(2, 0), (2.4, 0.25), (3.2, 0.30), (4, 0.08), (5.5, 0.12), (6.2, 0.70), (7, 0.48),
              (7.8, 0.18), (8.6, 0.42), (9.3, 0.55), (10.5, 0.65), (11.3, 0.50), (12.3, 0.22),
              (13.5, 0.08), (15, 0.09), (16.5, 0.05), (18, 0.07), (20, 0.04), (26, 0.03), (34, 0)],
             harm=(0, -30, -40)),
        noise([(2, 0), (2.5, 0.25), (4, 0.12), (6, 0.35), (7.8, 0.15), (9.5, 0.30), (11.5, 0.30),
               (13, 0.10), (17, 0.08), (22, 0.05), (30, 0.03), (34, 0)], [(2600, 5, 1.0), (1500, 3, 0.4)], air=0.08),
        noise([(1.8, 0), (2.2, 0.22), (2.8, 0)], [(7500, 1.2, 1.0)], air=0.1),
        noise([(5.8, 0), (6.2, 0.20), (7, 0)], [(6500, 1.0, 1.0)], air=0.1)), -13.6),
    # toggle off: a short high ping, a brief crack, then a low thunk ringing out
    'off': finish(mix(38,
        tone([(2, 4500), (4, 4350), (6, 4200)],
             [(2, 0), (2.5, 0.40), (3.5, 0.36), (4.5, 0.18), (5.5, 0.06), (6.5, 0)], harm=(0, -30)),
        noise([(2, 0), (2.5, 0.12), (4, 0.15), (5, 0.10), (6.5, 0)], [(4400, 5, 1.0)], air=0.05),
        noise([(3.5, 0), (4.3, 0.22), (5.5, 0.30), (7, 0.26), (8.5, 0.14), (10, 0.04), (11, 0)],
              [(7500, 1.0, 1.0), (3000, 1.5, 0.7)], air=0.2),
        tone([(7, 720), (10, 750), (14, 790), (38, 780)],
             [(7, 0), (8, 0.14), (9, 0.28), (10.5, 0.32), (12, 0.26), (14, 0.12), (17, 0.10),
              (20, 0.07), (26, 0.04), (34, 0.025), (38, 0)], harm=(0, -18, -26)),
        noise([(7.5, 0), (8.5, 0.25), (10.5, 0.30), (12.5, 0.22), (14, 0.12), (17, 0.10),
               (20, 0.07), (30, 0.04), (37, 0)], [(760, 3, 1.0), (1500, 2.5, 0.7)], air=0.08),
        noise([(13, 0), (15, 0.03), (28, 0.025), (36, 0)], [(4500, 2, 1.0)], air=0.0)), -17.4),
    # trackbar / scroll tick: a sharp click, then a short ring
    'tick': finish(mix(14,
        noise([(1.3, 0), (1.6, 0.75), (2.2, 0.52), (2.6, 0)], [(5000, 1.2, 1.0)], air=0.3),
        tone([(2.5, 3200), (4, 3130), (14, 3140)],
             [(2.5, 0), (3.5, 0.06), (4.5, 0.20), (5, 0.33), (6, 0.28), (7, 0.20), (8, 0.13),
              (9, 0.11), (10, 0.06), (11, 0.04), (12.5, 0.02), (14, 0)], harm=(0, -31, -47, -57))), -15.2),
    # end of a list: a dull bump with a low tail
    'wall': finish(mix(33,
        noise([(0.3, 0), (0.8, 0.15), (1.5, 0.12), (2, 0)], [(2500, 2, 1.0)], air=0.3),
        tone([(1.5, 1600), (3, 1680), (5, 1450), (8, 1560), (11, 1570)],
             [(1.5, 0), (2, 0.09), (3, 0.07), (4, 0.11), (5, 0.16), (6, 0.14), (7, 0.11), (8, 0.13),
              (9, 0.10), (10.5, 0.03), (11, 0)], harm=(0, -30, -31, -38, -40)),
        tone([(10, 425), (33, 419)], [(10, 0), (12, 0.02), (19, 0.03), (23, 0.06), (26, 0.08),
                                      (29, 0.06), (31, 0.03), (33, 0)])), -21.0),
    # notification: a held tone and its echo, then a fifth higher
    'notification': finish(mix(174,
        tone([(0, 2115), (1, 2074), (4, 2097), (44, 2097)],
             [(0, 0), (1, 0.08), (2, 0.24), (4.5, 0.33), (27, 0.33), (30, 0.28), (34, 0.21),
              (38, 0.13), (42, 0.05), (45, 0.015), (48, 0)], harm=(0, -22, -22, -35, -60)),
        tone([(48, 2090), (76, 2100)], [(48, 0), (52, 0.025), (72, 0.03), (76, 0.02), (80, 0)], harm=(0, -26)),
        tone([(117, 3234), (119, 3152), (122, 3140), (160, 3130)],
             [(117, 0), (118, 0.14), (119, 0.31), (121, 0.36), (130, 0.38), (141, 0.33), (147, 0.27),
              (152, 0.18), (156, 0.10), (158, 0.05), (159, 0.04), (170, 0.04), (174, 0)],
             harm=(0, -13, -40, -62))), -15.0),
    # settings (overlay / package settings): a small plastic tap, a firmer click, then a
    # high tick and a softer second tap. Struck-object resonances (pitch and level curves)
    # plus the noise of each hit.
    'settings': finish(mix(98,
        # tap: two resonances (~1.95 / ~3.45 kHz)
        tone([(0, 2050), (0.5, 1881), (1, 2143), (1.5, 1895), (4, 1895)],
             [(0, 0), (0.5, 0.241), (1, 0.302), (1.5, 0.242), (2, 0.04), (2.5, 0.013), (4, 0)]),
        tone([(0, 2973), (0.5, 3425), (1, 3049), (1.5, 3839), (2, 3145), (4, 3145)],
             [(0, 0), (0.5, 0.247), (1, 0.224), (1.5, 0.175), (2, 0.072), (2.5, 0.002), (4, 0)]),
        # main click: three resonances (~1.24 / ~1.9 / ~3.2 kHz)
        tone([(7, 1428), (8.5, 1428), (9.5, 1328), (10, 1112), (10.5, 1243), (11, 1342), (11.5, 1187), (12, 1428), (12.5, 1097), (13, 1136), (13.5, 1287), (17, 1287)],
             [(7, 0), (7.5, 0.005), (8, 0.242), (8.5, 0.069), (9, 0.383), (9.5, 0.329), (10, 0.182), (10.5, 0.275), (11, 0.207), (12, 0.122), (12.5, 0.11), (13, 0.066), (13.5, 0.07), (14, 0.023), (17, 0)]),
        tone([(7, 2231), (8, 2231), (8.5, 2257), (9, 1680), (17, 1669)],
             [(7, 0), (7.5, 0.006), (8, 0.372), (8.5, 0.248), (9, 0.351), (9.5, 0.255), (10, 0.114), (10.5, 0.215), (11.5, 0.116), (12, 0.109), (13, 0.041), (13.5, 0.06), (14, 0.02), (17, 0)]),
        tone([(7, 2820), (8, 2820), (8.5, 3061), (9, 3085), (17, 3085)],
             [(7, 0), (7.5, 0.008), (8, 0.316), (8.5, 0.366), (9, 0.181), (9.5, 0.022), (11.5, 0.041), (12.5, 0.01), (17, 0)]),
        # high tick (~3.26 kHz)
        tone([(87, 3290), (88, 3290), (88.5, 3210), (89, 3301), (89.5, 3278), (91, 3278)],
             [(87, 0), (87.5, 0.001), (88, 0.058), (88.5, 0.321), (89, 0.322), (89.5, 0.118), (90, 0.028), (90.5, 0.046), (91, 0)]),
        # second tap: the first tap's resonances again, softer
        tone([(90, 2307), (91.5, 2307), (92, 2194), (92.5, 2194), (93, 2307), (93.5, 1705), (94, 2271), (94.5, 1943), (95, 1919), (95.5, 1955), (96, 2137), (96.5, 2229), (98, 2229)],
             [(90, 0), (90.5, 0.02), (92, 0.031), (92.5, 0.004), (93, 0.061), (93.5, 0.062), (94, 0.138), (95, 0.101), (95.5, 0.054), (97, 0.009), (98, 0)]),
        tone([(90, 2601), (90.5, 3099), (91, 3001), (91.5, 3001), (92, 2928), (92.5, 3099), (93, 2553), (93.5, 3099), (94, 2549), (94.5, 2291), (96, 2301), (96.5, 2649), (98, 2649)],
             [(90, 0), (90.5, 0.036), (91, 0.039), (91.5, 0.014), (92, 0.038), (92.5, 0.022), (93, 0.063), (93.5, 0.028), (94, 0.149), (94.5, 0.093), (95.5, 0.033), (96, 0.039), (97, 0.007), (98, 0)]),
        tone([(90, 3318), (91, 3232), (91.5, 3232), (92, 3159), (92.5, 3475), (93, 3387), (93.5, 3569), (94, 2950), (94.5, 3549), (95.5, 3549), (96, 2950), (98, 2950)],
             [(90, 0), (90.5, 0.045), (91, 0.04), (91.5, 0.011), (94, 0.095), (94.5, 0.076), (95, 0.007), (96.5, 0.019), (98, 0)]),
        tone([(90, 3734), (90.5, 3419), (91, 3419), (91.5, 3928), (92, 3419), (92.5, 3577), (93, 3695), (93.5, 3419), (94, 3419), (94.5, 3773), (95, 3918), (95.5, 3419), (98, 3419)],
             [(90, 0), (90.5, 0.032), (91.5, 0.012), (93.5, 0.06), (94, 0.045), (94.5, 0.073), (95, 0.027), (95.5, 0.012), (98, 0)]),
        # the noisy part of each hit (what the resonances leave out), per band (Hz, Q)
        noise([(0, 0), (1, 0.101), (1.5, 0.083), (2, 0.165), (2.5, 0.053), (4.5, 0)], [(525, 0.7, 1.0)], air=0.0),
        noise([(0, 0), (0.5, 0), (1, 0.137), (1.5, 0.112), (2.5, 0.017), (4.5, 0)], [(1530, 0.9, 1.0)], air=0.0),
        noise([(0, 0), (0.5, 0.087), (1, 0.087), (1.5, 0.126), (2, 0.027), (4.5, 0)], [(3600, 1.5, 1.0)], air=0.0),
        noise([(0, 0), (0.5, 0.108), (1, 0.057), (1.5, 0.08), (2.5, 0), (4.5, 0)], [(7750, 1.11, 1.0)], air=0.0),
        noise([(7, 0), (8, 0), (9, 0.105), (9.5, 0.058), (11, 0.028), (11.5, 0.053), (12.5, 0.013), (17, 0)], [(525, 0.7, 1.0)], air=0.0),
        noise([(7, 0), (7.5, 0.173), (8, 0), (8.5, 0.189), (9, 0.599), (9.5, 0), (10.5, 0), (11, 0.2), (11.5, 0.167), (12, 0), (12.5, 0), (13, 0.072), (14.5, 0), (17, 0)], [(1530, 0.9, 1.0)], air=0.0),
        noise([(7, 0), (8, 0.323), (8.5, 0), (9, 0), (9.5, 0.061), (10.5, 0), (11, 0), (11.5, 0.048), (12, 0), (17, 0)], [(3600, 1.5, 1.0)], air=0.0),
        noise([(7, 0), (7.5, 0.167), (8, 0.495), (8.5, 0.102), (9.5, 0.041), (10, 0.022), (10.5, 0.098), (11, 0.033), (17, 0)], [(7750, 1.11, 1.0)], air=0.0),
        noise([(87, 0), (88.5, 0.036), (89, 0.027), (89.5, 0.063), (90, 0.033), (90.5, 0.06), (91, 0.03), (93.5, 0.005), (96.5, 0.027), (98, 0)], [(525, 0.7, 1.0)], air=0.0),
        noise([(87, 0), (87.5, 0.013), (88, 0.061), (88.5, 0.069), (89, 0), (95, 0), (95.5, 0.063), (96.5, 0), (98, 0)], [(1530, 0.9, 1.0)], air=0.0),
        noise([(87, 0), (88, 0), (88.5, 0.297), (89, 0.123), (89.5, 0), (90.5, 0), (92, 0.053), (92.5, 0), (93, 0), (93.5, 0.088), (94, 0), (95.5, 0.018), (98, 0)], [(3600, 1.5, 1.0)], air=0.0),
        noise([(87, 0), (87.5, 0.006), (88, 0.058), (88.5, 0.065), (89, 0.021), (90, 0.006), (94.5, 0.021), (98, 0)], [(7750, 1.11, 1.0)], air=0.0),
        ), -14.5),
}


if __name__ == '__main__':
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out_dir, exist_ok=True)
    for name, samples in sounds.items():
        write(os.path.join(out_dir, name + '.wav'), samples)
