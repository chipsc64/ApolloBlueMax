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
| `DEBUG_HUD=1` | `bluemax-apollo-debug` | developer HUD instead of the status line |
| `CAPITAL_TEST=1` | `bluemax-apollo-capital-test1` | test aid: start near the capital |
| `CAPITAL_TEST=2` | `bluemax-apollo-capital-test2` | as above, two capital targets already destroyed |
| `RENDERER=scaled` | `bluemax-apollo-scaled` | the classic picture at 640x400 in 16-bit colour (display path test) |
| `SOUND_TEST=1` | `bluemax-apollo-sound-test` | POKEY test tones; audio diagnostics on exit |

Options can be combined, for example
`make ... TICK_RATE=50 ENHANCED_SOUND=1`. When you change only options,
remove the matching `build/` directory first: make only tracks source
changes.

### Enhanced sound

The Atari has one POKEY with four channels, and Blue Max shares them: an
aircraft warning silences the machine gun, and a nearby enemy silences a
falling bomb. With `ENHANCED_SOUND=1`, guns, ground guns, warnings, beeps and
the bomb each get their own POKEY. All of them are mixed into the SAGA
stream, so the sounds play together. The default build keeps the original
behaviour.

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
make test-render                # renderers (classic, scaled)
```

`make headless` runs the game without a window and prints framebuffer
checksums.

## Project layout

```text
include/            core interfaces (game, world, flight, POKEY, sound, platform)
src/core/           portable game code: game.c, world.c, flight.c, collision.c,
                    render.c (classic renderer), render_scaled.c, renderer.c,
                    sprite.c, tile.c,
                    pokey.c, sound.c
src/main.c          main loop
platform/apollo/    SAGA video, audio, input and the AMMX blitter
platform/headless/  backend for tests and the headless build
tests/              host tests
tools/              asset and tune extraction, status font, snapshots
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
