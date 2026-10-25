#!/usr/bin/env python3
"""Converts the three customer kits (LIVE DRUM, KIT DRUM, KIT 2) into embedded factory kits.

  python3 Tools/build_user_kits.py <folder-with-extracted-kits>

Pad-mapped samples  ->  Resources/Kits/k<kit>_<pad 01..16>_<Name>.flac
Spare samples       ->  Resources/Kits/k<kit>_x<NN>_<Name>.flac     (offered as "swap" choices in the plugin)
Lossless FLAC (24-bit where the source was 24-bit / float) keeps the plugin size small.
"""
import os, re, subprocess, sys, wave
import numpy as np

SRC = sys.argv[1]
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources", "Kits")

# pad index (0..15): kick, kick2, snare, rim, side stick, floor, low, mid, high tom, hh closed, hh pedal, hh open,
#                    crash1, crash2, ride, ride bell
KITS = {
    5: ("LIVE_DRUM/LIVE DRUM", {
        0: "Kick C1", 1: "KICK TAMA UNITED ", 2: "Snare C1", 3: "Snare High C3", 4: "Side Stick A3",
        5: "Tom4 C3", 6: "Tom3 C3", 7: "Tom2 C3", 8: "Tom1 C2",
        9: "HiHat-Closed B1", 10: "HiHat-Pedal B2", 11: "HiHat-Opened C3",
        12: "Crash 1 A3", 13: "Crash 2 A3", 14: "Ride A3", 15: "Ride Bell A4"}),
    6: ("KIT_DRUM/KIT DRUM", {
        0: "Kick-main-f", 2: "SnareL-main-mf", 3: "SnareT-main-f", 4: "XStickL-main-f",
        5: "Tom4-main-f", 6: "Tom3-main-f", 7: "Tom2-main-f", 8: "Tom1-main-f",
        12: "CrashLRSwell-oh", 13: "China-oh-f", 15: "RideZBell-oh-f"}),
    7: ("KIT_2/KIT 2", {
        0: "Seben Kick", 2: "Snare Hit", 3: "Double Snare Hit",
        5: "HARD_COLLECTORS_TOM 4_DRY.1", 6: "HARD_COLLECTORS_TOM 3_DRY.1",
        7: "HARD_COLLECTORS_TOM 2_DRY.1", 8: "HARD_COLLECTORS_TOM 1_DRY.1",
        9: "Closed Hi Hat", 10: "Pedal Hat", 11: "Open Hihat", 12: "Crash", 13: "Cymbal Light"}),
}


def slug(name):
    s = re.sub(r"[^A-Za-z0-9]+", "_", name.strip()).strip("_")
    return s


def peak_and_bits(path):
    with open(path, "rb") as f:
        head = f.read(64)
    fmt3 = b"\x03\x00" in head[20:22] or head[20:22] == b"\x03\x00"
    try:
        w = wave.open(path); bits = w.getsampwidth() * 8; w.close()
    except wave.Error:
        bits = 32   # float wav
    out = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-"], capture_output=True).stdout
    x = np.frombuffer(out, dtype=np.float32)
    return float(np.abs(x).max()) if len(x) else 1.0, bits


def convert(src, dst):
    peak, bits = peak_and_bits(src)
    af = []
    if peak > 0.98:
        af = ["-af", "volume=%.4f" % (0.98 / peak)]
    fmt = ["-sample_fmt", "s16"] if bits == 16 else ["-sample_fmt", "s32", "-bits_per_raw_sample", "24"]
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", src] + af + ["-c:a", "flac"] + fmt + [dst], check=True)


os.makedirs(DST, exist_ok=True)
for old in os.listdir(DST):
    if re.match(r"k[5-9]_", old):
        os.remove(os.path.join(DST, old))

summary = []
for kit, (folder, mapping) in KITS.items():
    d = os.path.join(SRC, folder)
    files = {os.path.splitext(f)[0]: f for f in os.listdir(d) if f.lower().endswith(".wav")}
    used = set()
    for pad, name in sorted(mapping.items()):
        assert name in files, (kit, name)
        convert(os.path.join(d, files[name]), os.path.join(DST, "k%d_%02d_%s.flac" % (kit, pad + 1, slug(name))))
        used.add(name)
    extra = sorted(n for n in files if n not in used)
    for i, name in enumerate(extra):
        convert(os.path.join(d, files[name]), os.path.join(DST, "k%d_x%02d_%s.flac" % (kit, i + 1, slug(name))))
    summary.append((kit, len(mapping), len(extra)))

for kit, m, e in summary:
    print("kit %d: %d pads mapped, %d spare samples" % (kit, m, e))
