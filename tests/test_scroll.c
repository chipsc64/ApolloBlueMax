#include "bm_world.h"
#include "bm_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_scrolling_impact(uint8_t code,uint8_t mode){
  BmWorldBuffer world;
  unsigned tick,row,column,offset,duration;
  bm_world_buffer_init(&world,1234);
  memset(world.arena,code,sizeof(world.arena));
  memset(world.rows,code,sizeof(world.rows));
  world.source_column=0;
  world.area_type=0;
  bm_world_bomb_impact(&world,156,80);
  assert(world.bomb_impact_handled&&world.impact_mode==mode);
  offset=world.grass_impact_offset;duration=world.grass_impact_ticks;
  assert(duration>8u);
  for(tick=1;tick<=duration+8u;++tick){
    unsigned visible=0;
    uint8_t previous=world.arena[offset];
    bm_world_buffer_step(&world);
    assert(world.grass_impact_offset==offset);
    if(tick<duration&&!world.grass_impact_ticks){
      /* Terrain recycling invalidates an old animation. The next update
         must retire it without painting into the replacement terrain. */
      assert(tick>16u);
      if(mode==1)assert(previous!=0x3eu&&previous!=0x40u);
      else if(mode==3)assert(previous!=world.impact_expected[0]||
        world.arena[world.impact_other_offset]!=world.impact_expected[2]);
      else assert(0);
      assert(world.arena[offset]==previous);
      bm_world_buffer_step(&world);
      assert(!world.grass_impact_ticks);
      assert(world.arena[offset]==previous);
      return;
    }
    assert(world.grass_impact_ticks==(tick<duration?duration-tick:0u));
    /* Do not freeze fine_phase: include coarse LMS transitions. The
       impact remains attached to its arena cell throughout the animation. */
    for(row=0;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;++row)
      for(column=0;column<BM_ATARI_PLAYFIELD_FETCH_COLUMNS;++column)
        if(world.lms[row]+BM_VIEW_LMS_OFFSET+column==offset){
          assert(world.rows[row][column]==world.arena[offset]);
          ++visible;
        }
    /* The terrain can leave the fetch window before the effect expires.
       Only require visibility during the initial two coarse transitions. */
    if(tick<=16u)assert(visible);
    if(tick>=duration){
      if(mode==0)assert(world.arena[offset]==2u);
      if(mode==1){
        assert(world.arena[offset]==0x3eu);
        assert(world.arena[offset+1u]==0x41u);
      }
      if(mode==3){
        assert(world.arena[offset]==0xf5u&&world.arena[offset+1u]==0xf6u);
        assert(world.arena[world.impact_other_offset]==0xf5u);
        assert(world.arena[world.impact_other_offset+1u]==0xf6u);
      }
    }
  }
}

int main(void){
  BmWorldBuffer world;
  unsigned i;
  bm_world_buffer_init(&world,1234);
  world.river_car_active=1;world.river_car_destroyed=1;world.object_slot=1u;
  world.river_car_x=100;world.river_car_y=100;
  for(i=1;i<=8;++i){
    bm_world_buffer_step(&world);
    assert(world.fine_phase==(3u-(i/2u)%4u));
    assert(world.river_car_x==100u-i/2u);
    assert(world.river_car_y==100u+2u*(i/2u));
  }
  puts("ok: half-rate terrain, bridge-locked wreck");
  test_scrolling_impact(0,1);
  test_scrolling_impact(2,0);
  test_scrolling_impact(0xb9,3);
  puts("ok: grass, water and bridge impacts remain world-anchored");
  return 0;
}
