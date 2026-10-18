#include "bm_game.h"
#include "bm_chbas_assets.h"
#include "bm_geometry.h"
#include "bm_collision.h"
#include <string.h>
#ifndef BM_ATARI_FLIGHT
#define BM_ATARI_FLIGHT 0
#endif
#ifndef BM_CAPITAL_TEST
#define BM_CAPITAL_TEST 0
#endif
#ifndef BM_ENHANCED_SOUND
#define BM_ENHANCED_SOUND 0
#endif
#if !BM_ATARI_FLIGHT
static int16_t clamp16(int16_t v,int16_t lo,int16_t hi){return v<lo?lo:(v>hi?hi:v);}
#endif
#ifndef BM_SOUND_SAMPLES
#define BM_SOUND_SAMPLES 0
#endif
/* Sound output: one POKEY for every group, or (ENHANCED_SOUND) one each.
   The game only raises bm_sound_event_* events; it never writes POKEY
   registers itself. */
static const BmSoundOut *sound_out(BmGame *g){
  unsigned i;
  for(i=0;i<BM_SOUND_GROUPS;++i)
    g->sound_out.pokey[i]=(BM_ENHANCED_SOUND&&i)?&g->pokey_extra[i-1u]:&g->pokey;
  g->sound_out.separate=(uint8_t)BM_ENHANCED_SOUND;
  g->sound_out.mixer=BM_SOUND_SAMPLES?&g->mixer:0;
  return &g->sound_out;
}
/* EXXON.0 EB: in the attract mode ($6F3) PLAY.IT runs first and AFY waits
   (CNSL, DFLSH) until the tune clears $6FF; only then AFX and CW. */
static void demo_music(BmGame *g){
  g->music_wait=1u;bm_sound_event_demo_music(&g->sound,sound_out(g));
}
/* EXXON.6 XXX: an explosion sound (XSND runs it in the VBI). */
static void sound_explosion(BmGame *g,BmExplosion kind){
  bm_sound_event_explosion(&g->sound,sound_out(g),kind);
}
/* S7 (EXXON.12): an object drawn in a bonus colour ($7C, 50 or $A8) counts
   as a destroyed objective ($6AC) and scores two extra S100.  It falls
   through to R7: $66F=120 and XXX 50,$4C. */
static void score_s7(BmGame *g){
  sound_explosion(g,BM_EXPLOSION_OBJECT);
  uint8_t colour=g->classic_world.p2_colour;
  if(colour==0x7cu||colour==50u||colour==0xa8u){
    if(g->mission_targets_destroyed<255u)++g->mission_targets_destroyed;
    g->score+=200u;
  }
  g->score+=100u;
}
static uint8_t enemy_p2_pixel(const uint8_t *mask,int index,uint8_t p2_value){
  return (uint8_t)(p2_value?mask[index]==p2_value:mask[index]!=0u);
}
static uint8_t bomb_ship(BmGame *g,int sx,int sy){
  BmWorldBuffer *w=&g->classic_world;int dx,dy;
  if(!w->ship_active||w->ship_destroyed)return 0;
  /* BMBSHP: missile 1 overlaps either P2 or P3, then straight to Z6.
     Unlike SHP23 there is no three-hit counter or low-flight gate. */
  for(dy=-1;dy<=1;++dy)for(dx=0;dx<2;++dx){
    int px=sx+dx,py=sy+dy;
    int x=px-bm_pm_x(w->ship_x),y=py-bm_pm_y((int)w->ship_y-11);
    if(px<0||px>=320||py<0||py>=BM_ATARI_PLAYFIELD_HEIGHT||!bm_mask_ship(x,y))continue;
    w->ship_destroyed=1;w->ship_destroy_timer=120;w->ship_hits_remaining=0;
    w->gun2_shot_y=0;w->gun2_shot_explosion=0;
    w->impact_flash_ticks=12; /* BMBSHP -> ABA */
    bm_sound_event_shot_end(&g->sound);  /* BMBSHP: $648=255 */
    score_s7(g);
    g->player_bomb_explosion=0;g->player_bomb_target_hit=1;
    g->player_bomb_water_splash=0;g->player_bomb_grass_impact=0;
    return 1;
  }
  return 0;
}
static uint8_t count_bomb_objective(BmGame *g){
  uint8_t expected=g->mission_objective_type==1u?4u:
    (g->mission_objective_type==2u?5u:
     (g->mission_objective_type==3u?3u:0u));
  /* EXXON.12 W4/W7/W0 increments $6AC only when $6AD identifies the
     matching large tank, building or bridge family. */
  if(expected&&g->classic_world.impact_mode==expected&&
     g->mission_targets_destroyed<255u){
    ++g->mission_targets_destroyed;
    /* U2 clears $6AD when the marked target is retired. */
    g->mission_objective_type=0u;
    g->mission_objective_sighted=0u;
    return 1u;
  }
  return 0u;
}
static void hit_ship(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;int step,p;
  if(!g->player_shot_active||g->player.altitude>=28||!w->ship_active||
     w->ship_destroyed)return;
  for(step=0;step<=16;++step)for(p=0;p<4;++p){
    int sx=g->player_shot_x+step+p,sy=g->player_shot_y-step/2;
    int x=sx-bm_pm_x(w->ship_x),y=sy-bm_pm_y((int)w->ship_y-11);
    if(sx<0||sx>=320||sy<0||sy>=BM_ATARI_PLAYFIELD_HEIGHT||!bm_mask_ship(x,y))continue;
    g->player_shot_active=0; /* SHP23 -> HG */
    /* DEC $6DC: the counter is shared and only SHIP reloads it with 3. */
    w->ship_hits_remaining=--w->p2_hits;
    if(!w->p2_hits){
      w->ship_destroyed=1;w->ship_destroy_timer=120;
      w->gun2_shot_y=0;w->gun2_shot_explosion=0;
      score_s7(g);
    }
    return;
  }
}
/* EXXON.8 COLM02: only while LINE holds ($66E=$92) does a bullet on P2
   destroy the aircraft, at once.  SHP23 is not called for aircraft. */
static void hit_enemy_fighter(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  const uint8_t *fighter;
  int height,step,pixel;
  uint8_t p2_value;
  int x=bm_pm_x(w->enemy_fighter_x);
  int y=bm_pm_y(w->enemy_fighter_y);
  if(!g->player_shot_active||!w->enemy_in_line||!w->enemy_body_on||
     w->enemy_fighter_destroyed)return;
  fighter=bm_mask_enemy_fighter(w->enemy_fighter_forward,w->enemy_fighter_turn,
                                &height,&p2_value);
  /* M0PL is latched over TRIG's (+16,-8) step. */
  for(step=0;step<=16;++step)for(pixel=0;pixel<4;++pixel){
    int sx=g->player_shot_x+step+pixel;
    int sy=g->player_shot_y-step/2;
    int fx=sx-x,fy=sy-y;
    if(fx<0||fx>=16||fy<0||fy>=height||
       !enemy_p2_pixel(fighter,fy*16+fx,p2_value))continue;
    g->player_shot_active=0u;                 /* HG */
    if(w->p2_colour==0x7cu||w->p2_colour==0xa8u){
      if(g->mission_targets_destroyed<255u)++g->mission_targets_destroyed;
      g->score+=200u;
    }
    if(w->object_slot!=2u)g->score+=200u;     /* PLFWD scores 300 */
    g->score+=100u;
    w->enemy_in_line=0u;
    w->enemy_shadow_on=0u;                    /* P3 cleared */
    w->enemy_fighter_destroyed=1u;            /* $B1=0 */
    w->enemy_fighter_destroy_timer=100u;      /* $66F=100 */
    w->impact_flash_ticks=12u;                /* ABA */
    bm_sound_event_shot_end(&g->sound);       /* VB: RO */
    sound_explosion(g,BM_EXPLOSION_AIRCRAFT);  /* VB: XXX 50,$4C */
    return;
  }
}
static void river_vehicle_terrain_collision(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  const uint8_t *body,*tyres,*charset;
  int height,cx,cy,x=bm_pm_x(w->river_car_x),y=bm_pm_y(w->river_car_y);
  if(!w->river_car_active||w->river_car_destroyed||w->river_car_y<70u)return;
  charset=bm_mask_playfield_charset(w->tank_charset_loaded);
  bm_mask_vehicle(w->river_vehicle_truck,w->river_car_left,&body,&tyres,&height);
  /* XP23 reads P2PF/P3PF ($D006/$D007), NOT missile latches.
     Bit 1 is COLPF1: the water pixels exposed by a broken bridge.
     Match opaque body/tyre pixels against ANTIC pair 10, not the
     whole tile rectangle or the current RGB palette (which flashes). */
  for(cy=0;cy<=height;++cy)for(cx=0;cx<16;++cx){
    int sx=x+cx,sy=y+cy,tx,ty;
    uint8_t code,bits;
    if(!((cy<height&&body[cy*16+cx])||
         (cy>0&&tyres[(cy-1)*16+cx])))continue;
    if(sx<0||sx>=BM_SCREEN_WIDTH||sy<0||sy>=BM_ATARI_PLAYFIELD_HEIGHT)continue;
    sx+=8-w->fine_phase*2;sy+=w->fine_phase*2;
    tx=sx/8+w->source_column;ty=sy/8;
    if(tx<0||tx>=BM_ATARI_PLAYFIELD_FETCH_COLUMNS||ty>=BM_ATARI_PLAYFIELD_FETCH_ROWS)continue;
    code=w->rows[ty][tx];
    bits=charset[(code&127u)*8u+(sy&7)];
    if(((bits>>(6-((sx&7)/2)*2))&3u)!=2u)continue;
    w->river_car_destroyed=1u;w->river_car_destroy_timer=120u;
    score_s7(g);
    return;
  }
}
static void hit_river_vehicle(BmGame *g,int from_x,int from_y){
  BmWorldBuffer *w=&g->classic_world;
  const uint8_t *body,*tyres;int height,step,pixel;
  int x=bm_pm_x(w->river_car_x),y=bm_pm_y(w->river_car_y);
  /* SHP23 gates M0PL/P2/P3 hits on F6-F5 < 28. */
  if(!w->river_car_active||w->river_car_destroyed||g->player.altitude>=28)return;
  bm_mask_vehicle(w->river_vehicle_truck,w->river_car_left,&body,&tyres,&height);
  /* Existing direct-shot behaviour: sweep the four-pixel missile across
     its (+16,-8) update. XP23 itself handles terrain, not this test. */
  for(step=0;step<=16;++step)for(pixel=0;pixel<4;++pixel){
    int sx=from_x+step+pixel,sy=from_y-step/2;
    int cx=sx-x,cy=sy-y;
    if(sx<0||sx>=BM_SCREEN_WIDTH||sy<0||sy>=BM_ATARI_PLAYFIELD_HEIGHT||
       cx<0||cx>=16)continue;
    if((cy>=0&&cy<height&&body[cy*16+cx])||
       (cy>=1&&cy<=height&&tyres[(cy-1)*16+cx])){
      w->river_car_destroyed=1u;w->river_car_destroy_timer=120u;
      /* SHP23 -> HG ($19E1): clear F9 and its missile byte on a hit. */
      g->player_shot_active=0u;
      score_s7(g); /* SHP23 -> R6 -> S7 */
      return;
    }
  }
}
/* SHDW copies P1IMAGE ($82 $46 $2C $18 $3C $C2 $C2? see asset) to P1.
   SHCOL at F6-F5=80 clears rows 0/7 and writes $44/$42 to rows 1/6 (and
   P1 colour 2); TURN below 80 writes rows 0/7 as $02/$40 while banking
   (BJ) and $82/$41 otherwise (CK). */
const uint8_t *bm_game_shadow_pixels(const BmGame *g){
  static uint8_t pixels[16*8];
  uint8_t rows[8];
  unsigned r,c;
  for(r=0;r<8u;++r){
    uint8_t bits=0u;
    for(c=0;c<8u;++c)if(bm_mask_shadow()[r*16u+c*2u])bits|=(uint8_t)(0x80u>>c);
    rows[r]=bits;
  }
  if(g->shadow_far){rows[0]=0u;rows[7]=0u;rows[1]=0x44u;rows[6]=0x42u;}
  else if(g->player.bank){rows[0]=0x02u;rows[7]=0x40u;}
  for(r=0;r<8u;++r)for(c=0;c<16u;++c)
    pixels[r*16u+c]=(uint8_t)((rows[r]&(0x80u>>(c/2u)))?9u:0u);
  return pixels;
}
static uint8_t flight_joystick(const BmInput *in){
  if(in->y<0)return in->x<0?10u:(in->x>0?6u:14u);
  if(in->y>0)return in->x<0?9u:(in->x>0?5u:13u);
  return in->x<0?11u:(in->x>0?7u:15u);
}
static void project_flight(BmGame *g){
  /* DRP0 draws rows F5..F5+9 at HPOS F4; the sprite anchor is (8,5). */
  g->player.x=(int16_t)(bm_pm_x(g->flight_position.plane_x)+8);
  g->player.y=(int16_t)(bm_pm_y(g->flight_position.plane_y)+5);
  g->player.altitude=(int16_t)bm_flight_altitude(&g->flight_position);
}
/* Damage letters SC+$655..$658 as bits of BmGroundService.damage_slots. */
enum { BM_DAMAGE_F=1u, BM_DAMAGE_B=2u, BM_DAMAGE_M=4u, BM_DAMAGE_G=8u };
#if BM_ATARI_FLIGHT
/* EXXON.2 TURN: only a change of stick code acts.  11/10 bank left, 7/5
   bank right (BJ), everything else draws the level plane (CK).  Banking
   below F6-F5=6 is a CRASH. */
static uint8_t flight_turn(BmGame *g,uint8_t stick){
  if(stick==g->flight_turn_stick)return 0u;
  g->flight_turn_stick=stick;
  if(stick==11u||stick==10u||stick==5u||stick==7u){
    if(bm_flight_altitude(&g->flight_position)<6u)return 1u;
    g->player.bank=(int8_t)(stick==7u||stick==5u?1:-1);
  }else g->player.bank=0;
  return 0u;
}
#endif
#if BM_ATARI_FLIGHT
/* EXXON.1 CKCOL: P1PF of the shadow.  Only PF0, PF3 (runway), PF0+PF3 and
   PF1 while COLPF1=$2A (grey road) are safe; anything else crashes below
   $6D6, which is reloaded with (RANDOM&7)+15 whenever $604 is zero. */
/* GTIA P1PF: the shadow P1 (SHDW at F6) against the playfield. */
static uint8_t shadow_p1pf(const BmGame *g){
  return bm_world_playfield_mask_collision(&g->classic_world,
    bm_game_shadow_pixels(g),
    BM_MASK_WIDTH,BM_MASK_SHADOW_HEIGHT,BM_MASK_WIDTH,g->player.x,
    (int16_t)(g->player.y+g->player.altitude),8,5);
}
static uint8_t low_flight_crash(BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  uint8_t p1pf;
  if(!w->ckcol_enabled)return 0u;          /* CKCOL: LDA $C5, BNE L2 */
  p1pf=shadow_p1pf(g);
  if(p1pf==1u||p1pf==8u||p1pf==9u)return 0u;
  if(p1pf==2u&&w->colpf1==42u)return 0u;
  if(!w->vbi_counter)
    g->flight_low_limit=(uint8_t)(((g->rng>>16)&7u)+15u+(p1pf>=9u));
  return (uint8_t)(bm_flight_altitude(&g->flight_position)<
                   g->flight_low_limit);
}
/* EXXON.9 WIND (COMMON, once per coarse row): with chance $6C8/256 a gust
   of (RANDOM&31|7)+$6C9 rows pushes F4 one colour clock per row. */
static void wind_step(BmGame *g){
  uint8_t r;
  if(!g->classic_world.coarse_event)return;
  if(!g->wind_timer){
    r=(uint8_t)(g->rng>>24);
    if(r>=g->classic_world.wind_chance)return;
    g->wind_timer=(uint8_t)((((g->rng>>16)&31u)|7u)+
                            g->classic_world.wind_rows);
    g->wind_on=1u;
    /* ZR: carry clear stores A=RANDOM>>1 (usually non-zero, left);
       carry set stores 0 (right). */
    r=(uint8_t)(g->rng>>8);
    g->wind_left=(uint8_t)(!(r&1u)&&(r>>1));
    return;
  }
  if(!--g->wind_timer){g->wind_on=0u;return;}
  if(!g->wind_left){
    if(g->flight_position.plane_x!=184u)++g->flight_position.plane_x;
  }else if(g->flight_position.plane_x!=60u)--g->flight_position.plane_x;
}
#endif
/* MAX.1 AFP ($1AAA): the bomb explodes at (x,y); XPLSN takes over. */
static void bomb_lands(BmGame *g,int16_t x,int16_t y){
  g->player_bomb_explosion=25u;
  g->player_bomb_explosion_x=x;
  g->player_bomb_explosion_y=y;
  if(bomb_ship(g,x,y))return;
  if(bm_world_bomb_impact(&g->classic_world,x,y)){
    /* Z3: AAG -> S30, AAF -> S100. CKCHAR/W4: AAA -> S50;
       objective bonuses require mission state and are not added here. */
    uint8_t mode=g->classic_world.impact_mode;
    g->score+=mode==9u?100u:
      (mode==8u?60u:
      (mode==6u?(g->player.speed>=2?10u:0u):
      ((mode>=3u&&mode<=5u)||mode==7u?50u:30u)));
    /* W4/W7/W0: the target under the sight (U4/U2) scores one more S50;
       a bridge also does when its second hit takes $6F2 to zero (U5). */
    if(count_bomb_objective(g))g->score+=50u;
    if(mode==3u&&!--g->classic_world.bridge_score_state)g->score+=50u;
  }
  g->player_bomb_water_splash=g->classic_world.bomb_impact_water;
  g->player_bomb_target_hit=g->classic_world.bomb_impact_hit;
  /* Z3 .13 (roof zone, no roof): the drop sound runs on from $6EC (CDRP).
     Otherwise AFP silences AUDC2; water splashes ($1B6B), land explodes
     ($1B61: XXX $2A,$4C). */
  bm_sound_event_bomb_lands(&g->sound,sound_out(g),
    g->classic_world.bomb_zone==1u&&g->classic_world.bomb_impact_code!=96u?
      BM_BOMB_BEHIND_ROOFS:
    g->classic_world.bomb_impact_water?BM_BOMB_WATER:BM_BOMB_LAND);
  g->player_bomb_grass_impact=(uint8_t)(
    !g->player_bomb_target_hit&&!g->player_bomb_water_splash&&
    g->classic_world.bomb_impact_code==0u);
  if(g->classic_world.bomb_impact_handled)g->player_bomb_explosion=0u;
}
/* EXXON.2 CRASH.  lift is $654 (shadow rises that many lines); reason 3 is
   COLP02, the only caller that clears $655. */
static void begin_flight_crash(BmGame *g,uint8_t reason,uint8_t lift){
  uint8_t fast=(uint8_t)(g->flight_speed>=200u);   /* SC+$665>='2' */
  g->flight_crash_reason=reason;
  g->flight_crash_lift=lift;
  g->flight_crash_colp02=(uint8_t)(reason==3u);
  g->flight_crash_timer=0x50u;
  g->flight_crash_phase=1u;
  g->flight_crash_spread=0;
  /* ABA, then $6B9=22: XFLSH flashes the playfield for 22 updates. */
  g->classic_world.impact_flash_ticks=22u;
  /* XXX 80,$4F; later the propeller (AUDC1) and AUDC4 fall silent. */
  bm_sound_event_crash(&g->sound,sound_out(g));
  /* AFP: a falling bomb lands where it is; HG ends the shot. */
  if(g->player_bomb_active){
    g->player_bomb_active=0u;g->player_bomb_timer=255u;
    bomb_lands(g,g->player_bomb_x,g->player_bomb_y);
  }
  g->player_shot_active=0u;
  if(g->flight_crash_colp02){
    /* $D003/$D006=0 (P3, M2) and S100. */
    g->classic_world.enemy_shadow_on=0u;
    g->classic_world.enemy_fighter_shot_active=0u;
    g->score+=100u;
  }else if(fast){
    /* .20: at 200+ with $C5 set, AFP at $623=F4+3, $FA=F5. */
    bomb_lands(g,(int16_t)(g->player.x-2),(int16_t)(g->player.y-5));
  }
  g->flight_speed=0u;g->player.speed=0;
  g->mode=BM_STATE_CRASHING;
}
#if BM_ATARI_FLIGHT
static const uint8_t *player_pixels(const BmGame *g){
  return bm_mask_player(g->player.bank,(uint8_t)((g->tick&2u)!=0u));
}
static uint8_t shot_hits_player(const BmGame *g,int sx,int sy){
  const uint8_t *pixels=player_pixels(g);int x,y;
  for(y=0;y<2;++y)for(x=0;x<4;++x){
    int px=sx+x-g->player.x+8,py=sy+y-g->player.y+5;
    if(px>=0&&px<16&&py>=0&&py<10&&pixels[py*16+px])return 1u;
  }
  return 0u;
}
/* EXXON.7 DAMAGE/SV: 35 updates of immunity ($6F7).  With $659 already
   zero the engine stops ($63F=0, KMKZ).  Otherwise $659-1 and RANDOM&3 is
   drawn again until it hits a free letter: F, B, M or G. */
static void damage_player(BmGame *g){
  uint32_t r=g->rng;
  if(g->flight_damage_cooldown)return;
  g->flight_damage_cooldown=35u;
  g->status_flash=35u;                     /* SV: $CA=35 (COLRDM) */
  sound_explosion(g,BM_EXPLOSION_DAMAGE);  /* SV: XXX 15,$48 */
  if(!g->flight_service.damage_level){
    g->flight_fuel.active=0u;g->flight_fuel.empty=1u;
    return;
  }
  --g->flight_service.damage_level;
  for(;;){
    uint8_t bit;
    r=r*1664525u+1013904223u;
    bit=(uint8_t)(1u<<((r>>24)&3u));
    if(g->flight_service.damage_slots==15u)break;
    if(!(g->flight_service.damage_slots&bit)){
      g->flight_service.damage_slots|=bit;break;
    }
  }
}
/* COLM30 (EXXON.7): a gun missile only damages when F6+$6CB-gun_Y is below
   $6CC, i.e. the gun is near the player's depth. */
static uint8_t gun_depth_ok(const BmGame *g,uint8_t gun_y){
  const BmWorldBuffer *w=&g->classic_world;
  return (uint8_t)((uint8_t)(w->player_y_atari+w->gun_depth_bias-gun_y)<
                   w->gun_depth_range);
}
/* EXXON.10 CLG20 (missile 2: gun 2 and the ship): in areas 5..7 always a
   hit, otherwise COLM30's test against $69A, or $C1 of the object in $FD. */
static uint8_t gun2_depth_ok(const BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  uint8_t y=w->gun2_y;
  if(w->world_mode==BM_WORLD_CITY&&w->area_type>=5u&&w->area_type<8u)
    return 1u;
  if(w->object_slot==15u)y=w->ship_y;
  else if(w->object_slot==1u)y=w->river_car_y;
  else if(w->object_slot==2u||w->object_slot==3u)y=w->enemy_fighter_y;
  return gun_depth_ok(g,y);
}
/* COLM20: aircraft missiles need C2-C1 within F6-F5-$6CD .. +$6CE. */
static uint8_t air_depth_ok(const BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  uint8_t depth=(uint8_t)(g->player.altitude-w->air_depth_bias);
  return (uint8_t)((uint8_t)(w->enemy_fighter_shadow_y-w->enemy_fighter_y-
                             depth)<w->air_depth_range);
}
static void enemy_shots_hit_player(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  if(w->gun1_shot_y&&!w->gun1_shot_explosion&&gun_depth_ok(g,w->gun1_y)&&
     shot_hits_player(g,bm_pm_x(w->gun1_shot_x),bm_pm_y(w->gun1_shot_y))){
    w->gun1_shot_y=0u;w->gun1_shot_explosion=0u;damage_player(g);
  }
  if(w->gun2_shot_y&&!w->gun2_shot_explosion&&gun2_depth_ok(g)&&
     shot_hits_player(g,bm_pm_x(w->gun2_shot_x),bm_pm_y(w->gun2_shot_y))){
    w->gun2_shot_y=0u;w->gun2_shot_explosion=0u;damage_player(g);
  }
  if(w->enemy_fighter_shot_active&&air_depth_ok(g)&&
     shot_hits_player(g,bm_pm_x(w->enemy_fighter_shot_x),
                        bm_pm_y(w->enemy_fighter_shot_y))){
    w->enemy_fighter_shot_active=0u;damage_player(g);
  }
}
/* EXXON.3 COLP02: P2 overlaps the player's P0 and the aircraft's depth
   C2-C1 lies within F6-F5-6 .. +11. */
static uint8_t enemy_fighter_hits_player(const BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  const uint8_t *player,*fighter;
  int fighter_height,fx,fy,x,y;
  uint8_t p2_value,depth;
  if(!w->enemy_fighter_active||!w->enemy_body_on||w->enemy_fighter_destroyed)
    return 0u;
  depth=(uint8_t)(g->player.altitude-6);
  if((uint8_t)(w->enemy_fighter_shadow_y-w->enemy_fighter_y-depth)>=12u)
    return 0u;
  player=player_pixels(g);
  fighter=bm_mask_enemy_fighter(w->enemy_fighter_forward,w->enemy_fighter_turn,
                                &fighter_height,&p2_value);
  fx=bm_pm_x(w->enemy_fighter_x);
  fy=bm_pm_y(w->enemy_fighter_y);
  for(y=0;y<10;++y)for(x=0;x<16;++x){
    int ex=g->player.x-8+x-fx,ey=g->player.y-5+y-fy;
    if(player[y*16+x]&&ex>=0&&ex<16&&ey>=0&&ey<fighter_height&&
       enemy_p2_pixel(fighter,ey*16+ex,p2_value))return 1u;
  }
  return 0u;
}
/* EXXON.13 P0SHP (in SHIP): the player's shadow P1 overlaps the ship and
   F6-F5 < 20. */
static uint8_t ship_hits_player(const BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  int x,y,sx=bm_pm_x(w->ship_x),sy=bm_pm_y((int)w->ship_y-11);
  int shadow_x=g->player.x-8,shadow_y=g->player.y-5+g->player.altitude;
  if(!w->ship_active||w->ship_destroyed||g->player.altitude>=20)return 0u;
  for(y=0;y<BM_MASK_SHADOW_HEIGHT;++y)for(x=0;x<BM_MASK_WIDTH;++x){
    if(!bm_game_shadow_pixels(g)[y*16+x])continue;
    if(bm_mask_ship(shadow_x+x-sx,shadow_y+y-sy))return 1u;
  }
  return 0u;
}
#endif
static uint8_t player_runway_collision(const BmGame *g){
  const uint8_t *pixels=bm_mask_player(g->player.bank,(uint8_t)((g->tick&2u)!=0u));
  return bm_world_runway_mask_collision(&g->classic_world,pixels,BM_MASK_WIDTH,
                                         BM_MASK_PLANE_HEIGHT,BM_MASK_WIDTH,
                                         g->player.x,(int16_t)(g->player.y+2),8,5);
}
static uint8_t player_playfield_collision(const BmGame *g){
  const uint8_t *pixels=bm_mask_player(g->player.bank,(uint8_t)((g->tick&2u)!=0u));
  return bm_world_playfield_mask_collision(&g->classic_world,pixels,BM_MASK_WIDTH,
                                            BM_MASK_PLANE_HEIGHT,BM_MASK_WIDTH,
                                            g->player.x,g->player.y,8,5);
}
#if BM_ATARI_FLIGHT
/* EXXON.9 PRIOR (CITY loop).  $685 waits until area 5 and a shadow off
   PF0/PF3, then clears $C5 (no CKCOL in the capital).  $6EA=F6/2+F4 places
   the aircraft on the diagonal street: 150..212 in front of the buildings,
   213..242 behind them (623=4); elsewhere SHBNE hides the shadow and any
   P0PF below altitude 82 hits a building.  From area 8 the first P0PF
   PF0/PF3 contact restores 623=1 and $C5 ($686).  Returns 1 on a crash. */
static uint8_t prior_step(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  uint8_t depth;
  if(w->world_mode!=BM_WORLD_CITY)return 0u;
  if(w->prior_entry){
    if(w->area_type<5u||(shadow_p1pf(g)&9u))return 0u;
    w->prior_entry=0u;w->ckcol_enabled=0u;
  }
  if(w->prior_exit)return 0u;
  if(w->area_type>=8u&&(player_playfield_collision(g)&9u)){
    w->prior_exit=1u;w->gprior=1u;w->ckcol_enabled=1u;w->shadow_hidden=0u;
    return 0u;
  }
  depth=(uint8_t)((g->flight_position.shadow_y>>1)+g->flight_position.plane_x);
  w->prior_depth=depth;
  if(depth>=150u&&depth<243u){
    w->shadow_hidden=0u;w->gprior=depth<213u?1u:4u;
    return 0u;
  }
  w->shadow_hidden=1u;                     /* SHBNE */
  if(g->player.altitude<82&&player_playfield_collision(g))return 1u;
  w->gprior=1u;
  return 0u;
}
/* EXXON.14 COLCPTL (via CPTL/XCPTL in areas 6/7): P1PL, the shadow over
   the capital target, crashes below altitude 25. */
static uint8_t capital_collision(const BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  int r,c,x0=g->player.x-8,y0=g->player.y+g->player.altitude-5;
  if(w->world_mode!=BM_WORLD_CITY||(w->area_type!=6u&&w->area_type!=7u)||
     !w->capital_y||g->player.altitude>=25||w->shadow_hidden)return 0u;
  for(r=0;r<BM_MASK_SHADOW_HEIGHT;++r)for(c=0;c<BM_MASK_WIDTH;++c)
    if(bm_game_shadow_pixels(g)[r*16+c]&&bm_world_capital_covers(w,x0+c,y0+r))
      return 1u;
  return 0u;
}
#endif
/* EXXON.14 BMCPTL (BOMB, on impact, areas 6/7): M1PL with the capital
   target.  Its colour selects the $6C2 bit (50:1, 112:2, 96:4); S500, the
   XCPTLIM wreck, $6C3 suppresses the AFP explosion, XFLSH 16. */
static uint8_t capital_bomb(BmGame *g,int x,int y){
  BmWorldBuffer *w=&g->classic_world;
  uint8_t bit;int dx,dy,hit=0;
  if(w->world_mode!=BM_WORLD_CITY||(w->area_type!=6u&&w->area_type!=7u)||
     !w->capital_y)return 0u;
  for(dy=0;dy<3&&!hit;++dy)for(dx=0;dx<4&&!hit;++dx)
    hit=bm_world_capital_covers(w,x+dx,y+dy)!=0u;
  if(!hit)return 0u;
  bit=w->p2_colour==50u?1u:w->p2_colour==112u?2u:w->p2_colour==96u?4u:0u;
  if(!bit)return 0u;
  g->mission_phase|=bit;w->capital_destroyed=g->mission_phase;
  g->score+=500u;                          /* S500 */
  w->capital_wreck=1u;w->capital_flash=16u;
  w->impact_flash_ticks=16u;               /* $6B9=16 */
  w->quake=26;                             /* $6BE=26: QUAKE */
  sound_explosion(g,BM_EXPLOSION_CAPITAL); /* XXX 70,$4E */
  return 1u;
}
/* EXXON.14 LOW and EXXON.10 COLRDM: the status-line COLBK ($66E).  LOW
   only replaces 0/$12/$26: below altitude 24 (fuel left, 200+) it blinks
   $26 on RTCLOK bit 3, 24..28 gives $12.  COLRDM shows $30 while $CA
   counts down after SV. */
static void status_colour_step(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  int16_t alt=g->player.altitude;
  if(w->status_bk==0u||w->status_bk==38u||w->status_bk==18u){
    if(alt<24){
      w->status_bk=(uint8_t)(g->flight_fuel.active&&(g->play_frames&8u)&&
                             g->flight_speed>=200u?38u:0u);
      /* LOW .6: the warning tone on channel 2 ($6F8), or .10 silence. */
      if(w->status_bk)bm_sound_event_low_altitude(&g->sound,sound_out(g),BM_LOW_TONE);
      else if(g->flight_fuel.active)
        bm_sound_event_low_altitude(&g->sound,sound_out(g),BM_LOW_SILENT);
    }else{
      w->status_bk=alt<29?18u:0u;
      if(alt<29)                                     /* .3 */
        bm_sound_event_low_altitude(&g->sound,sound_out(g),BM_LOW_CLEAR);
    }
  }
  if(g->status_flash)w->status_bk=--g->status_flash?48u:0u;
}
/* EXXON.14 CKSC (COMMON): veterans ($6E7=2) keep EAS9.  Otherwise all
   objectives or a five-digit score select EAS9; else the thousands digit
   plus $6AC picks EAS2/EAS4 (novice only), EAS6, EAS8 or EAS9; zero keeps
   the current table. */
static void check_score(BmGame *g){
  uint8_t level=g->mission_rank_level,sum;
  if(level==2u)return;
  if(g->mission_targets_destroyed>=g->mission_objective_target||
     g->score>=10000u){
    bm_world_load_eas(&g->classic_world,BM_EAS9);return;
  }
  sum=(uint8_t)((g->score/1000u)%10u+g->mission_targets_destroyed);
  if(!sum)return;
  if(sum<3u){if(!level)bm_world_load_eas(&g->classic_world,BM_EAS2);}
  else if(sum<5u){if(!level)bm_world_load_eas(&g->classic_world,BM_EAS4);}
  else if(sum<7u)bm_world_load_eas(&g->classic_world,BM_EAS6);
  else if(sum<9u)bm_world_load_eas(&g->classic_world,BM_EAS8);
  else bm_world_load_eas(&g->classic_world,BM_EAS9);
}
/* MAX.1 BOMB ($1A6E): FA+1 per update until $61F (fixed at release), the
   narrow end alternates and $620 counts down; then BMCPTL or AFP. */
static void bomb_fall_step(BmGame *g){
  int16_t ground_y;
  if(!g->player_bomb_active)return;
  ground_y=g->player_bomb_target_y;
  if(g->player_bomb_y<ground_y){
    g->player_bomb_y++;
    g->player_bomb_phase^=1u;
    --g->player_bomb_timer;
    return;
  }
  g->player_bomb_timer=255u;
  g->player_bomb_active=0u;
  if(!capital_bomb(g,g->player_bomb_x,ground_y))
    bomb_lands(g,g->player_bomb_x,ground_y);
}
#if BM_ATARI_FLIGHT
/* M1PL bit 0: the falling bomb (missile 1) over the player's P0. */
static uint8_t bomb_hits_plane(const BmGame *g){
  const uint8_t *plane=player_pixels(g);
  int dx,row;
  for(row=0;row<3;++row)for(dx=0;dx<4;++dx){
    int x,y;
    if(row!=1&&dx<2)continue;               /* the narrow end */
    if(row==0&&g->player_bomb_phase)continue;
    if(row==2&&!g->player_bomb_phase)continue;
    x=g->player_bomb_x+dx-(g->player.x-8);
    y=g->player_bomb_y+row-(g->player.y-5);
    if(x>=0&&x<16&&y>=0&&y<10&&plane[y*16+x])return 1u;
  }
  return 0u;
}
/* PBMB's C0: a column whose A=(r>>1)+93 and A+35 stay out of the band
   C0..B0 around the player (B0=F5/2+F4/2+7, C0=B0-15). */
static uint8_t attack_column(BmGame *g){
  unsigned hi=(unsigned)(g->flight_position.plane_y>>1)+
              (g->flight_position.plane_x>>1)+(g->flight_position.plane_x&1u)+7u;
  unsigned lo=hi-15u,tries;
  uint8_t r=0u;
  for(tries=0;tries<256u;++tries){
    unsigned a;
    g->rng=g->rng*1664525u+1013904223u;r=(uint8_t)((g->rng>>24)&127u);
    a=(r>>1)+93u+(r&1u);
    if(a>=lo&&a<hi)continue;
    a+=35u;
    if(a>=lo&&a<hi)continue;
    break;
  }
  return r;
}
/* EXXON.15 BMBR on the ground: XPLSN/XFLSH run; a falling bomb that
   touches P0 crashes the aircraft (AFS), otherwise BOMB moves it.  An
   attacker in state 5 left of HPOS 170 drops one (1/32) unless the
   service is past the bombs ($6E0>=2); PC releases it from C0/C1/C2-8. */
static uint8_t airfield_bmbr(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  bm_world_crash_step(w);
  bm_sound_event_splash_step(&g->sound,sound_out(g));  /* BMBR: SPLASH */
  if(g->player_bomb_explosion)--g->player_bomb_explosion;
  if(g->player_bomb_active){
    if(bomb_hits_plane(g)){
      g->player_bomb_active=0u;begin_flight_crash(g,9u,0u);return 1u;
    }
    bomb_fall_step(g);
    return 0u;
  }
  if(w->object_slot==254u&&w->enemy_fighter_x<170u&&w->enemy_state==5u&&
     g->flight_service_phase<2u){
    int dx;
    g->rng=g->rng*1664525u+1013904223u;
    if((g->rng>>24)&31u)return 0u;
    dx=(int)w->enemy_fighter_x-(int)g->flight_position.plane_x;
    if(dx>=0&&dx<12&&
       (uint8_t)(w->enemy_fighter_shadow_y-g->flight_position.shadow_y-1u)>=20u)
      return 0u;
    g->player_bomb_active=1u;g->player_bomb_phase=0u;
    g->player_bomb_x=(int16_t)bm_pm_x(w->enemy_fighter_x+2);
    g->player_bomb_y=(int16_t)bm_pm_y(w->enemy_fighter_y+7);
    g->player_bomb_target_y=(int16_t)bm_pm_y(w->enemy_fighter_shadow_y-5);
    g->player_bomb_timer=15u;
  }
  return 0u;
}
/* EXXON.15 PBMB (ground loops): $FD=255 waits, 1 in 64 sends an attacker
   ($FD=254) that moves through L6 on odd RTCLOK frames; when it is gone
   DEC $FD returns to 255.  Returns 1 after a crash. */
static uint8_t airfield_attack(BmGame *g){
  BmWorldBuffer *w=&g->classic_world;
  if(w->object_slot==255u){
    if(airfield_bmbr(g))return 1u;
    g->rng=g->rng*1664525u+1013904223u;
    if(!((g->rng>>24)&63u))
      bm_world_airfield_attack_spawn(w,attack_column(g));
    return 0u;
  }
  if(w->object_slot!=254u)return 0u;
  /* PBMB .2: AUDC1=$C3 while the attacker flies; $641 again once L6 has
     cleared $FD. */
  bm_sound_event_attacker(&g->sound,sound_out(g),1u);
  if(g->play_frames&1u){
    bm_world_airfield_attack_step(w);
    if(!w->object_slot){
      w->object_slot=255u;
      bm_sound_event_attacker(&g->sound,sound_out(g),0u);
    }
  }
  return airfield_bmbr(g);
}
#endif
static void game_start(BmGame *g,uint32_t seed,uint8_t skill,uint8_t gravity,
                       uint8_t reversed){
  memset(g,0,sizeof(*g)); g->mode=BM_STATE_PLAYING; g->rng=seed?seed:1u;
  bm_pokey_reset(&g->pokey);bm_sound_reset(&g->sound);
  { unsigned i;for(i=0;i<5u;++i)bm_pokey_reset(&g->pokey_extra[i]); }
  /* EXXON.0: STOP.SN, AUDCTL 0, SKCTL 3; NNNN: AUDF1=251 and QUIET; then
     AFX (after the demo's tune, see demo_music). */
  bm_sound_event_power_on(&g->sound,sound_out(g));
  g->mission_rank_level=skill;g->mission_difficulty=gravity;
  g->option_reversed=reversed;g->flight_gravity_enabled=gravity;
  g->status_pf1=158u;                      /* CNSL/MENU START: $6F4=158 */
  g->player.x=160; g->player.y=105; g->player.speed=2;
  g->player.altitude=48; g->player.fuel=999; g->player.lives=3;
  g->flight_position=(BmFlightPosition){119u,165u,168u,4u,0};
  g->takeoff_speed=(BmTakeoffSpeed){144u,144u,144u};
  g->flight_fuel=(BmFuelState){300u,26u,1u,0u};
  g->flight_service=(BmGroundService){30u,34u,4u,90u,0u};
  /* EXXON.0: (RANDOM & 1) + 8 -> $6C7. */
  g->mission_objective_target=(uint8_t)(8u+((g->rng>>24)&1u));
  g->flight_gravity_countdown=14u;
  g->flight_low_limit=16u;            /* START: $6D6=16 */
  g->player_bomb_timer=255u;          /* $620 */
  g->flight_speed_countdown=4u;
#if BM_ATARI_FLIGHT
  g->mode=BM_STATE_TAKEOFF;project_flight(g);g->player.speed=0;
  g->flight_preroll=150u;
  g->player.fuel=(int16_t)g->flight_fuel.value;
#endif
  bm_world_buffer_init(&g->classic_world,seed^0x424d5744u);
  /* START: $6E7 0 keeps EAS0 ($6CF=$A5); 1 loads EAS6 with $6CF=180; 2
     loads EAS9, INC $6A7 and $6CF=200.  ($6E7 comes from the menu, T24.) */
#if BM_CAPITAL_TEST
  /* Test build: as after a sortie with all objectives ($6AC=$6C7), so the
     first takeoff takes V0/ZN into the city; 2 also sets $6C2=6. */
  g->mission_targets_destroyed=g->mission_objective_target;
#if BM_CAPITAL_TEST>=2
  g->mission_phase=6u;
#endif
#endif
  if(g->mission_rank_level==1u){
    bm_world_load_eas(&g->classic_world,BM_EAS6);
    g->classic_world.runway_interval=180u;
  }else if(g->mission_rank_level==2u){
    bm_world_load_eas(&g->classic_world,BM_EAS9);
    ++g->classic_world.enemy_tracking;
    g->classic_world.runway_interval=200u;
  }
}
void bm_game_init(BmGame *g,uint32_t seed){
  game_start(g,seed,0u,0u,0u);           /* power on: novice, normal */
}
void bm_game_boot(BmGame *g,uint32_t seed){
  bm_game_init(g,seed);
#if !BM_CAPITAL_TEST
  g->demo=1u;                              /* STX $6F3 (255) */
  demo_music(g);
#endif
}
/* START: MENU's choices live in RAM that START does not clear ($6E7 skill,
   $6E6 gravity, the $679/$680 swap). */
static void game_restart(BmGame *g){
  game_start(g,g->rng,g->mission_rank_level,g->mission_difficulty,
             g->option_reversed);
}
/* CRASH ends with $6F3=1 and START: the attract mode. */
static void game_demo(BmGame *g){
  game_restart(g);g->demo=1u;demo_music(g);
}
#if BM_ATARI_FLIGHT
static uint8_t player_playfield_collision(const BmGame *g);
#endif
/* Atari stick code (0 bits pressed: up, down, left, right) to BmInput. */
static void stick_to_input(uint8_t code,BmInput *in){
  uint8_t up=!(code&1u),down=!(code&2u),left=!(code&4u),right=!(code&8u);
  in->x=(int8_t)(left?-1:(right?1:0));
  in->y=(int8_t)(up&&!down?-1:(down&&!up?1:0));
}
/* EXXON.17 DEMO with DJOY, DBMB, DTKOF and DFLSH: the computer pilot
   replaces the stick ($D7) and the trigger ($D8). */
static void demo_input(BmGame *g,BmInput *in){
  uint8_t stick=15u,trigger=0u;
  g->status_pf1=(uint8_t)(144u+(g->play_frames&14u));     /* DFLSH */
  in->x=0;in->y=0;in->fire=0;in->bomb=0;
#if BM_ATARI_FLIGHT
  if(g->mode==BM_STATE_TAKEOFF){
    /* DTKOF: pull up once TKOSPD has carried into the hundreds ('1'). */
    if(g->takeoff_speed.hundreds==81u)stick=14u;
  }else if(g->mode==BM_STATE_PLAYING){
    uint8_t alt=(uint8_t)g->player.altitude,r;
    if(!g->flight_fuel.active){g->demo_stick=15u;stick=15u;}  /* .10 */
    else if(alt<25u){
      /* JSR UP: one climb step now; $D7 stays free for DBMB. */
      uint8_t before=g->flight_position.plane_y;
      (void)bm_flight_joystick_step(&g->flight_position,14u);
      if(g->flight_position.plane_y<before)
        g->alt_display=(uint16_t)(g->alt_display+(before-g->flight_position.plane_y));
    }
    else if(g->demo_stick==15u){
      /* .3: one in 64 starts a manoeuvre of 10..41 frames with a random
         code 5..14. */
      g->rng=g->rng*1664525u+1013904223u;r=(uint8_t)(g->rng>>24);
      if(!(r&63u)){
        g->demo_timer=(uint8_t)((g->play_frames&31u)+10u);
        do{g->rng=g->rng*1664525u+1013904223u;r=(uint8_t)((g->rng>>24)&15u);}
        while(r<5u||r==15u);
        stick=r;
      }
      g->demo_stick=stick;
    }else if((g->demo_stick==13u&&alt<27u)||!--g->demo_timer){
      g->demo_stick=15u;                                     /* .10 */
    }else stick=g->demo_stick;                               /* .6 */
    if(g->flight_speed>=200u){
      /* DBMB: release while a bomb falls; over PF2/PF3 dive and press
         (a bomb); otherwise fire while $66E is set. */
      if(!g->player_bomb_active){
        uint8_t p0pf=player_playfield_collision(g);
        if(p0pf==4u||p0pf==8u){stick=13u;trigger=1u;}
        else trigger=(uint8_t)(g->classic_world.status_bk!=0u);
      }
    }
  }
#endif
  stick_to_input(stick,in);
  in->fire=trigger;
}
/* CONSOL (53279): bit 0 START, bit 1 SELECT, bit 2 OPTION, active low. */
static uint8_t console_value(const BmInput *in){
  return (uint8_t)(7u&~((in->start?1u:0u)|(in->select?2u:0u)|(in->option?4u:0u)));
}
/* EXXON.16 MENU: OPTION moves the cursor, SELECT changes that line, START
   starts the game; every action needs a change of CONSOL ($6E9). */
static void options_menu(BmGame *g,const BmInput *in){
  uint8_t consol=console_value(in);
  if(consol==g->console_previous)return;
  g->console_previous=consol;
  if(consol==6u){game_restart(g);return;}                  /* START */
  if(consol==3u){g->menu_cursor=(uint8_t)((g->menu_cursor+1u)%3u);return;}
  if(consol!=5u)return;                                     /* SELECT */
  if(g->menu_cursor==0u)
    g->mission_rank_level=(uint8_t)((g->mission_rank_level+1u)%3u);
  else if(g->menu_cursor==1u)g->option_reversed^=1u;
  else g->mission_difficulty^=1u;
}
static void game_tick(BmGame *g,const BmInput *in){
  BmInput single=*in;
  /* REVERSE CTRL swaps $679 (dive) and $680 (climb). */
  if(g->option_reversed)single.y=(int8_t)-single.y;
#if BM_ATARI_FLIGHT
  /* One trigger as on the Atari (STRIG0).  The second button is a
     convenience that means exactly "trigger + dive" ($679). */
  if(single.bomb){single.fire=1u;single.x=0;single.y=1;single.bomb=0u;}
#endif
  in=&single;
  if(g->mode==BM_STATE_OPTIONS){options_menu(g,in);return;}
  /* EXXON.16 CNSL (COMMON, TAKOF, CRASH and RNK waits): START restarts,
     OPTION alone opens the menu ($6E9=3, cursor on the skill line). */
  { uint8_t consol=console_value(in);
    if(consol==6u&&g->mode!=BM_STATE_PAUSED){game_restart(g);return;}
    if(consol==3u&&g->mode!=BM_STATE_PAUSED){
      g->mode=BM_STATE_OPTIONS;g->menu_cursor=0u;g->console_previous=3u;
      bm_sound_event_quiet(sound_out(g));  /* CNSL: QUIET */
      return;
    }
  }
  if(g->demo)demo_input(g,&single);
  uint8_t edge=(uint8_t)(in->pause&&!g->previous_pause); g->previous_pause=in->pause;
  g->fire_active=in->fire;
  g->bomb_active=in->bomb;
  if(edge){
    if(g->mode==BM_STATE_PAUSED)g->mode=g->resume_mode;
    else if(g->mode==BM_STATE_PLAYING||g->mode==BM_STATE_TAKEOFF||
            g->mode==BM_STATE_GROUND){
      g->resume_mode=g->mode;g->mode=BM_STATE_PAUSED;
    }
  }
  if(g->mode!=BM_STATE_PAUSED)++g->play_frames;    /* RTCLOK */
  /* T22: one RTCLOK for game and world (PLBCK's $6A8, XC's $638). */
  g->classic_world.frame_counter=(uint8_t)(g->play_frames-1u);
  if(g->mode==BM_STATE_GAME_OVER&&!g->flight_crash_reason){
    /* STOPL with $6C2=7: 150 updates PROP/TURN/TIMER, the propeller tone
       falls from $C8 to $C0 every 15 updates, PLAY.IT, then RNK and CNSL
       until START or OPTION (no audio yet). */
    g->tick++;
#if BM_ATARI_FLIGHT
    if(g->end_timer&&flight_turn(g,flight_joystick(in))){
      begin_flight_crash(g,4u,0u);return;
    }
#endif
    if(g->end_timer){
      /* STOPL .2: after 150 updates the propeller drops one step every 15
         ($C8 -> $C0). */
      unsigned elapsed=150u+8u*15u-g->end_timer;
      --g->end_timer;
      if(elapsed>=150u&&!((elapsed-150u)%15u)){
        if(bm_sound_event_engine_stop_step(&g->sound,sound_out(g))){
          g->end_timer=0u;bm_sound_event_end_music(&g->sound);   /* .35: PLAY.IT, RNK */
        }
      }
    }
    return;                                /* RNK, then CNSL until START */
  }
  if(g->mode==BM_STATE_TAKEOFF&&g->music_wait)return;   /* AFY */
  if(g->mode==BM_STATE_TAKEOFF){
    BmTakeoffResult result;
    uint8_t p0pf;
    g->tick++;g->rng=g->rng*1664525u+1013904223u;
#if BM_ATARI_FLIGHT
    if(g->flight_preroll){
      /* CW: PROP, TURN, CNSL, FUEL, DFLSH, TIMER, ROAR for 150 frames; the
         scroll VBI is only installed at MZ, so the world stands still. */
      --g->flight_preroll;
      if(flight_turn(g,flight_joystick(in))){begin_flight_crash(g,4u,0u);return;}
      bm_flight_fuel_step(&g->flight_fuel,flight_joystick(in),
                          (uint8_t)(g->flight_service.damage_slots&BM_DAMAGE_F));
      g->player.fuel=(int16_t)g->flight_fuel.value;
      bm_sound_event_engine(&g->sound,sound_out(g),flight_joystick(in));
      return;
    }
#endif
    /* TAKOF: TKOSPD while SC+$667 still shows the takeoff font (>=100);
       after the switch to normal digits (100) the loop calls SPD, so the
       speed keeps rising towards 200 while the aircraft stays down. */
    if(g->takeoff_speed.ones>=100u){
      bm_flight_takeoff_speed_step(&g->takeoff_speed);
      if(g->takeoff_speed.ones<100u)g->flight_speed=100u;
    }else{
      bm_flight_speed_step(&g->flight_speed,&g->flight_speed_countdown,
                           g->flight_fuel.active,flight_joystick(in));
      g->takeoff_speed.hundreds=(uint8_t)(80u+g->flight_speed/100u);
    }
    bm_flight_fuel_step(&g->flight_fuel,flight_joystick(in),
                        (uint8_t)(g->flight_service.damage_slots&BM_DAMAGE_F));
    g->player.fuel=(int16_t)g->flight_fuel.value;
    /* TAKOF/DI reads P0PF ($D004) and crashes only on bit 0 (COLPF0). */
    p0pf=player_playfield_collision(g);
    result=bm_flight_takeoff_control(&g->takeoff_speed,flight_joystick(in),
                                     (uint8_t)(p0pf&1u));
    if(result==BM_TAKEOFF_AIRBORNE){
      g->mode=BM_STATE_PLAYING;
      g->player.speed=(int16_t)(g->flight_speed/100u);
      bm_world_after_takeoff(&g->classic_world,g->mission_targets_destroyed,
                             g->mission_objective_target);
#if BM_CAPITAL_TEST
      /* Test build: shorten ZN's first area (255 rows), but keep DROAD
         ($668 delay, then $669..$66A) finished before WP in area 3: every
         road row stores 709=$2A, which would survive into the capital. */
      if(g->classic_world.world_mode==BM_WORLD_CITY&&
         g->classic_world.area_type==1u){
        const BmWorldBuffer *w=&g->classic_world;
        unsigned road=(unsigned)w->road_delay+w->road_target-w->road_position+2u;
        g->classic_world.area_delay=(uint8_t)(road>61u+20u?road-61u:20u);
      }
#endif
    }
    else if(result==BM_TAKEOFF_CRASH)begin_flight_crash(g,1u,0u);
    bm_sound_event_engine(&g->sound,sound_out(g),flight_joystick(in));   /* TAKOF */
    /* TAKOF calls PBMB as well. */
    if(g->classic_world.object_slot==254u&&(g->play_frames&1u))
      bm_world_airfield_attack_step(&g->classic_world);
    bm_world_buffer_step(&g->classic_world);
    project_flight(g);
    return;
  }
  if(g->mode==BM_STATE_GROUND){
    uint8_t scroll_ground=0u;
#if BM_ATARI_FLIGHT
    /* GROUND and STOPL/MJ call TURN: banking on the ground crashes. */
    if(flight_turn(g,flight_joystick(in))){
      ++g->tick;
      begin_flight_crash(g,4u,0u);
      return;
    }
#endif
    g->tick++;g->rng=g->rng*1664525u+1013904223u;
#if BM_ATARI_FLIGHT
    /* GROUND, REFUEL, INCBMB and N4 call PBMB; MJ does not. */
    if(!g->flight_ground_rollout&&airfield_attack(g))return;
#endif
    if(g->flight_service_tail){
      /* N4: PROP, TURN, PBMB, ROAR for 14 updates, then MZ.  ($6B0=0 on
         RTCLOK&3==0 only feeds a TAKOF branch that never runs: it needs
         SC+$665<81 while TKOSPD keeps the takeoff glyph 144 there.) */
      bm_sound_event_engine(&g->sound,sound_out(g),flight_joystick(in));
      if(!--g->flight_service_tail){
        g->mode=BM_STATE_TAKEOFF;
        g->takeoff_speed=(BmTakeoffSpeed){144u,144u,144u};
        g->flight_speed=0u;g->player.speed=0;
        g->flight_fuel.active=1u;g->flight_fuel.empty=0u;
      }
      return;
    }
    if(!g->flight_ground_rollout){
      /* TRIGC (MU, INCBMB, SA): each new trigger press advances $6E0, so
         presses skip the rest of fuelling, bombs and repairs in turn. */
      uint8_t trigger=(uint8_t)(in->fire||in->bomb);
      if(trigger&&!g->flight_service_trigger&&g->flight_service_phase<3u){
        ++g->flight_service_phase;
        g->flight_service.bomb_countdown=0u;
        g->flight_service.repair_countdown=0u;
      }
      g->flight_service_trigger=trigger;
    }
    if(g->flight_ground_rollout){
      /* STOPL enters MM directly: one decimal speed unit per loop, without
         the $635 two-step limiter used by SPDL. */
      if(g->flight_speed)--g->flight_speed;
      if(g->flight_speed)scroll_ground=1u;
      if(!g->flight_speed){
        g->flight_ground_rollout=0u;
        g->flight_fuel.countdown=150u;
        if(g->mission_phase==7u){
          /* STOPL: the capital is destroyed - end of the game. */
          g->mode=BM_STATE_GAME_OVER;
          g->end_timer=150u+8u*15u;
          g->classic_world.landing=0u;
          return;
        }
      }
    }else if(!g->flight_service_phase){
      BmRefuelResult refuel=bm_flight_refuel_step(&g->flight_fuel,
                                                  &g->flight_service_phase);
      if(refuel==BM_REFUEL_FUEL_FULL)
        g->flight_service.bomb_countdown=34u;
      g->player.fuel=(int16_t)g->flight_fuel.value;
    }else{
      uint8_t phase=g->flight_service_phase;
      if(bm_flight_ground_service_step(&g->flight_service,
                                       &g->flight_service_phase))
        g->flight_service_tail=14u;
      /* SA: the last 19 of $F8's 90 updates beep ($15/$A2); FIX and a
         skip (ADO) silence channel 4. */
      if(g->flight_service_phase==2u){
        uint8_t c=g->flight_service.repair_countdown;
        if(c&&c<20u)bm_sound_event_repair_beep(sound_out(g),1u);
        else if(c==90u)bm_sound_event_repair_beep(sound_out(g),0u);
      }else if(phase==2u)bm_sound_event_repair_beep(sound_out(g),0u);
    }
    /* GROUND: the propeller falls one step per loop down to $C1. */
    if(!g->flight_ground_rollout)bm_sound_event_engine_idle(&g->sound,sound_out(g));
    g->player.speed=(int16_t)(g->flight_speed/100u);
    /* MJ still calls DRAW/DRUN while STOPL rolls. GROUND itself contains
       PROP/TURN/REFUEL/TIMER but no world draw, so V000 freezes terrain. */
    if(scroll_ground){
      g->classic_world.landing=BM_WORLD_ROLLOUT;
      bm_world_buffer_step(&g->classic_world);
    }
    g->classic_world.landing=0u;
    g->flight_runway_visible=bm_world_runway_visible(&g->classic_world);
    g->flight_runway_collision=player_runway_collision(g);
    return;
  }
  if(g->mode==BM_STATE_CRASHING){
    g->tick++;
    if(g->flight_crash_phase==1u){
      /* CRSH uses bit 2 of the descending $F7=$50 timer. P0 and P1 move one
         Atari colour clock in opposite directions on every update.  The
         loop also runs XFLSH, XPLSN, SPLASH and XSHIP; the VBI is XSND, so
         nothing scrolls. */
      if(g->flight_crash_timer&4u)g->flight_crash_spread-=2;
      else g->flight_crash_spread+=2;
      if(g->player_bomb_explosion)g->player_bomb_explosion--;
      bm_world_crash_step(&g->classic_world);
      bm_sound_event_splash_step(&g->sound,sound_out(g));   /* AFQ: SPLASH */
      if(!--g->flight_crash_timer){
        /* 704/705=0 hide both players; SK hides the enemy P2 after COLP02
           and a ship ($FD=15) loses its colour too.  Then $B0=$40. */
        g->flight_crash_phase=2u;
        g->flight_crash_timer=0x40u;
        if(g->flight_crash_colp02)g->classic_world.enemy_body_on=0u;
        g->classic_world.status_bk=0u;     /* STA $66E */
        if(g->classic_world.object_slot==15u)g->classic_world.ship_active=0u;
      }
    }else if(g->flight_crash_phase==2u){
      if(!--g->flight_crash_timer){
        /* RNKX, then RTCLOK 19 and 20 are set to 251; CNSL polls until
           location 19 wraps to zero, 5+4*256 frames later. */
        g->flight_crash_phase=3u;
        g->flight_crash_wait=1029u;
        g->mode=BM_STATE_RANKING;
      }
    }
    return;
  }
  if(g->mode==BM_STATE_RANKING){
    g->tick++;
    /* CNSL (top of bm_game_tick) handles START and OPTION; after the
       wait START runs with $6F3=1: the attract mode. */
    if(!--g->flight_crash_wait){
      game_demo(g);
    }
    return;
  }
  if(g->mode!=BM_STATE_PLAYING) return;
  g->tick++; g->rng=g->rng*1664525u+1013904223u;
  if(g->flight_damage_cooldown)--g->flight_damage_cooldown;
#if !BM_ATARI_FLIGHT
  g->player.bank=in->x<0?-1:(in->x>0?1:0);
#endif
#if BM_ATARI_FLIGHT
  {
    uint8_t joystick=flight_joystick(in);
    uint8_t crash=0u,turn_crash;
    if(g->flight_fuel.empty)
      crash=bm_flight_deadstick_step(&g->flight_position,
                                     &g->flight_fuel.countdown,joystick,13u);
    else if(!(g->flight_service.damage_slots&BM_DAMAGE_M)||
            !((--g->flight_control_counter)&1u)){
      /* JOYST: with M damage only every other $65A update steers.  Each
         F5 step also moves the ALT digits (SC+$66F, carries in ALT). */
      uint8_t before=g->flight_position.plane_y;
      crash=bm_flight_joystick_step(&g->flight_position,joystick);
      if(g->flight_position.plane_y<before)
        g->alt_display=(uint16_t)(g->alt_display+(before-g->flight_position.plane_y));
      else if(g->flight_position.plane_y>before){
        uint16_t down=(uint16_t)(g->flight_position.plane_y-before);
        g->alt_display=(uint16_t)(g->alt_display>down?(unsigned)(g->alt_display-down):0u);
      }
      if(g->alt_display>999u)g->alt_display=999u;
    }
    turn_crash=crash?0u:flight_turn(g,joystick);   /* COMMON: TURN */
    if(!crash&&!turn_crash)crash=bm_flight_gravity_step(&g->flight_position,
      &g->flight_gravity_countdown,g->flight_gravity_enabled,
      joystick,14u,13u);
    project_flight(g);
    if(crash||turn_crash){
      /* This frame's VBI scroll has happened when JOYST/TURN jump to CRASH. */
      bm_world_buffer_step(&g->classic_world);
      begin_flight_crash(g,turn_crash?4u:1u,0u);
      return;
    }
  }
#else
  g->player.x=clamp16((int16_t)(g->player.x+in->x*2),16,303);
  /* Preview only: keep the plane inside JOYST's F5 range 60..165 with the
     shadow fixed on F6=168. */
  g->player.y=clamp16((int16_t)(g->player.y+in->y),
                      (int16_t)(bm_pm_y(60)+5),(int16_t)(bm_pm_y(165)+5));
  g->player.altitude=(int16_t)(bm_pm_y(168)+5-g->player.y);
  g->player.speed=clamp16((int16_t)(g->player.speed-in->y),1,6);
#endif
#if BM_ATARI_FLIGHT
  g->classic_world.player_x_atari=g->flight_position.plane_x;
  /* Enemy TRIGB/TRIGF compares its P3 depth C2 with F6, the actual shadow
     scanline.  player.y is projected from F5 and is not interchangeable once
     the aircraft climbs above its initial three-line separation. */
  g->classic_world.player_y_atari=g->flight_position.shadow_y;
  g->classic_world.player_plane_y=g->flight_position.plane_y;
#else
  g->classic_world.player_x_atari=(uint8_t)clamp16(
    (int16_t)bm_pm_hpos(g->player.x-8),60,184);
  g->classic_world.player_y_atari=(uint8_t)clamp16(
    (int16_t)bm_pm_scanline(g->player.y-5+g->player.altitude),60,220);
  g->classic_world.player_plane_y=(uint8_t)clamp16(
    (int16_t)bm_pm_scanline(g->player.y-5),60,220);
#endif
  if(g->player_shot_active){
    /* MAX.1 TRIG: the missile ends at Y<38 or HPOS>=203 (tested before
       the step), otherwise it moves +8 colour clocks and -8 lines. */
    if(g->player_shot_y<bm_pm_y(38)||g->player_shot_x>=bm_pm_x(203))
      g->player_shot_active=0u;
    else{
      hit_enemy_fighter(g);
      hit_ship(g);
      if(g->player_shot_active)
        hit_river_vehicle(g,g->player_shot_x,g->player_shot_y);
      if(g->player_shot_active){
        g->player_shot_x=(int16_t)(g->player_shot_x+16);
        g->player_shot_y=(int16_t)(g->player_shot_y-8);
        /* SHBT after the step, only while F6-F5<28. */
        if(g->player.altitude<28){
          uint8_t points=bm_world_shot_ground(&g->classic_world,
            (uint8_t)bm_pm_hpos(g->player_shot_x),
            (uint8_t)bm_pm_scanline(g->player_shot_y));
          if(points){g->score+=points;g->player_shot_active=0u;}  /* HG */
        }
      }
    }
  }
  /* TRIG: trigger, G damage only on RANDOM&31==0; with one trigger it is
     ignored while the stick is down ($679) and while $620 counts a fresh
     bomb (the first 16 updates of its fall). */
  if(in->fire&&!g->player_shot_active&&
#if BM_ATARI_FLIGHT
     flight_joystick(in)!=13u&&(g->player_bomb_timer&0x80u)&&
#endif
     (!(g->flight_service.damage_slots&BM_DAMAGE_G)||!((g->rng>>19)&31u))){
    /* TRIG starts at player HPOS F4+6 and vertical position F5+2.  In the
       centred chunky sprite this is the upper-right nose at p+4,q-3. */
    g->player_shot_active=1u;
    g->player_shot_x=(int16_t)(g->player.x+4);
    g->player_shot_y=(int16_t)(g->player.y-3);
  }
  if(g->player_bomb_explosion){
    g->player_bomb_explosion--;
  }
  bomb_fall_step(g);
  /* BOMB: B damage releases only on RANDOM&31==0.  With one trigger the
     bomb needs trigger + stick down ($679); no bomb while a shot flies. */
  if(
#if BM_ATARI_FLIGHT
     in->fire&&flight_joystick(in)==13u&&
#else
     in->bomb&&
#endif
     !g->player_bomb_active&&!g->player_shot_active&&
     (!(g->flight_service.damage_slots&BM_DAMAGE_B)||!((g->rng>>11)&31u))
#if BM_ATARI_FLIGHT
     &&g->flight_service.bombs
#endif
    ){
    /* BOMB starts missile 1 at F4+2/F5+7.  Relative to the translated TRIG
       origin (F4+6/F5+2), this is eight pixels left and five pixels down. */
    bm_sound_event_bomb_release(&g->sound,sound_out(g));
    g->player_bomb_active=1u;
    g->player_bomb_phase=0u;
    g->player_bomb_x=(int16_t)(g->player.x-4);
    g->player_bomb_y=(int16_t)(g->player.y+2);
    /* $61F=F6+3: plane top (y-5) plus F6-F5 plus three scanlines. */
    g->player_bomb_target_y=(int16_t)(g->player.y-5+g->player.altitude+3);
    /* BOMB clears $6EB, then AFB: with $C5=0 (capital) a bomb between the
       buildings lands 80 lines higher, on the roofs ($6EB=1); behind them
       $6EB=$6EA. */
    g->classic_world.bomb_zone=0u;
    if(!g->classic_world.ckcol_enabled){
      uint8_t d=g->classic_world.prior_depth;
      if(d>=243u||d<150u){
        g->classic_world.bomb_zone=1u;
        g->player_bomb_target_y=(int16_t)(g->player_bomb_target_y-80);
      }else if(d>=213u)g->classic_world.bomb_zone=d;
    }
    g->player_bomb_timer=15u;
#if BM_ATARI_FLIGHT
    --g->flight_service.bombs;
#endif
  }
#if BM_ATARI_FLIGHT
  bm_flight_fuel_step(&g->flight_fuel,flight_joystick(in),
                        (uint8_t)(g->flight_service.damage_slots&BM_DAMAGE_F));
  g->player.fuel=(int16_t)g->flight_fuel.value;
  if(g->flight_landing.active)
    bm_flight_landing_speed_step(&g->flight_speed,
                                 &g->flight_speed_countdown);
  else
    bm_flight_speed_step(&g->flight_speed,&g->flight_speed_countdown,
                         g->flight_fuel.active,
                         flight_joystick(in));
  g->player.speed=(int16_t)(g->flight_speed/100u);
#endif
  g->world_scroll=(uint16_t)(g->world_scroll+g->player.speed);
  /* PBMB (COMMON): an airfield attacker keeps flying through L6 on odd
     RTCLOK frames; in the air $FD is not reset to 255. */
  if(g->classic_world.object_slot==254u&&(g->play_frames&1u))
    bm_world_airfield_attack_step(&g->classic_world);
  /* COMMON: ROAR (KMKZ once the engine is dead), GUNSND, GGNSND. */
#if BM_ATARI_FLIGHT
  if(g->flight_fuel.active)bm_sound_event_engine(&g->sound,sound_out(g),flight_joystick(in));
  else bm_sound_event_engine_dead(&g->sound,sound_out(g));
#endif
  /* SRT/LAND: RUNSND and CDRP as well. */
  bm_sound_event_update(&g->sound,sound_out(g));
  check_score(g);                          /* COMMON: CKSC */
  g->classic_world.player_fast=(uint8_t)(g->flight_speed>=200u);
  g->classic_world.capital_destroyed=g->mission_phase;   /* $6C2 */
  /* TRGT (in the world update) compares $6AC with $6C7. */
  g->classic_world.objectives_destroyed=g->mission_targets_destroyed;
  g->classic_world.objective_target=g->mission_objective_target;
  bm_world_buffer_step(&g->classic_world);
  status_colour_step(g);                   /* COMMON: LOW, COLRDM */
  /* SHCOL: only at exactly 80 / 79 and only while P1 holds a shadow. */
  if(!g->classic_world.shadow_hidden){
    if(g->player.altitude==80)g->shadow_far=1u;
    else if(g->player.altitude==79)g->shadow_far=0u;
  }
  if(g->classic_world.target_sight_active&&
     g->classic_world.target_sight_type){
    g->mission_objective_type=g->classic_world.target_sight_type;
    g->mission_objective_sighted=1u;
  }else if(g->mission_objective_sighted){
    /* Z9 reaches the lower border and falls through U2, which clears $6AD
       together with the P2 sight.  Keep manually assigned test objectives
       independent by only retiring types that originated from GNSGHT. */
    g->mission_objective_type=0u;
    g->mission_objective_sighted=0u;
  }
#if BM_ATARI_FLIGHT
  enemy_shots_hit_player(g);
  if(!g->flight_landing.active&&enemy_fighter_hits_player(g)){
    /* COLP02: $655=0, $654=F6-F5-6, then CRASH. */
    begin_flight_crash(g,3u,(uint8_t)(g->player.altitude-6));
    return;
  }
  if(!g->flight_landing.active&&ship_hits_player(g)){
    /* P0SHP: Z6 destroys the ship, then CRASH. */
    g->classic_world.ship_destroyed=1u;
    g->classic_world.ship_destroy_timer=120u;
    score_s7(g);
    begin_flight_crash(g,6u,(uint8_t)g->player.altitude); /* $654=F6-F5 */
    return;
  }
  wind_step(g);
  project_flight(g);
  if(prior_step(g)){
    /* PRIOR/ZK: a building; the shadow is already cleared (SHBNE). */
    begin_flight_crash(g,7u,0u);
    return;
  }
  if(low_flight_crash(g)){
    begin_flight_crash(g,5u,(uint8_t)g->player.altitude); /* CKCOL, $654=F6-F5 */
    return;
  }
  if(capital_collision(g)){
    begin_flight_crash(g,8u,(uint8_t)g->player.altitude); /* COLCPTL */
    return;
  }
#endif
  g->flight_runway_visible=bm_world_runway_visible(&g->classic_world);
  g->flight_runway_collision=player_runway_collision(g);
#if BM_ATARI_FLIGHT

  if(g->flight_landing.active){
    /* LAND/MK after COMMON, SPDL and CKCOL: at F6-F5=3 the shadow must lie
       wholly on the PF3 runway ($D005=8). */
    BmLandingResult landing=bm_flight_landing_step(&g->flight_landing,
      g->flight_speed,(uint8_t)g->player.altitude,
      g->classic_world.runway_marker,shadow_p1pf(g));
    if(landing==BM_LANDING_GROUND){
      g->mode=BM_STATE_GROUND;
      g->wind_timer=0u;g->wind_on=0u;   /* MK: $BE=0, SC+$673=0 */
      /* MK: SC+$687=$6B6=0 (no R/L) and $66E=0. */
      g->classic_world.runway_marker=0u;g->classic_world.marker_blank=0u;
      g->classic_world.status_bk=0u;
      g->flight_ground_rollout=1u;
      g->flight_service_phase=0u;
    }
    else if(landing==BM_LANDING_CRASH){
      begin_flight_crash(g,2u,0u);
    }
  }else if(g->classic_world.world_mode==BM_WORLD_SRT){
    /* CKLAND (SRT only, after COMMON): $609=0 and the trigger. */
    bm_flight_landing_begin(&g->flight_landing,
                            g->classic_world.runway_slot,
                            (uint8_t)(in->fire||in->bomb));
  }
  g->classic_world.landing=g->flight_landing.active?BM_WORLD_LAND:0u;
#endif
  river_vehicle_terrain_collision(g);
#if !BM_ATARI_FLIGHT
  if(g->tick%BM_TICK_HZ==0&&g->player.fuel>0) g->player.fuel--;
#endif
}
/* Per update: the world's sound requests, both shots' $648 (RN/RO, TRIG/HG)
   and the VBI's XSND (it runs in every loop, PAUSE included). */
void bm_game_tick(BmGame *g,const BmInput *in){
  BmWorldBuffer *w=&g->classic_world;
  const BmSoundOut *o;
  uint8_t shot=w->enemy_fighter_shot_active,own=g->player_shot_active;
  game_tick(g,in);
  if(g->mode==BM_STATE_OPTIONS)return;
  o=sound_out(g);
  if(w->sound_gun_fired)bm_sound_event_ground_gun(&g->sound);
  if(w->sound_flak)bm_sound_event_flak(&g->sound,o);
  if(w->sound_runway)bm_sound_event_runway(&g->sound);
  if(w->sound_warn)bm_sound_event_warning(&g->sound,o,(uint8_t)(w->sound_warn==1u));
  if(w->sound_flash)bm_sound_event_marker(o,(uint8_t)(w->sound_flash==2u));
  if(w->sound_line)
    bm_sound_event_line(&g->sound,o,(uint8_t)(w->sound_line==1u),w->sound_line_low);
  w->sound_gun_fired=w->sound_flak=w->sound_runway=0u;
  w->sound_warn=w->sound_flash=w->sound_line=0u;
  if(!shot&&w->enemy_fighter_shot_active)bm_sound_event_enemy_shot(&g->sound); /* RN */
  else if(shot&&!w->enemy_fighter_shot_active)bm_sound_event_shot_end(&g->sound); /* RO */
  /* MAX.1 TRIG: $648=3 when the own shot starts ($19B3), 255 when it ends
     (HG, $19EA); GUNSND crackles in between. */
  if(!own&&g->player_shot_active)bm_sound_event_own_shot(&g->sound);
  else if(own&&!g->player_shot_active)bm_sound_event_shot_end(&g->sound);
  /* The VBI: XSND, and timer 1 runs PLAY.IT; the demo's tune ends in AFX. */
  if(bm_sound_event_vblank(&g->sound,o)&&g->music_wait){
    g->music_wait=0u;bm_sound_event_takeoff_engine(&g->sound,o);
  }
}
