#include "bm_pokey.h"
#include "bm_sound.h"
#include <stdio.h>
#include <stdlib.h>

enum { RATE=44336, SECOND=RATE };
static int16_t buffer[SECOND];
static int failures;
static void check(int ok,const char *what){if(!ok){printf("FAIL: %s\n",what);++failures;}}

/* Rising edges per second of a square wave = its frequency. */
static unsigned edges(const int16_t *s,unsigned n){
  unsigned i,count=0;int high=s[0]>0;
  for(i=1;i<n;++i){int h=s[i]>3000;if(h&&!high)++count;if(s[i]<1000)high=0;else if(h)high=1;}
  return count;
}
static void render(BmPokey *k){bm_pokey_render(k,buffer,SECOND,RATE);}
static int near(unsigned value,double expected){return value+2u>=expected&&value<=expected+2.0;}

int main(void){
  BmPokey k;unsigned i,on=0;long sum=0;int16_t lo=32767,hi=-32768;
  bm_pokey_reset(&k);render(&k);
  for(i=0;i<SECOND;++i)if(buffer[i])++on;
  check(on==0u,"silence after reset");

  /* 64 kHz clock: f = 63921/(2*(AUDF+1)). */
  bm_pokey_write(&k,BM_POKEY_AUDF1,0x50u);bm_pokey_write(&k,BM_POKEY_AUDC1,0xa8u);
  render(&k);check(near(edges(buffer,SECOND),1789773.0/28/2/81),"pure tone AUDF=$50");
  for(i=0;i<SECOND;++i){if(buffer[i]<lo)lo=buffer[i];if(buffer[i]>hi)hi=buffer[i];}
  check(lo==0&&hi==8*2*256,"pure tone levels 0 and volume 8");

  /* 15 kHz clock (AUDCTL bit 0): f = 15700/(2*(AUDF+1)). */
  bm_pokey_write(&k,BM_POKEY_AUDCTL,0x01u);bm_pokey_write(&k,BM_POKEY_AUDF1,9u);
  render(&k);check(near(edges(buffer,SECOND),1789773.0/114/2/10),"15 kHz clock");

  /* 16-bit pair 1+2 at 1.79 MHz: f = 1789773/(2*(N+7)). */
  bm_pokey_write(&k,BM_POKEY_AUDC1,0u);bm_pokey_write(&k,BM_POKEY_AUDCTL,0x50u);
  bm_pokey_write(&k,BM_POKEY_AUDF1,0xebu);bm_pokey_write(&k,BM_POKEY_AUDF2,0x07u);
  bm_pokey_write(&k,BM_POKEY_AUDC2,0xa8u);
  render(&k);check(near(edges(buffer,SECOND),1789773.0/2/(0x07eb+7)),"16-bit pair 440 Hz");

  /* Volume only: a constant level. */
  bm_pokey_write(&k,BM_POKEY_AUDCTL,0u);bm_pokey_write(&k,BM_POKEY_AUDC2,0u);
  bm_pokey_write(&k,BM_POKEY_AUDC4,0x1fu);render(&k);
  for(on=0,i=0;i<SECOND;++i)if(buffer[i]!=15*2*256)++on;
  check(on==0u,"volume only");

  /* Poly17 noise: about half on, no steady period. */
  bm_pokey_write(&k,BM_POKEY_AUDC4,0u);
  bm_pokey_write(&k,BM_POKEY_AUDF1,0x10u);bm_pokey_write(&k,BM_POKEY_AUDC1,0x88u);
  render(&k);
  for(sum=0,i=0;i<SECOND;++i)sum+=buffer[i];
  check(sum/SECOND>8*2*256/3&&sum/SECOND<8*2*256*2/3,"poly17 noise average");

  /* High-pass 1 by 3, dividers started together at the same rate: the
     filter cancels the tone. */
  bm_pokey_reset(&k);bm_pokey_write(&k,BM_POKEY_AUDCTL,0x04u);bm_pokey_write(&k,BM_POKEY_AUDC1,0xa8u);
  bm_pokey_write(&k,BM_POKEY_AUDF1,0x40u);bm_pokey_write(&k,BM_POKEY_AUDF3,0x40u);
  render(&k);
  for(on=0,i=SECOND/2;i<SECOND;++i)if(buffer[i]>8*256)++on;
  check(on<SECOND/50,"high-pass with equal dividers is nearly silent");

  /* EXXON.6 XXX 15,$48 + XSND: AUDC3=$48; from count 15 the volume falls
     every fourth VBI until $40, then $644=0. */
  { BmSound snd;unsigned n=0;
    bm_pokey_reset(&k);bm_sound_reset(&snd);
    bm_sound_xxx(&snd,&k,15u,0x48u);
    check(k.audc[2]==0x48u&&snd.xp_count==1u,"XXX starts the explosion");
    while(snd.xp_count&&n<200u){bm_sound_xsnd(&snd,&k);++n;}
    check(k.audc[2]==0x40u,"XSND decays to volume 0");
    check(n==43u,"XSND length: 14 to the limit, 7x4 to $40, one to stop");
    /* GGNSND from $6AF=12: on, off, on, off, ..., then $45 below 4. */
    bm_sound_reset(&snd);snd.ground_gun=12u;
    bm_sound_ggnsnd(&snd,&k);check(k.audc[3]==0x67u&&k.audf[3]==60u,"GGNSND 11 on");
    bm_sound_ggnsnd(&snd,&k);check(k.audc[3]==0x67u,"GGNSND 10 on");
    bm_sound_ggnsnd(&snd,&k);check(k.audc[3]==0u,"GGNSND 9 off");
    for(n=0;n<6u;++n)bm_sound_ggnsnd(&snd,&k);
    check(snd.ground_gun==3u&&k.audc[3]==0x45u,"GGNSND tail $45");
    bm_sound_ggnsnd(&snd,&k);bm_sound_ggnsnd(&snd,&k);bm_sound_ggnsnd(&snd,&k);
    check(!snd.ground_gun&&k.audc[3]==0u,"GGNSND ends silent");
    /* ROAR: every 20 calls one step towards $C4, or up to $C8 on stick 6. */
    bm_sound_reset(&snd);snd.engine=0xc1u;
    for(n=0;n<2u+20u*5u;++n)bm_sound_roar(&snd,&k,15u);
    check(snd.engine==0xc4u&&k.audc[0]==0xc4u,"ROAR settles at $C4");
    for(n=0;n<20u*6u;++n)bm_sound_roar(&snd,&k,6u);
    check(snd.engine==0xc8u,"ROAR stick 6 reaches $C8");
    /* SPLASH ($64D=$2D): AUDF2 rises above 14, falls 7..13, then AUDC2
       falls from $83 below 3; 45 calls in all. */
    bm_sound_reset(&snd);bm_sound_splash_start(&snd,&k);
    check(k.audc[1]==0x83u&&k.audf[1]==0u,"SPLASH starts");
    bm_sound_splash(&snd,&k);bm_sound_splash(&snd,&k);
    check(k.audf[1]==1u,"SPLASH pitch step");
    for(n=0;n<43u;++n)bm_sound_splash(&snd,&k);
    check(!snd.splash&&k.audc[1]==0x80u,"SPLASH ends at volume 0");
    /* BOMB + CDRP: the whistle $14/$A2; a running drop sound ends with
       XXX $2A,$4C; DRPSND raises $642 every other call up to 70. */
    bm_sound_reset(&snd);bm_sound_bomb_release(&snd,&k,&k);
    check(k.audf[1]==0x14u&&k.audc[1]==0xa2u,"bomb whistle");
    snd.drop=snd.drop_last;snd.dropping=1u;
    for(n=0;n<200u&&snd.dropping;++n)bm_sound_cdrp(&snd,&k,&k);
    check(!snd.dropping&&k.audc[1]==0u&&snd.xp_limit==0x2au&&snd.drop_last==70u,
          "CDRP sweeps to 70, then the explosion");
    /* RUNSND from $19: AUDF4 10/25 alternate with $A4, then silence. */
    bm_sound_reset(&snd);snd.runway=0x19u;
    bm_sound_runsnd(&snd,&k);check(k.audf[3]==25u&&k.audc[3]==0xa4u,"RUNSND even");
    bm_sound_runsnd(&snd,&k);check(k.audf[3]==10u,"RUNSND odd");
    for(n=0;n<23u;++n)bm_sound_runsnd(&snd,&k);
    check(!snd.runway&&k.audc[3]==0u,"RUNSND ends");
    /* PLAY.IT: the first note $34/$10 is NOTE.TABLE[36+4]=$99 with $A0|8,
       lasting $10+$08 frames; the tune ends after 2713 VBIs with STOP.SN. */
    bm_pokey_reset(&k);bm_sound_reset(&snd);bm_sound_music_start(&snd);
    check(bm_sound_music_tick(&snd,&k)&&k.audf[0]==0x99u&&k.audc[0]==0xa8u&&
          snd.music_duration[0]==24u,"PLAY.IT first note");
    for(n=1;bm_sound_music_tick(&snd,&k);++n){}
    check(n+1u==2713u&&!snd.music_on&&!k.audc[0]&&!k.audc[1]&&!k.audc[2],
          "PLAY.IT length and STOP.SN");
  }
  if(failures){printf("%d pokey test(s) failed\n",failures);return 1;}
  printf("pokey tests passed\n");return 0;
}
