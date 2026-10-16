#ifndef BM_TILE_H
#define BM_TILE_H

#include "bm_platform.h"
#include <stddef.h>

enum {
  BM_CLASSIC_INITIAL_BASE = 16,
  BM_CLASSIC_DLI_BASE = 21,
  BM_CLASSIC_COLOUR_COUNT = 5,
  BM_ATARI_PLAYFIELD_COLUMNS = 40,
  BM_ATARI_PLAYFIELD_FETCH_COLUMNS = 41,
  BM_ATARI_PLAYFIELD_ROWS = 22,
  BM_ATARI_PLAYFIELD_HEIGHT = BM_ATARI_PLAYFIELD_ROWS * 8,
  BM_ATARI_PLAYFIELD_FETCH_ROWS = 23,
  BM_ATARI_WORLD_STRIDE = 65,
  BM_ATARI_FINE_SCROLL_PHASES = 4
};

typedef struct {
  uint8_t colbk, colpf0, colpf1, colpf2, colpf3;
} BmAntic4Colours;

typedef struct {
  int16_t y;
  BmAntic4Colours colours;
} BmAntic4Zone;

void bm_atari_pal_install(BmFramebuffer *framebuffer, uint8_t saga_base,
                          const uint8_t atari_registers[BM_CLASSIC_COLOUR_COUNT]);
void bm_antic4_blit(BmFramebuffer *framebuffer, const uint8_t charset[1024],
                    const uint8_t *screen_codes, uint16_t columns, uint16_t rows,
                    uint16_t screen_stride, int16_t x, int16_t y,
                    const BmAntic4Zone *zones,
                    size_t zone_count);
int bm_antic4_blit_playfield(BmFramebuffer *framebuffer,
                             const uint8_t charset[1024], const uint8_t *world,
                             uint16_t source_column, uint8_t fine_phase,
                             int16_t x, int16_t y, const BmAntic4Zone *zones,
                             size_t zone_count);

#endif
