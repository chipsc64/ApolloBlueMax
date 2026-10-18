#include "bm_game.h"
#include "bm_renderer.h"
#ifndef BM_SOUND_TEST
#define BM_SOUND_TEST 0
#endif
#ifndef BM_ENHANCED_SOUND
#define BM_ENHANCED_SOUND 0
#endif
extern BmPlatform bm_platform_create(void);
#if BM_SOUND_TEST
/* Test aid: a fixed POKEY programme, 3 s per step, over the running game.
   1 pure tone 394 Hz, 2 C-E-G chord, 3 white noise (poly17), 4 poly5+poly4
   rumble, 5 16-bit sweep at 1.79 MHz, 6 high-pass 1+3, 7 silence. */
static void sound_test(BmPokey *k){
  static uint32_t frame;
  static const uint8_t chord[3]={60u,47u,40u};
  unsigned step=(unsigned)(frame/150u)%7u,i;
  for(i=0;i<8u;++i)bm_pokey_write(k,i,0u);
  bm_pokey_write(k,BM_POKEY_AUDCTL,0u);
  switch(step){
  case 0:bm_pokey_write(k,BM_POKEY_AUDF1,0x50u);bm_pokey_write(k,BM_POKEY_AUDC1,0xa8u);break;
  case 1:for(i=0;i<3u;++i){bm_pokey_write(k,i*2u,chord[i]);bm_pokey_write(k,i*2u+1u,0xa6u);}break;
  case 2:bm_pokey_write(k,BM_POKEY_AUDF1,0x10u);bm_pokey_write(k,BM_POKEY_AUDC1,0x88u);break;
  case 3:bm_pokey_write(k,BM_POKEY_AUDF1,0x80u);bm_pokey_write(k,BM_POKEY_AUDC1,0x48u);break;
  case 4:{uint16_t f=(uint16_t)(0x0400u+(frame%150u)*20u);
    bm_pokey_write(k,BM_POKEY_AUDCTL,0x50u);
    bm_pokey_write(k,BM_POKEY_AUDF1,(uint8_t)f);bm_pokey_write(k,BM_POKEY_AUDF2,(uint8_t)(f>>8));
    bm_pokey_write(k,BM_POKEY_AUDC2,0xa8u);break;}
  case 5:bm_pokey_write(k,BM_POKEY_AUDCTL,0x04u);
    bm_pokey_write(k,BM_POKEY_AUDF1,0x40u);bm_pokey_write(k,BM_POKEY_AUDC1,0xa8u);
    bm_pokey_write(k,BM_POKEY_AUDF3,(uint8_t)(0x41u+(frame%150u)/10u));break;
  default:break;
  }
  ++frame;
}
#endif
#ifndef BM_SOUND_SAMPLES
#define BM_SOUND_SAMPLES 0
#endif
int main(void) {
  static BmGame game; BmPlatform platform=bm_platform_create(); BmInput input={0};
  BmPokey *const pokeys[6]={&game.pokey,&game.pokey_extra[0],&game.pokey_extra[1],
    &game.pokey_extra[2],&game.pokey_extra[3],&game.pokey_extra[4]};
  const BmRenderer *renderer=bm_renderer_selected();
  BmFrame frame={{0u,0u,BM_PIXEL_CLUT8},0,0,0u};
  unsigned due=1u;
  bm_game_boot(&game,0x424d4158u);
  frame.format=renderer->format;frame.indexed=&game.framebuffer;
  if(!platform.open || platform.open(&platform,&frame)!=0) return 1;
  while(!input.quit) {
    platform.poll_input(&platform,&input);
    while(due--) bm_game_tick(&game,&input);
    if(platform.acquire) platform.acquire(&platform,&frame);
    renderer->render(&game,&frame);
    platform.present(&platform,&frame);
#if BM_SOUND_TEST
    sound_test(&game.pokey);
#endif
    if(platform.audio_tick)
      platform.audio_tick(&platform,pokeys,BM_ENHANCED_SOUND?6u:1u,BM_SOUND_SAMPLES?&game.mixer:0);
    due=platform.wait_vblank(&platform);
  }
  platform.close(&platform); return 0;
}
