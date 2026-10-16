#include "bm_renderer.h"
#include "bm_profile.h"
/* RENDERER=scaled: the classic picture at 640x400 in 16-bit colour, every
   Atari pixel 2x2.  It tests the direct-colour, triple-buffered display
   path without new graphics; an upgraded renderer replaces it. */
static BmFramebuffer classic;

static uint16_t rgb565(uint32_t rgb){
  return (uint16_t)(((rgb>>8)&0xf800u)|((rgb>>5)&0x07e0u)|((rgb>>3)&0x001fu));
}

static void scaled_render(const BmGame *g,BmFrame *frame){
  BmFrame source={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&classic,0,0u};
  uint32_t pair[BM_PALETTE_SIZE];
  unsigned x,y,i;
  uint32_t profile=bm_profile_start();
  bm_renderer_classic.render(g,&source);
  bm_profile_stop(BM_PROFILE_CLASSIC,profile);profile=bm_profile_start();
  for(i=0;i<BM_PALETTE_SIZE;++i){
    uint32_t c=rgb565(classic.palette[i]);pair[i]=c<<16|c;
  }
  for(y=0;y<BM_SCREEN_HEIGHT;++y){
    const uint8_t *in=classic.pixels+y*BM_SCREEN_WIDTH;
    uint32_t *top=(uint32_t *)((uint8_t *)frame->pixels+(2u*y)*frame->stride);
    uint32_t *bottom=(uint32_t *)((uint8_t *)frame->pixels+(2u*y+1u)*frame->stride);
    for(x=0;x<BM_SCREEN_WIDTH;++x){uint32_t v=pair[in[x]];top[x]=v;bottom[x]=v;}
  }
  bm_profile_stop(BM_PROFILE_CONVERT,profile);
}
const BmRenderer bm_renderer_scaled={
  "scaled",{640u,400u,BM_PIXEL_RGB565},scaled_render
};
