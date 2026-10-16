#!/usr/bin/env python3
"""Write the example art: textures in art/materials/ (grass $C4, water $94,
16x16 and seamless, around the original Atari colours) and the player's
aircraft in art/sprites/ (player_0..5: level/left/right x propeller frame),
derived from the Atari shapes in the generated asset header at twice the
size with a highlight, a shade and an outline.  Replace them with real
artwork; this script only exists so the examples can be reproduced."""
import math
import os
import random
import struct
import sys
import zlib

TILE = 16


def write_png(path, pixels, width=TILE, height=TILE):
    rgba = len(pixels[0]) == 4
    raw = b''.join(b'\x00' + bytes(v for p in pixels[y * width:(y + 1) * width] for v in p)
                   for y in range(height))
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8,
                                                         6 if rgba else 2, 0, 0, 0))
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


def atari_sprite(header, name):
    """A 16-wide sprite from bm_chbas_assets.h: rows of 16 bytes, 0 = empty."""
    import re
    m = re.search(r'bm_asset_' + name + r'\[(\d+)\] = \{([^}]*)\}', header)
    values = [int(v) for v in m.group(2).replace('\n', '').split(',') if v.strip()]
    return [values[i:i + 16] for i in range(0, len(values), 16)]


def hd_plane(shape):
    """Twice the size: body light, lit from the top, shaded below, with a
    dark outline one HD pixel wide around it."""
    h, w = len(shape) * 2, 32
    inside = [[shape[y // 2][x // 2] != 0 for x in range(w)] for y in range(h)]
    def at(x, y):
        return 0 <= x < w and 0 <= y < h and inside[y][x]
    out = []
    for y in range(h):
        for x in range(w):
            if inside[y][x]:
                if not at(x, y - 1):
                    out.append((250, 250, 240, 255))
                elif not at(x, y + 1):
                    out.append((150, 150, 165, 255))
                else:
                    out.append((215, 215, 205, 255))
            elif any(at(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                out.append((40, 40, 48, 255))
            else:
                out.append((0, 0, 0, 0))
    return out, w, h


def main(art_dir, header_path='build/assets/chbas/bm_chbas_assets.h'):
    folder = os.path.join(art_dir, 'materials')
    os.makedirs(folder, exist_ok=True)
    rng = random.Random(1983)
    write_png(os.path.join(folder, 'C4.png'), grass(rng))
    write_png(os.path.join(folder, '94.png'), water(rng))
    if not os.path.exists(header_path):
        return
    header = open(header_path).read()
    sprites = os.path.join(art_dir, 'sprites')
    os.makedirs(sprites, exist_ok=True)
    # Variant = bank (0 level, 2 left, 4 right) + propeller frame (0 = frame 0).
    for bank, base in ((0, 'plane'), (2, 'bank_left'), (4, 'bank_right')):
        for frame in (0, 1):
            pixels, w, h = hd_plane(atari_sprite(header, f'{base}_frame_{frame}'))
            write_png(os.path.join(sprites, f'player_{bank + frame}.png'), pixels, w, h)


if __name__ == '__main__':
    main(*(sys.argv[1:] or ['art']))
