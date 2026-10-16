#ifndef BM_FRAME_H
#define BM_FRAME_H
#include <stdint.h>
#include "bm_platform.h"
/* A rendered picture as the platform shows it.  The classic renderer makes
   the Atari's 320x200 picture with 256 palette indices (CLUT8); upgraded
   renderers may ask for a larger frame with direct colour. */
typedef enum { BM_PIXEL_CLUT8, BM_PIXEL_RGB565, BM_PIXEL_ARGB32 } BmPixelFormat;
typedef struct { uint16_t width,height; BmPixelFormat pixel; } BmFrameFormat;
typedef struct BmFrame {
  BmFrameFormat format;
  /* CLUT8 320x200: pixels and palette in a BmFramebuffer. */
  BmFramebuffer *indexed;
  /* Direct colour formats: stride in bytes. */
  void *pixels;
  uint32_t stride;
} BmFrame;
#endif
