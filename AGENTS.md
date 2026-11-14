# AGENTS.md: Blue Max for Apollo 68080

## Read first

This repository targets the Apollo V4 platform. Before doing anything else,
read `ApolloKnowledgeBase/AGENTS.md` and follow it. It explains the platform,
the assumptions you must not make, and which knowledge base files to read for
your task. Paths in that file are relative to `ApolloKnowledgeBase/`.

If `ApolloKnowledgeBase/` is empty, the submodule has not been initialized.
Ask the user to run `git submodule update --init` instead of proceeding
without it.

## This repository

- **What it is:** a faithful port of Blue Max (Synapse Software, 1983) from
  the Atari 8-bit to the Apollo 68080 / SAGA. The game logic follows the
  original 6502 code update by update; a portable C99 core is tested on the
  host, the Apollo platform layer drives SAGA video and audio.
- **Kind:** game
- **Target core:** Core 12856 (the developer's V4). Many knowledge base
  facts are measured on older cores (12841, 12847): check
  `applies_to.core` and say so when a fact does not cover 12856.
- **Target model:** any V4 model; tested on the V4 Standalone (SA)
- **Target OS:** Coffin (a ROM-patched AmigaOS 3.x distribution; tested).
  ApolloOS R10 should work but is not tested. Coffin saves AMMX registers
  on task switches only with the Apollo bit set (`SetSR(0x800,0x800)` in
  `saga_open`); keep that.
- **Execution mode:** os-friendly, with direct SAGA register access for the
  display (mode, pointer, palette) and audio, by the developer's choice.
  Multitasking stays on; timer.device and a vertical blank interrupt server
  pace the game.
- **Languages:** C99, 68080 assembly (vasm, `platform/apollo/ammx_blit.s`)

Fields marked `not set yet`: ask the developer before relying on them, and
fill them in.

## Relevant knowledge base areas

- `ApolloKnowledgeBase/toolchain/` (with `best-practices/` and `pitfalls/`)
- `ApolloKnowledgeBase/core/cpu-68080/`, `core/best-practices/`, `core/pitfalls/`
- `ApolloKnowledgeBase/core/saga-video-isabelle/`
- `ApolloKnowledgeBase/core/saga-audio-arne/`
- `ApolloKnowledgeBase/core/saga-registers/`
- `ApolloKnowledgeBase/os/` (with `best-practices/` and `pitfalls/`)

## Build and test

```sh
# Apollo build (variants: see the option table in README.md)
make APOLLO_GCC_ROOT=/home/chips/ApolloCrossDev/Compilers/GCC-6.50-Stable
make APOLLO_GCC_ROOT=... RENDERER=hd SOUND=samples
make APOLLO_GCC_ROOT=... SOUND_TEST=1 tear-test

# Host tests, after every change
make test
make CLASSIC_TEST_PAGE=0 test
make ATARI_SCROLL=0 test
make test-flight
make test-takeoff
make test-scroll
make test-pokey
```

Build and fix all errors and warnings before presenting code (`-Werror` is
on). When only build options change, remove the matching `build/` directory
first.

## Repository-specific rules

- Never commit, push or merge without the developer's explicit permission
  for that change. Work happens on the branch `hd-sprites`; `main` only by
  explicit request.
- Never commit the developer's own art: the PNGs in `art/materials/`,
  `art/sprites/` and the folders `art/*.claude/`.
- Faithful first: anything that behaves differently from the Atari original
  goes behind a build option (`ENHANCED_SOUND`, `SMOOTH_SCROLL`, ...) and is
  marked "not original" in README.md.
- Host tests cannot catch m68k code generation bugs. Avoid 64-bit shifts and
  divisions in code that runs on the Apollo (GCC 6.5 miscompiled one here),
  and prove hardware results with a `SOUND_TEST` diagnostic line.
- `docs/` is local only (ignored by git): working notes in
  `docs/CLAUDE_TASKS.md`, recordings, briefs.
- The developer writes in Dutch; reply in Dutch.

## Knowledge base feedback

When you find something missing or wrong in ApolloKnowledgeBase while working
here, tell the user and offer to draft a change following
`ApolloKnowledgeBase/CONTRIBUTING.md`.

Offer a lesson when the user says the project, a release or a milestone is
done, and right away when an assumption turns out wrong. Follow
`ApolloKnowledgeBase/AGENTS.md` section 8: draft it from the lesson template,
show it to the user, and only submit it (as a pull request from the
`ApolloKnowledgeBase/` submodule) with their approval.
