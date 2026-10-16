#include "bm_renderer.h"
#ifndef BM_RENDERER_SCALED
#define BM_RENDERER_SCALED 0
#endif
/* RENDERER=classic or scaled (Makefile). */
const BmRenderer *bm_renderer_selected(void){
  return BM_RENDERER_SCALED?&bm_renderer_scaled:&bm_renderer_classic;
}
