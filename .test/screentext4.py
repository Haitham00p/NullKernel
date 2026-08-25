#!/usr/bin/env python3
import re, sys

src = open("/home/null/Kernel/drivers/video_font/font.c").read()
glyphs = []
for m in re.finditer(r"\{\s*([0-9A-Fa-fx,\s]+)\s*\}", src):
    vals = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]+)", m.group(1))]
    if len(vals) == 16:
        glyphs.append(vals)
fg = []
for g in glyphs:
    bits = 0
    for gy in range(16):
        for gx in range(8):
            bits = (bits << 1) | ((g[gy] >> (7 - gx)) & 1)
    fg.append(bits)

path = sys.argv[1]
data = open(path, "rb").read()
parts = data.split(b"\n", 3)
W, H = map(int, parts[1].split())
px = parts[3]

def ink(x, y):
    if x < 0 or x >= W or y < 0 or y >= H:
        return False
    o = (y * W + x) * 3
    return max(px[o], px[o + 1], px[o + 2]) > 120

def cellbits(cx, cy, sx, sy):
    bits = 0
    for gy in range(16):
        for gx in range(8):
            bits = (bits << 1) | (1 if ink(cx * 8 + gx + sx, cy * 16 + gy + sy) else 0)
    return bits

def best(bits):
    bs, bestc = -1, 32
    for gi, b in enumerate(fg):
        m = 128 - (bits ^ b).bit_count()
        if m > bs:
            bs, bestc = m, gi
    return chr(bestc), bs

def best_shifted(cx, cy):
    out = []
    for sy in range(4):
        for sx in range(4):
            ch, sc = best(cellbits(cx, cy, sx, sy))
            out.append((sc, sx, sy, ch))
    out.sort(reverse=True)
    return out[0]

for cy in range(50):
    line = ""
    for cx in range(60):
        sc, sx, sy, ch = best_shifted(cx, cy)
        if sc >= 110:
            line += ch
        elif sc >= 95:
            line += "?"
        else:
            line += " "
    print(f"row{cy}: {line}")