#!/usr/bin/env python3
"""Write the example textures in art/materials/: grass ($C4) and water ($94),
16x16 and seamless, around the original Atari colours.  Replace them with
real artwork; this script only exists so the examples can be reproduced."""
import math
import os
import random
import struct
import sys
import zlib

TILE = 16


def write_png(path, pixels):
    raw = b''.join(b'\x00' + bytes(v for p in pixels[y * TILE:(y + 1) * TILE] for v in p)
                   for y in range(TILE))
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', TILE, TILE, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    open(path, 'wb').write(png)


def clamp(v):
    return max(0, min(255, int(round(v))))


def grass(rng):
    base = (29, 80, 0)                       # Atari $C4
    out = []
    for y in range(TILE):
        for x in range(TILE):
            n = rng.uniform(-14, 14)
            blade = 18 if (x * 7 + y * 3) % 11 == 0 else 0
            out.append((clamp(base[0] + n * 0.5), clamp(base[1] + n + blade), clamp(base[2] + n * 0.2)))
    return out


def water(rng):
    base = (3, 70, 83)                       # Atari $94
    out = []
    for y in range(TILE):
        for x in range(TILE):
            wave = math.sin(2 * math.pi * (x / TILE + 2 * y / TILE)) * 10
            wave += math.sin(2 * math.pi * (3 * x / TILE - y / TILE)) * 5
            glint = 30 if (x + 5 * y) % 16 == 0 else 0
            n = rng.uniform(-3, 3)
            out.append((clamp(base[0] + glint * 0.6), clamp(base[1] + wave + n + glint),
                        clamp(base[2] + wave + n + glint)))
    return out


def main(art_dir):
    folder = os.path.join(art_dir, 'materials')
    os.makedirs(folder, exist_ok=True)
    rng = random.Random(1983)
    write_png(os.path.join(folder, 'C4.png'), grass(rng))
    write_png(os.path.join(folder, '94.png'), water(rng))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'art')
