#!/usr/bin/env python3
"""Video: tuning the drums with the TUNER of each mixer channel.
  python3 make_tuner_demo.py <snapshot_exe> <work_dir> <out.mp4>"""
import os, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

exe, work, out_mp4 = sys.argv[1:4]
frames_dir, out_dir = os.path.join(work, "frames"), os.path.join(work, "final")
for d in (work, frames_dir, out_dir):
    os.makedirs(d, exist_ok=True)
for d in (frames_dir, out_dir):
    for f in os.listdir(d): os.remove(os.path.join(d, f))

NAMES = ["Kick", "Kick 2", "Snare", "Snare Rim", "Side Stick", "Tom Floor", "Tom Low", "Tom Mid", "Tom High",
         "HH Closed", "HH Pedal", "HH Open", "Crash 1", "Splash", "Ride", "Ride Bell", "Crash 2", "Crash 3",
         "Splash 2", "China", "FX Cymbal"]
KITS = ["Standard Kit", "Soukous Fusion", "Seben Kit"]
DRUMS = [0, 2, 4, 8, 7, 6, 5]                       # kick, snare, side stick, toms high -> floor
CYMS_ALL = [9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]
CYMS_SHORT = [9, 11, 12, 14]

sb, caps = [], []
def ev(frame, text): sb.append(f"{frame}|{text}")

# (pad, name, fine-tune in semitones that brings the detected note to 0 cents)
TUNE = [(0, "Kick", -0.09), (2, "Snare", -0.39), (8, "Tom High", 0.22), (7, "Tom Mid", -0.46), (6, "Tom Low", 0.05), (5, "Tom Floor", 0.41)]
ev(0, "kit|Standard Kit"); ev(1, "page|MIXER")
caps.append((0, 70, "AFRICAN DRUM", "TUNER em cada canal do mixer"))
f = 74
for pad, name, st in TUNE:
    ev(f, f"trigger|{pad}")
    caps.append((f, f + 44, f"TUNER  -  {name}", "Antes: ouvindo a peça desafinada (ver centésimos)"))
    f += 22
    ev(f, f"trigger|{pad}")
    f += 24
    steps = 30
    for k in range(1, steps + 1):
        ev(f + k, f"tune|{pad}|{st * k / steps:.3f}")
    caps.append((f, f + steps + 6, f"TUNER  -  {name}", "Girando o tuner até ficar verde (0 cents)"))
    f += steps + 8
    ev(f, f"trigger|{pad}")
    caps.append((f, f + 24, f"TUNER  -  {name}", "Depois: afinado"))
    f += 22
    ev(f, f"trigger|{pad}")
    f += 30
ev(f, "startgroove")
caps.append((f, f + 190, "TUDO AFINADO", "Groove tocando com a bateria afinada"))
f += 190
ev(f, "stopgroove")
f += 20
N = f

open(os.path.join(work, "storyboard.txt"), "w").write("\n".join(sb) + "\n")
subprocess.run([exe, frames_dir, str(N), os.path.join(work, "storyboard.txt")], check=True,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
os.replace(os.path.join(frames_dir, "audio.wav"), os.path.join(work, "audio.wav"))

def font(size):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"):
        if os.path.exists(p): return ImageFont.truetype(p, size)
    return ImageFont.load_default()
F_TOP, F_BOT, F_BIG = font(24), font(30), font(54)

for i in range(N):
    img = Image.open(os.path.join(frames_dir, "frame_%04d.png" % i)).convert("RGBA")
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(ov)
    for (a, b, top, bot) in caps:
        if a <= i <= b:
            if a == 0:
                d.rounded_rectangle((140, 300, 1060, 470), 24, fill=(10, 14, 20, 228), outline=(232, 146, 58, 255), width=3)
                d.text((600, 358), top, font=F_BIG, fill=(255, 217, 163, 255), anchor="mm")
                d.text((600, 425), bot, font=font(26), fill=(255, 255, 255, 255), anchor="mm")
            else:
                wt = d.textlength(top, font=F_TOP) + 60
                d.rounded_rectangle((600 - wt / 2, 126, 600 + wt / 2, 168), 14, fill=(10, 14, 20, 225), outline=(232, 146, 58, 255), width=2)
                d.text((600, 147), top, font=F_TOP, fill=(255, 217, 163, 255), anchor="mm")
                wb = d.textlength(bot, font=F_BOT) + 70
                d.rounded_rectangle((600 - wb / 2, 688, 600 + wb / 2, 750), 18, fill=(10, 14, 20, 232), outline=(232, 146, 58, 255), width=2)
                d.text((600, 719), bot, font=F_BOT, fill=(255, 255, 255, 255), anchor="mm")
            break
    Image.alpha_composite(img, ov).convert("RGB").save(os.path.join(out_dir, "f_%04d.png" % i))

subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", "30", "-i", os.path.join(out_dir, "f_%04d.png"),
                "-i", os.path.join(work, "audio.wav"), "-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
                "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-shortest", out_mp4], check=True)
print("ok", out_mp4, N)
