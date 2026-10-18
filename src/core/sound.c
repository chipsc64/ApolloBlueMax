#include "bm_sound.h"
#include "bm_music_data.h"
#include <string.h>

void bm_sound_reset(BmSound *s){memset(s,0,sizeof(*s));s->roar_timer=2u;s->random=1u;}

void bm_sound_xsnd(BmSound *s,BmPokey *k){
  uint8_t random;
  if(!s->xp_count)return;
  s->random=s->random*1664525u+1013904223u;random=(uint8_t)(s->random>>24);
  /* ORA with RANDOM&3, ADC #$10 with the carry left by AND (clear). */
  bm_pokey_write(k,BM_POKEY_AUDF3,(uint8_t)(((random&3u)|s->xp_count)+0x10u));
  ++s->xp_count;
  if(s->xp_count<s->xp_limit)return;
  if(s->xp_volume==0x40u){s->xp_count=0u;return;}   /* PY */
  if(--s->xp_step)return;
  s->xp_step=4u;
  --s->xp_volume;
  bm_pokey_write(k,BM_POKEY_AUDC3,s->xp_volume);
}

void bm_sound_xxx(BmSound *s,BmPokey *k,uint8_t limit,uint8_t volume){
  s->xp_limit=limit;s->xp_volume=volume;
  bm_pokey_write(k,BM_POKEY_AUDC3,volume);
  s->xp_count=1u;s->xp_step=1u;
}

static void engine_down(BmSound *s,BmPokey *k){       /* PO */
  --s->engine;bm_pokey_write(k,BM_POKEY_AUDC1,s->engine);
}
static void engine_up(BmSound *s,BmPokey *k){         /* PS / PR */
  ++s->engine;bm_pokey_write(k,BM_POKEY_AUDC1,s->engine);
}
void bm_sound_roar(BmSound *s,BmPokey *k,uint8_t stick){
  if(--s->roar_timer)return;
  s->roar_timer=20u;
  if(stick==6u){if(s->engine!=0xc8u)engine_up(s,k);return;}   /* PR */
  if(s->engine==0xc4u)return;
  if(s->engine<0xc4u)engine_up(s,k);else engine_down(s,k);
}
void bm_sound_engine_dying(BmSound *s,BmPokey *k){
  if(--s->roar_timer)return;
  s->roar_timer=20u;
  if(s->engine!=0xc0u)engine_down(s,k);
}

void bm_sound_gunsnd(BmSound *s,BmPokey *k){
  if(!s->gun)return;
  --s->gun_timer;
  if(!(s->gun_timer&1u))return;
  bm_pokey_write(k,BM_POKEY_AUDF4,50u);
  if(s->gun==255u){bm_pokey_write(k,BM_POKEY_AUDC4,0x45u);--s->gun;return;}  /* QC */
  if(s->gun==254u){bm_pokey_write(k,BM_POKEY_AUDC4,0u);s->gun=0u;return;}   /* QD */
  --s->gun_toggle;
  bm_pokey_write(k,BM_POKEY_AUDC4,(s->gun_toggle&1u)?0x65u:0x60u);
}

void bm_sound_ggnsnd(BmSound *s,BmPokey *k){
  if(!s->ground_gun)return;
  if(!--s->ground_gun){bm_pokey_write(k,BM_POKEY_AUDC4,0u);return;}
  if(s->ground_gun<4u)bm_pokey_write(k,BM_POKEY_AUDC4,0x45u);
  else if(s->ground_gun&2u)bm_pokey_write(k,BM_POKEY_AUDC4,0x67u);
  else{bm_pokey_write(k,BM_POKEY_AUDC4,0u);return;}
  bm_pokey_write(k,BM_POKEY_AUDF4,60u);
}

void bm_sound_quiet(BmPokey *k){
  bm_pokey_write(k,BM_POKEY_AUDC1,0u);bm_pokey_write(k,BM_POKEY_AUDC2,0u);
  bm_pokey_write(k,BM_POKEY_AUDC3,0u);bm_pokey_write(k,BM_POKEY_AUDC4,0u);
}

void bm_sound_runsnd(BmSound *s,BmPokey *k){
  if(!s->runway)return;
  if(!--s->runway){bm_pokey_write(k,BM_POKEY_AUDC4,0u);return;}   /* QF */
  if(s->runway&1u){bm_pokey_write(k,BM_POKEY_AUDF4,10u);return;}
  bm_pokey_write(k,BM_POKEY_AUDF4,25u);bm_pokey_write(k,BM_POKEY_AUDC4,0xa4u);
}

void bm_sound_warn(BmSound *s,BmPokey *k){
  (void)s;
  bm_pokey_write(k,BM_POKEY_AUDF4,1u);bm_pokey_write(k,BM_POKEY_AUDC4,0x22u);
}

void bm_sound_splash(BmSound *s,BmPokey *k){
  uint8_t a;
  if(!s->splash)return;
  --s->splash;
  if(s->splash&1u)return;
  a=(uint8_t)(s->splash>>1);
  if(a<3u){--s->splash_c;bm_pokey_write(k,BM_POKEY_AUDC2,s->splash_c);return;}  /* QI */
  if(a<7u)return;
  if(a<14u)--s->splash_f;else ++s->splash_f;                                    /* QJ */
  bm_pokey_write(k,BM_POKEY_AUDF2,s->splash_f);                                 /* QK */
}
void bm_sound_splash_start(BmSound *s,BmPokey *k){
  s->splash_f=0u;bm_pokey_write(k,BM_POKEY_AUDF2,0u);
  s->splash_c=0x83u;bm_pokey_write(k,BM_POKEY_AUDC2,0x83u);
  s->splash=0x2du;
}

static void drop_end(BmSound *s,BmPokey *k,BmPokey *explosion){  /* AFF */
  s->dropping=0u;bm_pokey_write(k,BM_POKEY_AUDC2,0u);
  bm_sound_xxx(s,explosion,0x2au,0x4cu);
}
void bm_sound_bomb_release(BmSound *s,BmPokey *k,BmPokey *explosion){
  if(s->dropping)drop_end(s,k,explosion);
  bm_pokey_write(k,BM_POKEY_AUDF2,0x14u);s->drop=0x14u;
  bm_pokey_write(k,BM_POKEY_AUDC2,0xa2u);
  s->splash=0u;
}
void bm_sound_cdrp(BmSound *s,BmPokey *k,BmPokey *explosion){
  if(!s->dropping)return;
  if(s->drop>=70u){drop_end(s,k,explosion);return;}
  if(!(--s->drop_timer&1u))return;                               /* DRPSND */
  ++s->drop;s->drop_last=s->drop;
  bm_pokey_write(k,BM_POKEY_AUDF2,s->drop);bm_pokey_write(k,BM_POKEY_AUDC2,0xa2u);
}

/* EXXON.18 NOTE.TABLE ($51F8) and OCTAVE.OFFSET ($5241). */
static const uint8_t note_table[73]={
  0x01,0x03,0x06,0x09,0x0c,0x0f,0x12,0x16,0x17,0x19,0x1a,0x1c,
  0x1d,0x1f,0x21,0x23,0x25,0x28,0x2a,0x2d,0x2f,0x32,0x35,0x39,
  0x3c,0x40,0x44,0x48,0x4c,0x51,0x55,0x5b,0x60,0x66,0x6c,0x72,
  0x79,0x80,0x88,0x90,0x99,0xa2,0xad,0xb6,0xc1,0xcc,0xd9,0xe6,
  0xf3,0xff,0x6e,0x74,0x7a,0x83,0x89,0x92,0x9b,0xa7,0xad,0xb9,
  0xc5,0xce,0xdd,0xe9,0xf5,0x57,0x5b,0x61,0x66,0x6c,0x73,0x79,
  0x82};
static const uint8_t octave_offset[7]={0,12,24,36,48,60,72};
static const uint8_t *const voice_data[3]={bm_music_voice1,bm_music_voice2,bm_music_voice3};
static const uint8_t voice_volume[3]={8u,5u,3u};    /* ORA #8/#5/#3 */
static const uint8_t voice_release[3]={8u,4u,4u};   /* DD2/DD3/DD4 */

void bm_sound_stop(BmPokey *k){
  unsigned r;for(r=0;r<8u;++r)bm_pokey_write(k,r,0u);
}
void bm_sound_music_start(BmSound *s){
  unsigned v;
  for(v=0;v<3u;++v){
    s->music_duration[v]=1u;s->music_status[v]=1u;s->music_position[v]=0u;
  }
  s->music_on=1u;
}
/* GET.AUDF: rest ($xF) gives 0/0; distortion $A0 below $42, else $C0. */
static uint8_t get_audf(uint8_t note,uint8_t *distortion){
  unsigned index;
  if((note&15u)==15u){*distortion=0u;return 0u;}
  index=(note&15u)+octave_offset[(note>>4)%7u];
  if(index>72u)index=72u;
  *distortion=note<0x42u?0xa0u:0xc0u;
  return note_table[index];
}
static void play_note(BmSound *s,BmPokey *k){
  unsigned v;
  for(v=0;v<3u;++v){
    const uint8_t *data=voice_data[v];
    unsigned audc=v*2u+1u;
    uint8_t note,duration,distortion,audf;
    if(!s->music_status[v])continue;
    if(--s->music_duration[v]){
      /* Release: the last notes of an untied note lose volume.  Voice 1
         tests the volume nibble, voices 2 and 3 the whole AUDC. */
      if(s->music_duration[v]>=voice_release[v]||s->music_tie[v])continue;
      if(v==0u){
        if(s->music_volume[0]&15u)bm_pokey_write(k,audc,--s->music_volume[0]);
        else bm_pokey_write(k,audc,0u);
      }else{
        if(s->music_volume[v])--s->music_volume[v];
        bm_pokey_write(k,audc,s->music_volume[v]);
      }
      continue;
    }
    s->music_tie[v]=0u;
    note=data[s->music_position[v]];
    if(note==0xffu){s->music_status[v]=0u;continue;}
    audf=get_audf(note,&distortion);
    bm_pokey_write(k,audc-1u,audf);
    s->music_volume[v]=distortion?(uint8_t)(distortion|voice_volume[v]):0u;
    bm_pokey_write(k,audc,s->music_volume[v]);
    duration=data[s->music_position[v]+1u];
    if(duration&0x80u){duration&=0x7fu;s->music_tie[v]=1u;}
    s->music_duration[v]=(uint8_t)(duration+(duration>>1));   /* MULT, TEMPO 1 */
    s->music_position[v]=(uint16_t)(s->music_position[v]+2u);
  }
}
uint8_t bm_sound_music_tick(BmSound *s,BmPokey *k){
  if(!s->music_on)return 0u;
  bm_pokey_write(k,BM_POKEY_SKCTL,3u);bm_pokey_write(k,BM_POKEY_AUDCTL,0u);
  play_note(s,k);
  if(s->music_status[0]|s->music_status[1]|s->music_status[2])return 1u;
  s->music_on=0u;bm_sound_stop(k);
  return 0u;
}

/* ---- Game sound events (POKEY implementation) ------------------------- */

static BmPokey *group(const BmSoundOut *o,unsigned g){return o->pokey[g];}
static BmPokey *main_pokey(const BmSoundOut *o){return o->pokey[BM_SOUND_MAIN];}

void bm_sound_event_power_on(BmSound *s,const BmSoundOut *o){
  bm_pokey_write(main_pokey(o),BM_POKEY_SKCTL,3u);
  bm_pokey_write(main_pokey(o),BM_POKEY_AUDF1,251u);
  bm_sound_event_takeoff_engine(s,o);
}
void bm_sound_event_takeoff_engine(BmSound *s,const BmSoundOut *o){       /* AFX */
  BmPokey *k=main_pokey(o);
  bm_pokey_write(k,BM_POKEY_AUDF1,251u);bm_pokey_write(k,BM_POKEY_AUDF3,13u);
  s->engine=0xc1u;bm_pokey_write(k,BM_POKEY_AUDC1,0xc1u);
}
void bm_sound_event_quiet(const BmSoundOut *o){
  unsigned g;for(g=0;g<BM_SOUND_GROUPS;++g)bm_sound_quiet(group(o,g));
  if(o->mixer)bm_mixer_reset(o->mixer);
}
void bm_sound_event_demo_music(BmSound *s,const BmSoundOut *o){
  bm_sound_quiet(main_pokey(o));bm_sound_music_start(s);
}
void bm_sound_event_end_music(BmSound *s){bm_sound_music_start(s);}
uint8_t bm_sound_event_vblank(BmSound *s,const BmSoundOut *o){
  bm_sound_xsnd(s,main_pokey(o));
  return (uint8_t)(s->music_on&&!bm_sound_music_tick(s,main_pokey(o)));
}

/* XXX limit/volume per caller (CRASH, SV, VB, R7, BMCPTL, SW/C9, $1B61). */
static const uint8_t explosion_limit[7] ={80u,15u,50u,50u,70u,2u,0x2au};
static const uint8_t explosion_volume[7]={0x4fu,0x48u,0x4cu,0x4cu,0x4eu,0x47u,0x4cu};
void bm_sound_event_explosion(BmSound *s,const BmSoundOut *o,BmExplosion kind){
  bm_sound_xxx(s,main_pokey(o),explosion_limit[kind],explosion_volume[kind]);
}
void bm_sound_event_flak(BmSound *s,const BmSoundOut *o){
  if(!s->xp_count)bm_sound_event_explosion(s,o,BM_EXPLOSION_FLAK);
}
void bm_sound_event_crash(BmSound *s,const BmSoundOut *o){
  unsigned g;
  bm_sound_event_explosion(s,o,BM_EXPLOSION_CRASH);
  bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,0u);
  for(g=BM_SOUND_MAIN;g<=BM_SOUND_BEEP;++g)bm_pokey_write(group(o,g),BM_POKEY_AUDC4,0u);
  if(o->mixer){bm_mixer_stop(o->mixer,BM_VOICE_GUN);s->gun_sample=0u;}
}

void bm_sound_event_engine(BmSound *s,const BmSoundOut *o,uint8_t stick){
  bm_sound_roar(s,main_pokey(o),stick);
}
void bm_sound_event_engine_dead(BmSound *s,const BmSoundOut *o){
  bm_sound_engine_dying(s,main_pokey(o));
}
void bm_sound_event_engine_idle(BmSound *s,const BmSoundOut *o){         /* GROUND */
  if(s->engine>=0xc2u){--s->engine;bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,s->engine);}
}
void bm_sound_event_attacker(BmSound *s,const BmSoundOut *o,uint8_t flying){ /* PBMB */
  bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,flying?0xc3u:s->engine);
}
uint8_t bm_sound_event_engine_stop_step(BmSound *s,const BmSoundOut *o){  /* STOPL .2 */
  if(s->engine==0xc0u)return 1u;
  --s->engine;bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,s->engine);
  return 0u;
}

void bm_sound_event_enemy_shot(BmSound *s){s->gun=0x30u;}
void bm_sound_event_own_shot(BmSound *s){s->gun=3u;}
void bm_sound_event_shot_end(BmSound *s){s->gun=255u;}
void bm_sound_event_ground_gun(BmSound *s){s->ground_gun=12u;}
void bm_sound_event_runway(BmSound *s){s->runway=0x19u;}

/* SOUND=samples: the sample (or 0) for a sound, and its level (256 = full
   scale) next to the original POKEY sound.  Tune on the V4. */
#ifndef BM_SOUND_SAMPLES
#define BM_SOUND_SAMPLES 0
#endif
#if BM_SOUND_SAMPLES
/* Only here: the m68k compiler keeps unused constant tables. */
#include "bm_sound_assets.h"
typedef char bm_sample_ids_check[(int)BM_SAMPLE_FILES==(int)BM_SAMPLE_COUNT?1:-1];
#endif
static const BmSample *sample(const BmSoundOut *o,BmSampleId id){
#if BM_SOUND_SAMPLES
  return o->mixer?bm_samples[id]:0;
#else
  (void)o;(void)id;return 0;  /* the samples stay out of the binary */
#endif
}
enum { GAIN_GUN_OWN=96 };
/* The own gun ($648=3) loops its sample while it fires.  The POKEY's
   channel 4 stays silent meanwhile; RO's tail (255/254) is not needed. */
static void gun_samples(BmSound *s,const BmSoundOut *o){
  const BmSample *gun=sample(o,BM_SAMPLE_GUN_OWN);
  if(!gun)return;
  if(s->gun==3u){
    if(!s->gun_sample||!bm_mixer_active(o->mixer,BM_VOICE_GUN)){
      bm_pokey_write(group(o,BM_SOUND_GUN),BM_POKEY_AUDC4,0u);
      bm_mixer_play(o->mixer,BM_VOICE_GUN,gun,1u,GAIN_GUN_OWN);
      s->gun_sample=1u;
    }
  }else if(s->gun_sample){
    bm_mixer_release(o->mixer,BM_VOICE_GUN);
    s->gun_sample=0u;
    if(s->gun>=254u)s->gun=0u;
  }
}
void bm_sound_event_update(BmSound *s,const BmSoundOut *o){
  gun_samples(s,o);
  if(!s->gun_sample)bm_sound_gunsnd(s,group(o,BM_SOUND_GUN));
  bm_sound_splash(s,group(o,BM_SOUND_BOMB));
  bm_sound_ggnsnd(s,group(o,BM_SOUND_GROUND_GUN));
  bm_sound_runsnd(s,group(o,BM_SOUND_BEEP));
  bm_sound_cdrp(s,group(o,BM_SOUND_BOMB),main_pokey(o));
}
void bm_sound_event_splash_step(BmSound *s,const BmSoundOut *o){
  bm_sound_splash(s,group(o,BM_SOUND_BOMB));
}
void bm_sound_event_bomb_release(BmSound *s,const BmSoundOut *o){
  bm_sound_bomb_release(s,group(o,BM_SOUND_BOMB),main_pokey(o));
}
void bm_sound_event_bomb_lands(BmSound *s,const BmSoundOut *o,BmBombImpact impact){
  BmPokey *k=group(o,BM_SOUND_BOMB);
  if(impact==BM_BOMB_BEHIND_ROOFS){              /* Z3 .13: CDRP runs on */
    s->drop=s->drop_last;++s->dropping;return;
  }
  bm_pokey_write(k,BM_POKEY_AUDC2,0u);            /* AFP */
  if(impact==BM_BOMB_WATER)bm_sound_splash_start(s,k);
  else bm_sound_event_explosion(s,o,BM_EXPLOSION_BOMB);
}

void bm_sound_event_low_altitude(BmSound *s,const BmSoundOut *o,BmLowAltitude state){
  BmPokey *k=group(o,BM_SOUND_WARN);
  if(state==BM_LOW_TONE){
    s->splash=0u;s->low_tone=10u;
    bm_pokey_write(k,BM_POKEY_AUDF2,10u);bm_pokey_write(k,BM_POKEY_AUDC2,0xa6u);
  }else if(state==BM_LOW_SILENT)bm_pokey_write(k,BM_POKEY_AUDC2,0u);
  else if(s->low_tone){s->low_tone=0u;bm_pokey_write(k,BM_POKEY_AUDC2,0u);}
}
void bm_sound_event_line(BmSound *s,const BmSoundOut *o,uint8_t in_line,uint8_t low){
  BmPokey *k=group(o,BM_SOUND_WARN);
  /* Both paths leave channel 3 to a running explosion ($644) - unless the
     groups are separate.  LINE's AUDC2=0 then only stops LOW. */
  uint8_t free=(uint8_t)(o->separate||!s->xp_count);
  if(in_line){
    if(low)bm_pokey_write(k,BM_POKEY_AUDC2,0u);
    if(!free)return;
    if(--s->line_timer>=235u){
      bm_pokey_write(k,BM_POKEY_AUDF3,15u);bm_pokey_write(k,BM_POKEY_AUDC3,0xa2u);
    }else bm_pokey_write(k,BM_POKEY_AUDC3,0u);
    return;
  }
  s->line_timer=0u;
  if(free)bm_pokey_write(k,BM_POKEY_AUDC3,0u);
}
void bm_sound_event_warning(BmSound *s,const BmSoundOut *o,uint8_t on){
  BmPokey *k=group(o,BM_SOUND_WARN);
  if(!on){bm_pokey_write(k,BM_POKEY_AUDC4,0u);return;}
  if(!o->separate)s->gun=0u;                      /* WARN: $648=0 */
  bm_sound_warn(s,k);
}
void bm_sound_event_marker(const BmSoundOut *o,uint8_t on){
  BmPokey *k=group(o,BM_SOUND_BEEP);
  if(!on){bm_pokey_write(k,BM_POKEY_AUDC4,0u);return;}
  bm_pokey_write(k,BM_POKEY_AUDC4,0xa2u);bm_pokey_write(k,BM_POKEY_AUDF4,15u);
}
void bm_sound_event_repair_beep(const BmSoundOut *o,uint8_t on){
  BmPokey *k=group(o,BM_SOUND_BEEP);
  if(!on){bm_pokey_write(k,BM_POKEY_AUDC4,0u);return;}
  bm_pokey_write(k,BM_POKEY_AUDF4,0x15u);bm_pokey_write(k,BM_POKEY_AUDC4,0xa2u);
}
