#ifndef BM_SPRITE_H
#define BM_SPRITE_H

#include "bm_platform.h"
#include <stdint.h>

typedef struct {
  const uint8_t *pixels;
  uint16_t width,height,stride;
  int16_t anchor_x,anchor_y;
} BmSprite;

typedef void (*BmSpriteBlitter)(BmFramebuffer *,const BmSprite *,int,int);

/* Draw at anchor position (x,y). Palette index 0 is transparent. */
void bm_sprite_blit_c(BmFramebuffer *framebuffer,const BmSprite *sprite,int x,int y);
void bm_sprite_blit(BmFramebuffer *framebuffer,const BmSprite *sprite,int x,int y);
void bm_sprite_set_blitter(BmSpriteBlitter blitter);
void bm_sprite_blit_colour(BmFramebuffer *framebuffer,const BmSprite *sprite,
                           int x,int y,uint8_t colour);

#endif
