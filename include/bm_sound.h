#ifndef BM_SOUND_H
#define BM_SOUND_H
#include <stdint.h>
#include "bm_pokey.h"
#include "bm_mixer.h"
/* EXXON.6 sound routines on the software POKEY, with their Atari RAM. */
typedef struct {
  uint8_t engine;        /* $641: AUDC1 of the propeller roar ($C0..$C8) */
  uint8_t drop;          /* $642: AUDF2 of the falling bomb */
  uint8_t roar_timer;    /* $643: ROAR/KMKZ act every 20 calls */
  uint8_t xp_count;      /* $644: XSND explosion, 0 = silent */
  uint8_t xp_limit;      /* $645: count at which the volume starts to fall */
  uint8_t xp_volume;     /* $646: AUDC3 */
  uint8_t xp_step;       /* $647: four XSND calls per volume step */
  uint8_t gun;           /* $648: GUNSND, $30 shot flying, 255/254 tail */
  uint8_t drop_timer;    /* $649 */
  uint8_t gun_timer,gun_toggle; /* $64A, $64B */
  uint8_t ground_gun;    /* $6AF: GGNSND frames */
  uint8_t runway;        /* $64C: RUNSND frames */
  uint8_t splash,splash_f,splash_c; /* $64D..$64F: SPLASH */
  uint8_t drop_last;     /* $6EC: last DRPSND pitch */
  uint8_t dropping;      /* $6ED: CDRP active */
  uint8_t line_timer;    /* $6A4: LINE tone */
  uint8_t low_tone;      /* $6F8: LOW tone on */
  uint32_t random;       /* RANDOM ($D20A) for XSND */
  /* EXXON.18 PLAY.IT: V.STAT, DUR, VOL, TIE per voice and VV1..VV3. */
  uint8_t music_on;      /* CDTMR1 ($218) running */
  uint8_t music_status[3],music_duration[3],music_volume[3],music_tie[3];
  uint16_t music_position[3];
  uint8_t gun_sample;    /* SOUND=samples: the own gun plays its sample */
} BmSound;
void bm_sound_reset(BmSound *s);
/* EXXON.6 XSND: once per VBI. */
void bm_sound_xsnd(BmSound *s,BmPokey *k);
/* EXXON.6 XXX: A=limit ($645), X=volume ($646). */
void bm_sound_xxx(BmSound *s,BmPokey *k,uint8_t limit,uint8_t volume);
/* EXXON.6 ROAR (stick = $D7); KMKZ's engine part when the fuel is out. */
void bm_sound_roar(BmSound *s,BmPokey *k,uint8_t stick);
void bm_sound_engine_dying(BmSound *s,BmPokey *k);
void bm_sound_gunsnd(BmSound *s,BmPokey *k);
void bm_sound_ggnsnd(BmSound *s,BmPokey *k);
void bm_sound_runsnd(BmSound *s,BmPokey *k);
/* EXXON.6 WARN's tone (the caller clears $648). */
void bm_sound_warn(BmSound *s,BmPokey *k);
void bm_sound_splash(BmSound *s,BmPokey *k);
/* XPLSN water ($1B6B): the splash starts. */
void bm_sound_splash_start(BmSound *s,BmPokey *k);
/* MAX.1 BOMB ($1A0D/$1A59): a running drop sound ends, the whistle starts.
   explosion is the POKEY that carries XSND (the same one unless the
   enhanced mix separates them). */
void bm_sound_bomb_release(BmSound *s,BmPokey *k,BmPokey *explosion);
/* EXXON.15 CDRP (SRT): DRPSND until $642>=70, then AFF: XXX $2A,$4C. */
void bm_sound_cdrp(BmSound *s,BmPokey *k,BmPokey *explosion);
/* EXXON.18 PLAY.IT: start the three voices; the timer-1 VBI then calls
   bm_sound_music_tick once per frame.  It returns 0 once the tune is over
   (STOP.SN has cleared $D200..$D207 and $6FF=0). */
void bm_sound_music_start(BmSound *s);
uint8_t bm_sound_music_tick(BmSound *s,BmPokey *k);
/* EXXON.18 STOP.SN: $D200..$D207 = 0. */
void bm_sound_stop(BmPokey *k);
/* EXXON.16 QUIET: AUDC1..AUDC4 = 0. */
void bm_sound_quiet(BmPokey *k);

/* ---- Game sound events -------------------------------------------------
   The game only calls these.  Each names something that happens in the
   game; this POKEY implementation turns it into the original register
   writes.  A future sample-based backend can implement the same events.

   Output: the POKEY per sound group.  In the original all point to one
   POKEY; with ENHANCED_SOUND each group has its own (separate=1), so
   that a warning no longer stops the gun and LINE no longer silences the
   bomb. */
enum { BM_SOUND_MAIN, BM_SOUND_GUN, BM_SOUND_GROUND_GUN, BM_SOUND_WARN,
  BM_SOUND_BEEP, BM_SOUND_BOMB, BM_SOUND_GROUPS };
/* mixer (SOUND=samples): sounds with a sample in sounds/ play it there
   instead of their POKEY routine; 0 = POKEY only. */
typedef struct { BmPokey *pokey[BM_SOUND_GROUPS]; uint8_t separate; BmMixer *mixer; } BmSoundOut;
/* The samples (sounds/NAME.wav, tools/sound_assets.py keeps this order)
   and the mixer voice each sound uses. */
typedef enum { BM_SAMPLE_ENGINE, BM_SAMPLE_ENGINE_DYING, BM_SAMPLE_ATTACKER_ENGINE,
  BM_SAMPLE_GUN_OWN, BM_SAMPLE_GUN_ENEMY, BM_SAMPLE_GROUND_GUN, BM_SAMPLE_FLAK,
  BM_SAMPLE_BOMB_WHISTLE, BM_SAMPLE_EXPLOSION_SMALL, BM_SAMPLE_EXPLOSION_MEDIUM,
  BM_SAMPLE_EXPLOSION_LARGE, BM_SAMPLE_SPLASH, BM_SAMPLE_RUNWAY,
  BM_SAMPLE_COUNT } BmSampleId;
enum { BM_VOICE_ENGINE, BM_VOICE_ATTACKER, BM_VOICE_GUN, BM_VOICE_GROUND_GUN,
  BM_VOICE_BOMB, BM_VOICE_EXPLOSION, BM_VOICE_SPLASH, BM_VOICE_RUNWAY };

/* XXX callers, with their limit/volume. */
typedef enum { BM_EXPLOSION_CRASH, BM_EXPLOSION_DAMAGE, BM_EXPLOSION_AIRCRAFT,
  BM_EXPLOSION_OBJECT, BM_EXPLOSION_CAPITAL, BM_EXPLOSION_FLAK,
  BM_EXPLOSION_BOMB } BmExplosion;
typedef enum { BM_BOMB_LAND, BM_BOMB_WATER, BM_BOMB_BEHIND_ROOFS } BmBombImpact;
typedef enum { BM_LOW_TONE, BM_LOW_SILENT, BM_LOW_CLEAR } BmLowAltitude;

/* Game start (EXXON.0 NNNN: SKCTL 3, AUDF1 251) and AFX (the propeller). */
void bm_sound_event_power_on(BmSound *s,const BmSoundOut *o);
void bm_sound_event_takeoff_engine(BmSound *s,const BmSoundOut *o);
/* QUIET on every POKEY (CNSL -> OPTION). */
void bm_sound_event_quiet(const BmSoundOut *o);
/* PLAY.IT: the attract mode's tune (after QUIET) and the end tune. */
void bm_sound_event_demo_music(BmSound *s,const BmSoundOut *o);
void bm_sound_event_end_music(BmSound *s);
/* Once per update (the VBI): XSND and the tune.  Returns 1 when the tune
   has just ended. */
uint8_t bm_sound_event_vblank(BmSound *s,const BmSoundOut *o);
void bm_sound_event_explosion(BmSound *s,const BmSoundOut *o,BmExplosion kind);
/* The shell burst only sounds when no explosion is running ($644=0). */
void bm_sound_event_flak(BmSound *s,const BmSoundOut *o);
/* CRASH: propeller off, every channel-4 sound off. */
void bm_sound_event_crash(BmSound *s,const BmSoundOut *o);
/* Propeller: ROAR (stick = $D7), KMKZ when the engine is dead, GROUND's
   idle fall, the attacker's engine (PBMB) and STOPL's last steps (returns
   1 once the propeller is at $C0). */
void bm_sound_event_engine(BmSound *s,const BmSoundOut *o,uint8_t stick);
void bm_sound_event_engine_dead(BmSound *s,const BmSoundOut *o);
void bm_sound_event_engine_idle(BmSound *s,const BmSoundOut *o);
void bm_sound_event_attacker(BmSound *s,const BmSoundOut *o,uint8_t flying);
uint8_t bm_sound_event_engine_stop_step(BmSound *s,const BmSoundOut *o);
/* Machine guns ($648): a shot starts (enemy RN $30, own TRIG 3) or ends
   (RO/HG 255). */
void bm_sound_event_enemy_shot(BmSound *s);
void bm_sound_event_own_shot(BmSound *s);
void bm_sound_event_shot_end(BmSound *s);
/* A ground gun fires ($6AF=12); a runway appears (R3: $64C=$19). */
void bm_sound_event_ground_gun(BmSound *s);
void bm_sound_event_runway(BmSound *s);
/* COMMON/SRT per update: GUNSND, SPLASH, GGNSND, RUNSND, CDRP. */
void bm_sound_event_update(BmSound *s,const BmSoundOut *o);
/* SPLASH alone (BMBR, the CRASH loop). */
void bm_sound_event_splash_step(BmSound *s,const BmSoundOut *o);
void bm_sound_event_bomb_release(BmSound *s,const BmSoundOut *o);
void bm_sound_event_bomb_lands(BmSound *s,const BmSoundOut *o,BmBombImpact impact);
/* LOW: the low-altitude tone, its silent blink phase, or leaving the band. */
void bm_sound_event_low_altitude(BmSound *s,const BmSoundOut *o,BmLowAltitude state);
/* LINE: enemy in line (low: depth below 18) or not. */
void bm_sound_event_line(BmSound *s,const BmSoundOut *o,uint8_t in_line,uint8_t low);
/* The aircraft warning blinks (WARN / silence). */
void bm_sound_event_warning(BmSound *s,const BmSoundOut *o,uint8_t on);
/* FLSH: the landing marker's tone; SA: the repair beep. */
void bm_sound_event_marker(const BmSoundOut *o,uint8_t on);
void bm_sound_event_repair_beep(const BmSoundOut *o,uint8_t on);
#endif
