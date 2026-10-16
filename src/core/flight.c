#include "bm_flight.h"

void bm_flight_speed_step(uint16_t *speed, uint8_t *countdown,
                          uint8_t special_mode, uint8_t joystick){
  unsigned hundreds=*speed/100u;
  /* EXXON.5 SPD/KL: $63F != 0 and stick 6 select acceleration,
     except when the hundreds character is 83 (digit 3). */
  if(special_mode&&joystick==6u){
    if(hundreds==3u)return;
    if(--*countdown)return;
    *countdown=2u;
    ++*speed;
    return;
  }
  /* PD/KT accelerates below 200 without touching $635. KN/MM slows
     values above 200 every second call. Exactly 200 leaves the timer. */
  if(hundreds<2u){++*speed;return;}
  if(hundreds==3u||(*speed/10u)%10u>=1u||*speed%10u>=1u){
    if(--*countdown)return;
    *countdown=2u;
    --*speed;
  }
}

uint8_t bm_flight_joystick_step(BmFlightPosition *p, uint8_t joystick){
  switch(joystick){
    case 14: /* UP; at F5=60 it falls through to CA: the shadow descends
                until F6=168, so the altitude still increases. */
      if(p->plane_y==60u){
        if(p->shadow_y==168u)return 0;
        ++p->shadow_y;++p->turn_value;
        return 0;
      }
      ++p->turn_value;--p->plane_y;
      return 0;
    case 13: /* KK/DN: down */
      --p->turn_value;++p->plane_y;
      return (uint8_t)(p->shadow_y-p->plane_y==2u);
    case 7: /* PK: horizontal right, plane only. */
      if(p->plane_x<184u)++p->plane_x;
      return 0;
    case 11: /* PL: horizontal left, plane only. */
      if(p->plane_x>60u)--p->plane_x;
      return 0;
    case 6: /* FRWD: right and upward, including shadow. */
      if(p->plane_x==184u||p->plane_y==60u)return 0;
      ++p->plane_x;--p->shadow_y;++p->turn_value;--p->plane_y;
      return 0;
    case 10: /* CE: left and upward, including shadow. */
      if(p->plane_x>60u)--p->plane_x;
      if(p->plane_y==60u)return 0;
      --p->shadow_y;++p->turn_value;--p->plane_y;
      return 0;
    case 5: /* CD: right and downward, including shadow. */
      if(p->plane_x<184u)++p->plane_x;
      if(p->shadow_y==168u)return 0;
      ++p->shadow_y;--p->turn_value;++p->plane_y;
      return (uint8_t)(p->shadow_y-p->plane_y==2u);
    case 9: /* PN: three DEC $608, then BACK. */
      p->back_counter=(uint8_t)(p->back_counter-2u);
      /* fall through */
    default:
      /* BACK: every other stick code, neutral included.  Once $608 goes
         negative the aircraft drifts one colour clock back (left) and,
         through CB/DN, plane and shadow one line down. */
      --p->back_counter;
      if(!(p->back_counter&0x80u))return 0;
      p->back_counter=4u;
      if(p->plane_x==60u||p->shadow_y==168u)return 0;
      --p->plane_x;++p->shadow_y;++p->plane_y;
      return (uint8_t)(p->shadow_y-p->plane_y==2u);
  }
}

uint8_t bm_flight_altitude(const BmFlightPosition *p){
  return (uint8_t)(p->shadow_y-p->plane_y);
}

uint8_t bm_flight_gravity_step(BmFlightPosition *p,
                               uint8_t *countdown, uint8_t enabled,
                               uint8_t joystick, uint8_t up_code,
                               uint8_t down_code){
  if(!enabled||joystick==up_code||joystick==down_code)return 0;
  if(--*countdown)return 0;
  *countdown=40u;
  return bm_flight_joystick_step(p,down_code);
}

uint8_t bm_flight_deadstick_step(BmFlightPosition *p,uint8_t *countdown,
                                 uint8_t joystick,uint8_t down_code){
  uint8_t crash=0u;
  if(joystick==down_code)crash=bm_flight_joystick_step(p,down_code);
  if(--*countdown)return crash;
  *countdown=4u;
  if(bm_flight_joystick_step(p,down_code))crash=1u;
  if(joystick==5u){if(bm_flight_joystick_step(p,5u))crash=1u;}
  else if(joystick==10u){if(bm_flight_joystick_step(p,10u))crash=1u;}
  return crash;
}

void bm_flight_takeoff_speed_step(BmTakeoffSpeed *s){
  /* EXXON.16 TKOSPD uses the takeoff-font digits 144..153. On the
     hundreds carry it switches to the normal-font representation 100. */
  if(++s->ones!=154u)return;
  s->ones=144u;
  if(++s->tens!=154u)return;
  s->hundreds=81u;s->tens=80u;s->ones=80u;
}

BmTakeoffResult bm_flight_takeoff_control(const BmTakeoffSpeed *s,
                                          uint8_t joystick,
                                          uint8_t runway_collision){
  /* EXXON.5 TAKOF DI/DJ/DK: neutral stays on the runway while P2PF is
     clear. Pulling up is valid only after the display reaches 100. */
  if(joystick==15u)
    return runway_collision?BM_TAKEOFF_CRASH:BM_TAKEOFF_CONTINUE;
  if(joystick==14u)
    /* DJ compares the raw glyph code with literal 100: takeoff-font 144
       is rejected, normal-font 81 (digit 1) signals the completed carry. */
    return s->hundreds<100u?BM_TAKEOFF_AIRBORNE:BM_TAKEOFF_CRASH;
  return BM_TAKEOFF_CRASH;
}

uint8_t bm_flight_fuel_step(BmFuelState *fuel, uint8_t joystick,
                            uint8_t double_rate_status){
  unsigned decrements;
  if(!fuel->active)return 0;
  decrements=(double_rate_status||joystick==6u)?2u:1u;
  while(decrements--)--fuel->countdown;
  if(!(fuel->countdown&0x80u))return 0;
  fuel->countdown=40u;
  if(fuel->value){--fuel->value;return 1;}
  /* FUEL/MR observes 000 after the low digit decrements to glyph 79,
     restores that digit to 80 and disables $63F. */
  fuel->active=0u;fuel->empty=1u;
  return 0;
}

BmRefuelResult bm_flight_refuel_step(BmFuelState *fuel,
                                     uint8_t *service_phase){
  /* REFUEL first tests the hundreds glyph against 83 (digit 3). It then
     advances $6E0 and services bombs; the fuel digits are left untouched. */
  if(fuel->value>=300u){
    ++*service_phase;
    return BM_REFUEL_FUEL_FULL;
  }
  if(*service_phase>=1u)return BM_REFUEL_ORDNANCE;
  /* MU/MW uses an equality-to-zero counter, resets it to two, and increments
     the decimal display with the usual 9 -> 0 carries. Numeric storage gives
     the identical sequence up to the separately detected 300 state. */
  --fuel->countdown;
  if(fuel->countdown!=0u)return BM_REFUEL_WAITING;
  fuel->countdown=2u;
  ++fuel->value;
  fuel->empty=0u;
  return BM_REFUEL_WAITING;
}

uint8_t bm_flight_landing_begin(BmLandingState *landing,
                                uint8_t runway_slot,
                                uint8_t trigger_pressed){
  if(runway_slot||!trigger_pressed)return 0u;
  landing->active=1u;
  return 1u;
}

void bm_flight_landing_speed_step(uint16_t *speed, uint8_t *countdown){
  if(*speed==100u)return;
  --*countdown;
  if(*countdown!=0u)return;
  *countdown=2u;
  if(*speed>100u)--*speed;
  else ++*speed;
}

BmLandingResult bm_flight_landing_step(BmLandingState *landing,
                                       uint16_t speed, uint8_t altitude,
                                       uint8_t landing_marker,
                                       uint8_t p2pf_collision){
  (void)speed; /* LAND has no speed condition; SPDL only converges. */
  if(!landing->active)return BM_LANDING_INACTIVE;
  if(!landing_marker){landing->active=0u;return BM_LANDING_ABORTED;}
  if(altitude!=3u)return BM_LANDING_APPROACH;
  landing->active=0u;
  return p2pf_collision==8u?BM_LANDING_GROUND:BM_LANDING_CRASH;
}

uint8_t bm_flight_ground_service_step(BmGroundService *service,
                                      uint8_t *service_phase){
  if(*service_phase<1u)return 0u;
  if(*service_phase==1u){
    /* INCBMB: $F8 counts 34, then 8.  Only on expiry does it test the tens
       glyph: a full rack ends with ADN (INC $6E0), else one bomb more. */
    if(!service->bomb_countdown)service->bomb_countdown=34u;
    if(--service->bomb_countdown)return 0u;
    service->bomb_countdown=8u;
    if(service->bombs>=30u){
      service->bomb_countdown=0u;*service_phase=2u;return 0u;
    }
    ++service->bombs;
    return 0u;
  }
  if(*service_phase==2u){
    if(service->damage_level>=4u){*service_phase=3u;return 1u;}
    if(!service->repair_countdown)service->repair_countdown=90u;
    if(--service->repair_countdown)return 0u;
    ++service->damage_level;
    if(service->damage_slots)
      service->damage_slots&=(uint8_t)(service->damage_slots-1u);
    service->repair_countdown=90u;
    return 0u;
  }
  return 1u;
}
