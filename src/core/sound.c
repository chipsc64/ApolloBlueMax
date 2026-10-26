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

/* SOUND=samples: the sample (or 0) for a sound and its level (256 = full
   scale) next to the original POKEY sounds.  A sound without a sample
   keeps its POKEY routine.  Tune the levels on the V4. */
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
/* Levels for the example samples: about 1.2x the POKEY sound's loudness
   (the own gun 1.9x, approved on the V4); retune for real recordings. */
static const uint16_t sample_gain[BM_SAMPLE_COUNT]={
  54u,90u,16u,      /* engine, engine_dying (at AUDC1 level 8, less with a
                       lower level), attacker_engine */
  96u,54u,          /* gun_own, gun_enemy */
  75u,73u,          /* ground_gun, flak */
  14u,              /* bomb_whistle: the original whistle is soft */
  134u,213u,190u,   /* explosion_small, _medium, _large */
  72u,17u};         /* splash, runway (the friendly runway's signal) */
/* The propeller (AUDC1 $C0..$C8): with an engine sample the engine voice
   loops it at a volume that follows AUDC1 (and a pitch that rises a little with
   it, which the original does not do) and POKEY channel 1 stays silent.
   The dying engine (KMKZ) has its own sample.  While PLAY.IT runs, channel
   1 belongs to the tune.  Returns 0 when the POKEY plays the engine. */
static int engine_sync(BmSound *s,const BmSoundOut *o,uint8_t audc1,uint8_t dying){
  BmSampleId id=dying&&sample(o,BM_SAMPLE_ENGINE_DYING)?BM_SAMPLE_ENGINE_DYING:BM_SAMPLE_ENGINE;
  const BmSample *x=sample(o,id);
  uint8_t want=id==BM_SAMPLE_ENGINE_DYING?2u:1u;
  unsigned level=(audc1>>4)==0xcu?audc1&15u:0u;
  if(!x||s->music_on)return 0;
  bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,0u);
  if(want!=s->engine_sample||!bm_mixer_active(o->mixer,BM_VOICE_ENGINE)){
    bm_mixer_play(o->mixer,BM_VOICE_ENGINE,x,1u,0u);
    s->engine_sample=want;
  }
  /* Volume (level+2)/10: full at $C8, but idling on the ground ($C1)
     stays audible (3/10 instead of 1/8). */
  bm_mixer_set(o->mixer,BM_VOICE_ENGINE,(uint16_t)(256+((int)level-4)*12),
               (uint16_t)(level?sample_gain[id]*(level+2u)/10u:0u));
  return 1;
}
/* Play a sample once (or looped) on a voice; 0 if there is none. */
static int play(const BmSoundOut *o,BmSampleId id,unsigned voice,uint8_t loop){
  const BmSample *x=sample(o,id);
  if(!x)return 0;
  bm_mixer_play(o->mixer,voice,x,loop,sample_gain[id]);
  return 1;
}

void bm_sound_event_power_on(BmSound *s,const BmSoundOut *o){
  bm_pokey_write(main_pokey(o),BM_POKEY_SKCTL,3u);
  bm_pokey_write(main_pokey(o),BM_POKEY_AUDF1,251u);
  bm_sound_event_takeoff_engine(s,o);
}
void bm_sound_event_takeoff_engine(BmSound *s,const BmSoundOut *o){       /* AFX */
  BmPokey *k=main_pokey(o);
  bm_pokey_write(k,BM_POKEY_AUDF1,251u);bm_pokey_write(k,BM_POKEY_AUDF3,13u);
  s->engine=0xc1u;
  if(!engine_sync(s,o,0xc1u,0u))bm_pokey_write(k,BM_POKEY_AUDC1,0xc1u);
}
void bm_sound_event_quiet(const BmSoundOut *o){
  unsigned g;for(g=0;g<BM_SOUND_GROUPS;++g)bm_sound_quiet(group(o,g));
  if(o->mixer)bm_mixer_reset(o->mixer);
}
void bm_sound_event_demo_music(BmSound *s,const BmSoundOut *o){
  bm_sound_quiet(main_pokey(o));bm_sound_music_start(s);
  /* QUIET silences every sound on the main POKEY: the voices too. */
  if(o->mixer){
    bm_mixer_reset(o->mixer);
    s->own_firing=s->enemy_firing=s->gun_sample=s->bomb_sample=s->engine_sample=0u;
  }
}
void bm_sound_event_end_music(BmSound *s){bm_sound_music_start(s);}
uint8_t bm_sound_event_vblank(BmSound *s,const BmSoundOut *o){
  bm_sound_xsnd(s,main_pokey(o));
  return (uint8_t)(s->music_on&&!bm_sound_music_tick(s,main_pokey(o)));
}

/* XXX limit/volume per caller (CRASH, SV, VB, R7, BMCPTL, SW/C9, $1B61). */
static const uint8_t explosion_limit[7] ={80u,15u,50u,50u,70u,2u,0x2au};
static const uint8_t explosion_volume[7]={0x4fu,0x48u,0x4cu,0x4cu,0x4eu,0x47u,0x4cu};
/* The sample per XXX caller: crash, capital and bomb are large, aircraft
   and objects medium, damage small; the shell burst has its own. */
static const uint8_t explosion_sample[7]={BM_SAMPLE_EXPLOSION_LARGE,
  BM_SAMPLE_EXPLOSION_SMALL,BM_SAMPLE_EXPLOSION_MEDIUM,BM_SAMPLE_EXPLOSION_MEDIUM,
  BM_SAMPLE_EXPLOSION_LARGE,BM_SAMPLE_FLAK,BM_SAMPLE_EXPLOSION_LARGE};
void bm_sound_event_explosion(BmSound *s,const BmSoundOut *o,BmExplosion kind){
  if(play(o,(BmSampleId)explosion_sample[kind],
          kind==BM_EXPLOSION_FLAK?BM_VOICE_FLAK:BM_VOICE_EXPLOSION,0u))return;
  bm_sound_xxx(s,main_pokey(o),explosion_limit[kind],explosion_volume[kind]);
}
/* The burst only sounds when no POKEY explosion runs; a sample always. */
void bm_sound_event_flak(BmSound *s,const BmSoundOut *o){
  if(!s->xp_count||sample(o,BM_SAMPLE_FLAK))bm_sound_event_explosion(s,o,BM_EXPLOSION_FLAK);
}
void bm_sound_event_crash(BmSound *s,const BmSoundOut *o){
  unsigned g;
  bm_sound_event_explosion(s,o,BM_EXPLOSION_CRASH);
  bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,0u);
  engine_sync(s,o,0u,0u);
  if(o->mixer)bm_mixer_stop(o->mixer,BM_VOICE_ATTACKER);
  for(g=BM_SOUND_MAIN;g<=BM_SOUND_BEEP;++g)bm_pokey_write(group(o,g),BM_POKEY_AUDC4,0u);
  if(o->mixer){                                   /* RO/QD */
    bm_mixer_stop(o->mixer,BM_VOICE_GUN);bm_mixer_stop(o->mixer,BM_VOICE_ENEMY_GUN);
    s->own_firing=s->enemy_firing=s->gun_sample=0u;
  }
}

/* The engine routines write AUDC1 as on the Atari; with an engine sample
   the voice follows the new level and channel 1 is silenced again before
   anything is rendered. */
void bm_sound_event_engine(BmSound *s,const BmSoundOut *o,uint8_t stick){
  bm_sound_roar(s,main_pokey(o),stick);
  engine_sync(s,o,s->engine,0u);
}
void bm_sound_event_engine_dead(BmSound *s,const BmSoundOut *o){
  bm_sound_engine_dying(s,main_pokey(o));
  engine_sync(s,o,s->engine,1u);
}
void bm_sound_event_engine_idle(BmSound *s,const BmSoundOut *o){         /* GROUND */
  if(s->engine>=0xc2u){
    --s->engine;
    if(!engine_sync(s,o,s->engine,0u))bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,s->engine);
  }
}
/* PBMB: AUDC1=$C3 while the attacker flies.  With its own sample the
   attacker gets a voice and the own engine keeps sounding. */
void bm_sound_event_attacker(BmSound *s,const BmSoundOut *o,uint8_t flying){
  const BmSample *x=sample(o,BM_SAMPLE_ATTACKER_ENGINE);
  if(x){
    if(!flying)bm_mixer_stop(o->mixer,BM_VOICE_ATTACKER);
    else if(!bm_mixer_active(o->mixer,BM_VOICE_ATTACKER))
      play(o,BM_SAMPLE_ATTACKER_ENGINE,BM_VOICE_ATTACKER,1u);
    if(!engine_sync(s,o,s->engine,0u))bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,s->engine);
    return;
  }
  if(!engine_sync(s,o,flying?0xc3u:s->engine,0u))
    bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,flying?0xc3u:s->engine);
}
uint8_t bm_sound_event_engine_stop_step(BmSound *s,const BmSoundOut *o){  /* STOPL .2 */
  if(s->engine==0xc0u)return 1u;
  --s->engine;
  if(!engine_sync(s,o,s->engine,0u))bm_pokey_write(main_pokey(o),BM_POKEY_AUDC1,s->engine);
  return 0u;
}

void bm_sound_event_enemy_shot(BmSound *s){s->gun=0x30u;s->enemy_firing=1u;}
void bm_sound_event_own_shot(BmSound *s){s->gun=3u;s->own_firing=1u;}
void bm_sound_event_shot_end(BmSound *s){s->gun=255u;s->enemy_firing=0u;}
void bm_sound_event_own_shot_end(BmSound *s){s->gun=255u;s->own_firing=0u;}
void bm_sound_event_ground_gun(BmSound *s){s->ground_gun=12u;}
void bm_sound_event_runway(BmSound *s){s->runway=0x19u;}

/* A looped sample on its voice while on; at the end it plays out its pass. */
static void loop_voice(const BmSoundOut *o,unsigned voice,BmSampleId id,uint8_t on){
  const BmVoice *v=&o->mixer->voice[voice];
  if(on){if(!v->active||!v->loop)play(o,id,voice,1u);}
  else if(v->active&&v->loop)bm_mixer_release(o->mixer,voice);
}
/* The guns with samples: each gun loops its sample while it fires, on its
   own voice, whatever happens to $648 (a warning or the other gun's shot
   ends it on the Atari).  $648 still drives POKEY channel 4 for a gun
   without a sample; while it belongs to a sampled gun the channel is
   silent and RO's tail (255/254) is skipped. */
static void gun_samples(BmSound *s,const BmSoundOut *o){
  uint8_t own=(uint8_t)(sample(o,BM_SAMPLE_GUN_OWN)!=0),
          enemy=(uint8_t)(sample(o,BM_SAMPLE_GUN_ENEMY)!=0);
  if(own)loop_voice(o,BM_VOICE_GUN,BM_SAMPLE_GUN_OWN,s->own_firing);
  if(enemy)loop_voice(o,BM_VOICE_ENEMY_GUN,BM_SAMPLE_GUN_ENEMY,s->enemy_firing);
  if(s->gun==3u||s->gun==0x30u){
    uint8_t pokey_gun=(uint8_t)(s->gun_last&&!s->gun_sample);  /* still sounding */
    s->gun_sample=s->gun==3u?own:enemy;
    if(s->gun_sample&&pokey_gun)bm_pokey_write(group(o,BM_SOUND_GUN),BM_POKEY_AUDC4,0u);
  }else if(s->gun>=254u&&s->gun_sample)s->gun=0u;
  s->gun_last=s->gun;
}
/* AFF: the whistle ends in the bomb explosion. */
static void drop_end_event(BmSound *s,const BmSoundOut *o){
  s->dropping=0u;
  if(s->bomb_sample){bm_mixer_stop(o->mixer,BM_VOICE_BOMB);s->bomb_sample=0u;}
  else bm_pokey_write(group(o,BM_SOUND_BOMB),BM_POKEY_AUDC2,0u);
  bm_sound_event_explosion(s,o,BM_EXPLOSION_BOMB);
}
/* CDRP with the whistle sample: the counting stays, the POKEY is silent. */
static void cdrp_event(BmSound *s,const BmSoundOut *o){
  if(!s->bomb_sample&&!sample(o,BM_SAMPLE_EXPLOSION_LARGE)){
    bm_sound_cdrp(s,group(o,BM_SOUND_BOMB),main_pokey(o));return;
  }
  if(!s->dropping)return;
  if(s->drop>=70u){drop_end_event(s,o);return;}
  if(!(--s->drop_timer&1u))return;
  ++s->drop;s->drop_last=s->drop;
  if(s->bomb_sample)return;
  bm_pokey_write(group(o,BM_SOUND_BOMB),BM_POKEY_AUDF2,s->drop);
  bm_pokey_write(group(o,BM_SOUND_BOMB),BM_POKEY_AUDC2,0xa2u);
}
void bm_sound_event_update(BmSound *s,const BmSoundOut *o){
  gun_samples(s,o);
  if(!s->gun_sample)bm_sound_gunsnd(s,group(o,BM_SOUND_GUN));
  bm_sound_splash(s,group(o,BM_SOUND_BOMB));
  /* A fresh ground gun shot ($6AF=12) plays its sample instead. */
  if(s->ground_gun==12u&&play(o,BM_SAMPLE_GROUND_GUN,BM_VOICE_GROUND_GUN,0u))s->ground_gun=0u;
  bm_sound_ggnsnd(s,group(o,BM_SOUND_GROUND_GUN));
  /* A friendly runway in view ($64C=$19): its signal sample instead. */
  if(s->runway==0x19u&&play(o,BM_SAMPLE_RUNWAY,BM_VOICE_RUNWAY,0u))s->runway=0u;
  bm_sound_runsnd(s,group(o,BM_SOUND_BEEP));
  cdrp_event(s,o);
}
void bm_sound_event_splash_step(BmSound *s,const BmSoundOut *o){
  bm_sound_splash(s,group(o,BM_SOUND_BOMB));
}
void bm_sound_event_bomb_release(BmSound *s,const BmSoundOut *o){
  BmPokey *k=group(o,BM_SOUND_BOMB);
  if(!sample(o,BM_SAMPLE_BOMB_WHISTLE)&&!sample(o,BM_SAMPLE_EXPLOSION_LARGE)){
    bm_sound_bomb_release(s,k,main_pokey(o));return;
  }
  if(s->dropping)drop_end_event(s,o);
  s->drop=0x14u;s->splash=0u;
  if(play(o,BM_SAMPLE_BOMB_WHISTLE,BM_VOICE_BOMB,0u)){
    bm_pokey_write(k,BM_POKEY_AUDC2,0u);s->bomb_sample=1u;
  }else{
    bm_pokey_write(k,BM_POKEY_AUDF2,0x14u);bm_pokey_write(k,BM_POKEY_AUDC2,0xa2u);
  }
}
void bm_sound_event_bomb_lands(BmSound *s,const BmSoundOut *o,BmBombImpact impact){
  BmPokey *k=group(o,BM_SOUND_BOMB);
  if(impact==BM_BOMB_BEHIND_ROOFS){              /* Z3 .13: CDRP runs on */
    s->drop=s->drop_last;++s->dropping;return;
  }
  if(s->bomb_sample){bm_mixer_stop(o->mixer,BM_VOICE_BOMB);s->bomb_sample=0u;}
  bm_pokey_write(k,BM_POKEY_AUDC2,0u);            /* AFP */
  if(impact==BM_BOMB_WATER){
    if(!play(o,BM_SAMPLE_SPLASH,BM_VOICE_SPLASH,0u))bm_sound_splash_start(s,k);
  }else bm_sound_event_explosion(s,o,BM_EXPLOSION_BOMB);
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
