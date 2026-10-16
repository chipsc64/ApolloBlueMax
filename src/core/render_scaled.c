#include "bm_renderer.h"
#include "bm_profile.h"
/* RENDERER=scaled: the classic picture at 640x400 in 16-bit colour, every
   Atari pixel 2x2.  It tests the direct-colour, triple-buffered display
   path without new graphics; an upgraded renderer replaces it. */
static BmFramebuffer classic;

static uint16_t rgb565(uint32_t rgb){
  return (uint16_t)(((rgb>>8)&0xf800u)|((rgb>>5)&0x07e0u)|((rgb>>3)&0x001fu));
}

void bm_row_scale_c(const BmRowJob *job){
  uint32_t x;
  for(x=0;x<job->count;++x){
    uint32_t v=job->pairs[job->in[x]];job->top[x]=v;job->bottom[x]=v;
  }
}
BmRowConverter bm_row_scale=bm_row_scale_c;

static void scaled_render(const BmGame *g,BmFrame *frame){
  BmFrame source={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&classic,0,0u};
  uint32_t pair[BM_PALETTE_SIZE];
  BmRowJob job;
  unsigned y,i;
  uint32_t profile;
  bm_renderer_classic.render(g,&source);
  profile=bm_profile_start();
  for(i=0;i<BM_PALETTE_SIZE;++i){
    uint32_t c=rgb565(classic.palette[i]);pair[i]=c<<16|c;
  }
  job.pairs=pair;job.line_top=job.line_bottom=0;job.phase=0u;job.count=BM_SCREEN_WIDTH;
  for(y=0;y<BM_SCREEN_HEIGHT;++y){
    job.in=classic.pixels+y*BM_SCREEN_WIDTH;
    job.top=(uint32_t *)((uint8_t *)frame->pixels+(2u*y)*frame->stride);
    job.bottom=(uint32_t *)((uint8_t *)frame->pixels+(2u*y+1u)*frame->stride);
    bm_row_scale(&job);
  }
  bm_profile_stop(BM_PROFILE_CONVERT,profile);
}
const BmRenderer bm_renderer_scaled={
  "scaled",{640u,400u,BM_PIXEL_RGB565},scaled_render
};
