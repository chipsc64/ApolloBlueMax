APOLLO_GCC_ROOT ?= /mnt/m/bluemax-master/Compilers/GCC-6.50-Stable
.DEFAULT_GOAL := all
.PHONY: test-flight
test-flight:
	mkdir -p build/flight-test
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_flight.c src/core/flight.c -o build/flight-test/test
	build/flight-test/test
.PHONY: test-takeoff
test-takeoff: $(ASSET_HEADER)
	mkdir -p build/takeoff-test
	$(HOST_CC) $(filter-out -DBM_ATARI_FLIGHT=$(ATARI_FLIGHT) -DBM_CLASSIC_TEST_PAGE=$(CLASSIC_TEST_PAGE),$(HOST_CFLAGS)) -DBM_ATARI_FLIGHT=1 -DBM_CLASSIC_TEST_PAGE=1 tests/test_takeoff_game.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c -o build/takeoff-test/test
	build/takeoff-test/test
CC := $(APOLLO_GCC_ROOT)/bin/m68k-amigaos-gcc
AS := $(APOLLO_GCC_ROOT)/bin/vasmm68k_mot
# T26: the default build is the ported game (classic world, Atari flight
# model, Atari scroll cadence).  0 selects the old preview variants.
CLASSIC_TEST_PAGE ?= 1
ATARI_SCROLL ?= 1
ATARI_FLIGHT ?= 1
# Test aid: 1 starts as after a sortie with all objectives (straight into the
# city, short first area); 2 also marks two capital targets destroyed.
CAPITAL_TEST ?= 0
# 1 shows the developer HUD instead of the original status line.
DEBUG_HUD ?= 0
# 1 plays a fixed POKEY test programme over the game (T29).
SOUND_TEST ?= 0
# 1 gives guns, ground guns, warnings, beeps and the bomb their own POKEY,
# so that they no longer cut each other off (not in the original).
ENHANCED_SOUND ?= 0
# 50 runs the game like a PAL Atari: 50 updates per second paced by the
# E-clock (on a 60 Hz screen every sixth vblank has none) and the PAL POKEY
# clock.  60 = one update per vblank, as the NTSC original.
TICK_RATE ?= 60
BUILD_VARIANT := $(if $(filter 0,$(CLASSIC_TEST_PAGE)),-placeholder,)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter 0,$(ATARI_SCROLL)),-preview-scroll,)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter 0,$(ATARI_FLIGHT)),-preview-flight,)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter-out 0,$(CAPITAL_TEST)),-capital-test$(CAPITAL_TEST),)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter 1,$(DEBUG_HUD)),-debug,)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter 1,$(SOUND_TEST)),-sound-test,)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter 1,$(ENHANCED_SOUND)),-enhanced-sound,)
BUILD_VARIANT := $(BUILD_VARIANT)$(if $(filter 50,$(TICK_RATE)),-50hz,)
TARGET := bluemax-apollo$(BUILD_VARIANT)
BUILD := build/apollo$(BUILD_VARIANT)
ACP ?= $(CURDIR)/../ApolloExplorer/acp/acp
AMIGA_DEST ?= $(AMIGAHOST):Ram Disk/
HOST_CC ?= cc
HOST_BUILD := build/headless$(BUILD_VARIANT)
SNAPSHOT_BUILD := build/snapshot-scroll-$(ATARI_SCROLL)-flight-$(ATARI_FLIGHT)
CHBAS ?= ../../Source/BlueMax/Disk1/CHBAS
CHBAS_BINARY_DIR ?= ../../Source/BlueMax/Disk2
ASSET_BUILD := build/assets/chbas
ASSET_HEADER := $(ASSET_BUILD)/bm_chbas_assets.h
HOST_CFLAGS := -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -DBM_CLASSIC_TEST_PAGE=$(CLASSIC_TEST_PAGE) -Iinclude -I$(ASSET_BUILD)
HOST_CFLAGS += -DBM_ATARI_SCROLL=$(ATARI_SCROLL)
HOST_CFLAGS += -DBM_ATARI_FLIGHT=$(ATARI_FLIGHT)
HOST_CFLAGS += -DBM_CAPITAL_TEST=$(CAPITAL_TEST)
HOST_CFLAGS += -DBM_DEBUG_HUD=$(DEBUG_HUD)
HOST_CFLAGS += -DBM_ENHANCED_SOUND=$(ENHANCED_SOUND)
HOST_CFLAGS += -DBM_TICK_RATE=$(TICK_RATE)
CFLAGS += -DBM_TICK_RATE=$(TICK_RATE)
CFLAGS += -DBM_SOUND_TEST=$(SOUND_TEST)
CFLAGS += -DBM_ENHANCED_SOUND=$(ENHANCED_SOUND)
CFLAGS += -DBM_TICK_RATE=$(TICK_RATE)
CFLAGS := -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -m68080 -m68881 -ffast-math -fomit-frame-pointer -DREGPARM=4 -DBM_APOLLO_68080=1 -DBM_CLASSIC_TEST_PAGE=$(CLASSIC_TEST_PAGE) -noixemul -Iinclude -I$(ASSET_BUILD)
CFLAGS += -MMD -MP
ASFLAGS := -Fhunk -m68080 -m68881 -opt-speed -kick1hunks
CFLAGS += -DBM_ATARI_SCROLL=$(ATARI_SCROLL)
CFLAGS += -DBM_ATARI_FLIGHT=$(ATARI_FLIGHT)
CFLAGS += -DBM_CAPITAL_TEST=$(CAPITAL_TEST)
CFLAGS += -DBM_DEBUG_HUD=$(DEBUG_HUD)
CFLAGS += -DBM_SOUND_TEST=$(SOUND_TEST)
CFLAGS += -DBM_ENHANCED_SOUND=$(ENHANCED_SOUND)
CFLAGS += -DBM_TICK_RATE=$(TICK_RATE)
LDFLAGS := -m68080 -m68881 -noixemul
C_SOURCES := src/main.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c platform/apollo/platform_saga.c platform/apollo/ammx_selftest.c platform/apollo/sprite_ammx.c
S_SOURCES := platform/apollo/ammx_blit.s
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) $(patsubst %.s,$(BUILD)/%.o,$(S_SOURCES))
DEPENDENCIES := $(patsubst %.c,$(BUILD)/%.d,$(C_SOURCES))
.PHONY: all upload assets test headless snapshots boat-shot-snapshots clean
.NOTPARALLEL: all upload
.PHONY: test-pokey
test-pokey: $(ASSET_HEADER)
	mkdir -p build/pokey-test
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude -I$(ASSET_BUILD) tests/test_pokey.c src/core/pokey.c src/core/sound.c -o build/pokey-test/test
	build/pokey-test/test
.PHONY: test-scroll
test-scroll: $(ASSET_HEADER)
	mkdir -p build/scroll-test
	$(HOST_CC) $(filter-out -DBM_CLASSIC_TEST_PAGE=$(CLASSIC_TEST_PAGE) -DBM_ATARI_SCROLL=$(ATARI_SCROLL),$(HOST_CFLAGS)) -DBM_CLASSIC_TEST_PAGE=1 -DBM_ATARI_SCROLL=1 tests/test_scroll.c src/core/world.c src/core/tile.c -o build/scroll-test/test
	build/scroll-test/test
all: $(TARGET)
upload: $(TARGET)
	@test -n "$(AMIGAHOST)" || { echo "AMIGAHOST is not set" >&2; exit 1; }
	$(ACP) $(TARGET) "$(AMIGA_DEST)"
test: $(HOST_BUILD)/bluemax-test
	$(HOST_BUILD)/bluemax-test
headless: $(HOST_BUILD)/bluemax-headless
	$(HOST_BUILD)/bluemax-headless
snapshots: $(SNAPSHOT_BUILD)/frames/.stamp
boat-shot-snapshots: $(SNAPSHOT_BUILD)/boat-shot-snapshot
	mkdir -p $(SNAPSHOT_BUILD)/boat-shots
	$(SNAPSHOT_BUILD)/boat-shot-snapshot $(SNAPSHOT_BUILD)/boat-shots
$(SNAPSHOT_BUILD)/boat-shot-snapshot: tools/boat_shot_snapshot.c tools/classic_snapshot.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c include/bm_flight.h include/bm_game.h include/bm_pokey.h include/bm_sound.h include/bm_platform.h include/bm_sprite.h include/bm_tile.h include/bm_world.h $(ASSET_HEADER)
	mkdir -p $(dir $@)
	$(HOST_CC) $(filter-out -DBM_CLASSIC_TEST_PAGE=$(CLASSIC_TEST_PAGE),$(HOST_CFLAGS)) -DBM_CLASSIC_TEST_PAGE=1 tools/boat_shot_snapshot.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c -o $@
$(SNAPSHOT_BUILD)/frames/.stamp: tools/classic_snapshot.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c include/bm_flight.h include/bm_game.h include/bm_pokey.h include/bm_sound.h include/bm_platform.h include/bm_sprite.h include/bm_tile.h include/bm_world.h $(ASSET_HEADER)
	mkdir -p $(SNAPSHOT_BUILD)/frames
	$(HOST_CC) $(filter-out -DBM_CLASSIC_TEST_PAGE=$(CLASSIC_TEST_PAGE),$(HOST_CFLAGS)) -DBM_CLASSIC_TEST_PAGE=1 tools/classic_snapshot.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c -o $(SNAPSHOT_BUILD)/classic-snapshot
	$(SNAPSHOT_BUILD)/classic-snapshot $(SNAPSHOT_BUILD)/frames
	touch $@
assets: $(ASSET_HEADER)
$(ASSET_HEADER): tools/extract_chbas.py tools/music_data.py $(CHBAS) $(CHBAS_BINARY_DIR)/MAX.4
	python3 tools/extract_chbas.py $(CHBAS) $(ASSET_BUILD) --binary-dir $(CHBAS_BINARY_DIR)
	python3 tools/music_data.py $(CHBAS_BINARY_DIR)/MAX.4 $(ASSET_BUILD)/bm_music_data.h
$(HOST_BUILD)/bluemax-test: tests/test_game.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c include/bm_flight.h include/bm_game.h include/bm_pokey.h include/bm_sound.h include/bm_platform.h include/bm_sprite.h include/bm_tile.h include/bm_world.h $(ASSET_HEADER)
	mkdir -p $(dir $@)
	$(HOST_CC) $(filter-out -DBM_ATARI_FLIGHT=$(ATARI_FLIGHT),$(HOST_CFLAGS)) -DBM_ATARI_FLIGHT=0 tests/test_game.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c -o $@
$(HOST_BUILD)/bluemax-headless: src/main.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c platform/headless/platform_headless.c include/bm_flight.h include/bm_game.h include/bm_pokey.h include/bm_sound.h include/bm_platform.h include/bm_sprite.h include/bm_tile.h include/bm_world.h $(ASSET_HEADER)
	mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) src/main.c src/core/flight.c src/core/game.c src/core/pokey.c src/core/sound.c src/core/render.c src/core/sprite.c src/core/tile.c src/core/world.c platform/headless/platform_headless.c -o $@
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
$(BUILD)/%.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/src/core/game.o $(BUILD)/src/core/render.o $(BUILD)/src/core/tile.o $(BUILD)/src/core/world.o: $(ASSET_HEADER)
$(BUILD)/%.o: %.s Makefile
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<
-include $(DEPENDENCIES)
clean:
	rm -rf build $(TARGET)
