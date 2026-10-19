"""Sheet of Defender's pictures (the sprites as the original draws them).

    python tools/pictures.py out.png [scale]

Reads the picture descriptors from the assembled listing (a label, FCB
width,height, FDB even,odd,write,erase) and draws each one's even-phase
picture from the images, magnified, colour index by colour index, with its
name and size above it. The colours are a fixed debug palette: index 0
black, the others distinct, so a design can follow the original's colour
indices (which the game maps through its colour registers).
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'translate'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import asm2cpp  # noqa: E402
from ppm2png import write_png  # noqa: E402

PAL = [(0, 0, 0), (255, 255, 255), (255, 60, 60), (60, 255, 60), (80, 120, 255), (255, 255, 0), (0, 255, 255),
       (255, 0, 255), (255, 140, 0), (160, 160, 160), (140, 255, 140), (255, 160, 200), (120, 80, 255),
       (200, 120, 60), (0, 160, 120), (255, 220, 160)]

# 5x7 digits/letters for the labels
FONT = {}
_G = """0:7,5,5,5,7 1:2,6,2,2,7 2:7,1,7,4,7 3:7,1,7,1,7 4:5,5,7,1,1 5:7,4,7,1,7 6:7,4,7,5,7 7:7,1,1,1,1
8:7,5,7,5,7 9:7,5,7,1,7 A:2,5,7,5,5 B:6,5,6,5,6 C:7,4,4,4,7 D:6,5,5,5,6 E:7,4,6,4,7 F:7,4,6,4,4
G:7,4,5,5,7 H:5,5,7,5,5 I:7,2,2,2,7 J:1,1,1,5,7 K:5,5,6,5,5 L:4,4,4,4,7 M:5,7,7,5,5 N:6,5,5,5,5
O:7,5,5,5,7 P:7,5,7,4,4 Q:7,5,5,7,1 R:6,5,6,5,5 S:7,4,7,1,7 T:7,2,2,2,2 U:5,5,5,5,7 V:5,5,5,5,2
W:5,5,7,7,5 X:5,5,2,5,5 Y:5,5,2,2,2 Z:7,1,2,4,7 x:0,5,2,5,0"""
for tok in _G.split():
    k, v = tok.split(':')
    FONT[k] = [int(r) for r in v.split(',')]


def pictures():
    """[(name, region, addr, w, h, data_addr)] from the main listing."""
    out = []
    path = os.path.join(asm2cpp.REF, 'bin', 'defa7-defb6-amode1.lst')
    lines = open(path, encoding='latin-1').read().splitlines()
    for i, raw in enumerate(lines):
        m = asm2cpp.LINE.match(raw)
        if not m:
            continue
        src = asm2cpp.split_line(raw)[1]
        mm = re.match(r'^([A-Z0-9]+)\s+FCB\s+(\d+),(\d+)\s*$', src.split(';')[0].rstrip())
        if not mm or i + 1 >= len(lines):
            continue
        nx = lines[i + 1]
        hexs = nx[6:22].strip()
        if len(hexs) < 4:
            continue
        w, h = int(mm.group(2)), int(mm.group(3))
        p0 = int(hexs[0:4], 16)
        out.append((mm.group(1), int(m.group(1), 16), w, h, p0))
    return out


def read(addr, bank=2):
    if addr >= 0xD000:
        return asm2cpp.MAIN[addr - 0xD000]
    if addr >= 0xC000:
        return asm2cpp.BANKS[bank][addr - 0xC000]
    return 0


def pixels(w, h, p0):
    """Columns of h bytes, two pixels a byte (high nibble left)."""
    px = [[0] * (w * 2) for _ in range(h)]
    for cx in range(w):
        for y in range(h):
            v = read(p0 + cx * h + y)
            px[y][cx * 2] = v >> 4
            px[y][cx * 2 + 1] = v & 15
    return px


def main():
    out = sys.argv[1]
    s = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    pics = [p for p in pictures() if p[2] * p[3] > 0 and p[2] <= 16 and p[3] <= 32]
    cell_w = 16 * s + 8
    cols = 6
    tiles = []
    for name, addr, w, h, p0 in pics:
        tiles.append((name + ' %dx%d' % (w * 2, h), pixels(w, h, p0)))
    rows = (len(tiles) + cols - 1) // cols
    cell_h = 12 + 16 * s + 8
    W, H = cols * cell_w, rows * cell_h
    img = bytearray(W * H * 3)

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 3
            img[o:o + 3] = bytes(c)

    for i, (label, px) in enumerate(tiles):
        ox, oy = (i % cols) * cell_w, (i // cols) * cell_h
        tx = ox
        for ch in label:
            g = FONT.get(ch)
            if g:
                for r, bits in enumerate(g):
                    for b in range(3):
                        if bits & (4 >> b):
                            put(tx + b, oy + 2 + r, (200, 200, 200))
            tx += 4
        for y, row in enumerate(px):
            for x, v in enumerate(row):
                for dy in range(s - 1):
                    for dx in range(s - 1):
                        put(ox + x * s + dx, oy + 12 + y * s + dy, PAL[v] if v else (25, 25, 25))
    write_png(out, W, H, bytes(img))
    for name, addr, w, h, p0 in pics:
        print('%-8s %04X %2dx%-2d data %04X' % (name, addr, w * 2, h, p0))


if __name__ == '__main__':
    main()
