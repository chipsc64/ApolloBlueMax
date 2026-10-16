#include "bm_game.h"
#include "bm_sprite.h"
#include "bm_tile.h"
#include "bm_world.h"
#include "bm_geometry.h"
#include "bm_chbas_assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef BM_CLASSIC_TEST_PAGE
#define BM_CLASSIC_TEST_PAGE 0
#endif
#ifndef BM_ATARI_SCROLL
#define BM_ATARI_SCROLL 0
#endif
#if BM_CLASSIC_TEST_PAGE
#define EXPECTED_PLANE_COLOUR 26
#else
#define EXPECTED_PLANE_COLOUR 7
#endif

static void to_city(BmWorldBuffer *world){
  /* Tests of AREA/DROAD/BLDG/ENRUN run in the CITY loop.  Mirror the state
     ZN leaves behind (area 1, $B8=255, road at 1 heading for its target,
     $66C=36) with fixed values; tests override what they exercise. */
  world->world_mode=BM_WORLD_CITY;
  world->area_type=1u;world->area_delay=255u;world->area_transition=0u;
  world->road_position=1u;world->road_target=40u;
  world->road_delay=(uint8_t)(world->road_target|16u);
  world->runway_delay=world->road_delay;world->runway_phase=36u;
  world->building_delay=15u;world->object_marker=2u;
  world->generator.left_limit=1u;world->generator.right_limit=90u;
  /* No pending DRTR lower halves from the SRT prefill in these rows. */
  memset(world->tree_positions,0,sizeof(world->tree_positions));
  world->object_control=0u;
}

static void test_sprite_clipping(void){
  static BmFramebuffer f;
  static const uint8_t pixels[]={2,0,3,4, 0,5,0,6, 7,8,9,0};
  const BmSprite sprite={pixels,4,3,4,1,1};
  memset(f.pixels,1,sizeof(f.pixels));bm_sprite_blit_c(&f,&sprite,10,10);
  assert(f.pixels[9*BM_SCREEN_WIDTH+9]==2);
  assert(f.pixels[9*BM_SCREEN_WIDTH+10]==1);
  assert(f.pixels[11*BM_SCREEN_WIDTH+11]==9);
  memset(f.pixels,1,sizeof(f.pixels));bm_sprite_blit_c(&f,&sprite,0,10);
  assert(f.pixels[9*BM_SCREEN_WIDTH]==1);
  assert(f.pixels[9*BM_SCREEN_WIDTH+1]==3);
  memset(f.pixels,1,sizeof(f.pixels));bm_sprite_blit_c(&f,&sprite,BM_SCREEN_WIDTH,10);
  assert(f.pixels[9*BM_SCREEN_WIDTH+BM_SCREEN_WIDTH-1]==2);
  memset(f.pixels,1,sizeof(f.pixels));bm_sprite_blit_c(&f,&sprite,10,0);
  assert(f.pixels[9]==1);
  assert(f.pixels[10]==5);
  memset(f.pixels,1,sizeof(f.pixels));bm_sprite_blit_c(&f,&sprite,10,BM_SCREEN_HEIGHT);
  assert(f.pixels[(BM_SCREEN_HEIGHT-1)*BM_SCREEN_WIDTH+9]==2);
  memset(f.pixels,1,sizeof(f.pixels));bm_sprite_blit_c(&f,&sprite,-10,-10);
  assert(f.pixels[0]==1);
}

static void test_bank_frames(void){
  static BmGame game;
  BmInput input={0};
  bm_game_init(&game,1234);input.x=-1;bm_game_tick(&game,&input);bm_game_render(&game);
  assert(game.player.bank==-1);
  assert(game.framebuffer.pixels[100*BM_SCREEN_WIDTH+160]==EXPECTED_PLANE_COLOUR);
  assert(game.framebuffer.pixels[103*BM_SCREEN_WIDTH+156]==EXPECTED_PLANE_COLOUR);
  bm_game_init(&game,1234);input.x=1;bm_game_tick(&game,&input);bm_game_render(&game);
  assert(game.player.bank==1);
  assert(game.framebuffer.pixels[100*BM_SCREEN_WIDTH+154]==EXPECTED_PLANE_COLOUR);
  input.x=0;bm_game_tick(&game,&input);bm_game_render(&game);
  assert(game.player.bank==0);
  assert(game.framebuffer.pixels[100*BM_SCREEN_WIDTH+154]==EXPECTED_PLANE_COLOUR);
}

static void test_player_shot(void){
  BmGame game;BmInput input={0};
  bm_game_init(&game,1234u);
  input.fire=1u;
  bm_game_tick(&game,&input);
  assert(game.player_shot_active==1u);
  assert(game.player_shot_x==164&&game.player_shot_y==102);
  input.fire=0u;
  bm_game_tick(&game,&input);
  assert(game.player_shot_x==180&&game.player_shot_y==94);
  bm_game_render(&game);
#if BM_CLASSIC_TEST_PAGE
  assert(game.framebuffer.pixels[94u*BM_SCREEN_WIDTH+180u]==26u);
#else
  assert(game.framebuffer.pixels[94u*BM_SCREEN_WIDTH+180u]==7u);
#endif
  game.player_shot_x=310;game.player_shot_y=8;
  bm_game_tick(&game,&input);
  assert(game.player_shot_active==0u);
}

static void test_player_bomb(void){
  BmGame game;BmInput input={0};
  bm_game_init(&game,1234u);
  input.bomb=1u;
  bm_game_tick(&game,&input);
  assert(game.player_bomb_active==1u);
  assert(game.player_bomb_x==156&&game.player_bomb_y==107);
  input.bomb=0u;
  bm_game_tick(&game,&input);
  assert(game.player_bomb_x==156&&game.player_bomb_y==108);
  assert(game.player_bomb_phase==1u);
  bm_game_render(&game);
#if BM_CLASSIC_TEST_PAGE
  assert(game.framebuffer.pixels[109u*BM_SCREEN_WIDTH+158u]==28u);
  assert(game.framebuffer.pixels[109u*BM_SCREEN_WIDTH+159u]==28u);
#else
  assert(game.framebuffer.pixels[109u*BM_SCREEN_WIDTH+158u]==9u);
#endif
  game.player_bomb_y=152;
  /* Grass under the bomb: the start airfield otherwise lies here. */
  memset(game.classic_world.arena,0,sizeof(game.classic_world.arena));
  memset(game.classic_world.rows,0,sizeof(game.classic_world.rows));
  bm_game_tick(&game,&input);
  assert(game.player_bomb_active==0u);
  assert(game.player_bomb_explosion==0u);
  assert(game.player_bomb_grass_impact==1u);
  assert(game.player_bomb_explosion_x==156);
  /* $61F=F6+3: shadow top 148 (F6=168) plus three lines. */
  assert(game.player_bomb_explosion_y==151);
  bm_game_render(&game);
  /* XPLSN on grass: $BA-$639=15 with $639=F6+3=171, minus this tick. */
  assert(game.classic_world.grass_impact_ticks==14u);
  { unsigned i;
    for(i=0;i<4u;i++)bm_game_tick(&game,&input);
    assert(game.classic_world.grass_impact_ticks==10u);
  }
  game.player_bomb_explosion=23u;
  bm_game_render(&game);
#if BM_CLASSIC_TEST_PAGE
  assert(game.player_bomb_target_hit==0u);
  assert(game.framebuffer.palette[BM_CLASSIC_INITIAL_BASE+3u]==
         bm_atari_pal_rgb[game.classic_world.colpf2]);
  game.player_bomb_target_hit=1u;
  game.classic_world.impact_flash_ticks=10u;
  game.player_bomb_grass_impact=0u;
  bm_game_render(&game);
  assert(game.framebuffer.palette[BM_CLASSIC_INITIAL_BASE+3u]==
         bm_atari_pal_rgb[0x2eu]);
  assert(game.framebuffer.palette[BM_CLASSIC_INITIAL_BASE+4u]==
         bm_atari_pal_rgb[0x38u]);
#endif
  for(;game.player_bomb_explosion;)bm_game_tick(&game,&input);
  assert(game.player_bomb_explosion==0u);
}

static void test_bomb_world_impact(void){
  BmWorldBuffer world;uint16_t offset;uint8_t before;
  { unsigned code,phase;
    for(code=0xecu;code<=0xefu;++code)for(phase=0;phase<4;++phase){
      int x=196+(int)phase*2,y=132-(int)phase*2;
      bm_world_buffer_init(&world,1234u);
      memset(world.rows,0,sizeof(world.rows));
      world.source_column=0;world.fine_phase=(uint8_t)phase;
      world.rows[16][25]=(uint8_t)code;
      offset=(uint16_t)(world.lms[16]+BM_VIEW_LMS_OFFSET+25u);
      world.arena[offset]=(uint8_t)code;
      assert(bm_world_bomb_impact(&world,(int16_t)x,(int16_t)y)==1u);
      assert(world.bomb_impact_code==code&&world.bomb_impact_hit);
      assert(world.impact_mode==1u&&world.impact_flash_ticks==12u);
      assert(world.arena[offset]==0x40u);
    }
  }
  bm_world_buffer_init(&world,1234u);
  world.hangar_target_available=1u;
  world.fine_phase=0u;world.scroll_clock=1;world.source_column=0u;
  offset=(uint16_t)(world.lms[19]+BM_VIEW_LMS_OFFSET+22u);
  world.arena[offset]=0x95u;
  world.arena[offset+1u]=0x7eu;
  world.arena[world.lms[18]+BM_VIEW_LMS_OFFSET+22u]=0x96u;
  world.rows[19][22]=0x95u;
  world.rows[19][23]=0x7eu;
  world.rows[18][22]=0x96u;
  assert(bm_world_bomb_impact(&world,156,152)==1u);
  assert(world.bomb_impact_code==0x95u&&world.bomb_impact_hit==1u);
  assert(world.impact_mode==9u&&world.impact_flash_ticks==12u);
  assert(world.arena[offset]==0x95u&&world.arena[offset+1u]==0x7eu);
  assert(world.arena[world.lms[18]+BM_VIEW_LMS_OFFSET+22u]==0x96u);
  assert(world.hangar_target_available==0u);
  { uint8_t timer=world.grass_impact_ticks;
    assert(bm_world_bomb_impact(&world,156,152)==0u);
    assert(world.bomb_impact_handled&&world.bomb_impact_hit==0u);
    assert(world.grass_impact_ticks==timer);
  }
  world.rows[19][23]=0u;world.arena[offset+1u]=0u;
  before=0u;
  world.arena[offset]=0xfcu;world.rows[19][22]=0xfcu;
  world.rows[19][20]=0xfcu;
  assert(bm_world_bomb_impact(&world,156,152)==1u);
  assert(world.bomb_impact_code==0xfcu&&world.bomb_impact_hit==1u);
  assert(world.impact_mode==6u&&world.bomb_impact_handled);
  assert(world.arena[offset]==0xfcu&&before==0u);
}

static void test_bomb_water_impact(void){
  BmWorldBuffer world;uint16_t offset;
  bm_world_buffer_init(&world,1234u);
  world.area_type=0u;world.source_column=0u;
  world.fine_phase=0u;world.scroll_clock=1;
  offset=(uint16_t)(world.lms[19]+BM_VIEW_LMS_OFFSET+22u);
  world.arena[offset]=0x02u;world.rows[19][22]=0x02u;
  world.rows[19][20]=0x02u;
  assert(bm_world_bomb_impact(&world,156,152)==0u);
  assert(world.bomb_impact_code==0x02u);
  assert(world.bomb_impact_water==1u&&world.bomb_impact_hit==0u);
  assert(world.arena[offset]==0x02u);
  assert(world.impact_mode==0u&&world.grass_impact_ticks==18u);
  offset=world.grass_impact_offset;
  assert(world.arena[offset]==0x43u);
  while(world.grass_impact_ticks>1u){
    world.fine_phase=3u;bm_world_buffer_step(&world);
    assert(world.arena[offset]==(world.grass_impact_ticks==1u?0x02u:
      ((world.grass_impact_ticks&4u)?0x43u:0x44u)));
  }
  bm_world_buffer_step(&world);
  assert(world.arena[offset]==0x02u);
}

static void test_bomb_grass_impact(void){
  BmWorldBuffer world;int row,column;
  /* XPLSN defaults to land even for codes outside the old terrain list.
     F4 is the impact code shown in the first hardware report. */
  { static const uint8_t codes[]={0xf4u,0x40u,0x91u};
    unsigned i;
    for(i=0;i<sizeof(codes);++i){
      bm_world_buffer_init(&world,1234u);
      world.source_column=0u;world.fine_phase=0u;world.scroll_clock=1;
      for(row=19;row<=20;++row)
        for(column=18;column<=24;++column){
          world.rows[row][column]=0u;
          world.arena[world.lms[row]+BM_VIEW_LMS_OFFSET+column]=0u;
        }
      world.rows[19][20]=codes[i];
      world.arena[world.lms[19]+BM_VIEW_LMS_OFFSET+20u]=codes[i];
      assert(bm_world_bomb_impact(&world,156,152)==0u);
      assert(world.bomb_impact_handled&&world.impact_mode==1u);
      assert(world.arena[world.lms[19]+BM_VIEW_LMS_OFFSET+20u]==0x40u);
    }
    /* Hangar code 20 inside ADA's +-2 search: with $6F9 armed by the start
       airfield (MB phase 23), Z5 claims it as a hangar hit (mode 9). */
    world.fine_phase=3u;
    world.rows[19][19]=0x20u;
    world.arena[world.lms[19]+BM_VIEW_LMS_OFFSET+19u]=0x20u;
    assert(world.hangar_target_available);
    assert(bm_world_bomb_impact(&world,156,152)==1u);
    assert(world.bomb_impact_code==0x20u&&world.bomb_impact_handled);
    assert(world.impact_mode==9u);
  }
  bm_world_buffer_init(&world,1234u);
  world.source_column=0u;
  world.fine_phase=0u;world.scroll_clock=1;
  for(row=19;row<=20;++row)
    for(column=20;column<=24;++column){
      world.rows[row][column]=0u;
      world.arena[world.lms[row]+BM_VIEW_LMS_OFFSET+column]=0u;
    }
  /* A neighbouring road in the footprint must survive. */
  world.rows[20][21]=0xfcu;
  world.arena[world.lms[20]+BM_VIEW_LMS_OFFSET+21u]=0xfcu;
  assert(bm_world_bomb_impact(&world,156,152)==0u);
  assert(world.arena[world.lms[19]+BM_VIEW_LMS_OFFSET+20u]==0x40u);
  assert(world.arena[world.lms[20]+BM_VIEW_LMS_OFFSET+21u]==0xfcu);
  assert(world.bomb_impact_hit==0u&&world.bomb_impact_water==0u);
  assert(world.arena[world.lms[20]+BM_VIEW_LMS_OFFSET+20u]==0u);
  { unsigned offset=world.grass_impact_offset;
    uint8_t left,right;
    assert(world.grass_impact_ticks==14u); /* $BA-(152+20) */
    while(world.grass_impact_ticks>1u){
      /* Isolate XPLSN phase/expiry from arena-row recycling. */
      world.fine_phase=3u;
      bm_world_buffer_step(&world);
      assert(world.arena[offset]==((world.grass_impact_ticks&4u)?0x40u:0x3eu));
      assert(world.arena[offset+1u]==((world.grass_impact_ticks&4u)?0x42u:0x41u));
    }
    left=world.arena[offset];right=world.arena[offset+1u];
    bm_world_buffer_step(&world);
    assert(world.grass_impact_ticks==0u);
    assert(world.arena[offset]==left&&world.arena[offset+1u]==right);
    /* The source's shoreline guard applies only to the second glyph. */
    world.grass_impact_ticks=8u;world.arena[offset+1u]=0x0eu;
    bm_world_buffer_step(&world);
    assert(world.arena[offset+1u]==0x0eu);
    world.arena[offset]=0u;world.grass_impact_ticks=8u;
    bm_world_buffer_step(&world);
    assert(world.grass_impact_ticks==0u&&world.arena[offset]==0u);
  }
}

static void test_bomb_source_object_routes(void){
  static const uint8_t codes[]={33u,0xf0u,0xc6u,0x49u};
  unsigned i;
  for(i=0;i<sizeof(codes);++i){
    BmWorldBuffer world;unsigned row,col,offset;
    bm_world_buffer_init(&world,1234u);world.fine_phase=0u;world.scroll_clock=1;
    for(row=18;row<=20;++row)for(col=18;col<=26;++col){
      world.rows[row][col]=0u;world.arena[world.lms[row]+BM_VIEW_LMS_OFFSET+col]=0u;
    }
    col=i==0u?20u:22u;
    offset=world.lms[19]+BM_VIEW_LMS_OFFSET+col;
    world.rows[19][col]=codes[i];world.arena[offset]=codes[i];
    assert(bm_world_bomb_impact(&world,156,152)==(i==0u?0u:1u));
    assert(world.impact_mode==(i==3u?4u:1u));
    assert(world.impact_flash_ticks==(i==0u?0u:12u));
    assert(world.arena[offset]==(i==3u?0x49u:0x40u));
    world.fine_phase=3u;bm_world_buffer_step(&world);
    assert(world.arena[offset]==(i==3u?0x49u:
      ((world.grass_impact_ticks&4u)?0x40u:0x3eu)));
  }
}

static void test_weapon_scoring(void){
  static const uint8_t targets[]={0u,0xf0u,0xc6u,0x49u,0x95u};
  static const unsigned points[]={0u,30u,30u,50u,100u};
  BmGame game;BmInput input={0};unsigned i,row,col;
  bm_game_init(&game,1234u);
  input.fire=1u;input.bomb=1u;
  for(i=0;i<10u;++i)bm_game_tick(&game,&input);
  assert(game.score==0u);
  memset(&input,0,sizeof(input));
  for(i=0;i<sizeof(targets);++i){
    bm_game_init(&game,1234u);game.score=700u;
    game.classic_world.hangar_target_available=1u;
    for(row=18;row<=20;++row)for(col=18;col<=25;++col){
      game.classic_world.rows[row][col]=0u;
      game.classic_world.arena[game.classic_world.lms[row]+BM_VIEW_LMS_OFFSET+col]=0u;
    }
    game.classic_world.rows[19][22]=targets[i];
    game.classic_world.arena[game.classic_world.lms[19]+BM_VIEW_LMS_OFFSET+22u]=targets[i];
    game.player_bomb_active=1u;game.player_bomb_target_y=151;game.player_bomb_x=156;game.player_bomb_y=152;
    bm_game_tick(&game,&input);
    assert(game.score==700u+points[i]);
    for(col=0;col<4u;++col)bm_game_tick(&game,&input);
    assert(game.score==700u+points[i]);
  }
  {
    BmGame objective;
    bm_game_init(&objective,1234u);
    objective.mission_objective_type=1u;
    objective.classic_world.rows[19][22]=0x49u;
    objective.classic_world.arena[
      objective.classic_world.lms[19]+BM_VIEW_LMS_OFFSET+22u]=0x49u;
    objective.player_bomb_active=1u;objective.player_bomb_target_y=151;objective.player_bomb_x=156;
    objective.player_bomb_y=152;objective.player.y=105;
    { uint32_t before=objective.score;
      bm_game_tick(&objective,&input);
      assert(objective.mission_targets_destroyed==1u&&
             objective.mission_objective_type==0u);
      /* W4/U4: the tank under the sight scores S50 twice. */
      assert(objective.score==before+100u);
    }
  }
}

static void test_shore_impact_routes(void){
  static const uint8_t codes[]={3,8,13,5,12,4,7,10,14,15,16};
  unsigned i,r,c;
  for(i=0;i<sizeof(codes);++i){
    BmWorldBuffer w;unsigned original,expected;uint8_t water=i<3u;
    bm_world_buffer_init(&w,1234);w.fine_phase=0;w.source_column=0;
    /* Distinct, nonconsecutive LMS rows detect incorrect stride arithmetic. */
    w.lms[19]=1000;w.lms[20]=1500;
    for(r=19;r<=20;++r)for(c=18;c<=25;++c){
      w.rows[r][c]=0;w.arena[w.lms[r]+BM_VIEW_LMS_OFFSET+c]=0;
    }
    original=w.lms[19]+BM_VIEW_LMS_OFFSET+20u;
    w.rows[19][20]=codes[i];w.arena[original]=codes[i];
    expected=water?w.lms[20]+BM_VIEW_LMS_OFFSET+20u:original+((i==3u||i==4u)?1u:0u);
    w.arena[expected+1u]=0x02;
    assert(bm_world_bomb_impact(&w,156,152)==0);
    assert(w.bomb_impact_handled&&w.bomb_impact_water==water);
    assert(w.grass_impact_offset==expected&&w.impact_mode==(water?0:2));
    assert(w.arena[expected]==(water?0x43:0x40));
    if(expected!=original)assert(w.arena[original]==codes[i]);
    w.fine_phase=3;bm_world_buffer_step(&w);
    assert(w.arena[expected+1u]==0x02); /* single-cell bank animation */
  }
}

static void test_impact_character_memory(void){
  BmWorldBuffer w;unsigned i;
  bm_world_buffer_init(&w,1234);
  w.impact_mode=9;w.grass_impact_ticks=3;
  w.fine_phase=3;bm_world_buffer_step(&w);
  assert(w.hangar_impact_phase==1); /* ABC */
  w.fine_phase=3;bm_world_buffer_step(&w);
  assert(w.hangar_impact_phase==2); /* ABD */
  w.fine_phase=3;bm_world_buffer_step(&w);
  assert(w.hangar_impact_phase==2); /* retained after expiry */
  w.impact_mode=4;w.grass_impact_ticks=7;
  w.fine_phase=3;bm_world_buffer_step(&w);
  assert(w.tank_impact_phase==0&&w.hangar_impact_phase==2);
  for(i=0;i<5;++i){w.fine_phase=3;bm_world_buffer_step(&w);}
  assert(w.tank_impact_phase==2&&w.hangar_impact_phase==2);
  w.impact_mode=0;w.grass_impact_ticks=0;
  w.fine_phase=3;bm_world_buffer_step(&w);
  assert(w.hangar_impact_phase==2&&w.tank_impact_phase==2);
}

static void test_bridge_building_vehicle_impacts(void){
  static const uint8_t codes[]={0x58,0x56,0x54,0x5e,0x5c,0x5a,0xde,0xdc,
    0xda,0x57,0x5d,0xdd,0x59,0x55,
    0xb9,0xab,0xac,0xae,0xa9,0xb2,0xdb,0xaa,0xb7,0xb8,
    0xfd,0xce,0xfc,0xcd,0xcc,0xcb,0xb5,0xb6,
    0xba,0xbb,0xbc,0xbd,0xd0,0xd1,0xd2,0xd3};
  unsigned i,r,c;
  for(i=0;i<sizeof(codes);++i){
    BmGame g;BmInput input={0};BmWorldBuffer *w=&g.classic_world;
    unsigned a,b;uint8_t mode=i<14?5:(i<24?3:(i<32?6:(i<36?7:8)));
    bm_game_init(&g,1234);w->fine_phase=1;w->source_column=0;
    w->lms[18]=700;w->lms[19]=1000;w->lms[20]=1500;
    for(r=18;r<=20;++r)for(c=18;c<=26;++c){
      w->rows[r][c]=0;w->arena[w->lms[r]+BM_VIEW_LMS_OFFSET+c]=0;
    }
    w->rows[19][20]=codes[i];w->arena[w->lms[19]+BM_VIEW_LMS_OFFSET+20u]=codes[i];
    /* A nearby AAG target must not override CKCHAR's bridge route. */
    if(mode==3){
      w->rows[19][22]=0xecu;w->arena[w->lms[19]+BM_VIEW_LMS_OFFSET+22u]=0xecu;
    }
    g.player_bomb_active=1;g.player_bomb_target_y=151;g.player_bomb_x=156;g.player_bomb_y=152;
    bm_game_tick(&g,&input);
    assert(w->impact_mode==mode&&w->bomb_impact_handled);
    assert(g.player_bomb_explosion==0u);
    a=w->grass_impact_offset;b=w->impact_other_offset;
    if(mode==5){
      assert(w->arena[a]==codes[i]);
      w->grass_impact_ticks=5;w->fine_phase=3;bm_world_buffer_step(w);
      assert(w->building_impact_phase==2);
      continue;
    }
    if(mode==3||mode>=7){
      int down=(i>=21&&i<24)||i==32||i==33||i==36||i==37;
      assert(b==(unsigned)(w->lms[down?20:18]+BM_VIEW_LMS_OFFSET+20u-
        ((i==33||i==35||i==36||i==38)?1u:0u)));
    }
    w->grass_impact_ticks=5;w->fine_phase=3;bm_world_buffer_step(w);
    if(mode==3){assert(w->arena[a]==0xf7&&w->arena[a+1]==0xf8);
      assert(w->arena[b]==0xf7&&w->arena[b+1]==0xf8);}
    else if(mode==7){assert(w->arena[a]==0xf6&&w->arena[b]==0xf9);}
    else assert(w->arena[a]==0xfb&&w->arena[a+1]==0xfb);
    w->grass_impact_ticks=2;w->fine_phase=3;bm_world_buffer_step(w);
    if(mode==7)assert(w->arena[a]==2&&w->arena[b]==2);
    else assert(w->arena[a]==(mode==3?0xf5:0xf9));
    /* Recycled terrain must not be damaged by a stale impact. */
    w->arena[a]=0;w->grass_impact_ticks=8;w->fine_phase=3;
    bm_world_buffer_step(w);assert(w->grass_impact_ticks==0&&w->arena[a]==0);
  }
}

/* End-to-end "joystick plus one bomb sinks the first ship" scenario.  Since
   the T04 geometry fix the ship keeps its own water position (no preview
   recentering).  With the diagnostic river limits 22/40 every ship reaches
   the fixed preview bomb line (shadow F6=168) only left of the screen.
   Re-enable after T01 (SRT limits 16/75) and the Atari flight model. */
#ifndef BM_TEST_SHIP_BOMB_SCENARIO
#define BM_TEST_SHIP_BOMB_SCENARIO 0
#endif
static void test_river_vehicle_shot(void){
#if BM_CLASSIC_TEST_PAGE && BM_TEST_SHIP_BOMB_SCENARIO
  { static BmGame start,g;BmInput in={0};unsigned t,release,hit=0,ships=0;
    bm_game_init(&start,0x424d4158u);
    /* Joystick plus a single bomb only: follow the ship and search the
       release moment instead of hard-coding one geometry-dependent tick.
       The preview keeps the shadow on F6=168, so a ship near the left bank
       can leave the screen before it reaches that bomb line; try the next. */
    while(!hit&&ships<8u){
    while(start.classic_world.ship_active&&start.tick<0x10000)
      bm_game_tick(&start,&in);
    while(!start.classic_world.ship_active&&start.tick<0x10000)
      bm_game_tick(&start,&in);
    assert(start.classic_world.ship_active);
    ++ships;
    for(release=0;release<240u&&!hit;++release){
      g=start;
      for(t=0;t<240&&!g.classic_world.ship_destroyed;++t){
        int desired=bm_pm_x(g.classic_world.ship_x)+8;
        in.x=g.player.x<desired?1:(g.player.x>desired?-1:0);
        in.bomb=t==release;
        bm_game_tick(&g,&in);
      }
      hit=g.classic_world.ship_destroyed&&g.score==start.score+100u;
    }
    }
    assert(hit);
    assert(!g.player_bomb_active&&!g.player_bomb_explosion);
    in.x=0;in.bomb=0;
    for(t=0;t<12;++t)bm_game_tick(&g,&in);
    assert(g.score==start.score+100u&&!g.classic_world.impact_flash_ticks);
  }
#endif
  { unsigned layer;
    for(layer=0;layer<2;++layer){
      BmGame g;BmInput in={0};unsigned p;
      const uint8_t *pixels=layer?bm_asset_ship_layer_1:bm_asset_ship_layer_0;
      unsigned height=layer?BM_ASSET_SHIP_LAYER_1_HEIGHT:BM_ASSET_SHIP_LAYER_0_HEIGHT;
      for(p=0;p<height*16&&!pixels[p];++p){}
      assert(p<height*16);
      bm_game_init(&g,1234);g.classic_world.ship_active=1;
      g.classic_world.ship_x=123;
      /* Ground line F6+3 is y=151; SHPIM starts at C1-11. */
      g.classic_world.ship_y=(uint8_t)(182-p/16-(layer?2:0));
      g.classic_world.ship_hits_remaining=3;
      g.player_bomb_active=1;g.player_bomb_target_y=151;g.player_bomb_y=152;
      g.player_bomb_x=(int16_t)(150+p%16);
      bm_game_tick(&g,&in);
      assert(g.classic_world.ship_destroyed&&g.classic_world.ship_hits_remaining==0);
      assert(!g.player_bomb_explosion&&g.player_bomb_target_hit&&g.score==100);
      assert(!g.classic_world.gun2_shot_y);
      bm_game_tick(&g,&in);assert(g.score==100);
      bm_game_init(&g,1234);g.classic_world.ship_active=1;
      g.classic_world.ship_x=123;g.classic_world.ship_y=175;
      g.classic_world.ship_hits_remaining=3;
      g.player_bomb_active=1;g.player_bomb_target_y=151;g.player_bomb_y=152;g.player_bomb_x=16;
      bm_game_tick(&g,&in);
      assert(!g.classic_world.ship_destroyed&&g.classic_world.ship_hits_remaining==3);
    }
  }
#if BM_CLASSIC_TEST_PAGE
  { static BmGame g;BmInput in={0};unsigned t,visible_ticks=0,moored=0;
    bm_game_init(&g,0x424d4158u);
    for(t=0;t<0x2000u*(BM_ATARI_SCROLL?2u:1u);++t){
      uint8_t active=g.classic_world.ship_active;
      uint8_t old_x=g.classic_world.ship_x,old_y=g.classic_world.ship_y;
      bm_game_tick(&g,&in);
      if(active&&g.classic_world.ship_active){
        int dy=(int)g.classic_world.ship_y-old_y;
        assert(dy==0||dy==2||dy==4);
        assert((int)old_x-g.classic_world.ship_x==dy/2);
      }
      { unsigned r,c;
        for(r=0;r<BM_ATARI_PLAYFIELD_FETCH_ROWS;++r)
          for(c=1;c+1<BM_ATARI_PLAYFIELD_FETCH_COLUMNS;++c)
            if(g.classic_world.rows[r][c]==0xbau&&
               g.classic_world.rows[r][c+1]==0xbbu)++moored;
      }
      if(g.classic_world.ship_active){
        ++visible_ticks;
        assert(!g.classic_world.ship_destroyed);
        assert(g.classic_world.ship_hits_remaining==3u);
      }
    }
    assert(visible_ticks>120u); /* Past the former automatic explosion. */
    assert(moored>0); /* Separate, terrain-anchored DRBT boats are present. */
    /* Reach the first real ship using ONLY joystick/fire, no injected shots. */
    bm_game_init(&g,0x424d4158u);
    for(t=0;t<0x4000u&&!g.classic_world.ship_destroyed;++t){
      unsigned score_before=g.score;
      int desired=16;
      if(g.classic_world.ship_active){
        int sy=(int)g.classic_world.ship_y-31;
        desired=((int)g.classic_world.ship_x-48)*2-17*(115-sy)/9+4;
      }
      if(desired<16)desired=16;
      in.x=g.player.x<desired?1:(g.player.x>desired?-1:0);
      in.y=g.player.y<126?1:0;in.fire=1;
      bm_game_tick(&g,&in);
      /* S7: 100, or 300 for a ship drawn in the bonus colour 50. */
      if(g.classic_world.ship_destroyed)
        assert(g.score==score_before+(g.classic_world.p2_colour==50u?300u:100u));
    }
    assert(g.classic_world.ship_destroyed&&g.classic_world.ship_hits_remaining==0);
    /* Earlier road targets can also be hit by this continuous-fire run. */
  }
#endif
  { BmGame g;BmInput in={0};unsigned n,p;
    bm_game_init(&g,1234);g.player.y=126;
    g.classic_world.ship_active=1;g.classic_world.ship_hits_remaining=3;
    g.classic_world.object_slot=15u;g.classic_world.p2_hits=3u;
    g.classic_world.p2_colour=2u;
    for(p=0;p<16u*BM_ASSET_SHIP_LAYER_0_HEIGHT&&!bm_asset_ship_layer_0[p];++p){}
    assert(p<16u*BM_ASSET_SHIP_LAYER_0_HEIGHT);
    for(n=0;n<3;++n){
      g.classic_world.ship_x=123;g.classic_world.ship_y=111;
      g.player_shot_active=1;
      g.player_shot_x=(int16_t)(150+p%16-8);
      g.player_shot_y=(int16_t)(80+p/16+4);
      bm_game_tick(&g,&in);
      assert(!g.player_shot_active);
      assert(g.classic_world.ship_hits_remaining==2-n);
      assert(g.classic_world.ship_destroyed==(n==2));
      assert(g.score==(n==2?100u:0u));
    }
    assert(!g.classic_world.gun2_shot_y);
    bm_game_tick(&g,&in);assert(g.score==100);
  }
  unsigned truck,left;
  for(truck=0;truck<2;++truck)for(left=0;left<2;++left){
    BmGame g;BmInput in={0};unsigned p;
    const uint8_t *body=truck?(left?bm_asset_truck_left_body:bm_asset_truck_right_body):
      (left?bm_asset_car_left_body:bm_asset_car_right_body);
    bm_game_init(&g,1234);
    g.player.y=126; /* SHP23: altitude 27, just below the cutoff. */
    g.classic_world.river_car_active=1;g.classic_world.object_slot=1u;
    g.classic_world.p2_colour=146u;g.classic_world.river_car_x=100;
    g.classic_world.river_car_y=100;g.classic_world.river_car_left=(uint8_t)left;
    g.classic_world.river_vehicle_truck=(uint8_t)truck;
    for(p=0;p<16u*(truck?6u:5u)&&!body[p];++p){}
    assert(p<16u*(truck?6u:5u));
    g.player_shot_active=1;
    g.player_shot_x=(int16_t)(104+p%16-8);
    g.player_shot_y=(int16_t)(80+p/16+4);
    bm_game_tick(&g,&in);
    assert(g.classic_world.river_car_destroyed&&g.score==100);
    assert(!g.player_shot_active); /* SHP23 calls HG to consume TRIG. */
    bm_game_render(&g);
#if BM_CLASSIC_TEST_PAGE
    assert(g.classic_world.river_car_destroy_timer==119);
    assert(g.framebuffer.palette[32]==bm_atari_pal_rgb[0x2c]);
    assert(g.classic_world.river_car_x==(BM_ATARI_SCROLL?100:99));
    assert(g.classic_world.river_car_y==(BM_ATARI_SCROLL?100:102));
#endif
    bm_game_tick(&g,&in);assert(g.score==100);
#if BM_CLASSIC_TEST_PAGE
    assert(g.classic_world.river_car_x==(BM_ATARI_SCROLL?99:98));
    assert(g.classic_world.river_car_y==(BM_ATARI_SCROLL?102:104));
    /* R5: the wreck is released (R9/JG, $FD=0) once C1>=185. */
    g.classic_world.river_car_y=184;bm_game_tick(&g,&in);
    assert(g.classic_world.river_car_active);
    g.classic_world.river_car_y=185;bm_game_tick(&g,&in);
    assert(!g.classic_world.river_car_active&&!g.classic_world.object_slot);
#endif
  }
  { BmGame g;BmInput in={0};unsigned p;
    bm_game_init(&g,1234);g.player.y=125; /* altitude 28: no shot hit */
    memset(g.classic_world.arena,0x7e,sizeof(g.classic_world.arena));
    memset(g.classic_world.rows,0x7e,sizeof(g.classic_world.rows));
    g.classic_world.river_car_active=1;g.classic_world.object_slot=1u;
    g.classic_world.p2_colour=146u;g.classic_world.river_car_x=100;
    g.classic_world.river_car_y=100;
    for(p=0;p<80&&!bm_asset_car_right_body[p];++p){}
    assert(p<80);
    g.player_shot_active=1;
    g.player_shot_x=(int16_t)(104+p%16-8);
    g.player_shot_y=(int16_t)(80+p/16+4);
    bm_game_tick(&g,&in);
    assert(g.player.altitude==28&&!g.classic_world.river_car_destroyed&&g.score==0);
    assert(g.player_shot_active);
  }
  { BmGame g;BmInput in={0};
    bm_game_init(&g,1234);g.classic_world.river_car_active=1;
    g.classic_world.object_slot=1u;g.classic_world.p2_colour=146u;
    memset(g.classic_world.arena,0x7e,sizeof(g.classic_world.arena));
    memset(g.classic_world.rows,0x7e,sizeof(g.classic_world.rows));
    g.classic_world.river_car_x=100;g.classic_world.river_car_y=100;
    g.player_shot_active=1;g.player_shot_x=80;g.player_shot_y=60;
    bm_game_tick(&g,&in);assert(!g.classic_world.river_car_destroyed&&g.score==0);
    /* XP23 terrain collision is independent of a player shot. */
    g.player_shot_active=0;
    memset(g.classic_world.arena,0xf5,sizeof(g.classic_world.arena));
    memset(g.classic_world.rows,0xf5,sizeof(g.classic_world.rows));
    bm_game_tick(&g,&in);
    assert(g.classic_world.river_car_destroyed&&g.score==100);
    bm_game_tick(&g,&in);assert(g.score==100);
  }
}

static void test_antic4_tiles(void){
  static BmFramebuffer f;
  static uint8_t charset[1024];
  static const uint8_t screen_codes[]={0x00,0x80};
  static const uint8_t strided_codes[]={0x00,0x00,0x7f,0x01,0x00,0x7f};
  static const uint8_t registers[]={0x00,0xc4,0x94,0xd2,0x04};
  static const BmAntic4Zone zones[]={
    {0,{16,17,18,19,20}},
    {4,{21,22,23,24,25}}
  };
  unsigned y;
  memset(&f,0,sizeof(f));
  for(y=0;y<8;y++)charset[y]=0x1b; /* pairs 00, 01, 10, 11 */
  for(y=0;y<8;y++)charset[8+y]=0xff;
  bm_atari_pal_install(&f,BM_CLASSIC_INITIAL_BASE,registers);
  assert(f.palette[16]==0x040404u);
  assert(f.palette[17]==0x1d5000u);
  assert(f.palette[18]==0x034653u);
  assert(f.palette[19]==0x1b2e00u);
  assert(f.palette[20]==0x333333u);
  bm_antic4_blit(&f,charset,screen_codes,2,1,2,0,0,zones,2);
  assert(f.pixels[0]==16&&f.pixels[2]==17&&f.pixels[4]==18&&f.pixels[6]==19);
  assert(f.pixels[8]==16&&f.pixels[14]==20); /* screen-code bit 7: COLPF3 */
  assert(f.pixels[4*BM_SCREEN_WIDTH]==21);
  assert(f.pixels[4*BM_SCREEN_WIDTH+6]==24);
  assert(f.pixels[4*BM_SCREEN_WIDTH+14]==25);
  memset(f.pixels,99,sizeof(f.pixels));
  bm_antic4_blit(&f,charset,screen_codes,1,1,2,-2,0,zones,1);
  assert(f.pixels[0]==17&&f.pixels[2]==18&&f.pixels[4]==19);
  assert(f.pixels[BM_SCREEN_WIDTH-1]==99);
  memset(f.pixels,99,sizeof(f.pixels));
  bm_antic4_blit(&f,charset,strided_codes,2,2,3,0,0,zones,1);
  assert(f.pixels[8*BM_SCREEN_WIDTH]==19); /* second row starts at stride 3 */
}

static void test_antic4_playfield(void){
  static BmFramebuffer f;
  static uint8_t charset[1024];
  static uint8_t world[BM_ATARI_PLAYFIELD_FETCH_ROWS*BM_ATARI_WORLD_STRIDE];
  static const BmAntic4Zone zone={0,{16,17,18,19,20}};
  unsigned row;
  assert(BM_ATARI_PLAYFIELD_HEIGHT==176);
  memset(&f,0,sizeof(f));memset(charset,0,sizeof(charset));memset(world,0,sizeof(world));
  for(row=0;row<8;row++)charset[8+row]=0x1b;
  for(row=0;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;row++)world[row*BM_ATARI_WORLD_STRIDE+1]=1;
  assert(bm_antic4_blit_playfield(&f,charset,world,0,0,0,0,&zone,1)==0);
  assert(f.pixels[0]==16&&f.pixels[2]==17&&f.pixels[4]==18&&f.pixels[6]==19);
  memset(f.pixels,0,sizeof(f.pixels));
  assert(bm_antic4_blit_playfield(&f,charset,world,0,3,0,0,&zone,1)==0);
  assert(f.pixels[0]==16&&f.pixels[8]==17); /* extra tile covers left border */
  assert(bm_antic4_blit_playfield(&f,charset,world,25,0,0,0,&zone,1)==-1);
  assert(bm_antic4_blit_playfield(&f,charset,world,0,4,0,0,&zone,1)==-1);
}

static void test_dfbldg_charset(void){
  static const uint8_t expected_abz[]={0xfc,0xfc,0xf1,0xf1,0xc5,0xc5,0x15,0x15};
  static const uint8_t expected_acf[]={0x3f,0x3f,0x3f,0x03,0x03,0x03,0x3f,0x3f};
  assert(memcmp(bm_asset_max3_building_charset+90*8,expected_abz,8)==0);
  assert(memcmp(bm_asset_max3_building_charset+93*8,expected_acf,8)==0);
  assert(memcmp(bm_asset_max3_building_charset+90*8,
                bm_asset_max3_charset+90*8,8)!=0);
  assert(memcmp(bm_asset_max3_building_charset+93*8,
                bm_asset_max3_charset+93*8,8)!=0);
  assert(memcmp(bm_asset_max3_hangar_charset+21*8,
                bm_asset_max3_charset+21*8,96)==0);
  assert(memcmp(bm_asset_max3_hangar_charset+126*8,
                bm_asset_max3_charset+126*8,8)==0);
  assert(memcmp(bm_asset_max3_hangar_charset+90*8,
                bm_asset_max3_building_charset+90*8,8)==0);
  assert(memcmp(bm_asset_max3_hangar_charset+93*8,
                bm_asset_max3_building_charset+93*8,8)==0);
}

static void test_dftnk_charset(void){
  static const unsigned targets[]={69,79,73,74,18,63,127};
  static const uint8_t expected[][8]={
    {0x3f,0x3f,0x3f,0x03,0x03,0x03,0x3f,0x3f},
    {0x55,0x55,0x55,0x55,0x5f,0x5f,0xff,0xff},
    {0x03,0x03,0x0f,0x0f,0x3f,0x3f,0xff,0xff},
    {0xfc,0xfc,0xf1,0xf1,0xc5,0xc5,0x15,0x15},
    {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x54,0x54,0x50,0x50,0x40,0x40,0x00,0x00}};
  unsigned index;
  for(index=0;index<sizeof(targets)/sizeof(targets[0]);index++){
    assert(memcmp(bm_asset_max3_tank_charset+targets[index]*8,
                  expected[index],8)==0);
  }
  assert(memcmp(bm_asset_max3_tank_charset+90*8,
                bm_asset_max3_building_charset+90*8,8)==0);
  assert(memcmp(bm_asset_max3_tank_charset+21*8,
                bm_asset_max3_hangar_charset+21*8,96)==0);
  assert(memcmp(bm_asset_max3_tank_charset+126*8,
                bm_asset_max3_hangar_charset+126*8,8)==0);
}

typedef struct { const uint8_t *values; unsigned index; } TestRandom;
static uint8_t test_random_byte(void *userdata){
  TestRandom *random=(TestRandom*)userdata;return random->values[random->index++];
}

static void test_draw_segment_rules(void){
  BmWorldGenerator g;uint8_t row[BM_ATARI_WORLD_STRIDE];
  /* An expired left turn always continues through RND: (RANDOM&3)|2, then
     STRGHT decrements it once more. */
  { static const uint8_t v[]={0};TestRandom r={v,0};
    bm_world_generator_init(&g);g.direction=BM_ROAD_LEFT;g.segment_remaining=1;
    bm_world_generate_row(&g,row,test_random_byte,&r);
    assert(g.direction==BM_ROAD_STRAIGHT&&g.segment_remaining==1&&r.index==1);
  }
  /* An expired right turn clears $606, so the straight draws 10,9. */
  { static const uint8_t v[]={0};TestRandom r={v,0};
    bm_world_generator_init(&g);g.direction=BM_ROAD_RIGHT;g.segment_remaining=1;
    g.centre=40;g.shoulder_pending=1;
    bm_world_generate_row(&g,row,test_random_byte,&r);
    assert(g.direction==BM_ROAD_STRAIGHT&&row[40]==1&&row[20]==3);
    assert(row[19]==10&&row[18]==9&&g.shoulder_pending==1);
  }
  /* AX even -> AW: length 15, EC even -> AV: right turn of 3|(15>>1)=7,
     immediately decremented by RGHT. */
  { static const uint8_t v[]={0,15,0};TestRandom r={v,0};
    bm_world_generator_init(&g);g.direction=BM_ROAD_STRAIGHT;g.segment_remaining=1;
    g.turn_preference=0;g.previous_turn=0;g.centre=40;
    bm_world_generate_row(&g,row,test_random_byte,&r);
    assert(g.direction==BM_ROAD_RIGHT&&g.segment_remaining==6&&g.centre==41);
  }
  /* The same draw with EC odd -> BA keeps the full left length 15-1. */
  { static const uint8_t v[]={0,15,1};TestRandom r={v,0};
    bm_world_generator_init(&g);g.direction=BM_ROAD_STRAIGHT;g.segment_remaining=1;
    g.turn_preference=0;g.previous_turn=0;g.centre=40;
    bm_world_generate_row(&g,row,test_random_byte,&r);
    assert(g.direction==BM_ROAD_LEFT&&g.segment_remaining==14);
  }
}

static void test_world_rows(void){
  BmWorldGenerator g;uint8_t row[BM_ATARI_WORLD_STRIDE];
  static const uint8_t random_values[]={1,2};TestRandom random={random_values,0};
  bm_world_generator_init(&g);
  assert(g.direction==BM_ROAD_LEFT&&g.segment_remaining==6&&g.centre==26);
  memset(row,0xaa,sizeof(row));g.direction=BM_ROAD_STRAIGHT;g.segment_remaining=2;
  g.shoulder_pending=1;bm_world_generate_row(&g,row,0,0);
  assert(row[0]==0xaa&&row[26]==1&&row[25]==2&&row[7]==2&&row[6]==3&&row[5]==4);
  memset(row,0xaa,sizeof(row));g.direction=BM_ROAD_LEFT;g.segment_remaining=2;
  g.centre=26;g.edge_countdown=255;bm_world_generate_row(&g,row,0,0);
  assert(g.centre==25&&row[25]==12&&row[24]==2&&row[6]==2&&row[5]==16);
  memset(row,0xaa,sizeof(row));g.direction=BM_ROAD_RIGHT;g.segment_remaining=2;
  g.centre=26;g.edge_countdown=255;bm_world_generate_row(&g,row,0,0);
  assert(g.centre==27&&row[27]==6&&row[26]==5&&row[8]==2&&row[7]==8&&row[6]==7);
  memset(row,0xaa,sizeof(row));g.direction=BM_ROAD_STRAIGHT;g.segment_remaining=1;
  g.centre=26;bm_world_generate_row(&g,row,test_random_byte,&random);
  assert(random.index==2&&g.direction==BM_ROAD_STRAIGHT&&g.segment_remaining==1);
}

static void test_world_buffer(void){
  BmWorldBuffer a,b;uint16_t old_lms[BM_ATARI_PLAYFIELD_FETCH_ROWS];
  uint16_t old_top,old_bottom;unsigned step,row;int saw_area=0;
  bm_world_buffer_init(&a,1234);bm_world_buffer_init(&b,1234);
  assert(a.fine_phase==3&&a.source_column==0);
  /* EXXON.0 START: SRT loop with the river kept inside 16..75. */
  assert(a.world_mode==BM_WORLD_SRT&&a.area_type==0);
  assert(a.generator.left_limit==16&&a.generator.right_limit==75);
  assert(memcmp(&a,&b,sizeof(a))==0);
  memcpy(old_lms,a.lms,sizeof(old_lms));old_top=a.top;old_bottom=a.bottom_memory;
  for(step=0;step<4u*(BM_ATARI_SCROLL?2u:1u);step++)bm_world_buffer_step(&a);
  assert(a.fine_phase==3&&a.source_column==0);
  assert(a.lms[0]==old_top+1u);
  for(row=1;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;row++)
    assert(a.lms[row]==old_lms[row-1]+1u);
  assert(a.top==old_bottom-23u);
  assert(a.bottom_memory==old_lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]);
  for(row=0;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;row++)
    assert(memcmp(a.rows[row],a.arena+a.lms[row]+BM_VIEW_LMS_OFFSET,
                  BM_ATARI_PLAYFIELD_FETCH_COLUMNS)==0);
  for(step=0;step<4u*25u*(BM_ATARI_SCROLL?2u:1u);step++)bm_world_buffer_step(&a);
  assert(a.source_column==0); /* no periodic coarse reset/jump */
  for(step=0;step<4u*900u*(BM_ATARI_SCROLL?2u:1u);step++){
    bm_world_buffer_step(&a);
    /* DRUN steers the river away from a new runway through $610. */
    if(a.generator.turn_preference==255u||a.generator.turn_preference==0x40u)
      saw_area=1;
    assert(a.world_mode==BM_WORLD_SRT);
    assert(a.generator.centre>=16u&&a.generator.centre<=75u);
    for(row=0;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;row++){
      assert((size_t)a.lms[row]+BM_VIEW_LMS_OFFSET<=
             sizeof(a.arena)-BM_ATARI_PLAYFIELD_FETCH_COLUMNS);
      assert(memcmp(a.rows[row],a.arena+a.lms[row]+BM_VIEW_LMS_OFFSET,
                    BM_ATARI_PLAYFIELD_FETCH_COLUMNS)==0);
    }
  }
  assert(saw_area);
}

static void test_first_area_transition(void){
  BmWorldBuffer world;bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.area_transition=0;world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.area_type==2&&world.area_delay==61);
}

static void test_first_airfield_exit_sequence(void){
  BmWorldBuffer world;bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.road_position=20;
  world.road_target=20;world.road_delay=255;world.building_delay=255;
  world.runway_delay=255;
  world.area_type=2;world.area_transition=0;world.area_delay=0;
  world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==1&&world.area_transition==1);
  assert(world.area_delay>=64&&world.area_delay<=127);
  assert(world.object_control==1&&world.colpf1==0x2a);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==2&&world.area_transition==2&&world.area_delay==24);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==3&&world.area_transition==3&&world.area_delay==13);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.road_target==72&&world.area_transition==4);
}

static void test_classic_continues_after_bonus(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.road_position=71;
  world.road_target=72;world.road_delay=255;world.building_delay=255;
  world.runway_delay=255;world.area_type=3;world.area_transition=4;
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  /* XC (EXXON.8): $B7=0, 709=148, $FE=74, $638=(RTCLOK|32)&63, JMP SRT.
     The river limits keep the CITY values 1/90. */
  assert(world.world_mode==BM_WORLD_SRT&&world.area_type==0u);
  assert(world.colpf1==148u&&world.bridge_delay==74u);
  assert(world.normal_runway_delay>=32u&&world.normal_runway_delay<=63u);
  assert(world.generator.left_limit==1u&&world.generator.right_limit==90u);
#endif
}

static void test_classic_normal_bridge_rows(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;uint16_t draw_base;
  bm_world_buffer_init(&world,1234);
  world.area_progress=1u;world.area_type=0u;
  world.generator.centre=40u;
  world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10u;
  world.bridge_delay=0u;world.bridge_phase=8u;world.fine_phase=0u;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.bridge_phase==7u);
  assert(world.generator.segment_remaining==8u);
  world.bridge_delay=0u;world.bridge_phase=6u;world.fine_phase=0u;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.bridge_phase==5u);
  assert(world.arena[draw_base+42u]==0xb1u);
  assert(world.arena[draw_base+41u]==0x48u);
  assert(world.arena[draw_base+18u]==0xb1u);
  world.bridge_phase=4u;world.fine_phase=0u;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.bridge_phase==3u);
  assert(world.arena[draw_base+40u]==252u);
  assert(world.arena[draw_base+39u]==219u);
  assert(world.arena[draw_base+20u]==219u);
#endif
}

static void test_classic_normal_runway_rows(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;uint16_t draw_base;
  bm_world_buffer_init(&world,1234);
  world.area_progress=1u;world.area_type=0u;
  world.generator.centre=40u;
  world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10u;
  world.bridge_delay=30u;world.bridge_phase=0u;
  world.normal_runway_position=45u;world.normal_runway_delay=0u;
  world.normal_runway_phase=24u;world.fine_phase=0u;world.scroll_clock=1;
  /* No pending DRTR lower halves over the hangar columns. */
  memset(world.tree_positions,0,sizeof(world.tree_positions));
  world.hangar_target_available=0u;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.normal_runway_phase==23u);
  assert(world.hangar_target_available==2u);
  assert(world.arena[draw_base+34u]==0x9au);
  assert(world.arena[draw_base+33u]==0x95u);
  assert(world.arena[draw_base+31u]==0x7eu);
  assert(world.arena[draw_base+30u]==0x95u);
  assert(world.arena[draw_base+40u]==252u);
  world.normal_runway_phase=5u;world.fine_phase=0u;world.scroll_clock=1;
  memset(world.tree_positions,0,sizeof(world.tree_positions));
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.normal_runway_phase==4u);
  assert(world.arena[draw_base+45u]==145u);
  assert(world.arena[draw_base+44u]==148u);
  assert(world.arena[draw_base+36u]==148u);
  assert(world.arena[draw_base+35u]==147u);
#endif
}

static void test_classic_normal_runway_keeps_river_clear(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;
  unsigned centre;
  /* DRUN/MD: below 40 the river is steered left ($610=255) and a runway
     starts only for FF<30; MA then places it at 44+RANDOM&7. */
  for(centre=27;centre<=30;++centre){
    bm_world_buffer_init(&world,1234u);
    world.generator.centre=(uint8_t)centre;
    world.generator.direction=BM_ROAD_STRAIGHT;world.generator.segment_remaining=10;
    world.bridge_delay=30;world.bridge_phase=0;world.moored_ship_column=0;
    world.moored_ship_delay=200;
    world.object_slot=0;world.normal_runway_phase=0;world.normal_runway_delay=0;
    world.fine_phase=0;world.scroll_clock=1;
    bm_world_buffer_step(&world);
    /* LY: $FE=100 keeps bridges away, $61D=120 boats. */
    assert(world.bridge_delay==100u&&world.moored_ship_delay==120u);
    assert(world.object_slot==255u&&world.runway_slot==0u);
    assert(world.runway_marker==0x32u);
    if(centre==30)assert(world.normal_runway_phase==0);
    else{
      assert(world.normal_runway_phase==45);
      assert(world.normal_runway_position>=44u&&world.normal_runway_position<=51u);
    }
  }
  /* From 40 onwards the river is steered right ($610=$40) until FF>=65. */
  bm_world_buffer_init(&world,1234u);
  world.generator.centre=50u;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.bridge_delay=30;world.object_slot=0;
  world.moored_ship_column=0;world.normal_runway_delay=0;world.normal_runway_phase=0;
  world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.normal_runway_phase==0&&world.generator.turn_preference==0x40u);
  /* A busy $FD slot (e.g. an aircraft) postpones the runway. */
  bm_world_buffer_init(&world,1234u);
  world.generator.centre=20u;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.bridge_delay=30;world.object_slot=2;
  world.enemy_state=2;world.moored_ship_column=0;world.normal_runway_delay=0;
  world.normal_runway_phase=0;world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.normal_runway_phase==0&&world.runway_slot!=0u);
#endif
}

static void test_classic_normal_ship_position_and_motion(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;BmGame game;
  /* T27 DRBT ($1944): the moored boat is gun 1 (A2 with FF+2, $BF=$BC)
     and FBTGN fires it like FTNKGN: CKFGN only, no FBRGN HPOS limits. */
  { static BmWorldBuffer boat;
    bm_world_buffer_init(&boat,1234);
    boat.gun1_kind=0xbcu;boat.gun1_origin_x=boat.gun1_x=190u;
    boat.gun1_y=70u;boat.player_x_atari=80u;boat.fine_phase=3;
    bm_world_buffer_step(&boat);
    assert(boat.gun1_shot_y==70u&&boat.gun1_shot_x==190u&&
           boat.gun1_shot_direction==1u);
  }
  { unsigned fast,t;
    for(fast=0;fast<2;++fast){
      int moves=0;
      bm_world_buffer_init(&world,1234u);
      world.object_slot=15u;world.enemy_block=1u;
      world.ship_active=1;world.ship_x=100;world.ship_y=100;
      world.ship_fast=(uint8_t)fast;
      /* I7/J0: even VBIs only; slow skips the coarse frame, fast moves
         twice on it. */
      for(t=0;t<16;++t){
        bm_world_buffer_step(&world);
        if(world.scroll_event)
          moves+=world.coarse_event?(fast?2:0):1;
      }
#if BM_ATARI_SCROLL
      assert(moves==(fast?10:6));
#endif
      assert(world.ship_x==100-moves&&world.ship_y==100+2*moves);
      /* XSHP: the wreck moves once per even VBI. */
      world.ship_destroyed=1;world.ship_destroy_timer=120;moves=0;
      { uint8_t x=world.ship_x;
        for(t=0;t<16;++t){
          bm_world_buffer_step(&world);
          if(world.scroll_event)++moves;
        }
        assert(moves==(BM_ATARI_SCROLL?8:16)&&world.ship_x==(uint8_t)(x-moves));
      }
    }
  }
  /* I3: RANDOM&15==0, 29<=FF<60, 12<=$FE<25 -> C0=(FF-7)*4+27, C1=15. */
  { unsigned t;
    bm_world_buffer_init(&world,1234);
    world.generator.centre=40u;world.generator.direction=BM_ROAD_STRAIGHT;
    world.generator.segment_remaining=255u;
    world.object_slot=0u;world.enemy_block=1u;
    world.normal_runway_delay=200u;world.target_state=252u;
    for(t=0;t<400&&!world.ship_active;++t){
      world.bridge_delay=20u;world.generator.centre=40u;
      world.fine_phase=3u;bm_world_buffer_step(&world);
    }
    assert(world.ship_active&&world.object_slot==15u);
    assert(world.ship_x==159u&&world.ship_y==15u&&world.p2_hits==3u);
  }
  bm_game_init(&game,1234);
  memset(game.classic_world.rows,0,sizeof(game.classic_world.rows));
  game.classic_world.ship_active=1u;
  game.classic_world.ship_x=123u;game.classic_world.ship_y=31u; /* top y=0 */
  bm_game_render(&game);
  assert(game.framebuffer.pixels[1u*BM_SCREEN_WIDTH+160u]==29u);
  assert(game.framebuffer.pixels[2u*BM_SCREEN_WIDTH+160u]==30u);
  /* FSHGN fires missile 2 (gun 2's shot) before the ship moves: C1=70 ->
     $697=C0+2, Y=C1-7; FM2GN then moves it once (2 lines up, J3 dir 0). */
  world.ship_active=1u;world.ship_destroyed=0u;world.object_slot=15u;
  world.ship_y=70u;world.ship_x=123u;world.gun2_shot_y=0u;
  world.player_x_atari=119u;
  bm_world_buffer_step(&world);
  assert(world.gun2_x==125u&&world.gun2_shot_y==61u&&
         world.gun2_shot_x==124u&&world.gun2_shot_travel>=14u);
  bm_world_buffer_step(&world);
  assert(world.ship_destroyed==0u); /* No automatic diagnostic destruction. */
  game.classic_world.ship_active=1u;
  game.classic_world.ship_destroyed=1u;
  game.classic_world.ship_destroy_timer=120u;
  game.classic_world.ship_x=123u;game.classic_world.ship_y=32u; /* XSHPIM top 0 */
  bm_game_render(&game);
  assert(game.framebuffer.pixels[0u*BM_SCREEN_WIDTH+150u]==31u);
#endif
}

static void test_classic_river_car_spawn_and_motion(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;unsigned seed,armed=0,skipped=0;
  /* DRBR/JH: with $FD free, RANDOM&7!=0 arms DRP23 ($FD=1, $640=$62A=6). */
  for(seed=1;seed<200&&!(armed&&skipped);++seed){
    bm_world_buffer_init(&world,seed);
    world.generator.centre=40u;world.generator.direction=BM_ROAD_STRAIGHT;
    world.generator.segment_remaining=20u;world.bridge_delay=0u;
    world.bridge_phase=4u;world.object_slot=0u;world.enemy_block=1u;
    world.fine_phase=0u;world.scroll_clock=1u;
    bm_world_buffer_step(&world);
    assert(world.bridge_phase==3u);
    if(world.object_slot==1u){armed=1;assert(world.river_car_armed);}
    else{skipped=1;assert(!world.river_car_armed);}
  }
  assert(armed&&skipped);
  { unsigned left;
    for(left=0;left<2;++left){
      unsigned events=0u,guard=0u;
      for(seed=1;;++seed){
        bm_world_buffer_init(&world,seed);
        world.object_slot=1u;world.river_car_armed=1u;world.river_car_delay=6u;
        world.enemy_block=1u;world.target_state=252u;
        while(!world.river_car_active&&guard++<64u){
          bm_world_buffer_step(&world);
          if(world.scroll_event)++events;
        }
        if(world.river_car_left==left)break;
        events=0u;guard=0u;
      }
      /* IO waits for six SCROL updates; IS loads C1=12. */
      assert(events==6u&&world.river_car_y==12u);
      if(left)assert(world.river_car_x>=210u&&world.river_car_x<=241u);
      else assert(world.river_car_x>=91u&&world.river_car_x<=122u);
      { uint8_t x=world.river_car_x,y=world.river_car_y;unsigned t,even=0;
        for(t=0;t<8;++t){
          bm_world_buffer_step(&world);
          if(world.scroll_event)++even;
        }
        /* JE: one colour clock left per update; JN: +2 lines per even VBI. */
        assert(world.river_car_x==(uint8_t)(left?x-8u:x));
        assert(world.river_car_y==y+2u*even);
      }
      world.river_car_y=194u;bm_world_buffer_step(&world);
      assert(!world.river_car_active&&!world.object_slot);
    }
  }
  {BmGame game;bm_game_init(&game,1234);
   memset(game.classic_world.rows,0,sizeof(game.classic_world.rows));
   game.classic_world.river_car_active=1u;
   game.classic_world.river_car_left=1u;
   game.classic_world.river_car_x=66u;
   game.classic_world.river_car_y=124u; /* body top y=104 */
   bm_game_render(&game);
   assert(game.framebuffer.pixels[104u*BM_SCREEN_WIDTH+40u]==29u);}
#endif
}

#if BM_CLASSIC_TEST_PAGE
static void fighter_world(BmWorldBuffer *world,uint8_t slot,uint8_t state){
  bm_world_buffer_init(world,1234u);
  world->object_slot=slot;world->enemy_state=state;world->enemy_slow=0u;
  world->enemy_fighter_turn=0u;world->target_state=252u;
  world->enemy_fire_chance=0u;
}
#endif

static void test_runway_flash(void){
  /* T28 FLSH: with all objectives ($6AC>=$6C7) every DRUN runway row
     blinks the landing marker on bit 1 of $6B7; with $6C2=7 the status
     line shows COLBK 196 while the marker is blank. */
  BmWorldBuffer world;unsigned i,blank=0,shown=0,green=0;
  bm_world_buffer_init(&world,1234);
  world.objectives_destroyed=9u;world.objective_target=9u;
  world.capital_destroyed=7u;world.runway_marker=0x32u;
  world.normal_runway_delay=0u;world.normal_runway_phase=40u;
  world.normal_runway_position=44u;
  for(i=0;i<8u;++i){
    world.fine_phase=0u;world.scroll_clock=1u;world.status_bk=0u;
    bm_world_buffer_step(&world);
    if(world.marker_blank){++blank;if(world.status_bk==196u)++green;}
    else ++shown;
  }
  assert(blank==4u&&shown==4u&&green==4u);
  world.objectives_destroyed=8u;world.marker_blank=0u;
  for(i=0;i<8u;++i){
    world.fine_phase=0u;world.scroll_clock=1u;bm_world_buffer_step(&world);
    assert(!world.marker_blank);
  }
}

static void test_shbt_ground_targets(void){
  /* T19 SHBT: ADA maps HPOS 120 / scanline 100 to column ((120-26)>>2)-2=21
     of display row ((100-8)>>3)-1=10. */
  BmWorldBuffer world;unsigned base;
  bm_world_buffer_init(&world,1234u);
  base=world.lms[10]+world.source_column+21u;
  world.arena[base]=0x02u;world.grass_impact_ticks=0u;
  assert(bm_world_shot_ground(&world,120u,100u)==0u&&!world.grass_impact_ticks);
  world.arena[base]=198u;                       /* tank, left half: S10 */
  assert(bm_world_shot_ground(&world,120u,100u)==10u);
  assert(world.impact_mode==1u&&world.grass_impact_offset==base&&
         world.arena[base]==0x40u&&
         world.grass_impact_ticks==(uint8_t)(0xbbu-130u));
  world.arena[base]=237u;                       /* car, right half: DEY */
  assert(bm_world_shot_ground(&world,120u,100u)==10u&&
         world.grass_impact_offset==base-1u);
  world.gun1_origin_x=40u;world.gun2_origin_x=40u;
  world.arena[base]=177u;                       /* GUNC: $661=$698=0 */
  assert(bm_world_shot_ground(&world,120u,100u)==10u&&
         !world.gun1_origin_x&&!world.gun2_origin_x);
  world.boat_hits=2u;world.gun1_origin_x=40u;world.arena[base]=189u;
  assert(bm_world_shot_ground(&world,120u,100u)==0u&&world.boat_hits==1u&&
         world.gun1_origin_x);                  /* $6DE: first hit only counts */
  assert(bm_world_shot_ground(&world,120u,100u)==50u&&!world.gun1_origin_x&&
         world.impact_mode==7u&&world.grass_impact_offset==base-1u&&
         world.impact_other_offset==world.lms[9]+world.source_column+20u);
}

static void test_capital_target(void){
  /* T18b CPTL: area 6, $B8=63 spawns the $6C2 bit-1 target (colour 50). */
  BmWorldBuffer world;int x,y,found=0;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.area_type=6u;world.area_delay=63u;world.capital_destroyed=1u;
  world.fine_phase=3u;world.scroll_clock=1u;bm_world_buffer_step(&world);
  assert(!world.capital_y);                     /* already destroyed */
  world.capital_destroyed=0u;world.area_delay=63u;
  world.fine_phase=3u;world.scroll_clock=1u;bm_world_buffer_step(&world);
  assert(world.capital_y==20u&&world.capital_x==176u&&world.p2_colour==50u&&
         world.object_slot==20u&&world.capital_size==1u);
  /* Odd $604: two lines down, one colour clock left. */
  world.vbi_counter=2u;world.scroll_clock=0u;world.fine_phase=3u;
  bm_world_buffer_step(&world);
  assert(world.capital_y==22u&&world.capital_x==175u);
  world.capital_y=100u;
  for(y=0;y<BM_SCREEN_HEIGHT&&!found;++y)for(x=0;x<BM_SCREEN_WIDTH;++x)
    if(bm_world_capital_covers(&world,x,y)==2u){found=1;break;}
  assert(found&&bm_world_capital_covers(&world,bm_pm_x(175),bm_pm_y(100-6))==2u);
  /* Z3 in the capital: AFB zone 1 explodes only on roof 96 (mode 15, S30);
     elsewhere no explosion; zone 0 on the street $FE leaves a crater. */
  { int tx=((bm_pm_hpos(100)-26)>>2)-2-BM_VIEW_LMS_OFFSET;
    int ty=((bm_pm_scanline(80)-8)>>3)-1;
    world.capital_y=0u;world.grass_impact_ticks=0u;
    world.bomb_zone=1u;world.rows[ty][tx]=96u;
    assert(bm_world_bomb_impact(&world,100,80)==1u&&world.impact_mode==15u&&
           world.bomb_impact_handled&&world.grass_impact_ticks);
    world.grass_impact_ticks=0u;world.rows[ty][tx]=0xfeu;
    assert(bm_world_bomb_impact(&world,100,80)==0u&&world.bomb_impact_handled&&
           !world.grass_impact_ticks);
    world.bomb_zone=0u;
    assert(bm_world_bomb_impact(&world,100,80)==0u&&world.impact_mode==10u&&
           world.grass_impact_ticks);
  }
}

static void test_capital_builders(void){
  /* T18a BLDGC/BLDGL: WP arms both sides with delay 1; the first slice of
     each first building ($BA=0 -> END, width 6) starts at column 61 (right)
     or 24 (left) with 98+$BD, then four 100+$BC and six more. */
  BmWorldBuffer world;uint16_t draw_base;unsigned i;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.area_progress=1;world.area_type=4;
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;
  world.capital[0].active=world.capital[0].delay=1u;
  world.capital[1].active=world.capital[1].delay=1u;
  bm_world_buffer_step(&world);                 /* -> area 5, delays used */
  assert(world.area_type==5u&&world.capital[0].phase==0u);
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  for(i=0;i<2u;++i){
    const BmCapitalSide *s=&world.capital[i];
    unsigned start=i?24u:61u,c;
    assert(s->phase==6u&&s->row==1u&&s->width==6u&&s->cursor==start);
    assert(world.arena[draw_base+start]==(uint8_t)(98u+s->bd));
    for(c=1;c<=10u;++c)
      assert(world.arena[draw_base+start-c]==(uint8_t)(100u+s->bc));
  }
  /* CTYGN: area 6 with $B8=13 places the GUNC tile and A2 at column 32. */
  world.area_type=6u;world.area_delay=14u;world.gun1_origin_x=0u;
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.area_delay==13u&&world.arena[draw_base+25u]==49u&&
         world.gun1_origin_x==128u&&world.gun1_y==45u);
}

static void test_classic_land_freezes_objects(void){
#if BM_CLASSIC_TEST_PAGE
  /* T12: the LAND loop runs DRAW/DRUN and COMMON but none of PLBCK/PLFWD,
     SHIP or DRP23, so a warning fighter and a river car stand still. */
  BmWorldBuffer world;unsigned t;
  fighter_world(&world,2u,2u);world.enemy_warn=23u;world.landing=1u;
  for(t=0;t<200u;++t)bm_world_buffer_step(&world);
  assert(world.object_slot==2u&&world.enemy_state==2u&&world.enemy_warn==23u);
  world.landing=0u;
  for(t=0;t<200u&&world.enemy_warn==23u;++t)bm_world_buffer_step(&world);
  assert(world.enemy_warn<23u||world.enemy_state!=2u);
  fighter_world(&world,1u,0u);world.river_car_armed=1u;world.river_car_delay=5u;
  world.landing=1u;
  for(t=0;t<200u;++t)bm_world_buffer_step(&world);
  assert(world.river_car_armed&&world.river_car_delay==5u);
  /* T14 MJ: no COMMON either, so gun shots and TRGT stand still too. */
  fighter_world(&world,2u,2u);world.enemy_warn=23u;
  world.landing=BM_WORLD_ROLLOUT;world.target_state=5u;
  world.gun1_shot_x=120u;world.gun1_shot_y=100u;
  world.gun1_shot_direction=2u;world.gun1_shot_travel=10u;
  for(t=0;t<200u;++t)bm_world_buffer_step(&world);
  assert(world.enemy_warn==23u&&world.target_state==5u&&
         world.gun1_shot_x==120u&&world.gun1_shot_y==100u);
#endif
}

static void test_classic_enemy_fighter_entry(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;BmGame game;unsigned seed,back=0,forward=0;
  /* OB: RANDOM&31==0 claims the free slot, a second bit picks PLBCK
     ($FD=2, $640=2) or PLFWD (KV: $FD=3, $640=7); both warn 23 rows. */
  for(seed=1;seed<4000&&!(back&&forward);++seed){
    bm_world_buffer_init(&world,seed);
    world.object_slot=0u;world.fine_phase=3u;
    bm_world_buffer_step(&world);
    if(world.object_slot==2u){back=1;assert(world.enemy_state==2u&&world.enemy_warn==23u);}
    if(world.object_slot==3u){forward=1;assert(world.enemy_state==7u&&world.enemy_warn==23u);}
  }
  assert(back&&forward);
  /* OD counts $636 per coarse row, then OG spawns at C1=C2=186. */
  fighter_world(&world,2u,2u);world.enemy_warn=1u;
  while(world.enemy_state==2u)bm_world_buffer_step(&world);
  assert(world.enemy_state==3u&&world.coarse_event);
  bm_world_buffer_step(&world);
  assert(world.enemy_state==4u&&world.enemy_fighter_active);
  assert(world.enemy_fighter_y==186u&&world.enemy_fighter_shadow_y==186u);
  assert(world.enemy_fighter_x<64u&&world.enemy_body_on&&!world.enemy_shadow_on);
  assert(world.enemy_timer>=30u);
  /* OW: body climbs one line and C0+1 per update; P3 shadow waits. */
  { uint8_t x=world.enemy_fighter_x;
    bm_world_buffer_step(&world);
    assert(world.enemy_fighter_y==185u&&world.enemy_fighter_x==x+1u);
    assert(world.enemy_fighter_shadow_y==186u);
  }
  world.enemy_timer=1u;bm_world_buffer_step(&world);
  assert(world.enemy_state==5u&&world.enemy_shadow_on);
  /* OJ: body, shadow and C0 move together; BTURN state 1 freezes C0,
     state 2 freezes C1 and C2. */
  world.enemy_fighter_x=100u;world.enemy_fighter_y=150u;
  world.enemy_fighter_shadow_y=170u;
  bm_world_buffer_step(&world);
  assert(world.enemy_fighter_x==101u&&world.enemy_fighter_y==149u&&
         world.enemy_fighter_shadow_y==169u);
  world.enemy_fighter_turn=1u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_x==101u&&world.enemy_fighter_y==148u&&
         world.enemy_fighter_shadow_y==168u);
  world.enemy_fighter_turn=2u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_x==102u&&world.enemy_fighter_y==148u&&
         world.enemy_fighter_shadow_y==168u);
  /* TRIGB: state 5, C2>=F6, C0<170 and RANDOM<$6CA -> C0+6/C1+2. */
  world.enemy_fighter_turn=1u;world.player_y_atari=160u;
  world.enemy_fire_chance=255u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_shot_active&&!world.enemy_fighter_shot_forward);
  assert(world.enemy_fighter_shot_x==108u&&world.enemy_fighter_shot_y==150u);
  world.enemy_fire_chance=0u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_shot_x==111u&&world.enemy_fighter_shot_y==147u);
  /* OR: the slot is free once the shadow reaches C2<12. */
  world.enemy_fighter_shadow_y=12u;bm_world_buffer_step(&world);
  assert(world.object_slot==2u);
  bm_world_buffer_step(&world);
  assert(world.object_slot==0u&&!world.enemy_fighter_active);
  /* PLFWD: KD at C0=171..202, C1=C2=15 with only the SHDW1 layer. */
  fighter_world(&world,3u,8u);
  bm_world_buffer_step(&world);
  assert(world.enemy_state==9u&&world.enemy_fighter_forward);
  assert(world.enemy_fighter_x>=171u&&world.enemy_fighter_x<=202u);
  assert(world.enemy_fighter_y==15u&&world.enemy_fighter_shadow_y==15u);
  assert(!world.enemy_body_on&&world.enemy_shadow_on);
  world.enemy_timer=1u;bm_world_buffer_step(&world);
  assert(world.enemy_state==10u&&world.enemy_body_on);
  /* LF straight: (-1,+2,+2); FTL (state 1): (0,+2,+2); FTR: (-1,+1,+1). */
  world.enemy_fighter_x=150u;world.enemy_fighter_y=20u;
  world.enemy_fighter_shadow_y=20u;world.player_y_atari=10u;
  bm_world_buffer_step(&world);
  assert(world.enemy_fighter_x==149u&&world.enemy_fighter_y==22u&&
         world.enemy_fighter_shadow_y==22u);
  world.enemy_fighter_turn=1u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_x==149u&&world.enemy_fighter_y==24u&&
         world.enemy_fighter_shadow_y==24u);
  world.enemy_fighter_turn=2u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_x==148u&&world.enemy_fighter_y==25u&&
         world.enemy_fighter_shadow_y==25u);
  /* TRIGF: C2<F6 -> C0-3/C1+12, then (-3,+4) per update. */
  world.enemy_fighter_turn=1u;world.player_y_atari=168u;
  world.enemy_fire_chance=255u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_shot_active&&world.enemy_fighter_shot_forward);
  assert(world.enemy_fighter_shot_x==145u&&world.enemy_fighter_shot_y==37u);
  world.enemy_fire_chance=0u;bm_world_buffer_step(&world);
  assert(world.enemy_fighter_shot_x==142u&&world.enemy_fighter_shot_y==41u);
  /* LF/UZ releases the slot at C1>=186. */
  world.enemy_fighter_y=186u;bm_world_buffer_step(&world);
  assert(!world.object_slot&&!world.enemy_fighter_active);
  /* Render: PLBCK body at P2+C1. */
  bm_game_init(&game,1234u);
  game.classic_world.object_slot=2u;game.classic_world.enemy_state=6u;
  game.classic_world.enemy_fighter_active=1u;game.classic_world.enemy_body_on=1u;
  game.classic_world.enemy_fighter_x=100u;
  game.classic_world.enemy_fighter_y=37u; /* P2+C1: top y=17 */
  game.classic_world.enemy_fighter_shadow_y=55u;
  game.classic_world.p2_colour=44u;
  bm_game_render(&game);
  /* T16: the body takes COLPM2 (706), here the normal $2C. */
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[44u]);
  assert(game.framebuffer.pixels[17u*BM_SCREEN_WIDTH+104u]==29u);
  /* TRGTP2: a bonus $7C/$A8 fighter alternates on bit 3 of $604. */
  game.classic_world.p2_colour=0x7cu;game.classic_world.vbi_counter=8u;
  bm_game_render(&game);
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[0x7cu]);
  game.classic_world.vbi_counter=7u;bm_game_render(&game);
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[0xa8u]);
  /* A bonus ship (50) does not flash. */
  game.classic_world.p2_colour=50u;bm_game_render(&game);
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[50u]);
  /* XP2: the wreck flashes 44/54 on bit 2 of $66F, then stays 50. */
  game.classic_world.enemy_fighter_destroyed=1u;
  game.classic_world.enemy_fighter_destroy_timer=36u;bm_game_render(&game);
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[44u]);
  game.classic_world.enemy_fighter_destroy_timer=32u;bm_game_render(&game);
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[54u]);
  game.classic_world.enemy_fighter_destroy_timer=29u;bm_game_render(&game);
  assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[50u]);
  /* COLM02: in LINE a single bullet on P2 destroys a PLBCK for S100. */
  { unsigned opaque=0u;
    bm_game_init(&game,1234u);
    fighter_world(&game.classic_world,2u,5u);
    game.classic_world.enemy_fighter_active=1u;game.classic_world.enemy_body_on=1u;
    game.classic_world.enemy_shadow_on=1u;game.classic_world.p2_colour=44u;
    game.classic_world.enemy_fighter_x=100u;game.classic_world.enemy_fighter_y=101u;
    game.classic_world.enemy_fighter_shadow_y=128u;
    game.player.y=126;
    while(opaque<160u&&bm_asset_fighter_back[opaque]!=10u)++opaque;
    assert(opaque<160u);
    /* COLM02 uses the collision latch and LINE state of the previous
       frame, i.e. the body before this update's OL step. */
    game.classic_world.enemy_in_line=1u;
    game.player_shot_active=1u;
    game.player_shot_x=(int16_t)(bm_pm_x(100)+(int)(opaque%16u));
    game.player_shot_y=(int16_t)(bm_pm_y(101)+(int)(opaque/16u));
    bm_game_tick(&game,&(BmInput){0});
    assert(!game.player_shot_active&&game.classic_world.enemy_fighter_destroyed);
    assert(game.classic_world.enemy_fighter_destroy_timer==99u); /* $66F=100, XP2 */
    assert(game.score==100u&&!game.classic_world.enemy_shadow_on);
  }
#endif
}

static void test_target_sight(void){
  BmWorldBuffer world;
  bm_world_buffer_init(&world,1234u);
  bm_world_target_sight_start(&world,2u,40u,17u);
  assert(world.target_sight_active&&world.target_sight_type==2u&&
         world.target_sight_x==160u&&world.target_sight_y==17u);
  world.scroll_clock=1u;
  bm_world_buffer_step(&world);
  assert(world.target_sight_x==159u&&world.target_sight_y==19u);
  world.target_sight_y=190u;world.scroll_clock=1u;
  bm_world_buffer_step(&world);
  assert(!world.target_sight_active&&!world.target_sight_type);

#if BM_CLASSIC_TEST_PAGE
  {
    BmGame game;
    bm_game_init(&game,1234u);
    bm_world_target_sight_start(&game.classic_world,3u,39u,118u);
    bm_game_render(&game);
    /* HPOS 156 -> x216; GNSGHT ends the nine GNST rows at Y=118, so the
       top is scanline 110, framebuffer y 90, for every objective type. */
    assert(game.framebuffer.pixels[90u*BM_SCREEN_WIDTH+216u]==29u);
    /* O5: COLPM2 follows bit 2 of $C0 (HPOS 156: 50). */
    assert(game.framebuffer.palette[29u]==bm_atari_pal_rgb[50u]);
    bm_world_target_sight_start(&game.classic_world,1u,39u,118u);
    bm_game_render(&game);
    assert(game.framebuffer.pixels[90u*BM_SCREEN_WIDTH+216u]==29u);
    bm_world_target_sight_start(&game.classic_world,2u,39u,118u);
    bm_game_render(&game);
    assert(game.framebuffer.pixels[90u*BM_SCREEN_WIDTH+216u]==29u);
  }
  /* A pending $C4==5 request is consumed only by an actual generator. */
  bm_world_buffer_init(&world,1234u);
  bm_world_target_sight_request(&world);
  world.area_progress=1u;world.area_type=0u;
  world.bridge_delay=0u;world.bridge_phase=1u;
  world.generator.centre=40u;world.generator.direction=BM_ROAD_STRAIGHT;
  world.fine_phase=0u;world.scroll_clock=1u;
  bm_world_buffer_step(&world);
  /* GL starts GNSGHT at (FF-1)*4/Y=22; TRGT/Z9 runs later in the same
     frame and, on this even VBI, already moves it once. */
  assert(world.bridge_phase==0u&&!world.target_sight_request&&
         world.target_sight_active&&world.target_sight_type==3u&&
         world.target_sight_x==155u&&world.target_sight_y==24u);
  assert(world.target_state==253u&&world.object_slot==6u);
#endif
}

static void test_classic_tree_rows(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;uint16_t base;uint8_t positions[6];unsigned i,active=0u;
  bm_world_buffer_init(&world,1234u);
  world.area_progress=1u;world.area_type=0u;world.fine_phase=0u;world.scroll_clock=1;
  world.generator.centre=40u;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=20u;
  /* Keep DRBR/DRTNK/TNK, which also write $B6, out of this row. */
  world.object_control=2u;world.bridge_phase=0u;world.bridge_delay=50u;
  world.normal_runway_phase=0u;world.normal_runway_delay=20u;
  world.tank_phase=0u;world.tank2_phase=0u;
  world.tank_delay=100u;world.tank2_delay=100u;
  world.moored_ship_delay=100u;world.moored_ship_column=0u;
  bm_world_buffer_step(&world);
  base=(uint16_t)(world.bottom_memory-20u);
  memcpy(positions,world.tree_positions,sizeof(positions));
  for(i=0;i<6u;++i)if(positions[i]){
    uint8_t top=world.arena[base+positions[i]];
    uint8_t bottom=world.arena[base+positions[i]+1u];
    assert(top==37u||top==38u);
    assert(bottom==39u||bottom==40u);
    ++active;
  }
  assert(active==2u);
  world.object_control=0u;world.fine_phase=0u;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  base=(uint16_t)(world.bottom_memory-20u);
  for(i=0;i<6u;++i)if(positions[i]){
    uint8_t left=world.arena[base+positions[i]-1u];
    uint8_t right=world.arena[base+positions[i]];
    assert(left==33u||left==34u);
    assert(right==35u||right==36u);
  }
#endif
}

static void test_classic_normal_tank_uses_river_centre(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;uint16_t draw_base;unsigned column;
  bm_world_buffer_init(&world,1234);
  world.area_progress=1u;world.area_type=0u;
  world.generator.centre=30u;
  world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10u;
  world.bridge_delay=30u;world.bridge_phase=0u;
  world.normal_runway_delay=30u;world.normal_runway_phase=0u;
  world.tank_delay=0u;world.tank_phase=3u;world.tank_variant=0u;
  world.fine_phase=0u;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.tank_phase==2u&&world.tank_column==35u);
  assert(world.arena[draw_base+34u]==177u);
  for(column=36u;column<=40u;++column)
    assert(world.arena[draw_base+column]==69u);
  assert(world.arena[draw_base+41u]==74u);
  assert(world.gun1_origin_x==164u&&world.gun1_y==45u);
#endif
}

static void test_classic_normal_dynamic_tank(void){
#if BM_CLASSIC_TEST_PAGE
  BmWorldBuffer world;uint16_t draw_base;unsigned column;
  bm_world_buffer_init(&world,1234);
  world.area_progress=1u;world.area_type=0u;
  world.generator.centre=30u;
  world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10u;
  world.bridge_delay=30u;world.bridge_phase=0u;
  world.normal_runway_delay=30u;world.normal_runway_phase=0u;
  world.tank_delay=255u;world.tank_phase=0u;
  world.tank2_delay=0u;world.tank2_phase=1u;world.fine_phase=0u;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.tank2_phase==0u);
  assert(world.tank2_delay>=16u&&world.tank2_delay<=47u);
  assert(world.arena[draw_base+33u]==49u ||
         world.arena[draw_base+33u]==177u);
  for(column=35u;column<=40u;++column)
    assert(world.arena[draw_base+column]==72u ||
           world.arena[draw_base+column]==198u ||
           world.arena[draw_base+column]==199u);
  assert(world.gun1_origin_x==160u&&world.gun1_y==45u);
#endif
}

static void test_progressed_area_sequence(void){
  BmWorldBuffer world;uint16_t draw_base;unsigned column;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.road_delay=255;
  world.building_delay=255;world.runway_delay=255;
  world.area_progress=1;world.area_transition=0;world.area_type=2;
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==3&&world.area_delay==13);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==4&&world.area_delay==24&&world.object_control==0);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==5&&world.area_delay==24);
  world.area_delay=1;world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  /* CLEAR fills $FE; BLDGL (from column 24) and BLDGC (from 61) add the
     first slices of their buildings, leaving the street between them. */
  { unsigned buildings=0u;
    for(column=1;column<BM_ATARI_WORLD_STRIDE;column++){
      uint8_t v=world.arena[draw_base+column];
      if(column>24u&&column<51u)assert(v==0xfe);
      if(v!=0xfe)++buildings;
    }
    assert(buildings>=10u);
  }
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==6&&world.area_delay>=64&&world.area_delay<=95);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==7&&world.area_delay==36);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==8&&world.area_delay==24);
  world.area_delay=0;world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.area_type==1&&world.area_transition==1);
}

static void test_sortie_progress_trigger(void){
  BmWorldBuffer world;unsigned seed,saw_city=0,saw_srt=0;
  bm_world_buffer_init(&world,1234);
  /* EXXON.0 START: SRT river loop, limits 16/75, $B7=0, $6CF=EAS0. */
  assert(world.world_mode==BM_WORLD_SRT&&world.area_type==0);
  assert(world.generator.left_limit==16&&world.generator.right_limit==75);
  assert(world.runway_interval==0xa5&&world.normal_runway_delay==130);
  /* Fewer than two objectives: always SRT, only $6CF+=7. */
  assert(bm_world_after_takeoff(&world,0,8)==BM_WORLD_SRT);
  assert(bm_world_after_takeoff(&world,1,8)==BM_WORLD_SRT);
  assert(world.area_progress==0&&world.enemy_tracking==0);
  assert(world.runway_interval==0xa5+14);
  /* All objectives: V0 (INC $6AE) and ZN into CITY with carry set. */
  assert(bm_world_after_takeoff(&world,8,8)==BM_WORLD_CITY);
  assert(world.area_progress==1&&world.area_type==1);
  assert(world.generator.left_limit==1&&world.generator.right_limit==90);
  assert(world.road_position==1&&world.road_target>=31&&world.road_target<=46);
  assert(world.road_delay==(world.road_target|16)&&world.runway_phase==36);
  assert(world.area_delay==255&&world.area_transition==0);
  assert(bm_world_after_takeoff(&world,9,8)==BM_WORLD_CITY);
  assert(world.area_progress==2&&world.area_type==2);
  /* $6CF stops growing at 220. */
  world.runway_interval=218;bm_world_after_takeoff(&world,0,8);
  assert(world.runway_interval==225);bm_world_after_takeoff(&world,0,8);
  assert(world.runway_interval==225);
  /* 2..target-1: INC $6A7, RANDOM<160 -> ZN, otherwise SRT. */
  for(seed=1;seed<64&&!(saw_city&&saw_srt);++seed){
    uint8_t mode;
    bm_world_buffer_init(&world,seed);
    mode=bm_world_after_takeoff(&world,2,8);
    assert(world.enemy_tracking==1&&world.area_progress==0);
    if(mode==BM_WORLD_CITY){
      saw_city=1;
      assert(world.area_type==1&&world.road_target>=30&&world.road_target<=45);
    }else{saw_srt=1;assert(world.area_type==0);}
    assert(world.world_mode==mode);
  }
  assert(saw_city&&saw_srt);
}

static void test_offscreen_area_skips_normal_road(void){
  BmWorldBuffer world;
  BmWorldGenerator generator;
  uint16_t draw_base;
  unsigned column;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=90;
  world.generator.direction=BM_ROAD_RIGHT;
  world.generator.segment_remaining=7;
  world.generator.edge_countdown=23;
  generator=world.generator;
  world.road_position=1;
  world.road_target=43;
  world.road_delay=5;
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  memset(world.arena+draw_base+1,0xa5,BM_ATARI_WORLD_STRIDE-1u);
  bm_world_buffer_step(&world);
  assert(memcmp(&world.generator,&generator,sizeof(generator))==0);
  for(column=1;column<BM_ATARI_WORLD_STRIDE;column++)
    assert(world.arena[draw_base+column]==0);
}

static void test_crossroad_overlay(void){
  BmWorldBuffer world;uint16_t draw_base;unsigned step;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.road_delay=0;
  world.road_position=10;world.road_target=20;world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.arena[draw_base+10]==0x48&&world.arena[draw_base+9]==0xcc);
  assert(world.arena[draw_base+4]==0xfd&&world.road_position==11);
  assert(world.colpf1==0x2a);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.road_position=20;
  world.road_target=20;world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  for(step=0;step<5;step++)
    assert(world.arena[draw_base+18-step]==((const uint8_t[]){0xf4,0xfc,0xfc,0xfc,0xcb})[step]);
}

static void test_road_car_overlay(void){
  BmWorldBuffer world;uint16_t draw_base;uint32_t seed;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=2;
  world.road_delay=0;world.road_position=40;world.road_target=40;
  world.building_delay=255;world.runway_delay=255;
  for(seed=1;;++seed)
    if((uint8_t)((seed*1664525u+1013904223u)>>24)%16u==0u)break;
  world.rng=seed;world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.arena[draw_base+36]==239u);
  assert(world.arena[draw_base+35]==238u);
}

static uint16_t next_draw_base(const BmWorldBuffer *world){
  return (uint16_t)(world->lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
}

static void tank_step(BmWorldBuffer *world,uint16_t *draw_base){
  world->fine_phase=0;world->scroll_clock=1;
  *draw_base=next_draw_base(world);
  bm_world_buffer_step(world);
}

static void test_tank_overlay(void){
  BmWorldBuffer world;uint16_t draw_base;unsigned index;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.area_delay=255;world.road_delay=255;
  world.road_position=40;world.road_target=40;
  world.building_delay=255;world.runway_delay=255;
  world.tank_delay=0;world.tank_phase=0;
  tank_step(&world,&draw_base);
  assert(world.tank_phase==8);

  /* TT phases 7..4 emit the random approach trail. */
  world.tank_phase=5;
  tank_step(&world,&draw_base);
  assert(world.tank_phase==4&&world.object_control==2);

  /* Phase 3 runs DFTNK without emitting a row. */
  world.tank_impact_phase=2u;world.hangar_impact_phase=1u;
  tank_step(&world,&draw_base);
  assert(world.tank_phase==3&&world.glyph_mode==BM_CHARSET_TANK);
  assert(world.tank_impact_phase==0u&&world.hangar_impact_phase==1u);
  assert(world.tank_charset_loaded==1);

  world.tank_variant=0;
  tank_step(&world,&draw_base);
  assert(world.tank_phase==2&&world.tank_column==45);
  assert(world.gun1_origin_x==204u&&world.gun1_x==204u);
  assert(world.gun1_y==45u&&world.gun1_start_delay==49u);
  assert(world.arena[draw_base+44]==177u);
  for(index=46;index<51;index++)assert(world.arena[draw_base+index]==69u);
  assert(world.arena[draw_base+51]==74u);

  tank_step(&world,&draw_base);
  assert(world.tank_phase==1);
  for(index=45;index<50;index++)assert(world.arena[draw_base+index]==69u);
  assert(world.arena[draw_base+50]==18u);

  tank_step(&world,&draw_base);
  assert(world.tank_phase==0);
  assert(world.arena[draw_base+44]==255u);
  for(index=45;index<49;index++)assert(world.arena[draw_base+index]==191u);
  assert(world.arena[draw_base+49]==73u);
  assert(world.tank_delay>=26&&world.tank_delay<=41);
}

static void test_secondary_tank_overlay(void){
  BmWorldBuffer world;uint16_t draw_base;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;world.area_delay=255;
  world.road_position=40;world.road_target=40;world.road_delay=255;
  world.runway_delay=255;world.building_delay=255;
  world.tank_delay=255;world.tank2_delay=0;world.tank2_phase=0;
  tank_step(&world,&draw_base);
  assert(world.tank2_phase==1&&world.object_marker==4);
  assert(world.object_control==0); /* .1: STA $B6 with A=$6A2=0 */
  tank_step(&world,&draw_base);
  assert(world.tank2_phase==0);
  assert(world.tank2_delay>=16&&world.tank2_delay<=47);
  assert(world.object_control==255);
  /* E4 with $669==$66A, F4<160 and (F6>>1)+F4>=120: random F5 (+10) or
     F3 (-8).  Three tile pairs run leftward from there; the gun follows
     one column further left and B5 arms A2 with the first column. */
  { unsigned start=world.gun1_x/4u,index;
    assert(start==50u||start==32u);
    assert(world.gun1_origin_x==world.gun1_x);
    assert(world.gun1_y==45u&&world.gun1_start_delay==49u);
    assert(world.arena[draw_base+start-7u]==49u||
           world.arena[draw_base+start-7u]==177u);
    for(index=start-5u;index<=start;index++)
      assert(world.arena[draw_base+index]==72u ||
             world.arena[draw_base+index]==198u ||
             world.arena[draw_base+index]==199u);
  }
}

static void test_secondary_tank_preserves_road(void){
  static const uint8_t expected[]={0xfdu,0xceu,0xfcu,0xfcu,
                                   0xcdu,0xccu,0x48u};
  BmWorldBuffer world;uint16_t draw_base;unsigned index;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;world.area_delay=255;
  world.road_position=40;world.road_target=20;world.road_delay=0;
  world.runway_delay=255;world.building_delay=255;world.tank_delay=255;
  world.tank2_delay=0;world.tank2_phase=1;world.player_x_atari=160;
  tank_step(&world,&draw_base);
  for(index=0;index<sizeof(expected);++index)
    assert(world.arena[draw_base+34u+index]==expected[index]);
}

static void test_gun_emplacement_overlay(void){
  BmWorldBuffer world,early;uint16_t draw_base;unsigned step;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=4;
  world.area_delay=255;world.road_position=40;world.road_target=40;
  world.road_delay=255;world.runway_delay=255;world.building_delay=255;
  world.tank_delay=255;world.tank2_delay=255;world.gun_delay=7;
  tank_step(&world,&draw_base);
  assert(world.gun_delay==30);
  assert(world.arena[draw_base+30]==0xf0u);
  assert(world.arena[draw_base+41]==0xf0u);
  assert(world.gun1_x==148u&&world.gun1_origin_x==148u);
  assert(world.gun2_x==192u&&world.gun2_origin_x==192u);
  assert(world.gun1_y==45u&&world.gun2_y==45u);
  assert(world.gun1_start_delay==49u&&world.gun2_start_delay==49u);
  world.area_type=3;
  for(step=0;step<49u;++step)bm_world_buffer_step(&world);
  assert(world.gun1_start_delay==0u&&world.gun2_start_delay==0u);
  bm_world_buffer_step(&world);
  assert(world.gun1_y==46u&&world.gun2_y==46u);
  assert(world.gun1_x==148u&&world.gun2_x==192u);
  bm_world_buffer_step(&world);
  assert(world.gun1_y==47u&&world.gun2_y==47u);
  assert(world.gun1_x==147u&&world.gun2_x==191u);

  bm_world_buffer_init(&early,1234);to_city(&early);
  early.generator.centre=1;early.generator.direction=BM_ROAD_STRAIGHT;
  early.generator.segment_remaining=10;early.area_type=4;early.area_delay=255;
  early.road_position=40;early.road_target=40;early.road_delay=255;
  early.runway_delay=255;early.building_delay=255;early.tank_delay=255;
  early.tank2_delay=255;early.gun_delay=19;
  tank_step(&early,&draw_base);
  assert(early.gun_delay==18u&&early.gun1_origin_x==148u);
  assert(early.gun2_origin_x==192u);
}

static void test_gun_projectile(void){
  BmWorldBuffer world,detached;
  unsigned step;
  bm_world_buffer_init(&world,1234);
  world.fine_phase=3;world.gun1_origin_x=100;world.gun1_x=100;
  world.gun1_y=70;world.gun1_start_delay=0;world.player_x_atari=80;
  world.rows[7][13]=0xb1u; /* HPOS 100/Y70 -> (104,50) */
  bm_world_buffer_step(&world);
  assert(world.gun1_shot_x==100u&&world.gun1_shot_y==70u);
  assert(world.gun1_shot_direction==1u);
  assert(world.gun1_shot_travel>=15u&&world.gun1_shot_travel<=30u);
  bm_world_buffer_step(&world);
  assert(world.gun1_shot_x==98u&&world.gun1_shot_y==68u);
  world.gun1_shot_travel=1u;
  bm_world_buffer_step(&world);
  assert(world.gun1_shot_y==68u&&world.gun1_shot_explosion==25u);
  for(step=0;step<24u;++step)bm_world_buffer_step(&world);
  assert(world.gun1_shot_y==68u&&world.gun1_shot_explosion==1u);
  bm_world_buffer_step(&world);
  assert(world.gun1_shot_y==0u&&world.gun1_shot_explosion==0u);

  /* T27: CKFGN/FBRGN, not a visible gun tile, decide.  No shot while the
     start delay runs, left of HPOS 65, or for the boat ($BF=$BC); from
     HPOS 185 TA first moves the gun 96 colour clocks left. */
  bm_world_buffer_init(&detached,1234);
  detached.fine_phase=3;detached.gun1_origin_x=100;detached.gun1_x=100;
  detached.gun1_y=70;detached.player_x_atari=80;detached.gun1_start_delay=5;
  bm_world_buffer_step(&detached);
  assert(detached.gun1_shot_y==0u);
  detached.gun1_start_delay=0;detached.gun1_x=60;
  bm_world_buffer_step(&detached);
  assert(detached.gun1_shot_y==0u);
  detached.gun1_x=190;bm_world_buffer_step(&detached);
  assert(detached.gun1_shot_y==71u&&detached.gun1_shot_x==94u);
  bm_world_buffer_init(&detached,1234);
  detached.fine_phase=3;detached.gun1_origin_x=100;detached.gun1_x=100;
  detached.gun1_y=70;detached.gun1_kind=0xbcu;detached.gun1_x=60;
  bm_world_buffer_step(&detached);
  assert(detached.gun1_shot_y==70u);              /* FBTGN: no HPOS limit */
}

static void test_gun_projectile_render(void){
  BmGame game;
  bm_game_init(&game,1234);
  game.classic_world.gun1_shot_x=100u;
  game.classic_world.gun1_shot_y=70u;
#if BM_CLASSIC_TEST_PAGE
  game.classic_world.area_type=4u;
  memset(game.classic_world.rows,0,sizeof(game.classic_world.rows));
#endif
  bm_game_render(&game);
#if BM_CLASSIC_TEST_PAGE
  /* CLEAR's screen code 0 is the PF0 (grass) glyph in every charset. */
  assert(game.framebuffer.pixels[80u*BM_SCREEN_WIDTH+160u]==17u);
  assert(game.framebuffer.pixels[50u*BM_SCREEN_WIDTH+104u]==28u);
  assert(game.framebuffer.pixels[51u*BM_SCREEN_WIDTH+107u]==28u);
#else
  assert(game.framebuffer.pixels[50u*BM_SCREEN_WIDTH+104u]==9u);
  assert(game.framebuffer.pixels[51u*BM_SCREEN_WIDTH+107u]==9u);
#endif

  game.classic_world.gun1_shot_explosion=25u;
  bm_game_render(&game);
#if BM_CLASSIC_TEST_PAGE
  assert(game.framebuffer.pixels[46u*BM_SCREEN_WIDTH+104u]==28u);
  assert(game.framebuffer.pixels[47u*BM_SCREEN_WIDTH+102u]==28u);
#else
  assert(game.framebuffer.pixels[46u*BM_SCREEN_WIDTH+104u]==9u);
  assert(game.framebuffer.pixels[47u*BM_SCREEN_WIDTH+102u]==9u);
#endif
}

static void test_runway_overlay(void){
  static const uint8_t expected[]={0xf4,0xfc,0xfc,0xfc,0xfc,0xfc,
                                   0xfc,0xfc,0xfc,0xfc,0xcb};
  BmWorldBuffer world;uint16_t draw_base;unsigned index;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.road_delay=0;world.road_position=20;world.road_target=20;
  world.runway_delay=0;world.runway_phase=36;
  world.charset_mode=BM_CHARSET_BUILDING;world.fine_phase=0;world.scroll_clock=1;
  world.hangar_impact_phase=2u;world.tank_impact_phase=1u;
  world.hangar_target_available=0u;
  bm_world_buffer_step(&world);
  assert(world.runway_phase==35&&world.charset_mode==BM_CHARSET_HANGAR);
  assert(world.hangar_impact_phase==0u&&world.tank_impact_phase==1u);
  assert(world.hangar_target_available==35u);
  assert(world.object_control==0);
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.runway_phase==34);
  assert(world.object_control==2);
  for(index=0;index<sizeof(expected);index++)
    assert(world.arena[draw_base+12-index]==expected[index]);
}

static void test_hangar_overlay(void){
  BmWorldBuffer world;uint16_t draw_base;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.road_delay=0;world.road_position=40;world.road_target=40;
  world.building_delay=255;world.runway_delay=0;world.runway_phase=20;
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.runway_phase==19);
  assert(world.arena[draw_base+21]==0x9a);
  assert(world.arena[draw_base+20]==0x95);
  assert(world.arena[draw_base+19]==0x7e);
  assert(world.arena[draw_base+17]==0x95);
}

static void test_hangar_phase_fifteen(void){
  BmWorldBuffer world;uint16_t draw_base;unsigned index;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.road_delay=0;world.road_position=40;world.road_target=40;
  world.building_delay=255;world.runway_delay=0;world.runway_phase=16;
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.runway_phase==15);
  for(index=0;index<5;index++)
    assert(world.arena[draw_base+20-index]==
           ((const uint8_t[]){0x9b,0x7e,0x7e,0x7e,0x1c})[index]);
  assert(world.arena[draw_base+15]==0u);
}

static void test_runway_stops_in_area_two(void){
  BmWorldBuffer world;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=2;
  world.road_delay=0;world.road_position=40;world.road_target=40;
  world.building_delay=255;world.runway_delay=0;world.runway_phase=36;
  world.charset_mode=BM_CHARSET_BUILDING;world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.runway_phase==36&&world.charset_mode==BM_CHARSET_BUILDING);
}

static void test_building_overlay(void){
  BmWorldBuffer world;uint16_t draw_base;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.road_delay=0;
  world.road_position=20;world.road_target=20;world.runway_delay=255;
  world.building_delay=0;world.building_inactive=0;world.building_row=0;
  world.building_position=30;world.building_width=4;world.building_height=3;
  world.building_roof_start=2;world.building_variant=0;world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.building_row==1&&world.arena[draw_base+30]==84);
  assert(world.arena[draw_base+29]==87&&world.arena[draw_base+26]==87);
  world.fine_phase=0;world.scroll_clock=1;
  draw_base=(uint16_t)(world.lms[BM_ATARI_PLAYFIELD_FETCH_ROWS-1]-20u);
  bm_world_buffer_step(&world);
  assert(world.building_row==2&&world.arena[draw_base+30]==84);
  assert(world.arena[draw_base+29]==86&&world.arena[draw_base+28]==87);
}

static void test_building_right_position_carry(void){
  BmWorldBuffer world;
  bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=2;
  world.road_delay=0;world.road_position=43;world.road_target=43;
  world.runway_delay=19;world.building_delay=0;world.building_inactive=255;
  world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.building_position==(uint8_t)(43u+4u+world.building_width));
}

static void test_runway_does_not_reset_building_end_rows(void){
  BmWorldBuffer world;bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.road_position=40;world.road_target=40;world.road_delay=255;
  world.runway_delay=0;world.runway_phase=13;
  world.object_marker=2;world.object_control=2;
  world.building_delay=0;world.building_inactive=0;world.building_row=6;
  world.building_position=40;world.building_width=4;world.building_height=4;
  world.building_roof_start=2;world.building_end_row=2;
  world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.building_end_row==3&&world.building_inactive==0);
  world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.building_end_row==4&&world.building_inactive==255);
  assert(world.object_control==2);
}

static void test_building_waits_for_tank(void){
  BmWorldBuffer world;bm_world_buffer_init(&world,1234);to_city(&world);
  world.generator.centre=1;world.generator.direction=BM_ROAD_STRAIGHT;
  world.generator.segment_remaining=10;world.area_type=1;
  world.road_position=40;world.road_target=40;world.road_delay=255;
  world.runway_delay=255;world.building_delay=0;
  world.building_inactive=255;world.tank_delay=9;world.tank_phase=0;
  world.fine_phase=0;world.scroll_clock=1;bm_world_buffer_step(&world);
  assert(world.building_inactive==255&&world.building_delay==15);
  assert(world.tank_delay==8);

  world.building_delay=0;world.tank_delay=10;world.fine_phase=0;world.scroll_clock=1;
  bm_world_buffer_step(&world);
  assert(world.building_inactive==0&&world.object_marker==1);
}

#if BM_CLASSIC_TEST_PAGE
static uint32_t region_checksum(const BmFramebuffer *framebuffer,unsigned height){
  uint32_t hash=2166136261u;unsigned index;
  for(index=0;index<height*BM_SCREEN_WIDTH;index++){
    hash^=framebuffer->pixels[index];hash*=16777619u;
  }
  return hash;
}

static void test_classic_world_moves(void){
  static BmGame game;BmInput input={0};uint32_t before,after;
  bm_game_init(&game,1234);bm_game_render(&game);
  before=region_checksum(&game.framebuffer,80);
  bm_game_tick(&game,&input);bm_game_render(&game);
#if BM_ATARI_SCROLL
  assert(game.classic_world.fine_phase==3);
  assert(before==region_checksum(&game.framebuffer,80));
  bm_game_tick(&game,&input);bm_game_render(&game);
#endif
  after=region_checksum(&game.framebuffer,80);
  assert(game.classic_world.fine_phase==2);
  assert(before!=after);
}
#endif

static void test_runway_landing_signals(void){
  static const uint8_t mask[9]={0,0,0,0,1,0,0,0,0};
  BmWorldBuffer world;int x=160,y=108,column,row;
  bm_world_buffer_init(&world,1234u);
  memset(world.rows,0,sizeof(world.rows));
  world.source_column=3u;world.fine_phase=2u;
  column=(x-(int)world.fine_phase*2+8)/8+(int)world.source_column;
  row=(y+(int)world.fine_phase*2)/8;
  assert(!bm_world_runway_visible(&world));
  assert(bm_world_runway_collision(&world,(int16_t)x,(int16_t)y)==0u);
  world.rows[row][column]=0xfcu;
  assert(bm_world_runway_visible(&world));
  assert(bm_world_runway_collision(&world,(int16_t)x,(int16_t)y)==8u);
  assert(bm_world_runway_collision(&world,(int16_t)(x+8),(int16_t)y)==0u);
  world.rows[row][column]=0x91u;
  assert(bm_world_runway_collision(&world,(int16_t)x,(int16_t)y)==8u);
  assert(bm_world_runway_mask_collision(&world,mask,3u,3u,3u,
         (int16_t)x,(int16_t)y,1,1)==8u);
  assert(bm_world_runway_mask_collision(&world,mask,3u,3u,3u,
         (int16_t)(x+8),(int16_t)y,1,1)==0u);
  world.rows[row][column]=0u;world.rows[0][world.source_column]=0xcbu;
  assert(bm_world_runway_visible(&world));
}

int main(void){
  BmGame a,b,feedback;BmInput in={0};unsigned i;bm_game_init(&a,1234);bm_game_init(&b,1234);
  for(i=0;i<500;i++){in.x=i%40<20?1:-1;in.y=i%30==0?-1:0;in.fire=i%17==0;
    bm_game_tick(&a,&in);bm_game_tick(&b,&in);bm_game_render(&a);bm_game_render(&b);}
  assert(a.tick==500&&a.player.x==b.player.x&&a.score==b.score);
  assert(bm_framebuffer_checksum(&a.framebuffer)==bm_framebuffer_checksum(&b.framebuffer));
  bm_game_init(&feedback,1234);in.x=0;in.fire=1;in.bomb=1;
  bm_game_tick(&feedback,&in);bm_game_render(&feedback);
#if BM_CLASSIC_TEST_PAGE
  assert(feedback.framebuffer.pixels[100*BM_SCREEN_WIDTH+152]==26);
#else
  assert(feedback.framebuffer.pixels[100*BM_SCREEN_WIDTH+152]==7);
#endif
#if !BM_CLASSIC_TEST_PAGE
  assert(feedback.framebuffer.pixels[97*BM_SCREEN_WIDTH+152]==2);
#endif
#if BM_CLASSIC_TEST_PAGE
  assert(feedback.framebuffer.pixels[100*BM_SCREEN_WIDTH+162]==26);
#else
  assert(feedback.framebuffer.pixels[100*BM_SCREEN_WIDTH+162]==7);
#endif
  /* PROP: $605 counts 4..1 and swaps the propeller rows at 2 and 4, so the
     frame changes every second update. */
  in.x=0;bm_game_tick(&feedback,&in);
  bm_game_render(&feedback);
#if !BM_CLASSIC_TEST_PAGE
  assert(feedback.framebuffer.pixels[100*BM_SCREEN_WIDTH+162]==2);
#endif
  assert(feedback.framebuffer.pixels[148*BM_SCREEN_WIDTH+152]==9);
#if BM_CLASSIC_TEST_PAGE
#if !BM_DEBUG_HUD
  /* T23 status line: DSPLY's 'F' ($E6, PF3) is character 3 of the first
     wide line, i.e. x=16 at y=176; the background is $66E (CLUT 35). */
  assert(feedback.framebuffer.pixels[176*BM_SCREEN_WIDTH+16]==39);
  assert(feedback.framebuffer.pixels[176*BM_SCREEN_WIDTH+0]==35);
#else
  assert(feedback.framebuffer.pixels[179*BM_SCREEN_WIDTH+2]==26);
#endif
#endif
  test_sprite_clipping();
  test_bank_frames();
  test_player_shot();
  test_player_bomb();
  test_bomb_grass_impact();
  test_bomb_source_object_routes();
  test_weapon_scoring();
  test_shore_impact_routes();
  test_impact_character_memory();
  test_bridge_building_vehicle_impacts();
  test_river_vehicle_shot();
  test_bomb_world_impact();
  test_bomb_water_impact();
  test_antic4_tiles();
  test_antic4_playfield();
  test_dfbldg_charset();
  test_dftnk_charset();
  test_world_rows();test_draw_segment_rules();
  test_world_buffer();
  test_crossroad_overlay();
  test_road_car_overlay();
  test_tank_overlay();
  test_secondary_tank_overlay();
  test_secondary_tank_preserves_road();
  test_gun_emplacement_overlay();
  test_gun_projectile();
  test_gun_projectile_render();
  test_runway_overlay();
  test_runway_landing_signals();
  test_hangar_overlay();
  test_hangar_phase_fifteen();
  test_runway_stops_in_area_two();
  test_building_overlay();
  test_building_right_position_carry();
  test_runway_does_not_reset_building_end_rows();
  test_building_waits_for_tank();
  test_first_area_transition();
  test_first_airfield_exit_sequence();
  test_classic_continues_after_bonus();
  test_classic_normal_bridge_rows();
  test_classic_normal_runway_rows();
  test_classic_normal_runway_keeps_river_clear();
  test_classic_normal_ship_position_and_motion();
  test_classic_river_car_spawn_and_motion();
  test_classic_enemy_fighter_entry();
  test_classic_land_freezes_objects();
  test_shbt_ground_targets();
  test_runway_flash();
  test_capital_builders();
  test_capital_target();
  test_target_sight();
  test_classic_tree_rows();
  test_classic_normal_tank_uses_river_centre();
  test_classic_normal_dynamic_tank();
  test_progressed_area_sequence();
  test_sortie_progress_trigger();
  test_offscreen_area_skips_normal_road();
#if BM_CLASSIC_TEST_PAGE
  test_classic_world_moves();
#endif
  printf("ok checksum=%08lx\n",(unsigned long)bm_framebuffer_checksum(&a.framebuffer));return 0;
}
