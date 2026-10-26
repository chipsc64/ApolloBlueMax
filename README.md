# Blue Max for Apollo 68080

A faithful port of **Blue Max** (Synapse Software, 1983, by Bob Polin) from
the Atari 8-bit computers to the **Apollo 68080 / SAGA** (Vampire V4).

The game logic was reconstructed from the original 6502 source code and,
where the shipped game differs from that source, from the original `MAX.1`
binary. Flight model, scrolling, enemies, bombing, landing and refuelling,
the capital city, scoring, rankings and the attract mode follow the Atari
original update by update. Sound comes from a software POKEY that is driven
by the same register writes as the original code.

## Features

- Complete game: takeoff, flight, bombing runs, enemy aircraft, ground
  guns, landing and service (fuel, bombs, repairs), the capital, game over
  and ranking.
- Attract mode (the computer pilot) and the OPTION menu (skill, controls,
  gravity), as on the Atari.
- Original sound: a software POKEY emulation (4 channels, poly4/5/9/17,
  high-pass filters, 16-bit pairs) plays all sound effects and the PLAY.IT
  title tune. It streams to a 16-bit SAGA audio channel at 44.3 kHz.
- 320x200 8-bit chunky display through SAGA, with the AMMX blitter for
  sprites.
- Build options for an enhanced sound mix and a 50 Hz (PAL) game tempo.

## Requirements

- An Apollo 68080 system with SAGA and AMMX. Tested on a **Vampire V4
  Standalone** running ApolloOS.
- A joystick in **port 2**.
- To build: the m68k-amigaos GCC 6.5 cross compiler (for example from
  ApolloCrossDev), GNU make, Python 3 and a host C compiler for the tests.
- **The original Blue Max files** (see [Original game files](#original-game-files)).

## Controls

| Input | Action |
|---|---|
| Joystick (port 2) | Fly; the stick works as on the Atari |
| Fire | Machine gun |
| Fire + stick down (dive) | Drop a bomb |
| Second fire button | Bomb (shortcut for fire + dive) |
| F4 | START |
| F3 | SELECT |
| F2 | OPTION (menu) |
| Space | Pause |
| Q | Quit to the OS |

The function keys stand in for the Atari console keys, as in most Atari
emulators. After power-on the game runs the attract mode; press F4 to play.

## Original game files

This repository contains no original Blue Max code, graphics or music. The
build extracts the character set, sprites and tune from the released source
and binaries, published at
[gitlab.com/atari/bluemax](https://gitlab.com/atari/bluemax):

```sh
git clone https://gitlab.com/atari/bluemax.git
```

The build needs these files from that repository:

| File | Used for |
|---|---|
| `Disk1/CHBAS` | character set and graphics |
| `Disk2/MAX.2`, `Disk2/MAX.3` | sprite and glyph data |
| `Disk2/MAX.4` | PLAY.IT tune (three voices) |

By default the Makefile looks for them in `../../Source/BlueMax/`. Use
`CHBAS=` and `CHBAS_BINARY_DIR=` to point to your clone instead:

```sh
make CHBAS=/path/to/bluemax/Disk1/CHBAS CHBAS_BINARY_DIR=/path/to/bluemax/Disk2 ...
```

The source was made available courtesy of Bob Polin on AtariAge (2016) and
converted to text by Dan Boris. It is in Synapse Assembler format.

## Building

```sh
make APOLLO_GCC_ROOT=/path/to/GCC-6.50-Stable
```

This builds `bluemax-apollo`, the faithful port. Copy it to the Apollo and
run it from the Shell. `make upload AMIGAHOST=...` copies it with
ApolloExplorer's `acp`.

Add an option to build a variant:

| Option | Binary | Purpose |
|---|---|---|
| (none) | `bluemax-apollo` | the original game, 60 updates per second (NTSC) |
| `ENHANCED_SOUND=1` | `bluemax-apollo-enhanced-sound` | sounds no longer cut each other off (not original) |
| `TICK_RATE=50` | `bluemax-apollo-50hz` | PAL tempo: 50 updates per second, PAL POKEY clock |
| `SOUND=samples` | `bluemax-apollo-samples` | sounds with a sample in `sounds/` play it instead of their POKEY sound (see below) |
| `DEBUG_HUD=1` | `bluemax-apollo-debug` | developer HUD instead of the status line |
| `CAPITAL_TEST=1` | `bluemax-apollo-capital-test1` | test aid: start near the capital |
| `CAPITAL_TEST=2` | `bluemax-apollo-capital-test2` | as above, two capital targets already destroyed |
| `RENDERER=scaled` | `bluemax-apollo-scaled` | the classic picture at 640x400 in 16-bit colour (display path test) |
| `RENDERER=hd` | `bluemax-apollo-hd` | 640x400 in 16-bit colour with the textures from `art/` (see below) |
| `SOUND_TEST=1` | `bluemax-apollo-sound-test` | POKEY test tones; audio diagnostics on exit |

Options can be combined, for example
`make ... TICK_RATE=50 ENHANCED_SOUND=1`. When you change only options,
remove the matching `build/` directory first: make only tracks source
changes.

### Upgraded graphics (`RENDERER=hd`)

The HD renderer draws at 640x400 in 16-bit colour. The original picture
still decides what is where: coastlines, roads, buildings and objects, and
all collisions (they use the Atari shapes, see `collision.c`). Each flat
Atari colour of the playfield can get a texture:

```text
art/materials/C4.png    16x16, seamless: grass (Atari colour $C4)
art/materials/94.png    16x16, seamless: water (Atari colour $94)
```

The file name is the Atari colour in hex. A texture covers one Atari tile
(8x8 Atari pixels at 2x) and scrolls with the landscape. Colours without a
texture, sprites, the status line and flashes stay as in the original.
Objects (the aircraft, enemies, ships, vehicles, explosions, ...) can get
images too, at twice the Atari size (an Atari sprite is 16 pixels wide, its
image 32), with transparency:

```text
art/sprites/KIND_V.png  one variant of an object
art/sprites/KIND.png    every variant of an object
```

| KIND | Variants (V) |
|---|---|
| `player` | 0/1 level, 2/3 banking left, 4/5 banking right (even/odd: propeller frame) |
| `shadow` | 0 normal, 1 thin far shadow, 2 banking |
| `crash` | 0/1 the two halves of the explosion |
| `shot`, `enemy_shot`, `gunsight` | 0 |
| `capital` | 0 intact target (64x30), 1 wreck (64x36, drawn twice as wide while it pulses) |
| `bomb` | 0/1 the falling bomb's two phases |
| `explosion` | 0 on land, 1 water splash |
| `ship` | 0 afloat, 1 destroyed (placed one Atari line higher) |
| `vehicle` | bits: 1 truck, 2 driving left, 4 destroyed (one line higher) |
| `enemy` | bits: 1 coming forward, 2/4 turning left/right, 8 wreck (one line higher), 16 crash colour |
| `enemy_shadow` | 0 behind, 1 forward fighter's shadow layer |
| `gun_shot` | 0 shell, 1 shell burst |

An object is drawn from its images only when every variant has one (or
`KIND.png` exists); otherwise it stays as in the original. The image's
top-left corner lands at twice the Atari sprite's top-left corner. Pure
grey pixels (R=G=B) take the object's Atari colour, scaled by their grey
level, so bonus targets keep their colour. Collisions always use the
original shapes.

The textures and sprites in the repository are generated examples
(`tools/make_example_art.py`, derived from the Atari shapes); replace them
with real artwork. Shots, the bomb and the shells are small: `shot` 8x2,
`enemy_shot` 8x4, `bomb` 8x6, `gun_shot` 8x4 (shell) and 12x8 (burst).
For `enemy`, `ship`, `vehicle`, `gunsight` and `capital` the build warns about images
without pure grey pixels: bonus targets would no longer show. PNGs must be
8-bit RGB, RGBA, grey or palette images. `tools/art_assets.py` converts
them during the build, so no extra Python packages are needed.

### Enhanced sound

The Atari has one POKEY with four channels, and Blue Max shares them: an
aircraft warning silences the machine gun, and a nearby enemy silences a
falling bomb. With `ENHANCED_SOUND=1`, guns, ground guns, warnings, beeps and
the bomb each get their own POKEY. All of them are mixed into the SAGA
stream, so the sounds play together. The default build keeps the original
behaviour.

### Samples (`SOUND=samples`)

With `SOUND=samples` a game sound plays a recorded sample instead of its
POKEY routine when `sounds/NAME.wav` exists; every other sound stays the
original POKEY sound. The samples are mixed in software into the same
44.3 kHz stream as the POKEY. WAV files must be PCM, 8 or 16 bit, mono or
stereo, at any rate (44100 Hz recommended), at most 10 seconds;
`tools/sound_assets.py` converts them during the build.

| NAME | Sound |
|---|---|
| `engine` | the propeller, looped; its volume follows the throttle as on the Atari, its pitch rises a little with it |
| `engine_dying` | the engine without fuel, looped until it stops |
| `attacker_engine` | an attacking aircraft over the airfield, looped; the own engine keeps sounding |
| `gun_own` | the player's machine gun: one shot, looped while firing |
| `gun_enemy` | an enemy fighter's gun: one shot, looped while it fires |
| `ground_gun` | an anti-aircraft gun firing |
| `flak` | its shell bursting |
| `explosion_small` | damage to the player's aircraft |
| `explosion_medium` | an enemy aircraft, ship, vehicle or building destroyed |
| `explosion_large` | a bomb, the capital target, the player's crash |
| `bomb_whistle` | a falling bomb; stopped where it lands (behind roofs it runs on into the explosion) |
| `splash` | a bomb in the water |
| `runway` | the signal when a friendly runway comes into view |

The files in `sounds/` are generated examples (`tools/make_example_sounds.py`);
replace them with real recordings. While the title tune plays, its POKEY
channels are left alone. The title tune and the warning tones (low altitude,
enemy in line, aircraft warning, landing marker, repair beep) always stay
POKEY sounds.

At start the Apollo build checks the mixer on fixed input and prints
`mixer self-test passed`. With `SOUND_TEST=1` it also prints, on exit, how
many samples were started and their peak level.

### 50 Hz

The default build runs one game update per vertical blank. On the V4's
60 Hz display that is the tempo of the NTSC original. `TICK_RATE=50` paces
updates with the timer.device E-clock instead and switches the POKEY to the
PAL clock: the game, music and effects run as on a PAL Atari, about 17%
slower. On a 60 Hz screen every sixth frame repeats.

## Tests

The game core is portable C99 and is tested on the host:

```sh
make test                       # world and rendering
make CLASSIC_TEST_PAGE=0 test   # legacy placeholder world
make test-flight                # Atari flight model
make test-takeoff               # game flow: takeoff, landing, service, capital, ...
make test-scroll                # scroll timing
make test-pokey                 # POKEY emulation, sound routines and tune
make test-render                # renderers (classic, scaled, hd)
```

`make headless` runs the game without a window and prints framebuffer
checksums.

## Project layout

```text
include/            core interfaces (game, world, flight, POKEY, sound, platform)
src/core/           portable game code: game.c, world.c, flight.c, collision.c,
                    render.c (classic renderer), render_scaled.c, render_hd.c,
                    renderer.c,
                    sprite.c, tile.c,
                    pokey.c, sound.c, mixer.c
src/main.c          main loop
platform/apollo/    SAGA video, audio, input and the AMMX blitter
platform/headless/  backend for tests and the headless build
tests/              host tests
tools/              asset, tune, art and sound conversion, status font, snapshots
art/                upgraded graphics for RENDERER=hd
sounds/             samples for SOUND=samples
```

## How it works

- **Video:** the Atari display (ANTIC mode 4 playfield, player/missile
  graphics, GTIA priorities and collisions) is rebuilt in software into an
  8-bit chunky framebuffer. That framebuffer is double-buffered through SAGA
  and uses the standard Atari palette.
- **Timing:** a VERTB interrupt server counts vertical blanks; one game
  update corresponds to one Atari VBI.
- **Sound:** game code writes `$D200..$D208` into a software POKEY.
  Every frame the platform renders the samples needed just ahead of the
  play position into a looped 8192-sample buffer on SAGA audio channel 0.
  The play position is computed from the E-clock. The channel's AUD0
  interrupt only corrects drift.

## License

The port code is released under the [MIT License](LICENSE). The license
covers this port only, not Blue Max itself (see below).

## Credits and legal

Blue Max was written by Bob Polin and published by Synapse Software in
1983. All rights to the original game, its code, graphics and music belong
to their respective owners. This project does not include any of them. It
reads your copy at build time and claims no rights to it.

Before distributing builds, check whether you may distribute the graphics
and music they contain.

Apollo, Vampire and SAGA are trademarks of the Apollo Team.
