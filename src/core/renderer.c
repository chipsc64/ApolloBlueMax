#include "bm_renderer.h"
#include "bm_profile.h"
uint32_t (*bm_profile_cycles)(void);
uint32_t bm_profile_total[BM_PROFILE_SLOTS],bm_profile_count[BM_PROFILE_SLOTS];
#ifndef BM_RENDERER_SCALED
#define BM_RENDERER_SCALED 0
#endif
#ifndef BM_RENDERER_HD
#define BM_RENDERER_HD 0
#endif
/* RENDERER=classic, scaled or hd (Makefile). */
const BmRenderer *bm_renderer_selected(void){
  if(BM_RENDERER_HD)return &bm_renderer_hd;
  return BM_RENDERER_SCALED?&bm_renderer_scaled:&bm_renderer_classic;
}
