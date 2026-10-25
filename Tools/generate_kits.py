#!/usr/bin/env python3
"""Generates the four factory kits (16 samples each) used by DBL Drums.

The samples are synthesized (modal / noise synthesis) - they are NOT recordings of any
commercial drum brand. Run:  python3 Tools/generate_kits.py
Output: Resources/Kits/k<kit>_<pad>_<name>.wav
"""
import os
import wave
import numpy as np
from scipy import signal

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources", "Kits")

SLUGS = ["kick", "kick2", "snare", "snarerim", "sidestick", "floortom", "lowtom", "midtom",
         "hightom", "hhclosed", "hhpedal", "hhopen", "crash1", "crash2", "ride", "ridebell"]

KITS = {
    1: dict(name="Studio Maple", kick=60, kick_dec=0.34, click=0.5, snare=190, snare_dec=0.22, snare_noise=0.75,
            toms=(82, 108, 138, 176), tom_dec=0.55, hat=1.0, crash=1.5, ride=1.0, bright=1.0),
    2: dict(name="Rock Birch", kick=52, kick_dec=0.44, click=0.8, snare=172, snare_dec=0.30, snare_noise=0.95,
            toms=(72, 96, 122, 156), tom_dec=0.70, hat=1.1, crash=1.7, ride=0.9, bright=1.15),
    3: dict(name="Jazz Bop", kick=78, kick_dec=0.24, click=0.25, snare=225, snare_dec=0.17, snare_noise=0.55,
            toms=(112, 142, 178, 224), tom_dec=0.45, hat=0.8, crash=1.1, ride=1.35, bright=0.8),
    4: dict(name="Tight Funk", kick=64, kick_dec=0.17, click=0.6, snare=245, snare_dec=0.12, snare_noise=0.7,
            toms=(96, 126, 158, 198), tom_dec=0.32, hat=0.9, crash=1.0, ride=0.8, bright=1.05),
}


def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def noise(n, rng):
    return rng.standard_normal(n)


def band(x, lo, hi, order=4):
    sos = signal.butter(order, [lo, hi], btype="band", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def high(x, f, order=4):
    sos = signal.butter(order, f, btype="high", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def low(x, f, order=4):
    sos = signal.butter(order, f, btype="low", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def env(t, tau, attack=0.0005):
    e = np.exp(-t / tau)
    if attack > 0:
        e *= np.minimum(1.0, t / attack)
    return e


def sweep_sine(t, f_end, f_start_mult, sweep_tau, extra_phase=0.0):
    f = f_end * (1.0 + (f_start_mult - 1.0) * np.exp(-t / sweep_tau))
    phase = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(phase + extra_phase)


def norm(x, peak=0.9):
    m = np.max(np.abs(x)) + 1e-9
    return x / m * peak


def fade_out(x, ms=12):
    n = int(SR * ms / 1000)
    if x.ndim == 1:
        x[-n:] *= np.linspace(1, 0, n)
    else:
        x[:, -n:] *= np.linspace(1, 0, n)
    return x


def kick(p, rng, variant=1):
    f0 = p["kick"] * (1.0 if variant == 1 else 1.18)
    dec = p["kick_dec"] * (1.0 if variant == 1 else 0.7)
    t = t_axis(0.8)
    body = sweep_sine(t, f0, 3.2, 0.028) * env(t, dec)
    sub = np.sin(2 * np.pi * f0 * 0.5 * t) * env(t, dec * 0.8) * 0.25
    click = high(noise(len(t), rng), 2500) * env(t, 0.004) * p["click"] * (1.0 if variant == 1 else 1.4)
    beater = band(noise(len(t), rng), 800, 3500) * env(t, 0.012) * 0.25
    x = body + sub + click + beater
    x = np.tanh(x * 1.4)
    return norm(fade_out(x), 0.95)


def snare(p, rng, mode="main"):
    f = p["snare"]
    if mode == "main":
        t = t_axis(0.55)
        tone = (sweep_sine(t, f, 1.5, 0.02) * env(t, 0.09) * 0.8
                + sweep_sine(t, f * 1.62, 1.3, 0.02) * env(t, 0.06) * 0.4)
        wires = band(noise(len(t), rng), 1800, 9500) * env(t, p["snare_dec"]) * p["snare_noise"]
        crack = high(noise(len(t), rng), 3000) * env(t, 0.006) * 0.6
        x = tone + wires + crack
    elif mode == "rim":
        t = t_axis(0.45)
        tone = sweep_sine(t, f * 1.9, 1.2, 0.01) * env(t, 0.05) * 0.7
        ring = np.sin(2 * np.pi * 1450 * t) * env(t, 0.03) * 0.5
        wires = band(noise(len(t), rng), 2000, 10000) * env(t, p["snare_dec"] * 0.9) * p["snare_noise"] * 1.1
        crack = high(noise(len(t), rng), 2500) * env(t, 0.004) * 0.9
        x = tone + ring + wires + crack
    else:  # side stick
        t = t_axis(0.2)
        x = (np.sin(2 * np.pi * 1750 * t) * env(t, 0.012) * 0.7
             + np.sin(2 * np.pi * 980 * t) * env(t, 0.02) * 0.6
             + band(noise(len(t), rng), 1200, 6000) * env(t, 0.01) * 0.5)
    x = np.tanh(x * 1.3)
    return norm(fade_out(x), 0.9)


def tom(p, rng, idx):
    f = p["toms"][idx]
    t = t_axis(1.0)
    dec = p["tom_dec"]
    x = (sweep_sine(t, f, 1.7, 0.045) * env(t, dec)
         + sweep_sine(t, f * 1.51, 1.4, 0.04) * env(t, dec * 0.45) * 0.28
         + sweep_sine(t, f * 2.2, 1.2, 0.03) * env(t, dec * 0.25) * 0.12)
    x += band(noise(len(t), rng), 600, 4000) * env(t, 0.012) * 0.45
    x = np.tanh(x * 1.2)
    return norm(fade_out(x), 0.92)


METAL = np.array([263.0, 400.0, 421.0, 474.0, 587.0, 845.0]) * 2.1


def metal(t, rng, scale=1.0):
    x = np.zeros_like(t)
    for fr in METAL * scale:
        x += np.sign(np.sin(2 * np.pi * fr * t + rng.uniform(0, 6.28)))
    return x / len(METAL)


def hat(p, rng, mode):
    br = p["bright"]
    if mode == "closed":
        t = t_axis(0.22)
        x = high(metal(t, rng, br), 6500) * env(t, 0.035) + high(noise(len(t), rng), 7500) * env(t, 0.03) * 0.8
    elif mode == "pedal":
        t = t_axis(0.18)
        x = (band(metal(t, rng, br * 0.9), 4000, 11000) * env(t, 0.03)
             + np.sin(2 * np.pi * 180 * t) * env(t, 0.02) * 0.4
             + high(noise(len(t), rng), 6000) * env(t, 0.025) * 0.5)
    else:
        t = t_axis(0.9)
        e = env(t, 0.32 * p["hat"])
        x = high(metal(t, rng, br), 6000) * e + high(noise(len(t), rng), 7000) * e * 0.9
        x *= (1 + 0.15 * np.sin(2 * np.pi * 9 * t))
    x = fade_out(x)
    return norm(x, 0.7)


def stereo_noise(n, rng):
    return np.stack([noise(n, rng), noise(n, rng)])


def cymbal(p, rng, kind):
    if kind == "crash":
        dur = p["crash"]
        t = t_axis(dur)
        e = env(t, dur * 0.33, attack=0.0015)
        parts = [350, 530, 790, 1190, 1670, 2310, 3170, 4300, 5900]
        tonal = sum(np.sin(2 * np.pi * (fr * rng.uniform(0.97, 1.03)) * t + rng.uniform(0, 6.28)) * env(t, dur * 0.25) / (1 + i * 0.25)
                    for i, fr in enumerate(parts)) * 0.25
        n2 = np.stack([high(noise(len(t), rng), 2500), high(noise(len(t), rng), 2500)])
        x = n2 * e * 0.8 + tonal * e
    elif kind == "ride":
        dur = 2.2 * p["ride"] if p["ride"] < 1.2 else 2.6
        t = t_axis(dur)
        parts = [520, 770, 1130, 1580, 2250, 3400, 5200]
        tonal = sum(np.sin(2 * np.pi * (fr * rng.uniform(0.985, 1.015)) * t) * env(t, 0.9 * p["ride"] + 0.2) / (1 + i * 0.3)
                    for i, fr in enumerate(parts)) * 0.35
        wash = np.stack([high(noise(len(t), rng), 4500), high(noise(len(t), rng), 4500)]) * env(t, 0.5 * p["ride"] + 0.15) * 0.45
        ping = high(noise(len(t), rng), 3000) * env(t, 0.012) * 0.5
        x = wash + tonal + ping
    else:  # bell
        dur = 1.4
        t = t_axis(dur)
        parts = [(690, 1.0), (1210, 0.7), (1890, 0.55), (2740, 0.4), (3820, 0.3), (5300, 0.15)]
        tonal = sum(np.sin(2 * np.pi * fr * t) * env(t, 0.55 * p["ride"]) * a for fr, a in parts)
        ping = high(noise(len(t), rng), 3500) * env(t, 0.01) * 0.5
        x = np.stack([tonal + ping, tonal * 0.98 + ping])
    x = fade_out(x, 30)
    return norm(x, 0.8)


def write_wav(path, x):
    x = np.clip(x, -1, 1)
    if x.ndim == 1:
        data = (x * 32767).astype("<i2")
        ch = 1
    else:
        data = (x.T * 32767).astype("<i2")
        ch = 2
    with wave.open(path, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


def main():
    os.makedirs(OUT, exist_ok=True)
    for old in os.listdir(OUT):
        if old.endswith(".wav"):
            os.remove(os.path.join(OUT, old))

    total = 0
    for k, p in KITS.items():
        rng = np.random.default_rng(1000 + k)
        sounds = [
            kick(p, rng, 1), kick(p, rng, 2),
            snare(p, rng, "main"), snare(p, rng, "rim"), snare(p, rng, "stick"),
            tom(p, rng, 0), tom(p, rng, 1), tom(p, rng, 2), tom(p, rng, 3),
            hat(p, rng, "closed"), hat(p, rng, "pedal"), hat(p, rng, "open"),
            cymbal(p, rng, "crash"), cymbal(p, rng, "crash"),
            cymbal(p, rng, "ride"), cymbal(p, rng, "bell"),
        ]
        for i, (slug, x) in enumerate(zip(SLUGS, sounds)):
            fn = os.path.join(OUT, f"k{k}_{i + 1:02d}_{slug}.wav")
            write_wav(fn, x)
            total += os.path.getsize(fn)
    print(f"wrote {4 * 16} files, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
