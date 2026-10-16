#!/usr/bin/env python3
"""Extract CHBAS .HS data and create labelled SVG contact sheets.

Only the Python standard library is required.  Both likely Atari pixel layouts
are emitted because CHBAS mixes one-bit player/missile images and two-bit
character graphics.
"""

import argparse
import html
import json
import math
import re
from pathlib import Path


TF_RE = re.compile(r'\.TF\s+"[^"]*?(MAX\.\d+)"', re.IGNORECASE)
HS_RE = re.compile(r'^\s*(\d+)\s+(?:(\w+)\s+)?\.HS\s+([0-9A-F]+)\s*$', re.IGNORECASE)
PALETTES = {
    1: ("#101c38", "#f0f0e8"),
    2: ("#101c38", "#346cb0", "#d8d0a0", "#f0f0e8"),
}
ANTIC4_INDICES = (0, 3, 4, 5, 6)
ANTIC4_INITIAL_BYTES = (0x00, 0xc4, 0x94, 0xd2, 0x04)


def atari_pal_palette():
    """Return Atari800's standard generated PAL palette as 0xRRGGBB values.

    Parameters and measured PAL GTIA delay coefficients follow Atari800 5.2's
    colours_pal.c standard preset: 23.2 degree delay, gamma 2.35, levels 16–235.
    """
    even = ((1, 5), (1, 6), (1, 7), (0, 0), (0, 1), (0, 2),
            (0, 4), (0, 5), (0, 6), (0, 7), (1, 1), (1, 2),
            (1, 3), (1, 4), (1, 5))
    odd = ((1, 1), (1, 0), (0, 7), (0, 6), (0, 5), (0, 4),
           (0, 2), (0, 1), (0, 0), (1, 7), (1, 5), (1, 4),
           (1, 3), (1, 2), (1, 1))
    luma = (0.6941, 0.7091, 0.7241, 0.7401, 0.7560, 0.7741,
            0.7931, 0.8121, 0.8260, 0.8470, 0.8700, 0.8930,
            0.9160, 0.9420, 0.9690, 1.0000)
    base_delay, add_delay, adjustment = 0.421894970414201, 0.446563064859117, 23.2 / 360.0
    delay = lambda item: base_delay + add_delay * item[0] + adjustment * item[1]
    even_burst, odd_burst = delay(even[0]), delay(odd[0])
    subcarrier = (even_burst + odd_burst) / 2.0
    burst_difference = (even_burst - odd_burst) % 1.0
    saturation_scale = math.sqrt(2.0) / math.sqrt(
        2.0 * math.cos(burst_difference * 2.0 * math.pi) + 2.0)

    def srgb(value):
        linear = value ** 2.35 if value >= 0.0 else value / 12.92
        encoded = linear * 12.92 if linear <= 0.0031308 else 1.055 * linear ** (1.0 / 2.4) - 0.055
        return max(0, min(255, int(encoded * 255)))

    result = []
    for hue in range(16):
        u = v = 0.0
        if hue:
            even_angle = (0.5 - (delay(even[hue - 1]) - subcarrier)) * 2.0 * math.pi
            odd_angle = (0.5 + (delay(odd[hue - 1]) - subcarrier)) * 2.0 * math.pi
            saturation = 0.175 * saturation_scale
            u = (math.cos(even_angle) + math.cos(odd_angle)) * saturation / 2.0
            v = (math.sin(even_angle) + math.sin(odd_angle)) * saturation / 2.0
        for level in range(16):
            y = (luma[level] - luma[0]) / (luma[15] - luma[0])
            y = y * ((235 - 16) / 255.0) + 16 / 255.0
            r = srgb(y + 1.13983 * v)
            g = srgb(y - 0.39465 * u - 0.58060 * v)
            b = srgb(y + 2.03211 * u)
            result.append((r << 16) | (g << 8) | b)
    return result


def write_pal_sheet(path, palette):
    scale, margin = 28, 12
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{16 * scale + 2 * margin}" height="{16 * scale + 38}">',
           '<rect width="100%" height="100%" fill="#20242b"/>',
           '<text x="12" y="22" fill="white" font-family="monospace" font-size="14">Atari PAL standard reference — hue x luminance</text>']
    for hue in range(16):
        for level in range(16):
            value = palette[hue * 16 + level]
            out.append(f'<rect x="{margin + level * scale}" y="{30 + hue * scale}" width="{scale}" height="{scale}" fill="#{value:06x}"/>')
    out.append("</svg>\n")
    path.write_text("\n".join(out), encoding="utf-8")


def parse_chbas(path):
    segments = {}
    labels = {}
    current = None
    for line_number, line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        match = TF_RE.search(line)
        if match:
            current = match.group(1).upper()
            segments.setdefault(current, bytearray())
            labels.setdefault(current, [])
            continue
        match = HS_RE.match(line)
        if not match or current is None:
            continue
        source_number, label, hex_data = match.groups()
        if len(hex_data) % 2:
            raise ValueError(f"{path}:{line_number}: odd number of hex digits")
        if label:
            labels[current].append({
                "name": label.upper(),
                "offset": len(segments[current]),
                "source_number": int(source_number),
            })
        segments[current].extend(bytes.fromhex(hex_data))

    for name, entries in labels.items():
        end = len(segments[name])
        for index, entry in enumerate(entries):
            next_offset = entries[index + 1]["offset"] if index + 1 < len(entries) else end
            entry["length"] = next_offset - entry["offset"]
    return {name: bytes(data) for name, data in segments.items()}, labels


def decode_atari_binary(path):
    data = path.read_bytes()
    if len(data) < 6 or data[:2] != b"\xff\xff":
        raise ValueError(f"{path}: not a single-segment Atari load file")
    start = int.from_bytes(data[2:4], "little")
    end = int.from_bytes(data[4:6], "little")
    expected = end - start + 1
    return start, end, data[6:6 + expected]


def pixels(rows, depth):
    mask = (1 << depth) - 1
    count = 8 // depth
    return [[(byte >> ((count - 1 - x) * depth)) & mask for x in range(count)] for byte in rows]


def write_sheet(path, title, items, depth, columns=5):
    cell_w, cell_h, scale = 180, 180, 6
    rows = (len(items) + columns - 1) // columns
    width, height = columns * cell_w, 42 + rows * cell_h
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#20242b"/>',
        f'<text x="12" y="26" fill="white" font-family="monospace" font-size="18">{html.escape(title)}</text>',
    ]
    palette = PALETTES[depth]
    for index, item in enumerate(items):
        ox = (index % columns) * cell_w + 10
        oy = 42 + (index // columns) * cell_h
        name, offset, data = item
        out.append(f'<text x="{ox}" y="{oy + 16}" fill="white" font-family="monospace" font-size="12">{html.escape(name)}</text>')
        out.append(f'<text x="{ox}" y="{oy + 31}" fill="#aab2bd" font-family="monospace" font-size="10">offset ${offset:04X}, {len(data)} bytes</text>')
        image = pixels(data, depth)
        draw_scale = min(scale, max(1, 128 // max(1, len(image))))
        top = oy + 40
        out.append(f'<rect x="{ox}" y="{top}" width="{len(image[0]) * scale}" height="{len(image) * draw_scale}" fill="#101c38" stroke="#626b77"/>')
        for y, row in enumerate(image):
            for x, value in enumerate(row):
                if value:
                    out.append(f'<rect x="{ox + x * scale}" y="{top + y * draw_scale}" width="{scale}" height="{draw_scale}" fill="{palette[value]}"/>')
    out.append("</svg>\n")
    path.write_text("\n".join(out), encoding="utf-8")


def chunky_mask(data, colour):
    # A normal-width GTIA player pixel is one colour clock: two pixels in the
    # 320-wide SAGA framebuffer.
    return [value for byte in data for x in range(8)
            for value in ((colour if byte & (0x80 >> x) else 0),) * 2]


def make_ship(max2):
    ship = [0] * (16 * 14)
    for layer, colour, y_offset in ((max2[252:264], 11, 2), (max2[240:252], 10, 0)):
        layer_pixels = chunky_mask(layer, colour)
        for y in range(12):
            for x in range(16):
                if layer_pixels[y * 16 + x]:
                    ship[(y + y_offset) * 16 + x] = layer_pixels[y * 16 + x]
    return ship


def make_fighter(max2, left):
    """Reconstruct the initial FTURN P2/P3 frame at spawn height."""
    p2 = max2[340:351] if left else max2[330:340]
    height = 11 if left else 10
    image = [0] * (16 * height)
    shadow = bytearray(max2[88:98])
    if left:
        shadow[5] = 0x40
        shadow_y = 1
    else:
        shadow[4] >>= 1
        shadow[5] = 0x20
        shadow[6] >>= 1
        shadow_y = 0
    for y, byte in enumerate(shadow):
        for x in range(8):
            if byte & (0x80 >> x):
                row = y + shadow_y
                if row < height:
                    image[row * 16 + x * 2:row * 16 + x * 2 + 2] = (9, 9)
    for y, byte in enumerate(p2):
        for x in range(8):
            if byte & (0x80 >> x):
                image[y * 16 + x * 2:y * 16 + x * 2 + 2] = (10, 10)
    return image


def make_forward_fighter(max2):
    """Reconstruct the straight PLFWD P2/P3 frame installed by LN."""
    p2 = max2[98:108]       # PLANE1
    p3 = bytearray(max2[88:98])  # SHDW1, used as the second player layer
    p3[4:7] = bytes((0x10, 0x40, 0x60))
    image = [0] * (16 * 10)
    for layer, colour in ((p3, 9), (p2, 10)):
        pixels = chunky_mask(layer, colour)
        for index, value in enumerate(pixels):
            if value:
                image[index] = value
    return image


def make_forward_p3(max2):
    """PLFWD/KD: only the SHDW1 P3 layer is visible until LN adds PLANE1."""
    return chunky_mask(max2[88:98], 9)


def make_back_fighter(max2):
    """Reconstruct the initial PLBCK P2/P3 body installed by OG."""
    p2 = max2[0:10]  # P0IMAGE
    p3 = bytearray(10)
    p3[2:5] = bytes((0x0c, 0x04, 0x10))
    image = [0] * (16 * 10)
    for layer, colour in ((p3, 9), (p2, 10)):
        pixels = chunky_mask(layer, colour)
        for index, value in enumerate(pixels):
            if value:
                image[index] = value
    return image


def antic4_strip(charset, screen_codes):
    """Decode ANTIC mode 4 screen codes to 320-wide chunky pixels."""
    width = len(screen_codes) * 8
    image = [0] * (width * 8)
    for character, screen_code in enumerate(screen_codes):
        glyph = screen_code & 0x7f
        pair3 = ANTIC4_INDICES[4 if screen_code & 0x80 else 3]
        mapping = (ANTIC4_INDICES[0], ANTIC4_INDICES[1],
                   ANTIC4_INDICES[2], pair3)
        for y, byte in enumerate(charset[glyph * 8:glyph * 8 + 8]):
            for logical_x, shift in enumerate((6, 4, 2, 0)):
                colour = mapping[(byte >> shift) & 3]
                x = character * 8 + logical_x * 2
                image[y * width + x:y * width + x + 2] = (colour, colour)
    return image


def write_antic4_strip(path, title, screen_codes, image):
    scale = 6
    image_width = len(screen_codes) * 8
    width, height = image_width * scale + 24, 112
    codes = " ".join(f"{value:02X}" for value in screen_codes)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
           '<rect width="100%" height="100%" fill="#20242b"/>',
           f'<text x="12" y="22" fill="white" font-family="monospace" font-size="14">{html.escape(title)}</text>',
           f'<text x="12" y="40" fill="#aab2bd" font-family="monospace" font-size="11">screen codes: {codes}</text>']
    pal = atari_pal_palette()
    colour_lookup = {value: f"#{pal[ANTIC4_INITIAL_BYTES[index]]:06x}"
                     for index, value in enumerate(ANTIC4_INDICES)}
    for y in range(8):
        for x in range(image_width):
            colour = colour_lookup[image[y * image_width + x]]
            out.append(f'<rect x="{12 + x * scale}" y="{52 + y * scale}" width="{scale}" height="{scale}" fill="{colour}"/>')
    out.append("</svg>\n")
    path.write_text("\n".join(out), encoding="utf-8")


def write_chunky_sheet(path, title, items):
    colours = {7: "#f0f0e8", 8: "#e05038", 9: "#202018",
               10: "#d8d0a0", 11: "#5088c8"}
    cell_w, cell_h, scale = 240, 180, 8
    width, height = cell_w * len(items), cell_h
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
           '<rect width="100%" height="100%" fill="#20242b"/>',
           f'<text x="12" y="24" fill="white" font-family="monospace" font-size="16">{html.escape(title)}</text>']
    for item_index, (name, item_width, item_height, data) in enumerate(items):
        ox=item_index*cell_w+20;oy=48
        out.append(f'<text x="{ox}" y="{oy}" fill="white" font-family="monospace" font-size="12">{html.escape(name)} ({item_width}x{item_height})</text>')
        for y in range(item_height):
            for x in range(item_width):
                value=data[y*item_width+x]
                if value:
                    out.append(f'<rect x="{ox+x*scale}" y="{oy+14+y*scale}" width="{scale}" height="{scale}" fill="{colours.get(value, "#ffffff")}"/>')
    out.append("</svg>\n")
    path.write_text("\n".join(out),encoding="utf-8")


def write_c_header(path, max2, max3):
    # EXXON.0 defines P1IMAGE=P0IMAGE+$10. EXXON.1/.2/.5 establish
    # these subranges and row counts.
    assets = (
        ("bm_asset_plane", max2[0:10], 7),
        ("bm_asset_propeller_0", max2[10:13], 7),
        ("bm_asset_propeller_1", max2[13:16], 7),
        ("bm_asset_plane_frame_0", max2[10:13] + max2[3:10], 7),
        ("bm_asset_plane_frame_1", max2[13:16] + max2[3:10], 7),
        ("bm_asset_shadow", max2[16:24], 9),
        ("bm_asset_bank_0", max2[24:34], 7),
        ("bm_asset_bank_1", max2[34:44], 7),
        # EXXON.1 BP/BQ/BR/BS replace the first three player rows while
        # banking; the P1IMAGE ranges above only contain the base shapes.
        ("bm_asset_bank_left_frame_0", bytes((0x00,0x08,0x02)) + max2[27:34], 7),
        ("bm_asset_bank_left_frame_1", bytes((0x04,0x04,0x00)) + max2[27:34], 7),
        ("bm_asset_bank_right_frame_0", bytes((0xc0,0x48,0x62)) + max2[37:44], 7),
        ("bm_asset_bank_right_frame_1", bytes((0xc4,0x44,0x60)) + max2[37:44], 7),
        ("bm_asset_car_right_body", max2[44:49], 10),
        ("bm_asset_car_left_body", max2[49:54], 10),
        ("bm_asset_car_right_tyres", max2[54:59], 11),
        ("bm_asset_car_left_tyres", max2[59:64], 11),
        ("bm_asset_truck_right_body", max2[64:70], 10),
        ("bm_asset_truck_right_tyres", max2[70:76], 11),
        ("bm_asset_truck_left_body", max2[76:82], 10),
        ("bm_asset_truck_left_tyres", max2[82:88], 11),
        ("bm_asset_explosion", max2[183:196], 8),
        ("bm_asset_ship_layer_0", max2[240:252], 10),
        ("bm_asset_ship_layer_1", max2[252:264], 11),
        ("bm_asset_landing_plane", max2[98:108], 7),
        ("bm_asset_fighter_right", max2[330:340], 10),
        ("bm_asset_fighter_left", max2[340:351], 10),
        ("bm_asset_gunsight", max2[351:360], 8),
        ("bm_asset_crash", max2[360:370], 8),
        ("bm_asset_destroyed_ship", max2[370:383], 10),
    )
    # DFBLDG restores ten runtime charset glyphs before a building starts.
    # Most happen to match the disk image, but ABZ (tile 90) and ACF (tile 93)
    # do not; keeping the complete copy map here documents the self-modifying
    # Atari charset rather than special-casing only those two differences.
    building_charset = bytearray(max3)
    for target, source in (
            (84, max3[8:16]), (86, max3[16:24]),
            (88, max3[776:784]), (90, max2[507:515]),
            (92, max2[515:523]), (94, max2[499:507]),
            (85, max2[279:287]), (89, max2[523:531]),
            (87, max2[531:539]), (93, max2[491:499])):
        building_charset[target * 8:target * 8 + 8] = source
    # AEG is a 96-byte save buffer immediately after MAX.2.  RESET/X0 copies
    # ADS..AEC (runtime charset tiles 21..32) into it before play starts;
    # DFHNGR later restores those saved bytes.  Its source is consequently the
    # original MAX.3 range 21*8..33*8, not either end of the MAX.3 file.
    # DFHNGR runs on the live charset after DFBLDG; preserve the ten building
    # glyph replacements and overlay only ADS..AEC plus AED.
    hangar_charset = bytearray(building_charset)
    hangar_charset[21 * 8:33 * 8] = max3[21 * 8:33 * 8]
    hangar_charset[126 * 8:127 * 8] = max2[279:287]
    # DFTNK restores seven runtime glyphs from ABL/ABM/ABN/ABO/ABP/ABR/ABT.
    # Build it on top of the live DFHNGR+DFBLDG charset.  These routines copy
    # disjoint glyph ranges into one mutable Atari charset; DFTNK therefore
    # must not discard either the buffered hangar or building glyphs.
    tank_charset = bytearray(hangar_charset)
    for target, source in (
            (69, max2[491:499]), (79, max3[1000:1008]),
            (73, max2[499:507]), (74, max2[507:515]),
            (18, max2[515:523]), (63, max2[279:287]),
            (127, max2[523:531])):
        tank_charset[target * 8:target * 8 + 8] = source
    out = [
        "/* Generated from CHBAS by tools/extract_chbas.py; do not edit. */",
        "#ifndef BM_CHBAS_ASSETS_H", "#define BM_CHBAS_ASSETS_H",
        "#include <stdint.h>", "enum { BM_ASSET_WIDTH = 16 };",
    ]
    pal = atari_pal_palette()
    out.append("static const uint32_t bm_atari_pal_rgb[256] = {")
    for offset in range(0, 256, 8):
        out.append("  " + ",".join(f"0x{value:06x}u" for value in pal[offset:offset + 8]) + ",")
    out.append("};")
    out.append("static const uint8_t bm_asset_max3_charset[1024] = {")
    for offset in range(0, len(max3), 16):
        out.append("  " + ",".join(f"0x{value:02x}" for value in max3[offset:offset + 16]) + ",")
    out.append("};")
    out.append("static const uint8_t bm_asset_max3_hangar_charset[1024] = {")
    for offset in range(0, len(hangar_charset), 16):
        out.append("  " + ",".join(f"0x{value:02x}" for value in hangar_charset[offset:offset + 16]) + ",")
    out.append("};")
    out.append("static const uint8_t bm_asset_max3_building_charset[1024] = {")
    for offset in range(0, len(building_charset), 16):
        out.append("  " + ",".join(f"0x{value:02x}" for value in building_charset[offset:offset + 16]) + ",")
    out.append("};")
    out.append("static const uint8_t bm_asset_max3_tank_charset[1024] = {")
    for offset in range(0, len(tank_charset), 16):
        out.append("  " + ",".join(f"0x{value:02x}" for value in tank_charset[offset:offset + 16]) + ",")
    out.append("};")
    for name, rows, colour in assets:
        values = chunky_mask(rows, colour)
        out.append(f"enum {{ {name.upper()}_HEIGHT = {len(rows)} }};")
        out.append(f"static const uint8_t {name}[{len(values)}] = {{")
        for offset in range(0, len(values), 16):
            out.append("  " + ",".join(str(value) for value in values[offset:offset + 16]) + ",")
        out.append("};")
    ship = make_ship(max2)
    out.append("enum { BM_ASSET_SHIP_HEIGHT = 14 };")
    out.append("static const uint8_t bm_asset_ship[224] = {")
    for offset in range(0, len(ship), 16):
        out.append("  " + ",".join(str(value) for value in ship[offset:offset + 16]) + ",")
    out.append("};")
    for name, image, height in (("bm_asset_fighter_right_initial", make_fighter(max2, False), 10),
                                ("bm_asset_fighter_left_initial", make_fighter(max2, True), 11),
                                ("bm_asset_fighter_forward", make_forward_fighter(max2), 10),
                                ("bm_asset_fighter_back", make_back_fighter(max2), 10),
                                ("bm_asset_fighter_forward_p3", make_forward_p3(max2), 10)):
        out.append(f"enum {{ {name.upper()}_HEIGHT = {height} }};")
        out.append(f"static const uint8_t {name}[{len(image)}] = {{")
        for offset in range(0, len(image), 16):
            out.append("  " + ",".join(str(value) for value in image[offset:offset + 16]) + ",")
        out.append("};")
    runway = max2[229:240]
    out.append(f"enum {{ BM_ASSET_ENRUN_TILE_COUNT = {len(runway)} }};")
    out.append("static const uint8_t bm_asset_enrun_tiles[BM_ASSET_ENRUN_TILE_COUNT] = {")
    out.append("  " + ",".join(f"0x{value:02x}" for value in runway) + ",")
    out.append("};")
    runway_pixels = antic4_strip(max3, runway)
    out.append("enum { BM_ASSET_ENRUN_WIDTH = 88, BM_ASSET_ENRUN_HEIGHT = 8 };")
    out.append("static const uint8_t bm_asset_enrun_strip[BM_ASSET_ENRUN_WIDTH * BM_ASSET_ENRUN_HEIGHT] = {")
    for offset in range(0, len(runway_pixels), 16):
        out.append("  " + ",".join(str(value) for value in runway_pixels[offset:offset + 16]) + ",")
    out.append("};")
    out.extend(("#endif", ""))
    path.write_text("\n".join(out), encoding="ascii")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="path to Disk1/CHBAS")
    parser.add_argument("output", type=Path, help="output directory")
    parser.add_argument("--binary-dir", type=Path, help="optional directory containing MAX.2 and MAX.3")
    args = parser.parse_args()

    segments, labels = parse_chbas(args.source)
    required = {"MAX.2", "MAX.3"}
    if not required.issubset(segments):
        raise SystemExit("CHBAS does not contain both MAX.2 and MAX.3")
    args.output.mkdir(parents=True, exist_ok=True)

    verification = {}
    for name, data in segments.items():
        (args.output / f"{name.lower()}.bin").write_bytes(data)
        if args.binary_dir:
            start, end, binary_data = decode_atari_binary(args.binary_dir / name)
            matching = 0
            for left, right in zip(data, binary_data):
                if left != right:
                    break
                matching += 1
            verification[name] = {
                "load_start": start, "load_end": end,
                "source_bytes": len(data), "binary_payload_bytes": len(binary_data),
                "matching_prefix_bytes": matching,
            }

    max2_items = []
    for entry in labels["MAX.2"]:
        start = entry["offset"]
        max2_items.append((entry["name"], start, segments["MAX.2"][start:start + entry["length"]]))
    glyphs = [(f"glyph {index:03d} (${index:02X})", index * 8, segments["MAX.3"][index * 8:index * 8 + 8]) for index in range(128)]
    write_sheet(args.output / "max2_named_1bpp.svg", "MAX.2 named blocks — 1 bit per pixel", max2_items, 1)
    write_sheet(args.output / "max2_named_2bpp.svg", "MAX.2 named blocks — 2 bits per pixel", max2_items, 2)
    write_sheet(args.output / "max3_charset_1bpp.svg", "MAX.3 character set — 1 bit per pixel", glyphs, 1, 8)
    write_sheet(args.output / "max3_charset_2bpp.svg", "MAX.3 character set — 2 bits per pixel", glyphs, 2, 8)
    write_chunky_sheet(args.output / "max2_semantic_sprites.svg", "Source-derived complete sprites", (
        ("XPL1IM explosion",16,13,chunky_mask(segments["MAX.2"][183:196],8)),
        ("SHPIM + SHPIM1 ship",16,14,make_ship(segments["MAX.2"])),
    ))
    write_chunky_sheet(args.output / "max2_gameplay_masks.svg", "Source-derived gameplay masks", (
        ("FTR fighter right (P2)",16,10,chunky_mask(segments["MAX.2"][330:340],10)),
        ("FTL fighter left (P2)",16,11,chunky_mask(segments["MAX.2"][340:351],10)),
        ("GNST gunsight",16,9,chunky_mask(segments["MAX.2"][351:360],8)),
        ("XCRIM crash",16,10,chunky_mask(segments["MAX.2"][360:370],8)),
        ("XSHPIM destroyed ship",16,13,chunky_mask(segments["MAX.2"][370:383],10)),
    ))
    write_chunky_sheet(args.output / "max2_fighter_frames.svg", "FTURN initial P2/P3 frames", (
        ("FTR right",16,10,make_fighter(segments["MAX.2"],False)),
        ("FTL left",16,11,make_fighter(segments["MAX.2"],True)),
    ))
    runway = segments["MAX.2"][229:240]
    runway_pixels = antic4_strip(segments["MAX.3"], runway)
    write_antic4_strip(args.output / "max3_enrun_strip.svg",
                       "ENRNIM reconstructed through MAX.3 — ANTIC mode 4",
                       runway, runway_pixels)
    pal = atari_pal_palette()
    write_pal_sheet(args.output / "atari_pal_standard.svg", pal)
    (args.output / "atari_pal_standard.act").write_bytes(
        bytes(component for value in pal
              for component in (value >> 16, (value >> 8) & 0xff, value & 0xff)))
    write_c_header(args.output / "bm_chbas_assets.h", segments["MAX.2"], segments["MAX.3"])
    semantic_assets = {
        "explosion": {"source": "XPL1IM", "offset": 183, "width": 16, "height": 13, "format": "player_mask"},
        "ship": {"width": 16, "height": 14, "format": "two_player_layers",
                 "layers": [{"source": "SHPIM", "offset": 240, "height": 12, "y": 0},
                            {"source": "SHPIM1", "offset": 252, "height": 12, "y": 2}]},
        "enrun_tiles": {"source": "ENRNIM", "offset": 229, "count": 11,
                        "width": 88, "height": 8, "format": "antic_mode_4",
                        "character_set": "MAX.3", "character_mask": "0x7f",
                        "colour_mapping": {"00": "COLBK", "01": "COLPF0",
                                           "10": "COLPF1",
                                           "11_bit7_clear": "COLPF2",
                                           "11_bit7_set": "COLPF3"}},
        "fighter_right": {"source": "FTR", "offset": 330, "width": 16,
                          "height": 10, "format": "p2_p3_initial_frame",
                          "players": ["P2", "P3"], "p3_base": "SHDW1"},
        "fighter_left": {"source": "FTL", "offset": 340, "width": 16,
                         "height": 11, "format": "p2_p3_initial_frame",
                         "players": ["P2", "P3"], "p3_base": "SHDW1",
                         "p3_y": 1},
        "gunsight": {"source": "GNST", "offset": 351, "width": 16,
                     "height": 9, "format": "player_mask", "player": "P2"},
        "crash": {"source": "XCRIM", "offset": 360, "width": 16,
                  "height": 10, "format": "player_mask", "player": "P2"},
        "destroyed_ship": {"source": "XSHPIM", "offset": 370, "width": 16,
                           "height": 13, "format": "player_mask", "player": "P2"},
    }
    atari_display = {
        "display_list_instruction": "0x74",
        "display_list_flags": ["LMS", "HSCROL", "ANTIC_MODE_4"],
        "initial_colour_registers": {
            "COLPM0": "0x0e", "COLPM1": "0x00", "COLPM2": "dynamic",
            "COLPM3": "0x00", "COLPF0": "0xc4", "COLPF1": "0x94",
            "COLPF2": "0xd2", "COLPF3": "0x04", "COLBK": "0x00",
        },
        "dli_register_writes": {
            "COLBK": "0xe0", "COLPF1": "memory_0x06f4",
            "COLPF2": "0x36", "COLPF3": "0x98",
        },
        "atari_colour_byte": {
            "hue_bits": "7..4", "luminance_bits": "3..1",
            "ignored_bit": 0, "rgb_profile": "atari800_pal_standard",
            "rgb_profile_parameters": {"gtia_delay_degrees": 23.2,
                                       "gamma": 2.35, "black_level": 16,
                                       "white_level": 235},
            "rgb_profile_source": "Atari800 colours_pal.c standard preset",
        },
    }
    pal_reference = atari_pal_palette()
    atari_display["initial_colour_rgb"] = {
        name: f"0x{pal_reference[value]:06x}"
        for name, value in (("COLPM0", 0x0e), ("COLPM1", 0x00),
                            ("COLPM3", 0x00), ("COLPF0", 0xc4),
                            ("COLPF1", 0x94), ("COLPF2", 0xd2),
                            ("COLPF3", 0x04), ("COLBK", 0x00))
    }
    atari_display["dli_fixed_colour_rgb"] = {
        name: f"0x{pal_reference[value]:06x}"
        for name, value in (("COLBK", 0xe0), ("COLPF2", 0x36),
                            ("COLPF3", 0x98))
    }
    metadata = {"segments": {name: len(data) for name, data in segments.items()}, "labels": labels,
                "semantic_assets": semantic_assets, "atari_display": atari_display,
                "binary_verification": verification}
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"extracted MAX.2={len(segments['MAX.2'])} bytes, MAX.3={len(segments['MAX.3'])} bytes")
    if verification:
        for name, result in verification.items():
            print(f"{name}: {result['matching_prefix_bytes']}/{result['source_bytes']} source bytes match binary payload")


if __name__ == "__main__":
    main()
