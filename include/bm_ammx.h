#ifndef BM_AMMX_H
#define BM_AMMX_H
#include "bm_sprite.h"
#include <stdint.h>
#if defined(__GNUC__) && defined(__mc68000__)
#define BM_REG(register_name, declaration) declaration __asm(#register_name)
#else
#define BM_REG(register_name, declaration) declaration
#endif
void bm_ammx_copy8(BM_REG(a0,uint8_t *destination),BM_REG(a1,const uint8_t *source),BM_REG(d0,uint32_t bytes));
void bm_ammx_sprite8(BM_REG(a0,uint8_t *destination),BM_REG(a1,const uint8_t *source),BM_REG(d0,uint32_t bytes));
int bm_ammx_selftest(void);
void bm_sprite_blit_ammx(BmFramebuffer *framebuffer,const BmSprite *sprite,int x,int y);
/* 68080 event counters: CPU clock cycles and data cache misses. */
uint32_t bm_cpu_cycles(void);
uint32_t bm_cpu_cache_misses(void);
#endif
