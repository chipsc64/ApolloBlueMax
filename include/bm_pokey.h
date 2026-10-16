#ifndef BM_POKEY_H
#define BM_POKEY_H
#include <stdint.h>
/* Software POKEY: the game writes the Atari sound registers ($D200..$D208,
   $D20F) exactly as the 6502 code does; the platform renders them to PCM. */
enum { BM_POKEY_AUDF1=0, BM_POKEY_AUDC1=1, BM_POKEY_AUDF2=2, BM_POKEY_AUDC2=3,
  BM_POKEY_AUDF3=4, BM_POKEY_AUDC3=5, BM_POKEY_AUDF4=6, BM_POKEY_AUDC4=7,
  BM_POKEY_AUDCTL=8, BM_POKEY_SKCTL=15 };
/* Machine cycle rate: NTSC, or PAL with TICK_RATE=50 (a PAL Atari). */
#ifndef BM_TICK_RATE
#define BM_TICK_RATE 60
#endif
#if BM_TICK_RATE == 50
#define BM_POKEY_CLOCK 1773447u
#else
#define BM_POKEY_CLOCK 1789773u
#endif
typedef struct {
  uint8_t audf[4],audc[4],audctl,skctl;
  int32_t counter[4];   /* machine cycles until the channel's next underflow */
  uint8_t out[4];       /* divider flip-flop after the distortion select */
  uint8_t highpass[2];  /* channel 1/2 filter latches, clocked by channel 3/4 */
  uint8_t dirty;        /* a write changed the dividers */
  uint32_t cycle;       /* free-running machine cycle count for the polys */
  uint32_t fraction;    /* 16.16 cycles carried over between samples */
} BmPokey;
void bm_pokey_reset(BmPokey *pokey);
void bm_pokey_write(BmPokey *pokey,unsigned reg,uint8_t value);
/* Mono signed 16-bit samples; all four channels at volume 15 give 30720. */
void bm_pokey_render(BmPokey *pokey,int16_t *out,unsigned count,uint32_t rate);
#endif
