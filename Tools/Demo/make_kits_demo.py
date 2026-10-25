#!/usr/bin/env python3
"""Video that plays every piece of the three built-in kits (cymbals are shared) and shows the piece swap.
  python3 make_kits_demo.py <snapshot_exe> <work_dir> <out.mp4>"""
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

sb, caps = [], []        # storyboard lines, (start, end, top, bottom)
f = 0
def ev(frame, text): sb.append(f"{frame}|{text}")

ev(0, "page|PERFORMANCE")
caps.append((0, 70, "AFRICAN DRUM", "3 kits  |  pratos iguais em todos  |  troca de peças"))
f = 70
for ki, kit in enumerate(KITS):
    ev(f, f"kit|{kit}")
    start = f
    f += 12
    for p in DRUMS:
        ev(f, f"select|{p}"); ev(f + 1, f"trigger|{p}")
        caps.append((f, f + 13, f"KIT {ki + 1}/3  -  {kit}", NAMES[p]))
        f += 14
    f += 8
    cyms = CYMS_ALL if ki == 0 else CYMS_SHORT
    for p in cyms:
        ev(f, f"select|{p}"); ev(f + 1, f"trigger|{p}")
        caps.append((f, f + 23, f"KIT {ki + 1}/3  -  {kit}" + ("   (pratos compartilhados)" if ki else ""), NAMES[p]))
        f += 24
    f += 10
    ev(f, "startgroove")
    caps.append((f, f + 150, f"KIT {ki + 1}/3  -  {kit}", "Groove tocando com este kit"))
    f += 150
    ev(f, "stopgroove")
    f += 30

# swap demo on the last kit
ev(f, "select|0")
caps.append((f, f + 40, "TROCAR PEÇA  -  Seben Kit", "Kick do Seben Kit"))
ev(f + 2, "trigger|0"); ev(f + 20, "trigger|0")
f += 44
ev(f, "swap|0|k1_01_Kick"); ev(f + 1, "select|0")
caps.append((f, f + 44, "TROCAR PEÇA  -  Seben Kit", "Kick trocado pelo Kick do Standard Kit"))
ev(f + 6, "trigger|0"); ev(f + 24, "trigger|0")
f += 48
ev(f, "select|5"); ev(f + 2, "trigger|5")
caps.append((f, f + 38, "TROCAR PEÇA  -  Seben Kit", "Tom Floor do Seben Kit"))
f += 42
ev(f, "swap|5|k2_06_Tom_Floor"); ev(f + 1, "select|5")
caps.append((f, f + 50, "TROCAR PEÇA  -  Seben Kit", "Tom Floor trocado pelo do Soukous Fusion"))
ev(f + 6, "trigger|5"); ev(f + 26, "trigger|5")
f += 54
ev(f, "startgroove")
caps.append((f, f + 150, "KIT MISTURADO", "Groove com as peças trocadas"))
f += 150
ev(f, "stopgroove")
f += 20
ev(f, "page|MIXER")
caps.append((f, f + 100, "MIXER", "21 canais  -  toms High > Floor  -  mute/solo por grupo"))
f += 100
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
