#!/usr/bin/env python3
"""Generate bm_art_assets.h from the upgraded graphics in art/.

art/materials/CC.png  a seamless 16x16 texture for Atari colour $CC (hex):
                      the HD renderer draws every playfield pixel of that
                      colour from it, one texture per (2x scaled) tile.
Colours without a texture keep their flat Atari colour.

art/sprites/KIND_V.png / KIND.png
                      an object at twice the Atari size (KIND and variant V
                      as in BmSpriteKind; KIND.png serves every variant).
                      Alpha < 128 is transparent; pure grey pixels (R=G=B)
                      take the object's Atari colour, scaled by their grey.
                      A kind is drawn in HD only when every variant has an
                      image (or KIND.png exists); otherwise it stays classic.

PNGs are decoded here (8-bit greyscale, RGB, RGBA or palette, no interlace)
so no Python packages are needed.  Output is RGB565."""
import os
import struct
import sys
import zlib

TILE = 16


def read_png(path):
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'{path}: not a PNG file')
    pos, idat, palette = 8, b'', None
    transparency = {}
    width = height = depth = colour = interlace = None
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            width, height, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
        elif kind == b'PLTE':
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif kind == b'tRNS':
            transparency = {i: a for i, a in enumerate(chunk)}
        elif kind == b'IDAT':
            idat += chunk
        elif kind == b'IEND':
            break
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(colour)
    if depth != 8 or channels is None or interlace:
        raise ValueError(f'{path}: use 8-bit RGB, RGBA, grey or palette PNG without interlace')
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(height):
        kind, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0
            b = prev[x]
            c = prev[x - channels] if x >= channels else 0
            if kind == 1:
                line[x] = (line[x] + a) & 255
            elif kind == 2:
                line[x] = (line[x] + b) & 255
            elif kind == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 255
        rows.append(line)
        prev = line
    pixels = []
    for line in rows:
        for x in range(width):
            v = line[x * channels:(x + 1) * channels]
            if colour == 3:
                pixels.append(tuple(palette[v[0]]) + (transparency.get(v[0], 255),))
            elif colour in (0, 4):
                pixels.append((v[0], v[0], v[0], v[1] if colour == 4 else 255))
            else:
                pixels.append(tuple(v[:3]) + ((v[3],) if colour == 6 else (255,)))
    return width, height, pixels


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


# BmSpriteKind order (bm_renderer.h) and the number of variants each.
SPRITE_KINDS = [('player', 6), ('shadow', 2), ('crash', 2), ('shot', 1),
                ('bomb', 2), ('explosion', 2), ('ship', 2), ('vehicle', 8),
                ('enemy', 32), ('enemy_shadow', 2), ('enemy_shot', 1),
                ('gunsight', 1), ('gun_shot', 2), ('capital', 1)]
MAX_SPRITE = 64


def sprite_data(name, pixels):
    """RGB565 values and a mask per pixel: 0 transparent, 1 opaque, 2 tint
    (the value is then the grey level)."""
    values, mask = [], []
    for r, g, b, a in pixels:
        if a < 128:
            values.append(0); mask.append(0)
        elif r == g == b:
            values.append(r); mask.append(2)
        else:
            values.append(rgb565(r, g, b)); mask.append(1)
    return values, mask


def load_sprites(art_dir):
    folder = os.path.join(art_dir, 'sprites')
    images = {}
    if not os.path.isdir(folder):
        return images
    names = {k for k, _ in SPRITE_KINDS}
    for name in sorted(os.listdir(folder)):
        stem, ext = os.path.splitext(name)
        if ext.lower() != '.png':
            continue
        kind, variant = stem, None
        if stem not in names and '_' in stem:
            head, tail = stem.rsplit('_', 1)
            if head in names and tail.isdigit():
                kind, variant = head, int(tail)
        if kind not in names:
            raise SystemExit(f'{name}: unknown sprite (use KIND.png or KIND_V.png, '
                             f'KIND one of {", ".join(k for k, _ in SPRITE_KINDS)})')
        count = dict(SPRITE_KINDS)[kind]
        if variant is not None and variant >= count:
            raise SystemExit(f'{name}: {kind} has variants 0..{count - 1}')
        w, h, pixels = read_png(os.path.join(folder, name))
        if w > MAX_SPRITE or h > MAX_SPRITE:
            raise SystemExit(f'{name}: at most {MAX_SPRITE}x{MAX_SPRITE} pixels')
        images[(kind, variant)] = (w, h, pixels)
    return images


def sprite_source(art_dir):
    images = load_sprites(art_dir)
    out = ['typedef struct { uint16_t width, height; const uint16_t *pixels;'
           ' const uint8_t *mask; } BmArtSprite;',
           f'enum {{ BM_ART_SPRITE_KINDS = {len(SPRITE_KINDS)}, BM_ART_SPRITE_VARIANTS = 32 }};']
    symbol = {}
    for (kind, variant), (w, h, pixels) in images.items():
        name = f'bm_art_sprite_{kind}' + ('' if variant is None else f'_{variant}')
        values, mask = sprite_data(name, pixels)
        out.append(f'static const uint16_t {name}_px[{w * h}] = {{')
        for k in range(0, len(values), 16):
            out.append('  ' + ','.join(f'0x{v:04x}' for v in values[k:k + 16]) + ',')
        out.append('};')
        out.append(f'static const uint8_t {name}_mask[{w * h}] = {{')
        for k in range(0, len(mask), 32):
            out.append('  ' + ','.join(str(v) for v in mask[k:k + 32]) + ',')
        out.append('};')
        out.append(f'static const BmArtSprite {name} = {{ {w}, {h}, {name}_px, {name}_mask }};')
        symbol[(kind, variant)] = name
    complete = 0
    out.append('/* Per kind and variant: the image (KIND_V.png, else KIND.png) or 0. */')
    out.append('static const BmArtSprite *const bm_art_sprites[BM_ART_SPRITE_KINDS][BM_ART_SPRITE_VARIANTS] = {')
    for index, (kind, count) in enumerate(SPRITE_KINDS):
        row = []
        for v in range(32):
            name = symbol.get((kind, v)) or symbol.get((kind, None))
            row.append(f'&{name}' if name and v < count else '0')
        if (kind, None) in symbol or all((kind, v) in symbol for v in range(count)):
            complete |= 1 << index
        out.append('  { ' + ','.join(row) + ' },')
    out.append('};')
    out.append(f'/* Kinds drawn in HD: every variant has an image. */')
    out.append(f'enum {{ BM_ART_SPRITE_COMPLETE = 0x{complete:x}u }};')
    return out


def main(art_dir, target):
    materials = {}
    folder = os.path.join(art_dir, 'materials')
    if os.path.isdir(folder):
        for name in sorted(os.listdir(folder)):
            stem, ext = os.path.splitext(name)
            if ext.lower() != '.png':
                continue
            try:
                colour = int(stem, 16)
            except ValueError:
                raise SystemExit(f'{name}: name must be the Atari colour in hex, e.g. C4.png')
            if not 0 <= colour <= 255:
                raise SystemExit(f'{name}: Atari colour must be 00..FF')
            w, h, pixels = read_png(os.path.join(folder, name))
            if (w, h) != (TILE, TILE):
                raise SystemExit(f'{name}: must be {TILE}x{TILE} pixels, is {w}x{h}')
            materials[colour] = [rgb565(*p[:3]) for p in pixels]
    out = ['/* Generated by tools/art_assets.py from art/ - do not edit. */',
           '#ifndef BM_ART_ASSETS_H', '#define BM_ART_ASSETS_H', '#include <stdint.h>',
           f'enum {{ BM_ART_TILE = {TILE} }};']
    for colour, data in materials.items():
        out.append(f'static const uint16_t bm_art_material_{colour:02x}[{TILE * TILE}] = {{')
        for k in range(0, len(data), 16):
            out.append('  ' + ','.join(f'0x{v:04x}' for v in data[k:k + 16]) + ',')
        out.append('};')
    out.append('/* Per Atari colour: its 16x16 texture, or 0 (flat colour). */')
    out.append('static const uint16_t *const bm_art_materials[256] = {')
    for colour in range(256):
        out.append(f'  bm_art_material_{colour:02x},' if colour in materials else '  0,')
    out.append('};')
    out.extend(sprite_source(art_dir))
    out.append('#endif')
    os.makedirs(os.path.dirname(target), exist_ok=True)
    open(target, 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
