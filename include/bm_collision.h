#ifndef BM_COLLISION_H
#define BM_COLLISION_H
#include <stdint.h>
/* Collision masks: the shapes as the Atari's GTIA and ANTIC see them.
   Collisions, hits and the playfield colour under an object are always
   tested against these Atari-resolution masks (16 pixels wide, one byte
   per pixel, 0 = transparent), never against what the renderer draws.
   They come from the original graphics and stay so when the display is
   upgraded; the renderer keeps its own images (bm_asset_*). */
enum { BM_MASK_WIDTH=16, BM_MASK_PLANE_HEIGHT=10, BM_MASK_SHADOW_HEIGHT=8 };

/* The player's P0: level, or banked (bank<0 left, >0 right); frame 0/1 is
   the propeller phase. */
const uint8_t *bm_mask_player(int8_t bank,uint8_t frame);
/* The enemy aircraft's P2.  Composite images carry P3 pixels as another
   value: a pixel is set when it equals *p2_value, or is non-zero when
   *p2_value is 0. */
const uint8_t *bm_mask_enemy_fighter(uint8_t forward,uint8_t turn,int *height,
                                     uint8_t *p2_value);
/* The ship (P2 and P3, the second two lines lower): 1 if (x,y) is set. */
uint8_t bm_mask_ship(int x,int y);
/* The river vehicle: body rows 0..height-1, tyres rows 1..height. */
void bm_mask_vehicle(uint8_t truck,uint8_t left,const uint8_t **body,
                     const uint8_t **tyres,int *height);
/* The shadow's P1 image (SHDW) before SHCOL/TURN change rows. */
const uint8_t *bm_mask_shadow(void);
/* ANTIC mode 4 glyphs (128 x 8 bytes) for the playfield colour under a
   pixel: the runtime charset with the tank or the hangar glyphs. */
const uint8_t *bm_mask_playfield_charset(uint8_t tank_charset_loaded);
#endif
