#ifndef BM_FLIGHT_H
#define BM_FLIGHT_H
#include <stdint.h>

/* SPD's three screen digits decoded as a number, not pixels per tick.
   countdown corresponds to $635; callers retain the Atari joystick code. */
void bm_flight_speed_step(uint16_t *speed, uint8_t *countdown,
                          uint8_t special_mode, uint8_t joystick);

typedef struct {
  uint8_t plane_x;       /* $F4: Atari player/missile horizontal position. */
  uint8_t plane_y;       /* $F5: top scanline of the player plane. */
  uint8_t shadow_y;      /* $F6: top scanline of the shadow. */
  uint8_t back_counter;  /* $608: rate limiter for joystick code 9. */
  int16_t turn_value;    /* decoded SC+$66F; only its +/- changes matter here. */
} BmFlightPosition;

/* Position-only portion of EXXON.2 JOYST. Returns non-zero when the original
   routine enters CRASH because the shadow/plane separation reaches two. */
uint8_t bm_flight_joystick_step(BmFlightPosition *position, uint8_t joystick);

/* $F6-$F5 is the source's unsigned player altitude/depth separation. */
uint8_t bm_flight_altitude(const BmFlightPosition *position);

/* EXXON.5 GRAV. Vertical input suppresses gravity; otherwise $62D counts
   down and invokes JOYST's KK/down route every forty eligible updates. */
uint8_t bm_flight_gravity_step(BmFlightPosition *position,
                               uint8_t *countdown, uint8_t enabled,
                               uint8_t joystick, uint8_t up_code,
                               uint8_t down_code);
/* EXXON.5 KMKZ powerless-flight replacement for ordinary JOYST. */
uint8_t bm_flight_deadstick_step(BmFlightPosition *position,
                                 uint8_t *countdown,uint8_t joystick,
                                 uint8_t down_code);

typedef enum {
  BM_TAKEOFF_CONTINUE,
  BM_TAKEOFF_AIRBORNE,
  BM_TAKEOFF_CRASH
} BmTakeoffResult;

typedef struct {
  uint8_t hundreds, tens, ones; /* SC+$665..$667 display codes. */
} BmTakeoffSpeed;

void bm_flight_takeoff_speed_step(BmTakeoffSpeed *speed);
BmTakeoffResult bm_flight_takeoff_control(const BmTakeoffSpeed *speed,
                                          uint8_t joystick,
                                          uint8_t runway_collision);

typedef struct {
  uint16_t value;        /* SC+$64B..$64D decoded from glyphs 80..89. */
  uint8_t countdown;     /* $63A, tested with 6502 BMI after decrement. */
  uint8_t active;        /* $63F != 0. */
  uint8_t empty;         /* $643 set when the display reaches 000. */
} BmFuelState;

/* Returns non-zero only on the update that changes fuel by one. */
uint8_t bm_flight_fuel_step(BmFuelState *fuel, uint8_t joystick,
                            uint8_t double_rate_status);

typedef enum {
  BM_REFUEL_WAITING,
  BM_REFUEL_FUEL_FULL,
  BM_REFUEL_ORDNANCE
} BmRefuelResult;

/* EXXON.5 REFUEL's fuel-display portion. service_phase is $6E0. Once fuel
   already reads 300 the source advances to bomb servicing; while that phase
   is active REFUEL no longer changes the fuel digits. */
BmRefuelResult bm_flight_refuel_step(BmFuelState *fuel,
                                     uint8_t *service_phase);

typedef enum {
  BM_LANDING_INACTIVE,
  BM_LANDING_APPROACH,
  BM_LANDING_ABORTED,
  BM_LANDING_GROUND,
  BM_LANDING_CRASH
} BmLandingResult;

typedef struct {
  uint8_t active;        /* SC+$687/$6B6 are non-zero during LAND. */
} BmLandingState;

/* CKLAND starts LAND only when runway_slot ($609) is clear and the trigger
   shadow ($D8) is zero (pressed). */
uint8_t bm_flight_landing_begin(BmLandingState *landing,
                                uint8_t runway_slot,
                                uint8_t trigger_pressed);
/* SPDL's dedicated convergence to display speed 100. */
void bm_flight_landing_speed_step(uint16_t *speed, uint8_t *countdown);
/* LAND completes at altitude three, whatever the speed.  The shadow's P1PF
   ($D005) must then equal exactly eight; losing the landing marker
   SC+$687 before that returns to the main flight loop. */
BmLandingResult bm_flight_landing_step(BmLandingState *landing,
                                       uint16_t speed, uint8_t altitude,
                                       uint8_t landing_marker,
                                       uint8_t p2pf_collision);

typedef struct {
  uint8_t bombs;             /* SC+$651/$652 decoded, maximum 30. */
  uint8_t bomb_countdown;    /* INCBMB: first 34, then 8. */
  uint8_t damage_level;      /* $659: four means fully repaired. */
  uint8_t repair_countdown;  /* REFUEL/SB: 90 per FIX call. */
  uint8_t damage_slots;      /* SC+$655..$658 represented as bits 0..3. */
} BmGroundService;

/* Uses service_phase $6E0: 1 fills bombs, 2 repairs, 3 is complete. */
uint8_t bm_flight_ground_service_step(BmGroundService *service,
                                      uint8_t *service_phase);
#endif
