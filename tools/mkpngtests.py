#!/usr/bin/env python3
"""Writes a C header with PNG files made by an independent encoder (this script + Python's zlib), together with the
expected RGBA8 pixel CRC32 of each, so the guest's libpng can be checked bit for bit. usage: mkpngtests.py out.h"""
import random, struct, sys, zlib

def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

def paeth(a, b, c):
    p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)

def filt(ft, row, prev, bpp):
    out = bytearray()
    for i, x in enumerate(row):
        a = row[i - bpp] if i >= bpp else 0; b = prev[i]; c = prev[i - bpp] if i >= bpp else 0
        pred = (0, a, b, (a + b) // 2, paeth(a, b, c))[ft]
        out.append((x - pred) & 255)
    return bytes([ft]) + bytes(out)

def pack(samples, bd):                       # samples: list of ints for one row, packed MSB first
    if bd == 8: return bytes(samples)
    if bd == 16: return b"".join(struct.pack(">H", v) for v in samples)
    per = 8 // bd; samples = list(samples) + [0] * (-len(samples) % per); out = bytearray()
    for i in range(0, len(samples), per):
        out.append(sum(samples[i + k] << (8 - bd * (k + 1)) for k in range(per)))
    return bytes(out)

def encode(w, h, ct, bd, px, plte=None, trns=None, adam7=False):   # px[y][x] = tuple of samples
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]; bpp = max(1, ch * bd // 8)
    passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)] if adam7 else [(0, 0, 1, 1)]
    raw = bytearray(); fi = 0
    for (x0, y0, dx, dy) in passes:
        xs = list(range(x0, w, dx)); ys = list(range(y0, h, dy))
        if not xs or not ys: continue
        prev = bytes(len(pack([0] * (len(xs) * ch), bd)))
        for y in ys:
            row = pack([s for x in xs for s in px[y][x]], bd)
            raw += filt(fi % 5, row, prev, bpp); prev = row; fi += 1
    ihdr = struct.pack(">IIBBBBB", w, h, bd, ct, 0, 0, 1 if adam7 else 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
    if plte: data += chunk(b"PLTE", bytes(c for e in plte for c in e))
    if trns: data += chunk(b"tRNS", bytes(trns))
    return data + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")

def rgba_crc(rows): return zlib.crc32(bytes(c for r in rows for p in r for c in p)) & 0xFFFFFFFF

R = random.Random(2026)
tests = []
# 1) RGBA 8-bit, odd size, all five filter types
w, h = 61, 37
px = [[tuple(R.randrange(256) if k == 3 else (x * 4 + y * 3 + k * 17) & 255 for k in range(4)) for x in range(w)] for y in range(h)]
tests.append(("rgba8_all_filters", w, h, encode(w, h, 6, 8, px), rgba_crc(px)))
# 2) 4-bit palette with transparency
w, h = 33, 20
pal = [(R.randrange(256), R.randrange(256), R.randrange(256)) for _ in range(16)]; tr = [255 - 16 * i for i in range(10)]
idx = [[(R.randrange(16),) for _ in range(w)] for _ in range(h)]
exp = [[pal[p[0]] + ((tr[p[0]] if p[0] < len(tr) else 255),) for p in r] for r in idx]
tests.append(("palette4_trns", w, h, encode(w, h, 3, 4, idx, pal, tr), rgba_crc(exp)))
# 3) 16-bit gray -> strip to high byte, expand to RGB
w, h = 17, 9
g = [[(R.randrange(65536),) for _ in range(w)] for _ in range(h)]
exp = [[(v[0] >> 8,) * 3 + (255,) for v in r] for r in g]
tests.append(("gray16", w, h, encode(w, h, 0, 16, g), rgba_crc(exp)))
# 4) Adam7 interlaced RGB
w, h = 24, 21
rgb = [[tuple((x * 11 + y * 5 + k * 40) & 255 for k in range(3)) for x in range(w)] for y in range(h)]
exp = [[p + (255,) for p in r] for r in rgb]
tests.append(("rgb8_adam7", w, h, encode(w, h, 2, 8, rgb, adam7=True), rgba_crc(exp)))

with open(sys.argv[1], "w") as f:
    for n, w, h, d, c in tests:
        f.write("static const unsigned char png_%s[%d] = {%s};\n" % (n, len(d), ",".join(map(str, d))))
    f.write("static const struct { const char *name; const unsigned char *data; unsigned long len; unsigned w, h; unsigned long crc; } png_cases[] = {\n")
    for n, w, h, d, c in tests: f.write('  {"%s", png_%s, %d, %d, %d, 0x%08xUL},\n' % (n, n, len(d), w, h, c))
    f.write("};\n#define PNG_NCASES %d\n" % len(tests))
