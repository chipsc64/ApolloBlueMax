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
/* The objects the classic renderer draws, one entry each per frame, so
   that an upgraded renderer can draw them its own way.  x/y: top-left of
   the 16-pixel-wide Atari image in the 320x200 picture.  colour: the
   object's Atari colour where it matters (P2 objects), else 0.  behind:
   drawn behind the playfield (GPRIOR 4).  Kinds set in bm_sprite_hd_mask
   are recorded but not drawn by the classic renderer. */
typedef enum {
  BM_SPRITE_PLAYER,        /* variant: bank (0 level, 1 left, 2 right) * 2 + propeller frame */
  BM_SPRITE_SHADOW,        /* variant: 0 normal, 1 the thin far shadow (SHCOL), 2 banking (TURN) */
  BM_SPRITE_PLAYER_CRASH,  /* variant: 0 = P0 half, 1 = P1 half */
  BM_SPRITE_PLAYER_SHOT,
  BM_SPRITE_BOMB,          /* variant: the narrow end's phase */
  BM_SPRITE_BOMB_EXPLOSION,/* variant: 1 = water splash */
  BM_SPRITE_SHIP,          /* variant: 1 = destroyed (one line higher) */
  BM_SPRITE_VEHICLE,       /* variant: 1 truck, 2 moving left, 4 destroyed (one line higher) */
  BM_SPRITE_ENEMY,         /* variant: 1 forward, 2/4 turn left/right, 8 wreck (one line higher), 16 crash colour */
  BM_SPRITE_ENEMY_SHADOW,  /* variant: 1 = PLFWD's P3 layer */
  BM_SPRITE_ENEMY_SHOT,
  BM_SPRITE_GUNSIGHT,
  BM_SPRITE_GUN_SHOT,      /* variant: 1 = shell burst */
  BM_SPRITE_CAPITAL_TARGET,/* variant: 0 intact (from the P3 top), 1 wreck */
  BM_SPRITE_KINDS
} BmSpriteKind;
#ifndef BM_SMOOTH_SCROLL
#define BM_SMOOTH_SCROLL 0
#endif
/* SMOOTH_SCROLL: the playfield's horizontal offset this update (1 when it
   took its scroll step: half a step back; the vertical one is -1 then). */
static inline int bm_render_smooth_dx(const BmWorldBuffer *w){
  return BM_SMOOTH_SCROLL&&w->scrolled?1:0;
}
/* wide: every image column twice (the capital wreck's SIZEP2 3 phase);
   bottom: the first screen line the object no longer reaches.
   left..below: while bm_sprite_hd_mask is set, the pixels the classic
   renderer itself drew for this object (left > right: none). */
typedef struct {
  uint8_t kind,variant,colour,behind,wide; int16_t x,y,bottom;
  int16_t left,top,right,below;
} BmSpriteDraw;
enum { BM_SPRITE_DRAW_MAX=32 };
typedef struct { BmSpriteDraw draw[BM_SPRITE_DRAW_MAX]; unsigned count; } BmSpriteList;
extern BmSpriteList bm_sprite_list;
extern uint32_t bm_sprite_hd_mask;
/* While bm_sprite_hd_mask is set: per classic pixel, 1 + the list entry
   that drew it last (0: no object), so an upgraded renderer can put a
   classic object back over an upgraded one drawn before it. */
extern uint8_t bm_sprite_owner[BM_SCREEN_WIDTH*BM_SCREEN_HEIGHT];
/* The kinds the hd renderer draws from art/sprites (render_hd.c); every
   complete kind by default. */
extern uint32_t bm_hd_sprite_kinds;

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
