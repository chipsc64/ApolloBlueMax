#include "bm_renderer.h"
#include <stdio.h>
#include <string.h>
/* The renderers: the selected one is classic by default, and the scaled
   renderer shows exactly the classic picture, 2x2 per pixel in RGB565. */
static BmGame game;
static uint16_t scaled[640u*400u];
static int failures;
static void check(int ok,const char *what){if(!ok){printf("FAIL: %s\n",what);++failures;}}
static uint16_t rgb565(uint32_t c){
  return (uint16_t)(((c>>8)&0xf800u)|((c>>5)&0x07e0u)|((c>>3)&0x001fu));
}
int main(void){
  BmInput in;unsigned f,x,y,bad=0;
  BmFrame classic={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&game.framebuffer,0,0u};
  BmFrame big={{640u,400u,BM_PIXEL_RGB565},0,scaled,640u*2u};
  check(bm_renderer_selected()==&bm_renderer_classic,"classic is the default renderer");
  check(bm_renderer_scaled.format.width==640u&&bm_renderer_scaled.format.height==400u&&
        bm_renderer_scaled.format.pixel==BM_PIXEL_RGB565,"scaled format 640x400 RGB565");
  memset(&in,0,sizeof in);bm_game_boot(&game,7u);
  for(f=0;f<3000u;++f){
    in.fire=(f%300u)<20u;bm_game_tick(&game,&in);
    if(f%500u)continue;
    bm_renderer_classic.render(&game,&classic);
    bm_renderer_scaled.render(&game,&big);
    for(y=0;y<400u;++y)for(x=0;x<640u;++x){
      uint8_t index=game.framebuffer.pixels[(y/2u)*BM_SCREEN_WIDTH+x/2u];
      if(scaled[y*640u+x]!=rgb565(game.framebuffer.palette[index]))++bad;
    }
  }
  check(!bad,"scaled picture = classic picture 2x2");
  if(failures){printf("%d render test(s) failed\n",failures);return 1;}
  printf("render tests passed\n");return 0;
}
