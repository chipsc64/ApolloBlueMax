#include "bm_game.h"
#include <stdio.h>
#include <string.h>
typedef struct{unsigned frames;} State;static State state;
static int openp(BmPlatform*p,const BmFramebuffer*f){(void)f;memset(&state,0,sizeof(state));p->userdata=&state;return 0;}
static void poll(BmPlatform*p,BmInput*i){State*s=(State*)p->userdata;memset(i,0,sizeof(*i));i->x=(s->frames/60u)&1u?-1:1;i->quit=s->frames>=300u;}
static void present(BmPlatform*p,const BmFramebuffer*f){State*s=(State*)p->userdata;if(s->frames%50u==0)printf("frame=%u checksum=%08lx\n",s->frames,(unsigned long)bm_framebuffer_checksum(f));s->frames++;}
static void noop(BmPlatform*p){(void)p;}
static unsigned onetick(BmPlatform*p){(void)p;return 1u;}
static void noaudio(BmPlatform*p,BmPokey*const*k,unsigned n){(void)p;(void)k;(void)n;}
BmPlatform bm_platform_create(void){BmPlatform p={0};p.open=openp;p.poll_input=poll;p.present=present;p.audio_tick=noaudio;p.wait_vblank=onetick;p.close=noop;return p;}
