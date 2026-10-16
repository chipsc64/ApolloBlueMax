#include "bm_game.h"
#include "bm_frame.h"
#include <stdio.h>
#include <string.h>
/* Headless backend: prints a checksum of every 50th frame.  Direct colour
   frames are drawn into a buffer here, as the SAGA backend's video memory. */
typedef struct{unsigned frames;} State;static State state;
static uint8_t direct[640u*400u*4u];
static uint32_t direct_bytes(const BmFrame*f){
  return (uint32_t)f->format.width*f->format.height*(f->format.pixel==BM_PIXEL_RGB565?2u:4u);
}
static int openp(BmPlatform*p,const BmFrame*f){
  if(f->format.pixel!=BM_PIXEL_CLUT8&&direct_bytes(f)>sizeof(direct))return -1;
  memset(&state,0,sizeof(state));p->userdata=&state;return 0;
}
static void poll(BmPlatform*p,BmInput*i){State*s=(State*)p->userdata;memset(i,0,sizeof(*i));i->x=(s->frames/60u)&1u?-1:1;i->quit=s->frames>=300u;}
static void acquire(BmPlatform*p,BmFrame*f){
  (void)p;
  if(f->format.pixel==BM_PIXEL_CLUT8)return;
  f->pixels=direct;f->stride=(uint32_t)f->format.width*(f->format.pixel==BM_PIXEL_RGB565?2u:4u);
}
static uint32_t frame_checksum(const BmFrame*f){
  uint32_t h=2166136261u,i,n;const uint8_t*b=(const uint8_t*)f->pixels;
  if(f->format.pixel==BM_PIXEL_CLUT8)return bm_framebuffer_checksum(f->indexed);
  for(i=0,n=direct_bytes(f);i<n;++i){h^=b[i];h*=16777619u;}
  return h;
}
static void present(BmPlatform*p,const BmFrame*f){State*s=(State*)p->userdata;if(s->frames%50u==0)printf("frame=%u checksum=%08lx\n",s->frames,(unsigned long)frame_checksum(f));s->frames++;}
static void noop(BmPlatform*p){(void)p;}
static unsigned onetick(BmPlatform*p){(void)p;return 1u;}
static void noaudio(BmPlatform*p,BmPokey*const*k,unsigned n){(void)p;(void)k;(void)n;}
BmPlatform bm_platform_create(void){BmPlatform p={0};p.open=openp;p.poll_input=poll;p.acquire=acquire;p.present=present;p.audio_tick=noaudio;p.wait_vblank=onetick;p.close=noop;return p;}
