#ifndef BM_SOUND_H
#define BM_SOUND_H
#include <stdint.h>
#include "bm_pokey.h"
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
#endif
