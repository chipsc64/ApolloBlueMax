#ifndef BM_PLATFORM_H
#define BM_PLATFORM_H
#include <stdint.h>
#include "bm_pokey.h"
enum { BM_SCREEN_WIDTH=320, BM_SCREEN_HEIGHT=200, BM_PALETTE_SIZE=256, BM_TICK_HZ=50 };
/* start/select/option: the Atari console keys (CONSOL, location 53279). */
typedef struct { int8_t x,y; uint8_t fire,bomb,pause,quit,start,select,option; } BmInput;
typedef struct {
  uint8_t pixels[BM_SCREEN_WIDTH*BM_SCREEN_HEIGHT];
  uint32_t palette[BM_PALETTE_SIZE];
} BmFramebuffer;
typedef struct BmPlatform BmPlatform;
struct BmFrame;
struct BmPlatform {
  void *userdata;
  /* open picks the display mode for the renderer's frame format. */
  int (*open)(BmPlatform *, const struct BmFrame *);
  void (*poll_input)(BmPlatform *, BmInput *);
  /* acquire hands out the buffer to draw the next frame into (direct
     colour frames are drawn straight into display memory); present queues
     it for the next vblank. */
  void (*acquire)(BmPlatform *, struct BmFrame *);
  void (*present)(BmPlatform *, const struct BmFrame *);
  /* The POKEYs to mix: one, or six with ENHANCED_SOUND. */
  void (*audio_tick)(BmPlatform *, BmPokey *const *, unsigned);
  /* Waits for the next vblank; returns the game updates due (one per
     vblank, or paced to TICK_RATE=50 by the platform clock: 0..2). */
  unsigned (*wait_vblank)(BmPlatform *);
  void (*close)(BmPlatform *);
};
#endif
