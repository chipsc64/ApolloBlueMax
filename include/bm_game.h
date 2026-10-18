#ifndef BM_GAME_H
#define BM_GAME_H
#include "bm_platform.h"
#include "bm_flight.h"
#include "bm_world.h"
#include "bm_sound.h"
typedef enum { BM_STATE_TITLE, BM_STATE_TAKEOFF, BM_STATE_PLAYING,
  BM_STATE_GROUND, BM_STATE_CRASHING, BM_STATE_PAUSED,
  BM_STATE_RANKING, BM_STATE_GAME_OVER, BM_STATE_OPTIONS } BmGameMode;
typedef struct { int16_t x,y,speed,altitude,fuel; int8_t bank; uint8_t lives; } BmPlayer;
typedef struct {
  BmGameMode mode,resume_mode; uint32_t tick,rng,score; uint16_t world_scroll;
  uint8_t previous_pause,fire_active,bomb_active;
  uint8_t flight_gravity_enabled,flight_gravity_countdown;
  uint8_t flight_speed_countdown;
  uint8_t flight_runway_visible,flight_runway_collision;
  uint8_t flight_ground_rollout,flight_service_phase;
  uint8_t flight_crash_reason; /* 1 JOYST, 2 LAND, 3 COLP02, 4 TURN, 5 CKCOL, 6 P0SHP, 7 PRIOR, 8 COLCPTL, 9 PBMB bomb on the ground. */
  uint8_t flight_crash_lift;     /* $654: CRASH raises the shadow this many lines. */
  uint8_t flight_crash_colp02;   /* $655=0: the enemy P2 flashes and vanishes. */
  uint16_t flight_crash_wait;    /* RTCLOK 19/20=251 until 19 wraps: 1029 frames. */
  uint8_t flight_turn_stick;     /* $607: TURN reacts to stick changes. */
  uint8_t flight_control_counter; /* $65A: M damage halves JOYST. */
  uint8_t flight_low_limit;       /* $6D6: CKCOL crash altitude. */
  uint8_t flight_preroll;         /* EXXON.0 CW: 150 idle frames before TAKOF. */
  uint8_t wind_timer,wind_left;   /* WIND: $BE rows, $695 direction. */
  uint8_t wind_on;                /* SC+$673 'W' indicator. */
  uint8_t flight_crash_timer,flight_crash_phase;
  uint8_t flight_service_trigger; /* $6DF: TRIGC's last trigger state. */
  uint8_t flight_service_tail;    /* REFUEL/N4: $F8=14 updates before MZ. */
  uint32_t play_frames;  /* RTCLOK since START (pause excluded) for RNK */
  uint16_t alt_display;  /* SC+$66D..$66F, moved by JOYST's F5 steps */
  uint8_t status_flash;  /* $CA: COLRDM status red after SV */
  /* OPTION menu (EXXON.16 MENU); survives bm_game_init like the Atari RAM. */
  uint8_t option_reversed; /* $679/$680 swapped: REVERSE CTRL */
  uint8_t menu_cursor;     /* $6E8: 0 skill, 1 control, 2 gravity */
  uint8_t console_previous; /* $6E9: last CONSOL value seen by MENU */
  /* EXXON.17 attract mode. */
  uint8_t demo;            /* $6F3: DEMO flies, DEMI on the status line */
  uint8_t demo_stick;      /* $6F6: DJOY's current stick code */
  uint8_t demo_timer;      /* $6F5: frames left for that stick code */
  uint8_t status_pf1;      /* $6F4: status PF1 (158; DFLSH in the demo) */
  uint8_t shadow_far;      /* SHCOL: thin grey shadow set at F6-F5=80 */
  uint16_t end_timer;    /* STOPL end sequence before RNK */
  uint8_t music_wait;    /* $6FF: the demo waits for PLAY.IT (EB/AFY) */
  uint8_t flight_damage_cooldown;
  /* RNKX inputs: original $6E6, $6E7, $6C2 and $6AC respectively. */
  uint8_t mission_difficulty,mission_rank_level;
  uint8_t mission_phase,mission_targets_destroyed,mission_objective_type;
  uint8_t mission_objective_sighted; /* $6AD belongs to the active GNSGHT. */
  uint8_t mission_objective_target; /* Original random $6C7: eight or nine. */
  uint8_t mission_target_cooldown; /* Original TRGT delay $6AA. */
  int16_t flight_crash_spread;
  uint16_t flight_speed;
  uint8_t player_shot_active;
  int16_t player_shot_x,player_shot_y;
  uint8_t player_bomb_active,player_bomb_phase,player_bomb_explosion;
  uint8_t player_bomb_water_splash,player_bomb_target_hit;
  uint8_t player_bomb_grass_impact;
  int16_t player_bomb_x,player_bomb_y;
  int16_t player_bomb_target_y; /* $61F=F6+3 fixed at release */
  uint8_t player_bomb_timer;     /* $620: TRIG waits while positive */
  int16_t player_bomb_explosion_x,player_bomb_explosion_y;
  BmFlightPosition flight_position;
  BmTakeoffSpeed takeoff_speed;
  BmFuelState flight_fuel;
  BmLandingState flight_landing;
  BmGroundService flight_service;
  BmPlayer player; BmWorldBuffer classic_world; BmFramebuffer framebuffer;
  BmPokey pokey;   /* $D200..$D20F as the 6502 code writes them */
  BmSound sound;   /* EXXON.6 sound state */
  /* ENHANCED_SOUND: own POKEYs for guns, ground guns, warnings, beeps and
     the bomb, mixed with the main one so that they no longer cut off. */
  BmPokey pokey_extra[5];
  BmSoundOut sound_out;   /* routing to the POKEYs, refreshed before use */
  BmMixer mixer;          /* SOUND=samples: the sample voices */
} BmGame;
void bm_game_init(BmGame *, uint32_t seed);
/* Power on as the original: EXXON.0 sets $6F3 and enters the attract mode. */
void bm_game_boot(BmGame *, uint32_t seed);
/* The player's P1 shadow as 16x8 chunky pixels (P1IMAGE with SHCOL's and
   TURN's edits to rows 0, 1, 6 and 7). */
const uint8_t *bm_game_shadow_pixels(const BmGame *);
void bm_game_tick(BmGame *, const BmInput *);
void bm_game_render(BmGame *);
uint32_t bm_framebuffer_checksum(const BmFramebuffer *);
#endif
