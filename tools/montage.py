"""Put PPM pictures side by side in one PNG (standard library only).

    python tools/montage.py out.png columns a.ppm b.ppm ...
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from ppm2png import read_ppm, write_png  # noqa: E402


def main():
    out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
    pics = [read_ppm(f) for f in files]
    w = max(p[0] for p in pics)
    h = max(p[1] for p in pics)
    rows = (len(pics) + cols - 1) // cols
    W, H = w * cols + (cols - 1) * 2, h * rows + (rows - 1) * 2
    img = bytearray([40, 40, 40] * (W * H))
    for i, (pw, ph, data) in enumerate(pics):
        ox, oy = (i % cols) * (w + 2), (i // cols) * (h + 2)
        for y in range(ph):
            row = data[y * pw * 3:(y + 1) * pw * 3]
            o = ((oy + y) * W + ox) * 3
            img[o:o + len(row)] = row
    write_png(out, W, H, bytes(img))


if __name__ == '__main__':
    main()
