#include "bm_pokey.h"
#include "bm_sound.h"
#include "bm_mixer.h"
#include "bm_sound_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  /* T37a: the sample mixer. */
  { static const int16_t ramp[4]={1000,2000,3000,4000};
    static const BmSample once={ramp,4u,RATE},slow={ramp,4u,RATE/2u};
    BmMixer m;int16_t out[12];unsigned n;
    bm_mixer_reset(&m);
    for(n=0;n<12u;++n)out[n]=100;
    bm_mixer_play(&m,0u,&once,0u,256u);bm_mixer_render(&m,out,12u,RATE);
    check(out[0]==1100&&out[3]==4100&&out[4]==100&&!bm_mixer_active(&m,0u),
          "mixer: a sample once, added to the output");
    for(n=0;n<12u;++n)out[n]=0;
    bm_mixer_play(&m,0u,&once,1u,128u);bm_mixer_render(&m,out,10u,RATE);
    check(out[4]==500&&out[9]==1000&&bm_mixer_active(&m,0u),"mixer: loop at half volume");
    bm_mixer_release(&m,0u);bm_mixer_render(&m,out,10u,RATE);
    check(!bm_mixer_active(&m,0u),"mixer: a released loop ends with its pass");
    for(n=0;n<12u;++n)out[n]=0;
    bm_mixer_play(&m,1u,&slow,0u,256u);bm_mixer_render(&m,out,8u,RATE);
    check(out[0]==1000&&out[1]==1000&&out[2]==2000&&out[7]==4000,"mixer: rate conversion");
    for(n=0;n<12u;++n)out[n]=32000;
    bm_mixer_play(&m,1u,&once,0u,256u);bm_mixer_render(&m,out,4u,RATE);
    check(out[0]==32767,"mixer: saturation");
  }
  /* T37a: with a gun_own sample the own gun loops it and channel 4 stays
     silent; the end releases it.  Enemy fire stays POKEY. */
  if(bm_samples[BM_SAMPLE_GUN_OWN]){
    BmSound snd;BmMixer m;BmSoundOut o;unsigned g,n;
    for(g=0;g<BM_SOUND_GROUPS;++g)o.pokey[g]=&k;
    o.separate=0u;o.mixer=&m;
    bm_pokey_reset(&k);bm_sound_reset(&snd);bm_mixer_reset(&m);
    bm_sound_event_own_shot(&snd);
    for(n=0;n<8u;++n)bm_sound_event_update(&snd,&o);
    check(bm_mixer_active(&m,BM_VOICE_GUN)&&m.voice[BM_VOICE_GUN].loop&&k.audc[3]==0u,
          "own gun: sample loops, POKEY channel 4 silent");
    bm_sound_event_shot_end(&snd);bm_sound_event_update(&snd,&o);
    check(!m.voice[BM_VOICE_GUN].loop&&snd.gun==0u&&k.audc[3]==0u,"own gun: released at RO");
    bm_sound_event_enemy_shot(&snd);for(n=0;n<4u;++n)bm_sound_event_update(&snd,&o);
    if(bm_samples[BM_SAMPLE_GUN_ENEMY])
      /* sound.c has its own copy of the (static) sample: compare data. */
      check(snd.gun_sample==2u&&m.voice[BM_VOICE_GUN].sample->length==
            bm_samples[BM_SAMPLE_GUN_ENEMY]->length&&
            !memcmp(m.voice[BM_VOICE_GUN].sample->data,bm_samples[BM_SAMPLE_GUN_ENEMY]->data,64u)&&
            k.audc[3]==0u,"enemy gun: its own sample on the gun voice");
    else check(k.audc[3]!=0u,"enemy gun without a sample stays POKEY");
    o.mixer=0;bm_pokey_reset(&k);bm_sound_reset(&snd);
    bm_sound_event_own_shot(&snd);for(n=0;n<4u;++n)bm_sound_event_update(&snd,&o);
    check(k.audc[3]!=0u,"without the mixer the own gun is POKEY");
  }
  /* T37b: explosions, ground gun, flak, bomb whistle and splash. */
  { BmSound snd;BmMixer m;BmSoundOut o;unsigned g,n;
    for(g=0;g<BM_SOUND_GROUPS;++g)o.pokey[g]=&k;
    o.separate=0u;o.mixer=&m;
    bm_pokey_reset(&k);bm_sound_reset(&snd);bm_mixer_reset(&m);
    if(bm_samples[BM_SAMPLE_EXPLOSION_MEDIUM]){
      bm_sound_event_explosion(&snd,&o,BM_EXPLOSION_AIRCRAFT);
      check(bm_mixer_active(&m,BM_VOICE_EXPLOSION)&&!snd.xp_count&&k.audc[2]==0u,
            "explosion: sample instead of XXX");
    }
    if(bm_samples[BM_SAMPLE_FLAK]){
      snd.xp_count=5u;bm_sound_event_flak(&snd,&o);
      check(bm_mixer_active(&m,BM_VOICE_FLAK),"flak: its sample, even during an explosion");
      snd.xp_count=0u;
    }
    if(bm_samples[BM_SAMPLE_GROUND_GUN]){
      bm_sound_event_ground_gun(&snd);bm_sound_event_update(&snd,&o);
      check(bm_mixer_active(&m,BM_VOICE_GROUND_GUN)&&!snd.ground_gun&&k.audc[3]==0u,
            "ground gun: sample instead of GGNSND");
    }
    if(bm_samples[BM_SAMPLE_BOMB_WHISTLE]&&bm_samples[BM_SAMPLE_EXPLOSION_LARGE]){
      bm_mixer_reset(&m);
      bm_sound_event_bomb_release(&snd,&o);
      check(bm_mixer_active(&m,BM_VOICE_BOMB)&&snd.bomb_sample&&k.audc[1]==0u,
            "bomb: the whistle sample, POKEY channel 2 silent");
      bm_sound_event_bomb_lands(&snd,&o,BM_BOMB_LAND);
      check(!bm_mixer_active(&m,BM_VOICE_BOMB)&&bm_mixer_active(&m,BM_VOICE_EXPLOSION),
            "bomb on land: whistle stops, explosion sample");
      /* Behind the roofs the whistle runs on (CDRP) into the explosion. */
      bm_mixer_reset(&m);bm_sound_reset(&snd);
      bm_sound_event_bomb_release(&snd,&o);snd.drop_last=0x30u;
      bm_sound_event_bomb_lands(&snd,&o,BM_BOMB_BEHIND_ROOFS);
      check(bm_mixer_active(&m,BM_VOICE_BOMB),"bomb behind roofs: the whistle runs on");
      for(n=0;n<200u&&snd.dropping;++n)bm_sound_event_update(&snd,&o);
      check(!snd.dropping&&!bm_mixer_active(&m,BM_VOICE_BOMB)&&
            bm_mixer_active(&m,BM_VOICE_EXPLOSION)&&k.audc[1]==0u,
            "CDRP with samples ends in the explosion sample");
    }
    if(bm_samples[BM_SAMPLE_SPLASH]){
      bm_mixer_reset(&m);bm_sound_reset(&snd);
      bm_sound_event_bomb_release(&snd,&o);bm_sound_event_bomb_lands(&snd,&o,BM_BOMB_WATER);
      check(bm_mixer_active(&m,BM_VOICE_SPLASH)&&!snd.splash&&k.audc[1]==0u,
            "bomb in water: splash sample instead of SPLASH");
    }
    /* Without the mixer every event is the POKEY routine. */
    o.mixer=0;bm_pokey_reset(&k);bm_sound_reset(&snd);
    bm_sound_event_explosion(&snd,&o,BM_EXPLOSION_AIRCRAFT);
    check(snd.xp_count==1u&&k.audc[2]==0x4cu,"without the mixer: XXX");
    bm_sound_event_bomb_release(&snd,&o);
    check(k.audf[1]==0x14u&&k.audc[1]==0xa2u,"without the mixer: the POKEY whistle");
  }
  if(failures){printf("%d pokey test(s) failed\n",failures);return 1;}
  printf("pokey tests passed\n");return 0;
}
