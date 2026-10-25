#!/usr/bin/env python3
"""Builds the 'how to add samples / MIDI' tutorial video.
The plugin frames come from Tools/Snapshot (real editor + engine, storyboard mode); this script adds the
cursor, dragged file icons, captions and the Finder / file-dialog mock-ups that an offscreen render cannot show.

  python3 make_demo.py <snapshot_exe> <work_dir> <samples_dir> <kit_dir> <midi_file> <out.mp4>
"""
import math, os, re, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

exe, work, samples, kit, midi, out_mp4 = sys.argv[1:7]
os.makedirs(work, exist_ok=True)
frames_dir = os.path.join(work, "frames"); out_dir = os.path.join(work, "final")
for d in (frames_dir, out_dir):
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d): os.remove(os.path.join(d, f))

FPS, N = 30, 1170
layout = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source", "PluginEditor.cpp")).read()
def part(name):
    m = re.search(r'"%s", ([\d.]+)f, ([\d.]+)f' % name, layout)
    cx, cy = float(m.group(1)), float(m.group(2))
    s = min(626 / 660, 330 / 404)
    return (287 + (626 - 660 * s) / 2 + cx * s, 173 + cy * s)

SNARE, KICK, MIDTOM = part("SNARE"), part("KICK"), part("MID TOM")
LOAD, MYKITS, KITCARD, PLAY = (1106, 187), (994, 502), (1055, 412), (328, 609)
TAB_MIXER, TAB_GROOVE, TAB_PERF = (186, 114), (374, 114), (76, 114)
MIDIMAP = (1121, 27)
IMPORT = (812, 534)
START = (150, 735)

# ---------------------------------------------------------------- storyboard for the real plugin
sb = f"""
0|page|PERFORMANCE
120|select|2
120|load|2|{samples}/My Snare.wav
123|trigger|2
133|trigger|2
143|trigger|2
240|select|0
240|load|0|{samples}/My Kick.wav
243|trigger|0
253|trigger|0
263|trigger|0
318|select|7
322|trigger|7
385|load|7|{samples}/My Tom.wav
388|trigger|7
398|trigger|7
408|trigger|7
520|makekit|{kit}|My Studio Kit
585|kit|My Studio Kit
590|trigger|0
600|trigger|2
610|trigger|9
622|startgroove
770|page|MIXER
850|stopgroove
862|page|PERFORMANCE
880|page|GROOVE
940|importmidi|{midi}
1000|page|PERFORMANCE
1085|midimap|Roland/TD-17
"""
# storyboard timings are consolidated below (frame numbers above are authoritative)
open(os.path.join(work, "storyboard.txt"), "w").write(sb)

subprocess.run([exe, frames_dir, str(N), os.path.join(work, "storyboard.txt")], check=True,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
os.replace(os.path.join(frames_dir, "audio.wav"), os.path.join(work, "audio.wav"))

# ---------------------------------------------------------------- overlay helpers
def font(size, bold=True):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
              "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"):
        if os.path.exists(p): return ImageFont.truetype(p, size)
    return ImageFont.load_default()
F_CAP, F_SMALL, F_BIG, F_TINY = font(22), font(14), font(40), font(12, False)

def ease(t): return t * t * (3 - 2 * t)
def lerp(a, b, t): return a + (b - a) * t

# cursor keyframes: (frame, x, y)
KEYS = [(0, 700, 700), (40, 700, 700), (60, *START), (70, *START), (118, *SNARE), (160, *SNARE),
        (176, *START), (186, *START), (238, *KICK), (285, *KICK),
        (300, 640, 600), (316, *MIDTOM), (330, *MIDTOM), (362, *LOAD), (385, *LOAD), (420, 640, 500),
        (440, *MYKITS), (462, *MYKITS), (500, *MYKITS), (546, 1000, 360), (560, *KITCARD), (585, *KITCARD),
        (620, *PLAY), (640, *PLAY), (680, 640, 400), (735, 640, 400), (755, *TAB_MIXER), (770, *TAB_MIXER),
        (850, 700, 600), (858, *TAB_PERF), (872, *TAB_GROOVE), (890, *TAB_GROOVE), (898, *IMPORT), (908, *IMPORT),
        (930, 790, 450), (940, 790, 450), (985, 700, 300), (1005, *TAB_PERF), (1015, *MIDIMAP), (1030, *MIDIMAP),
        (1045, 1060, 362), (1060, 1060, 362), (1075, 790, 372), (1085, 790, 372), (1120, 700, 500), (1170, 700, 500)]
CLICKS = [123, 243, 322, 385, 462, 585, 640, 770, 862, 880, 900, 938, 1005, 1030, 1085]
# dragged file icon: (start, drop, label, from, to)
DRAGS = [(60, 118, "My Snare.wav", START, SNARE), (176, 238, "My Kick.wav", START, KICK)]

CAPTIONS = [
    (0, 55, "African Drum  -  demo", None),
    (56, 175, "Arraste um sample (WAV, AIFF, FLAC, MP3) direto para o tambor", "1"),
    (176, 295, "Funciona em qualquer tambor ou prato. O som toca na hora", "1"),
    (296, 420, "Ou selecione o tambor e clique em LOAD  (CLEAR limpa o pad)", "2"),
    (421, 550, "Kit completo: MY KITS abre a pasta. Uma pasta por kit, o nome do arquivo define o pad", "3"),
    (551, 700, "O kit aparece em KIT PRESETS e pode ser trocado a qualquer momento, até tocando", "3"),
    (701, 855, "No MIXER: mesa de som com 21 canais, grupos, mute/solo, pan, saída e choke", "4"),
    (856, 1000, "Aba GROOVE: IMPORT MIDI ou arraste .mid / pastas. Aparecem em  My MIDI", "5"),
    (1001, 1140, "MIDI MAP: General MIDI, AD2 Standard, Roland, Alesis, Yamaha... e MIDI Learn", "6"),
    (1141, 1170, "African Drum  |  African Gospel", None),
]

def cursor_pos(f):
    for (f0, x0, y0), (f1, x1, y1) in zip(KEYS, KEYS[1:]):
        if f0 <= f <= f1:
            t = ease((f - f0) / max(1, f1 - f0))
            return lerp(x0, x1, t), lerp(y0, y1, t)
    return KEYS[-1][1], KEYS[-1][2]

def draw_cursor(d, x, y):
    pts = [(x, y), (x, y + 21), (x + 5, y + 16), (x + 9, y + 25), (x + 13, y + 23), (x + 9, y + 15), (x + 16, y + 15)]
    d.polygon([(px + 1.5, py + 1.5) for px, py in pts], fill=(0, 0, 0, 90))
    d.polygon(pts, fill=(255, 255, 255, 255), outline=(0, 0, 0, 255))

def file_icon(d, x, y, label, alpha=235):
    w = d.textlength(label, font=F_SMALL) + 38
    d.rounded_rectangle((x + 14, y + 12, x + 14 + w, y + 40), 8, fill=(30, 36, 46, alpha), outline=(232, 146, 58, 255), width=2)
    d.rectangle((x + 22, y + 17, x + 36, y + 35), fill=(255, 255, 255, 255), outline=(60, 60, 60, 255))
    d.polygon([(x + 30, y + 17), (x + 36, y + 17), (x + 36, y + 23)], fill=(200, 200, 200, 255))
    d.text((x + 42, y + 18), label, font=F_SMALL, fill=(255, 255, 255, 255))

def window(d, box, title):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 + 4, y0 + 6, x1 + 4, y1 + 6), 12, fill=(0, 0, 0, 90))
    d.rounded_rectangle(box, 12, fill=(244, 244, 247, 255), outline=(150, 150, 160, 255), width=1)
    d.rounded_rectangle((x0, y0, x1, y0 + 30), 12, fill=(226, 226, 232, 255))
    d.rectangle((x0, y0 + 18, x1, y0 + 30), fill=(226, 226, 232, 255))
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        d.ellipse((x0 + 12 + i * 20, y0 + 9, x0 + 24 + i * 20, y0 + 21), fill=c)
    d.text((x0 + 84, y0 + 7), title, font=F_SMALL, fill=(60, 60, 70, 255))

def finder(d, f):
    box = (330, 200, 850, 520)
    window(d, box, "Documentos / African Gospel / African Drum / Kits")
    x0, y0 = box[0] + 20, box[1] + 48
    if f < 520:
        d.text((x0, y0 + 10), "(pasta vazia)  Crie uma pasta por kit e coloque os samples dentro", font=F_SMALL, fill=(110, 110, 120, 255))
        return
    d.text((x0, y0), "[pasta]  My Studio Kit", font=F_SMALL, fill=(20, 80, 160, 255))
    names = ["Kick", "Kick 2", "Snare", "Snare Rim", "Side Stick", "Tom Floor", "Tom Low", "Tom Mid",
             "Tom High", "Hat Closed", "Hat Pedal", "Hat Open", "Crash 1", "Crash 2", "Ride", "Ride Bell"]
    for i, n in enumerate(names):
        cx, cy = x0 + 20 + (i // 8) * 240, y0 + 34 + (i % 8) * 28
        d.rectangle((cx, cy, cx + 14, cy + 18), fill=(255, 255, 255, 255), outline=(90, 90, 100, 255))
        d.text((cx + 24, cy + 1), n + ".wav", font=F_SMALL, fill=(30, 30, 40, 255))
    d.text((x0, box[3] - 30), "O nome do arquivo (kick, snare, tom, hat, crash, ride...) escolhe o pad", font=F_TINY, fill=(110, 110, 120, 255))

MAIN = ["- Empty -", "AD2 Standard", "General MIDI", "African Drum Default (C1 - D#2)", None, "2box", "Alesis", "Efnote", "GEWA",
        "MegaDRUM", "Millenium", "Nux", "Other plugins", "Roland", "Simmons", "Yamaha"]
ROLAND = ["TD-1", "TD-02", "TD-07", "TD-11", "TD-17", "TD-25", "TD-27", "TD-30", "TD-50", "TD-50X"]

def menu_box(d, x, y, items, hl, arrows):
    w = 262
    h = sum(10 if it is None else 24 for it in items) + 12
    d.rounded_rectangle((x + 3, y + 5, x + w + 3, y + h + 5), 10, fill=(0, 0, 0, 110))
    d.rounded_rectangle((x, y, x + w, y + h), 10, fill=(40, 40, 44, 250), outline=(90, 90, 96, 255), width=1)
    cy = y + 6
    for it in items:
        if it is None:
            d.line((x + 10, cy + 5, x + w - 10, cy + 5), fill=(90, 90, 96, 255)); cy += 10; continue
        if it == hl: d.rounded_rectangle((x + 5, cy, x + w - 5, cy + 23), 6, fill=(232, 146, 58, 255))
        d.text((x + 16, cy + 4), it, font=F_SMALL, fill=(20, 12, 6, 255) if it == hl else (235, 235, 235, 255))
        if arrows and it in arrows: d.text((x + w - 24, cy + 4), ">", font=F_SMALL, fill=(20, 12, 6, 255) if it == hl else (200, 200, 200, 255))
        cy += 24

def midimap_menu(d, f):
    arrows = {"2box", "Alesis", "Efnote", "GEWA", "MegaDRUM", "Millenium", "Nux", "Other plugins", "Roland", "Simmons", "Yamaha"}
    hl = "Roland" if f >= 1045 else ("General MIDI" if f >= 1035 else None)
    menu_box(d, 930, 46, MAIN, hl, arrows)
    if f >= 1055:
        menu_box(d, 660, 258, ROLAND, "TD-17" if f >= 1068 else None, None)

def dialog(d):
    box = (360, 230, 840, 470)
    window(d, box, "Choose MIDI files to import")
    d.rounded_rectangle((box[0] + 20, box[1] + 50, box[2] - 20, box[1] + 84), 6, fill=(60, 120, 230, 255))
    d.text((box[0] + 34, box[1] + 59), "Lagos_Highlife_GM.mid", font=F_SMALL, fill=(255, 255, 255, 255))
    d.text((box[0] + 34, box[1] + 100), "Groove_Praise_6-8.mid", font=F_SMALL, fill=(30, 30, 40, 255))
    d.rounded_rectangle((box[2] - 110, box[3] - 46, box[2] - 20, box[3] - 16), 7, fill=(40, 120, 240, 255))
    d.text((box[2] - 84, box[3] - 40), "Open", font=F_SMALL, fill=(255, 255, 255, 255))

for f in range(N):
    img = Image.open(os.path.join(frames_dir, "frame_%04d.png" % f)).convert("RGBA")
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)

    if 455 <= f < 545: finder(d, f)
    if 904 <= f < 940: dialog(d)
    if 1022 <= f < 1088: midimap_menu(d, f)

    cx, cy = cursor_pos(f)
    for (a, b, label, p0, p1) in DRAGS:
        if a <= f <= b:
            file_icon(d, cx, cy, label)
    for c in CLICKS:
        if 0 <= f - c < 12:
            r = 6 + (f - c) * 3
            d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=(232, 146, 58, int(255 * (1 - (f - c) / 12))), width=3)
    draw_cursor(d, cx, cy)

    for (a, b, text, step) in CAPTIONS:
        if a <= f <= b:
            if step is None and f < 60:
                d.rounded_rectangle((120, 300, 1080, 470), 24, fill=(10, 14, 20, 225), outline=(232, 146, 58, 255), width=3)
                d.text((600, 358), "AFRICAN DRUM", font=F_BIG, fill=(255, 217, 163, 255), anchor="mm")
                d.text((600, 412), "samples, kits, MIDI e MIDI MAP", font=font(30), fill=(255, 255, 255, 255), anchor="mm")
            elif step is None:
                d.rounded_rectangle((220, 330, 980, 440), 24, fill=(10, 14, 20, 225), outline=(232, 146, 58, 255), width=3)
                d.text((600, 385), text, font=font(34), fill=(255, 217, 163, 255), anchor="mm")
            else:
                w = d.textlength(text, font=F_CAP) + 110
                x0 = 600 - w / 2
                d.rounded_rectangle((x0, 690, x0 + w, 748), 16, fill=(10, 14, 20, 232), outline=(232, 146, 58, 255), width=2)
                d.ellipse((x0 + 12, 702, x0 + 46, 736), fill=(232, 146, 58, 255))
                d.text((x0 + 29, 719), step, font=F_CAP, fill=(10, 14, 20, 255), anchor="mm")
                d.text((x0 + 62, 719), text, font=F_CAP, fill=(255, 255, 255, 255), anchor="lm")
            break

    Image.alpha_composite(img, ov).convert("RGB").save(os.path.join(out_dir, "f_%04d.png" % f))

subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", os.path.join(out_dir, "f_%04d.png"),
                "-i", os.path.join(work, "audio.wav"), "-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
                "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest", out_mp4], check=True)
print("ok", out_mp4)
