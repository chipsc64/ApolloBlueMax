#!/usr/bin/env python3
"""Write the example art: textures in art/materials/ (grass $C4, water $94,
16x16 and seamless, around the original Atari colours) and the player's
aircraft and the other objects in art/sprites/, derived from the Atari
shapes in the generated asset header at twice the size with a highlight,
a shade and an outline.  P2 parts (the object's colour) are pure grey so
that they take the Atari colour; P3 parts are dark; explosions fiery.  Replace them with real
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


def bits_to_rows(rows):
    """Eight-bit rows (one bit = one Atari colour clock = two pixels)."""
    return [[9 if row & (0x80 >> (c // 2)) else 0 for c in range(16)] for row in rows]


def shadow_rows(shape, variant):
    """SHDW, with SHCOL (far: rows 0/7 off, 1/6 $44/$42) or TURN banking
    (rows 0/7 $02/$40), as bm_game_shadow_pixels."""
    rows = []
    for r in range(8):
        bits = 0
        for c in range(8):
            if shape[r][c * 2]:
                bits |= 0x80 >> c
        rows.append(bits)
    if variant == 1:
        rows[0] = rows[7] = 0
        rows[1], rows[6] = 0x44, 0x42
    elif variant == 2:
        rows[0], rows[7] = 0x02, 0x40
    return bits_to_rows(rows)


def compose(layers, outline=True):
    """layers: (shape, dy, style).  Styles: 'tint' (pure grey, lit from the
    top), 'p3' (dark), 'fire', 'water', 'shadow'.  Later layers cover
    earlier ones, like the Atari's P2 over P3."""
    height = max(len(shape) + dy for shape, dy, _ in layers) * 2
    width = max(len(shape[0]) for shape, _, _ in layers) * 2
    style = [[None] * width for _ in range(height)]
    for shape, dy, kind in layers:
        for y in range(len(shape) * 2):
            for x in range(width):
                if shape[y // 2][x // 2]:
                    style[y + dy * 2][x] = kind
    def filled(x, y):
        return 0 <= x < width and 0 <= y < height and style[y][x] is not None
    out = []
    for y in range(height):
        for x in range(width):
            kind = style[y][x]
            if kind is None:
                near = any(filled(x + dx, y + dy) and style[y + dy][x + dx] != 'shadow'
                           for dx in (-1, 0, 1) for dy in (-1, 0, 1))
                out.append((40, 40, 48, 255) if outline and near else (0, 0, 0, 0))
                continue
            top, bottom = not filled(x, y - 1), not filled(x, y + 1)
            if kind == 'tint':
                g = 250 if top else (150 if bottom else 215)
                out.append((g, g, g, 255))
            elif kind == 'p3':
                out.append((70, 70, 82, 255) if top else (44, 44, 54, 255))
            elif kind == 'fire':
                t = y / max(1, height - 1)
                out.append((255, int(235 - 170 * t), int(110 - 90 * t), 255) if not bottom
                           else (170, 40, 20, 255))
            elif kind == 'water':
                out.append((235, 245, 255, 255) if top else (170, 205, 230, 255))
            else:
                out.append((22, 26, 22, 255))
    return out, width, height


def write_sprite(folder, name, image):
    pixels, w, h = image
    write_png(os.path.join(folder, name + '.png'), pixels, w, h)


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
    a = lambda name: atari_sprite(header, name)
    def split(shape, p2, p3):
        """A composite: P2 value(s) and P3 value(s) as two layers."""
        return ([[v if v in p2 else 0 for v in row] for row in shape],
                [[v if v in p3 else 0 for v in row] for row in shape])
    # The shadow: normal, far (SHCOL), banking (TURN).
    for v in range(3):
        write_sprite(sprites, f'shadow_{v}', compose([(shadow_rows(a('shadow'), v), 0, 'shadow')], False))
    # The enemy fighter: P2 (value 10, or 7 in the banking frames) takes its
    # colour, P3 (9) is dark.  Variant bits: 1 forward, 2/4 turning, 8 wreck,
    # 16 crash colour (same look).
    looks = {0: 'fighter_back', 1: 'fighter_forward', 2: 'bank_left_frame_1',
             4: 'bank_right_frame_1', 3: 'fighter_left_initial', 5: 'fighter_right_initial'}
    for look, name in looks.items():
        p2, p3 = split(a(name), (7, 10), (9,))
        image = compose([(p3, 0, 'p3'), (p2, 0, 'tint')])
        write_sprite(sprites, f'enemy_{look}', image)
        write_sprite(sprites, f'enemy_{look | 16}', image)
    write_sprite(sprites, 'enemy', compose([(split(a('fighter_back'), (10,), (9,))[1], 0, 'p3'),
                                           (split(a('fighter_back'), (10,), (9,))[0], 0, 'tint')]))
    write_sprite(sprites, 'enemy_8', compose([(a('explosion'), 0, 'fire')]))
    write_sprite(sprites, 'enemy_shadow_0', compose([(a('shadow'), 0, 'shadow')], False))
    write_sprite(sprites, 'enemy_shadow_1', compose([(a('fighter_forward_p3'), 0, 'shadow')], False))
    # The ship: SHPIM on P2, SHPIM1 on P3 two lines lower; the wreck XSHPIM.
    write_sprite(sprites, 'ship_0', compose([(a('ship_layer_1'), 2, 'p3'), (a('ship_layer_0'), 0, 'tint')]))
    write_sprite(sprites, 'ship_1', compose([(a('destroyed_ship'), 0, 'fire')]))
    # Vehicles: body on P2, tyres on P3 one line lower; a wreck is the crash
    # image one line higher, with the tyres two lines below its top.
    for v in range(8):
        kind = 'truck' if v & 1 else 'car'
        side = 'left' if v & 2 else 'right'
        tyres = a(f'{kind}_{side}_tyres')
        if v & 4:
            image = compose([(a('crash'), 0, 'fire'), (tyres, 2, 'p3')])
        else:
            image = compose([(tyres, 1, 'p3'), (a(f'{kind}_{side}_body'), 0, 'tint')])
        write_sprite(sprites, f'vehicle_{v}', image)
    # Explosions: a bomb on land and in water, and the crash halves.
    write_sprite(sprites, 'explosion_0', compose([(a('explosion'), 0, 'fire')]))
    write_sprite(sprites, 'explosion_1', compose([(a('explosion'), 0, 'water')]))
    for v in range(2):
        write_sprite(sprites, f'crash_{v}', compose([(a('explosion'), 0, 'fire')]))
    write_sprite(sprites, 'gunsight', compose([(a('gunsight'), 0, 'tint')]))
    # The capital target (world.c): P3 (dark) two lines above P2 (its
    # colour), 8 bits at SIZEP2 1, so one bit is 8 HD pixels; the wreck is
    # P2 alone and is stretched to twice the width while it pulses.
    world = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'core', 'world.c')).read()
    def capital(name):
        values = world.split(f'static const uint8_t {name}[')[1].split('{')[1].split('}')[0]
        return [[1 if v & (0x80 >> (x // 4)) else 0 for x in range(32)]
                for v in (int(t, 16) for t in values.replace('\n', ' ').split(',') if t.strip())]
    write_sprite(sprites, 'capital_0', compose([(capital('capital_p3'), 0, 'p3'),
                                                (capital('capital_p2'), 2, 'tint')]))
    write_sprite(sprites, 'capital_1', compose([(capital('capital_wreck_p2'), 0, 'tint')]))
    # Shots, the bomb and the shells are missiles, drawn as small blocks
    # (render.c): one character per HD pixel, at the classic block's top-left.
    for name, rows in SMALL.items():
        write_sprite(sprites, name, small(rows))


# The player's shot (4x1 Atari pixels), the enemy's shot (4x2, takes the
# enemy's colour), the bomb's two phases (wide row 1; narrow right half on
# row 0, then on row 2), the shell (4x2) and its burst (four 4-pixel rows,
# the odd ones two pixels further left).
SMALL = {
    'shot': ['YWWWWWWy',
             'yyyyyyyo'],
    'enemy_shot': ['.LLLLLL.',
                   'LTTTTTTL',
                   'TTTTTTTT',
                   '.DDDDDD.'],
    'bomb_0': ['....hhmm',
               '....mmdd',
               'hhhhhhmm',
               'mmmmmmdd',
               '........',
               '........'],
    'bomb_1': ['........',
               '........',
               'hhhhhhmm',
               'mmmmmmdd',
               '....hhmm',
               '....mmdd'],
    'gun_shot_0': ['.SSSSSS.',
                   'SSSSSSSs',
                   'sSSSSSSs',
                   '.ssssss.'],
    'gun_shot_1': ['....kKKKKKk.',
                   '...KKoKKKKKk',
                   '.kKKKKKk....',
                   'kKKoKKKKk...',
                   '....kKKoKKk.',
                   '...KKKKKKKk.',
                   '.kKKKKKk....',
                   '.kkkkkk.....'],
}
SMALL_COLOURS = {
    '.': (0, 0, 0, 0),
    'W': (255, 250, 210, 255), 'Y': (255, 255, 255, 255), 'y': (230, 190, 70, 255),
    'o': (255, 140, 40, 255),
    'L': (255, 255, 255, 255), 'T': (200, 200, 200, 255), 'D': (130, 130, 130, 255),
    'h': (122, 128, 112, 255), 'm': (78, 84, 70, 255), 'd': (44, 48, 40, 255),
    'S': (52, 50, 46, 255), 's': (24, 22, 20, 255),
    'K': (34, 32, 30, 255), 'k': (70, 66, 60, 255),
}


def small(rows):
    return [SMALL_COLOURS[c] for row in rows for c in row], len(rows[0]), len(rows)


if __name__ == '__main__':
    main(*(sys.argv[1:] or ['art']))
