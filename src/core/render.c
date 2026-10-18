#include "bm_game.h"
#include "bm_renderer.h"
#include "bm_profile.h"
uint32_t (*bm_profile_cycles)(void);
uint32_t bm_profile_total[BM_PROFILE_SLOTS],bm_profile_count[BM_PROFILE_SLOTS];
#include "bm_sprite.h"
#include "bm_tile.h"
#include "bm_chbas_assets.h"
#include "bm_geometry.h"
#include "bm_status_font.h"
#ifndef BM_DEBUG_HUD
#define BM_DEBUG_HUD 0
#endif
#include <string.h>
#ifndef BM_CLASSIC_TEST_PAGE
#define BM_CLASSIC_TEST_PAGE 0
#endif
#ifndef BM_ATARI_FLIGHT
#define BM_ATARI_FLIGHT 0
#endif
BmSpriteList bm_sprite_list;
uint32_t bm_sprite_hd_mask;
/* Records an object; 1 if the classic renderer draws it itself. */
static int sprite(unsigned kind,unsigned variant,int x,int y,unsigned colour,
                  unsigned behind){
  if(bm_sprite_list.count<BM_SPRITE_DRAW_MAX){
    BmSpriteDraw *d=&bm_sprite_list.draw[bm_sprite_list.count++];
    d->kind=(uint8_t)kind;d->variant=(uint8_t)variant;d->colour=(uint8_t)colour;
    d->behind=(uint8_t)behind;d->x=(int16_t)x;d->y=(int16_t)y;
  }
  return !(bm_sprite_hd_mask&(1u<<kind));
}
static void px(BmFramebuffer *f,int x,int y,uint8_t c){
  if((unsigned)x<BM_SCREEN_WIDTH&&(unsigned)y<BM_SCREEN_HEIGHT)f->pixels[y*BM_SCREEN_WIDTH+x]=c;
}
static void draw_gun_shot(BmFramebuffer *f,uint8_t atari_x,uint8_t atari_y,
                          uint8_t explosion,uint8_t colour){
  /* Missile 3/2 HPOS and scanline from A2/B3 and FBRGN/FM2GN. */
  int x=bm_pm_x(atari_x);
  int y=bm_pm_y(atari_y);
  int dx,dy,width=4,height=2;
  if(!atari_y)return;
  /* The burst's odd rows start two pixels further left: its box does. */
  if(!sprite(BM_SPRITE_GUN_SHOT,explosion?1u:0u,explosion?x-2:x,explosion?y-4:y,0u,0u))return;
  if(explosion){
    /* FBRGN/FM2GN replace the two missile scanlines with four staggered
       scanlines for 25 frames.  Adjacent Atari missile lanes are one
       color-clock apart, hence the two-pixel horizontal stagger here. */
    for(dy=0;dy<4;dy++){
      int burst_x=x-((dy&1)?2:0);
      for(dx=0;dx<4;dx++)px(f,burst_x+dx,y-4+dy,colour);
    }
    return;
  }
  for(dy=0;dy<height;dy++)for(dx=0;dx<width;dx++)px(f,x+dx,y+dy,colour);
}
#if BM_CLASSIC_TEST_PAGE
static void draw_screen_shot(BmFramebuffer *f,int x,int y,
                             uint8_t explosion,uint8_t colour){
  int dx,dy;
  if(!y)return;
  if(explosion){
    for(dy=0;dy<4;dy++)for(dx=0;dx<4;dx++)
      px(f,(int)x+dx-((dy&1)?2:0),(int)y-4+dy,colour);
  }else{
    for(dy=0;dy<2;dy++)for(dx=0;dx<4;dx++)
      px(f,(int)x+dx,(int)y+dy,colour);
  }
}
#endif
#if BM_CLASSIC_TEST_PAGE
static const char *debug_pattern(char c){
  switch(c){
    case '0':return "111101101101111";case '1':return "010110010010111";
    case '2':return "111001111100111";case '3':return "111001111001111";
    case '4':return "101101111001001";case '5':return "111100111001111";
    case '6':return "111100111101111";case '7':return "111001010010010";
    case '8':return "111101111101111";case '9':return "111101111001111";
    case 'A':return "010101111101101";case 'B':return "110101110101110";
    case 'C':return "111100100100111";case 'E':return "111100110100111";
    case 'F':return "111100110100100";case 'H':return "101101111101101";
    case 'G':return "111100101101111";
    case 'I':return "111010010010111";case 'P':return "110101110100100";
    case 'R':return "110101110101101";case 'T':return "111010010010010";
    case 'U':return "101101101101111";case 'V':return "101101101101010";
    case 'W':return "101101111111101";case 'X':return "101101010101101";
    case 'D':return "110101101101110";case 'J':return "001001001101111";
    case 'K':return "101101110101101";case 'L':return "100100100100111";
    case 'M':return "101111111101101";case 'N':return "110101101101101";
    case 'O':return "010101101101010";case 'Q':return "111101101111001";
    case 'S':return "111100111001111";case 'Y':return "101101010010010";
    case 'Z':return "111001010100111";
    case ':':return "000010000010000";case '/':return "001001010100100";
    default:return "000000000000000";
  }
}
static int debug_char(BmFramebuffer*f,int x,int y,char c){
  const char *pattern=debug_pattern(c);int row,column;
  for(row=0;row<5;row++)for(column=0;column<3;column++)
    if(pattern[row*3+column]=='1')px(f,x+column,y+row,26);
  return x+4;
}
static int debug_text(BmFramebuffer*f,int x,int y,const char*s){
  while(*s) x=debug_char(f,x,y,*s++);
  return x;
}
static int debug_hex(BmFramebuffer*f,int x,int y,uint32_t value,unsigned digits){
  static const char hex[]="0123456789ABCDEF";int shift;
  for(shift=(int)(digits-1u)*4;shift>=0;shift-=4)
    x=debug_char(f,x,y,hex[(value>>(unsigned)shift)&15u]);
  return x;
}
static int debug_decimal(BmFramebuffer*f,int x,int y,unsigned value){
  x=debug_char(f,x,y,(char)('0'+value/100u%10u));
  x=debug_char(f,x,y,(char)('0'+value/10u%10u));
  return debug_char(f,x,y,(char)('0'+value%10u));
}
static void rnkx_text(uint8_t *status,const BmGame *g){
  /* RNKX: AFH clears SC+$649..$674 and writes RNKI at SC+$64B.  The latter
     is exactly two character cells into that 44-byte status strip. */
  static const uint8_t rnki[11]={
    0xb2u,0xa1u,0xaeu,0xabu,0x9au,0x80u,
    0xa3u,0xacu,0xa1u,0xb3u,0xb3u
  };
  static const uint8_t kmkzi[16]={
    0xabu,0xa1u,0xadu,0xa9u,0xabu,0xa1u,0xbau,0xa5u,
    0x80u,0xb4u,0xb2u,0xa1u,0xa9u,0xaeu,0xa5u,0xa5u
  };
  static const uint8_t rswi[14]={
    0xb2u,0xb5u,0xaeu,0xb7u,0xa1u,0xb9u,0x80u,
    0xb3u,0xb7u,0xa5u,0xa5u,0xb0u,0xa5u,0xb2u
  };
  static const uint8_t acdi[9]={
    0xa1u,0xa9u,0xb2u,0x80u,0xa3u,0xa1u,0xa4u,0xa5u,0xb4u
  };
  static const uint8_t ftgi[12]={
    0x26u,0x2cu,0x39u,0x29u,0x2eu,0x27u,
    0x00u,0x34u,0x29u,0x27u,0x25u,0x32u
  };
  unsigned score_class=g->score>=20000u?255u:g->score/100u;
  memcpy(status+2,rnki,sizeof(rnki));
  /* CLCSC discards the lowest displayed score digit and converts the rest;
     with BmGame's point-valued score this is score/100, saturated to $FF. */
  if(score_class<2u&&g->flight_fuel.active){
    status[14]=0x91u;
    memcpy(status+25,kmkzi,sizeof(kmkzi));
  }else{
    /* RNKX .88: (($6E6*2+$6E7)*4)+CLCSC+$6AC, with 6502 carry. */
    unsigned total=(unsigned)g->mission_difficulty*8u+
      (unsigned)g->mission_rank_level*4u+score_class+
      (unsigned)g->mission_targets_destroyed;
    unsigned value=total&255u;
    if(total>255u||g->mission_phase==7u){
      status[14]=0x94u;
      memcpy(status+26,ftgi,sizeof(ftgi));
    }else if(value>=65u){status[14]=0x91u;memcpy(status+28,acdi,sizeof(acdi));}
    else if(value>=48u){status[14]=0x92u;memcpy(status+28,acdi,sizeof(acdi));}
    else if(value>=33u){status[14]=0x93u;memcpy(status+28,acdi,sizeof(acdi));}
    else if(value>=20u){status[14]=0x94u;memcpy(status+28,acdi,sizeof(acdi));}
    else if(value>=10u){status[14]=0x91u;memcpy(status+25,rswi,sizeof(rswi));}
    else if(value>=6u){status[14]=0x92u;memcpy(status+25,rswi,sizeof(rswi));}
    else if(value>=3u){status[14]=0x93u;memcpy(status+25,rswi,sizeof(rswi));}
    else{status[14]=0x94u;memcpy(status+25,rswi,sizeof(rswi));}
  }
}
/* EXXON.16 RNK (end of game): V = RTCLOK 18:19 - CLCSC - ($6E6*4+$6E7)*4.
   The lower V, the better: below 0 or 60 "blue max" (BLUE, BLMX), 60..159
   SQLD, from 160 FTG, and a third RTCLOK byte WORST/WORST1. */
static void rnk_text(uint8_t *status,const BmGame *g){
  static const uint8_t rnki[11]={0x32,0x21,0x2e,0x2b,0x1a,0x00,0x23,0x2c,
    0x21,0x33,0x33};
  static const uint8_t demi[15]={0x62,0x6c,0x75,0x65,0x00,0x6d,0x61,0x78};
  static const uint8_t sqldi[15]={0x33,0x31,0x35,0x21,0x24,0x32,0x2f,0x2e,
    0x00,0x2c,0x25,0x21,0x24,0x25,0x32};
  static const uint8_t ftgi[12]={0x26,0x2c,0x39,0x29,0x2e,0x27,0x00,0x34,
    0x29,0x27,0x25,0x32};
  uint8_t rank,blue=0u;
  unsigned score_class=g->score>=20000u?255u:g->score/100u,column;
  unsigned clock=(unsigned)((g->play_frames>>8)&0xffffu);
  uint8_t hi=(uint8_t)(clock>>8),lo=(uint8_t)clock,v;
  uint8_t bias=(uint8_t)(((g->mission_difficulty*4u)+g->mission_rank_level)*4u);
  memcpy(status+2,rnki,sizeof(rnki));
  if(lo>=score_class){
    v=(uint8_t)(lo-score_class);
    if(hi){memcpy(status+26,ftgi,sizeof(ftgi));rank=0x14u;goto done;} /* WORST1 */
  }else{
    if(!hi)goto top;
    if(hi!=1u){memcpy(status+26,ftgi,sizeof(ftgi));rank=0x13u;goto done;} /* WORST */
    v=(uint8_t)(lo-score_class);
  }
  if(v<bias)goto top;
  v=(uint8_t)(v-bias);
  if(v>=200u){memcpy(status+26,ftgi,sizeof(ftgi));rank=0x12u;goto done;}
  if(v>=160u){memcpy(status+26,ftgi,sizeof(ftgi));rank=0x11u;goto done;}
  if(v>=60u){
    memcpy(status+25,sqldi,sizeof(sqldi));
    rank=v>=120u?0x14u:v>=100u?0x13u:v>=80u?0x12u:0x11u;
    goto done;
  }
  blue=1u;
  rank=v>=40u?0xd4u:v>=20u?0xd3u:(g->mission_rank_level?0xd2u:0xd3u);
  goto done;
top:
  blue=1u;
  rank=!g->mission_rank_level?0xd3u:g->mission_rank_level==1u?0xd2u:0xd1u;
done:
  if(blue){
    for(column=0;column<11u;++column)status[2+column]=(uint8_t)(rnki[column]|0x40u);
    memset(status+25,0,19);
    for(column=0;column<15u;++column)status[28+column]=(uint8_t)(demi[column]|0xc0u);
  }
  status[14]=rank;
}
/* The three mode-6 status lines (DLIR: CHBASE=$E0, COLBK=$66E, COLPF1=
   $6F4, COLPF2=54, COLPF3=152; COLPF0 keeps 708).  Screen memory starts at
   SC+$646 with 24-byte wide-playfield lines; characters 2..21 are visible.
   DSPLY fills SC+$649..$68A at START. */
static void status_line(BmFramebuffer *f,const BmGame *g){
  static const uint8_t dsply[66]={
    0xe6,0xda,0x53,0x50,0x50,0x00,0xe2,0xda,0x53,0x50,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf3,0xf0,0xe4,0xda,0x50,0x50,0x50,
    0x00,0xe1,0xec,0xf4,0xda,0x50,0x50,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0xf3,0xe3,0xef,0xf2,0xe5,0xda,0x00,0x50,0x50,0x50,0x50,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00};
  const BmWorldBuffer *w=&g->classic_world;
  uint8_t buf[72]={0};
  unsigned score=g->score,value,line,c,row,bit;
#define SCR(a) buf[(a)-0x646]
  if(g->demo){
    /* $6F3: the display list points at SC+$690, where EXXON.0 copied DEMI
       to SC+$698 ("BLUE MAX", "BY BOB POLIN", "(C)SYNAPSE 1983"). */
    static const uint8_t demi[59]={0x62,0x6c,0x75,0x65,0x00,0x6d,0x61,0x78,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0xe2,0xf9,0x00,0xe2,0xef,0xe2,0x00,0xf0,0xef,0xec,0xe9,0xee,0x00,0x00,
      0,0,0,0,0,0,0,0,
      0xc8,0xe3,0xc9,0xf3,0xf9,0xee,0xe1,0xf0,0xf3,0xe5,0x00,0xd1,0xd9,0xd8,0xd3};
    memset(buf,0,sizeof(buf));
    memcpy(buf+8,demi,sizeof(demi));
    goto draw;
  }
  memcpy(&SCR(0x649),dsply,sizeof(dsply));
#if BM_ATARI_FLIGHT
  value=g->flight_fuel.value;
  SCR(0x64b)=(uint8_t)(0x50u+value/100u%10u);
  SCR(0x64c)=(uint8_t)(0x50u+value/10u%10u);
  SCR(0x64d)=(uint8_t)(0x50u+value%10u);
  value=g->flight_service.bombs;
  SCR(0x651)=(uint8_t)(0x50u+value/10u%10u);
  SCR(0x652)=(uint8_t)(0x50u+value%10u);
  /* SV: F $A6, B $A2, M $AD, G $A7 at SC+$655..$658. */
  if(g->flight_service.damage_slots&1u)SCR(0x655)=0xa6u;
  if(g->flight_service.damage_slots&2u)SCR(0x656)=0xa2u;
  if(g->flight_service.damage_slots&4u)SCR(0x657)=0xadu;
  if(g->flight_service.damage_slots&8u)SCR(0x658)=0xa7u;
  if(g->mode==BM_STATE_TAKEOFF){
    /* TKOSPD keeps the takeoff glyphs (144.., PF2) until the carry. */
    SCR(0x665)=g->takeoff_speed.hundreds;
    SCR(0x666)=g->takeoff_speed.tens;
    SCR(0x667)=g->takeoff_speed.ones;
    if(g->takeoff_speed.ones<100u){
      value=g->flight_speed;
      SCR(0x665)=(uint8_t)(0x50u+value/100u%10u);
      SCR(0x666)=(uint8_t)(0x50u+value/10u%10u);
      SCR(0x667)=(uint8_t)(0x50u+value%10u);
    }
  }else{
    value=g->flight_speed;
    SCR(0x665)=(uint8_t)(0x50u+value/100u%10u);
    SCR(0x666)=(uint8_t)(0x50u+value/10u%10u);
    SCR(0x667)=(uint8_t)(0x50u+value%10u);
  }
#endif
  value=g->alt_display;
  SCR(0x66d)=(uint8_t)(0x50u+value/100u%10u);
  SCR(0x66e)=(uint8_t)(0x50u+value/10u%10u);
  SCR(0x66f)=(uint8_t)(0x50u+value%10u);
  if(w->warning_on)SCR(0x672)=0xb0u;          /* OF: 'P' */
  if(g->wind_on)SCR(0x673)=0x77u;             /* WIND: 'W' */
  /* S10..S10000: the ten-thousands digit stays blank until used. */
  if(score>=10000u)SCR(0x67f)=(uint8_t)(0x50u+score/10000u%10u);
  SCR(0x680)=(uint8_t)(0x50u+score/1000u%10u);
  SCR(0x681)=(uint8_t)(0x50u+score/100u%10u);
  SCR(0x682)=(uint8_t)(0x50u+score/10u%10u);
#if BM_ATARI_FLIGHT
  /* SC+$687: DRUN's $32 'R' window marker; CKLAND writes 236 ('L'). */
  SCR(0x687)=g->flight_landing.active?0xecu:w->runway_marker;
#else
  SCR(0x687)=w->runway_marker;
#endif
  if(w->marker_blank)SCR(0x687)=0x40u;        /* FLSH */
  if(w->line_star==1u)SCR(0x65a)=0x8au;       /* ASTRSK '*' */
  if(w->line_star==2u)SCR(0x68a)=0x8au;
  if(g->mode==BM_STATE_RANKING||
     (g->mode==BM_STATE_GAME_OVER&&!g->flight_crash_reason&&!g->end_timer)){
    /* AFH clears SC+$649..$674, then RNKX/RNK write their text. */
    uint8_t text[44]={0};
    if(g->mode==BM_STATE_RANKING)rnkx_text(text,g);else rnk_text(text,g);
    memcpy(&SCR(0x649),text,sizeof(text));
  }
draw:
#undef SCR
  f->palette[35]=bm_atari_pal_rgb[w->status_bk];
  f->palette[36]=bm_atari_pal_rgb[w->colpf0];
  f->palette[37]=bm_atari_pal_rgb[g->status_pf1];
  f->palette[38]=bm_atari_pal_rgb[54u];
  f->palette[39]=bm_atari_pal_rgb[152u];
  for(line=0;line<3u;++line)for(c=2;c<22u;++c){
    uint8_t code=buf[line*24u+c];
    const uint8_t *glyph=bm_status_font+(unsigned)(code&0x3fu)*8u;
    for(row=0;row<8u;++row)for(bit=0;bit<8u;++bit){
      uint8_t colour=(glyph[row]&(0x80u>>bit))?(uint8_t)(36u+(code>>6)):35u;
      int x=(int)(c-2u)*16+(int)bit*2,y=BM_ATARI_PLAYFIELD_HEIGHT+(int)(line*8u+row);
      px(f,x,y,colour);px(f,x+1,y,colour);
    }
  }
}
static unsigned debug_flight_speed(const BmGame *g){
#if BM_ATARI_FLIGHT
  if(g->mode==BM_STATE_TAKEOFF){
    if(g->takeoff_speed.hundreds>=144u)
      return (unsigned)(g->takeoff_speed.tens-144u)*10u+
             (unsigned)(g->takeoff_speed.ones-144u);
    return g->flight_speed;
  }
  return g->flight_speed;
#else
  return (unsigned)g->player.speed*100u;
#endif
}
static void debug_hud(BmFramebuffer*f,const BmGame*g){
  const BmWorldBuffer*w=&g->classic_world;int x=2,y=179;
  x=debug_text(f,x,y,"T");x=debug_hex(f,x,y,g->tick,4);x+=4;
  x=debug_text(f,x,y,"A");x=debug_hex(f,x,y,w->area_type,2);x+=4;
  x=debug_text(f,x,y,"F");x=debug_hex(f,x,y,w->fine_phase,1);x+=4;
  x=debug_text(f,x,y,"C");x=debug_hex(f,x,y,w->generator.centre,2);x+=4;
  x=debug_text(f,x,y,"R");x=debug_hex(f,x,y,w->road_position,2);
  x=debug_text(f,x,y,"/");x=debug_hex(f,x,y,w->road_target,2);x+=4;
  x=debug_text(f,x,y,"U");x=debug_hex(f,x,y,w->runway_phase,2);
  x=debug_text(f,x,y,"/");debug_hex(f,x,y,w->runway_delay,2);
  x=2;y=189;
  x=debug_text(f,x,y,"BV");x=debug_hex(f,x,y,w->building_variant,2);x+=4;
  x=debug_text(f,x,y,"R");x=debug_hex(f,x,y,w->building_row,2);x+=4;
  x=debug_text(f,x,y,"P");x=debug_hex(f,x,y,w->building_position,2);x+=4;
  /* O: shared P2/P3 object slot $FD; Q: objective state $C4. */
  x=debug_text(f,x,y,"O");x=debug_hex(f,x,y,w->object_slot,2);x+=4;
  x=debug_text(f,x,y,"Q");x=debug_hex(f,x,y,w->target_state,2);x+=4;
  x=debug_text(f,x,y,"E");x=debug_hex(f,x,y,w->building_end_row,1);x+=4;
  x=debug_text(f,x,y,"I");x=debug_hex(f,x,y,w->building_inactive?1u:0u,1);x+=4;
  x=debug_text(f,x,y,"X");x=debug_text(f,x,y,w->charset_mode==BM_CHARSET_HANGAR?"H":"B");x+=4;
  x=debug_text(f,x,y,"G");debug_hex(f,x,y,
    (w->gun1_shot_y?1u:0u)|(w->gun2_shot_y?2u:0u),1);
  x=250;y=179;
  x=debug_text(f,x,y,"B");x=debug_hex(f,x,y,w->bomb_impact_code,2);x+=4;
  x=debug_text(f,x,y,"H");debug_hex(f,x,y,w->bomb_impact_hit,1);
  x=280;y=179;x=debug_text(f,x,y,"F");
  debug_decimal(f,x,y,(unsigned)g->player.fuel);
#if BM_ATARI_FLIGHT
  x=300;y=179;x=debug_text(f,x,y,"A");
  debug_decimal(f,x,y,(unsigned)g->flight_service.bombs);
#endif
  x=250;y=189;x=debug_text(f,x,y,"S");
  if(w->ship_active)debug_hex(f,x,y,w->ship_hits_remaining,1);
  else debug_text(f,x,y,"-");
  x=260;y=189;x=debug_text(f,x,y,"H");
  x=debug_decimal(f,x,y,(unsigned)g->player.altitude);x+=4;
  x=debug_text(f,x,y,"V");
  debug_decimal(f,x,y,debug_flight_speed(g));
#if BM_ATARI_FLIGHT
  x=240;y=169;x=debug_text(f,x,y,"C");
  x=debug_hex(f,x,y,g->flight_crash_reason,1u);x+=2;
  x=debug_text(f,x,y,"L");
  /* 8: the CKLAND window ($609=0 with SC+$687 set), which the original
     shows as a status-line glyph until T23 ports that line. */
  x=debug_hex(f,x,y,(g->flight_runway_visible?1u:0u)|
    (g->flight_runway_collision?2u:0u)|
    (g->flight_landing.active?4u:0u)|
    (!w->runway_slot&&w->runway_marker?8u:0u),1u);x+=2;
  x=debug_text(f,x,y,"D");
  x=debug_hex(f,x,y,g->flight_service.damage_level,1u);x+=2;
  /* Damage letters F/B/M/G as a hex mask, then the WIND indicator. */
  x=debug_hex(f,x,y,g->flight_service.damage_slots,1u);x+=2;
  if(g->wind_on)debug_text(f,x,y,g->wind_left?"W<":"W>");
#endif
}
#endif
#if BM_CLASSIC_TEST_PAGE
/* COLPM2 (706) as the original leaves it for this frame.  Only one P2
   object exists at a time ($FD), so one CLUT entry (29) serves them all. */
static uint8_t p2_colour(const BmGame *g){
  const BmWorldBuffer *w=&g->classic_world;
  /* Z9/O5: after each sight step $C0 bit 2 picks 50 or 14. */
  if(w->target_sight_active)return (w->target_sight_x&4u)?50u:14u;
  /* XP2: $66F below 30 is 50, otherwise bit 2 picks 44 or 54. */
  if(w->enemy_fighter_active&&w->enemy_fighter_destroyed)
    return w->enemy_fighter_destroy_timer<30u?50u:
      ((w->enemy_fighter_destroy_timer&4u)?44u:54u);
  /* TRGTP2 (COMMON): $7C/$A8 alternate on bit 3 of $604. */
  if(w->p2_colour==0x7cu||w->p2_colour==0xa8u)
    return (w->vbi_counter&8u)?0x7cu:0xa8u;
  return w->p2_colour;
}
/* Enemy composites carry their P3 pixels as colour 9; every other pixel
   belongs to P2 and takes COLPM2. */
static void blit_p2(BmFramebuffer *f,const BmSprite *s,int x,int y){
  int r,c;
  for(r=0;r<s->height;++r)for(c=0;c<s->width;++c){
    uint8_t v=s->pixels[r*s->stride+c];
    if(v)px(f,x-s->anchor_x+c,y-s->anchor_y+r,v==9u?9u:29u);
  }
}
#endif
#if BM_CLASSIC_TEST_PAGE
/* GPRIOR 4 (PRIOR): playfield in front of the players, so a player pixel
   only shows where the playfield is COLBK. */
static void blit_behind_playfield(BmFramebuffer *f,const BmSprite *s,int x,
                                  int y,int colour){
  int r,c;
  for(r=0;r<s->height;++r)for(c=0;c<s->width;++c){
    int xx=x-s->anchor_x+c,yy=y-s->anchor_y+r;
    uint8_t v=s->pixels[r*s->stride+c],under;
    if(!v||(unsigned)xx>=BM_SCREEN_WIDTH||(unsigned)yy>=BM_SCREEN_HEIGHT)continue;
    under=f->pixels[yy*BM_SCREEN_WIDTH+xx];
    if(under==BM_CLASSIC_INITIAL_BASE||under==BM_CLASSIC_DLI_BASE)
      f->pixels[yy*BM_SCREEN_WIDTH+xx]=(uint8_t)(colour<0?v:colour);
  }
}
#endif
static void palette(BmFramebuffer*f,const BmGame*g){
  uint8_t initial[BM_CLASSIC_COLOUR_COUNT]={0x00,0xc4,0x94,0xd2,0x04};
  uint8_t dli[BM_CLASSIC_COLOUR_COUNT]={0x00,0xc4,0x94,0x36,0x98};
  f->palette[0]=0x101c38;f->palette[1]=0x346cb0;f->palette[2]=0x88bce8;
  f->palette[3]=0x4c7438;f->palette[4]=0x789c48;f->palette[5]=0x604028;
  f->palette[6]=0xd8d0a0;f->palette[7]=0xf0f0e8;f->palette[8]=0xb02820;
  f->palette[9]=0x202018;
  /* Preview build: the generated straight fighter uses colour 10 and its
     banking frames white (7).  The classic page draws every P2 object in
     COLPM2 instead (CLUT 29, see p2_colour()). */
  f->palette[10]=f->palette[7];
#if BM_CLASSIC_TEST_PAGE
  initial[0]=g->classic_world.colbk;
  initial[1]=g->classic_world.colpf0;
  initial[2]=g->classic_world.colpf1;
  initial[3]=g->classic_world.colpf2;
  initial[4]=g->classic_world.colpf3;
  dli[0]=g->classic_world.colbk;
  dli[1]=g->classic_world.colpf0;
  dli[2]=g->classic_world.colpf1;
  dli[3]=g->classic_world.colpf2;
  dli[4]=g->classic_world.colpf3;
  if(g->classic_world.impact_flash_ticks&2u){
    /* XFLSH has its own twelve-tick timer, independent of XPLSN.
       RANDOM selects which of COLPF0/COLPF1 receives $34 versus $2C/$36. */
    uint8_t swap=(uint8_t)(g->rng&1u);
    initial[1]=dli[1]=swap?0x34u:0x2cu;
    initial[2]=dli[2]=swap?0x2cu:0x36u;
    initial[3]=dli[3]=0x2eu;
    initial[4]=dli[4]=0x38u;
  }
  /* Keep diagnostics out of the chipset-managed low CLUT entries and next to
     the already hardware-validated ANTIC banks. */
  f->palette[26]=0xffffff;
  f->palette[27]=0xe02020;
  f->palette[28]=bm_atari_pal_rgb[0x00];
  f->palette[29]=bm_atari_pal_rgb[p2_colour(g)];
  f->palette[30]=bm_atari_pal_rgb[0x00];
  f->palette[31]=bm_atari_pal_rgb[
    (g->classic_world.ship_destroy_timer&8u)?0x2cu:0x36u];
  f->palette[32]=bm_atari_pal_rgb[
    (g->classic_world.river_car_destroy_timer&4u)?0x2cu:0x36u];
  f->palette[33]=bm_atari_pal_rgb[0x2cu];
  f->palette[34]=bm_atari_pal_rgb[0x30u];
#else
  (void)g;
#endif
  bm_atari_pal_install(f,BM_CLASSIC_INITIAL_BASE,initial);
  bm_atari_pal_install(f,BM_CLASSIC_DLI_BASE,dli);
}
/* EXXON.16 CNSL .5: the OPTION screen.  OPTIM is a 20-byte-per-line text
   display list (mode 7 = 16 scanlines, mode 6 = 8) over SC, ROM font,
   712=80 above the DLIR1 line and 48 below it, 708=14, 709=$68.  The two
   empty top lines are left out to fit 200 lines. */
static void options_screen(BmFramebuffer *f,const BmGame *g){
  static const uint8_t mode[16]={7,7,7,7,7,7,6,7,7,6,7,7,6,7,7,6};
  static const uint8_t opti[7]={0x2f,0x30,0x34,0x29,0x2f,0x2e,0x33};
  static const uint8_t skill[3][12]={
    {0x2e,0x2f,0x36,0x29,0x23,0x25,0,0,0,0,0,0},
    {0x29,0x2e,0x34,0x25,0x32,0x2d,0x25,0x24,0x29,0x21,0x34,0x25},
    {0x21,0x24,0x36,0x21,0x2e,0x23,0x25,0x24,0,0,0,0}};
  static const uint8_t control[2][12]={
    {0x2e,0x2f,0x32,0x2d,0x21,0x2c,0x00,0x23,0x34,0x32,0x2c,0x00},
    {0x32,0x25,0x36,0x25,0x32,0x33,0x25,0x00,0x23,0x34,0x32,0x2c}};
  static const uint8_t gravity[13]={0x2e,0x2f,0x00,0x27,0x32,0x21,0x36,
    0x29,0x34,0x39,0,0,0};
  uint8_t sc[320]={0};
  unsigned line,c,row,bit;int y=4;
  memcpy(sc+46,opti,sizeof(opti));
  memcpy(sc+124,skill[g->mission_rank_level%3u],12);
  memcpy(sc+184,control[g->option_reversed?1:0],12);
  memcpy(sc+244,gravity+(g->mission_difficulty?3:0),10);
  sc[g->menu_cursor==0u?122:g->menu_cursor==1u?182:242]=0x4au;
  f->palette[35]=bm_atari_pal_rgb[80u];
  f->palette[36]=bm_atari_pal_rgb[14u];
  f->palette[37]=bm_atari_pal_rgb[0x68u];
  f->palette[38]=bm_atari_pal_rgb[0x48u];
  f->palette[39]=bm_atari_pal_rgb[48u];
  memset(f->pixels,35,sizeof(f->pixels));
  for(line=2;line<16u;++line){
    unsigned height=mode[line]==7u?16u:8u;
    uint8_t bk=line>=4u?39u:35u;           /* DLIR1 after the third line */
    for(row=0;row<height;++row){
      int yy=y+(int)row;
      for(c=0;c<20u;++c){
        uint8_t code=sc[line*20u+c];
        uint8_t bits=bm_status_font[(unsigned)(code&0x3fu)*8u+
                                    (mode[line]==7u?row/2u:row)];
        for(bit=0;bit<8u;++bit){
          uint8_t colour=(bits&(0x80u>>bit))?(uint8_t)(36u+(code>>6)):bk;
          px(f,(int)c*16+(int)bit*2,yy,colour);
          px(f,(int)c*16+(int)bit*2+1,yy,colour);
        }
      }
    }
    y+=(int)height;
  }
}
/* The classic renderer: the Atari picture into a 320x200 CLUT8 framebuffer. */
static void classic_render(const BmGame*g,BmFramebuffer*f){
  int x,y,p=g->player.x,q=g->player.y;palette(f,g);
  bm_sprite_list.count=0u;
  if(g->mode==BM_STATE_OPTIONS){options_screen(f,g);return;}
#if BM_CLASSIC_TEST_PAGE
  static const BmAntic4Zone classic_zones[]={
    {0,{16,17,18,19,20}},
    {BM_ATARI_PLAYFIELD_HEIGHT,{21,22,23,24,25}}
  };
  const uint8_t *classic_charset =
    g->classic_world.tank_charset_loaded ? bm_asset_max3_tank_charset :
      bm_asset_max3_hangar_charset;
#endif
  const BmSprite plane0={bm_asset_plane_frame_0,BM_ASSET_WIDTH,BM_ASSET_PLANE_FRAME_0_HEIGHT,BM_ASSET_WIDTH,8,5};
  const BmSprite plane1={bm_asset_plane_frame_1,BM_ASSET_WIDTH,BM_ASSET_PLANE_FRAME_1_HEIGHT,BM_ASSET_WIDTH,8,5};
  const BmSprite bank_left0={bm_asset_bank_left_frame_0,BM_ASSET_WIDTH,BM_ASSET_BANK_LEFT_FRAME_0_HEIGHT,BM_ASSET_WIDTH,8,5};
  const BmSprite bank_left1={bm_asset_bank_left_frame_1,BM_ASSET_WIDTH,BM_ASSET_BANK_LEFT_FRAME_1_HEIGHT,BM_ASSET_WIDTH,8,5};
  const BmSprite bank_right0={bm_asset_bank_right_frame_0,BM_ASSET_WIDTH,BM_ASSET_BANK_RIGHT_FRAME_0_HEIGHT,BM_ASSET_WIDTH,8,5};
  const BmSprite bank_right1={bm_asset_bank_right_frame_1,BM_ASSET_WIDTH,BM_ASSET_BANK_RIGHT_FRAME_1_HEIGHT,BM_ASSET_WIDTH,8,5};
  /* SHDW copies eight rows to P1 from F6; anchor matches the plane's. */
  const BmSprite shadow={bm_game_shadow_pixels(g),BM_ASSET_WIDTH,BM_ASSET_SHADOW_HEIGHT,BM_ASSET_WIDTH,8,5};
#if BM_CLASSIC_TEST_PAGE
  const BmSprite ship0={bm_asset_ship_layer_0,BM_ASSET_WIDTH,
    BM_ASSET_SHIP_LAYER_0_HEIGHT,BM_ASSET_WIDTH,0,0};
  const BmSprite ship1={bm_asset_ship_layer_1,BM_ASSET_WIDTH,
    BM_ASSET_SHIP_LAYER_1_HEIGHT,BM_ASSET_WIDTH,0,0};
  const BmSprite destroyed_ship={bm_asset_destroyed_ship,BM_ASSET_WIDTH,13u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite car_right_body={bm_asset_car_right_body,BM_ASSET_WIDTH,5u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite car_right_tyres={bm_asset_car_right_tyres,BM_ASSET_WIDTH,5u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite car_left_body={bm_asset_car_left_body,BM_ASSET_WIDTH,5u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite car_left_tyres={bm_asset_car_left_tyres,BM_ASSET_WIDTH,5u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite truck_right_body={bm_asset_truck_right_body,BM_ASSET_WIDTH,6u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite truck_right_tyres={bm_asset_truck_right_tyres,BM_ASSET_WIDTH,6u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite truck_left_body={bm_asset_truck_left_body,BM_ASSET_WIDTH,6u,
    BM_ASSET_WIDTH,0,0};
  const BmSprite truck_left_tyres={bm_asset_truck_left_tyres,BM_ASSET_WIDTH,6u,
    BM_ASSET_WIDTH,0,0};
#endif
  const BmSprite *plane=g->player.bank<0?((g->tick&2u)?&bank_left0:&bank_left1):
                        (g->player.bank>0?((g->tick&2u)?&bank_right0:&bank_right1):
                         ((g->tick&2u)?&plane0:&plane1));
#if BM_CLASSIC_TEST_PAGE
  memset(f->pixels,BM_CLASSIC_INITIAL_BASE,sizeof(f->pixels));
#else
  for(y=0;y<70;y++)memset(&f->pixels[y*320],1,320);
  for(;y<118;y++)memset(&f->pixels[y*320],2,320);
  for(;y<200;y++)memset(&f->pixels[y*320],((y+(g->world_scroll>>2))&8)?3:4,320);
#endif
#if !BM_CLASSIC_TEST_PAGE
  for(y=118;y<200;y++){
    int h=(y-112)*2,m=160+((g->world_scroll>>3)&15)-8;
    for(x=m-h;x<=m+h;x++)px(f,x,y,5);
    px(f,m-h,y,6);
    px(f,m+h,y,6);
  }
#endif
#if BM_CLASSIC_TEST_PAGE
  if(g->classic_world.capital_gun_glyph){
    /* CHBAS ADK is glyph 49 (GUNC).  WR copies ADJ over it for the capital:
       a PF1 barrel on a PF2/PF3 base; DFGN copies ADI back in area 1. */
    static uint8_t capital_charset[1024];
    static const uint8_t adj[8]={0x00,0x20,0x20,0x20,0xec,0xec,0xfc,0xfc};
    memcpy(capital_charset,classic_charset,sizeof(capital_charset));
    memcpy(capital_charset+49u*8u,adj,8u);
    classic_charset=capital_charset;
  }
  if(g->classic_world.hangar_impact_phase||g->classic_world.tank_impact_phase||
     g->classic_world.building_impact_phase){
    static uint8_t impact_charset[1024];
    static const uint8_t abc[8]={0x04,0x11,0x40,0x11,0x40,0x04,0x40,0x41};
    static const uint8_t abd[8]={0x45,0x54,0x15,0x51,0x44,0x55,0x15,0x51};
    static const uint8_t hangar[13]={21,22,23,24,25,26,27,28,29,30,31,32,126};
    static const uint8_t tank[7]={69,63,79,73,74,18,127};
    static const uint8_t building[10]={84,86,88,90,92,94,85,89,87,93};
    unsigned family,i;
    memcpy(impact_charset,classic_charset,sizeof(impact_charset));
    /* Character RAM survives a later impact in another family. Only the
       corresponding DFHNGR/DFTNK writer restores its own glyph range. */
    for(family=0;family<3u;++family){
      const uint8_t *glyphs=family==2u?building:(family?tank:hangar);
      unsigned count=family==2u?10u:(family?7u:13u);
      uint8_t phase=family==2u?g->classic_world.building_impact_phase:
        (family?g->classic_world.tank_impact_phase:g->classic_world.hangar_impact_phase);
      if(phase)for(i=0;i<count;++i)
        memcpy(impact_charset+glyphs[i]*8u,phase==1u?abc:abd,8u);
    }
    classic_charset=impact_charset;
  }
  bm_antic4_blit_playfield(f,
    classic_charset,
    &g->classic_world.rows[0][0],g->classic_world.source_column,
    g->classic_world.fine_phase,
    (int16_t)(g->classic_world.quake_shift?-2:0),0,classic_zones,  /* QUAKE */
    sizeof(classic_zones)/sizeof(classic_zones[0]));
  for(y=BM_ATARI_PLAYFIELD_HEIGHT;y<BM_SCREEN_HEIGHT;y++)
    memset(&f->pixels[y*BM_SCREEN_WIDTH],BM_CLASSIC_INITIAL_BASE,BM_SCREEN_WIDTH);
  if(g->classic_world.ship_active){
    /* SHIP/I5 writes SHPIM to P2 rows C1-11..C1 and SHPIM1 to P3 two
       lines lower; C1 starts at 15, above the playfield. */
    int ship_x=bm_pm_x(g->classic_world.ship_x);
    int ship_y=bm_pm_y((int)g->classic_world.ship_y-11);
    if(sprite(BM_SPRITE_SHIP,g->classic_world.ship_destroyed?1u:0u,ship_x,
              g->classic_world.ship_destroyed?ship_y-1:ship_y,
              g->classic_world.ship_destroyed?
                ((g->classic_world.ship_destroy_timer&8u)?0x2cu:0x36u):p2_colour(g),0u)){
      if(g->classic_world.ship_destroyed)
        /* Z6 writes XSHPIM to P2 rows C1-12..C1. */
        bm_sprite_blit_colour(f,&destroyed_ship,ship_x,ship_y-1,31u);
      else{
        bm_sprite_blit_colour(f,&ship0,ship_x,ship_y,29u);
        bm_sprite_blit_colour(f,&ship1,ship_x,ship_y+2,30u);
      }
    }
  }
  if(g->classic_world.river_car_active){
    /* DRP23/IV writes the body to P2 rows C1..C1+4 and the tyres to P3
       one line lower. */
    int car_x=bm_pm_x(g->classic_world.river_car_x);
    int car_y=bm_pm_y(g->classic_world.river_car_y);
    const BmSprite *body;
    const BmSprite *tyres;
    if(g->classic_world.river_vehicle_truck){
      body=g->classic_world.river_car_left?&truck_left_body:&truck_right_body;
      tyres=g->classic_world.river_car_left?&truck_left_tyres:&truck_right_tyres;
    }else{
      body=g->classic_world.river_car_left?&car_left_body:&car_right_body;
      tyres=g->classic_world.river_car_left?&car_left_tyres:&car_right_tyres;
    }
    if(sprite(BM_SPRITE_VEHICLE,(g->classic_world.river_vehicle_truck?1u:0u)|
              (g->classic_world.river_car_left?2u:0u)|
              (g->classic_world.river_car_destroyed?4u:0u),car_x,
              g->classic_world.river_car_destroyed?car_y-1:car_y,
              g->classic_world.river_car_destroyed?
                ((g->classic_world.river_car_destroy_timer&4u)?0x2cu:0x36u):p2_colour(g),0u)){
      if(g->classic_world.river_car_destroyed){
        static const BmSprite wreck={bm_asset_crash,16,10,16,0,0};
        bm_sprite_blit_colour(f,&wreck,car_x,car_y-1,32u);
      }else bm_sprite_blit_colour(f,body,car_x,car_y,29u);
      bm_sprite_blit_colour(f,tyres,car_x,car_y+1,30u);
    }
  }
  if(g->classic_world.enemy_fighter_active){
    const BmWorldBuffer *w=&g->classic_world;
    static const BmSprite back={bm_asset_fighter_back,16,10,16,0,0};
    static const BmSprite forward={
      bm_asset_fighter_forward,16,BM_ASSET_FIGHTER_FORWARD_HEIGHT,16,0,0
    };
    static const BmSprite forward_p3={
      bm_asset_fighter_forward_p3,16,BM_ASSET_FIGHTER_FORWARD_P3_HEIGHT,16,0,0
    };
    static const BmSprite ftl={
      bm_asset_fighter_left_initial,16,BM_ASSET_FIGHTER_LEFT_INITIAL_HEIGHT,16,0,0
    };
    static const BmSprite ftr={
      bm_asset_fighter_right_initial,16,BM_ASSET_FIGHTER_RIGHT_INITIAL_HEIGHT,16,0,0
    };
    static const BmSprite bank_right={
      bm_asset_bank_right_frame_1,16,BM_ASSET_BANK_RIGHT_FRAME_1_HEIGHT,16,0,0
    };
    static const BmSprite bank_left={
      bm_asset_bank_left_frame_1,16,BM_ASSET_BANK_LEFT_FRAME_1_HEIGHT,16,0,0
    };
    static const BmSprite enemy_shadow={
      bm_asset_shadow,16,BM_ASSET_SHADOW_HEIGHT,16,0,0
    };
    /* PLBCK/OG and PLFWD/OZ write the body from P2+C1; the PLBCK shadow
       (OK) and the PLFWD SHDW1 layer (KU) from P3+C2. */
    int fighter_x=bm_pm_x(w->enemy_fighter_x);
    int fighter_y=bm_pm_y(w->enemy_fighter_y);
    int shadow_y=bm_pm_y(w->enemy_fighter_shadow_y);
    unsigned look=(w->enemy_fighter_forward?1u:0u)|
                  (w->enemy_fighter_turn==1u?2u:w->enemy_fighter_turn==2u?4u:0u);
    if(w->enemy_fighter_destroyed){
      /* COLM02 -> DG writes XPL1IM to P2 rows C1-1..C1+10; P3 is cleared. */
      static const BmSprite wreck={
        bm_asset_explosion,16,BM_ASSET_EXPLOSION_HEIGHT,16,0,0
      };
      if(sprite(BM_SPRITE_ENEMY,8u,fighter_x,fighter_y-1,p2_colour(g),0u))
        bm_sprite_blit_colour(f,&wreck,fighter_x,fighter_y-1,29u);
    }else if(g->mode==BM_STATE_CRASHING&&g->flight_crash_colp02){
      /* CRSH/SK: with $655=0 the enemy P2 takes the crash colour. */
      if(w->enemy_body_on&&sprite(BM_SPRITE_ENEMY,look|16u,fighter_x,fighter_y,
                                  p2_colour(g),0u)){
        const BmSprite *body=w->enemy_fighter_forward?&forward:&back;
        bm_sprite_blit_colour(f,body,fighter_x,fighter_y,
                              (g->flight_crash_timer&4u)?34u:33u);
      }
    }else if(!w->enemy_fighter_forward){
      if(w->enemy_shadow_on&&sprite(BM_SPRITE_ENEMY_SHADOW,0u,fighter_x,shadow_y,0u,0u))
        bm_sprite_blit_colour(f,&enemy_shadow,fighter_x,shadow_y,30u);
      if(w->enemy_body_on&&sprite(BM_SPRITE_ENEMY,look,fighter_x,fighter_y,p2_colour(g),0u))
        blit_p2(f,w->enemy_fighter_turn==1u?&bank_left:
                  (w->enemy_fighter_turn==2u?&bank_right:&back),
                fighter_x,fighter_y);
    }else if(w->enemy_body_on){
      if(sprite(BM_SPRITE_ENEMY,look,fighter_x,fighter_y,p2_colour(g),0u))
        blit_p2(f,w->enemy_fighter_turn==1u?&ftl:
                  (w->enemy_fighter_turn==2u?&ftr:&forward),
                fighter_x,fighter_y);
    }else if(w->enemy_shadow_on){
      if(sprite(BM_SPRITE_ENEMY_SHADOW,1u,fighter_x,shadow_y,0u,0u))
        bm_sprite_blit(f,&forward_p3,fighter_x,shadow_y);
    }
  }
  if(g->classic_world.capital_y){
    /* CPTL: AAH/XCPTLIM on P2 (COLPM2, CLUT 29), AAI on P3 (707=0). */
    const BmWorldBuffer *w=&g->classic_world;
    int x0=bm_pm_x(w->capital_x),y0=bm_pm_y((int)w->capital_y+w->capital_shift-20);
    int xx,yy;
    if(sprite(BM_SPRITE_CAPITAL_TARGET,0u,x0,y0,p2_colour(g),w->gprior==4u))
    for(yy=y0;yy<=y0+22&&yy<BM_ATARI_PLAYFIELD_HEIGHT;++yy)
      for(xx=x0;xx<x0+64;++xx){
        uint8_t cover,under;
        if((unsigned)xx>=BM_SCREEN_WIDTH||yy<0)continue;
        cover=bm_world_capital_covers(w,xx,yy);
        if(!cover)continue;
        under=f->pixels[yy*BM_SCREEN_WIDTH+xx];
        if(w->gprior==4u&&under!=BM_CLASSIC_INITIAL_BASE&&
           under!=BM_CLASSIC_DLI_BASE)continue;
        f->pixels[yy*BM_SCREEN_WIDTH+xx]=cover==2u?29u:30u;
      }
  }
  if(g->classic_world.enemy_fighter_shot_active&&
     sprite(BM_SPRITE_ENEMY_SHOT,0u,bm_pm_x(g->classic_world.enemy_fighter_shot_x),
            bm_pm_y(g->classic_world.enemy_fighter_shot_y),p2_colour(g),0u))
    draw_screen_shot(f,bm_pm_x(g->classic_world.enemy_fighter_shot_x),
      bm_pm_y(g->classic_world.enemy_fighter_shot_y),0u,29u); /* M2 = COLPM2 */
  if(g->classic_world.target_sight_active){
    static const BmSprite sight={bm_asset_gunsight,16,9,16,0,0};
    /* GNSGHT writes the nine GNST rows to P2 ending at Y=$6AB. */
    int sight_x=bm_pm_x(g->classic_world.target_sight_x);
    int sight_y=bm_pm_y((int)g->classic_world.target_sight_y-8);
    if(sprite(BM_SPRITE_GUNSIGHT,0u,sight_x,sight_y,p2_colour(g),0u))
      bm_sprite_blit_colour(f,&sight,sight_x,sight_y,29u);
  }
#endif
  /* LAND clears the Atari shadow state before entering GROUND. A subsequent
     TAKOF does not call SHDW either: JOYST/UP restores it when the aircraft
     first climbs from H003 to H004. The initial takeoff is different because
     startup explicitly draws SHDW; service_phase is still zero there. */
  if(g->mode!=BM_STATE_GROUND&&g->mode!=BM_STATE_CRASHING&&
     g->mode!=BM_STATE_RANKING&&g->mode!=BM_STATE_GAME_OVER&&
     !(g->flight_service_phase&&g->player.altitude<=3)&&
     !g->classic_world.shadow_hidden&&         /* PRIOR: SHBNE */
     sprite(BM_SPRITE_SHADOW,g->shadow_far?1u:(g->player.bank?2u:0u),
            p-8,q+g->player.altitude-5,0u,
#if BM_CLASSIC_TEST_PAGE
            g->classic_world.gprior==4u
#else
            0u
#endif
            )){
#if BM_CLASSIC_TEST_PAGE
    /* SHCOL: P1 colour 705=2 for the thin far shadow, else 0. */
    f->palette[40]=bm_atari_pal_rgb[2];
    if(g->classic_world.gprior==4u)
      blit_behind_playfield(f,&shadow,p,q+g->player.altitude,
                            g->shadow_far?40:-1);
    else if(g->shadow_far)
      bm_sprite_blit_colour(f,&shadow,p,q+g->player.altitude,40u);
    else
#endif
    bm_sprite_blit(f,&shadow,p,q+g->player.altitude);
  }
  if(g->mode==BM_STATE_CRASHING&&g->flight_crash_phase==1u){
    /* AFQ writes XPL1IM to P0 and P1 rows F5-2..F5+10 (plane top is q-5).
       P1 still holds the shadow, raised $654 lines; rows outside the
       explosion stay visible at P1's HPOS in the CRSH colour. */
    static const BmSprite player_crash={
      bm_asset_explosion,16,BM_ASSET_EXPLOSION_HEIGHT,16,8,7
    };
    uint8_t colour=(g->flight_crash_timer&4u)?34u:33u;
    int shadow_top=q-5+(uint8_t)(g->player.altitude-g->flight_crash_lift);
    int r,c,p1=p+g->flight_crash_spread;
    for(r=0;r<BM_ASSET_SHADOW_HEIGHT&&!g->classic_world.shadow_hidden;++r){
      int yy=shadow_top+r;
      if(yy>=q-7&&yy<q-7+BM_ASSET_EXPLOSION_HEIGHT)continue;
      for(c=0;c<16;++c)if(bm_game_shadow_pixels(g)[r*16+c])px(f,p1-8+c,yy,colour);
    }
    if(sprite(BM_SPRITE_PLAYER_CRASH,0u,p-g->flight_crash_spread-8,q-7,0u,0u))
      bm_sprite_blit_colour(f,&player_crash,p-g->flight_crash_spread,q,colour);
    if(sprite(BM_SPRITE_PLAYER_CRASH,1u,p1-8,q-7,0u,0u))
      bm_sprite_blit_colour(f,&player_crash,p1,q,colour);
  }else if(g->mode!=BM_STATE_CRASHING&&g->mode!=BM_STATE_RANKING&&
           !(g->mode==BM_STATE_GAME_OVER&&g->flight_crash_reason)&&
           sprite(BM_SPRITE_PLAYER,
                  (g->player.bank<0?2u:g->player.bank>0?4u:0u)|((g->tick&2u)?0u:1u),
                  p-8,q-5,0u,
#if BM_CLASSIC_TEST_PAGE
                  g->classic_world.gprior==4u
#else
                  0u
#endif
                  )){
#if BM_CLASSIC_TEST_PAGE
    if(g->classic_world.gprior==4u)blit_behind_playfield(f,plane,p,q,26);
    else bm_sprite_blit_colour(f,plane,p,q,26);
#else
    bm_sprite_blit(f,plane,p,q);
#endif
  }
  if(g->player_shot_active&&
     sprite(BM_SPRITE_PLAYER_SHOT,0u,g->player_shot_x,g->player_shot_y,0u,0u)){
    for(y=0;y<1;y++)for(x=0;x<4;x++)
      px(f,g->player_shot_x+x,g->player_shot_y+y,
#if BM_CLASSIC_TEST_PAGE
        26
#else
        7
#endif
      );
  }
  if(g->player_bomb_active&&
     sprite(BM_SPRITE_BOMB,g->player_bomb_phase,g->player_bomb_x,g->player_bomb_y,0u,0u)){
    uint8_t colour=
#if BM_CLASSIC_TEST_PAGE
      28u;
#else
      9u;
#endif
    int narrow_y=g->player_bomb_y+(g->player_bomb_phase?2:0);
    for(x=0;x<4;x++)px(f,g->player_bomb_x+x,g->player_bomb_y+1,colour);
    for(x=2;x<4;x++)px(f,g->player_bomb_x+x,narrow_y,colour);
  }
  if(g->player_bomb_explosion){
    static const BmSprite bomb_explosion={
      bm_asset_explosion,16,BM_ASSET_EXPLOSION_HEIGHT,16,0,0
    };
    /* XPL1IM is centred around the missile impact in the original PM image. */
    if(!sprite(BM_SPRITE_BOMB_EXPLOSION,g->player_bomb_water_splash?1u:0u,
               g->player_bomb_explosion_x-6,
               g->player_bomb_explosion_y-(g->player_bomb_water_splash?9:6),0u,0u)){}
    else if(g->player_bomb_water_splash)
      bm_sprite_blit_colour(f,&bomb_explosion,g->player_bomb_explosion_x-6,
                            g->player_bomb_explosion_y-9,
#if BM_CLASSIC_TEST_PAGE
                            26u
#else
                            7u
#endif
      );
    else
      bm_sprite_blit(f,&bomb_explosion,g->player_bomb_explosion_x-6,
                     g->player_bomb_explosion_y-6);
  }
  draw_gun_shot(f,g->classic_world.gun1_shot_x,
                 g->classic_world.gun1_shot_y,
                 g->classic_world.gun1_shot_explosion,
#if BM_CLASSIC_TEST_PAGE
                 28
#else
                 9
#endif
  );
  draw_gun_shot(f,g->classic_world.gun2_shot_x,
                 g->classic_world.gun2_shot_y,
                 g->classic_world.gun2_shot_explosion,
#if BM_CLASSIC_TEST_PAGE
                 28
#else
                 9
#endif
  );
#if BM_CLASSIC_TEST_PAGE
#endif
#if BM_CLASSIC_TEST_PAGE
#if BM_DEBUG_HUD
  (void)status_line;
  debug_hud(f,g);
#else
  (void)debug_hud;                         /* kept for DEBUG_HUD=1 */
  status_line(f,g);
#endif
#endif
}
static void classic_render_frame(const BmGame*g,BmFrame*frame){
  uint32_t profile=bm_profile_start();
  classic_render(g,frame->indexed);
  bm_profile_stop(BM_PROFILE_CLASSIC,profile);
}
const BmRenderer bm_renderer_classic={
  "classic",{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},classic_render_frame
};
/* The classic picture into the game's own framebuffer (tests and tools). */
void bm_game_render(BmGame*g){classic_render(g,&g->framebuffer);}
uint32_t bm_framebuffer_checksum(const BmFramebuffer*f){
  uint32_t h=2166136261u;unsigned i;for(i=0;i<sizeof(f->pixels);i++){h^=f->pixels[i];h*=16777619u;}return h;
}
