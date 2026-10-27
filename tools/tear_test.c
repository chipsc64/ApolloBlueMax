/* Tearing test for the Apollo: the game's own display code (triple
   buffering, the delayed pointer swap after the vertical blank), with
   moving vertical bars instead of the game.

   Three white bars cross a dark screen at 4, 8 and 16 pixels per frame.
   A bar that stays one straight line means every frame is shown whole; a
   bar that breaks at some height means the display switched to the next
   frame while it was drawing it (tearing).  The top and bottom thirds have
   a different shade so a break is easy to place.  Q (or 60 s) ends it.

   F4 / joystick up: swap 0.25 ms later; F3 / joystick down: earlier.
   White squares top left: the delay, one per 0.25 ms (it starts at the
   game's default).  Find the delays at which the break leaves and comes
   back into the picture; the last one is printed on exit. */
#include "bm_platform.h"
#include "bm_frame.h"
#include <stdio.h>
#include <string.h>

extern BmPlatform bm_platform_create(void);     /* as in src/main.c */
extern uint32_t bm_swap_delay_us;               /* platform_saga.c */

enum { W = 640, H = 400, BAR = 12, SECONDS = 60 };

static void fill_rows(uint16_t *pixels, uint32_t stride, int y0, int y1, uint16_t c){
  int x, y;
  for (y = y0; y < y1; ++y) {
    uint16_t *row = (uint16_t *)((uint8_t *)pixels + (uint32_t)y * stride);
    for (x = 0; x < W; ++x) row[x] = c;
  }
}
static void squares(uint16_t *pixels, uint32_t stride, unsigned n){
  unsigned k; int x, y;
  for (k = 0; k < n; ++k) {
    int x0 = 8 + (int)(k % 40u) * 14, y0 = 8 + (int)(k / 40u) * 14;
    for (y = y0; y < y0 + 10; ++y) {
      uint16_t *row = (uint16_t *)((uint8_t *)pixels + (uint32_t)y * stride);
      for (x = x0; x < x0 + 10; ++x) row[x] = 0xffff;
    }
  }
}
static void bar(uint16_t *pixels, uint32_t stride, int x0, uint16_t c){
  int x, y;
  for (y = 0; y < H; ++y) {
    uint16_t *row = (uint16_t *)((uint8_t *)pixels + (uint32_t)y * stride);
    for (x = x0; x < x0 + BAR; ++x) row[(unsigned)x % W] = c;
  }
}

int main(void){
  BmPlatform platform = bm_platform_create();
  BmFrame frame = {{W, H, BM_PIXEL_RGB565}, 0, 0, 0u};
  BmInput input;
  unsigned long n;
  memset(&input, 0, sizeof(input));
  if (!platform.open || platform.open(&platform, &frame) != 0) return 1;
  for (n = 0; !input.quit && n < 60ul * SECONDS; ++n) {
    uint16_t *p;
    uint8_t later, earlier;
    static uint8_t was_later, was_earlier;
    platform.poll_input(&platform, &input);
    later = (uint8_t)(input.start || input.y < 0);
    earlier = (uint8_t)(input.select || input.y > 0);
    if (later && !was_later && bm_swap_delay_us < 16000u) bm_swap_delay_us += 250u;
    if (earlier && !was_earlier && bm_swap_delay_us) bm_swap_delay_us -= 250u;
    was_later = later; was_earlier = earlier;
    if (platform.acquire) platform.acquire(&platform, &frame);
    p = (uint16_t *)frame.pixels;
    fill_rows(p, frame.stride, 0, H / 3, 0x2104);              /* dark grey */
    fill_rows(p, frame.stride, H / 3, 2 * H / 3, 0x0010);      /* dark blue */
    fill_rows(p, frame.stride, 2 * H / 3, H, 0x2104);
    bar(p, frame.stride, (int)((n * 4u) % W), 0xffff);
    bar(p, frame.stride, (int)((n * 8u) % W), 0xffe0);         /* yellow */
    bar(p, frame.stride, (int)((n * 16u) % W), 0xf800);        /* red */
    squares(p, frame.stride, bm_swap_delay_us / 250u);
    platform.present(&platform, &frame);
    platform.wait_vblank(&platform);
  }
  platform.close(&platform);
  printf("Blue Max tear test: %lu frames, swap delay %lu us\n", n,
         (unsigned long)bm_swap_delay_us);
  return 0;
}
