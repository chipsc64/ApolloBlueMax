#!/usr/bin/env python3
"""Write the example samples in sounds/: synthetic placeholders that show
the format and make SOUND=samples testable.  Replace them with real
recordings; this script only exists so the examples can be reproduced.

gun_own.wav, gun_enemy.wav  one machine gun shot, looped while the gun
             fires: the original GUNSND sounds every fourth frame at 60 Hz,
             so one shot lasts 1/15 s.
ground_gun.wav, flak.wav    an anti-aircraft gun firing; its shell bursting.
explosion_small/medium/large.wav  damage; aircraft and objects; bombs,
             crashes and the capital.
bomb_whistle.wav  a falling bomb, stopped where it lands.
splash.wav   a bomb in the water."""
import math
import os
import random
import struct
import sys

RATE = 44100


def write_wav(path, values, rate=RATE):
    pcm = struct.pack(f'<{len(values)}h', *values)
    header = (b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE'
              + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
              + b'data' + struct.pack('<I', len(pcm)))
    open(path, 'wb').write(header + pcm)


def gun_shot(rng):
    """A sharp crack of high noise over a short body, so that it cuts
    through the low engine drone (the original gun is high POKEY noise)."""
    length = RATE // 15
    out, prev, low = [], 0.0, 0.0
    for i in range(length):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        high, prev = noise - prev, noise                 # first difference: bright
        low += (noise - low) * 0.25
        v = (0.9 * high * math.exp(-t * 28) + 0.6 * low * math.exp(-t * 40)
             + 0.25 * math.sin(2 * math.pi * 140 * t) * math.exp(-t * 60))
        out.append(v)
    peak = max(abs(v) for v in out)
    return [int(round(v / peak * 29000)) for v in out]


def normalise(out, peak_to=29000):
    peak = max(abs(v) for v in out) or 1
    return [int(round(v / peak * peak_to)) for v in out]


def noise_burst(rng, seconds, decay, brightness, rumble=0.0, rumble_hz=50):
    """Noise between a low-passed body and its bright edge, with a low
    sine rumble, all decaying."""
    out, low, prev = [], 0.0, 0.0
    for i in range(int(RATE * seconds)):
        t = i / RATE
        n = rng.uniform(-1, 1)
        low += (n - low) * (0.02 + 0.2 * brightness)
        high, prev = n - prev, n
        env = math.exp(-t * decay) * min(1.0, t * 400)
        v = env * ((1 - brightness) * low * 3 + brightness * high * 0.6)
        v += rumble * math.sin(2 * math.pi * rumble_hz * t) * math.exp(-t * decay * 0.7)
        out.append(v)
    return normalise(out)


def whistle(seconds=4.0):
    out, phase = [], 0.0
    for i in range(int(RATE * seconds)):
        t = i / RATE
        f = 1400 * math.exp(-t * 0.25)                  # falling pitch
        phase += 2 * math.pi * f / RATE
        out.append(math.sin(phase) * min(1.0, t * 20))
    return normalise(out)


def splash(rng, seconds=1.2):
    out, low = [], 0.0
    for i in range(int(RATE * seconds)):
        t = i / RATE
        n = rng.uniform(-1, 1)
        low += (n - low) * 0.15
        env = min(1.0, t * 60) * math.exp(-t * 3.5)
        out.append(env * (low * 2 + 0.3 * n * math.exp(-t * 8)))
    return normalise(out)


def main(sound_dir):
    os.makedirs(sound_dir, exist_ok=True)
    rng = random.Random(1983)
    sounds = {
        'gun_own': gun_shot(rng),
        'gun_enemy': noise_burst(rng, 1 / 15, 30, 0.5, 0.3, 110),
        'ground_gun': noise_burst(rng, 0.35, 12, 0.35, 0.8, 60),
        'flak': noise_burst(rng, 0.6, 7, 0.55, 0.5, 80),
        'explosion_small': noise_burst(rng, 0.6, 6, 0.3, 0.6, 55),
        'explosion_medium': noise_burst(rng, 1.2, 3.5, 0.25, 0.7, 45),
        'explosion_large': noise_burst(rng, 2.2, 2.0, 0.2, 0.9, 38),
        'bomb_whistle': whistle(),
        'splash': splash(rng),
    }
    for name, values in sounds.items():
        write_wav(os.path.join(sound_dir, name + '.wav'), values)


if __name__ == '__main__':
    main(*(sys.argv[1:] or ['sounds']))
