#!/usr/bin/env python3
"""Generates the African Drum groove library (original patterns, Standard MIDI Files).

Notes follow the plugin's pad map (C1 = 36 ... D#2 = 51):
 36 Kick, 37 Kick 2, 38 Snare, 39 Snare Rim, 40 Side Stick, 41 Floor Tom, 42 Low Tom,
 43 Mid Tom, 44 High Tom, 45 HH Closed, 46 HH Pedal, 47 HH Open, 48 Crash 1, 49 Crash 2,
 50 Ride, 51 Ride Bell

Run:  python3 Tools/generate_midi.py
Output: Resources/MIDI/<Genre>/<Genre>__<Name>.mid
"""
import os
import random

PPQ = 480
BAR = PPQ * 4

NOTE = dict(K=36, K2=37, S=38, R=39, SS=40, FT=41, LT=42, MT=43, HT=44,
            HC=45, HP=46, HO=47, C1=48, C2=49, RD=50, RB=51)
VEL = {"g": 38, "o": 68, "x": 92, "X": 118}

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources", "MIDI")


def b(*groups):
    return "".join(groups)


# ---------------------------------------------------------------- building blocks (16 steps)
H8 = b("x.x.", "x.x.", "x.x.", "x.x.")
H16 = b("xoxo", "xoxo", "xoxo", "xoxo")
ST = b("....", "X...", "....", "X...")        # snare on 2 and 4
FOUR = b("X...", "X...", "X...", "X...")       # four on the floor

FILL16_A = dict(S=b("x.x.", "x.x.", "....", "...."), HT=b("....", "....", "xx..", "...."),
                MT=b("....", "....", "..xx", "...."), FT=b("....", "....", "....", "xxxx"),
                K=b("....", "....", "....", "X..."))
FILL16_B = dict(S=b("gggg", "oooo", "xxxx", "XXXX"))
FILL16_C = dict(HT=b("xxxx", "....", "....", "...."), MT=b("....", "xxxx", "....", "...."),
                LT=b("....", "....", "xxxx", "...."), FT=b("....", "....", "....", "xxxx"),
                K=b("X...", "X...", "X...", "X..."))
FILL12_A = dict(S=b("xxx", "xxx", "...", "..."), HT=b("...", "...", "xxx", "..."),
                FT=b("...", "...", "...", "xxx"), K=b("X..", "...", "...", "..."))
FILL12_B = dict(S=b("ooo", "ooo", "xxx", "xXX"))
FILL12_C = dict(HT=b("xx.", "...", "...", "..."), MT=b("...", "xx.", "...", "..."),
                LT=b("...", "...", "xx.", "..."), FT=b("...", "...", "...", "xxx"),
                K=b("X..", "...", "...", "..."))


class G:
    def __init__(self, genre, name, bpm, steps, main, alt=None, fill=None, crash=True,
                 ts=(4, 4), fillset=None, half=None):
        self.genre, self.name, self.bpm, self.steps = genre, name, bpm, steps
        self.main, self.alt, self.fill, self.crash = main, alt, fill, crash
        self.ts = ts
        self.bar = PPQ * 4 * ts[0] // ts[1]                  # ticks per bar
        self.fillset = fillset                                # (A, B, C) for non-4/4 or custom grids
        self.half = half if half is not None else steps // 2  # step of the "second half" accent


def pat16(**kw):
    return kw


GROOVES = []


def add(*a, **k):
    GROOVES.append(G(*a, **k))


# ---------------------------------------------------------------- WORSHIP
add("Worship", "Driving 8ths", 128, 16,
    dict(HC=H8, K=b("X...", "..x.", "X...", "..x."), S=ST),
    alt=dict(HC=b("x.x.", "x.x.", "x.x.", "x.x."), HO=b("....", "....", "....", "..x."),
             K=b("X...", "..x.", "X...", "..x."), S=ST),
    fill=FILL16_C)
add("Worship", "Floor Tom Build", 74, 16,
    dict(FT=b("X...", "X...", "X...", "X..."), K=b("X...", "....", "X...", "....")),
    alt=dict(FT=b("X...", "X...", "X...", "X..."), K=b("X...", "....", "X...", "...."),
             S=b("....", "....", "....", "x.x.")),
    fill=FILL16_C)
add("Worship", "Anthem Rock", 78, 16,
    dict(RD=b("x...", "x...", "x...", "x..."), K=b("X...", "....", "X...", "..x."), S=ST),
    alt=dict(HC=H8, K=b("X...", "....", "X...", "..x."), S=ST),
    fill=FILL16_A)
add("Worship", "Ambient Pulse", 70, 16,
    dict(HC=b("x...", "x...", "x...", "x..."), K=b("X...", "....", "....", "...."),
         SS=b("....", "....", "x...", "....")),
    alt=dict(HC=b("x...", "x...", "x...", "x.x."), K=b("X...", "....", "..x.", "...."),
             SS=b("....", "....", "x...", "....")),
    fill=FILL16_B)

# ---------------------------------------------------------------- ROCK
add("Rock", "Basic 8ths", 120, 16,
    dict(HC=H8, K=b("X...", "....", "X.x.", "...."), S=ST),
    alt=dict(HC=H8, K=b("X...", "..x.", "X...", "..x."), S=ST), fill=FILL16_A)
add("Rock", "Driving 8ths", 138, 16,
    dict(HC=H8, K=b("X.x.", "X.x.", "X.x.", "X.x."), S=ST),
    alt=dict(HC=b("x.x.", "x.x.", "x.x.", "x..."), HO=b("....", "....", "....", "..x."),
             K=b("X.x.", "X.x.", "X.x.", "X.x."), S=ST), fill=FILL16_C)
add("Rock", "Half-Time", 88, 16,
    dict(HC=H8, K=b("X...", "..x.", "....", "x..."), S=b("....", "....", "X...", "....")),
    alt=dict(HC=H8, K=b("X...", "..x.", "....", "x.x."), S=b("....", "....", "X...", "....")),
    fill=FILL16_A)
add("Rock", "16th Hats", 108, 16,
    dict(HC=H16, K=b("X...", "..x.", "X...", "...x"), S=b("....", "X..g", "..g.", "X.g.")),
    alt=dict(HC=H16, K=b("X...", "..x.", "X.x.", "...."), S=b("....", "X..g", "..g.", "X.g.")),
    fill=FILL16_C)
add("Rock", "Punk", 176, 16,
    dict(HC=H8, K=b("X.x.", "..x.", "X.x.", "..x."), S=ST),
    alt=dict(HC=H8, K=b("X.x.", "X.x.", "X.x.", "X.x."), S=ST), fill=FILL16_B)
add("Rock", "Hard Rock Ride", 118, 16,
    dict(RD=H8, K=b("X...", "....", "X.x.", "...."), S=ST),
    alt=dict(RD=H8, K=b("X...", "..x.", "X.x.", "...."), S=ST), fill=FILL16_A)
add("Rock", "Shuffle", 112, 12,
    dict(HC=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "...", "X..", "..x"), S=b("...", "X..", "...", "X..")),
    alt=dict(HC=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "...", "X..", "..x"),
             S=b("...", "X.g", "...", "X..")), fill=FILL12_A)

# ---------------------------------------------------------------- BLUES
add("Blues", "Slow Blues 12-8", 56, 12,
    dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=b("X..", "...", "X..", "..."), S=b("...", "X..", "...", "X..")),
    alt=dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=b("X..", "..o", "X..", "..."), S=b("...", "X..", "...", "X..")),
    fill=FILL12_A)
add("Blues", "Texas Shuffle", 126, 12,
    dict(HC=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "..x", "X..", "..x"), S=b("...", "X..", "...", "X..")),
    alt=dict(HC=b("X.x", "X.x", "X.x", "X.x"), K=b("X..", "..x", "X..", "..x"), S=b("...", "X.g", "...", "X..")),
    fill=FILL12_A)
add("Blues", "Chicago Shuffle", 108, 12,
    dict(RD=b("x.x", "x.x", "x.x", "x.x"), HP=b("...", "x..", "...", "x.."),
         K=b("X..", "...", "X..", "..."), S=b("...", "X..", "...", "X..")),
    alt=dict(RD=b("x.x", "x.x", "x.x", "x.x"), HP=b("...", "x..", "...", "x.."),
             K=b("X..", "...", "X..", "..x"), S=b("...", "X.g", "...", "X..")),
    fill=FILL12_A)
add("Blues", "Blues Rock", 96, 16,
    dict(HC=H8, K=b("X...", "..x.", "X...", "...."), S=b("....", "X..g", "..g.", "X...")),
    alt=dict(HC=H8, K=b("X...", "..x.", "X.x.", "...."), S=b("....", "X..g", "..g.", "X..g")),
    fill=FILL16_A)
add("Blues", "Jump Blues", 156, 12,
    dict(HC=b("x..", "x..", "x..", "x.."), K=b("X..", "X..", "X..", "X.."), S=b("...", "X.x", "...", "X.x")),
    alt=dict(RD=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "X..", "X..", "X.."), S=b("...", "X.x", "...", "X.x")),
    fill=FILL12_A)

# ---------------------------------------------------------------- POP
add("Pop", "Basic", 112, 16,
    dict(HC=H8, K=b("X...", "....", "X..x", "..x."), S=ST),
    alt=dict(HC=H8, K=b("X...", "..x.", "X..x", "..x."), S=ST), fill=FILL16_A)
add("Pop", "Dance Pop", 122, 16,
    dict(HC=b("x...", "x...", "x...", "x..."), HO=b("..x.", "..x.", "..x.", "..x."), K=FOUR, S=ST),
    alt=dict(HC=b("x...", "x...", "x...", "x.x."), HO=b("..x.", "..x.", "..x.", "..x."), K=FOUR, S=ST),
    fill=FILL16_B)
add("Pop", "Ballad", 72, 16,
    dict(HC=H8, K=b("X...", "....", "..x.", "x..."), S=ST),
    alt=dict(HC=H8, K=b("X...", "..x.", "..x.", "x..."), S=ST), fill=FILL16_B)
add("Pop", "Electro Pop", 124, 16,
    dict(HC=H16, K=FOUR, S=ST),
    alt=dict(HC=H16, K=b("X...", "X...", "X...", "X.x."), S=ST), fill=FILL16_C)

# ---------------------------------------------------------------- FUNK
add("Funk", "Groove A", 98, 16,
    dict(HC=H16, K=b("X..x", "..x.", "..xx", "...."), S=b("....", "X.gg", "g..g", "X.g.")),
    alt=dict(HC=H16, K=b("X..x", "..x.", "..x.", "x..."), S=b("....", "X.gg", "g..g", "X.gg")),
    fill=FILL16_A)
add("Funk", "Open Hat", 104, 16,
    dict(HC=H8, HO=b("....", "....", "....", "..x."), K=b("X..x", "..x.", "X...", "..x."),
         S=b("....", "X..g", "..g.", "X..g")),
    alt=dict(HC=H8, K=b("X..x", "..x.", "X...", "..x."), S=b("....", "X..g", "g.g.", "X..g")),
    fill=FILL16_C)
add("Funk", "Slow", 84, 16,
    dict(HC=H16, K=b("X...", "..x.", "...x", "..x."), S=b("....", "X..g", "g..g", "X...")),
    alt=dict(HC=H16, K=b("X...", "..x.", "...x", "x..."), S=b("....", "X..g", "g..g", "X..g")),
    fill=FILL16_B)
add("Funk", "Rock", 110, 16,
    dict(HC=H8, K=b("X.x.", "..x.", "X.x.", "..x."), S=b("....", "X...", "....", "X.g.")),
    alt=dict(HC=H8, K=b("X.x.", "..x.", "X.x.", "x..."), S=b("....", "X...", "....", "X.gg")),
    fill=FILL16_A)

# ---------------------------------------------------------------- SOUL / R&B
add("Soul", "RnB Groove", 86, 16,
    dict(HC=b("x.x.", "x.xx", "x.x.", "x.xx"), K=b("X...", "..x.", "...x", "x..."),
         S=b("....", "X...", "....", "X.g.")),
    alt=dict(HC=H16, K=b("X...", "..x.", "...x", "x..."), S=b("....", "X..g", "....", "X.g.")),
    fill=FILL16_A)
add("Soul", "Neo Soul Laid Back", 78, 16,
    dict(HC=b("o.o.", "o.o.", "o.o.", "o.o."), K=b("X..x", "....", "..x.", "x..."),
         S=b("....", "X..g", "..g.", "X...")),
    alt=dict(HC=H16, K=b("X..x", "....", "..x.", "x.x."), S=b("....", "X..g", "..g.", "X..g")),
    fill=FILL16_B)
add("Soul", "Motown Pulse", 120, 16,
    dict(HC=H8, K=b("X...", "....", "X.x.", "...."), S=b("....", "X...", "....", "X.x.")),
    alt=dict(HC=H8, K=b("X...", "..x.", "X.x.", "...."), S=b("....", "X...", "....", "X.x.")),
    fill=FILL16_A)

# ---------------------------------------------------------------- JAZZ
add("Jazz", "Swing Ride", 138, 12,
    dict(RD=b("x..", "x.x", "x..", "x.x"), HP=b("...", "x..", "...", "x.."),
         K=b("o..", "...", "o..", "..."), S=b("...", "...", "..g", "...")),
    alt=dict(RD=b("x..", "x.x", "x..", "x.x"), HP=b("...", "x..", "...", "x.."),
             K=b("o..", "...", "o..", "..."), S=b("...", "..g", "...", "..g")),
    fill=FILL12_A, crash=False)
add("Jazz", "Ballad Brush Feel", 66, 12,
    dict(RD=b("o..", "o.o", "o..", "o.o"), HP=b("...", "x..", "...", "x.."), K=b("o..", "...", "...", "...")),
    alt=dict(RD=b("o..", "o.o", "o..", "o.o"), HP=b("...", "x..", "...", "x.."), K=b("o..", "...", "o..", "...")),
    fill=FILL12_A, crash=False)
add("Jazz", "Bossa Nova", 118, 16,
    dict(SS=b("x..x", "..x.", "..x.", "x..."), K=b("X...", "..x.", "X...", "..x."),
         HC=b("o.o.", "o.o.", "o.o.", "o.o."), HP=b("....", "x...", "....", "x...")),
    alt=dict(SS=b("x..x", "..x.", "..x.", "x..."), K=b("X...", "..x.", "X...", "..x."),
             RD=b("x.x.", "x.x.", "x.x.", "x.x."), HP=b("....", "x...", "....", "x...")),
    fill=FILL16_A, crash=False)

# ---------------------------------------------------------------- LATIN
add("Latin", "Samba", 100, 16,
    dict(HC=H16, K=b("X..x", "X..x", "X..x", "X..x"), SS=b("..x.", "...x", "..x.", "x...")),
    alt=dict(HC=H16, K=b("X..x", "X..x", "X..x", "X..x"), SS=b("..x.", "...x", "..x.", "x..."),
             RB=b("x...", "....", "x...", "....")),
    fill=FILL16_C, crash=False)
add("Latin", "Reggaeton Dembow", 95, 16,
    dict(HC=H8, K=FOUR, S=b("...x", "..x.", "...x", "..x.")),
    alt=dict(HC=H16, K=FOUR, S=b("...x", "..x.", "...x", "..x.")), fill=FILL16_B)
add("Latin", "Afrobeat", 108, 16,
    dict(HC=H16, K=b("X..x", "..x.", "X...", "..x."), S=b("....", "X...", "...g", "X...")),
    alt=dict(RD=b("x.x.", "x.x.", "x.x.", "x.x."), K=b("X..x", "..x.", "X...", "..x."),
             S=b("....", "X...", "...g", "X..g")),
    fill=FILL16_C)

# ---------------------------------------------------------------- REGGAE
add("Reggae", "One Drop", 76, 16,
    dict(HC=H8, K=b("....", "....", "X...", "...."), R=b("....", "....", "X...", "....")),
    alt=dict(HC=H8, K=b("....", "....", "X...", "...."), R=b("....", "....", "X...", "....")),
    fill=FILL16_B, crash=False)
add("Reggae", "Steppers", 80, 16,
    dict(HC=H8, K=FOUR, S=b("....", "....", "X...", "....")),
    alt=dict(HC=H8, K=FOUR, S=b("....", "....", "X...", "...."), HO=b("....", "....", "....", "..x.")),
    fill=FILL16_A, crash=False)
add("Reggae", "Rockers", 82, 16,
    dict(HC=H8, K=FOUR, R=ST),
    alt=dict(HC=H8, K=FOUR, R=ST, HO=b("....", "....", "....", "..x.")), fill=FILL16_A, crash=False)

# ---------------------------------------------------------------- COUNTRY
add("Country", "Two Step", 118, 16,
    dict(HC=H8, K=b("X...", "....", "X...", "...."), S=ST),
    alt=dict(HC=H8, K=b("X...", "....", "X...", "..x."), S=ST), fill=FILL16_A)
add("Country", "Train Beat", 132, 16,
    dict(S=b("X.gx", "X.gx", "X.gx", "X.gx"), K=b("X...", "....", "X...", "....")),
    alt=dict(S=b("X.gx", "X.gx", "X.gx", "X.gx"), K=b("X...", "....", "X...", "...."),
             HC=b("....", "x...", "....", "x...")),
    fill=FILL16_B)

# ---------------------------------------------------------------- HIP-HOP
add("Hip-Hop", "Boom Bap", 90, 16,
    dict(HC=H8, K=b("X...", "..x.", "..x.", "...."), S=ST),
    alt=dict(HC=H8, K=b("X...", "..x.", "..x.", "..x."), S=ST), fill=FILL16_B)
add("Hip-Hop", "Trap", 140, 16,
    dict(HC=b("x.x.", "x.xx", "x.x.", "xxxx"), K=b("X...", "....", "..x.", "...x"),
         S=b("....", "....", "X...", "....")),
    alt=dict(HC=b("x.x.", "x.xx", "x.x.", "x.xx"), K=b("X...", "..x.", "....", "...x"),
             S=b("....", "....", "X...", "....")),
    fill=FILL16_B, crash=False)
add("Hip-Hop", "Lo-Fi", 78, 16,
    dict(HC=b("o.o.", "o.oo", "o.o.", "o.o."), K=b("X...", "..x.", "..x.", "...."),
         S=b("....", "X...", "....", "X..g")),
    alt=dict(HC=b("o.o.", "o.oo", "o.o.", "o.oo"), K=b("X...", "..x.", "..x.", "x..."),
             S=b("....", "X...", "....", "X..g")),
    fill=FILL16_B, crash=False)

# ---------------------------------------------------------------- METAL
add("Metal", "Double Kick", 160, 16,
    dict(HC=H8, K=b("xxxx", "xxxx", "xxxx", "xxxx"), S=ST),
    alt=dict(C2=b("X...", "....", "X...", "...."), K=b("xxxx", "xxxx", "xxxx", "xxxx"), S=ST),
    fill=FILL16_C)
add("Metal", "Thrash", 188, 16,
    dict(HC=H8, K=b("x.x.", "x.x.", "x.x.", "x.x."), S=ST),
    alt=dict(HC=H8, K=b("x.x.", "x.xx", "x.x.", "x.xx"), S=ST), fill=FILL16_B)
add("Metal", "Breakdown Half-Time", 96, 16,
    dict(C2=b("X...", "....", "....", "...."), K=b("X.x.", "..x.", "....", "x.xx"),
         S=b("....", "....", "X...", "....")),
    alt=dict(HC=b("x...", "x...", "x...", "x..."), K=b("X.x.", "..x.", "....", "x.xx"),
             S=b("....", "....", "X...", "....")),
    fill=FILL16_C)

# ---------------------------------------------------------------- DANCE
add("Dance", "Disco", 118, 16,
    dict(HC=b("x...", "x...", "x...", "x..."), HO=b("..x.", "..x.", "..x.", "..x."), K=FOUR, S=ST),
    alt=dict(HC=H16, HO=b("..x.", "..x.", "..x.", "..x."), K=FOUR, S=ST), fill=FILL16_B)
add("Dance", "House", 124, 16,
    dict(HO=b("..x.", "..x.", "..x.", "..x."), HC=b("x...", "x...", "x...", "x..."), K=FOUR, S=ST),
    alt=dict(HO=b("..x.", "..x.", "..x.", "..x."), HC=H16, K=FOUR, S=ST), fill=FILL16_B, crash=False)
add("Dance", "Drum and Bass", 174, 16,
    dict(HC=H8, K=b("X...", "....", "..x.", "...."), S=ST),
    alt=dict(HC=H8, K=b("X...", "....", "..x.", "..x."), S=ST), fill=FILL16_B, crash=False)

# ---------------------------------------------------------------- generated families (Gospel + Naija Praise)
def S(steps, **lv):
    """Pattern from step lists: S(16, X=[0, 8], x=[3], g=[7])."""
    out = ["."] * steps
    for ch, pos in lv.items():
        for p in pos:
            out[p] = ch
    return "".join(out)


def rng_(a, b_, step=1):
    return list(range(a, b_, step))


# fill sets for 12-step bars of 3 beats (3/4 and 6/8)
F3_A = dict(S=S(12, x=[0, 2]), HT=S(12, x=[4, 5]), MT=S(12, x=[6, 7]), FT=S(12, x=[8, 9, 10, 11]), K=S(12, X=[0]))
F3_B = dict(S=b("ogog", "xxxx", "XXXX"))
F3_C = dict(HT=S(12, x=[0, 1, 2, 3]), MT=S(12, x=[4, 5, 6, 7]), FT=S(12, x=[8, 9, 10, 11]), K=S(12, X=[0, 4, 8]))
F3 = (F3_A, F3_B, F3_C)


def sset(bar, ch_from, ch_to, only=None):
    return bar


def put(pat, positions, ch, only_empty=True):
    l = list(pat)
    for p in positions:
        if not only_empty or l[p] == ".":
            l[p] = ch
    return "".join(l)


def apply_mod(bar, mod, fam):
    out = dict(bar)
    steps = fam["steps"]
    beat = fam["beat"]
    if mod == "ride" and "HC" in out:
        out["RD"] = out.pop("HC")
    elif mod == "side" and "S" in out:
        out["SS"] = out.pop("S")
    elif mod == "rim" and "S" in out:
        out["R"] = out.pop("S")
    elif mod == "ghost" and "S" in out:
        out["S"] = put(out["S"], fam.get("ghost", []), "g")
    elif mod == "open" and "HC" in out:
        hc = list(out["HC"])
        ho = list(out.get("HO", "." * steps))
        for p in fam.get("open", []):
            if hc[p] != ".":
                hc[p] = "."
                ho[p] = "x"
        out["HC"], out["HO"] = "".join(hc), "".join(ho)
    elif mod == "push" and "K" in out:
        out["K"] = put(out["K"], fam.get("push", []), "x")
    elif mod == "toms":
        out["FT"] = put(out.get("FT", "." * steps), fam.get("tom", []), "x")
    elif mod == "thin" and "HC" in out:
        out["HC"] = "".join(c if i % beat == 0 else "." for i, c in enumerate(out["HC"]))
    elif mod == "bell":
        out["RB"] = put(out.get("RB", "." * steps), fam.get("bell", []), "x")
    elif mod == "double":
        out["K2"] = put(out.get("K2", "." * steps), fam.get("push", []), "g")
    return out


def sig(bar):
    return tuple(sorted(bar.items()))


def expand(genre, prefix, fam, variations, crash=True, fillset=None, ts=(4, 4)):
    seen = set()
    n = 0
    for suffix, mods, mult in variations:
        main = fam["main"]
        alt = fam["alt"]
        for m in mods:
            main = apply_mod(main, m, fam)
            alt = apply_mod(alt, m, fam)
        key = (sig(main), sig(alt))
        if key in seen:
            continue
        seen.add(key)
        name = f"{prefix} {fam['name']} - {suffix}"
        add(genre, name, max(40, int(round(fam["bpm"] * mult))), fam["steps"], main, alt=alt,
            fill=(fillset[0] if fillset else fam.get("fill")), crash=crash, ts=ts, fillset=fillset,
            half=fam.get("half"))
        n += 1
    return n


def fam(name, bpm, steps, main, alt, **kw):
    d = dict(name=name, bpm=bpm, steps=steps, main=main, alt=alt, beat=kw.pop("beat", steps // 4))
    d.update(kw)
    return d


V6 = [("Classic", [], 1.0), ("Ride Lift", ["ride"], 1.0), ("Ghost Notes", ["ghost"], 1.0),
      ("Open Hat", ["open"], 1.04), ("Kick Push", ["push"], 1.0), ("Sidestick Soft", ["side", "thin"], 0.88),
      ("Tom Drive", ["toms"], 1.0), ("Rimshot Shout", ["rim", "push"], 1.12)]
V4 = [("Classic", [], 1.0), ("Ride Lift", ["ride"], 1.0), ("Ghost Notes", ["ghost"], 1.0),
      ("Kick Push", ["push"], 1.0), ("Sidestick Soft", ["side", "thin"], 0.88), ("Slow Church", ["ghost", "thin"], 0.8)]
V3 = [("Classic", [], 1.0), ("Ride Lift", ["ride"], 1.0), ("Ghost Notes", ["ghost"], 1.0),
      ("Sidestick Soft", ["side", "thin"], 0.88), ("Kick Push", ["push"], 1.0)]

# ---- Gospel 4/4 (16 steps)
G44 = [
    fam("Praise Uptempo", 134, 16, dict(HC=S(16, x=[2, 6, 10, 14]), K=FOUR, S=ST), dict(HC=H8, K=FOUR, S=ST),
        ghost=[7, 15], open=[14], push=[7, 11, 15], tom=[6, 14], fill=FILL16_B),
    fam("Chop", 98, 16,
        dict(HC=b("x.x.", "x.x.", "x.x.", "x..."), HO=b("....", "....", "....", "..x."),
             K=b("X..x", "..x.", "..x.", "x..."), S=b("....", "X..g", "..g.", "X..g")),
        dict(HC=H8, K=b("X..x", "..x.", "X.x.", "x..."), S=b("....", "X..g", "..g.", "X.g.")),
        ghost=[7, 9], open=[10], push=[14, 15], tom=[6, 14], fill=FILL16_A),
    fam("Half-Time Groove", 76, 16,
        dict(HC=H8, K=b("X...", "..x.", "....", "x.x."), S=b("....", "....", "X...", "...g")),
        dict(HC=H8, K=b("X...", "..x.", "..x.", "x..."), S=b("....", "....", "X...", "..gg")),
        ghost=[7, 11], open=[6, 14], push=[10, 15], tom=[6, 14], fill=FILL16_A),
    fam("Ballad Sidestick", 66, 16,
        dict(HC=b("x...", "x...", "x...", "x..."), SS=ST, K=b("X...", "....", "x...", "..x.")),
        dict(HC=H8, SS=ST, K=b("X...", "..x.", "x...", "....")),
        ghost=[7], open=[6, 14], push=[7, 15], tom=[6, 14], fill=FILL16_B),
    fam("Church Shout", 142, 16,
        dict(HC=H8, K=S(16, X=[0, 8], x=[6, 14]), S=S(16, X=[4, 12], x=[15])),
        dict(HC=H8, K=S(16, X=[0, 8], x=[3, 6, 14]), S=S(16, X=[4, 12], x=[10, 15])),
        ghost=[7, 9], open=[6, 14], push=[10, 11], tom=[6, 14], fill=FILL16_C),
    fam("Gospel Fusion 16", 104, 16,
        dict(HC=H16, K=S(16, X=[0], x=[3, 7, 10]), S=S(16, X=[4, 12], g=[7, 9, 15])),
        dict(HC=H16, K=S(16, X=[0], x=[3, 7, 10, 14]), S=S(16, X=[4, 12], g=[1, 7, 15])),
        ghost=[1, 11], open=[6, 14], push=[12, 15], tom=[6, 14], fill=FILL16_A),
    fam("Offbeat Praise", 124, 16,
        dict(HO=S(16, x=[2, 6, 10, 14]), HC=S(16, x=[0, 4, 8, 12]), K=FOUR, S=ST),
        dict(HO=S(16, x=[2, 6, 10, 14]), HC=H16, K=FOUR, S=ST),
        ghost=[7, 15], open=[4], push=[7, 15], tom=[6, 14], fill=FILL16_B),
    fam("Worship Toms", 72, 16,
        dict(HC=S(16, x=[0, 4, 8, 12]), FT=S(16, x=[6, 14]), K=S(16, X=[0], x=[10]), S=S(16, X=[8])),
        dict(HC=S(16, x=[0, 4, 8, 12]), FT=S(16, x=[2, 6, 10, 14]), K=S(16, X=[0], x=[10, 14]), S=S(16, X=[8], g=[15])),
        ghost=[15], open=[12], push=[7, 14], tom=[2, 10], fill=FILL16_C),
    fam("Traditional Backbeat", 110, 16,
        dict(HC=H8, K=S(16, X=[0, 8]), S=ST),
        dict(HC=H8, K=S(16, X=[0], x=[2, 8, 10]), S=ST),
        ghost=[7, 15], open=[6, 14], push=[3, 14], tom=[6, 14], fill=FILL16_A),
    fam("Hand Clap Stomp", 118, 16,
        dict(K=FOUR, R=ST, HC=S(16, o=[2, 6, 10, 14])),
        dict(K=FOUR, R=ST, HC=S(16, x=[2, 6, 10, 14]), FT=S(16, x=[15])),
        ghost=[], open=[6], push=[7, 15], tom=[7, 15], fill=FILL16_B),
    fam("Gospel Swing 16", 94, 16,
        dict(HC=S(16, x=[0, 3, 4, 7, 8, 11, 12, 15]), K=S(16, X=[0], x=[6, 10]), S=S(16, X=[4, 12], g=[7, 15])),
        dict(HC=S(16, x=[0, 3, 4, 7, 8, 11, 12, 15]), K=S(16, X=[0], x=[3, 6, 10]), S=S(16, X=[4, 12], g=[1, 7, 15])),
        ghost=[1, 9], open=[7, 15], push=[14], tom=[6, 14], fill=FILL16_A),
    fam("Slow Burn Ride", 60, 16,
        dict(RD=H8, K=S(16, X=[0], x=[11]), SS=ST),
        dict(RD=H8, K=S(16, X=[0], x=[7, 11]), SS=ST, HP=S(16, x=[4, 12])),
        ghost=[], open=[], push=[14], tom=[6, 14], fill=FILL16_B),
]
for f in G44:
    expand("Gospel", "4-4", f, V6)

# ---- Gospel 12/8 (12 steps in 4 beats, triplet feel)
G128 = [
    fam("Shuffle", 92, 12,
        dict(HC=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "..x", "X..", "..."), S=b("...", "X.g", "..g", "X..")),
        dict(HC=b("x.x", "x.x", "x.x", "x.."), HO=b("...", "...", "...", "..x"),
             K=b("X..", "..x", "X..", "..."), S=b("...", "X.g", "..g", "X..")),
        ghost=[5, 8], open=[10], push=[3, 9], tom=[5, 11], fill=FILL12_A),
    fam("Slow Worship", 62, 12,
        dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=b("X..", "...", "X.o", "..."), S=b("...", "X..", "...", "X..")),
        dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=b("X..", "...", "X..", "..o"), S=b("...", "X..", "...", "X..")),
        ghost=[5, 11], open=[], push=[8, 11], tom=[5, 11], fill=FILL12_A),
    fam("Fast Shuffle", 128, 12,
        dict(HC=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "X..", "X..", "X.."), S=b("...", "X..", "...", "X..")),
        dict(RD=b("x.x", "x.x", "x.x", "x.x"), K=b("X..", "X..", "X..", "X.."), S=b("...", "X..", "...", "X..")),
        ghost=[5, 11], open=[10], push=[], tom=[5, 11], fill=FILL12_A),
    fam("Soul Ballad", 54, 12,
        dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=S(12, X=[0, 6], x=[8]), S=S(12, X=[3, 9], g=[11])),
        dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=S(12, X=[0, 6], x=[8, 10]), S=S(12, X=[3, 9], g=[5, 11])),
        ghost=[5, 7], open=[], push=[2, 11], tom=[5, 11], fill=FILL12_B),
    fam("Triplet Pulse", 100, 12,
        dict(HC=S(12, x=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]), K=S(12, X=[0, 6]), S=S(12, X=[3, 9])),
        dict(HC=S(12, x=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]), K=S(12, X=[0, 6], x=[8]), S=S(12, X=[3, 9], g=[11])),
        ghost=[5, 11], open=[2, 8], push=[10], tom=[5, 11], fill=FILL12_C),
]
for f in G128:
    expand("Gospel", "12-8", f, V4)

# ---- Gospel 3/4 (12 steps = 16ths over 3 beats)
G34 = [
    fam("Waltz Classic", 96, 12,
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0]), S=S(12, X=[4, 8])),
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0], x=[10]), S=S(12, X=[4, 8])),
        ghost=[7, 11], open=[10], push=[6, 10], tom=[6, 10], fill=F3_A, half=8),
    fam("Slow Waltz Worship", 66, 12,
        dict(HC=S(12, x=[0, 4, 8], o=[2, 6, 10]), K=S(12, X=[0]), SS=S(12, X=[4, 8])),
        dict(HC=S(12, x=[0, 4, 8], o=[2, 6, 10]), K=S(12, X=[0], x=[6]), SS=S(12, X=[4, 8], g=[11])),
        ghost=[11], open=[10], push=[6, 10], tom=[6, 10], fill=F3_B, half=8),
    fam("Waltz Swing", 108, 12,
        dict(HC=S(12, x=[0, 3, 4, 7, 8, 11]), K=S(12, X=[0], x=[6]), S=S(12, X=[4, 8], g=[11])),
        dict(HC=S(12, x=[0, 3, 4, 7, 8, 11]), K=S(12, X=[0], x=[3, 6]), S=S(12, X=[4, 8], g=[1, 11])),
        ghost=[1, 7], open=[7, 11], push=[10], tom=[6, 10], fill=F3_A, half=8),
    fam("Praise Three Beat", 120, 12,
        dict(HC=S(12, x=[2, 6, 10]), K=S(12, X=[0, 4, 8]), R=S(12, X=[4, 8])),
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0, 4, 8]), R=S(12, X=[4, 8])),
        ghost=[], open=[6, 10], push=[7, 11], tom=[7, 11], fill=F3_C, half=8),
    fam("Ride Waltz", 84, 12,
        dict(RD=S(12, x=[0, 4, 6, 8, 10]), HP=S(12, x=[4, 8]), K=S(12, X=[0]), S=S(12, g=[7, 11])),
        dict(RD=S(12, x=[0, 4, 6, 8, 10]), HP=S(12, x=[4, 8]), K=S(12, X=[0], x=[6]), S=S(12, g=[3, 7, 11])),
        ghost=[3], open=[], push=[10], tom=[6, 10], fill=F3_A, half=8),
]
for f in G34:
    expand("Gospel", "3-4", f, V3, fillset=F3, ts=(3, 4))

# ---- Gospel 6/8 (12 steps = 16ths over two dotted-quarter beats)
G68 = [
    fam("Dotted Praise", 72, 12,
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0], x=[8]), S=S(12, X=[6], g=[11])),
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0], x=[3, 8]), S=S(12, X=[6], g=[9, 11])),
        ghost=[9, 11], open=[10], push=[4, 10], tom=[5, 11], fill=F3_A, half=6, beat=6),
    fam("Slow Worship", 54, 12,
        dict(HC=S(12, X=[0], o=[2, 4, 6, 8, 10]), K=S(12, X=[0], x=[7]), S=S(12, X=[6])),
        dict(HC=S(12, X=[0], o=[2, 4, 6, 8, 10]), K=S(12, X=[0], x=[4, 7]), S=S(12, X=[6], g=[11])),
        ghost=[11], open=[], push=[10], tom=[5, 11], fill=F3_B, half=6, beat=6),
    fam("Gospel Swing", 84, 12,
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0, 6]), S=S(12, X=[3, 9])),
        dict(RD=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0, 6], x=[8]), S=S(12, X=[3, 9], g=[11])),
        ghost=[5, 11], open=[10], push=[8], tom=[5, 11], fill=F3_A, half=6, beat=6),
    fam("Fast Shout", 110, 12,
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0, 6]), S=S(12, X=[3, 9], x=[11])),
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0, 6], x=[4, 10]), S=S(12, X=[3, 9], x=[5, 11])),
        ghost=[1, 7], open=[4, 10], push=[8], tom=[5, 11], fill=F3_C, half=6, beat=6),
    fam("Ballad Sidestick", 60, 12,
        dict(HC=S(12, x=[0, 4, 8]), SS=S(12, X=[6]), K=S(12, X=[0], x=[7])),
        dict(HC=S(12, x=[0, 2, 4, 6, 8, 10]), SS=S(12, X=[6], g=[11]), K=S(12, X=[0], x=[7, 10])),
        ghost=[11], open=[10], push=[3], tom=[5, 11], fill=F3_B, half=6, beat=6),
    fam("Praise Skip", 96, 12,
        dict(HC=S(12, x=[0, 3, 6, 9]), K=S(12, X=[0, 6], x=[8]), R=S(12, X=[6])),
        dict(HC=S(12, x=[0, 3, 6, 9]), K=S(12, X=[0, 6], x=[4, 8]), R=S(12, X=[6], g=[11])),
        ghost=[11], open=[9], push=[10], tom=[5, 11], fill=F3_A, half=6, beat=6),
]
for f in G68:
    expand("Gospel", "6-8", f, V3 + [("Open Hat", ["open"], 1.04)], fillset=F3, ts=(6, 8))

# ---- Naija Praise (Nigerian praise & worship feels)
BELL12 = [0, 2, 4, 5, 7, 9, 11]
NV = [("Classic", [], 1.0), ("Bell Lift", ["bell"], 1.0), ("Ghost Notes", ["ghost"], 1.0),
      ("Open Hat", ["open"], 1.04), ("Kick Push", ["push"], 1.0), ("Ride Lift", ["ride"], 1.0)]

NAIJA44 = [
    fam("Afro Highlife", 118, 16,
        dict(HC=H8, K=S(16, X=[0, 8], x=[11]), S=S(16, X=[12], x=[4]), SS=S(16, x=[3, 6, 10, 14])),
        dict(HC=H8, K=S(16, X=[0, 8], x=[7, 11]), S=S(16, X=[4, 12]), SS=S(16, x=[3, 10, 14])),
        ghost=[7, 15], open=[6, 14], push=[3, 14], bell=[0, 3, 6, 10, 12], tom=[2, 10], fill=FILL16_C),
    fam("Praise Bounce", 124, 16,
        dict(HC=H16, K=S(16, X=[0], x=[3, 6, 10]), S=S(16, X=[4, 12], g=[7, 15]), RB=S(16, x=[2, 6, 10, 14])),
        dict(HC=H16, K=S(16, X=[0], x=[3, 6, 10, 14]), S=S(16, X=[4, 12], g=[1, 7, 15]), RB=S(16, x=[2, 6, 10, 14])),
        ghost=[1, 11], open=[6, 14], push=[12], bell=[0, 4, 8, 12], tom=[6, 14], fill=FILL16_A),
    fam("Afrobeat Praise", 108, 16,
        dict(HC=H16, K=S(16, X=[0], x=[7, 10]), S=S(16, X=[8]), SS=S(16, x=[3, 6, 11, 14])),
        dict(HC=H16, K=S(16, X=[0], x=[7, 10, 13]), S=S(16, X=[8], g=[15]), SS=S(16, x=[3, 6, 11, 14], g=[1])),
        ghost=[15], open=[6, 14], push=[4, 14], bell=[0, 6, 12], tom=[6, 14], fill=FILL16_C),
    fam("Juju Praise", 100, 16,
        dict(HC=H8, HT=S(16, x=[2, 6, 10, 14]), K=S(16, X=[0, 8]), SS=S(16, X=[4, 12])),
        dict(HC=H8, HT=S(16, x=[2, 6, 10, 14]), MT=S(16, x=[3, 11]), K=S(16, X=[0, 8], x=[10]), SS=S(16, X=[4, 12])),
        ghost=[], open=[6, 14], push=[3, 14], bell=[0, 3, 6, 10, 12], tom=[7, 15], fill=FILL16_C),
    fam("Talking Drums", 128, 16,
        dict(HT=S(16, x=[1, 3, 6, 9, 11, 14]), MT=S(16, x=[2, 7, 10, 15]), LT=S(16, x=[4, 12]),
             K=S(16, X=[0, 8]), S=S(16, X=[12])),
        dict(HT=S(16, x=[1, 3, 6, 9, 11, 14]), MT=S(16, x=[2, 7, 10, 15]), LT=S(16, x=[4, 12]),
             K=S(16, X=[0, 8], x=[11]), S=S(16, X=[12], g=[5]), HC=S(16, x=[0, 4, 8, 12])),
        ghost=[5, 13], open=[], push=[6, 14], bell=[0, 6, 12], tom=[7], fill=FILL16_C),
    fam("Owambe Dance", 132, 16,
        dict(HO=S(16, x=[2, 6, 10, 14]), K=FOUR, S=ST, RB=S(16, x=[0, 3, 6, 10, 12])),
        dict(HO=S(16, x=[2, 6, 10, 14]), HC=S(16, x=[0, 4, 8, 12]), K=FOUR, S=ST, RB=S(16, x=[0, 3, 6, 10, 12])),
        ghost=[7, 15], open=[], push=[7, 15], bell=[2, 8], tom=[6, 14], fill=FILL16_B),
    fam("Amapiano Praise", 112, 16,
        dict(HC=S(16, o=rng_(0, 16), x=[2, 6, 10, 14]), K=S(16, X=[0], x=[10]), LT=S(16, x=[3, 6, 11, 14]), R=S(16, X=[8])),
        dict(HC=S(16, o=rng_(0, 16), x=[2, 6, 10, 14]), K=S(16, X=[0], x=[7, 10]), LT=S(16, x=[3, 6, 11, 14]),
             R=S(16, X=[4, 12])),
        ghost=[], open=[6, 14], push=[14], bell=[2, 10], tom=[7, 15], fill=FILL16_B),
    fam("Afro Worship Slow", 70, 16,
        dict(HC=S(16, o=[0, 2, 4, 6, 8, 10, 12, 14]), K=S(16, X=[0], x=[10]), SS=S(16, X=[12]), RB=S(16, x=[6])),
        dict(HC=S(16, o=[0, 2, 4, 6, 8, 10, 12, 14]), K=S(16, X=[0], x=[7, 10]), SS=S(16, X=[4, 12]), RB=S(16, x=[6, 14])),
        ghost=[], open=[14], push=[6, 14], bell=[2, 10], tom=[6, 14], fill=FILL16_B),
    fam("Highlife Shuffle", 116, 16,
        dict(HC=S(16, x=[0, 3, 4, 7, 8, 11, 12, 15]), K=S(16, X=[0, 8], x=[6]), S=S(16, X=[4, 12])),
        dict(HC=S(16, x=[0, 3, 4, 7, 8, 11, 12, 15]), K=S(16, X=[0, 8], x=[6, 14]), S=S(16, X=[4, 12], g=[7]),
             RB=S(16, x=[2, 10])),
        ghost=[7, 15], open=[7, 15], push=[3, 11], bell=[0, 6, 12], tom=[6, 14], fill=FILL16_A),
    fam("Praise Break Shaker", 126, 16,
        dict(HC=b("xoxo", "xoxo", "xoxo", "xoxo"), K=S(16, X=[0, 10], x=[6]), S=S(16, X=[4, 12])),
        dict(HC=b("xoxo", "xoxo", "xoxo", "xoxo"), K=S(16, X=[0, 10], x=[6, 14]), S=S(16, X=[4, 12], g=[15]),
             C2=S(16, X=[0])),
        ghost=[7, 15], open=[6, 14], push=[3], bell=[2, 8, 14], tom=[6, 14], fill=FILL16_C),
]
for f in NAIJA44:
    expand("Naija Praise", "4-4", f, NV)

NAIJA128 = [
    fam("Afro Bell Praise", 100, 12,
        dict(RB=S(12, x=BELL12), K=S(12, X=[0, 6]), S=S(12, X=[3, 9]), HC=S(12, o=[0, 3, 6, 9])),
        dict(RB=S(12, x=BELL12), K=S(12, X=[0, 6], x=[8]), S=S(12, X=[3, 9], g=[11]), HC=S(12, o=[0, 3, 6, 9])),
        ghost=[5, 11], open=[], push=[10], bell=[], tom=[5, 11], fill=FILL12_C),
    fam("Slow Naija Worship", 60, 12,
        dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=S(12, X=[0], x=[8]), SS=S(12, X=[9]), RB=S(12, x=[6])),
        dict(HC=b("Xoo", "Xoo", "Xoo", "Xoo"), K=S(12, X=[0, 6], x=[8]), SS=S(12, X=[3, 9]), RB=S(12, x=[6, 11])),
        ghost=[], open=[], push=[10], bell=[2, 8], tom=[5, 11], fill=FILL12_B),
    fam("Juju Roll", 90, 12,
        dict(HT=S(12, x=[0, 2, 4, 6, 8, 10]), K=S(12, X=[0, 6]), SS=S(12, X=[3, 9])),
        dict(HT=S(12, x=[0, 2, 4, 6, 8, 10]), MT=S(12, x=[1, 7]), K=S(12, X=[0, 6], x=[8]), SS=S(12, X=[3, 9])),
        ghost=[], open=[], push=[10], bell=[0, 4, 7], tom=[5, 11], fill=FILL12_C),
]
for f in NAIJA128:
    expand("Naija Praise", "12-8", f, NV[:4])

NAIJA68 = [
    fam("Praise Dance", 96, 12,
        dict(RB=S(12, x=[0, 2, 5, 7, 9]), K=S(12, X=[0], x=[8]), S=S(12, X=[6]), HC=S(12, o=[0, 2, 4, 6, 8, 10])),
        dict(RB=S(12, x=[0, 2, 5, 7, 9]), K=S(12, X=[0], x=[3, 8]), S=S(12, X=[6], g=[11]),
             HC=S(12, o=[0, 2, 4, 6, 8, 10])),
        ghost=[9, 11], open=[], push=[10], bell=[], tom=[5, 11], fill=F3_A, half=6, beat=6),
    fam("Highlife Skip", 108, 12,
        dict(HC=S(12, x=[0, 3, 6, 9]), K=S(12, X=[0, 6]), SS=S(12, x=[2, 4], X=[8]), S=S(12, X=[6])),
        dict(HC=S(12, x=[0, 3, 6, 9]), K=S(12, X=[0, 6], x=[4, 10]), SS=S(12, x=[2], X=[8]), S=S(12, X=[6])),
        ghost=[11], open=[9], push=[10], bell=[0, 5, 9], tom=[5, 11], fill=F3_C, half=6, beat=6),
]
for f in NAIJA68:
    expand("Naija Praise", "6-8", f, NV[:4], fillset=F3, ts=(6, 8))

NAIJA34 = [
    fam("Naija Waltz Praise", 100, 12,
        dict(RB=S(12, x=[0, 4, 6, 10]), K=S(12, X=[0], x=[6]), S=S(12, X=[4, 8]), HC=S(12, o=[0, 2, 4, 6, 8, 10])),
        dict(RB=S(12, x=[0, 4, 6, 10]), K=S(12, X=[0], x=[6, 10]), S=S(12, X=[4, 8], g=[11]),
             HC=S(12, o=[0, 2, 4, 6, 8, 10])),
        ghost=[11], open=[], push=[10], bell=[2, 8], tom=[6, 10], fill=F3_A, half=8),
]
for f in NAIJA34:
    expand("Naija Praise", "3-4", f, NV[:4], fillset=F3, ts=(3, 4))


# ---------------------------------------------------------------- FILLS (fill bar + landing bar)
FILLS = [
    ("Tom Cascade", 100, 16, FILL16_C),
    ("Snare Roll Crescendo", 100, 16, FILL16_B),
    ("Snare to Toms", 100, 16, FILL16_A),
    ("Triplet Fill", 90, 12, FILL12_A),
    ("Gospel Turnaround", 92, 12,
     dict(S=b("x.x", "x.x", "x..", "..."), HT=b("...", "...", "..x", "x.x"), FT=b("...", "...", "...", "..x"),
          K=b("X..", "...", "X..", "..."))),
    ("Blues Turnaround", 80, 12,
     dict(K=b("X..", "...", "X..", "X.x"), S=b("...", "X..", "...", "xxx"))),
    ("Half-Time Fill", 80, 16,
     dict(S=b("....", "....", "x.x.", "xxxx"), K=b("X...", "....", "....", "...."))),
    ("Floor Tom Buildup", 74, 16,
     dict(FT=b("x...", "x...", "x.x.", "xxxx"), K=b("X...", "X...", "X...", "X..."))),
    ("Tom Run Up", 110, 16,
     dict(FT=b("xxxx", "....", "....", "...."), LT=b("....", "xxxx", "....", "...."),
          MT=b("....", "....", "xxxx", "...."), HT=b("....", "....", "....", "xxxx"))),
    ("Sixteenth Snare Break", 120, 16,
     dict(S=b("X...", "xxxx", "X...", "xxxx"), K=b("X...", "....", "X...", "...."))),
]
for nm, bpm, steps, fill in FILLS:
    add("Fills", nm, bpm, steps, fill, alt=None, fill=None, crash=False)
    GROOVES[-1].is_fill = True


# ---------------------------------------------------------------- writer
def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def write_midi(path, events, bpm, total_ticks, name, ts=(4, 4)):
    trk = bytearray()

    def add_ev(delta, data):
        trk.extend(vlq(delta))
        trk.extend(data)

    nm = name.encode()
    add_ev(0, b"\xFF\x03" + bytes([len(nm)]) + nm)
    add_ev(0, b"\xFF\x51\x03" + int(60_000_000 / bpm).to_bytes(3, "big"))
    den_pow = {2: 1, 4: 2, 8: 3}[ts[1]]
    add_ev(0, b"\xFF\x58\x04" + bytes([ts[0], den_pow, 0x18, 0x08]))

    last = 0
    for tick, _, data in sorted(events):
        add_ev(tick - last, data)
        last = tick
    add_ev(max(0, total_ticks - last), b"\xFF\x2F\x00")

    hdr = b"MThd" + (6).to_bytes(4, "big") + (0).to_bytes(2, "big") + (1).to_bytes(2, "big") + PPQ.to_bytes(2, "big")
    with open(path, "wb") as f:
        f.write(hdr + b"MTrk" + len(trk).to_bytes(4, "big") + bytes(trk))


def bar_events(bar, steps, bar_index, rng, voff=0, bar_ticks=BAR):
    ev = []
    step_ticks = bar_ticks // steps
    dur = min(step_ticks - 10, 110)
    for inst, pattern in bar.items():
        assert len(pattern) == steps, (inst, pattern, steps)
        for s, ch in enumerate(pattern):
            if ch == ".":
                continue
            tick = bar_index * bar_ticks + s * step_ticks
            vel = max(1, min(127, VEL[ch] + voff + rng.randint(-4, 4)))
            note = NOTE[inst]
            ev.append((tick, 1, bytes([0x99, note, vel])))
            ev.append((tick + dur, 0, bytes([0x89, note, 0])))
    return ev


# ---------------------------------------------------------------- song sections
HATS = ("HC", "HO", "RD", "HP", "SS", "RB")


def first(steps):
    return "X" + "." * (steps - 1)


def chorus_bar(bar, steps, half=None):
    """Open-hat accent on beat 3 for hi-hat grooves."""
    out = dict(bar)
    if "HC" in out:
        hc = list(out["HC"])
        ho = list(out.get("HO", "." * steps))
        pos = half if half is not None else steps // 2
        if hc[pos] != ".":
            hc[pos] = "."
            ho[pos] = "x"
        out["HC"], out["HO"] = "".join(hc), "".join(ho)
    return out


def bridge_bar(bar, steps, half=None):
    """Half-time feel: backbeat only on beat 3, hats move to the ride."""
    out = dict(bar)
    if "HC" in out and "RD" not in out:
        out["RD"] = out.pop("HC")
    if "S" in out:
        hp = half if half is not None else steps // 2
        out["S"] = "." * hp + "X" + "." * (steps - hp - 1)
    return out


def snare_build(bar, steps, step=None):
    out = dict(bar)
    step = step if step else (1 if steps == 12 else 2)
    pat = ["."] * steps
    for s in range(0, steps, step):
        frac = s / steps
        pat[s] = "o" if frac < 0.5 else ("x" if frac < 0.75 else "X")
    out["S"] = "".join(pat)
    return out


SECTION_FILES = [
    ("01", "Intro"), ("02", "Verse"), ("03", "PreChorus"), ("04", "Chorus"), ("05", "Bridge"),
    ("06", "Break"), ("07", "Outro"), ("08", "FillA"), ("09", "FillB"), ("10", "FillC"), ("11", "Groove"),
]


def make_sections(g):
    """Returns {section: [(bar_pattern, velocity_offset), ...]} (one entry per bar)."""
    steps = g.steps
    q = steps // 4
    main = dict(g.main)
    alt = dict(g.alt) if g.alt else dict(main)
    if g.fillset:
        fillA, fillB, fillC = g.fillset
    else:
        fillA = g.fill or (FILL12_A if steps == 12 else FILL16_A)
        fillB = FILL12_B if steps == 12 else FILL16_B
        fillC = FILL12_C if steps == 12 else FILL16_C
    one = first(steps)

    def crash(bar):
        out = dict(bar)
        if g.crash:
            out["C1"] = one
        return out

    soft = {k: v for k, v in main.items() if k in HATS}
    intro_soft = (soft, -14) if soft else (main, -24)

    hit = {"C1": one, "K": one, "S": one}
    roll = "." * (3 * q) + "x" * (q - 1) + "X"
    hit2 = {"C1": one, "K": one, "S": roll}
    landing = crash(main)
    landing["K"] = one if "K" not in landing else landing["K"]

    sec = {}
    sec["Intro"] = [intro_soft, intro_soft, (main, -6), (fillA, 0)]
    sec["Verse"] = [(main, -6), (main, -6), (main, -6), (alt, -6)]
    sec["PreChorus"] = [(main, 0), (alt, 0), (snare_build(main, steps, 2 if g.ts != (4, 4) else None), 4), (fillA, 6)]
    sec["Chorus"] = [(crash(chorus_bar(main, steps, g.half)), 8), (chorus_bar(alt, steps, g.half), 8),
                     (chorus_bar(main, steps, g.half), 8), (fillB, 8)]
    sec["Bridge"] = [(crash(bridge_bar(main, steps, g.half)), -8), (bridge_bar(main, steps, g.half), -8),
                     (bridge_bar(alt, steps, g.half), -8), (fillC, 0)]
    sec["Break"] = [(hit, 6), (hit2, 6)]
    sec["Outro"] = [(main, -4), (alt, -8), (fillA, 0), ({"C1": one, "K": one}, 10)]
    sec["FillA"] = [(main, 0), (main, 0), (fillA, 4), (landing, 8)]
    sec["FillB"] = [(main, 0), (main, 0), (fillB, 4), (landing, 8)]
    sec["FillC"] = [(main, 0), (main, 0), (fillC, 4), (landing, 8)]
    sec["Groove"] = [(main, 0), (main, 0), (alt, 0), (fillA, 0)]

    full = []
    for name in ("Intro", "Verse", "PreChorus", "Chorus", "Verse", "Chorus", "Bridge", "Break", "Chorus", "Outro"):
        full += sec[name]
    sec["FullSong"] = full
    return sec


def safe(s):
    return s.replace(" ", "_")


def write_bars(path, bars, steps, bpm, rng, title, bar_ticks=BAR, ts=(4, 4)):
    events = []
    for i, (bar, voff) in enumerate(bars):
        events += bar_events(bar, steps, i, rng, voff, bar_ticks)
    write_midi(path, events, bpm, len(bars) * bar_ticks, title, ts)


def main():
    os.makedirs(OUT, exist_ok=True)
    for root, _, files in os.walk(OUT):
        for f in files:
            if f.endswith(".mid"):
                os.remove(os.path.join(root, f))

    count = 0
    for g in GROOVES:
        folder = os.path.join(OUT, g.genre)
        os.makedirs(folder, exist_ok=True)
        rng = random.Random(f"{g.genre}-{g.name}")
        base = f"{safe(g.genre)}__{safe(g.name)}"

        if getattr(g, "is_fill", False):
            one = first(g.steps)
            bars = [(g.main, 0), ({"C1": one, "K": one}, 6)]
            write_bars(os.path.join(folder, f"{base}__00_Fill.mid"), bars, g.steps, g.bpm, rng,
                       f"{g.genre} - {g.name}")
            count += 1
            continue

        sec = make_sections(g)
        write_bars(os.path.join(folder, f"{base}__00_FullSong.mid"), sec["FullSong"], g.steps, g.bpm, rng,
                   f"{g.genre} - {g.name} - Full Song", g.bar, g.ts)
        count += 1
        for num, name in SECTION_FILES:
            write_bars(os.path.join(folder, f"{base}__{num}_{name}.mid"), sec[name], g.steps, g.bpm, rng,
                       f"{g.genre} - {g.name} - {name}", g.bar, g.ts)
            count += 1

    # hand-made / customer MIDI files kept in Tools/custom_midi/<Genre>/ are embedded as they are
    import shutil
    custom = os.path.join(os.path.dirname(os.path.abspath(__file__)), "custom_midi")
    for root, _, files in os.walk(custom):
        for f in files:
            if f.endswith(".mid"):
                genre = os.path.basename(root)
                os.makedirs(os.path.join(OUT, genre), exist_ok=True)
                shutil.copy(os.path.join(root, f), os.path.join(OUT, genre, f))
                count += 1

    n_groove = len([g for g in GROOVES if not getattr(g, "is_fill", False)])
    genres = sorted({g.genre for g in GROOVES})
    print(f"wrote {count} MIDI files ({n_groove} grooves x 12 sections + fills) in {len(genres)} genres")
    print(", ".join(genres))


if __name__ == "__main__":
    main()
