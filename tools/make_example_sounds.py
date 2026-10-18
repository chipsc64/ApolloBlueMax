#!/usr/bin/env python3
"""Write the example samples in sounds/: synthetic placeholders that show
the format and make SOUND=samples testable.  Replace them with real
recordings; this script only exists so the examples can be reproduced.

gun_own.wav  one machine gun shot, looped while the trigger is held: the
             original GUNSND sounds every fourth frame at 60 Hz, so one
             shot lasts 1/15 s."""
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


def main(sound_dir):
    os.makedirs(sound_dir, exist_ok=True)
    write_wav(os.path.join(sound_dir, 'gun_own.wav'), gun_shot(random.Random(1983)))


if __name__ == '__main__':
    main(*(sys.argv[1:] or ['sounds']))
