#include "bm_renderer.h"
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
extern BmRowConverter bm_row_scale,bm_row_hd;
void bm_render_set_converters(BmRowConverter scale,BmRowConverter hd){
  bm_row_scale=scale?scale:bm_row_scale_c;
  bm_row_hd=hd?hd:bm_row_hd_c;
}
