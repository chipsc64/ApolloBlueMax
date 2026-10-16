#include "bm_flight.h"
#include <assert.h>
#include <stdio.h>

int main(void){
  uint16_t speed;uint8_t timer;unsigned i;
  BmFlightPosition p;
  BmTakeoffSpeed takeoff;
  BmFuelState fuel;
  speed=198;timer=2;
  for(i=0;i<4;++i)bm_flight_speed_step(&speed,&timer,0,15);
  assert(speed==200&&timer==2);
  speed=202;timer=2;
  for(i=0;i<4;++i)bm_flight_speed_step(&speed,&timer,0,15);
  assert(speed==200&&timer==2);
  speed=299;timer=2;
  bm_flight_speed_step(&speed,&timer,1,6);
  assert(speed==299&&timer==1);
  bm_flight_speed_step(&speed,&timer,1,6);
  assert(speed==300&&timer==2);
  for(i=0;i<10;++i)bm_flight_speed_step(&speed,&timer,1,6);
  assert(speed==300&&timer==2);
  bm_flight_speed_step(&speed,&timer,1,15);
  bm_flight_speed_step(&speed,&timer,1,15);
  assert(speed==299&&timer==2);
  speed=210;timer=1;bm_flight_speed_step(&speed,&timer,0,15);
  assert(speed==209&&timer==2);
  speed=99;timer=1;bm_flight_speed_step(&speed,&timer,0,15);
  assert(speed==100&&timer==1);
  speed=250;timer=0;bm_flight_speed_step(&speed,&timer,0,15);
  assert(speed==250&&timer==255); /* DEC wraps, no implicit reset. */
  p=(BmFlightPosition){119,165,168,4,0};
  assert(!bm_flight_joystick_step(&p,14));
  assert(p.plane_x==119&&p.plane_y==164&&p.shadow_y==168&&p.turn_value==1);
  assert(!bm_flight_joystick_step(&p,6));
  assert(p.plane_x==120&&p.plane_y==163&&p.shadow_y==167&&p.turn_value==2);
  assert(!bm_flight_joystick_step(&p,10));
  assert(p.plane_x==119&&p.plane_y==162&&p.shadow_y==166&&p.turn_value==3);
  assert(!bm_flight_joystick_step(&p,7)&&p.plane_x==120&&p.shadow_y==166);
  assert(!bm_flight_joystick_step(&p,11)&&p.plane_x==119&&p.shadow_y==166);
  assert(!bm_flight_joystick_step(&p,5));
  assert(p.plane_x==120&&p.plane_y==163&&p.shadow_y==167&&p.turn_value==2);
  p=(BmFlightPosition){100,100,110,4,0};
  assert(!bm_flight_joystick_step(&p,9));
  assert(p.plane_x==100&&p.plane_y==100&&p.shadow_y==110&&p.back_counter==1);
  assert(!bm_flight_joystick_step(&p,9));
  assert(p.plane_x==99&&p.plane_y==101&&p.shadow_y==111&&p.back_counter==4);
  p=(BmFlightPosition){100,165,168,4,0};
  assert(bm_flight_joystick_step(&p,13));
  p=(BmFlightPosition){60,60,80,4,0};
  bm_flight_joystick_step(&p,10);assert(p.plane_x==60&&p.plane_y==60&&p.shadow_y==80);
  p=(BmFlightPosition){184,80,100,4,0};
  bm_flight_joystick_step(&p,6);assert(p.plane_x==184&&p.plane_y==80&&p.shadow_y==100);
  p=(BmFlightPosition){184,80,168,4,0};
  bm_flight_joystick_step(&p,5);assert(p.plane_x==184&&p.plane_y==80&&p.shadow_y==168);
  /* BACK: neutral (15) decrements $608; on the fifth update the aircraft
     drifts one colour clock left with plane and shadow one line down. */
  p=(BmFlightPosition){120u,100u,130u,4u,0};
  for(i=0;i<4u;++i){assert(!bm_flight_joystick_step(&p,15));
    assert(p.plane_x==120u&&p.plane_y==100u&&p.shadow_y==130u);}
  assert(!bm_flight_joystick_step(&p,15));
  assert(p.plane_x==119u&&p.plane_y==101u&&p.shadow_y==131u&&p.back_counter==4u);
  /* BACK stops at the bottom shadow line and at the left limit. */
  p=(BmFlightPosition){120u,165u,168u,0u,0};
  bm_flight_joystick_step(&p,15);assert(p.plane_x==120u&&p.plane_y==165u);
  p=(BmFlightPosition){60u,100u,130u,0u,0};
  bm_flight_joystick_step(&p,15);assert(p.plane_x==60u&&p.shadow_y==130u);
  /* UP at F5=60 continues at CA: the shadow descends to 168. */
  p=(BmFlightPosition){120u,60u,100u,4u,0};
  bm_flight_joystick_step(&p,14);
  assert(p.plane_y==60u&&p.shadow_y==101u&&bm_flight_altitude(&p)==41u);
  p.shadow_y=168u;bm_flight_joystick_step(&p,14);
  assert(p.plane_y==60u&&p.shadow_y==168u);
  p=(BmFlightPosition){119,164,168,4,0};timer=2;
  assert(bm_flight_altitude(&p)==4u);
  assert(!bm_flight_gravity_step(&p,&timer,1,15,14,13));
  assert(timer==1u&&p.plane_y==164u&&bm_flight_altitude(&p)==4u);
  assert(!bm_flight_gravity_step(&p,&timer,1,15,14,13));
  assert(timer==40u&&p.plane_y==165u&&bm_flight_altitude(&p)==3u);
  p.plane_y=165u;timer=1;
  assert(!bm_flight_gravity_step(&p,&timer,1,14,14,13));
  assert(timer==1u&&p.plane_y==165u);
  assert(!bm_flight_gravity_step(&p,&timer,1,13,14,13));
  assert(timer==1u&&p.plane_y==165u);
  assert(!bm_flight_gravity_step(&p,&timer,0,15,14,13));
  assert(timer==1u&&p.plane_y==165u);
  p=(BmFlightPosition){119,165,168,4,0};timer=1;
  assert(bm_flight_gravity_step(&p,&timer,1,15,14,13));
  assert(timer==40u&&p.plane_y==166u&&bm_flight_altitude(&p)==2u);
  p.plane_y=250u;p.shadow_y=5u;
  assert(bm_flight_altitude(&p)==11u); /* 6502 unsigned subtraction wraps. */
  takeoff=(BmTakeoffSpeed){144,144,144};
  for(i=0;i<9u;++i)bm_flight_takeoff_speed_step(&takeoff);
  assert(takeoff.hundreds==144u&&takeoff.tens==144u&&takeoff.ones==153u);
  bm_flight_takeoff_speed_step(&takeoff);
  assert(takeoff.tens==145u&&takeoff.ones==144u);
  for(i=0;i<90u;++i)bm_flight_takeoff_speed_step(&takeoff);
  assert(takeoff.hundreds==81u&&takeoff.tens==80u&&takeoff.ones==80u);
  takeoff=(BmTakeoffSpeed){144,153,153};
  bm_flight_takeoff_speed_step(&takeoff);
  assert(takeoff.hundreds==81u&&takeoff.tens==80u&&takeoff.ones==80u);
  takeoff=(BmTakeoffSpeed){144,150,150};
  assert(bm_flight_takeoff_control(&takeoff,15,0)==BM_TAKEOFF_CONTINUE);
  assert(bm_flight_takeoff_control(&takeoff,15,1)==BM_TAKEOFF_CRASH);
  assert(bm_flight_takeoff_control(&takeoff,14,0)==BM_TAKEOFF_CRASH);
  assert(bm_flight_takeoff_control(&takeoff,7,0)==BM_TAKEOFF_CRASH);
  takeoff=(BmTakeoffSpeed){81,80,80};
  assert(bm_flight_takeoff_control(&takeoff,14,1)==BM_TAKEOFF_AIRBORNE);
  fuel=(BmFuelState){300u,26u,1u,0u};
  for(i=0;i<26u;++i)assert(!bm_flight_fuel_step(&fuel,15u,0u));
  assert(fuel.value==300u&&fuel.countdown==0u);
  assert(bm_flight_fuel_step(&fuel,15u,0u));
  assert(fuel.value==299u&&fuel.countdown==40u);
  for(i=0;i<40u;++i)assert(!bm_flight_fuel_step(&fuel,15u,0u));
  assert(bm_flight_fuel_step(&fuel,15u,0u)&&fuel.value==298u);
  fuel=(BmFuelState){100u,1u,1u,0u};
  assert(bm_flight_fuel_step(&fuel,6u,0u));
  assert(fuel.value==99u&&fuel.countdown==40u);
  fuel=(BmFuelState){1u,1u,1u,0u};
  assert(bm_flight_fuel_step(&fuel,15u,1u)&&fuel.value==0u&&fuel.active);
  fuel.countdown=0u;
  assert(!bm_flight_fuel_step(&fuel,15u,0u));
  assert(fuel.value==0u&&!fuel.active&&fuel.empty&&fuel.countdown==40u);
  assert(!bm_flight_fuel_step(&fuel,6u,1u)&&fuel.countdown==40u);

  {
    BmFlightPosition p={119u,164u,168u,4u,0};uint8_t countdown=2u;
    assert(!bm_flight_deadstick_step(&p,&countdown,15u,13u));
    assert(p.plane_y==164u&&countdown==1u);
    assert(!bm_flight_deadstick_step(&p,&countdown,15u,13u));
    assert(p.plane_y==165u&&p.shadow_y==168u&&countdown==4u);
    countdown=1u;p.plane_y=164u;p.shadow_y=168u;
    assert(!bm_flight_deadstick_step(&p,&countdown,10u,13u));
    assert(p.plane_x==118u&&p.plane_y==164u&&p.shadow_y==167u);
    countdown=4u;p.plane_y=165u;p.shadow_y=168u;
    assert(bm_flight_deadstick_step(&p,&countdown,13u,13u));
    assert(p.plane_y==166u&&countdown==3u);
  }

  {
    uint8_t service=0u;
    fuel=(BmFuelState){98u,2u,0u,1u};
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_WAITING);
    assert(fuel.value==98u&&fuel.countdown==1u&&fuel.empty);
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_WAITING);
    assert(fuel.value==99u&&fuel.countdown==2u&&!fuel.empty);
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_WAITING);
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_WAITING);
    assert(fuel.value==100u&&fuel.countdown==2u);
    fuel.value=299u;fuel.countdown=1u;
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_WAITING);
    assert(fuel.value==300u&&service==0u);
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_FUEL_FULL);
    assert(fuel.value==300u&&service==1u);
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_FUEL_FULL);
    assert(service==2u);
    fuel.value=250u;
    assert(bm_flight_refuel_step(&fuel,&service)==BM_REFUEL_ORDNANCE);
    assert(fuel.value==250u);
  }

  {
    BmLandingState landing={0u};uint16_t landing_speed=103u;
    uint8_t landing_countdown=2u;
    assert(!bm_flight_landing_begin(&landing,1u,1u)&&!landing.active);
    assert(!bm_flight_landing_begin(&landing,0u,0u)&&!landing.active);
    assert(bm_flight_landing_begin(&landing,0u,1u)&&landing.active);
    bm_flight_landing_speed_step(&landing_speed,&landing_countdown);
    assert(landing_speed==103u&&landing_countdown==1u);
    bm_flight_landing_speed_step(&landing_speed,&landing_countdown);
    assert(landing_speed==102u&&landing_countdown==2u);
    bm_flight_landing_speed_step(&landing_speed,&landing_countdown);
    bm_flight_landing_speed_step(&landing_speed,&landing_countdown);
    bm_flight_landing_speed_step(&landing_speed,&landing_countdown);
    bm_flight_landing_speed_step(&landing_speed,&landing_countdown);
    assert(landing_speed==100u);
    assert(bm_flight_landing_step(&landing,100u,4u,1u,8u)==
           BM_LANDING_APPROACH);
    /* LAND (EXXON) has no speed requirement: altitude 3 + P1PF 8 lands. */
    assert(bm_flight_landing_step(&landing,101u,3u,1u,8u)==
           BM_LANDING_GROUND);
    assert(!landing.active);
    assert(bm_flight_landing_begin(&landing,0u,1u));
    assert(bm_flight_landing_step(&landing,100u,3u,1u,0u)==
           BM_LANDING_CRASH);
    assert(bm_flight_landing_begin(&landing,0u,1u));
    assert(bm_flight_landing_step(&landing,100u,4u,0u,0u)==
           BM_LANDING_ABORTED);
  }
  {
    BmGroundService service={28u,2u,2u,2u,5u};uint8_t phase=1u;
    assert(!bm_flight_ground_service_step(&service,&phase));
    assert(service.bombs==28u&&service.bomb_countdown==1u);
    assert(!bm_flight_ground_service_step(&service,&phase));
    assert(service.bombs==29u&&service.bomb_countdown==8u);
    service.bombs=30u;
    { unsigned k;
      /* The full rack is only noticed when the next 8-update wait ends. */
      for(k=0;k<7u;++k)
        assert(!bm_flight_ground_service_step(&service,&phase)&&phase==1u);
    }
    assert(!bm_flight_ground_service_step(&service,&phase)&&phase==2u);
    assert(!bm_flight_ground_service_step(&service,&phase));
    assert(service.damage_level==2u&&service.repair_countdown==1u);
    assert(!bm_flight_ground_service_step(&service,&phase));
    assert(service.damage_level==3u&&service.repair_countdown==90u&&
           service.damage_slots==4u);
    service.damage_level=4u;
    assert(bm_flight_ground_service_step(&service,&phase)&&phase==3u);
  }
  puts("ok: Atari SPD acceleration, deceleration, hold and timer wrap");
  puts("ok: Atari JOYST position transitions, bounds, limiter and crash gate");
  puts("ok: Atari F6-F5 altitude and forty-update GRAV gate");
  puts("ok: Atari TKOSPD digit carries and TAKOF control decisions");
  puts("ok: Atari FUEL drain, empty state and REFUEL display cadence");
  puts("ok: Atari CKLAND gate, SPDL cadence and LAND outcomes");
  puts("ok: Atari INCBMB and FIX ground-service order and cadence");
  puts("ok: Atari empty-fuel KMKZ dead-stick descent and steering");
  return 0;
}
