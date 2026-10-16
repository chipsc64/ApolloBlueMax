#ifndef BM_RENDERER_H
#define BM_RENDERER_H
#include "bm_frame.h"
#include "bm_game.h"
/* A renderer draws the game state into a frame of its own format.  It only
   reads the game: collisions use the masks of bm_collision.h, so a
   renderer with other graphics cannot change the gameplay. */
typedef struct {
  const char *name;
  BmFrameFormat format;
  void (*render)(const BmGame *game,BmFrame *frame);
} BmRenderer;
/* The Atari picture: 320x200, CLUT8 (render.c). */
extern const BmRenderer bm_renderer_classic;
/* The classic picture at 640x400 RGB565, 2x2 per pixel (render_scaled.c). */
extern const BmRenderer bm_renderer_scaled;
/* The classic picture at 640x400 RGB565 with the art/materials textures on
   the playfield (render_hd.c). */
extern const BmRenderer bm_renderer_hd;
/* The renderer this build uses (Makefile RENDERER=...). */
const BmRenderer *bm_renderer_selected(void);
#endif
