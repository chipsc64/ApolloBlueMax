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
/* Converting one classic line (320 palette indices) into two 640-pixel
   RGB565 lines, as pixel pairs: one uint32_t per Atari pixel and line.
   pairs: the colour of each index twice.  For hd, line_top/line_bottom
   give per index this line's texture row (8 pairs) or 0, and phase is the
   pixel's place in its tile at x = 0.  The C versions are the reference;
   a platform may install faster ones (AMMX on the Apollo). */
typedef struct {
  const uint8_t *in;
  uint32_t *top,*bottom;
  const uint32_t *pairs;
  const uint32_t *const *line_top,*const *line_bottom;
  uint32_t phase,count;
} BmRowJob;
typedef void (*BmRowConverter)(const BmRowJob *job);
void bm_row_scale_c(const BmRowJob *job);
void bm_row_hd_c(const BmRowJob *job);
/* 0 restores the C version. */
void bm_render_set_converters(BmRowConverter scale,BmRowConverter hd);
/* The renderer this build uses (Makefile RENDERER=...). */
const BmRenderer *bm_renderer_selected(void);
#endif
