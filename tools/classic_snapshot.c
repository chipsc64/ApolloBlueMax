#include "bm_game.h"
#include <stdio.h>
#include <stdlib.h>

static void put_u16(FILE *file, unsigned value) {
  fputc((int)(value & 255u), file); fputc((int)((value >> 8) & 255u), file);
}
static void put_u32(FILE *file, uint32_t value) {
  put_u16(file, (unsigned)(value & 65535u));
  put_u16(file, (unsigned)(value >> 16));
}
static int write_bmp(const char *directory, const BmGame *game) {
  const BmWorldBuffer *world = &game->classic_world;
  char path[512];
  FILE *file;
  unsigned index;
  if (snprintf(path, sizeof(path),
               "%s/T%04lX_A%02X_U%02X_BV%02X_R%02X_X%c.bmp",
               directory, (unsigned long)game->tick, world->area_type,
               world->runway_phase, world->building_variant,
               world->building_row,
               world->charset_mode == BM_CHARSET_HANGAR ? 'H' : 'B') >=
      (int)sizeof(path)) return -1;
  file = fopen(path, "wb");
  if (!file) return -1;
  fputc('B', file); fputc('M', file);
  put_u32(file, 54u + BM_SCREEN_WIDTH * BM_SCREEN_HEIGHT * 3u);
  put_u32(file, 0u); put_u32(file, 54u); put_u32(file, 40u);
  put_u32(file, BM_SCREEN_WIDTH); put_u32(file, BM_SCREEN_HEIGHT);
  put_u16(file, 1u); put_u16(file, 24u); put_u32(file, 0u);
  put_u32(file, BM_SCREEN_WIDTH * BM_SCREEN_HEIGHT * 3u);
  put_u32(file, 2835u); put_u32(file, 2835u); put_u32(file, 0u); put_u32(file, 0u);
  for (index = 0; index < BM_SCREEN_WIDTH * BM_SCREEN_HEIGHT; ++index) {
    unsigned source = BM_SCREEN_WIDTH * BM_SCREEN_HEIGHT - BM_SCREEN_WIDTH
                    - (index / BM_SCREEN_WIDTH) * BM_SCREEN_WIDTH
                    + index % BM_SCREEN_WIDTH;
    uint32_t rgb = game->framebuffer.palette[game->framebuffer.pixels[source]];
    fputc((int)(rgb & 255u), file);
    fputc((int)((rgb >> 8) & 255u), file);
    fputc((int)((rgb >> 16) & 255u), file);
  }
  return fclose(file);
}

static int write_ppm(const char *directory, const BmGame *game) {
  const BmWorldBuffer *world = &game->classic_world;
  char path[512]; FILE *file; unsigned index;
  if (snprintf(path, sizeof(path), "%s/T%04lX.ppm", directory,
               (unsigned long)game->tick) >= (int)sizeof(path)) return -1;
  file = fopen(path, "wb"); if (!file) return -1;
  fprintf(file, "P6\n%u %u\n255\n", BM_SCREEN_WIDTH, BM_SCREEN_HEIGHT);
  for (index=0; index<BM_SCREEN_WIDTH*BM_SCREEN_HEIGHT; ++index) {
    uint32_t rgb=game->framebuffer.palette[game->framebuffer.pixels[index]];
    fputc((int)(rgb>>16)&255,file);fputc((int)(rgb>>8)&255,file);
    fputc((int)rgb&255,file);
  }
  (void)world;
  return fclose(file);
}
static void dump_world(const BmGame *game) {
  const BmWorldBuffer *w=&game->classic_world; unsigned row,column;
  printf("T%04lX A%02X C%02X U%02X/%02X RP%02X/%02X BV%02X BR%02X BE%02X BI%02X "
         "NR%02X/%02X/%02X BP%02X/%02X TP%02X TD%02X TV%02X TC%02X "
         "T2%02X/%02X OC%02X OM%02X GM%02X "
         "TS%02X/%02X/%02X/%02X MR%02X/%02X/%02X "
         "SH%02X/%02X/%02X/%02X/%02X SS%04X/%02X/%02X "
         "RC%02X/%02X/%02X/%02X/%02X "
         "G1%02X/%02X/%02X S%02X/%02X E%02X G2%02X/%02X/%02X "
         "S%02X/%02X E%02X\n",
         (unsigned long)game->tick,w->area_type,w->generator.centre,
         w->runway_phase,w->runway_delay,
         w->road_position,w->road_target,w->building_variant,w->building_row,w->building_end_row,
         w->building_inactive,w->normal_runway_phase,w->normal_runway_delay,
         w->normal_runway_position,w->bridge_phase,w->bridge_delay,
         w->tank_phase,w->tank_delay,w->tank_variant,
         w->tank_column,w->tank2_phase,w->tank2_delay,w->object_control,
         w->object_marker,w->glyph_mode,
         w->target_sight_active,w->target_sight_type,w->target_sight_x,
         w->target_sight_y,game->mission_objective_type,
         game->mission_targets_destroyed,game->mission_objective_target,
         w->ship_active,w->ship_x,w->ship_y,
         w->ship_destroyed,w->ship_destroy_timer,
         w->gun2_shot_x,w->gun2_shot_y,w->gun2_shot_explosion,
         w->river_car_active,w->river_car_x,w->river_car_y,w->river_car_left,
         w->river_vehicle_truck,
         w->gun1_x,w->gun1_y,
         w->gun1_start_delay,w->gun1_shot_x,w->gun1_shot_y,
         w->gun1_shot_explosion,w->gun2_x,w->gun2_y,w->gun2_start_delay,
         w->gun2_shot_x,w->gun2_shot_y,w->gun2_shot_explosion);
  for(row=0;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;++row){
    unsigned first=BM_ATARI_WORLD_STRIDE,last=0;
    for(column=1;column<BM_ATARI_WORLD_STRIDE;++column)
      if(w->rows[row][column]){if(first==BM_ATARI_WORLD_STRIDE)first=column;last=column;}
    if(last){printf("  %02u %02u..%02u:",row,first,last);
      for(column=first;column<=last;++column)printf(" %02X",w->rows[row][column]);
      putchar('\n');}
  }
}

int main(int argc, char **argv) {
  static const uint32_t diagnostic_ticks[] = {
    0x0276u, 0x031fu, 0x03feu, 0x04f8u, 0x0790u, 0x080eu, 0x081eu, 0x083eu,
    0x0a68u, 0x0b28u, 0x0d04u, 0x0d15u, 0x109au, 0x1130u,
    0x1150u, 0x1170u, 0x1190u, 0x11b0u, 0x15a0u, 0x15c0u,
    0x15e0u, 0x1600u, 0x1620u
  };
  static BmGame game;
  BmInput input = {0};
  uint8_t previous_phase = 255u;
  unsigned snapshots = 0;
  unsigned have_reference = 0;
  unsigned frames;
  unsigned diagnostic_index = 0;
  unsigned projectile_snapshots = 0;
  unsigned progressed_snapshot = 0;
  unsigned normal_tank_snapshot = 0;
  unsigned normal_dynamic_tank_snapshot = 0;
  unsigned normal_dynamic_tank_armed = 0;
  unsigned normal_ship_snapshot = 0;
  unsigned normal_ship_shot_snapshot = 0;
  unsigned normal_destroyed_ship_snapshot = 0;
  unsigned normal_river_car_snapshot = 0;
  unsigned normal_river_truck_snapshot = 0;
  unsigned normal_enemy_fighter_snapshot = 0;
  unsigned normal_enemy_fighter_left_snapshot = 0;
  unsigned normal_enemy_fighter_forward_snapshot = 0;
  unsigned normal_enemy_fighter_forward_shot_snapshot = 0;
  unsigned normal_enemy_fighter_forward_turn_mask = 0;
  unsigned target_sight_snapshot = 0;
  uint8_t previous_gun1_shot = 0u, previous_gun2_shot = 0u;
  uint8_t previous_target_sight = 0u;
  const char *directory = argc > 1 ? argv[1] : "build/snapshots";
  bm_game_init(&game, 0x424d4158u);
  for (frames = 0; frames < 20000u &&
       (!have_reference || snapshots < 14u || game.tick < 0x3000u);
       ++frames) {
    /* Pulse fire and bomb in separate windows so their original missile
       trajectories can be inspected without affecting world generation. */
    input.fire = game.tick == 0x0a1fu;
    input.bomb = game.tick == 0x0a5fu;
    bm_game_tick(&game, &input);
    if (!previous_target_sight && game.classic_world.target_sight_active)
      printf("GNSGHT start T%04lX type=%u x=%u y=%u screen-x=%d\n",
             (unsigned long)game.tick,game.classic_world.target_sight_type,
             game.classic_world.target_sight_x,
             game.classic_world.target_sight_y,
             ((int)game.classic_world.target_sight_x-48)*2);
    previous_target_sight=game.classic_world.target_sight_active;
    if (!target_sight_snapshot && game.classic_world.target_sight_active) {
      printf("GNSGHT first frame T%04lX type=%u x=%u y=%u objective=%u\n",
             (unsigned long)game.tick,game.classic_world.target_sight_type,
             game.classic_world.target_sight_x,
             game.classic_world.target_sight_y,game.mission_objective_type);
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      target_sight_snapshot = 1u;
    }
    if (projectile_snapshots < 4u &&
        ((!previous_gun1_shot && game.classic_world.gun1_shot_y) ||
         (!previous_gun2_shot && game.classic_world.gun2_shot_y))) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      ++projectile_snapshots;
    }
    previous_gun1_shot = game.classic_world.gun1_shot_y;
    previous_gun2_shot = game.classic_world.gun2_shot_y;
    if (!normal_ship_snapshot && game.classic_world.ship_active) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_ship_snapshot = 1u;
    }
    if (!normal_ship_shot_snapshot && game.classic_world.ship_active &&
        game.classic_world.gun2_shot_y) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_ship_shot_snapshot = 1u;
    }
    if (!normal_destroyed_ship_snapshot &&
        game.classic_world.ship_destroyed) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_destroyed_ship_snapshot = 1u;
    }
    if (!normal_river_car_snapshot && game.classic_world.river_car_active) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_river_car_snapshot = 1u;
    }
    if (!normal_river_truck_snapshot && game.classic_world.river_car_active &&
        game.classic_world.river_vehicle_truck) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_river_truck_snapshot = 1u;
    }
    if (!normal_enemy_fighter_snapshot &&
        game.classic_world.enemy_fighter_active) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_enemy_fighter_snapshot = 1u;
    }
    if (!normal_enemy_fighter_left_snapshot &&
        game.classic_world.enemy_fighter_turn == 1u) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_enemy_fighter_left_snapshot = 1u;
    }
    if (!normal_enemy_fighter_forward_snapshot &&
        game.classic_world.enemy_fighter_active &&
        game.classic_world.enemy_fighter_forward) {
      printf("PLFWD first frame T%04lX x=%u y=%u\n",
             (unsigned long)game.tick,game.classic_world.enemy_fighter_x,
             game.classic_world.enemy_fighter_y);
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_enemy_fighter_forward_snapshot = 1u;
    }
    if (!normal_enemy_fighter_forward_shot_snapshot &&
        game.classic_world.enemy_fighter_shot_active &&
        game.classic_world.enemy_fighter_shot_forward) {
      printf("TRIGF first frame T%04lX x=%u y=%u\n",
             (unsigned long)game.tick,game.classic_world.enemy_fighter_shot_x,
             game.classic_world.enemy_fighter_shot_y);
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_enemy_fighter_forward_shot_snapshot = 1u;
    }
    if (game.classic_world.enemy_fighter_active &&
        game.classic_world.enemy_fighter_forward &&
        game.classic_world.enemy_fighter_turn &&
        !(normal_enemy_fighter_forward_turn_mask&
          (1u<<game.classic_world.enemy_fighter_turn))) {
      printf("FTURN first frame T%04lX state=%u x=%u y=%u shadow=%u\n",
             (unsigned long)game.tick,game.classic_world.enemy_fighter_turn,
             game.classic_world.enemy_fighter_x,
             game.classic_world.enemy_fighter_y,
             game.classic_world.enemy_fighter_shadow_y);
      bm_game_render(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_enemy_fighter_forward_turn_mask|=
        1u<<game.classic_world.enemy_fighter_turn;
    }
    if (!normal_tank_snapshot && game.classic_world.area_progress != 0u &&
        game.classic_world.tank_phase == 2u) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_tank_snapshot = 1u;
    }
    if (game.classic_world.area_progress != 0u &&
        game.classic_world.tank2_phase == 1u)
      normal_dynamic_tank_armed = 1u;
    if (!normal_dynamic_tank_snapshot && normal_dynamic_tank_armed &&
        game.classic_world.tank2_phase == 0u) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      normal_dynamic_tank_snapshot = 1u;
    }
    if (!progressed_snapshot && game.classic_world.area_progress != 0u) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      progressed_snapshot = 1u;
    }
    if (game.tick == 0x08a0u || game.tick == 0x08b0u ||
        game.tick == 0x08c0u || game.tick == 0x08d0u ||
        game.tick == 0x08e0u || game.tick == 0x08e8u ||
        game.tick == 0x08f0u || game.tick == 0x08f8u ||
        game.tick == 0x08f9u || game.tick == 0x08fau ||
        game.tick == 0x0900u || game.tick == 0x0901u ||
        game.tick == 0x0a20u || game.tick == 0x0a24u ||
        game.tick == 0x0a28u || game.tick == 0x0a40u ||
        game.tick == 0x0a60u || game.tick == 0x0a70u ||
        game.tick == 0x0a8eu ||
        game.tick == 0x0a78u || game.tick == 0x0a90u ||
        game.tick == 0x0940u || game.tick == 0x09a0u ||
        game.tick == 0x0ac0u ||
        game.tick == 0x0f08u || game.tick == 0x0f20u ||
        game.tick == 0x0f40u ||
        (game.tick >= 0x0d00u && game.tick <= 0x0de0u &&
         (game.tick & 0x1fu) == 0u)) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
    }
    if (game.tick >= 0x0288u && game.tick <= 0x0295u) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
    }
    if (diagnostic_index < sizeof(diagnostic_ticks)/sizeof(diagnostic_ticks[0]) &&
        game.tick == diagnostic_ticks[diagnostic_index]) {
      bm_game_render(&game);
      dump_world(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      if (write_ppm(directory, &game) != 0) return 2;
      ++diagnostic_index;
    }
    if (game.classic_world.area_type == 1u &&
        game.classic_world.runway_phase <= 20u &&
        game.classic_world.runway_phase >= 7u &&
        game.classic_world.runway_phase != previous_phase) {
      bm_game_render(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      previous_phase = game.classic_world.runway_phase;
      ++snapshots;
    }
    if (!have_reference && game.classic_world.building_variant == 6u &&
        game.classic_world.building_row == 2u &&
        game.classic_world.charset_mode == BM_CHARSET_BUILDING) {
      bm_game_render(&game);
      if (write_bmp(directory, &game) != 0) return 2;
      have_reference = 1u;
    }
    if (game.tick == 0x0f08u) {
      bm_game_render(&game);
      if (write_bmp(directory, &game) != 0) return 2;
    }
    if (game.tick >= 0x0400u && game.tick <= 0x04c0u &&
        (game.tick & 3u) == 0u) {
      bm_game_render(&game);
      if (write_bmp(directory, &game) != 0) return 2;
    }
  }
  printf("wrote %u runway snapshots, BV06 reference=%u, %u diagnostics and "
         "%u projectile starts to %s\n", snapshots, have_reference,
         diagnostic_index, projectile_snapshots, directory);
  return snapshots && have_reference && diagnostic_index ==
         sizeof(diagnostic_ticks)/sizeof(diagnostic_ticks[0]) ? 0 : 3;
}
