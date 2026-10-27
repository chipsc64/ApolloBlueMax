#ifndef BM_WORLD_H
#define BM_WORLD_H

#include "bm_tile.h"
#include <stdint.h>

typedef uint8_t (*BmWorldRandom)(void *userdata);

typedef enum {
  BM_ROAD_STRAIGHT = 1,
  BM_ROAD_LEFT = 2,
  BM_ROAD_RIGHT = 3
} BmRoadDirection;

/* EXXON.0 has two main loops: SRT (river country, every first sortie) and
   CITY (entered through ZN after a takeoff, left again through XC). */
typedef enum {
  BM_WORLD_SRT = 0,
  BM_WORLD_CITY = 1
} BmWorldMode;

/* Values of BmWorldBuffer.landing (EXXON.5 blocking loops). */
enum { BM_WORLD_LAND = 1, BM_WORLD_ROLLOUT = 2 };

typedef enum {
  BM_CHARSET_BUILDING = 0,
  BM_CHARSET_HANGAR = 1,
  BM_CHARSET_TANK = 2
} BmWorldCharset;

typedef struct {
  uint8_t direction;
  uint8_t segment_remaining;
  uint8_t previous_turn;
  uint8_t edge_countdown;
  uint8_t shoulder_pending;
  uint8_t turn_preference;
  uint8_t centre;
  uint8_t left_limit;
  uint8_t right_limit;
} BmWorldGenerator;

/* EXXON.9 BLDGC state of one capital street side.  The right side lives in
   $67A..$67D/$681..$684/$B9..$BD, BLDGL swaps in the left copy $687..$693. */
typedef struct {
  uint8_t cursor;      /* $67A: column of the building's leading edge */
  uint8_t width;       /* $67B */
  uint8_t prev_width;  /* $67C */
  uint8_t delay;       /* $67D */
  uint8_t prev_bd, prev_bb, prev_bc; /* $681..$683: previous building */
  uint8_t active;      /* $684 */
  uint8_t row;         /* $B9 */
  uint8_t phase;       /* $BA: 0 new, 6 first building, 7 afterwards */
  uint8_t bb, bc, bd;  /* $BB..$BD: glyph offsets */
} BmCapitalSide;

typedef struct {
  uint8_t arena[25u * BM_ATARI_WORLD_STRIDE];
  uint16_t lms[BM_ATARI_PLAYFIELD_FETCH_ROWS];
  uint8_t rows[BM_ATARI_PLAYFIELD_FETCH_ROWS][BM_ATARI_WORLD_STRIDE];
  BmWorldGenerator generator;
  uint32_t rng;
  uint32_t gun_rng;
  uint32_t tree_rng;
  uint16_t top;
  uint16_t bottom_memory;
  uint8_t world_mode;       /* BmWorldMode */
  uint8_t prefill;          /* EXXON.0 EA loop: J, DRAW, MB, DRTR only. */
  uint8_t landing;          /* EXXON.5 BM_WORLD_LAND: DRAW, DRUN, COMMON only;
                               BM_WORLD_ROLLOUT (MJ): DRAW, DRUN, DRTR, XPLSN. */
  uint8_t object_slot;      /* $FD shared P2/P3 object slot. */
  uint8_t bridge_skip;      /* $61B: J's RANDOM&3; non-zero skips BR row 2. */
  uint8_t bridge_score_state; /* $6F2 */
  uint8_t boat_hits;        /* $6DE */
  uint8_t runway_slot;      /* $609: zero while a DRUN runway may be landed on. */
  uint8_t runway_marker;    /* SC+$687/$6B6 landing marker ("R"). */
  uint8_t runway_half;      /* $62C halves the $638 countdown. */
  uint8_t runway_interval;  /* $6CF from EAS*, +7 per sortie up to 220. */
  uint8_t enemy_tracking;   /* $6A7 enables TRACK for rear fighters. */
  uint8_t enemy_block;      /* $6A6 blocks PLBCK/PLFWD spawns. */
  BmCapitalSide capital[2]; /* BLDGC right [0], BLDGL left [1] */
  uint8_t ckcol_enabled;    /* $C5: CKCOL runs (PRIOR clears it) */
  uint8_t prior_entry;      /* $685: waiting to enter the capital */
  uint8_t prior_exit;       /* $686: capital left again */
  uint8_t prior_depth;      /* $6EA = F6/2+F4 */
  uint8_t gprior;           /* 623: 1 players in front, 4 behind PF */
  uint8_t shadow_hidden;    /* SHBNE cleared the P1 shadow */
  /* CPTL capital target: AAH on P2, AAI on P3, $FD=20. */
  uint8_t capital_y;        /* $6BF: bottom P2 row, 0 = none */
  uint8_t capital_x;        /* $C0: HPOS */
  uint8_t capital_exit;     /* $6C1: steps below $6BF=196 */
  uint8_t capital_shift;    /* lines moved on below 196 */
  uint8_t capital_wreck;    /* $6C5: XCPTLIM shown, XCPTL animates */
  uint8_t capital_flash;    /* $6C4 */
  uint8_t capital_size;     /* $6C6 / SIZEP2: 1 double, 3 quadruple */
  uint8_t capital_destroyed; /* $6C2 bits 1/2/4, mirrored from BmGame */
  uint8_t bomb_zone;        /* $6EB from AFB */
  uint8_t vbi_counter;      /* $604 */
  uint8_t frame_counter;    /* RTCLOK low byte (20); BmGame keeps it in step. */
  uint8_t coarse_event;     /* F0==0 during this update. */
  uint8_t target_state;     /* $C4: 0 bonus next, 5 request, 10 retry, 253 sight */
  uint8_t target_cooldown;  /* $6AA */
  uint8_t objectives_destroyed, objective_target; /* $6AC/$6C7 from BmGame */
  uint8_t p2_hits;          /* $6DC, set to three only by SHIP */
  uint8_t p2_colour;        /* 706: $7C/50/$A8 mark a bonus objective */
  uint8_t enemy_state;      /* $640 */
  uint8_t enemy_timer;      /* $62F */
  uint8_t enemy_warn;       /* $636 */
  uint8_t enemy_slow;       /* $6A8 */
  uint8_t enemy_body_on, enemy_shadow_on;
  uint8_t enemy_in_line;    /* LINE: $66E=$92, bullets destroy at once */
  uint8_t status_bk;        /* $66E: status-line COLBK (LINE, LOW, COLRDM) */
  uint8_t line_star;        /* ASTRSK: 1 '*' at $65A (above), 2 at $68A */
  uint8_t flash_counter;    /* $6B7: FLSH */
  uint8_t marker_blank;     /* FLSH stored 64 (blank) in SC+$687 */
  uint8_t player_fast;      /* SC+$665>='2' (speed 200+), for ASTRSK */
  int8_t quake;             /* $6BE: QUAKE counts down while positive */
  uint8_t quake_shift;      /* this frame HSCROL=F1-1 */
  uint8_t enemy_fire_chance; /* $6CA from EAS* */
  /* The other EAS* parameters (CHBAS, copied to $6C8..$6CE). */
  uint8_t wind_chance;      /* $6C8: WIND starts when RANDOM < this */
  uint8_t wind_rows;        /* $6C9: added to the gust length */
  uint8_t gun_depth_bias;   /* $6CB: COLM30 F6+$6CB-gun_Y ... */
  uint8_t gun_depth_range;  /* $6CC: ... must stay below this */
  uint8_t air_depth_bias;   /* $6CD: COLM20 F6-F5-$6CD ... */
  uint8_t air_depth_range;  /* $6CE: ... C2-C1 window width */
  uint8_t warning_on;       /* SC+$672 aircraft warning indicator */
  uint8_t area_type;
  uint8_t area_delay;
  uint8_t area_transition;
  uint8_t area_progress;
  uint8_t bridge_delay;
  uint8_t bridge_phase;
  uint8_t bridge_right_bank;
  uint8_t bridge_right_water;
  uint8_t bridge_left_bank;
  uint8_t bridge_left_water;
  uint8_t normal_runway_delay;
  uint8_t normal_runway_phase;
  uint8_t normal_runway_position;
  uint8_t normal_runway_saved_left_limit;
  uint8_t normal_runway_saved_right_limit;
  uint8_t normal_runway_count;
  uint16_t ship_delay;
  uint8_t ship_ready;
  uint8_t moored_ship_column;
  uint8_t moored_ship_delay;
  uint16_t moored_ship_offset;
  uint8_t ship_active;
  uint8_t ship_x;
  uint8_t ship_y;
  uint8_t ship_fast;
  uint8_t ship_hits_remaining; /* SHIP/P7: $6DC starts at three. */
  uint8_t ship_destroyed;
  uint8_t ship_destroy_timer;
  uint8_t river_car_active;
  uint8_t river_car_x;
  uint8_t river_car_y;
  uint8_t river_car_left;
  uint8_t river_car_spawned;
  uint8_t river_car_armed,river_car_delay; /* DRBR/JH $62A countdown. */
  uint8_t river_car_destroyed,river_car_destroy_timer;
  uint8_t river_vehicle_truck;
  uint8_t river_vehicle_count;
  uint8_t enemy_fighter_active;
  uint8_t enemy_fighter_spawned;
  uint8_t enemy_fighter_x;
  uint8_t enemy_fighter_y;
  uint8_t enemy_fighter_forward;
  uint8_t enemy_fighter_turn;
  uint8_t enemy_fighter_shadow_y;
  uint8_t enemy_fighter_hits_remaining;
  uint8_t enemy_fighter_destroyed;
  uint8_t enemy_fighter_destroy_timer;
  uint8_t enemy_fighter_shot_active;
  uint8_t enemy_fighter_shot_fired;
  uint8_t enemy_fighter_shot_x;
  uint8_t enemy_fighter_shot_y;
  uint8_t enemy_fighter_shot_forward;
  uint8_t target_sight_active,target_sight_type,target_sight_request;
  uint8_t target_sight_x,target_sight_y;
  uint8_t road_delay;
  uint8_t road_position;
  uint8_t road_target;
  uint8_t runway_delay;
  uint8_t runway_phase;
  uint8_t building_delay;
  uint8_t building_row;
  uint8_t building_variant;
  uint8_t building_position;
  uint8_t building_width;
  uint8_t building_height;
  uint8_t building_roof_start;
  uint8_t building_end_row;
  uint8_t object_control;
  uint8_t building_inactive;
  uint8_t charset_mode;
  uint8_t glyph_mode;
  uint8_t tank_charset_loaded;
  uint8_t object_marker;
  uint8_t tree_positions[6];
  uint16_t tree_row_offsets[6];
  uint8_t tank_delay;
  uint8_t tank_phase;
  uint8_t tank_variant;
  uint8_t tank_column;
  uint8_t tank2_delay;
  uint8_t tank2_phase;
  uint8_t gun_delay;
  uint8_t gun1_x;
  uint8_t gun1_origin_x;
  uint8_t gun1_y;
  uint8_t gun1_start_delay;
  uint8_t gun1_phase;
  uint8_t gun1_kind;        /* $BF: 0 FBRGN (bridge), 45 FTNKGN, $BC boat */
  uint8_t gun1_shot_x;
  uint8_t gun1_shot_y;
  uint8_t gun1_shot_direction;
  uint8_t gun1_shot_travel;
  uint8_t gun1_shot_explosion;
  uint8_t gun2_x;
  uint8_t gun2_origin_x;
  uint8_t gun2_y;
  uint8_t gun2_start_delay;
  uint8_t gun2_phase;
  uint8_t gun2_shot_x;
  uint8_t gun2_shot_y;
  uint8_t gun2_shot_direction;
  uint8_t gun2_shot_travel;
  uint8_t gun2_shot_explosion;
  uint8_t player_x_atari;
  uint8_t player_plane_y;   /* F5 */
  uint8_t player_y_atari;
  uint8_t bomb_impact_code;
  uint8_t bomb_impact_hit;
  uint8_t bomb_impact_water;
  uint8_t grass_impact_ticks;
  uint8_t impact_mode; /* Original $624: 0 water, 1 land, 4 tank, 9 hangar. */
  uint8_t impact_flash_ticks; /* Independent ABA/XFLSH timer ($6B9). */
  uint8_t hangar_impact_phase, tank_impact_phase; /* Persistent character RAM. */
  uint8_t hangar_target_available; /* Original $6F9. */
  uint8_t bomb_impact_handled;
  uint8_t building_impact_phase;
  uint8_t capital_gun_glyph; /* ADK (glyph 49) holds ADJ: WR sets, DFGN resets */
  uint16_t impact_other_offset;
  uint8_t impact_expected[4];
  uint16_t grass_impact_offset;
  uint8_t colbk;
  uint8_t colpf0;
  uint8_t colpf1;
  uint8_t colpf2;
  uint8_t colpf3;
  uint8_t source_column;
  uint8_t fine_phase;
  uint8_t scroll_clock;
  uint8_t scroll_event;
  /* This update: the playfield took a fine scroll step (scrolled) and the
     capital target took its step (capital_moved).  bm_game_tick clears
     both; SMOOTH_SCROLL draws ground objects half a step back on the
     update of their step. */
  uint8_t scrolled, capital_moved;
  /* Sound requests for the game (EXXON.7 FBRGN/SW, EXXON.10 FM2GN/C9):
     a gun fired ($6AF=12) and a shell burst (XXX 2,$47 if $644=0). */
  uint8_t sound_gun_fired, sound_flak;
  /* LINE (1 in line, 2 not; sound_line_low: depth<18 silences AUDC2),
     the aircraft warning (1 WARN, 2 AUDC4=0), R3's RUNSND start and FLSH
     (1 AUDC4=0, 2 the marker tone). */
  uint8_t sound_line, sound_line_low, sound_warn, sound_runway, sound_flash;
} BmWorldBuffer;

void bm_world_generator_init(BmWorldGenerator *generator);
void bm_world_generate_row(BmWorldGenerator *generator,
                           uint8_t row[BM_ATARI_WORLD_STRIDE],
                           BmWorldRandom random_byte, void *random_userdata);
void bm_world_buffer_init(BmWorldBuffer *buffer, uint32_t seed);
void bm_world_buffer_step(BmWorldBuffer *buffer);
/* CRASH loop: XFLSH and XPLSN only; no scroll and no objects move. */
void bm_world_crash_step(BmWorldBuffer *buffer);
uint8_t bm_world_bomb_impact(BmWorldBuffer *buffer, int16_t screen_x,
                             int16_t screen_y);
/* LAND/P2PF helpers for the two source runway families. The collision value
   is GTIA-compatible: bit 3 (value 8) denotes the grey PF3 runway surface. */
uint8_t bm_world_runway_visible(const BmWorldBuffer *buffer);
uint8_t bm_world_runway_collision(const BmWorldBuffer *buffer,
                                  int16_t screen_x, int16_t screen_y);
uint8_t bm_world_runway_mask_collision(const BmWorldBuffer *buffer,
                                       const uint8_t *pixels,
                                       uint16_t width, uint16_t height,
                                       uint16_t stride, int16_t screen_x,
                                       int16_t screen_y, int16_t anchor_x,
                                       int16_t anchor_y);
/* GTIA-style PnPF latch for an opaque sprite mask. Bits 0..3 correspond to
   COLPF0..COLPF3; ANTIC mode-4 pair 11 selects PF2 or PF3 via screen bit 7. */
uint8_t bm_world_playfield_mask_collision(const BmWorldBuffer *buffer,
                                          const uint8_t *pixels,
                                          uint16_t width,uint16_t height,
                                          uint16_t stride,int16_t screen_x,
                                          int16_t screen_y,int16_t anchor_x,
                                          int16_t anchor_y);
/* EXXON.0 after TAKOF: $6CF+7, then SRT (fewer than two objectives or
   RANDOM>=160) or V0/ZN into CITY.  Returns the resulting BmWorldMode. */
/* SHBT: bullet at HPOS/scanline against ground characters; returns the
   points scored (0: no hit, the bullet flies on). */
uint8_t bm_world_shot_ground(BmWorldBuffer *buffer, uint8_t hpos,
                             uint8_t scanline);
/* EXXON.15 PBMB: the airfield attacker ($FD=254). */
void bm_world_airfield_attack_spawn(BmWorldBuffer *buffer, uint8_t x);
void bm_world_airfield_attack_step(BmWorldBuffer *buffer);
/* CPTL object coverage at framebuffer (x,y): 0 none, 2 P2, 3 P3. */
uint8_t bm_world_capital_covers(const BmWorldBuffer *buffer, int x, int y);
/* EAS0/2/4/6/8/9 as table indices 0..5 (CHBAS). */
enum { BM_EAS0, BM_EAS2, BM_EAS4, BM_EAS6, BM_EAS8, BM_EAS9 };
/* Copies the seven bytes $6C8..$6CE of one EAS table. */
void bm_world_load_eas(BmWorldBuffer *buffer, uint8_t table);
uint8_t bm_world_after_takeoff(BmWorldBuffer *buffer,
                               uint8_t objectives_destroyed,
                               uint8_t objective_target);
void bm_world_target_sight_start(BmWorldBuffer *buffer,uint8_t objective_type,
                                 uint8_t world_column,uint8_t source_y);
void bm_world_target_sight_request(BmWorldBuffer *buffer);

#endif
