#include "bm_tile.h"
#include "bm_chbas_assets.h"

void bm_atari_pal_install(BmFramebuffer *framebuffer, uint8_t saga_base,
                          const uint8_t atari_registers[BM_CLASSIC_COLOUR_COUNT]) {
  unsigned index;
  if (saga_base > BM_PALETTE_SIZE - BM_CLASSIC_COLOUR_COUNT) return;
  for (index = 0; index < BM_CLASSIC_COLOUR_COUNT; ++index)
    framebuffer->palette[saga_base + index] = bm_atari_pal_rgb[atari_registers[index]];
}

static const BmAntic4Colours *colours_for_y(const BmAntic4Zone *zones,
                                            size_t zone_count, int y) {
  size_t index;
  const BmAntic4Colours *result;
  if (!zone_count) return 0;
  result = &zones[0].colours;
  for (index = 1; index < zone_count && y >= zones[index].y; ++index)
    result = &zones[index].colours;
  return result;
}

void bm_antic4_blit(BmFramebuffer *framebuffer, const uint8_t charset[1024],
                    const uint8_t *screen_codes, uint16_t columns, uint16_t rows,
                    uint16_t screen_stride, int16_t x, int16_t y,
                    const BmAntic4Zone *zones,
                    size_t zone_count) {
  uint16_t tile_x, tile_y;
  if (!framebuffer || !charset || !screen_codes || !zones || !zone_count ||
      screen_stride < columns) return;
  for (tile_y = 0; tile_y < rows; ++tile_y) {
    unsigned glyph_y;
    for (glyph_y = 0; glyph_y < 8; ++glyph_y) {
      int destination_y = y + (int)tile_y * 8 + (int)glyph_y;
      const BmAntic4Colours *colours;
      if ((unsigned)destination_y >= BM_SCREEN_HEIGHT) continue;
      colours = colours_for_y(zones, zone_count, destination_y);
      for (tile_x = 0; tile_x < columns; ++tile_x) {
        uint8_t screen_code = screen_codes[(size_t)tile_y * screen_stride + tile_x];
        uint8_t glyph = charset[(size_t)(screen_code & 0x7fu) * 8 + glyph_y];
        unsigned pair;
        for (pair = 0; pair < 4; ++pair) {
          uint8_t value = (uint8_t)((glyph >> (6 - pair * 2)) & 3u);
          uint8_t colour = value == 0 ? colours->colbk :
                           value == 1 ? colours->colpf0 :
                           value == 2 ? colours->colpf1 :
                           ((screen_code & 0x80u) ? colours->colpf3 : colours->colpf2);
          int destination_x = x + (int)tile_x * 8 + (int)pair * 2;
          if ((unsigned)destination_x < BM_SCREEN_WIDTH)
            framebuffer->pixels[destination_y * BM_SCREEN_WIDTH + destination_x] = colour;
          ++destination_x;
          if ((unsigned)destination_x < BM_SCREEN_WIDTH)
            framebuffer->pixels[destination_y * BM_SCREEN_WIDTH + destination_x] = colour;
        }
      }
    }
  }
}

int bm_antic4_blit_playfield(BmFramebuffer *framebuffer,
                             const uint8_t charset[1024], const uint8_t *world,
                             uint16_t source_column, uint8_t fine_phase,
                             int16_t x, int16_t y, const BmAntic4Zone *zones,
                             size_t zone_count) {
  int16_t fine_x, fine_y;
  if (!world || fine_phase >= BM_ATARI_FINE_SCROLL_PHASES ||
      source_column + BM_ATARI_PLAYFIELD_FETCH_COLUMNS > BM_ATARI_WORLD_STRIDE)
    return -1;
  /* The supplied fetch row starts one byte before the LMS origin.  HSCROL
     3,2,1,0 therefore places that extra tile at -2,-4,-6,-8; after LMS +1
     the old origin continues two pixels left without exposing the border. */
  fine_x = (int16_t)(fine_phase * 2 - 8);
  fine_y = (int16_t)(-(int16_t)fine_phase * 2);
  bm_antic4_blit(framebuffer, charset, world + source_column,
                 BM_ATARI_PLAYFIELD_FETCH_COLUMNS, BM_ATARI_PLAYFIELD_FETCH_ROWS,
                 BM_ATARI_WORLD_STRIDE, (int16_t)(x + fine_x),
                 (int16_t)(y + fine_y), zones, zone_count);
  return 0;
}
