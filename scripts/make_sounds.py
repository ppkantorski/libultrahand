"""
File: make_sounds.py
Author: ppkantorski
Description:
    This script generates Ultrahand's default sound effects (enter, exit, move,
    notification, on, off, settings, tick, wall) as 16-bit mono 48 kHz WAV files.
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
rng = np.random.default_rng(2026)           # fixed seed: same files every run


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
    # settings: a soft tap, a warm two-note chime (D6 + A6 together), then a high D7
    'settings': finish(mix(90,
        click(0, 3, 1800, 0.6, 0.25, bright=0.1),
        blip(1, 1174.7, 1.0, 6, 24, 0.80, harm=(0, -10, -16, -24), curve=1.6),
        blip(1, 1760.0, 1.0, 6, 22, 0.55, harm=(0, -18, -28), curve=1.6),
        blip(62, 2349.3, 1.0, 4, 18, 0.50, harm=(0, -24, -32), curve=1.8)), -14.0),
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
}


if __name__ == '__main__':
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out_dir, exist_ok=True)
    for name, samples in sounds.items():
        write(os.path.join(out_dir, name + '.wav'), samples)
