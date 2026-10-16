#include "bm_world.h"
#include "bm_chbas_assets.h"
#include "bm_geometry.h"
#include <string.h>
#ifndef BM_CLASSIC_TEST_PAGE
#define BM_CLASSIC_TEST_PAGE 0
#endif

static void put(uint8_t row[BM_ATARI_WORLD_STRIDE], uint8_t position,
                uint8_t tile) {
  if (position > 0 && position < BM_ATARI_WORLD_STRIDE) row[position] = tile;
}

static uint8_t random_or_zero(BmWorldRandom random_byte, void *userdata) {
  return random_byte ? random_byte(userdata) : 0;
}

static void choose_straight(BmWorldGenerator *g, BmWorldRandom random_byte,
                            void *userdata) {
  g->segment_remaining = (uint8_t)((random_or_zero(random_byte, userdata) & 3u) | 2u);
  g->direction = BM_ROAD_STRAIGHT;
}

void bm_world_generator_init(BmWorldGenerator *generator) {
  memset(generator, 0, sizeof(*generator));
  generator->direction = BM_ROAD_LEFT;
  generator->segment_remaining = 6;
  generator->edge_countdown = 255;
  generator->shoulder_pending = 1;
  generator->centre = 26;
  generator->left_limit = 16;
  generator->right_limit = 75;
}

static void straight(BmWorldGenerator *g, uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t y = g->centre;
  unsigned count;
  put(row, y, 1); if (!--y) goto done;
  for (count = 0; count < 19; ++count) { put(row, y, 2); if (!--y) goto done; }
  put(row, y, 3); if (!--y) goto done;
  if (g->shoulder_pending) put(row, y, 4);
  else { put(row, y, 10); if (--y) put(row, y, 9); }
done:
  g->shoulder_pending = 1;
}

static int left(BmWorldGenerator *g, uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t y;
  unsigned count;
  if (g->segment_remaining & 1u) {
    --g->centre; y = g->centre;
    if (y == g->left_limit) {           /* W: $602=4, JMP RND */
      g->previous_turn = 4; return 1;
    }
    put(row, y, 12); if (!--y) goto finish;
    for (count = 0; count < 19; ++count) { put(row, y, 2); if (!--y) goto finish; }
    put(row, y, g->segment_remaining == 1 ? 16 : 14);
  } else {
    y = g->centre; put(row, y, 11); if (!--y) goto finish;
    for (count = 0; count < 19; ++count) { put(row, y, 2); if (!--y) goto finish; }
    put(row, y, 13); if (!--y) goto finish;
    if (g->edge_countdown == 255) {
      put(row, y, 4); --g->edge_countdown; g->previous_turn = 255;
      return 0;
    } else put(row, y, 15);
  }
finish:
  --g->edge_countdown;
  return 0;
}

static int right(BmWorldGenerator *g, uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t y;
  unsigned count;
  ++g->centre; y = g->centre;
  if (y >= g->right_limit) {           /* $606=0, DEC FF, $602=5, RND */
    g->shoulder_pending = 0; --g->centre; g->previous_turn = 5;
    return 1;
  }
  put(row, y, 6); if (!--y) goto finish;
  put(row, y, 5); if (!--y) goto finish;
  for (count = 0; count < 18; ++count) { put(row, y, 2); if (!--y) goto finish; }
  put(row, y, 8); if (!--y) goto finish;
  put(row, y, 7); if (!--y) goto finish;
  if (g->edge_countdown == 255) {
    put(row, y, 4); --g->edge_countdown; g->previous_turn = 255;
  } else {
    put(row, y, 10); if (--y) put(row, y, 9);
  }
finish:
  --g->edge_countdown;
  return 0;
}

/* EXXON.1 DRAW.  STRGHT, LFT and RGHT first decrement $601; an expired
   straight goes through AX (50 % RND, else AW), an expired turn always
   through RND (a short straight).  AW picks the new turn from $602, $610 or
   RANDOM; a right turn (AV) shortens its length to 3|($601>>1). */
void bm_world_generate_row(BmWorldGenerator *g,
                           uint8_t row[BM_ATARI_WORLD_STRIDE],
                           BmWorldRandom random_byte, void *userdata) {
  if (!g || !row) return;
  memset(row + 1, 0, BM_ATARI_WORLD_STRIDE - 1);
  if (g->direction == BM_ROAD_RIGHT) goto rght;
  if (g->direction != BM_ROAD_STRAIGHT) goto lft;
strght:
  if (!--g->segment_remaining) {
    if (random_or_zero(random_byte, userdata) & 1u) goto rnd;   /* AX */
    goto aw;
  }
  straight(g, row);
  return;
rnd:
  choose_straight(g, random_byte, userdata);
  goto strght;
aw:
  g->segment_remaining =
    (uint8_t)((random_or_zero(random_byte, userdata) & 15u) | 3u);
  g->edge_countdown = 255;
  if (g->previous_turn == 4) goto av;
  if (g->previous_turn == 5) goto ba;
  if (g->turn_preference) {
    uint8_t carry = (uint8_t)(g->turn_preference & 1u);
    g->turn_preference >>= 1;
    if (!carry) goto av;
    goto ba;
  }
  if (!(random_or_zero(random_byte, userdata) & 1u)) goto av;  /* EC */
ba:
  g->direction = BM_ROAD_LEFT;
  goto lft;
av:
  g->direction = BM_ROAD_RIGHT;
  g->segment_remaining = (uint8_t)(3u | (g->segment_remaining >> 1));
  goto rght;
lft:
  if (!--g->segment_remaining) goto rnd;
  if (left(g, row)) goto rnd;
  return;
rght:
  if (!--g->segment_remaining) { g->shoulder_pending = 0; goto rnd; }
  if (right(g, row)) goto rnd;
}

static uint8_t buffer_random(void *userdata) {
  BmWorldBuffer *buffer = (BmWorldBuffer *)userdata;
  buffer->rng = buffer->rng * 1664525u + 1013904223u;
  return (uint8_t)(buffer->rng >> 24);
}

static uint8_t gun_direction(uint8_t origin_x, uint8_t player_x);

static uint8_t gun_random(BmWorldBuffer *buffer) {
  buffer->gun_rng = buffer->gun_rng * 1664525u + 1013904223u;
  return (uint8_t)(buffer->gun_rng >> 24);
}

static void start_tank_gun(BmWorldBuffer *buffer, uint8_t column);
static void set_target_state(BmWorldBuffer *b, uint8_t value);

/* DFHNGR (EXXON.15) restores the hangar glyphs 21..32 and 126. */
static void load_hangar_glyphs(BmWorldBuffer *buffer) {
  buffer->charset_mode=BM_CHARSET_HANGAR;
  buffer->glyph_mode=BM_CHARSET_HANGAR;
  buffer->hangar_impact_phase=0u;
}

/* RND7 (EXXON.3): (RANDOM&7)|3. */
static uint8_t bridge_random_width(BmWorldBuffer *buffer) {
  return (uint8_t)((buffer_random(buffer) & 7u) | 3u);
}

static void bridge_put(uint8_t row[BM_ATARI_WORLD_STRIDE], int position,
                       uint8_t tile) {
  if (position > 0 && position < BM_ATARI_WORLD_STRIDE)
    row[position] = tile;
}

/* EXXON.3 DRBR.  J (VBIR) counts $FE down per coarse step and arms
   F8=8/$61B; DRBR then waits for a straight river with 34<=FF<54 and draws
   one bridge row per coarse step. */
static void draw_bridge(BmWorldBuffer *buffer,
                        uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  static const uint8_t br1[25] = {
    0xb1,0x04,0xb2,0xaf,0xaf,0xaf,0xfb,0x9b,0x9b,0xb2,0xaf,0xaf,0xaf,
    0xaf,0xfb,0x9b,0x9b,0xb2,0xaf,0xaf,0xaf,0xfb,0x04,0x48,0xb1
  };
  static const uint8_t br2[21] = {
    0xab,0xa9,0xae,0xae,0xae,0xab,0xab,0xa9,0xae,0xae,0xae,0xae,0xae,
    0xab,0xab,0xa9,0xae,0xae,0xae,0xab,0xac
  };
  static const uint8_t br3[21] = {
    0xb8,0xb7,0xaa,0xaa,0xaa,0xb8,0xb8,0xb7,0xaa,0xaa,0xaa,0xaa,0xaa,
    0xb8,0xb8,0xb7,0xaa,0xaa,0xaa,0xb8,0xb7
  };
  int y, index;
  uint8_t count, centre = buffer->generator.centre;
  if (buffer->bridge_delay) return;
  /* LV: steer DRAW back into the bridge window through $610. */
  if (centre >= 54u) { buffer->generator.turn_preference = 3u; return; }
  if (centre < 34u) { buffer->generator.turn_preference = 4u; return; }
  if (buffer->generator.direction != BM_ROAD_STRAIGHT) return;
  --buffer->bridge_phase;
  if (buffer->bridge_skip && buffer->bridge_phase == 2u) --buffer->bridge_phase;
  y = centre;
  switch (buffer->bridge_phase) {
    case 5u: /* FS: BR1, then A2 with FF+9 and $BF=0. */
      y += 2;
      for (index = 24; index >= 0; --index) bridge_put(row,y--,br1[index]);
      start_tank_gun(buffer,(uint8_t)(centre+9u));
      buffer->gun1_kind = 0u;                      /* STX $BF: FBRGN */
      break;
    case 4u: /* FR */
      buffer->bridge_right_bank = bridge_random_width(buffer);
      for (count=0;count<buffer->bridge_right_bank;++count) bridge_put(row,y++,252u);
      buffer->bridge_right_water = bridge_random_width(buffer);
      for (count=0;count<buffer->bridge_right_water&&y<64;++count)
        bridge_put(row,y++,181u);
      while (y < 64) bridge_put(row,y++,182u);
      y=(int)centre-1;
      for(index=20;index>=0;--index) bridge_put(row,y--,br2[index]);
      buffer->bridge_left_bank=bridge_random_width(buffer);
      for(count=0;count<buffer->bridge_left_bank;++count) bridge_put(row,y--,252u);
      buffer->bridge_left_water=bridge_random_width(buffer);
      for(count=0;count<buffer->bridge_left_water&&y>0;++count) bridge_put(row,y--,181u);
      while (y > 0) bridge_put(row,y--,182u);
      break;
    case 3u: /* JH arms DRP23 in the shared slot, then draws GS. */
      if (!buffer->object_slot && (buffer_random(buffer) & 7u)) {
        buffer->object_slot = 1u;
        buffer->river_car_armed = 1u;
        /* $640=$62A=6.  JH runs in the main loop after this frame's VBI
           SCROL, so the first DEC $62A comes with the next scroll step;
           move_river_car (same step) decrements once already, hence 7. */
        buffer->river_car_delay = 7u;
      }
      /* fall through */
    case 2u: /* GS */
      memset(row+1,252u,BM_ATARI_WORLD_STRIDE-1u);
      y=(int)centre-1;
      for(index=0;index<20;++index) bridge_put(row,y--,219u);
      break;
    case 1u: /* FQ */
      y += buffer->bridge_right_bank;
      for(count=0;count<buffer->bridge_right_water&&y<65;++count) bridge_put(row,y++,179u);
      while (y < 65) bridge_put(row,y++,180u);
      y=centre;
      for(index=20;index>=0;--index) bridge_put(row,--y,br3[index]);
      y -= buffer->bridge_left_bank;
      for(count=0;count<buffer->bridge_left_water&&y>0;++count) bridge_put(row,y--,179u);
      while (y > 0) bridge_put(row,y--,180u);
      break;
    case 0u: /* GL */
      buffer->object_control=255u;
      buffer->bridge_score_state=2u;
      buffer->bridge_delay=(uint8_t)((buffer_random(buffer)|32u)&63u);
      if(buffer->target_sight_request){
        bm_world_target_sight_start(buffer,3u,(uint8_t)(centre-1u),22u);
        buffer->target_sight_request=0u;
      }
      break;
    default: /* phases 7 and 6 */
      buffer->object_control=0u;
      buffer->generator.segment_remaining=8u; /* $601 */
      break;
  }
}

static void draw_normal_runway_hangar(BmWorldBuffer *buffer,
                                      uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  static const uint8_t hangar1[5]={0x95u,0x7eu,0x7eu,0x95u,0x9au};
  static const uint8_t hangar2[6]={0x96u,0x97u,0x98u,0x99u,0x9bu,0x9au};
  static const uint8_t hangar3[6]={0x1cu,0x7eu,0x7eu,0x7eu,0x9bu,0x9au};
  static const uint8_t hangar5[4]={0x1du,0x1eu,0x1fu,0x20u};
  const uint8_t *tiles=0;unsigned count=0,skip=0,index;
  /* SH has written 145 + nine middle tiles + 147 and decremented Y once
     more before UE/UF/UG, so the hangar starts eleven columns beside $65B. */
  int position=(int)buffer->normal_runway_position-11;
  switch(buffer->normal_runway_phase){
    case 23u:tiles=hangar1;count=5u;break;
    case 22u:tiles=hangar2;count=6u;break;
    case 21u:case 20u:case 19u:tiles=hangar3;count=6u;break;
    case 18u:tiles=hangar3;count=5u;skip=1u;break;
    case 17u:tiles=hangar5;count=4u;skip=2u;break;
    default:return;
  }
  position-=(int)skip;
  for(index=0;index<count;++index)
    bridge_put(row,position-(int)index,tiles[count-1u-index]);
}

/* EXXON.10 MB/SH/MC: one friendly runway row per coarse step. */
static void runway_flash(BmWorldBuffer *b);
static void draw_runway_rows(BmWorldBuffer *buffer,
                             uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  int position;unsigned index;uint8_t middle=252u,phase;
  if (!--buffer->normal_runway_phase) {
    /* MC: CMP #255 leaves carry set only when the runway owned $FD. */
    uint8_t owned=(uint8_t)(buffer->object_slot==255u);
    buffer->object_control=255u;
    if (owned) buffer->object_slot=0u;
    buffer->normal_runway_delay=(uint8_t)((buffer_random(buffer)&31u)+
      buffer->runway_interval+owned);
    return;
  }
  runway_flash(buffer);                    /* SH */
  phase=buffer->normal_runway_phase;
  if (phase==1u||phase==4u||phase==44u) {
    /* NB: the first row also steers DRAW left when FF<45; EZ sets B6=2. */
    if (phase==44u&&buffer->generator.centre<45u)
      buffer->generator.turn_preference=255u;
    buffer->object_control=2u;
    middle=148u;
  }
  position=buffer->normal_runway_position;
  bridge_put(row,position--,145u);
  for(index=0;index<9u;++index) bridge_put(row,position--,middle);
  bridge_put(row,position,147u);
  if (phase==26u) { buffer->object_control=0u; load_hangar_glyphs(buffer); }
  if (phase==23u) {
    buffer->object_control=2u;
    buffer->hangar_target_available=2u; /* $6F9 */
  }
  if (phase==2u) { /* V2: landing window closes. */
    buffer->runway_marker=0u;
    buffer->runway_slot=1u;
  }
  draw_normal_runway_hangar(buffer,row);
}

/* EXXON.13 FLSH (from DRUN's MD and SH): once all objectives are hit
   ($6AC>=$6C7) the landing marker blinks on bit 1 of $6B7; with the capital
   destroyed ($6C2=7) the status line flashes 196 as well. */
static void runway_flash(BmWorldBuffer *b) {
  uint8_t v;
  if (b->objectives_destroyed < b->objective_target) return;
  v = --b->flash_counter;
  if (v & 2u) {
    b->marker_blank = 1u;
    if (b->capital_destroyed == 7u && b->runway_marker) b->status_bk = 196u;
    b->sound_flash = 1u;                   /* .4: AUDC4=0 */
  } else {
    if (b->capital_destroyed == 7u) b->status_bk = 0u;
    b->marker_blank = 0u;
    b->sound_flash = b->runway_marker ? 2u : 1u;   /* AUDC4 $A2, AUDF4 $F */
  }
}

/* EXXON.10 DRUN.  $638 runs at half coarse rate; a new runway waits for a
   free $FD slot and a river centre below 30 or from 65 onwards. */
static void draw_friendly_runway(BmWorldBuffer *buffer,
                                 uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t centre=buffer->generator.centre;
  if (buffer->normal_runway_delay) {
    --buffer->runway_half;
    if (buffer->runway_half & 1u) --buffer->normal_runway_delay;
    return;
  }
  if (buffer->normal_runway_phase) { draw_runway_rows(buffer,row); return; }
  if (!buffer->bridge_delay) return;
  buffer->bridge_delay=100u;
  if (buffer->moored_ship_column) return;
  buffer->moored_ship_delay=120u;
  if (buffer->object_slot!=255u) {
    if (buffer->object_slot!=0u&&buffer->object_slot!=5u) return;
    /* R3: claim the slot, open the landing window, cancel a pending
       objective request ($C4=$32). */
    buffer->object_slot=255u;
    buffer->runway_slot=0u;
    buffer->runway_marker=0x32u;
    buffer->marker_blank=0u;               /* SC+$687=$32 */
    set_target_state(buffer,0x32u);
    buffer->sound_runway=1u;               /* $64C=$19: RUNSND */
  }
  /* MD */
  runway_flash(buffer);
  if (centre>=40u) {
    buffer->generator.turn_preference=0x40u;
    if (centre<65u) return;
  } else {
    buffer->generator.turn_preference=255u;
    if (centre>=30u) return;
  }
  /* MA: CMP #50 leaves carry set for the SBC branch. */
  {
    uint8_t r=(uint8_t)(buffer_random(buffer)&7u);
    buffer->normal_runway_position=(uint8_t)(centre<50u?44u+r:37u-r);
  }
  buffer->normal_runway_phase=45u;
  buffer->object_control=0u;
  load_hangar_glyphs(buffer);
}

/* MAX.1 $190D..$197B (DRBT): boats made of world characters in the river,
   independent of the moving SHIP player layers. */
static void draw_moored_ship(BmWorldBuffer *b,uint8_t *row){
  int c;
  if(b->moored_ship_column){
    c=b->moored_ship_column;
    row[c]=0xbbu;
    if(c>0)row[c-1]=0xbau;
    b->moored_ship_column=0;
    b->moored_ship_delay=(uint8_t)((buffer_random(b)|16u)&31u);
    b->boat_hits=2u;
    return;
  }
  if(b->moored_ship_delay){--b->moored_ship_delay;return;}
  if(b->bridge_delay<5u&&b->generator.centre>=31u&&b->generator.centre<58u){
    b->moored_ship_delay=15u;
    return;
  }
  c=(int)b->generator.centre-4;
  if(c<1||c>=BM_ATARI_WORLD_STRIDE)return;
  row[c]=0xbdu;row[c-1]=0xbcu;
  b->moored_ship_column=(uint8_t)(c-1);
  b->moored_ship_offset=(uint16_t)(row-b->arena+c-1);
  /* DRBT (MAX.1 $1944): A2 with FF+2, then $BF=$661=$BC: gun 1 is now the
     boat gun and FBTGN fires it. */
  start_tank_gun(b,(uint8_t)(b->generator.centre+2u));
  b->gun1_kind=0xbcu;
}


/* $C4 objective state.  Requests ($C4=5) are mirrored in
   target_sight_request for the generators that consume them. */
static void set_target_state(BmWorldBuffer *b, uint8_t value) {
  b->target_state = value;
  b->target_sight_request = (uint8_t)(value == 5u);
}

/* An object drawn while $C4 is zero becomes the bonus objective
   (706=$7C/50) and decrements $C4. */
static uint8_t take_bonus_target(BmWorldBuffer *b) {
  if (b->target_state) return 0u;
  set_target_state(b, 255u);
  return 1u;
}

static void release_slot(BmWorldBuffer *b) {
  b->object_slot = 0u;
}

/* EXXON.11 MVSHP: the ship moves two scanlines down and one colour clock
   left, the same vector as the terrain on an even VBI. */
static void mvshp(BmWorldBuffer *b) {
  b->ship_y = (uint8_t)(b->ship_y + 2u);
  --b->ship_x;
}

/* EXXON.11 SHIP (SRT loop only). */
static void move_normal_ship(BmWorldBuffer *buffer) {
  if (buffer->world_mode != BM_WORLD_SRT) return;
  if (!buffer->object_slot) {
    /* I3: RANDOM&15==0, 29<=FF<60 and 12<=$FE<25.  The final CPY leaves
       carry clear, so SBC #6 yields FF-7. */
    uint8_t centre = buffer->generator.centre;
    if (buffer_random(buffer) & 15u) return;
    if (centre >= 60u || centre < 29u) return;
    if (buffer->bridge_delay < 12u || buffer->bridge_delay >= 25u) return;
    buffer->ship_x = (uint8_t)(((uint8_t)(centre - 7u) << 2) + 27u);
    buffer->ship_y = 15u;
    buffer->object_slot = 15u;
    buffer->p2_colour = take_bonus_target(buffer) ? 50u : 2u;
    buffer->p2_hits = 3u;                       /* P7: $6DC=3 */
    buffer->ship_hits_remaining = 3u;
    buffer->ship_fast = (uint8_t)(buffer_random(buffer) & 1u); /* $62B */
    buffer->ship_active = 1u;
    buffer->ship_destroyed = 0u;
    buffer->ship_destroy_timer = 0u;
    return;
  }
  if (buffer->object_slot != 15u || !buffer->ship_active) return;
  if (buffer->ship_destroyed) {
    /* XSHP: $66F counts down; the wreck moves on even VBIs. */
    if (!buffer->ship_destroy_timer || !--buffer->ship_destroy_timer ||
        buffer->ship_y >= 194u) {
      buffer->ship_active = 0u;
      release_slot(buffer);
      return;
    }
    if (buffer->scroll_event) mvshp(buffer);
    return;
  }
  /* FSHGN (EXXON.11): the ship fires missile 2, gun 2's shot ($69C):
     C1 70..177, $697=C0+2, Y=C1-7, then J3/C6 as FM2GN.  COMMON's FM2GN
     moves it in the same frame. */
  if (!buffer->gun2_shot_y && buffer->ship_y>=70u && buffer->ship_y<178u) {
    buffer->gun2_x=(uint8_t)(buffer->ship_x+2u);
    buffer->gun2_shot_x=buffer->gun2_x;
    buffer->gun2_shot_y=(uint8_t)(buffer->ship_y-7u);
    buffer->gun2_shot_direction=gun_direction(buffer->gun2_x,
                                              buffer->player_x_atari);
    buffer->gun2_shot_travel=(uint8_t)((gun_random(buffer)&15u)+15u);
    buffer->gun2_shot_explosion=0u;
  }
  if (!buffer->scroll_event) return;          /* even $604 */
  if (buffer->ship_y >= 194u) {
    buffer->ship_active = 0u;
    release_slot(buffer);
    return;
  }
  /* I7/J0: the slow ship skips the coarse-scroll frame, the fast one moves
     twice on it. */
  if (!buffer->ship_fast) {
    if (!buffer->coarse_event) mvshp(buffer);
  } else {
    mvshp(buffer);
    if (buffer->coarse_event) mvshp(buffer);
  }
}

/* EXXON.3 DRP23: the bridge car or truck owned by $FD=1. */
static void move_river_car(BmWorldBuffer *buffer) {
  if (buffer->object_slot != 1u) return;
  if (buffer->river_car_destroyed) {
    /* XP23/R5: the wreck moves with the terrain until C1>=185. */
    if (buffer->river_car_destroy_timer) --buffer->river_car_destroy_timer;
    if (buffer->river_car_y >= 185u) {
      buffer->river_car_active = 0u;
      release_slot(buffer);
      return;
    }
    if (buffer->scroll_event) {                /* even $604 */
      buffer->river_car_y = (uint8_t)(buffer->river_car_y + 2u);
      --buffer->river_car_x;
    }
    return;
  }
  if (buffer->river_car_armed) {
    uint8_t r;
    unsigned right;
    /* IO: SCROL decrements $62A; IS loads C1=12 once it is zero. */
    if (buffer->scroll_event && buffer->river_car_delay)
      --buffer->river_car_delay;
    if (buffer->river_car_delay) return;
    buffer->river_car_armed = 0u;
    buffer->river_car_y = 12u;
    buffer->river_vehicle_truck = 0u;
    r = buffer_random(buffer);
    if (take_bonus_target(buffer)) {
      buffer->p2_colour = 0x7cu;
      right = r & 1u;                         /* P9 */
    } else if (!(r & 1u)) {
      buffer->p2_colour = 18u;                /* IT */
      right = (r >> 1) & 1u;                  /* P9 */
    } else if (!(r & 2u)) {
      /* JW/KC: truck, colour 10 or 44, direction from the next bit. */
      buffer->river_vehicle_truck = 1u;
      buffer->p2_colour = (r & 4u) ? 10u : 44u;
      right = (r >> 3) & 1u;
    } else {
      buffer->p2_colour = 146u;
      right = (r >> 2) & 1u;
    }
    /* IY/JY enter with carry set (+91); IU/JZ with carry clear (+210). */
    buffer->river_car_left = (uint8_t)!right;
    buffer->river_car_x = (uint8_t)((buffer_random(buffer) & 31u) +
                                    (right ? 91u : 210u));
    buffer->river_car_active = 1u;
    buffer->river_car_destroyed = 0u;
    buffer->river_car_destroy_timer = 0u;
    ++buffer->river_vehicle_count;
    return;
  }
  if (!buffer->river_car_active) return;
  /* IZ/JE */
  if (buffer->river_car_y >= 194u) {
    buffer->river_car_active = 0u;
    release_slot(buffer);
    return;
  }
  if (buffer->river_car_left) --buffer->river_car_x;   /* JF */
  if (buffer->scroll_event)                            /* JN: even $604 */
    buffer->river_car_y = (uint8_t)(buffer->river_car_y + 2u);
}

/* EXXON.8 LINE: the enemy is "in line" with the player when its depth
   C2-C1 is within 14 of F6-F5-7 and F6>=C2.  COLM02 then lets a bullet
   destroy it at once. */
static void enemy_line(BmWorldBuffer *b) {
  uint8_t depth, gap;
  if (b->player_y_atari < b->enemy_fighter_shadow_y) {
    b->enemy_in_line = 0u;
    b->line_star = 0u; b->status_bk = 0u;      /* AEL, UO */
    b->sound_line = 2u;
    return;
  }
  depth = (uint8_t)(b->player_y_atari - b->player_plane_y - 7u);
  gap = (uint8_t)(b->enemy_fighter_shadow_y - b->enemy_fighter_y);
  b->enemy_in_line = (uint8_t)((uint8_t)(gap - depth) < 14u);
  if (b->enemy_in_line) {
    b->line_star = 0u; b->status_bk = 146u;    /* $66E=$92 */
    b->sound_line = 1u; b->sound_line_low = (uint8_t)(depth < 18u);
    return;
  }
  b->sound_line = 2u;
  /* ASTRSK (only at 200+): '*' on the top line when the enemy flies
     higher, on the bottom line when lower; then UO clears $66E. */
  if (b->player_fast) b->line_star = gap < depth ? 2u : 1u;
  b->status_bk = 0u;
}

/* Aircraft warning (OD/LQ): per coarse row; blinks while $636>=13. */
static uint8_t enemy_warning_step(BmWorldBuffer *b) {
  if (!b->coarse_event) return 0u;
  if (!--b->enemy_warn) {
    b->warning_on = 0u;
    return 1u;
  }
  b->warning_on = b->enemy_warn >= 13u ? (uint8_t)!(b->enemy_warn & 1u) : 0u;
  if (b->enemy_warn >= 13u) b->sound_warn = b->warning_on ? 1u : 2u;  /* KZ/OF, KY/OE */
  return 0u;
}

/* XP2: destroyed aircraft.  Returns non-zero while a wreck is shown. */
static uint8_t enemy_wreck(BmWorldBuffer *b) {
  if (!b->enemy_fighter_destroyed) return 0u;
  if (!b->enemy_fighter_destroy_timer ||
      !--b->enemy_fighter_destroy_timer ||
      (b->object_slot == 3u && b->enemy_fighter_y >= 187u)) {
    b->enemy_fighter_active = 0u;
    b->enemy_fighter_destroyed = 0u;
    b->enemy_body_on = b->enemy_shadow_on = 0u;
    release_slot(b);
    return 1u;
  }
  if (b->object_slot == 3u) ++b->enemy_fighter_y;
  return 1u;
}

static void enemy_shot_step(BmWorldBuffer *b) {
  if (!b->enemy_fighter_shot_active) return;
  if (b->enemy_fighter_shot_forward) {
    /* SF: end at Y>=185 or X<55, else -3 colour clocks and +4 lines. */
    if (b->enemy_fighter_shot_y >= 185u || b->enemy_fighter_shot_x < 55u)
      b->enemy_fighter_shot_active = 0u;
    else {
      b->enemy_fighter_shot_x = (uint8_t)(b->enemy_fighter_shot_x - 3u);
      b->enemy_fighter_shot_y = (uint8_t)(b->enemy_fighter_shot_y + 4u);
    }
  } else if (b->enemy_fighter_shot_y < 30u ||
             b->enemy_fighter_shot_x >= 205u) {
    b->enemy_fighter_shot_active = 0u;            /* RR -> RO */
  } else {
    b->enemy_fighter_shot_x = (uint8_t)(b->enemy_fighter_shot_x + 3u);
    b->enemy_fighter_shot_y = (uint8_t)(b->enemy_fighter_shot_y - 3u);
  }
}

/* TRIGB: missile 2 from the rear fighter, chance $6CA per update. */
static void enemy_trigb(BmWorldBuffer *b) {
  if (b->enemy_fighter_shot_active) { enemy_shot_step(b); return; }
  if ((int8_t)(b->enemy_fighter_shadow_y - b->player_y_atari) < 0) return;
  if (b->enemy_fighter_x >= 170u) return;
  if (buffer_random(b) >= b->enemy_fire_chance) return;
  if (b->enemy_state != 5u) return;
  b->enemy_fighter_shot_active = 1u;
  b->enemy_fighter_shot_forward = 0u;
  b->enemy_fighter_shot_x = (uint8_t)(b->enemy_fighter_x + 6u);
  b->enemy_fighter_shot_y = (uint8_t)(b->enemy_fighter_y + 2u);
}

/* TRIGF: missile 2 from the forward fighter (C0-3/C1+12). */
static void enemy_trigf(BmWorldBuffer *b) {
  if (b->enemy_fighter_shot_active) { enemy_shot_step(b); return; }
  if (b->enemy_fighter_shadow_y >= b->player_y_atari) return;
  if (buffer_random(b) >= b->enemy_fire_chance) return;
  b->enemy_fighter_shot_active = 1u;
  b->enemy_fighter_shot_forward = 1u;
  b->enemy_fighter_shot_x = (uint8_t)(b->enemy_fighter_x - 3u);
  b->enemy_fighter_shot_y = (uint8_t)(b->enemy_fighter_y + 12u);
}

/* BTURN/TRACK for the rear fighter. */
static void enemy_bturn(BmWorldBuffer *b) {
  if (b->enemy_fighter_turn) return;              /* K2: propeller only */
  /* TRACK: slow tracking fighters turn toward the player once. */
  if (b->enemy_state == 5u && b->enemy_slow && b->enemy_tracking) {
    unsigned own = (unsigned)b->enemy_fighter_shadow_y + b->enemy_fighter_x;
    unsigned player = (unsigned)b->player_y_atari + b->player_x_atari;
    if (own > 255u) own = 255u;
    if (player > 255u) player = 255u;
    b->enemy_fighter_turn = player < own ? 1u : 2u;
    return;
  }
  if (buffer_random(b) & 63u) return;
  if (b->enemy_fighter_y >= 90u) return;
  /* K3: random bit selects K5 (state 1) or K4 (state 2). */
  b->enemy_fighter_turn = (buffer_random(b) & 1u) ? 1u : 2u;
}

/* FTURN for the forward fighter. */
static void enemy_fturn(BmWorldBuffer *b) {
  if (b->enemy_fighter_turn) {
    if (b->enemy_fighter_turn == 2u) {            /* M6/M7 */
      ++b->enemy_fighter_y;
      ++b->enemy_fighter_shadow_y;
    }
    return;
  }
  if (buffer_random(b) & 63u) return;
  if (b->enemy_fighter_y < 35u || b->enemy_fighter_y >= 130u) return;
  /* M1 (carry clear) is FTR/state 2, otherwise FTL/state 1. */
  b->enemy_fighter_turn = (buffer_random(b) & 1u) ? 1u : 2u;
}

/* EXXON.6 PLBCK ($FD=2) and EXXON.5 PLFWD ($FD=3). */
/* EXXON.6 L6: the PLBCK state machine ($640 2..6), shared by PLBCK ($FD=2)
   and PBMB's airfield attacker ($FD=254). */
static void plbck_states(BmWorldBuffer *b) {
  switch (b->enemy_state) {
    case 2u:                                    /* OD */
      if (enemy_warning_step(b)) b->enemy_state = 3u;
      return;
    case 3u:                                    /* OG */
      b->enemy_fighter_y = 186u;
      b->enemy_fighter_shadow_y = 186u;
      b->enemy_timer = (uint8_t)(((buffer_random(b) & 63u) | 24u) + 6u);
      b->enemy_fighter_x = (uint8_t)(buffer_random(b) & 63u);
      b->enemy_state = 4u;
      b->p2_colour = take_bonus_target(b) ? 0x7cu : 44u;
      b->enemy_fighter_active = 1u;
      b->enemy_fighter_forward = 0u;
      b->enemy_fighter_destroyed = 0u;
      b->enemy_body_on = 1u;
      b->enemy_shadow_on = 0u;
      b->enemy_in_line = 0u;
      return;
    case 4u:                                    /* OW */
      if (!--b->enemy_timer) {
        b->enemy_state = 5u;
        b->enemy_shadow_on = 1u;
      }
      break;
    case 5u:                                    /* OJ */
      if (enemy_wreck(b)) return;
      enemy_line(b);
      break;
    default:
      goto or_label;
  }
  /* OX: COLP02 is evaluated by BmGame. */
  enemy_trigb(b);
  enemy_bturn(b);
  if (b->enemy_fighter_turn == 2u) goto or_label;
  if (b->enemy_fighter_y < 14u) {               /* .1 -> K1, AEL */
    b->enemy_body_on = 0u;
    b->enemy_in_line = 0u;
    ++b->enemy_state;
    goto or_label;
  }
  --b->enemy_fighter_y;                         /* OL/OQ */
  if (b->enemy_state != 5u) goto ov_label;
or_label:
  if (b->enemy_fighter_shadow_y < 12u) {        /* K0 */
    b->enemy_shadow_on = 0u;
    b->enemy_fighter_active = 0u;
    release_slot(b);
    return;
  }
  if (b->enemy_fighter_x >= 220u) {             /* OS */
    b->enemy_shadow_on = b->enemy_body_on = 0u;
    b->enemy_fighter_shadow_y = 0u;
    b->enemy_fighter_active = 0u;
    release_slot(b);
    return;
  }
  if (b->enemy_fighter_turn != 2u) --b->enemy_fighter_shadow_y; /* J9 */
ov_label:
  if (b->enemy_fighter_turn != 1u) ++b->enemy_fighter_x;        /* OV */
  return;
}

static void move_enemy_fighter(BmWorldBuffer *buffer) {
  BmWorldBuffer *b = buffer;
  if (!b->object_slot) {
    /* PLBCK/OB */
    if (buffer_random(b) & 31u) return;
    b->enemy_slow = 0u;
    b->enemy_fighter_turn = 0u;
    if (b->enemy_block) return;
    b->enemy_warn = 23u;
    if (!(buffer_random(b) & 1u)) {               /* OY -> KV */
      b->enemy_state = 7u;
      b->object_slot = 3u;
      return;
    }
    b->object_slot = 2u;
    b->enemy_state = 2u;
    if (b->area_type < 3u && (b->frame_counter & 1u)) b->enemy_slow = 1u;
    return;
  }
  if (b->object_slot == 2u) {
    if (b->enemy_slow && !b->scroll_event) {      /* odd $604 */
      if (enemy_wreck(b)) return;
      enemy_trigb(b);
      if (b->enemy_state >= 5u) enemy_line(b);
      return;
    }
    plbck_states(b);
    return;
  }
  if (b->object_slot != 3u) return;
  switch (b->enemy_state) {
    case 7u:                                      /* LQ */
      if (enemy_warning_step(b)) b->enemy_state = 8u;
      return;
    case 8u:                                      /* KD */
      b->p2_colour = take_bonus_target(b) ? 0x7cu : 44u;
      /* CMP #8 left carry set: ADC #170 adds one. */
      b->enemy_fighter_x = (uint8_t)((buffer_random(b) & 31u) + 171u);
      b->enemy_fighter_y = 15u;
      b->enemy_fighter_shadow_y = 15u;
      b->enemy_state = 9u;
      b->enemy_timer = (uint8_t)(((buffer_random(b) & 31u) | 12u) + 3u);
      b->enemy_fighter_active = 1u;
      b->enemy_fighter_forward = 1u;
      b->enemy_fighter_destroyed = 0u;
      b->enemy_body_on = 0u;
      b->enemy_shadow_on = 1u;                    /* SHDW1 in P3 */
      b->enemy_in_line = 0u;
      return;
    case 9u:                                      /* LN */
      if (!--b->enemy_timer) {
        b->enemy_state = 10u;
        b->enemy_body_on = 1u;
      }
      goto lb_label;
    default:
      break;
  }
  /* LF (states 10 and 11) */
  if (enemy_wreck(b)) return;
  enemy_trigf(b);
  enemy_fturn(b);
  if (b->enemy_fighter_y >= 186u ||                /* UZ */
      b->enemy_fighter_x < 30u) {                  /* LG -> LD, UZ */
    b->enemy_body_on = b->enemy_shadow_on = 0u;
    b->enemy_fighter_active = 0u;
    b->enemy_fighter_shot_active = 0u;
    release_slot(b);
    return;
  }
  if (b->enemy_fighter_turn != 2u)                 /* L7 */
    b->enemy_fighter_y = (uint8_t)(b->enemy_fighter_y + 2u);
  if (b->enemy_state == 11u) goto lp_label;        /* LO */
  enemy_line(b);
lb_label:
  if (b->enemy_fighter_shadow_y >= 190u) {         /* LD */
    b->enemy_shadow_on = 0u;
    ++b->enemy_state;
    goto lp_label;
  }
  if (b->enemy_fighter_turn == 2u) goto m4_label;
  b->enemy_fighter_shadow_y = (uint8_t)(b->enemy_fighter_shadow_y + 2u);
lp_label:
  if (b->enemy_fighter_turn == 1u) return;         /* M2 */
m4_label:
  --b->enemy_fighter_x;
}

/* EXXON.12 TRGT (COMMON): choose the next objective. */
static void target_step(BmWorldBuffer *b) {
  if (b->target_state == 253u) {
    /* Z9: the sight moves on even VBIs; U2 retires it at Y>=190. */
    if (!b->target_sight_active) return;
    if (b->target_sight_y >= 190u) {
      b->target_sight_active = 0u;
      b->target_sight_type = 0u;
      set_target_state(b, 252u);                /* DEC $C4 */
      release_slot(b);
    } else if (b->scroll_event) {
      b->target_sight_y = (uint8_t)(b->target_sight_y + 2u);
      --b->target_sight_x;                       /* DEC $C0 */
    }
    return;
  }
  if (b->target_state != 10u) {
    if (!b->coarse_event) return;
    if (b->objectives_destroyed >= b->objective_target || b->enemy_block ||
        !b->normal_runway_delay || !b->target_state ||
        b->target_state == 5u) return;
    if (b->target_cooldown) { --b->target_cooldown; return; }
    if (!(buffer_random(b) & 1u)) {              /* O3: next object */
      set_target_state(b, 0u);
      b->target_cooldown = (uint8_t)((buffer_random(b) | 64u) & 127u);
      return;
    }
  }
  /* Q5 */
  if (b->object_slot) {
    set_target_state(b, 10u);
    if (b->object_slot == 5u) release_slot(b);
    return;
  }
  /* Q6 */
  if (!b->tank_delay) { set_target_state(b, 10u); return; }
  if (b->tank_delay >= 20u) {
    if (!b->area_type) {
      if (!b->bridge_delay || b->bridge_delay >= 15u) {
        set_target_state(b, 10u);
        return;
      }
      if (b->generator.centre < 30u || b->generator.centre >= 58u) return;
    } else if (!b->building_delay || b->building_delay >= 25u) {
      set_target_state(b, 10u);
      return;
    }
  }
  /* Q2 */
  set_target_state(b, 5u);
  b->object_slot = 5u;
  b->target_cooldown = (uint8_t)((buffer_random(b) | 64u) & 127u);
}

static uint8_t tree_random(BmWorldBuffer *buffer) {
  uint32_t x=buffer->tree_rng;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  buffer->tree_rng=x;
  return (uint8_t)x;
}

/* EXXON.2 DRTR builds every tree over two consecutive generated rows.
   $60A..$60F hold the left column of each pair started on the previous row;
   this call completes those lower halves before starting up to $B6 new
   trees outside the river band FF-23..FF+1. */
static void draw_trees(BmWorldBuffer *buffer, uint8_t *row) {
  uint8_t count=buffer->object_control,low,high;
  unsigned slot,attempts;
  for (slot=0;slot<6u;++slot) {
    uint8_t y=buffer->tree_positions[slot];
    if (!y) continue;
    buffer->tree_positions[slot]=0u;
    row[y]=(tree_random(buffer)&1u)?36u:35u;
    row[y-1u]=(tree_random(buffer)&1u)?34u:33u;
  }
  if (!count) return;
  if (count==255u) {
    uint8_t value;
    do value=(uint8_t)(tree_random(buffer)&7u); while (!value);
    count=(uint8_t)(value-1u);
    if (!count) return;
  }
  /* EM: $612=FF+2, $611=FF+2-25 (BPL clamps a negative result to zero). */
  high=(uint8_t)(buffer->generator.centre+2u);
  low=(uint8_t)(high-25u);
  if (low&0x80u) low=0u;
  /* $616: 24 attempts shared by all trees of this row. */
  attempts=25u;
  slot=count;
  while (slot && --attempts) {
    uint8_t y=(uint8_t)(tree_random(buffer)&63u);
    if (y>=62u) continue;
    if (y<high && y>=low) continue;
    if (row[y] || row[y+1u] || row[y+2u]) continue;
    row[y+2u]=(tree_random(buffer)&1u)?39u:40u;
    row[y+1u]=(tree_random(buffer)&1u)?37u:38u;
    --slot;
    buffer->tree_positions[slot]=(uint8_t)(y+1u);
  }
}


enum {
  BM_WORLD_ARENA_BYTES = 25u * BM_ATARI_WORLD_STRIDE,
  BM_WORLD_INITIAL_BOTTOM = 24u * BM_ATARI_WORLD_STRIDE - 1u
};

static void materialize_view(BmWorldBuffer *buffer) {
  unsigned row;
  memset(buffer->rows, 0, sizeof(buffer->rows));
  for (row = 0; row < BM_ATARI_PLAYFIELD_FETCH_ROWS; ++row) {
    uint16_t offset = buffer->lms[row];
    /* HSCROL makes ANTIC fetch a wide line; the normal window shows
       LMS+4 onwards, plus a partially visible LMS+3 (bm_geometry.h). */
    if (offset + BM_VIEW_LMS_OFFSET <=
        BM_WORLD_ARENA_BYTES - BM_ATARI_PLAYFIELD_FETCH_COLUMNS)
      memcpy(buffer->rows[row], buffer->arena + offset + BM_VIEW_LMS_OFFSET,
             BM_ATARI_PLAYFIELD_FETCH_COLUMNS);
  }
}

static void area_prepare(BmWorldBuffer *buffer) {
  uint8_t centre = buffer->generator.centre;
  if (centre >= 2u && centre < 87u)
    buffer->generator.turn_preference = centre < 40u ? 255u : 128u;
}

static void area_advance(BmWorldBuffer *buffer) {
  if (buffer->area_delay) { --buffer->area_delay; return; }
  /* $677 selects B7's staged exit from the first airfield. */
  if (buffer->area_transition != 0u) {
    ++buffer->area_transition;
    if (buffer->area_transition == 2u) {
      ++buffer->area_type;
      buffer->area_delay = 24u;
    } else if (buffer->area_transition == 3u) {
      ++buffer->area_type;
      buffer->area_delay = 13u;
    } else {
      buffer->road_target = 72u;
      if ((uint8_t)(buffer->road_position + 1u) == buffer->road_target) {
        /* XC (EXXON.8): leave CITY for the SRT loop.  $666/$667 keep the
           CITY limits 1/90. */
        buffer->area_type = 0u;
        buffer->colpf1 = 148u;
        buffer->bridge_delay = 74u;          /* 148>>1 -> $FE */
        buffer->normal_runway_delay =
          (uint8_t)((buffer->frame_counter | 32u) & 63u);
        buffer->world_mode = BM_WORLD_SRT;
        /* AKA: release a pending objective request and its slot. */
        if (buffer->target_state == 5u) {
          set_target_state(buffer, 0u);
          buffer->object_slot = 0u;
        }
      }
    }
    return;
  }
  ++buffer->area_type;
  switch (buffer->area_type) {
    case 2u:
      buffer->area_delay = 61u;
      break;
    case 3u:
      buffer->object_control = 1u;
      if (buffer->area_progress != 0u) {
        /* WP: 709=0 and INC $6A6 (no new PLBCK/PLFWD until WV). */
        buffer->colpf1 = 0u;
        ++buffer->enemy_block;
        /* WP: $684, $67D, $694, $68A, $68E = 1. */
        buffer->capital[0].active = buffer->capital[0].delay = 1u;
        buffer->capital[1].active = buffer->capital[1].delay = 1u;
        buffer->area_delay = 13u;
        break;
      }
      /* WP sees initial $6AE==0 and enters U7 (DFGN: ADI -> ADK). */
      buffer->capital_gun_glyph = 0u;
      buffer->area_type = 1u;
      buffer->area_transition = 1u;
      buffer->colbk = 0u;
      buffer->colpf2 = 210u;
      buffer->colpf1 = 0x2au;
      buffer->area_delay = (uint8_t)((buffer_random(buffer) & 63u) | 64u);
      break;
    case 4u:
      buffer->object_control = 0u;
      buffer->capital[0].phase = buffer->capital[1].phase = 0u; /* $BA,$690 */
      buffer->area_delay = 24u;
      break;
    case 5u:
      /* WR ends with DFBLDG after restoring the hangar/tank glyph ranges. */
      buffer->hangar_impact_phase=0u;
      buffer->tank_impact_phase=0u;
      buffer->building_impact_phase=0u;
      buffer->charset_mode = BM_CHARSET_BUILDING;
      buffer->glyph_mode = BM_CHARSET_BUILDING;
      buffer->tank_charset_loaded = 1u;
      buffer->capital_gun_glyph = 1u;  /* WR: ADJ -> ADK (glyph 49) */
      buffer->colpf2 = 6u;   /* WR: 710 */
      buffer->colbk = 4u;    /* WR: 712 */
      buffer->area_delay = 24u;
      break;
    case 6u:
      buffer->colpf3 = 16u;  /* WT: 711 */
      buffer->colpf0 = 48u;  /* WT: 708 */
      buffer->area_delay = (uint8_t)((buffer_random(buffer) & 31u) | 64u);
      break;
    case 7u:
      buffer->area_delay = 36u;
      break;
    case 8u:
      /* WV */
      buffer->colpf0 = 196u;
      buffer->colpf3 = 4u;
      --buffer->enemy_block;
      buffer->area_delay = 24u;
      break;
    default:
      /* U7 starts the next airfield cycle; DFGN restores ADI in ADK. */
      buffer->capital_gun_glyph = 0u;
      buffer->area_type = 1u;
      buffer->area_transition = 1u;
      buffer->object_control = 1u;
      buffer->colbk = 0u;
      buffer->colpf2 = 210u;
      buffer->colpf1 = 0x2au;
      buffer->area_delay = (uint8_t)((buffer_random(buffer) & 63u) | 64u);
      break;
  }
}

static void draw_crossroad(BmWorldBuffer *buffer, uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  static const uint8_t road_r[7] = {0xfdu,0xceu,0xfcu,0xfcu,0xcdu,0xccu,0x48u};
  static const uint8_t road_s[5] = {0xcbu,0xfcu,0xfcu,0xfcu,0xf4u};
  int position;
  unsigned index;
  if (buffer->road_delay) { --buffer->road_delay; return; }
  position = buffer->road_position;
  if (buffer->road_position != buffer->road_target) {
    buffer->colpf1 = 0x2au;
    for (index = 0; index < 7u; ++index) {
      int destination = position - (int)index;
      if (destination > 0 && destination < BM_ATARI_WORLD_STRIDE)
        row[destination] = road_r[6u-index];
    }
    ++buffer->road_position;
    return;
  }
  for (index = 0; index < 5u; ++index) {
    int destination = position - 2 - (int)index;
    if (destination > 0 && destination < BM_ATARI_WORLD_STRIDE)
      row[destination] = road_s[4u-index];
  }
}

static void draw_road_car(BmWorldBuffer *buffer,
                          uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t first;
  int position;
  if ((buffer_random(buffer) & 15u) != 0u) return;
  position = (int)buffer->road_target - 4;
  if (buffer->area_type == 2u || buffer->area_type == 3u) {
    first = 239u;
  } else if (buffer->area_type >= 4u) {
    return;
  } else {
    first = (buffer_random(buffer) & 1u) ? 237u : 239u;
  }
  if (position >= 0 && position < BM_ATARI_WORLD_STRIDE)
    row[position] = first;
  --position;
  if (position >= 0 && position < BM_ATARI_WORLD_STRIDE)
    row[position] = (uint8_t)(first - 1u);
}

static void draw_runway(BmWorldBuffer *buffer,
                        uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  static const uint8_t hangar1[5] = {0x95u,0x7eu,0x7eu,0x95u,0x9au};
  static const uint8_t hangar2[6] = {0x96u,0x97u,0x98u,0x99u,0x9bu,0x9au};
  static const uint8_t hangar3[6] = {0x1cu,0x7eu,0x7eu,0x7eu,0x9bu,0x9au};
  static const uint8_t hangar5[4] = {0x1du,0x1eu,0x1fu,0x20u};
  const uint8_t *hangar = 0;
  unsigned hangar_count = 0;
  unsigned hangar_skip = 0;
  int position;
  unsigned index;
  if (buffer->runway_delay) { --buffer->runway_delay; return; }
  if (buffer->runway_phase == 36u && buffer->area_type >= 2u) return;
  if (--buffer->runway_phase == 0u) {
    buffer->runway_phase = 36u;
    buffer->runway_delay = (uint8_t)((buffer_random(buffer) & 127u) | 64u);
    return;
  }
  if (buffer->runway_phase == 35u) {
    buffer->hangar_target_available=35u;
    buffer->hangar_impact_phase=0u;
    buffer->charset_mode = BM_CHARSET_HANGAR;
    buffer->glyph_mode = BM_CHARSET_HANGAR;
    buffer->object_marker = 2u;
    buffer->object_control = 0u;
    return;
  }
  /* ENRUN's $676 marker remains 2 after DFHNGR.  On every following phase
     TZ copies that value to B6, forcing an overlapping BLDG towards its end
     before the shared screen codes are interpreted as hangar glyphs. */
  if (buffer->object_marker == 2u)
    buffer->object_control = 2u;
  position = (int)buffer->road_target - 8;
  for (index = 0; index < BM_ASSET_ENRUN_TILE_COUNT; ++index) {
    int destination = position - (int)index;
    if (destination > 0 && destination < BM_ATARI_WORLD_STRIDE)
      row[destination] =
        bm_asset_enrun_tiles[BM_ASSET_ENRUN_TILE_COUNT - 1u - index];
  }
  if (buffer->runway_phase == 20u) {
    buffer->hangar_impact_phase=0u;
    buffer->charset_mode = BM_CHARSET_HANGAR;
    buffer->glyph_mode = BM_CHARSET_HANGAR;
    buffer->object_marker = 2u;
    buffer->object_control = 0u;
    return;
  }
  switch (buffer->runway_phase) {
    case 19u: hangar=hangar1;hangar_count=5u;break;
    case 18u: hangar=hangar2;hangar_count=6u;break;
    case 17u:
    case 16u: hangar=hangar3;hangar_count=6u;break;
    case 15u: hangar=hangar3;hangar_count=5u;hangar_skip=1u;break;
    case 14u: hangar=hangar5;hangar_count=4u;hangar_skip=2u;break;
    default: return;
  }
  position = (int)buffer->road_target - 19 - (int)hangar_skip;
  for (index = 0; index < hangar_count; ++index) {
    int destination = position - (int)index;
    if (destination >= 0 && destination < BM_ATARI_WORLD_STRIDE)
      row[destination] = hangar[hangar_count - 1u - index];
  }
}

static void building_put(uint8_t row[BM_ATARI_WORLD_STRIDE], int position,
                         uint8_t tile) {
  if (position >= 0 && position < BM_ATARI_WORLD_STRIDE) row[position] = tile;
}

static void start_building(BmWorldBuffer *buffer) {
  uint8_t random;
  if (buffer->area_type >= 3u) return;
  /* VH keeps BLDG away from an imminent or active TNKC sequence. */
  if (buffer->tank_delay < 10u) {
    buffer->building_delay = 15u;
    return;
  }
  buffer->building_width = (uint8_t)((buffer_random(buffer) & 7u) | 3u);
  random = buffer_random(buffer);
  if ((random & 1u) && buffer->runway_delay >= 20u)
    buffer->building_position = (uint8_t)(buffer->road_target - 9u);
  else
    buffer->building_position = (uint8_t)(buffer->road_target + 4u +
                                          buffer->building_width);
  buffer->building_height = (uint8_t)(3u + (buffer_random(buffer) & 1u));
  buffer->building_roof_start = (uint8_t)(2u + (buffer_random(buffer) & 1u));
  if (buffer->building_position < buffer->road_target)
    buffer->building_variant = (buffer_random(buffer) & 1u) ? 6u : 134u;
  else if (buffer->area_type >= 2u)
    buffer->building_variant = 6u;
  else
    buffer->building_variant = (buffer_random(buffer) & 1u) ? 6u : 0u;
  buffer->building_row = 0u;
  buffer->building_end_row = 0u;
  buffer->object_control = 0u;
  buffer->building_inactive = 0u;
  buffer->building_impact_phase=0u;
  buffer->charset_mode = BM_CHARSET_BUILDING;
  buffer->glyph_mode = BM_CHARSET_BUILDING;
  buffer->object_marker = 1u;
}

static void start_tank_gun(BmWorldBuffer *buffer, uint8_t column) {
  /* A2 is entered with the logical world column in A.  Unlike the guarded
     GUNC compatibility path, the original TNKC/TNKC1 calls always replace
     the perspective origin. */
  buffer->gun1_x = (uint8_t)(column * 4u);
  buffer->gun1_origin_x = buffer->gun1_x;
  buffer->gun1_start_delay = 50u;
  buffer->gun1_y = 45u;
}

/* B3 (EXXON.10): second gun origin $697/$698, delay 50, Y 45. */
static void start_second_gun(BmWorldBuffer *buffer, uint8_t column) {
  buffer->gun2_x = (uint8_t)(column * 4u);
  buffer->gun2_origin_x = buffer->gun2_x;
  buffer->gun2_start_delay = 50u;
  buffer->gun2_y = 45u;
}

/* B5 (EXXON.10): arm the gun of a freshly drawn tank row.  With $B7 non-zero
   it always uses A2; with $B7=0 A2 only while $FE>=10, $61D>=10 and gun one
   ($661) is free, otherwise B3. */
static void start_tank_gun_b5(BmWorldBuffer *buffer, uint8_t column) {
  if (buffer->area_type || (buffer->bridge_delay >= 10u &&
      buffer->moored_ship_delay >= 10u && !buffer->gun1_origin_x)) {
    start_tank_gun(buffer, column);
    buffer->gun1_kind = 45u;              /* A2 returns A=45: STA $BF */
  } else
    start_second_gun(buffer, column);
}

/* DRTNK (SRT, base FF) and TNKC (CITY, base $66A) share QS/TT. */
static void draw_tank(BmWorldBuffer *buffer,
                      uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  unsigned count;
  int position;
  uint8_t tile;
  if (buffer->tank_delay) { --buffer->tank_delay; return; }
  if (buffer->tank_phase == 0u) {
    if (buffer->world_mode == BM_WORLD_SRT) {
      /* DRTNK/QM */
      if (buffer->normal_runway_delay < 20u) { buffer->tank_delay = 60u; return; }
      if (buffer->bridge_delay < 8u && buffer->generator.centre >= 25u) {
        buffer->tank_delay = 10u;
        return;
      }
      if (buffer->generator.centre >= 36u) {
        buffer->generator.turn_preference = 3u;
        return;
      }
    } else if (buffer->area_type >= 3u) return;       /* TNKC/TV */
    /* QS */
    buffer->tank_phase = 8u;
    buffer->object_control = 0u;
    buffer->object_marker = 0u;
    buffer->tank_variant = (buffer_random(buffer) & 1u) ? 0u : 128u;
    return;
  }
  position = (buffer->world_mode == BM_WORLD_SRT ?
              (int)buffer->generator.centre : (int)buffer->road_target) + 2;
  --buffer->tank_phase;
  if (buffer->tank_phase >= 4u) {
    /* TT forces B6=2 while $676 is clear.  This advances any building that
       overlaps the tank approach into its ending rows before both routines
       write into the same freshly generated screen row. */
    if (buffer->object_marker == 0u) buffer->object_control = 2u;
    for (count = 0; count < 6u; ++count) {
      ++position;
      if (buffer_random(buffer) & 1u) {
        if (position < BM_ATARI_WORLD_STRIDE) row[position] = 198u;
        ++position;
        if (position < BM_ATARI_WORLD_STRIDE) row[position] = 199u;
      } else {
        ++position;
        if (position < BM_ATARI_WORLD_STRIDE) row[position] = 72u;
      }
    }
    return;
  }
  if (buffer->tank_phase == 3u) {
    buffer->tank_impact_phase=0u;
    buffer->object_control = 0u;
    buffer->glyph_mode = BM_CHARSET_TANK;
    buffer->tank_charset_loaded = 1u;
    return;
  }
  if (buffer->tank_phase == 2u) {
    position += 3;
    buffer->tank_column = (uint8_t)position;
    ++position;
    tile = (uint8_t)(69u + buffer->tank_variant);
    for (count = 0; count < 5u; ++count)
      if (position++ < BM_ATARI_WORLD_STRIDE) row[position - 1] = tile;
    tile = (uint8_t)(74u + buffer->tank_variant);
    if (position < BM_ATARI_WORLD_STRIDE) row[position] = tile;
    /* QY stores $6A3=11 and calls B5.  In the active road areas B5 enters A2
       with A=$66A+$6A3, arming the first gun origin for this tank. */
    start_tank_gun_b5(buffer,
      (uint8_t)((buffer->area_type ? buffer->road_target :
                 buffer->generator.centre) + 11u));
    position = (int)buffer->tank_column - 1;
    if (position >= 0 && position < BM_ATARI_WORLD_STRIDE) row[position] = 177u;
    return;
  }
  if (buffer->tank_phase == 1u) {
    /* RC reloads Y=$653 and enters RD after its INY sequence.  Unlike phase
       2, this row therefore starts exactly at tank_column. */
    position = (int)buffer->tank_column;
    tile = (uint8_t)(69u + buffer->tank_variant);
    for (count = 0; count < 5u; ++count)
      if (position++ < BM_ATARI_WORLD_STRIDE) row[position - 1] = tile;
    tile = (uint8_t)(18u + buffer->tank_variant);
    if (position < BM_ATARI_WORLD_STRIDE) row[position] = tile;
    return;
  }
  position = (int)buffer->tank_column - 1;
  if (position >= 0 && position < BM_ATARI_WORLD_STRIDE) row[position] = 255u;
  ++position;
  for (count = 0; count < 4u; ++count)
    if (position++ < BM_ATARI_WORLD_STRIDE) row[position - 1] = 191u;
  tile = (uint8_t)(73u + buffer->tank_variant);
  if (position < BM_ATARI_WORLD_STRIDE) row[position] = tile;
  if (buffer->object_marker == 0u) buffer->object_control = 255u;
  buffer->tank_delay = (uint8_t)((buffer_random(buffer) & 15u) + 26u);
  if(buffer->target_sight_request){
    bm_world_target_sight_start(buffer,1u,
      (uint8_t)(buffer->tank_column+14u),11u);
    buffer->target_sight_request=0u;
  }
}

static int is_road_tile(uint8_t tile) {
  return tile == 0xcbu || tile == 0xfcu || tile == 0xf4u ||
         tile == 0xfdu || tile == 0xceu || tile == 0xcdu ||
         tile == 0xccu || tile == 0x48u;
}

static void secondary_tank_put(uint8_t row[BM_ATARI_WORLD_STRIDE],
                               int position, uint8_t tile) {
  if (position >= 0 && position < (int)BM_ATARI_WORLD_STRIDE &&
      !is_road_tile(row[position]))
    row[position] = tile;
}

/* TNK (SRT) and TNKC1 (CITY) share F1/E3..F2. */
static void draw_tank_secondary(BmWorldBuffer *buffer,
                                uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  unsigned count;
  int position;
  uint8_t tile,centre=buffer->generator.centre;
  if (buffer->tank2_delay) { --buffer->tank2_delay; return; }
  if (buffer->tank2_phase == 0u) {
    uint8_t wait;
    if (buffer->world_mode == BM_WORLD_CITY)
      /* TNKC1/TI */
      wait = (uint8_t)(buffer->area_type >= 4u || buffer->runway_delay < 2u ||
                       buffer->building_delay < 2u);
    else
      /* TNK/E9 */
      wait = (uint8_t)(buffer->normal_runway_delay < 4u ||
                       (buffer->bridge_delay < 2u && centre >= 30u &&
                        centre < 58u));
    /* F1: with DRTNK/TNKC imminent only SRT with FF>=45 may start. */
    if (!wait && buffer->tank_delay < 2u &&
        (buffer->area_type || centre < 45u)) wait = 1u;
    if (wait) { buffer->tank2_delay = 10u; return; }
    buffer->tank2_phase = 1u;
    buffer->object_control = 0u;   /* STA $B6 with A=$6A2=0 */
    buffer->object_marker = 4u;
    return;
  }

  /* E3 */
  --buffer->tank2_phase;
  buffer->tank2_delay = (uint8_t)((buffer_random(buffer) & 31u) | 16u);
  if (buffer->object_marker == 4u) buffer->object_control = 255u;

  if (buffer->area_type) {
    /* E4 */
    int base = buffer->road_target;
    if (buffer->road_target != buffer->road_position) position = base + 10;
    else if (buffer->player_x_atari >= 160u) position = base + 20;
    else if ((uint8_t)((buffer->player_y_atari >> 1) +
                       buffer->player_x_atari +
                       (buffer->player_y_atari & 1u)) < 120u)
      position = base - 18;
    else if (buffer_random(buffer) & 1u) position = base + 10;  /* .2 -> F5 */
    else position = base - 8;                                   /* F3 */
  } else {
    /* F4 */
    position = centre < 44u ? (int)centre + 10 : (int)centre - 24;
  }
  /* B5 receives base+$6A3, i.e. the first tile column. */
  { uint8_t gun_column = (uint8_t)position;
  for (count = 0; count < 3u; ++count) {
    if (buffer_random(buffer) & 1u) {
      tile = 199u;
      secondary_tank_put(row, position, tile);
      --position;
      tile = 198u;
    } else {
      tile = 72u;
      secondary_tank_put(row, position, tile);
      --position;
    }
    secondary_tank_put(row, position, tile);
    --position;
  }
  --position;
  tile = (buffer_random(buffer) & 1u) ? 49u : 177u;
  secondary_tank_put(row, position, tile);
  start_tank_gun_b5(buffer, gun_column);
  }
}

static void gun_put(uint8_t row[BM_ATARI_WORLD_STRIDE], int position,
                    uint8_t tile) {
  if (position >= 0 && position < (int)BM_ATARI_WORLD_STRIDE)
    row[position] = tile;
}

static void start_gun_entities(BmWorldBuffer *buffer,
                               uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  /* GUNC .2: $BF=45, tile 240 at $66A-10 with A2 (column+7) and at
     $66A+1 with B3 (column+7); both replace any active gun. */
  int first = (int)buffer->road_position - 10;
  int second = (int)buffer->road_position + 1;
  gun_put(row, first, 0xf0u);
  gun_put(row, second, 0xf0u);
  buffer->gun1_kind = 45u;
  start_tank_gun(buffer, (uint8_t)(first + 7));
  start_second_gun(buffer, (uint8_t)(second + 7));
}

/* EXXON.7 GUNC ($4A4F).  Area 4 periodically writes the two gun-emplacement
   markers into the generated world row.  Calls to the original moving-object
   setup code do not alter this row and belong to the later entity port. */
static void draw_gun_emplacements(BmWorldBuffer *buffer,
                                  uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t offset;
  int position;
  if (buffer->area_type != 4u) return;
  --buffer->gun_delay;
  if (buffer->gun_delay == 6u) {
    buffer->gun_delay = 30u;
    start_gun_entities(buffer, row);
    return;
  }
  if (buffer->gun_delay == 18u) {
    start_gun_entities(buffer, row);
    return;
  }
  if (buffer->gun_delay < 4u) return;
  if (buffer_random(buffer) & 1u) return;
  offset = (uint8_t)(buffer_random(buffer) & 15u);
  if (buffer_random(buffer) & 1u)
    position = (int)buffer->road_position + offset;
  else
    position = (int)buffer->road_position - 13 - offset;
  gun_put(row, position, 0xf1u);
  gun_put(row, position + 1, 0xf2u);
}

static uint8_t gun_direction(uint8_t origin_x, uint8_t player_x) {
  int difference = (int)origin_x - (int)player_x;
  if (difference >= 20) return 1u;
  if (difference >= 0) return 0u;
  if (difference >= -16) return 2u;
  return 4u;
}


static void move_gun_shot(BmWorldBuffer *buffer, uint8_t second) {
  uint8_t *shot_x = second ? &buffer->gun2_shot_x : &buffer->gun1_shot_x;
  uint8_t *shot_y = second ? &buffer->gun2_shot_y : &buffer->gun1_shot_y;
  uint8_t *direction = second ? &buffer->gun2_shot_direction :
                                &buffer->gun1_shot_direction;
  uint8_t *travel = second ? &buffer->gun2_shot_travel :
                             &buffer->gun1_shot_travel;
  uint8_t *explosion = second ? &buffer->gun2_shot_explosion :
                                &buffer->gun1_shot_explosion;
  uint8_t origin_x = second ? buffer->gun2_x : buffer->gun1_x;
  uint8_t origin_y = second ? buffer->gun2_y : buffer->gun1_y;
  if (*shot_y == 0u) {
    /* CKFGN/CKFM2: gun active, start delay over, 70 <= Y < 166. */
    if (!(second ? buffer->gun2_origin_x : buffer->gun1_origin_x) ||
        (second ? buffer->gun2_start_delay : buffer->gun1_start_delay) ||
        origin_y < 70u || origin_y >= 166u) return;
    if (second) {
      /* CKFM2: missile 2 is shared; only with $FD 0, 5 or 20. */
      if (buffer->object_slot && buffer->object_slot != 5u &&
          buffer->object_slot != 20u) return;
    } else if (buffer->gun1_kind == 0u) {
      /* FBRGN: no shot left of HPOS 65; from 185 (and half the time from
         161) TA first moves the gun 96 colour clocks left. */
      if (origin_x >= 185u || (origin_x >= 161u && (gun_random(buffer) & 1u))) {
        buffer->gun1_x = origin_x = (uint8_t)(origin_x - 96u);
      } else if (origin_x < 65u) return;
    }
    /* FTNKGN ($BF=45) and FBTGN ($BF=$BC) both enter A3: no HPOS limits. */
    *shot_x = origin_x;
    *shot_y = origin_y;
    *direction = second && buffer->area_type >= 6u && buffer->area_type < 8u ?
                 4u : gun_direction(origin_x, buffer->player_x_atari);
    *travel = (uint8_t)((gun_random(buffer) & 15u) + 15u);
    *explosion = 0u;
    buffer->sound_gun_fired = 1u;                /* $6AF=12: GGNSND */
    return;
  }
  if (*direction == 0u) --*shot_x;
  else if (*direction == 1u) *shot_x = (uint8_t)(*shot_x - 2u);
  else if (*direction == 4u) ++*shot_x;
  if (*travel) {
    --*travel;
    if (*travel) {
      *shot_y = (uint8_t)(*shot_y - 2u);
      return;
    }
    *explosion = 25u;
    buffer->sound_flak = 1u;                     /* SW/C9: XXX 2,$47 */
    return;
  }
  if (*explosion && --*explosion) return;
  *shot_y = 0u;
}

static void move_gun_entities(BmWorldBuffer *buffer) {
  /* SRT calls DRAW (which may replace the A2 origin) before FBRGN, while
     BRGPOS runs later through COMMON.  Gun 1 must therefore fire/update
     before its perspective origin moves. */
  move_gun_shot(buffer, 0u);
  if (buffer->gun1_origin_x) {
    if (buffer->gun1_start_delay) {
      --buffer->gun1_start_delay;
    } else if (++buffer->gun1_y == 210u) {
      buffer->gun1_origin_x = 0u;
    } else {
      --buffer->gun1_phase;
      if ((buffer->gun1_phase & 1u) == 0u) --buffer->gun1_x;
    }
  }
  /* The second missile retains the source order M2GPOS -> FM2GN. */
  if (buffer->gun2_origin_x) {
    if (buffer->gun2_start_delay) {
      --buffer->gun2_start_delay;
    } else if (++buffer->gun2_y == 210u || buffer->gun2_x < 50u) {
      buffer->gun2_origin_x = 0u;
    } else {
      --buffer->gun2_phase;
      if ((buffer->gun2_phase & 1u) == 0u) --buffer->gun2_x;
    }
  }
  move_gun_shot(buffer, 1u);
}

static void draw_building(BmWorldBuffer *buffer,
                          uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  unsigned count;
  int position;
  uint8_t tile;
  if (buffer->building_delay) { --buffer->building_delay; return; }
  if (buffer->building_inactive) {
    start_building(buffer);
    return;
  }
  ++buffer->building_row;
  position = buffer->building_position;
  if (buffer->building_row < buffer->building_height) {
    tile = (uint8_t)(84u + buffer->building_variant);
    building_put(row, position--, tile);
    tile = (uint8_t)(tile + 2u);
    if (buffer->building_row == 1u) {
      tile = (uint8_t)(87u + buffer->building_variant);
      for (count = 0; count < buffer->building_width; ++count)
        building_put(row, position--, tile);
      if (buffer->object_marker == 1u) buffer->object_control = 2u;
      return;
    }
    building_put(row, position--, tile);
    if (buffer->building_row != 2u) building_put(row, position--, tile);
    tile = (uint8_t)(tile + 1u);
    for (count = 0; count < buffer->building_width; ++count)
      building_put(row, position--, tile);
    return;
  }
  --buffer->building_position;
  position = buffer->building_position;
  if (buffer->building_row < buffer->building_roof_start) {
    tile = (uint8_t)(86u + buffer->building_variant);
    for (count = 1; count < buffer->building_height; ++count)
      building_put(row, position--, tile);
    tile = (uint8_t)(tile + 1u);
    for (count = 0; count < buffer->building_width; ++count)
      building_put(row, position--, tile);
    return;
  }
  ++buffer->building_end_row;
  if (buffer->building_end_row == buffer->building_height) {
    buffer->building_inactive = 255u;
    buffer->building_delay = (uint8_t)((buffer_random(buffer) & 31u) + 25u);
    /* BLDG/VP tests $C4 only after the final building row, then uses the
       current (already perspective-adjusted) $670+10 as GNSGHT anchor. */
    if(buffer->target_sight_request){
      bm_world_target_sight_start(buffer,2u,
        (uint8_t)(buffer->building_position+10u),17u);
      buffer->target_sight_request=0u;
    }
    return;
  }
  tile = (uint8_t)(86u + buffer->building_variant);
  for (count = buffer->building_end_row + 1u;
       count < buffer->building_height; ++count)
    building_put(row, position--, tile);
  building_put(row, position--, (uint8_t)(88u + buffer->building_variant));
  for (count = 1; count < buffer->building_width; ++count)
    building_put(row, position--, 85u);
  building_put(row, position, 89u);
}

/* CHBAS AAH (P2), AAI (P3) and XCPTLIM (P2 wreck), top row first. */
static const uint8_t capital_p2[13] = {0x08,0x38,0x29,0x39,0x33,0x03,0xff,
  0xff,0xb7,0xb6,0xb6,0xfc,0xfc};
static const uint8_t capital_p3[13] = {0x08,0x38,0x37,0x07,0x56,0x46,0xcc,
  0xfc,0x00,0x00,0x48,0x48,0x48};
static const uint8_t capital_wreck_p2[18] = {0x10,0x08,0x24,0x28,0x99,0x3c,
  0x2b,0x0a,0xd5,0x2a,0x32,0x85,0x58,0x35,0x8a,0x3b,0xd5,0xa2};

uint8_t bm_world_capital_covers(const BmWorldBuffer *b, int x, int y) {
  int scan = bm_pm_scanline(y), base, bit, clocks;
  if (!b->capital_y) return 0u;
  /* Below $6BF=196 the image slides on but rows past 198 are gone. */
  if (scan > 198 && b->capital_shift) return 0u;
  scan -= b->capital_shift;
  clocks = b->capital_size == 3u ? 4 : 2;      /* SIZEP2 1 or 3 */
  bit = (bm_pm_hpos(x) - b->capital_x) / clocks;
  if (bm_pm_hpos(x) < b->capital_x || bit > 7) return 0u;
  if (b->capital_wreck) {
    base = b->capital_y - 17;
    if (scan >= base && scan < base + 18 &&
        (capital_wreck_p2[scan - base] & (0x80u >> bit))) return 2u;
    return 0u;
  }
  base = b->capital_y - 12;
  if (scan >= base && scan < base + 13 &&
      (capital_p2[scan - base] & (0x80u >> bit))) return 2u;
  /* P3 keeps SIZEP3=1 and sits two lines higher. */
  bit = (bm_pm_hpos(x) - b->capital_x) / 2;
  base = b->capital_y - 14;
  if (bit <= 7 && scan >= base && scan < base + 13 &&
      (capital_p3[scan - base] & (0x80u >> bit))) return 3u;
  return 0u;
}

/* EXXON.13 CPTL with XCPTL (areas 6/7, CITY loop). */
static void capital_target_step(BmWorldBuffer *b) {
  static const uint8_t colour[3] = {50u, 112u, 96u};
  uint8_t bit, i;
  if (b->world_mode != BM_WORLD_CITY ||
      (b->area_type != 6u && b->area_type != 7u)) return;
  if (b->capital_wreck) {                    /* XCPTL */
    uint8_t a = --b->capital_flash;
    b->p2_colour = (a & 2u) ? 48u : 44u;
    if (a & 8u) {
      if (b->capital_size != 1u) { b->capital_x += 8u; b->capital_size = 1u; }
    } else if (b->capital_size != 3u) {
      b->capital_x -= 8u; b->capital_size = 3u;
    }
  }
  if (!b->capital_y) {
    if (b->area_type == 7u) return;
    i = b->area_delay == 63u ? 0u : b->area_delay == 32u ? 1u :
        b->area_delay == 1u ? 2u : 3u;
    if (i == 3u) return;
    bit = (uint8_t)(1u << i);
    if (b->capital_destroyed & bit) return;
    b->p2_colour = colour[i];
    b->object_slot = 20u;
    b->capital_y = 20u; b->capital_exit = 20u; b->capital_shift = 0u;
    b->capital_x = 176u; b->capital_size = 1u;
    return;
  }
  if (!(b->vbi_counter & 1u)) return;        /* odd $604 only */
  if (b->capital_y >= 196u) {
    if (!--b->capital_exit) {
      b->capital_y = 0u; b->capital_wreck = 0u; b->object_slot = 0u;
      return;
    }
    b->capital_shift = (uint8_t)(b->capital_shift + 2u);
  }
  --b->capital_x;
  if (b->capital_y < 196u) b->capital_y = (uint8_t)(b->capital_y + 2u);
}

/* EXXON.9 BLDGC/BLDGL: the capital (areas 5..7) adds one diagonal slice of
   its perspective buildings to every new row.  Y runs down from $67A. */
static void capital_put(uint8_t row[BM_ATARI_WORLD_STRIDE], uint8_t *y,
                        uint8_t value, unsigned count) {
  while (count--) {
    if (*y < BM_ATARI_WORLD_STRIDE) row[*y] = value;
    --*y;
  }
}

/* END: remember the finished building, start a new one. */
static void capital_end(BmWorldBuffer *b, BmCapitalSide *s, uint8_t right) {
  uint8_t r;
  s->prev_width = s->width;
  s->prev_bd = s->bd; s->prev_bb = s->bb; s->prev_bc = s->bc;
  s->cursor = right ? 61u : 24u;             /* $694 selects 61 or 61-37 */
  s->width = (uint8_t)((buffer_random(b) & 3u) + 3u);
  s->row = 0u;
  r = buffer_random(b);
  if ((r & 1u) && b->area_type == 6u) {      /* XY: the capital itself */
    switch ((r >> 1) & 3u) {
    case 0u: s->bd = 132u; s->bb = s->bc = 128u; break;   /* YB */
    case 1u: s->bd = s->bc = 6u; s->bb = 85u; break;      /* YC */
    case 2u: s->bd = s->bc = 134u; s->bb = 213u; break;   /* YD */
    default: s->bd = s->bc = 128u; s->bb = 209u; break;
    }
  } else if (!((r >> 1) & 3u)) {             /* YA */
    s->bd = s->bc = 0u; s->bb = 81u;
  } else {                                   /* XZ */
    s->bd = 4u; s->bc = s->bb = 0u;
  }
}

static void capital_side(BmWorldBuffer *b, uint8_t row[BM_ATARI_WORLD_STRIDE],
                         BmCapitalSide *s, uint8_t right) {
  uint8_t x, y, a;
  if (s->delay) { --s->delay; return; }      /* ZE */
  if (!s->phase) {                           /* YG */
    capital_end(b, s, right);
    s->width = 6u; s->phase = 6u;
  }
  x = ++s->row;                              /* YL */
  if (x < 4u) {                              /* YK */
    y = s->cursor;
    if (!s->active) {
      y = (uint8_t)(s->cursor - (x + 4u));   /* B77 */
      goto xv;
    }
    capital_put(row, &y, (uint8_t)(98u + s->bd), 1u);
    capital_put(row, &y, (uint8_t)(18u + s->bb), x == 1u ? 0u : x - 1u);
    a = (uint8_t)(100u + s->bc);             /* XYZ */
    capital_put(row, &y, a, 4u);
    if (s->phase != 6u) goto xv;
    goto yi;
  }
  if (!s->active) {
    y = (uint8_t)(s->cursor - (x + 4u));     /* B77 */
    if (x != 7u) goto yr;
    s->delay = 100u;
    return;
  }
  y = --s->cursor;                           /* YQ */
  capital_put(row, &y, (uint8_t)(18u + s->bb), 3u);
  a = (uint8_t)(100u + s->bc);               /* XYZ */
  capital_put(row, &y, a, 4u);
  if (x == 7u) s->phase = 7u;
  else if (s->phase == 7u) goto yr;          /* XQ/YJ */
yi:
  capital_put(row, &y, a, s->width);
  if (s->phase == 6u) return;                /* YH */
  if (b->area_type == 7u && b->area_delay < 13u) s->active = 0u;
  capital_end(b, s, right);                  /* YS */
  return;
yr:
  x = s->row;
  if (x != 6u)
    capital_put(row, &y, (uint8_t)(18u + s->prev_bb), x == 5u ? 1u : 2u);
  capital_put(row, &y, (uint8_t)(101u + s->prev_bd), 1u);   /* XS */
  capital_put(row, &y, 96u, (unsigned)s->prev_width + 3u);
  capital_put(row, &y, 97u, 1u);
  return;
xv:
  capital_put(row, &y, (uint8_t)(18u + s->prev_bb), 3u);
  capital_put(row, &y, (uint8_t)(100u + s->prev_bc), (unsigned)s->prev_width + 4u);
}

/* EXXON.15 CTYGN: guns at column 25 (tile 49) with A2 or B3 at column 32,
   area 5 at $B8=4, areas 6/7 at fixed $B8 values. */
static void capital_guns(BmWorldBuffer *b, uint8_t row[BM_ATARI_WORLD_STRIDE]) {
  uint8_t c = b->area_delay;
  if (b->area_type == 5u) {
    if (c != 4u) return;
  } else if (b->area_type == 6u || b->area_type == 7u) {
    if (c == 1u || c == 25u || c == 49u || c == 74u) {
      row[25] = 49u; start_second_gun(b, 32u); return;          /* B3 */
    }
    if (c != 13u && c != 37u && c != 61u) return;
  } else return;
  row[25] = 49u; start_tank_gun(b, 32u);                         /* A2 */
}

static void coarse_step(BmWorldBuffer *buffer) {
  uint16_t draw_base;
  uint16_t next_top;
  unsigned row;
  next_top = (uint16_t)(buffer->bottom_memory - 23u);
  buffer->bottom_memory = buffer->lms[BM_ATARI_PLAYFIELD_FETCH_ROWS - 1u];
  draw_base = (uint16_t)(buffer->bottom_memory - 20u);
  for (row = BM_ATARI_PLAYFIELD_FETCH_ROWS - 1u; row > 0; --row)
    buffer->lms[row] = (uint16_t)(buffer->lms[row - 1u] + 1u);
  buffer->lms[0] = (uint16_t)(buffer->top + 1u);
  buffer->top = next_top;
  buffer->coarse_event = 1u;
  /* VBIR/J: $FE counts coarse steps; reaching zero arms DRBR (F8=8). */
  if (buffer->bridge_delay && !--buffer->bridge_delay) {
    buffer->bridge_phase = 8u;
    buffer->bridge_skip = (uint8_t)(buffer_random(buffer) & 3u);
  }
  if (draw_base <= BM_WORLD_ARENA_BYTES - BM_ATARI_WORLD_STRIDE) {
    uint8_t *row = buffer->arena + draw_base;
    if (buffer->world_mode == BM_WORLD_CITY) {
      /* CITY: CLEAR, then AREA (DRAW in range, else B7/DROAD/TNKC/...). */
      memset(row + 1u,
             buffer->area_type >= 5u && buffer->area_type < 8u ? 0xfe : 0,
             BM_ATARI_WORLD_STRIDE - 1u);
      if (buffer->generator.centre >= 2u && buffer->generator.centre < 87u) {
        area_prepare(buffer);
        bm_world_generate_row(&buffer->generator, row, buffer_random, buffer);
      } else {
        area_advance(buffer);
        if (buffer->world_mode == BM_WORLD_SRT) goto srt; /* XC: JMP SRT */
        if (buffer->area_type < 5u || buffer->area_type >= 8u) {
          draw_crossroad(buffer, row);
          if (buffer->road_position == buffer->road_target) {
            draw_road_car(buffer, row);
            draw_building(buffer, row);
            draw_runway(buffer, row);
          }
          draw_tank(buffer, row);
          draw_tank_secondary(buffer, row);
          draw_gun_emplacements(buffer, row);
        }
      }
      /* BLDGC, BLDGL and CTYGN (areas 5..7, coarse steps only). */
      if (buffer->area_type >= 5u && buffer->area_type < 8u) {
        capital_side(buffer, row, &buffer->capital[0], 1u);
        capital_side(buffer, row, &buffer->capital[1], 0u);
        capital_guns(buffer, row);
      }
      /* CITY calls DRUN only while $638 is zero. */
      if (!buffer->normal_runway_delay) draw_friendly_runway(buffer, row);
    } else {
srt:
      bm_world_generate_row(&buffer->generator, row, buffer_random, buffer);
      if (buffer->prefill) {
        /* EXXON.0 EA loop: J, DRAW, MB, DRTR. */
        if (buffer->normal_runway_phase) draw_runway_rows(buffer, row);
      } else if (buffer->landing) {
        /* LAND: DRAW and DRUN only (DRTR follows through COMMON). */
        draw_friendly_runway(buffer, row);
      } else {
        draw_bridge(buffer, row);          /* DRBR */
        draw_moored_ship(buffer, row);     /* DRBT */
        draw_friendly_runway(buffer, row); /* DRUN */
        draw_tank(buffer, row);            /* DRTNK */
        draw_tank_secondary(buffer, row);  /* TNK */
      }
    }
    draw_trees(buffer, row);               /* COMMON: DRTR */
  }
  materialize_view(buffer);
}

void bm_world_load_eas(BmWorldBuffer *buffer, uint8_t table) {
  /* CHBAS EAS0..EAS9: $6C8 $6C9 $6CA $6CB $6CC $6CD $6CE. */
  static const uint8_t eas[6][7] = {
    {0x04u,0x02u,0x10u,0x14u,0x28u,0x09u,0x12u},
    {0x04u,0x04u,0x40u,0x19u,0x32u,0x0au,0x14u},
    {0x05u,0x06u,0x80u,0x1eu,0x3cu,0x0bu,0x16u},
    {0x06u,0x08u,0xb0u,0x23u,0x46u,0x0cu,0x18u},
    {0x06u,0x08u,0xe0u,0x28u,0x50u,0x0du,0x1au},
    {0x07u,0x0cu,0xffu,0x30u,0x60u,0x0eu,0x1cu}
  };
  const uint8_t *e = eas[table];
  buffer->wind_chance = e[0];
  buffer->wind_rows = e[1];
  buffer->enemy_fire_chance = e[2];
  buffer->gun_depth_bias = e[3];
  buffer->gun_depth_range = e[4];
  buffer->air_depth_bias = e[5];
  buffer->air_depth_range = e[6];
}

void bm_world_buffer_init(BmWorldBuffer *buffer, uint32_t seed) {
  unsigned row;
  memset(buffer, 0, sizeof(*buffer));
  buffer->rng = seed ? seed : 1u;
  buffer->gun_rng = (seed ? seed : 1u) ^ 0x47554e31u;
  buffer->tree_rng = (seed ? seed : 1u) ^ 0x54524545u;
  buffer->fine_phase = 3;
  /* EXXON.0 START: every game begins in the SRT river loop with river
     limits 16/75, $600=2 (left), $601=6, $603=255, $606=$610=1, FF=26. */
  bm_world_generator_init(&buffer->generator);
  buffer->generator.turn_preference = 1u;
  buffer->world_mode = BM_WORLD_SRT;
  buffer->area_type = 0u;                       /* $B7 */
  buffer->bridge_delay = 150u;                  /* $FE */
  buffer->moored_ship_delay =
    (uint8_t)((buffer_random(buffer) & 31u) + 15u);   /* $61D */
  buffer->normal_runway_phase = 26u;            /* $613: start airfield */
  buffer->normal_runway_position = 44u;         /* $65B */
  buffer->runway_slot = 26u;                    /* $609 */
  buffer->runway_interval = 0xa5u;              /* EAS0 -> $6CF (novice) */
  /* EXXON.0 seeds $650 from RANDOM: (value | 24) & 127. */
  buffer->tank_delay = (uint8_t)((buffer_random(buffer) | 24u) & 127u);
  buffer->tank2_delay = 44u;                    /* $6A1 */
  buffer->gun_delay = 25u;                      /* $6B1 */
  buffer->building_inactive = 255u;
  buffer->player_x_atari = 119u;
  buffer->player_plane_y = 165u;
  buffer->player_y_atari = 168u;
  buffer->target_state = 26u;                   /* $C4 */
  bm_world_load_eas(buffer, BM_EAS0);           /* START, novice */
  buffer->ckcol_enabled = 1u;                   /* START: $C5=1 */
  buffer->gprior = 1u;                          /* 623 */
  buffer->quake = -1;                           /* $6BE negative: none */
  load_hangar_glyphs(buffer);                   /* RESET: JSR DFHNGR */
  buffer->colbk = 0x00u;
  buffer->colpf0 = 0xc4u;
  buffer->colpf1 = 0x94u;
  buffer->colpf2 = 0xd2u;
  buffer->colpf3 = 0x04u;
  buffer->top = 0;
  buffer->bottom_memory = BM_WORLD_INITIAL_BOTTOM;
  for (row = 0; row < BM_ATARI_PLAYFIELD_FETCH_ROWS; ++row)
    buffer->lms[row] = (uint16_t)((row + 1u) * BM_ATARI_WORLD_STRIDE);
  /* EA: 25 times J (coarse scroll), DRAW, MB and DRTR; this draws the
     start airfield with its hangar. */
  buffer->prefill = 1u;
  for (row = 0; row < 25u; ++row) coarse_step(buffer);
  buffer->prefill = 0u;
  buffer->coarse_event = 0u;
  buffer->object_control = 255u;                /* $B6=255 */
  buffer->normal_runway_delay = 130u;           /* $638 */
  materialize_view(buffer);
}

uint8_t bm_world_after_takeoff(BmWorldBuffer *buffer,
                               uint8_t objectives_destroyed,
                               uint8_t objective_target) {
  uint8_t carry = 0u;
  if (!buffer) return BM_WORLD_SRT;
  /* .1: $6BE=$6CF (QUAKE) and $6CF+=7 below 220. */
  if (buffer->runway_interval < 220u) buffer->runway_interval += 7u;
  if (objectives_destroyed < 2u) {
    buffer->world_mode = BM_WORLD_SRT;
    return BM_WORLD_SRT;
  }
  if (objectives_destroyed >= objective_target) {
    ++buffer->area_progress;                    /* V0: INC $6AE */
    carry = 1u;                                 /* left by CMP $6C7 */
  } else {
    ++buffer->enemy_tracking;                   /* INC $6A7 */
    if (buffer_random(buffer) >= 160u) {        /* .18 -> .9 JMP SRT */
      buffer->world_mode = BM_WORLD_SRT;
      return BM_WORLD_SRT;
    }
  }
  /* ZN */
  buffer->generator.right_limit = 90u;          /* $667 */
  buffer->generator.left_limit = 1u;            /* $666 */
  buffer->road_position = 1u;                   /* $669 */
  buffer->road_target =
    (uint8_t)((buffer_random(buffer) & 15u) + 30u + carry); /* $66A */
  buffer->road_delay = (uint8_t)(buffer->road_target | 16u); /* $668 */
  buffer->runway_delay = buffer->road_delay;    /* $66B */
  buffer->runway_phase = 36u;                   /* $66C */
  buffer->area_transition = 0u;                 /* $677 */
  buffer->area_delay = 255u;                    /* $B8 */
  buffer->building_delay =
    (uint8_t)(buffer_random(buffer) & 31u);     /* $B2 */
  ++buffer->area_type;                          /* INC $B7 */
  buffer->prior_entry = 90u;                    /* $685 */
  buffer->prior_exit = 0u;                      /* $686 */
  /* AKA */
  if (buffer->target_state == 5u) {
    set_target_state(buffer, 0u);
    buffer->object_slot = 0u;
  }
  buffer->world_mode = BM_WORLD_CITY;
  return BM_WORLD_CITY;
}

static void animate_impact_tiles(BmWorldBuffer *b){
  unsigned i,offset[4]={b->grass_impact_offset,b->grass_impact_offset+1u,
    b->impact_other_offset,b->impact_other_offset+1u};
  uint8_t v[4],phase=(uint8_t)(b->grass_impact_ticks&4u);
  unsigned count=b->impact_mode==6u?2u:4u;
  for(i=0;i<count;++i)
    if(offset[i]<BM_WORLD_ARENA_BYTES&&b->arena[offset[i]]!=b->impact_expected[i]){
      b->grass_impact_ticks=0u;return;
    }
  if(b->impact_mode==3u){
    v[0]=v[2]=phase?0xf7u:0xf5u;v[1]=v[3]=phase?0xf8u:0xf6u;
  }else if(b->impact_mode==7u){
    if(b->grass_impact_ticks==1u||!phase)
      for(i=0;i<4u;++i)v[i]=b->grass_impact_ticks==1u?2u:0x44u;
    else {v[0]=0xf6u;v[1]=0xf5u;v[2]=0xf9u;v[3]=0xf5u;}
  }else {v[0]=v[2]=phase?0xfbu:0xf9u;v[1]=v[3]=phase?0xfbu:0xfau;}
  for(i=0;i<count;++i)if(offset[i]<BM_WORLD_ARENA_BYTES){
    b->arena[offset[i]]=v[i];b->impact_expected[i]=v[i];
  }
  materialize_view(b);
}

static void step_impacts(BmWorldBuffer *buffer) {
  /* MAX.1 XPLSN $1BB0/$1C0C: retain the original arena address while
     LMS pointers scroll past it.  Expiry leaves the last two glyphs. */
  if(buffer->grass_impact_ticks&&--buffer->grass_impact_ticks){
    unsigned offset=buffer->grass_impact_offset;
    if(buffer->impact_mode==3u||buffer->impact_mode==6u||
       buffer->impact_mode==7u||buffer->impact_mode==8u){
      animate_impact_tiles(buffer);
    }else if(buffer->impact_mode==4u||buffer->impact_mode==5u||buffer->impact_mode==9u){
      /* These routes animate character RAM, not screen codes. */
      uint8_t phase=(buffer->grass_impact_ticks&6u)==6u?0u:
        ((buffer->grass_impact_ticks&2u)?1u:2u);
      if(buffer->impact_mode==9u)buffer->hangar_impact_phase=phase;
      else if(buffer->impact_mode==5u)buffer->building_impact_phase=phase;
      else buffer->tank_impact_phase=phase;
    }else if((buffer->impact_mode==10u||buffer->impact_mode==15u)&&
             offset+1u<BM_WORLD_ARENA_BYTES){
      /* AEI ($FE/$5F crater) and XI (96/67 roof), two characters. */
      uint8_t v=(buffer->grass_impact_ticks&2u)?
        (buffer->impact_mode==10u?0xfeu:96u):(buffer->impact_mode==10u?0x5fu:67u);
      buffer->arena[offset]=buffer->arena[offset+1u]=v;
      materialize_view(buffer);
    }else if(buffer->impact_mode==0u&&offset<BM_WORLD_ARENA_BYTES&&
             (buffer->arena[offset]==0x43u||buffer->arena[offset]==0x44u)){
      buffer->arena[offset]=buffer->grass_impact_ticks==1u?0x02u:
        ((buffer->grass_impact_ticks&4u)?0x43u:0x44u);
      materialize_view(buffer);
    }else if((buffer->impact_mode==1u||buffer->impact_mode==2u)&&offset+1u<BM_WORLD_ARENA_BYTES&&
       (buffer->arena[offset]==0x40u||buffer->arena[offset]==0x3eu)){
      uint8_t phase=(uint8_t)(buffer->grass_impact_ticks&4u);
      buffer->arena[offset]=phase?0x40u:0x3eu;
      if(buffer->impact_mode==1u&&buffer->arena[offset+1u]!=0x0eu)
        buffer->arena[offset+1u]=phase?0x42u:0x41u;
      materialize_view(buffer);
    }else buffer->grass_impact_ticks=0u;
  }
}

void bm_world_buffer_step(BmWorldBuffer *buffer) {
  /* VBIR decrements $604 and scrolls only on even parity.  scroll_clock
     mirrors bit 0 so tests can still choose the next update's parity. */
  buffer->vbi_counter = (uint8_t)((buffer->vbi_counter & 0xfeu) |
                                  (buffer->scroll_clock & 1u));
  --buffer->vbi_counter;
  ++buffer->frame_counter;
  buffer->scroll_clock = (uint8_t)(buffer->vbi_counter & 1u);
#if BM_ATARI_SCROLL
  buffer->scroll_event=(uint8_t)!buffer->scroll_clock;
#else
  buffer->scroll_event=1u;
#endif
  buffer->coarse_event=0u;
  if(buffer->impact_flash_ticks)--buffer->impact_flash_ticks;
  /* VBI: fine/coarse scroll; the coarse step draws the new row (DRAW, DRBR,
     DRBT, DRUN, DRTNK, TNK or the CITY equivalents, then DRTR). */
  if(buffer->scroll_event){
    if (buffer->fine_phase) --buffer->fine_phase;
    else {
      buffer->fine_phase = 3;
      coarse_step(buffer);
    }
  }
  /* Main loop: PLBCK/PLFWD, SHIP, COMMON (TRGT, XPLSN, guns), FBTGN, DRP23.
     The LAND and MJ loops run none of PLBCK/PLFWD/SHIP/DRP23. */
  if (!buffer->landing) {
    move_enemy_fighter(buffer);
    move_normal_ship(buffer);
  }
  capital_target_step(buffer);
  /* QUAKE (COMMON, MAX.1 $1C8B): while $6BE>=0 count down and use
     HSCROL=F1-1 for this frame when F1 is non-zero. */
  buffer->quake_shift = 0u;
  if (buffer->quake >= 0) {
    --buffer->quake;
    buffer->quake_shift = (uint8_t)(buffer->fine_phase != 0u);
  }
  /* MJ calls no COMMON: no TRGT and no guns, only XPLSN. */
  if (buffer->landing != BM_WORLD_ROLLOUT) target_step(buffer);
  step_impacts(buffer);
  if (buffer->landing == BM_WORLD_ROLLOUT) return;
  move_gun_entities(buffer);
  if (!buffer->landing) move_river_car(buffer);
}

static uint8_t bomb_target_code(uint8_t code) {
  static const uint8_t targets[]={
    /* MAX.2 AAF: hangar target glyphs. */
    0x95u,0x7eu,0x96u,0x97u,0x98u,0x99u,0x9bu,
    0x1cu,0x1du,0x1eu,0x1fu,0x20u,0x9au,
    /* MAX.2 AAG: gun and tank target glyphs. */
    0xecu,0xedu,0xeeu,0xefu,0xf0u,0xf1u,0xf2u,
    0xc6u,0xc7u,0x31u,0xb1u,
    /* AAA: large tank/body tiles, CKCHAR -> W4 -> mode 4. */
    0x49u,0xc9u,0x4au,0xcau,0x7fu,0xffu,0xc5u,0x45u,0x9cu,0xbfu
  };
  unsigned index;
  for(index=0;index<sizeof(targets);++index)
    if(code==targets[index])return 1u;
  return 0u;
}

static uint8_t bomb_object_mode(uint8_t code){
  static const uint8_t hangar[]={0x95,0x7e,0x96,0x97,0x98,0x99,0x9b,
    0x1c,0x1d,0x1e,0x1f,0x20,0x9a};
  static const uint8_t tank[]={0x49,0xc9,0x4a,0xca,0x7f,0xff,0xc5,0x45,0x9c,0xbf};
  unsigned i;
  for(i=0;i<sizeof(hangar);++i)if(code==hangar[i])return 9u;
  for(i=0;i<sizeof(tank);++i)if(code==tank[i])return 4u;
  return 1u;
}

static uint8_t bomb_water_code(const BmWorldBuffer *buffer,uint8_t code){
  /* XPLSN $1B17/$1B47: direct water cells.  B5/B6 are AAE road targets,
     not water; bank-specific pointer adjustments remain separate. */
  return (uint8_t)((buffer->area_type==0u&&code==0x02u)||
                   code==0xafu);
}

static uint8_t additional_impact_mode(uint8_t code){
  static const uint8_t building[]={0x58,0x56,0x54,0x5e,0x5c,0x5a,0xde,
    0xdc,0xda,0x57,0x5d,0xdd,0x59,0x55};
  static const uint8_t bridge[]={0xb9,0xab,0xac,0xae,0xa9,0xb2,0xdb,0xaa,0xb7,0xb8};
  static const uint8_t road[]={0xfd,0xce,0xfc,0xcd,0xcc,0xcb,0xb5,0xb6};
  unsigned i;
  for(i=0;i<sizeof(building);++i)if(code==building[i])return 5u;
  for(i=0;i<sizeof(bridge);++i)if(code==bridge[i])return 3u;
  for(i=0;i<sizeof(road);++i)if(code==road[i])return 6u;
  if(code>=0xbau&&code<=0xbdu)return 7u;
  if(code>=0xd0u&&code<=0xd3u)return 8u;
  return 0u;
}

/* EXXON.14 SHBT (MAX.1 $4507), called by TRIG after every bullet step while
   F6-F5<28.  ADA maps ($61E,$F9) to a character: tanks and cars score S10,
   a GUNC gun also silences both guns ($661, $698), and a moored boat needs
   $6DE hits before S50 and the end of its gun.  ADP then ends the bullet
   (HG), flashes (ABA) and Z7 starts XPLSN with $624=1 or 7. */
uint8_t bm_world_shot_ground(BmWorldBuffer *buffer, uint8_t hpos,
                             uint8_t scanline) {
  int row = ((scanline - 8) >> 3) - 1, column = ((hpos - 26) >> 2) - 2;
  int offset;
  uint8_t code, mode = 1u, points = 10u;
  if (row < 0 || row >= (int)BM_ATARI_PLAYFIELD_FETCH_ROWS) return 0u;
  offset = (int)buffer->lms[row] + buffer->source_column + column;
  if (offset < 1 || offset + 1 >= (int)BM_WORLD_ARENA_BYTES) return 0u;
  code = buffer->arena[offset];
  switch (code) {
  case 198u: case 236u: case 238u: break;
  case 199u: case 237u: case 239u: --offset; break;
  case 49u: case 177u:
    buffer->gun1_origin_x = 0u;                                 /* $661 */
    buffer->gun2_origin_x = 0u;                                 /* $698 */
    --offset; break;
  case 189u: --offset; /* fall through */
  case 188u:
    if (--buffer->boat_hits) return 0u;
    buffer->gun1_origin_x = 0u;                                 /* $661 */
    mode = 7u; points = 50u; break;
  default: return 0u;
  }
  buffer->impact_mode = mode;
  buffer->grass_impact_offset = (uint16_t)offset;
  buffer->impact_other_offset = BM_WORLD_ARENA_BYTES;
  if (mode == 7u && row > 0) {
    /* C7/C8 = DL-3,X: the row above. */
    int other = (int)buffer->lms[row - 1] + buffer->source_column +
                (offset - (int)buffer->lms[row] - buffer->source_column);
    if (other >= 0 && other + 1 < (int)BM_WORLD_ARENA_BYTES)
      buffer->impact_other_offset = (uint16_t)other;
  }
  /* Z7: $622=$BB-$639 with $639=F9+30. */
  buffer->grass_impact_ticks = (uint8_t)(0xbbu - (uint8_t)(scanline + 30u));
  if (!buffer->impact_flash_ticks) buffer->impact_flash_ticks = 12u;
  if (mode == 1u) buffer->arena[offset] = 0x40u;
  {
    unsigned i, addresses[4];
    addresses[0] = (unsigned)offset; addresses[1] = (unsigned)offset + 1u;
    addresses[2] = buffer->impact_other_offset;
    addresses[3] = addresses[2] + 1u;
    for (i = 0; i < 4u; ++i) buffer->impact_expected[i] =
      addresses[i] < BM_WORLD_ARENA_BYTES ? buffer->arena[addresses[i]] : 0u;
  }
  materialize_view(buffer);
  return points;
}

uint8_t bm_world_bomb_impact(BmWorldBuffer *buffer, int16_t screen_x,
                             int16_t screen_y) {
  int tile_x,tile_y,search_x,hit_x=-1,offset;
  if(!buffer)return 0u;
  buffer->bomb_impact_handled=0u;
  buffer->bomb_impact_code=0u;
  buffer->bomb_impact_hit=0u;
  buffer->bomb_impact_water=0u;
  /* CKCHAR tests bridges before falling through to Z3's nearby targets.
     Do not let the legacy expanded ADA search steal a visible bridge
     impact and turn it into an AAG land/fire effect. */
  tile_x=(screen_x+8-buffer->fine_phase*2)/8;
  tile_y=(screen_y+buffer->fine_phase*2)/8;
  if(tile_x>=0&&tile_x+buffer->source_column<BM_ATARI_PLAYFIELD_FETCH_COLUMNS&&
     tile_y>=0&&tile_y<BM_ATARI_PLAYFIELD_FETCH_ROWS&&
     additional_impact_mode(buffer->rows[tile_y][tile_x+buffer->source_column])==3u)
    goto terrain_hit;
  /* Literal ADA (EXXON.14): column ((HPOS-26)>>2)-2 from LMS and display
     list entry (Y-8)>>3, whose first mode-4 LMS is entry one.  rows[]
     starts at LMS+BM_VIEW_LMS_OFFSET. */
  tile_x=((bm_pm_hpos(screen_x)-26)>>2)-2-BM_VIEW_LMS_OFFSET;
  tile_y=((bm_pm_scanline(screen_y)-8)>>3)-1;
  if(tile_y<0||tile_y>=(int)BM_ATARI_PLAYFIELD_FETCH_ROWS)return 0u;
  if(tile_x>=0&&tile_x<(int)BM_ATARI_PLAYFIELD_FETCH_COLUMNS)
    buffer->bomb_impact_code=buffer->rows[tile_y][tile_x];
  buffer->bomb_impact_water=bomb_water_code(buffer,buffer->bomb_impact_code);
  /* Z3 in the capital: AFB's $6EB.  1 (between buildings, $61F-80) only
     explodes on a roof (96: S30, $624=15); any other $6EB means no
     explosion (.13 / DT).  With $6EB=0 the street $FE gets a crater
     ($624=10, no score). */
  if(buffer->bomb_zone||buffer->bomb_impact_code==0xfeu){
    uint8_t mode=buffer->bomb_zone?15u:10u;
    buffer->bomb_impact_handled=1u;
    if(buffer->bomb_zone&&
       (buffer->bomb_zone!=1u||buffer->bomb_impact_code!=96u))return 0u;
    offset=(int)buffer->lms[tile_y]+BM_VIEW_LMS_OFFSET+buffer->source_column+tile_x;
    if(tile_x<0||offset<0||offset+1>=(int)BM_WORLD_ARENA_BYTES)return 0u;
    buffer->impact_mode=mode;
    buffer->grass_impact_offset=(uint16_t)offset;
    buffer->grass_impact_ticks=(uint8_t)(0xbbu-bm_pm_scanline(screen_y));
    if(mode==15u){buffer->bomb_impact_hit=1u;return 1u;}
    return 0u;
  }
  /* Z3 checks the impact character and up to two neighbouring characters. */
  for(search_x=tile_x-2;search_x<=tile_x+2;++search_x){
    if(search_x<0||search_x>=(int)BM_ATARI_PLAYFIELD_FETCH_COLUMNS)continue;
    if(bomb_target_code(buffer->rows[tile_y][search_x])){
      hit_x=search_x;
      buffer->bomb_impact_code=buffer->rows[tile_y][search_x];
      break;
    }
  }
  if(hit_x<0){
terrain_hit:
    /* Terrain pixels use the inverse of bm_antic4_blit_playfield, rather
       than ADA's expanded target search coordinates. */
    tile_x=(screen_x+8-buffer->fine_phase*2)/8;
    tile_y=(screen_y+buffer->fine_phase*2)/8;
    if(tile_x<0||tile_x+1>=(int)BM_ATARI_PLAYFIELD_FETCH_COLUMNS||
       tile_y<0||tile_y+1>=(int)BM_ATARI_PLAYFIELD_FETCH_ROWS)return 0u;
    buffer->bomb_impact_code=buffer->rows[tile_y][tile_x+buffer->source_column];
    buffer->bomb_impact_water=bomb_water_code(buffer,buffer->bomb_impact_code);
    /* DRCAR's EC..EF (and the other AAG targets) must be tested at
       their rendered position too: ADA's legacy projection can select
       the next row in this preview. Z3 routes these to S30/ABA/U3. */
    if(bomb_target_code(buffer->bomb_impact_code)&&
       bomb_object_mode(buffer->bomb_impact_code)==1u){
      hit_x=tile_x;
      goto object_hit;
    }
    { uint8_t mode=additional_impact_mode(buffer->bomb_impact_code);
      if(mode){
        int other_row=tile_y-1;
        uint8_t code=buffer->bomb_impact_code;
        unsigned i,addresses[4];
        if(code==0xaau||code==0xb7u||code==0xb8u||
           code==0xbau||code==0xbbu||code==0xd0u||code==0xd1u)
          other_row=tile_y+1;
        if(code==0xbbu||code==0xbdu||code==0xd0u||code==0xd2u)--tile_x;
        offset=(int)buffer->lms[tile_y]+BM_VIEW_LMS_OFFSET+buffer->source_column+tile_x;
        if(offset<0||offset+1>=(int)BM_WORLD_ARENA_BYTES)return 0u;
        buffer->impact_mode=mode;buffer->bomb_impact_handled=1u;
        buffer->bomb_impact_hit=1u;
        buffer->grass_impact_offset=(uint16_t)offset;
        buffer->impact_other_offset=BM_WORLD_ARENA_BYTES;
        if(other_row>=0&&other_row<(int)BM_ATARI_PLAYFIELD_FETCH_ROWS){
          int other=(int)buffer->lms[other_row]+BM_VIEW_LMS_OFFSET+buffer->source_column+tile_x;
          if(other>=0&&other+1<(int)BM_WORLD_ARENA_BYTES)
            buffer->impact_other_offset=(uint16_t)other;
        }
        buffer->grass_impact_ticks=(uint8_t)(0xbbu-bm_pm_scanline(screen_y));
        if(!buffer->impact_flash_ticks)buffer->impact_flash_ticks=12u;
        addresses[0]=(unsigned)offset;addresses[1]=(unsigned)offset+1u;
        addresses[2]=buffer->impact_other_offset;addresses[3]=addresses[2]+1u;
        for(i=0;i<4u;++i)buffer->impact_expected[i]=
          addresses[i]<BM_WORLD_ARENA_BYTES?buffer->arena[addresses[i]]:0u;
        return 1u;
      }
    }
    /* XPLSN $1B47/$1B4B falls through to ordinary land for EVERY code
       not claimed by CKCHAR or the water/bank branches. This includes
       runway edges and previously damaged terrain, not just grass/trees. */
    {
      offset=(int)buffer->lms[tile_y]+BM_VIEW_LMS_OFFSET+buffer->source_column+tile_x;
      /* XPLSN $1B8F: move the pointer to the following LMS row for
         the water-side bank codes.  It is not arena + row stride. */
      if(buffer->bomb_impact_code==3u||buffer->bomb_impact_code==8u||
         buffer->bomb_impact_code==13u){
        offset=(int)buffer->lms[tile_y+1]+BM_VIEW_LMS_OFFSET+buffer->source_column+tile_x;
        buffer->bomb_impact_water=1u;
      }
      /* $1BA2 increments the pointer only for 5/12; the other bank
         codes enter mode 2 directly and modify just one cell. */
      if(buffer->bomb_impact_code==5u||buffer->bomb_impact_code==12u)++offset;
      if(offset>=0&&offset+1<(int)BM_WORLD_ARENA_BYTES){
        /* XPLSN $1B4B: first write $40.  CMP #2 on grass leaves carry
           clear, so SBC computes $BA - impact_scanline ($639). */
        buffer->grass_impact_offset=(uint16_t)offset;
        buffer->impact_mode=buffer->bomb_impact_water?0u:1u;
        if(!buffer->bomb_impact_water&&
           (buffer->bomb_impact_code==4u||buffer->bomb_impact_code==5u||
            buffer->bomb_impact_code==7u||buffer->bomb_impact_code==10u||
            buffer->bomb_impact_code==12u||buffer->bomb_impact_code==14u||
            buffer->bomb_impact_code==15u||buffer->bomb_impact_code==16u))
          buffer->impact_mode=2u;
        buffer->bomb_impact_handled=1u;
        buffer->grass_impact_ticks=buffer->bomb_impact_water?18u:
          (uint8_t)((buffer->bomb_impact_code>=2u?0xbbu:0xbau)-bm_pm_scanline(screen_y));
        buffer->arena[offset]=buffer->bomb_impact_water?0x43u:0x40u;
        materialize_view(buffer);
      }
    }
    return 0u;
  }
object_hit:
  offset=(int)buffer->lms[tile_y]+BM_VIEW_LMS_OFFSET+buffer->source_column+hit_x;
  if(offset<0||offset+1>=(int)BM_WORLD_ARENA_BYTES)return 0u;
  buffer->bomb_impact_handled=1u;
  if(bomb_object_mode(buffer->bomb_impact_code)==9u){
    if(!buffer->hangar_target_available)return 0u;
    buffer->hangar_target_available=0u;
  }
  buffer->impact_mode=bomb_object_mode(buffer->bomb_impact_code);
  buffer->grass_impact_offset=(uint16_t)offset;
  buffer->grass_impact_ticks=(uint8_t)(0xbbu-bm_pm_scanline(screen_y));
  if(!buffer->impact_flash_ticks)buffer->impact_flash_ticks=12u;
  buffer->bomb_impact_hit=1u;
  buffer->bomb_impact_water=0u;
  /* Z3/AAG -> U3 ($1B4B) uses the ordinary land pair.  Hangar Z5 and
     large-tank mode 4 alter glyphs while retaining their screen layout. */
  if(buffer->impact_mode==1u)buffer->arena[offset]=0x40u;
  materialize_view(buffer);
  return 1u;
}

void bm_world_target_sight_start(BmWorldBuffer *buffer,uint8_t objective_type,
                                 uint8_t world_column,uint8_t source_y){
  if(!buffer||objective_type<1u||objective_type>3u)return;
  /* GNSGHT stores the generator's column after two ASLs in C0/HPOSP2. */
  buffer->target_sight_x=(uint8_t)(world_column*4u);
  buffer->target_sight_y=source_y;
  buffer->target_sight_type=objective_type;
  buffer->target_sight_active=1u;
  set_target_state(buffer,253u);   /* GNSGHT: $C4=253, INC $FD */
  ++buffer->object_slot;
}

void bm_world_target_sight_request(BmWorldBuffer *buffer){
  /* TRGT/Q2: $C4=5 and $FD=5. */
  if(buffer&&!buffer->target_sight_active){
    set_target_state(buffer,5u);
    buffer->object_slot=5u;
  }
}

static uint8_t runway_tile(uint8_t code){
  /* DRUN: 145/147 with 252 normally and 148 on its two end rows.
     ENRUN: CB, ten FC/F4 surface/edge characters. */
  return code==0x91u||code==0x93u||code==0x94u||code==0xfcu||
         code==0xcbu||code==0xf4u;
}

uint8_t bm_world_runway_visible(const BmWorldBuffer *buffer){
  unsigned row,column,end=(unsigned)buffer->source_column+
    BM_ATARI_PLAYFIELD_FETCH_COLUMNS;
  if(end>BM_ATARI_WORLD_STRIDE)end=BM_ATARI_WORLD_STRIDE;
  for(row=0;row<BM_ATARI_PLAYFIELD_FETCH_ROWS;++row)
    for(column=buffer->source_column;
        column<end;
        ++column)
      if(runway_tile(buffer->rows[row][column]))return 1u;
  return 0u;
}

uint8_t bm_world_runway_collision(const BmWorldBuffer *buffer,
                                  int16_t screen_x,int16_t screen_y){
  int world_x=(int)screen_x-(int)buffer->fine_phase*2+8;
  int world_y=(int)screen_y+(int)buffer->fine_phase*2;
  int column,row;
  if(world_x<0||world_y<0)return 0u;
  column=world_x/8+(int)buffer->source_column;
  row=world_y/8;
  if(column<0||column>=BM_ATARI_WORLD_STRIDE||row<0||
     row>=BM_ATARI_PLAYFIELD_FETCH_ROWS)return 0u;
  return runway_tile(buffer->rows[row][column])?8u:0u;
}

uint8_t bm_world_runway_mask_collision(const BmWorldBuffer *buffer,
                                       const uint8_t *pixels,
                                       uint16_t width,uint16_t height,
                                       uint16_t stride,int16_t screen_x,
                                       int16_t screen_y,int16_t anchor_x,
                                       int16_t anchor_y){
  uint16_t x,y;
  if(!pixels||stride<width)return 0u;
  for(y=0;y<height;++y)for(x=0;x<width;++x){
    if(!pixels[(size_t)y*stride+x])continue;
    if(bm_world_runway_collision(buffer,
         (int16_t)(screen_x-anchor_x+(int16_t)x),
         (int16_t)(screen_y-anchor_y+(int16_t)y))==8u)return 8u;
  }
  return 0u;
}

uint8_t bm_world_playfield_mask_collision(const BmWorldBuffer *buffer,
                                          const uint8_t *pixels,
                                          uint16_t width,uint16_t height,
                                          uint16_t stride,int16_t screen_x,
                                          int16_t screen_y,int16_t anchor_x,
                                          int16_t anchor_y){
  const uint8_t *charset;
  uint16_t x,y;
  uint8_t latch=0u;
  if(!buffer||!pixels||stride<width)return 0u;
  charset=buffer->tank_charset_loaded?bm_asset_max3_tank_charset:
                                        bm_asset_max3_hangar_charset;
  for(y=0;y<height;++y)for(x=0;x<width;++x){
    int sx,sy,column,row,pair;
    uint8_t code,glyph,value;
    if(!pixels[(size_t)y*stride+x])continue;
    sx=(int)screen_x-anchor_x+(int)x-(int)buffer->fine_phase*2+8;
    sy=(int)screen_y-anchor_y+(int)y+(int)buffer->fine_phase*2;
    if(sx<0||sy<0)continue;
    column=sx/8+(int)buffer->source_column;row=sy/8;
    if(column<0||column>=BM_ATARI_WORLD_STRIDE||row<0||
       row>=BM_ATARI_PLAYFIELD_FETCH_ROWS)continue;
    code=buffer->rows[row][column];
    glyph=charset[(size_t)(code&0x7fu)*8u+(unsigned)(sy&7)];
    pair=(sx&7)/2;
    value=(uint8_t)((glyph>>(6-pair*2))&3u);
    if(value==1u)latch|=1u;
    else if(value==2u)latch|=2u;
    else if(value==3u)latch|=(uint8_t)((code&0x80u)?8u:4u);
  }
  return latch;
}

void bm_world_crash_step(BmWorldBuffer *buffer) {
  if (buffer->impact_flash_ticks) --buffer->impact_flash_ticks;
  step_impacts(buffer);
}

/* EXXON.15 PBMB on the ground: OG puts a PLBCK fighter at C1=C2=186, then
   $640=4, $62F=70, $6A8=1 and $FD=254 with C0 chosen near the player. */
void bm_world_airfield_attack_spawn(BmWorldBuffer *b, uint8_t x) {
  b->object_slot = 254u;
  b->enemy_state = 4u;
  b->enemy_timer = 70u;
  b->enemy_slow = 1u;
  b->enemy_fighter_turn = 0u;
  b->enemy_fighter_y = b->enemy_fighter_shadow_y = 186u;
  b->enemy_fighter_x = x;
  b->p2_colour = take_bonus_target(b) ? 0x7cu : 44u;
  b->enemy_fighter_active = 1u;
  b->enemy_fighter_forward = 0u;
  b->enemy_fighter_destroyed = 0u;
  b->enemy_body_on = 1u;
  b->enemy_shadow_on = 0u;
  b->enemy_in_line = 0u;
}

/* PBMB .2: JSR L6 for the attacker ($FD=254). */
void bm_world_airfield_attack_step(BmWorldBuffer *b) {
  if (b->object_slot == 254u) plbck_states(b);
}
