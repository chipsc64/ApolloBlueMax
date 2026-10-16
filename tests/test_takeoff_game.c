#include "bm_game.h"
#include "bm_geometry.h"
#include "bm_chbas_assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t colour_bounds(const BmFramebuffer *frame,uint8_t colour){
  unsigned i,minx=BM_SCREEN_WIDTH,miny=BM_SCREEN_HEIGHT,maxx=0u,maxy=0u,count=0u;
  for(i=0;i<BM_SCREEN_WIDTH*BM_SCREEN_HEIGHT;++i)if(frame->pixels[i]==colour){
    unsigned x=i%BM_SCREEN_WIDTH,y=i/BM_SCREEN_WIDTH;++count;
    if(x<minx)minx=x;
    if(x>maxx)maxx=x;
    if(y<miny)miny=y;
    if(y>maxy)maxy=y;
  }
  assert(count);
  return (uint32_t)(minx|(miny<<9)|(maxx<<17)|(maxy<<26));
}

int main(void){
  BmGame game;BmInput input={0};unsigned i;
  uint8_t stopped_phase,stopped_source;
  uint32_t speed100,speed200,height4,fuel300,fuel299,bombs30,bombs29;
  uint32_t grounded_without_shadow,airborne_with_shadow;
  uint32_t restart_on_runway,restart_height4;
  uint32_t fighter_wreck_bounds;
  bm_game_init(&game,1234);
  assert(game.mode==BM_STATE_TAKEOFF);
  assert(game.mission_objective_target==8u||game.mission_objective_target==9u);
  assert(game.player.x==150&&game.player.y==150&&game.player.altitude==3);
  assert(game.player.fuel==300&&game.flight_fuel.value==300u);
  bm_game_render(&game);fuel300=bm_framebuffer_checksum(&game.framebuffer);
  game.player.fuel=299;game.flight_fuel.value=299;
  bm_game_render(&game);fuel299=bm_framebuffer_checksum(&game.framebuffer);
  assert(fuel300!=fuel299);
  game.player.fuel=300;game.flight_fuel.value=300;
  bm_game_render(&game);bombs30=bm_framebuffer_checksum(&game.framebuffer);
  game.flight_service.bombs=29u;bm_game_render(&game);
  bombs29=bm_framebuffer_checksum(&game.framebuffer);assert(bombs30!=bombs29);
  /* EXXON.0 CW: 150 idle frames before TAKOF; no scroll, FUEL already
     counts, speed stays 000. */
  { uint8_t phase=game.classic_world.fine_phase;uint16_t top=game.classic_world.top;
    for(i=0;i<150u;++i)bm_game_tick(&game,&input);
    assert(game.mode==BM_STATE_TAKEOFF&&!game.flight_preroll);
    assert(game.classic_world.fine_phase==phase&&game.classic_world.top==top);
    assert(game.flight_fuel.value<300u&&game.takeoff_speed.hundreds==144u);
    /* Continue the takeoff timeline below from a fresh tick/fuel state. */
    game.tick=0u;game.flight_fuel=(BmFuelState){300u,26u,1u,0u};
    game.player.fuel=300;
  }
  game.flight_service.bombs=30u;
  input.pause=1;bm_game_tick(&game,&input);assert(game.mode==BM_STATE_PAUSED);
  input.pause=0;bm_game_tick(&game,&input);assert(game.mode==BM_STATE_PAUSED);
  input.pause=1;bm_game_tick(&game,&input);assert(game.mode==BM_STATE_TAKEOFF);
  input.pause=0;
  for(i=0;i<98u;++i){
    bm_game_tick(&game,&input);
    assert(game.mode==BM_STATE_TAKEOFF);
  }
  assert(game.takeoff_speed.hundreds==144u);
  bm_game_tick(&game,&input);
  assert(game.takeoff_speed.hundreds==81u&&game.mode==BM_STATE_TAKEOFF);
  /* After the font switch TAKOF calls SPD: waiting raises the speed. */
  { static BmGame wait;
    wait=game;for(i=0;i<20u;++i)bm_game_tick(&wait,&input);
    assert(wait.mode==BM_STATE_TAKEOFF&&wait.flight_speed==120u);
    input.y=-1;bm_game_tick(&wait,&input);
    assert(wait.mode==BM_STATE_PLAYING&&wait.flight_speed==121u);
    input.y=0;
  }
  input.y=-1;bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_PLAYING&&game.tick==101u);
  game.flight_low_limit=0u; /* CKCOL is tested separately below */
  assert(game.player.fuel==298&&game.flight_fuel.value==298u);
  /* SPD also runs in the pull-up update: 100 -> 101. */
  assert(game.flight_speed==101u&&game.flight_speed_countdown==4u&&game.player.speed==1);
  /* TRGT: $C4=10 retries Q5 every update; with $FD free and the tank
     approaching ($650<20) Q2 sets $C4=$FD=5 and reloads $6AA to 64..127. */
  game.mission_objective_type=0u;
  game.classic_world.target_sight_active=0u;
  game.classic_world.target_state=10u;game.classic_world.object_slot=0u;
  game.classic_world.enemy_block=1u;
  game.classic_world.tank_delay=10u;input.y=0;
  bm_game_tick(&game,&input);
  assert(game.classic_world.target_sight_request&&
         game.classic_world.target_state==5u&&
         game.classic_world.object_slot==5u&&
         game.classic_world.target_cooldown>=64u&&
         game.classic_world.target_cooldown<=127u);
  game.classic_world.target_sight_request=0u;
  game.classic_world.target_state=253u;
  /* Z9 -> U2 retires both the sight and its $6AD objective at Y>=190. */
  game.classic_world.target_sight_active=1u;
  game.classic_world.target_sight_type=2u;
  game.classic_world.target_sight_y=190u;
  game.classic_world.scroll_clock=1u;
  game.mission_objective_type=2u;
  game.mission_objective_sighted=1u;
  bm_game_tick(&game,&input);
  assert(!game.classic_world.target_sight_active&&
         !game.mission_objective_type&&!game.mission_objective_sighted);
  bm_game_render(&game);speed100=bm_framebuffer_checksum(&game.framebuffer);
  game.flight_speed=200u;game.player.speed=2;bm_game_render(&game);
  speed200=bm_framebuffer_checksum(&game.framebuffer);assert(speed100!=speed200);
  game.flight_speed=100u;game.player.speed=1;
  assert(game.player.y==150&&game.player.altitude==3);
  /* M damage (SC+$657): JOYST only acts when DEC $65A leaves bit 0 clear. */
  { static BmGame damaged;unsigned moves=0u;
    damaged=game;damaged.flight_service.damage_slots=4u;
    input.x=0;input.y=-1;
    for(i=0;i<6u;++i){uint8_t y=damaged.flight_position.plane_y;
      bm_game_tick(&damaged,&input);
      if(damaged.flight_position.plane_y!=y)++moves;}
    assert(moves==3u);
    input.y=0;
  }
  /* TURN: changing to a banking stick below F6-F5=6 is a CRASH. */
  { static BmGame bank;
    bank=game;input.y=0;input.x=-1;bm_game_tick(&bank,&input);
    assert(bank.mode==BM_STATE_CRASHING&&bank.flight_crash_reason==4u);
    input.x=0;
  }
  input.x=0;input.y=-1;bm_game_tick(&game,&input);
  assert(game.player.y==149&&game.player.altitude==4);
  bm_game_render(&game);height4=bm_framebuffer_checksum(&game.framebuffer);
  assert(height4!=speed100);
  input.y=1;bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_PLAYING&&game.player.y==150&&game.player.altitude==3);
  assert(game.flight_speed==102u&&game.player.speed==1);
  for(i=0;i<98u;++i){input.y=0;bm_game_tick(&game,&input);}
  assert(game.flight_speed==200u&&game.player.speed==2);
  input.x=1;input.y=-1;
  for(i=0;i<4u;++i)bm_game_tick(&game,&input);
  assert(game.flight_speed==201u&&game.flight_speed_countdown==2u);
  input.x=0;input.y=0;
  for(i=0;i<4u;++i)bm_game_tick(&game,&input);
  assert(game.flight_speed==200u&&game.flight_speed_countdown==2u&&game.player.speed==2);
  game.flight_gravity_enabled=1;game.flight_gravity_countdown=1;
  { static BmGame slow;
    /* T13: below 200 a falling bomb lands where it is (AFP) and HG ends
       the shot; no impact at the plane. */
    slow=game;slow.flight_speed=150u;
    slow.player_bomb_active=1u;slow.player_bomb_x=100;slow.player_bomb_y=50;
    slow.player_bomb_target_y=120;slow.player_shot_active=1u;
    slow.player_shot_x=200;slow.player_shot_y=40;
    input.y=0;bm_game_tick(&slow,&input);
    printf("dbg %d %d %d %d %d %d\n",slow.mode,slow.player_bomb_active,slow.player_shot_active,slow.player_bomb_explosion_x,slow.player_bomb_explosion_y,slow.classic_world.impact_flash_ticks);
    assert(slow.mode==BM_STATE_CRASHING&&!slow.player_bomb_active&&
           !slow.player_shot_active&&slow.player_bomb_explosion_x==100&&
           slow.player_bomb_explosion_y==50&&
           slow.classic_world.impact_flash_ticks==22u);
  }
  input.y=0;bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_CRASHING&&game.player.altitude==2&&
         game.flight_crash_timer==80u&&game.flight_crash_phase==1u);
  /* At 200+ CRASH calls AFP at F4+3/F5: an impact at the plane. */
  assert(game.player_bomb_explosion_x==game.player.x-2&&
         game.player_bomb_explosion_y==game.player.y-5&&
         game.flight_crash_lift==0u&&game.flight_speed==0u);
  { uint8_t column=game.classic_world.source_column;
    uint8_t phase=game.classic_world.fine_phase;
    for(i=0;i<22u;++i)bm_game_tick(&game,&input);
    /* XSND VBI: no scroll; XFLSH ran out after 22 updates. */
    assert(game.classic_world.source_column==column&&
           game.classic_world.fine_phase==phase&&
           game.classic_world.impact_flash_ticks==0u);
  }
  for(i=0;i<58u;++i)bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_CRASHING&&game.flight_crash_timer==64u&&
         game.flight_crash_phase==2u);
  for(i=0;i<64u;++i)bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_RANKING&&game.flight_crash_wait==1029u&&
         game.flight_crash_phase==3u);
#if BM_CLASSIC_TEST_PAGE
  bm_game_render(&game);
  {
    uint32_t low_score=bm_framebuffer_checksum(&game.framebuffer);
    uint8_t first=game.framebuffer.pixels[184u*BM_SCREEN_WIDTH];
    uint8_t different=0u;
    unsigned pixel;
    for(pixel=184u*BM_SCREEN_WIDTH;pixel<192u*BM_SCREEN_WIDTH;++pixel)
      if(game.framebuffer.pixels[pixel]!=first)different=1u;
    assert(different); /* Original RNKI glyph strip, not the normal debug HUD. */
    game.score=200u;
    bm_game_render(&game);
    assert(bm_framebuffer_checksum(&game.framebuffer)!=low_score);
    {
      uint32_t restart=bm_framebuffer_checksum(&game.framebuffer);
      game.score=3300u;
      bm_game_render(&game);
      assert(bm_framebuffer_checksum(&game.framebuffer)!=restart);
      restart=bm_framebuffer_checksum(&game.framebuffer);
      game.score=6500u;
      bm_game_render(&game);
      assert(bm_framebuffer_checksum(&game.framebuffer)!=restart);
      restart=bm_framebuffer_checksum(&game.framebuffer);
      game.mission_phase=7u;
      bm_game_render(&game);
      assert(bm_framebuffer_checksum(&game.framebuffer)!=restart);
      game.mission_phase=0u;game.score=1000u;
      game.mission_difficulty=2u;game.mission_rank_level=1u;
      game.mission_targets_destroyed=3u;
      bm_game_render(&game);
      assert(bm_framebuffer_checksum(&game.framebuffer)!=restart);
    }
  }
#endif
  /* RTCLOK 19/20=251: CNSL waits 1029 frames; fire does nothing, START
     (CNSL) begins a real game at once. */
  { static BmGame skip;
    skip=game;input.fire=1;bm_game_tick(&skip,&input);
    assert(skip.mode==BM_STATE_RANKING);
    input.fire=0;input.start=1;bm_game_tick(&skip,&input);
    assert(skip.mode==BM_STATE_TAKEOFF&&!skip.demo);
    input.start=0;
  }
  for(i=0;i<1028u;++i){
    bm_game_tick(&game,&input);
    assert(game.mode==BM_STATE_RANKING);
  }
  assert(game.flight_crash_wait==1u&&game.flight_crash_phase==3u);
  bm_game_tick(&game,&input);
  /* ...then START with $6F3=1: the attract mode. */
  assert(game.mode==BM_STATE_TAKEOFF&&game.flight_crash_timer==0u&&
         game.flight_crash_phase==0u&&game.demo);
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  game.flight_speed=200;game.player.speed=2;input.x=0;input.y=-1;
  for(i=0;i<105u;++i)bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_PLAYING&&game.flight_position.plane_y==60u&&
         game.player.y==45&&game.player.altitude==108);
  input.y=1;
  for(i=0;i<105u;++i)bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_PLAYING&&game.flight_position.plane_y==165u&&
         game.player.y==150&&game.player.altitude==3);
  bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_CRASHING&&game.player.altitude==2);
  assert(game.flight_crash_reason==1u);
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  game.flight_speed=200;game.player.speed=2;
  game.flight_fuel=(BmFuelState){1u,1u,1u,0u};
  input.x=1;input.y=-1;bm_game_tick(&game,&input);
  assert(game.player.fuel==0&&game.flight_fuel.active&&!game.flight_fuel.empty);
  game.flight_fuel.countdown=0;input.x=0;input.y=0;
  bm_game_tick(&game,&input);
  assert(game.player.fuel==0&&!game.flight_fuel.active&&game.flight_fuel.empty);
  game.flight_position.plane_y=165u;game.flight_position.shadow_y=168u;
  game.flight_fuel.countdown=1u;
  bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_CRASHING&&game.flight_crash_reason==1u);
  bm_game_init(&game,1234);game.flight_preroll=0u;
  input.y=-1;bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_CRASHING&&game.flight_crash_timer==80u);
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  memset(game.classic_world.rows,0,sizeof(game.classic_world.rows));
  game.classic_world.scroll_clock=0u;
  for(i=0;i<BM_ATARI_PLAYFIELD_FETCH_ROWS;++i)
    memset(game.classic_world.rows[i],0xfcu,sizeof(game.classic_world.rows[i]));
  assert((bm_world_playfield_mask_collision(&game.classic_world,
           bm_asset_plane_frame_0,16u,10u,16u,game.player.x,game.player.y,
           8,5)&1u)==0u);
  for(i=0;i<BM_ATARI_PLAYFIELD_FETCH_ROWS;++i)
    memset(game.classic_world.rows[i],0x00u,sizeof(game.classic_world.rows[i]));
  assert((bm_world_playfield_mask_collision(&game.classic_world,
           bm_asset_plane_frame_0,16u,10u,16u,game.player.x,game.player.y,
           8,5)&1u)==1u);
  for(i=0;i<BM_ATARI_PLAYFIELD_FETCH_ROWS;++i)
    memset(game.classic_world.rows[i],0xfcu,sizeof(game.classic_world.rows[i]));
  input.x=0;input.y=0;input.fire=0;bm_game_tick(&game,&input);
  assert(game.flight_runway_visible&&game.flight_runway_collision==8u);
  game.classic_world.scroll_clock=0u;
  /* CKLAND needs the DRUN window ($609=0, SC+$687 set) and the trigger.
     Ordinary SPD advances 099 to 100 before CKLAND runs this tick. */
  game.classic_world.runway_slot=0u;game.classic_world.runway_marker=0x32u;
  /* Trigger only: with one trigger, trigger + down would release a bomb. */
  game.flight_speed=99u;game.player.speed=1;input.fire=1;input.bomb=0;input.y=0;
  bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_PLAYING&&game.flight_landing.active&&
         game.player.altitude==3&&game.flight_crash_reason==0u);
  /* T12: LAND has no speed condition; only F6-F5=3 and P1PF=8 count. */
  { static BmGame fast,off,city;
    fast=game;fast.classic_world.scroll_clock=0u;fast.flight_speed=180u;
    input.fire=0;input.y=0;bm_game_tick(&fast,&input);
    assert(fast.mode==BM_STATE_GROUND&&!fast.flight_landing.active);
    /* Shadow not wholly on the runway: MK crashes. */
    off=game;off.classic_world.scroll_clock=0u;
    for(i=0;i<BM_ATARI_PLAYFIELD_FETCH_ROWS;++i)
      memset(off.classic_world.rows[i],0x00u,sizeof(off.classic_world.rows[i]));
    bm_game_tick(&off,&input);
    assert(off.mode==BM_STATE_CRASHING&&off.flight_crash_reason==2u);
    /* CKLAND is only called from the SRT loop. */
    city=game;city.flight_landing.active=0u;city.classic_world.landing=0u;
    city.classic_world.world_mode=BM_WORLD_CITY;city.classic_world.scroll_clock=0u;
    city.classic_world.runway_slot=0u;city.classic_world.runway_marker=0x32u;
    input.fire=1;bm_game_tick(&city,&input);
    assert(!city.flight_landing.active);
    input.fire=0;
  }
  game.classic_world.scroll_clock=0u;game.flight_speed=100u;
  input.fire=0;input.y=0;bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_GROUND&&!game.flight_landing.active);
  assert(game.player.altitude==3&&game.flight_crash_reason==0u);
  bm_game_render(&game);
  grounded_without_shadow=bm_framebuffer_checksum(&game.framebuffer);
  game.mode=BM_STATE_PLAYING;bm_game_render(&game);
  airborne_with_shadow=bm_framebuffer_checksum(&game.framebuffer);
  assert(grounded_without_shadow!=airborne_with_shadow);
  /* GROUND calls TURN: a banking stick on the ground crashes. */
  { static BmGame ground;
    ground=game;ground.mode=BM_STATE_GROUND;ground.flight_turn_stick=15u;
    input.x=1;bm_game_tick(&ground,&input);
    assert(ground.mode==BM_STATE_CRASHING&&ground.flight_crash_reason==4u);
    input.x=0;
  }
  game.mode=BM_STATE_GROUND;
  game.flight_fuel.value=298u;game.player.fuel=298;
  input.fire=0;input.y=0;
  for(i=0;i<100u;++i)bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_GROUND&&game.flight_speed==0u&&
         !game.flight_ground_rollout&&game.flight_fuel.countdown==150u);
  stopped_phase=game.classic_world.fine_phase;
  stopped_source=game.classic_world.source_column;
  for(i=0;i<149u;++i)bm_game_tick(&game,&input);
  assert(game.flight_fuel.value==298u&&game.flight_fuel.countdown==1u);
  assert(game.classic_world.fine_phase==stopped_phase&&
         game.classic_world.source_column==stopped_source);
  bm_game_tick(&game,&input);
  assert(game.flight_fuel.value==299u&&game.player.fuel==299&&
         game.flight_fuel.countdown==2u);
  /* T14 TRIGC: each new trigger press advances $6E0. */
  { static BmGame skip;
    skip=game;skip.flight_service.damage_level=2u;
    input.fire=1;bm_game_tick(&skip,&input);
    assert(skip.flight_service_phase==1u&&skip.flight_fuel.value==299u);
    bm_game_tick(&skip,&input);                 /* held: no new press */
    assert(skip.flight_service_phase==1u);
    input.fire=0;bm_game_tick(&skip,&input);
    input.fire=1;bm_game_tick(&skip,&input);
    assert(skip.flight_service_phase==2u&&skip.flight_service.bombs==game.flight_service.bombs);
    input.fire=0;bm_game_tick(&skip,&input);
    assert(skip.flight_service_phase==2u&&skip.flight_service.repair_countdown==88u);
    input.fire=1;bm_game_tick(&skip,&input);
    assert(skip.flight_service_phase==3u&&skip.flight_service_tail==14u&&
           skip.flight_service.damage_level==2u);
    input.fire=0;
    for(i=0;i<13u;++i)bm_game_tick(&skip,&input);
    assert(skip.mode==BM_STATE_GROUND);
    bm_game_tick(&skip,&input);
    assert(skip.mode==BM_STATE_TAKEOFF);
  }
  game.flight_fuel.value=300u;game.player.fuel=300;
  bm_game_tick(&game,&input);assert(game.flight_service_phase==1u);
  /* INCBMB notices the full rack only when its first 34-update wait ends. */
  for(i=0;i<33u;++i){
    bm_game_tick(&game,&input);assert(game.flight_service_phase==1u);
  }
  bm_game_tick(&game,&input);assert(game.flight_service_phase==2u);
  bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_GROUND&&game.flight_service_phase==3u&&
         game.flight_service_tail==14u);
  for(i=0;i<14u;++i)bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_TAKEOFF&&game.flight_service_phase==3u&&
         game.takeoff_speed.hundreds==144u&&game.flight_speed==0u);
  bm_game_render(&game);
  restart_on_runway=bm_framebuffer_checksum(&game.framebuffer);
  game.player.altitude=4;bm_game_render(&game);
  restart_height4=bm_framebuffer_checksum(&game.framebuffer);
  assert(restart_on_runway!=restart_height4);
  game.player.altitude=3;
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  /* The bomb button means trigger + down, so the aircraft also dives:
     release it well above the ground. */
  game.flight_position.shadow_y=200u;
  input.x=0;input.y=0;input.fire=0;input.bomb=1;
  bm_game_tick(&game,&input);
  assert(game.player_bomb_active&&game.flight_service.bombs==29u);
  /* $61F is fixed at release: F6+3 -> framebuffer F6+3-20. */
  assert(game.player_bomb_target_y==bm_pm_y(200+3));
  /* TRIG waits while $620 counts the first 16 bomb updates. */
  input.bomb=0;input.fire=1;bm_game_tick(&game,&input);
  assert(!game.player_shot_active);
  input.fire=0;
  game.player_bomb_active=0u;game.flight_service.bombs=0u;
  bm_game_tick(&game,&input);
  assert(!game.player_bomb_active&&game.flight_service.bombs==0u);
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  input.bomb=0;input.fire=0;input.x=0;input.y=0;
  /* After FBRGN's -2 step the missile is at (150,150): mask pixel (8,5). */
  game.classic_world.gun1_shot_x=123u;
  game.classic_world.gun1_shot_y=172u;
  /* COLM30: F6+$6CB-$65D < $6CC (EAS0 $14/$28): the gun is near F6. */
  game.classic_world.gun1_y=170u;game.classic_world.gun1_origin_x=0u;
  game.classic_world.gun1_shot_direction=2u;
  game.classic_world.gun1_shot_travel=10u;
  bm_game_tick(&game,&input);
  assert(game.classic_world.gun1_shot_y==0u&&
         game.flight_service.damage_level==3u&&
         game.flight_service.damage_slots!=0u&&
         game.flight_damage_cooldown==35u);
  /* COLP02: P2 overlaps P0 and C2-C1 lies in F6-F5-6 .. +11. */
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  game.flight_position.plane_y=165u;game.flight_position.shadow_y=175u;
  game.classic_world.object_slot=2u;game.classic_world.enemy_state=6u;
  game.classic_world.enemy_block=1u;game.classic_world.target_state=252u;
  game.classic_world.enemy_fighter_active=1u;game.classic_world.enemy_body_on=1u;
  game.classic_world.enemy_fighter_x=122u;
  game.classic_world.enemy_fighter_y=171u;
  game.classic_world.enemy_fighter_shadow_y=179u;
  { uint32_t score=game.score;
    bm_game_tick(&game,&input);
    assert(game.mode==BM_STATE_CRASHING&&game.flight_crash_reason==3u);
    /* $654=F6-F5-6, S100, P3 and M2 off; SK hides P2 after CRSH. */
    assert(game.flight_crash_colp02&&game.flight_crash_lift==4u&&
           game.score==score+100u&&!game.classic_world.enemy_shadow_on&&
           game.classic_world.enemy_body_on);
    bm_game_render(&game);
    for(i=0;i<80u;++i)bm_game_tick(&game,&input);
    assert(game.flight_crash_phase==2u&&!game.classic_world.enemy_body_on);
  }
  /* Same overlap, but the player flies 10 lines higher: no collision. */
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  game.flight_position.plane_y=165u;game.flight_position.shadow_y=185u;
  game.classic_world.object_slot=2u;game.classic_world.enemy_state=6u;
  game.classic_world.enemy_block=1u;game.classic_world.target_state=252u;
  game.classic_world.enemy_fighter_active=1u;game.classic_world.enemy_body_on=1u;
  game.classic_world.enemy_fighter_x=122u;
  game.classic_world.enemy_fighter_y=171u;
  game.classic_world.enemy_fighter_shadow_y=179u;
  bm_game_tick(&game,&input);
  assert(game.mode==BM_STATE_PLAYING);
  /* COLM02 in the full Atari flight integration: one bullet in LINE
     destroys the rear fighter, whose XPL1IM wreck then stays at one screen
     position while the terrain keeps moving. */
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  game.flight_speed=200u;game.player.speed=2;
  game.flight_position.plane_y=133u;game.flight_position.shadow_y=160u;
  game.classic_world.object_slot=2u;game.classic_world.enemy_state=5u;
  game.classic_world.enemy_block=1u;game.classic_world.target_state=252u;
  game.classic_world.enemy_fighter_active=1u;game.classic_world.enemy_body_on=1u;
  game.classic_world.enemy_shadow_on=1u;game.classic_world.enemy_in_line=1u;
  game.classic_world.p2_colour=44u;
  game.classic_world.enemy_fighter_x=112u;
  game.classic_world.enemy_fighter_y=110u;
  game.classic_world.enemy_fighter_shadow_y=128u;
  input.x=0;input.y=0;input.fire=0;input.bomb=0;
  { unsigned opaque=0u;
    while(opaque<160u&&bm_asset_fighter_back[opaque]!=10u)++opaque;
    assert(opaque<160u);
    game.player_shot_active=1u;
    game.player_shot_x=(int16_t)(bm_pm_x(112)+(int)(opaque%16u));
    game.player_shot_y=(int16_t)(bm_pm_y(110)+(int)(opaque/16u));
    bm_game_tick(&game,&input);
  }
  assert(game.classic_world.enemy_fighter_destroyed&&game.score==100u);
  bm_game_render(&game);
  fighter_wreck_bounds=colour_bounds(&game.framebuffer,29u);
  {
    uint8_t fighter_x=game.classic_world.enemy_fighter_x;
    uint8_t fighter_y=game.classic_world.enemy_fighter_y;
    uint16_t scroll=game.world_scroll;
    for(i=0;i<40u;++i)bm_game_tick(&game,&input);
  assert(game.classic_world.enemy_fighter_x==fighter_x&&
           game.classic_world.enemy_fighter_y==fighter_y);
    assert(game.world_scroll>scroll);
  }
  bm_game_render(&game);
  assert(colour_bounds(&game.framebuffer,29u)==fighter_wreck_bounds);
  /* COLM20/DAMAGE for the independent TRIGB missile: the aircraft depth
     C2-C1 must lie within F6-F5-$6CD .. +$6CE (EAS0 9/$12). */
  bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
  game.flight_low_limit=0u; /* CKCOL tested separately */
  game.flight_position.plane_y=133u;game.flight_position.shadow_y=160u;
  game.classic_world.object_slot=2u;game.classic_world.enemy_state=5u;
  game.classic_world.enemy_block=1u;game.classic_world.target_state=252u;
  game.classic_world.enemy_fighter_active=1u;game.classic_world.enemy_body_on=1u;
  game.classic_world.enemy_fighter_turn=1u;
  game.classic_world.enemy_fighter_x=30u;game.classic_world.enemy_fighter_y=60u;
  game.classic_world.enemy_fighter_shadow_y=90u;
  game.classic_world.enemy_fighter_shot_active=1u;
  /* RR moves (+3,-3) first, onto plane mask pixel (0,5). */
  game.classic_world.enemy_fighter_shot_x=116u;
  game.classic_world.enemy_fighter_shot_y=141u;
  bm_game_tick(&game,&input);
  assert(!game.classic_world.enemy_fighter_shot_active&&
         game.flight_service.damage_level==3u&&
         game.flight_damage_cooldown==35u);
  /* CKCOL: the shadow's P1PF over the river (PF0+PF1 with COLPF1=$94)
     crashes below $6D6; the runway (PF3) and a higher altitude are safe. */
  { static const uint8_t codes[3]={0x02u,0xfcu,0x02u};
    static const uint8_t shadow[3]={175u,175u,195u};
    static const uint8_t crash[3]={1u,0u,0u};
    unsigned k;
    for(k=0;k<3u;++k){
      bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;
      game.flight_position.plane_y=165u;game.flight_position.shadow_y=shadow[k];
      game.classic_world.object_slot=2u;game.classic_world.enemy_state=6u;
      game.classic_world.enemy_block=1u;
      memset(game.classic_world.arena,codes[k],sizeof(game.classic_world.arena));
      memset(game.classic_world.rows,codes[k],sizeof(game.classic_world.rows));
      game.classic_world.vbi_counter=5u;game.flight_low_limit=16u;
      input.x=0;input.y=0;input.fire=0;input.bomb=0;
      bm_game_tick(&game,&input);
      if(crash[k])assert(game.mode==BM_STATE_CRASHING&&game.flight_crash_reason==5u);
      else assert(game.mode==BM_STATE_PLAYING);
    }
  }
  /* Damage letters: F doubles FUEL's $63A decrement, G gates TRIG to
     RANDOM&31==0, WIND moves F4 once per coarse row. */
  { unsigned launches[2]={0u,0u},k,t;
    bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;game.flight_low_limit=0u;
    game.flight_position.shadow_y=205u;game.flight_fuel.countdown=20u;
    game.flight_service.damage_slots=1u;input.x=0;input.y=0;input.fire=0;
    bm_game_tick(&game,&input);
    assert(game.flight_fuel.countdown==18u);
    for(k=0;k<2u;++k){
      bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;game.flight_low_limit=0u;
      game.flight_position.shadow_y=205u;
      game.classic_world.object_slot=2u;game.classic_world.enemy_state=6u;
      game.classic_world.enemy_block=1u;
      game.flight_service.damage_slots=k?8u:0u;input.fire=1;
      for(t=0;t<320u;++t){uint8_t was=game.player_shot_active;
        int16_t old_x=game.player_shot_x;
        bm_game_tick(&game,&input);
        if(game.player_shot_active&&(!was||game.player_shot_x<old_x))
          ++launches[k];}
    }
    assert(launches[0]>20u&&launches[1]*2u<=launches[0]);
    input.fire=0;
    bm_game_init(&game,1234);game.mode=BM_STATE_PLAYING;game.flight_low_limit=0u;
    /* F6=168 blocks the BACK drift, so only the wind moves F4. */
    game.flight_position.plane_y=128u;game.flight_position.shadow_y=168u;
    game.wind_timer=50u;game.wind_left=0u;
    { uint8_t x=game.flight_position.plane_x;unsigned rows=0u;
      for(t=0;t<40u;++t){bm_game_tick(&game,&input);
        if(game.classic_world.coarse_event)++rows;}
      assert(rows>=4u&&game.flight_position.plane_x>=x+rows-1u);
    }
  }
  /* T17 CKSC: thousands digit + $6AC selects the EAS table. */
  { static BmGame d;
    bm_game_init(&d,1234);d.mode=BM_STATE_PLAYING;d.flight_low_limit=0u;
    assert(d.classic_world.enemy_fire_chance==0x10u&&
           d.classic_world.wind_chance==4u&&d.classic_world.wind_rows==2u&&
           d.classic_world.air_depth_bias==9u&&
           d.classic_world.air_depth_range==0x12u&&
           d.classic_world.runway_interval==0xa5u);
    input.x=0;input.y=0;input.fire=0;input.bomb=0;
    d.score=900u;bm_game_tick(&d,&input);           /* sum 0: keep EAS0 */
    assert(d.classic_world.enemy_fire_chance==0x10u);
    d.score=1000u;bm_game_tick(&d,&input);          /* sum 1: EAS2 */
    assert(d.classic_world.enemy_fire_chance==0x40u&&
           d.classic_world.wind_rows==4u&&d.classic_world.gun_depth_bias==0x19u);
    d.mission_targets_destroyed=1u;d.score=3000u;   /* sum 4: EAS4 */
    bm_game_tick(&d,&input);
    assert(d.classic_world.enemy_fire_chance==0x80u);
    d.score=5000u;bm_game_tick(&d,&input);          /* sum 6: EAS6 */
    assert(d.classic_world.enemy_fire_chance==0xb0u);
    d.score=7000u;bm_game_tick(&d,&input);          /* sum 8: EAS8 */
    assert(d.classic_world.enemy_fire_chance==0xe0u);
    d.score=8000u;bm_game_tick(&d,&input);          /* sum 9: EAS9 */
    assert(d.classic_world.enemy_fire_chance==0xffu&&
           d.classic_world.wind_chance==7u&&d.classic_world.air_depth_range==0x1cu);
    /* Intermediate ($6E7=1) skips EAS2/EAS4. */
    bm_game_init(&d,1234);d.mode=BM_STATE_PLAYING;d.flight_low_limit=0u;
    d.mission_rank_level=1u;d.score=2000u;bm_game_tick(&d,&input);
    assert(d.classic_world.enemy_fire_chance==0x10u);
    /* Five digits or all objectives: EAS9. */
    d.mission_rank_level=0u;d.score=12000u;bm_game_tick(&d,&input);
    assert(d.classic_world.enemy_fire_chance==0xffu);
    bm_game_init(&d,1234);d.mode=BM_STATE_PLAYING;d.flight_low_limit=0u;
    d.mission_targets_destroyed=d.mission_objective_target;
    bm_game_tick(&d,&input);
    assert(d.classic_world.enemy_fire_chance==0xffu);
  }
  /* T19: TRIG calls SHBT after each step, only below altitude 28. */
  { static BmGame b;unsigned base,col,row;int16_t sx,sy;
    bm_game_init(&b,1234);b.mode=BM_STATE_PLAYING;b.flight_low_limit=0u;
    b.player_shot_active=1u;b.player_shot_x=100;b.player_shot_y=100;
    sx=(int16_t)(b.player_shot_x+16);sy=(int16_t)(b.player_shot_y-8);
    col=(unsigned)(((bm_pm_hpos(sx)-26)>>2)-2);
    row=(unsigned)(((bm_pm_scanline(sy)-8)>>3)-1);
    base=b.classic_world.lms[row]+b.classic_world.source_column+col;
    b.classic_world.arena[base]=198u;
    { static BmGame high;uint32_t score;
      high=b;high.flight_position.shadow_y=(uint8_t)(high.flight_position.plane_y+40u);
      input.x=0;input.y=0;input.fire=0;input.bomb=0;
      bm_game_tick(&high,&input);
      assert(high.player_shot_active&&high.classic_world.arena[base]==198u);
      score=b.score;bm_game_tick(&b,&input);
      assert(b.player.altitude<28&&!b.player_shot_active&&b.score==score+10u&&
             b.classic_world.impact_mode==1u);
    }
  }
  /* T18a PRIOR (CITY): entry clears $C5, $6EA=F6/2+F4 selects the zone. */
  { static BmGame c;unsigned r;
    bm_game_init(&c,1234);c.mode=BM_STATE_PLAYING;c.flight_low_limit=255u;
    c.classic_world.world_mode=BM_WORLD_CITY;c.classic_world.area_type=6u;
    c.classic_world.area_delay=50u;
    c.classic_world.prior_entry=90u;c.classic_world.prior_exit=0u;
    for(r=0;r<BM_ATARI_PLAYFIELD_FETCH_ROWS;++r)
      memset(c.classic_world.rows[r],0xfeu,sizeof(c.classic_world.rows[r]));
    c.classic_world.fine_phase=3u;c.classic_world.scroll_clock=1u;
    input.x=0;input.y=0;input.fire=0;input.bomb=0;
    bm_game_tick(&c,&input);
    /* No CKCOL crash although $6D6=255: PRIOR cleared $C5 first. */
    assert(c.mode==BM_STATE_PLAYING&&!c.classic_world.prior_entry&&
           !c.classic_world.ckcol_enabled);
    assert(c.classic_world.prior_depth==(uint8_t)(168u/2u+119u)&&
           c.classic_world.gprior==1u&&!c.classic_world.shadow_hidden);
    c.flight_position.plane_x=140u;c.classic_world.scroll_clock=1u;
    bm_game_tick(&c,&input);
    assert(c.classic_world.gprior==4u&&!c.classic_world.shadow_hidden);
    { static BmGame high,low;
      c.flight_position.plane_x=170u;              /* $6EA=254: building */
      for(r=0;r<BM_ATARI_PLAYFIELD_FETCH_ROWS;++r)
        memset(c.classic_world.rows[r],0x12u,sizeof(c.classic_world.rows[r]));
      high=c;high.flight_position.plane_y=80u;   /* altitude 88 */
      high.classic_world.scroll_clock=1u;bm_game_tick(&high,&input);
      assert(high.mode==BM_STATE_PLAYING&&high.classic_world.shadow_hidden);
      low=c;low.classic_world.scroll_clock=1u;bm_game_tick(&low,&input);
      assert(low.mode==BM_STATE_CRASHING&&low.flight_crash_reason==7u);
    }
    /* Area 8: P0 over PF0/PF3 again restores $C5 and 623=1 ($686). */
    c.flight_position.plane_x=119u;c.classic_world.area_type=8u;
    for(r=0;r<BM_ATARI_PLAYFIELD_FETCH_ROWS;++r)
      memset(c.classic_world.rows[r],0x00u,sizeof(c.classic_world.rows[r]));
    c.classic_world.scroll_clock=1u;c.flight_low_limit=0u;
    bm_game_tick(&c,&input);
    assert(c.classic_world.prior_exit&&c.classic_world.ckcol_enabled&&
           c.classic_world.gprior==1u);
  }
  /* T18b: AFB, BMCPTL and COLCPTL. */
  { static BmGame c,b;
    bm_game_init(&c,1234);c.mode=BM_STATE_PLAYING;c.flight_low_limit=0u;
    c.classic_world.world_mode=BM_WORLD_CITY;c.classic_world.area_type=6u;
    c.classic_world.area_delay=50u;c.classic_world.prior_entry=0u;
    c.classic_world.prior_exit=1u;c.classic_world.ckcol_enabled=0u;
    c.classic_world.fine_phase=3u;
    /* AFB: $6EA outside 150..242 -> $6EB=1 and $61F-80. */
    b=c;b.classic_world.prior_depth=250u;b.flight_position.shadow_y=200u;
    input.x=0;input.y=0;input.fire=0;input.bomb=1;
    b.classic_world.scroll_clock=1u;bm_game_tick(&b,&input);
    assert(b.player_bomb_active&&b.classic_world.bomb_zone==1u);
    input.bomb=0;
    /* BMCPTL: the bomb lands on the colour-112 target: bit 2, S500. */
    b=c;b.classic_world.capital_y=120u;b.classic_world.capital_x=110u;
    b.classic_world.capital_size=1u;b.classic_world.p2_colour=112u;
    b.classic_world.object_slot=20u;
    b.player_bomb_active=1u;b.player_bomb_x=(int16_t)(bm_pm_x(110)+2);
    b.player_bomb_y=b.player_bomb_target_y=(int16_t)bm_pm_y(120-6);
    { uint32_t score=b.score;
      b.classic_world.scroll_clock=1u;bm_game_tick(&b,&input);
      assert(b.mission_phase==2u&&b.score>=score+500u&&
             b.classic_world.capital_wreck&&b.player_bomb_explosion==0u);
    }
    /* COLCPTL: shadow over the target below altitude 25 crashes. */
    b=c;b.classic_world.capital_size=1u;b.classic_world.p2_colour=50u;
    b.classic_world.capital_x=(uint8_t)(b.flight_position.plane_x-4u);
    b.classic_world.capital_y=(uint8_t)(b.flight_position.shadow_y+6u);
    b.classic_world.object_slot=20u;
    b.classic_world.scroll_clock=1u;bm_game_tick(&b,&input);
    assert(b.mode==BM_STATE_CRASHING&&b.flight_crash_reason==8u);
  }
  /* T31: EB/AFY - the attract mode waits for PLAY.IT before AFX and CW;
     the propeller only starts ($C1) when the tune is over. */
  { static BmGame d;unsigned f=0;
    bm_game_boot(&d,99u);
    input.x=0;input.y=0;input.fire=0;input.bomb=0;
    assert(d.demo&&d.music_wait&&d.mode==BM_STATE_TAKEOFF);
    while(d.music_wait&&f<4000u){bm_game_tick(&d,&input);++f;}
    assert(f==2713u&&d.pokey.audc[0]==0xc1u&&d.flight_preroll);
  }
  /* T18c: STOPL with $6C2=7 ends the game; RNK, then START. */
  { static BmGame e;uint32_t before,after;
    bm_game_init(&e,1234);e.mode=BM_STATE_GROUND;e.flight_ground_rollout=1u;
    e.flight_speed=1u;e.mission_phase=7u;e.flight_turn_stick=15u;
    input.x=0;input.y=0;input.fire=0;input.bomb=0;
    bm_game_tick(&e,&input);
    assert(e.mode==BM_STATE_GAME_OVER&&e.end_timer==270u&&!e.flight_crash_reason);
    bm_game_render(&e);before=bm_framebuffer_checksum(&e.framebuffer);
    for(i=0;i<270u;++i)bm_game_tick(&e,&input);
    assert(e.mode==BM_STATE_GAME_OVER&&!e.end_timer);
    bm_game_render(&e);after=bm_framebuffer_checksum(&e.framebuffer);
    assert(before!=after);                     /* RNK status line */
    e.play_frames=200u*256u;bm_game_render(&e);  /* V=200: FTG */
    assert(bm_framebuffer_checksum(&e.framebuffer)!=after);
    e.play_frames=271u;
    for(i=0;i<50u;++i)bm_game_tick(&e,&input);
    assert(e.mode==BM_STATE_GAME_OVER);
    input.fire=1;bm_game_tick(&e,&input);
    assert(e.mode==BM_STATE_GAME_OVER);         /* fire: nothing */
    input.fire=0;input.start=1;bm_game_tick(&e,&input);
    assert(e.mode==BM_STATE_TAKEOFF&&!e.mission_phase&&!e.demo);
    input.start=0;
  }
  /* T24a: CNSL/MENU.  OPTION opens the menu, SELECT changes the cursor
     line, OPTION moves the cursor, START starts with the choices kept. */
  { static BmGame m;BmInput k={0};
    bm_game_init(&m,1234);
    k.option=1;bm_game_tick(&m,&k);
    assert(m.mode==BM_STATE_OPTIONS&&m.menu_cursor==0u);
    bm_game_tick(&m,&k);assert(m.menu_cursor==0u);     /* held: no edge */
    k.option=0;bm_game_tick(&m,&k);
    k.select=1;bm_game_tick(&m,&k);assert(m.mission_rank_level==1u);
    k.select=0;bm_game_tick(&m,&k);
    k.option=1;bm_game_tick(&m,&k);assert(m.menu_cursor==1u);
    k.option=0;bm_game_tick(&m,&k);
    k.select=1;bm_game_tick(&m,&k);assert(m.option_reversed==1u);
    k.select=0;bm_game_tick(&m,&k);
    k.option=1;bm_game_tick(&m,&k);k.option=0;bm_game_tick(&m,&k);
    k.select=1;bm_game_tick(&m,&k);assert(m.mission_difficulty==1u);
    k.select=0;bm_game_tick(&m,&k);
    bm_game_render(&m);
    k.start=1;bm_game_tick(&m,&k);
    assert(m.mode==BM_STATE_TAKEOFF&&m.mission_rank_level==1u&&
           m.option_reversed&&m.mission_difficulty==1u&&
           m.flight_gravity_enabled==1u&&
           m.classic_world.enemy_fire_chance==0xb0u&&
           m.classic_world.runway_interval==180u);
    /* START during play restarts and keeps the choices. */
    k.start=0;m.mode=BM_STATE_PLAYING;m.score=500u;
    k.start=1;bm_game_tick(&m,&k);
    assert(m.mode==BM_STATE_TAKEOFF&&!m.score&&m.mission_rank_level==1u);
    /* Power-on init resets them. */
    bm_game_init(&m,1234);
    assert(!m.mission_rank_level&&!m.option_reversed&&!m.mission_difficulty);
  }
  /* T25 SHCOL/TURN shadow rows and QUAKE. */
  { static BmGame h;const uint8_t *px;unsigned c;BmInput k={0};
    bm_game_init(&h,1234);h.mode=BM_STATE_PLAYING;h.flight_low_limit=0u;
    h.flight_position.shadow_y=168u;h.flight_position.plane_y=88u; /* 80 */
    h.classic_world.scroll_clock=1u;bm_game_tick(&h,&k);
    assert(h.player.altitude==80&&h.shadow_far);
    px=bm_game_shadow_pixels(&h);
    for(c=0;c<16u;++c)assert(!px[c]&&!px[7u*16u+c]);       /* rows 0/7 */
    assert(px[16u+2u]&&px[16u+10u]&&!px[16u+12u]);         /* row 1 $44 */
    h.flight_position.plane_y=89u;h.classic_world.scroll_clock=1u;
    bm_game_tick(&h,&k);assert(h.player.altitude==79&&!h.shadow_far);
    h.player.bank=-1;px=bm_game_shadow_pixels(&h);
    assert(!px[0]&&px[12u]&&px[7u*16u+2u]&&!px[7u*16u+14u]); /* $02/$40 */
    h.player.bank=0;px=bm_game_shadow_pixels(&h);
    assert(px[0]&&px[12u]&&px[7u*16u+2u]&&px[7u*16u+14u]);  /* $82/$41 */
    /* QUAKE: positive $6BE counts down with HSCROL-1 when F1 != 0. */
    h.classic_world.quake=2;h.classic_world.fine_phase=2u;
    h.classic_world.scroll_clock=0u;
    bm_world_buffer_step(&h.classic_world);
    assert(h.classic_world.quake==1&&h.classic_world.quake_shift);
    h.classic_world.quake=-1;bm_world_buffer_step(&h.classic_world);
    assert(h.classic_world.quake==-1&&!h.classic_world.quake_shift);
  }
  /* T28 PBMB: on the ground with $FD=255 an attacker comes (1/64), flies
     through L6 and $FD returns to 255; a bomb on P0 crashes (reason 9). */
  { static BmGame a;BmInput k={0};unsigned t,spawned=0,back=0;
    bm_game_init(&a,77);a.mode=BM_STATE_GROUND;a.flight_ground_rollout=0u;
    a.flight_service_phase=3u;a.flight_service_tail=0u;
    a.flight_service.damage_level=4u;a.flight_service.bombs=30u;
    a.flight_fuel.value=300u;a.flight_turn_stick=15u;
    a.classic_world.object_slot=255u;
    for(t=0;t<3000u&&!back;++t){
      uint8_t before=a.classic_world.object_slot;
      a.flight_service_tail=0u;a.flight_service_phase=0u;   /* stay parked */
      a.flight_fuel.countdown=150u;
      bm_game_tick(&a,&k);
      if(a.mode!=BM_STATE_GROUND)break;
      if(before==255u&&a.classic_world.object_slot==254u){
        ++spawned;
        assert(a.classic_world.enemy_state==4u&&
               a.classic_world.enemy_fighter_y==186u);
      }
      if(spawned&&before==254u&&a.classic_world.object_slot==255u)back=1u;
    }
    assert(spawned&&(back||a.mode==BM_STATE_CRASHING));
    bm_game_init(&a,77);a.mode=BM_STATE_GROUND;a.flight_turn_stick=15u;
    a.flight_service_phase=0u;a.flight_fuel.countdown=150u;
    a.classic_world.object_slot=254u;a.classic_world.enemy_state=5u;
    a.classic_world.enemy_fighter_active=1u;
    a.classic_world.enemy_fighter_x=200u;
    a.player_bomb_active=1u;a.player_bomb_phase=0u;
    a.player_bomb_x=(int16_t)(a.player.x-2);a.player_bomb_y=(int16_t)(a.player.y-3);
    a.player_bomb_target_y=(int16_t)(a.player.y+20);
    a.play_frames=0u;bm_game_tick(&a,&k);
    assert(a.mode==BM_STATE_CRASHING&&a.flight_crash_reason==9u);
  }
  puts("ok: optional BmGame takeoff reaches 100 then accepts pull-up");
  return 0;
}
