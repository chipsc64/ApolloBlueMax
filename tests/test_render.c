#include "bm_renderer.h"
#include <stdio.h>
#include <string.h>
#include "bm_tile.h"
#include "bm_art_assets.h"
/* The renderers: the selected one is classic by default, and the scaled
   renderer shows exactly the classic picture, 2x2 per pixel in RGB565. */
static BmGame game;
static uint16_t scaled[640u*400u],hd[640u*400u];
static int failures;
static void check(int ok,const char *what){if(!ok){printf("FAIL: %s\n",what);++failures;}}
static uint16_t rgb565(uint32_t c){
  return (uint16_t)(((c>>8)&0xf800u)|((c>>5)&0x07e0u)|((c>>3)&0x001fu));
}
int main(void){
  BmInput in;unsigned f,x,y,bad=0;
  BmFrame classic={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&game.framebuffer,0,0u};
  BmFrame big={{640u,400u,BM_PIXEL_RGB565},0,scaled,640u*2u};
  check(bm_renderer_selected()==&bm_renderer_classic,"classic is the default renderer");
  check(bm_renderer_scaled.format.width==640u&&bm_renderer_scaled.format.height==400u&&
        bm_renderer_scaled.format.pixel==BM_PIXEL_RGB565,"scaled format 640x400 RGB565");
  memset(&in,0,sizeof in);bm_game_boot(&game,7u);
  for(f=0;f<3000u;++f){
    in.fire=(f%300u)<20u;bm_game_tick(&game,&in);
    if(f%500u)continue;
    bm_renderer_classic.render(&game,&classic);
    bm_renderer_scaled.render(&game,&big);
    for(y=0;y<400u;++y)for(x=0;x<640u;++x){
      uint8_t index=game.framebuffer.pixels[(y/2u)*BM_SCREEN_WIDTH+x/2u];
      if(scaled[y*640u+x]!=rgb565(game.framebuffer.palette[index]))++bad;
    }
  }
  check(!bad,"scaled picture = classic picture 2x2");
  /* hd: textured playfield pixels come from art/materials, aligned to the
     tiles; every other pixel equals the scaled picture. */
  { BmFrame wide={{640u,400u,BM_PIXEL_RGB565},0,hd,640u*2u};
    unsigned textured=0,other_bad=0,tiles=0,tile_bad=0;
    bm_game_boot(&game,7u);
    for(f=0;f<6000u;++f){
      const BmWorldBuffer *w=&game.classic_world;
      int x0,y0;
      in.fire=(f%300u)<20u;bm_game_tick(&game,&in);
      if(f%250u||(w->impact_flash_ticks&2u))continue;
      bm_renderer_classic.render(&game,&classic);
      bm_renderer_scaled.render(&game,&big);
      bm_renderer_hd.render(&game,&wide);
      x0=(w->quake_shift?-2:0)+w->fine_phase*2-8;y0=-(int)w->fine_phase*2;
      for(y=0;y<400u;++y)for(x=0;x<640u;++x){
        unsigned cx=x/2u,cy=y/2u;
        uint8_t index=game.framebuffer.pixels[cy*BM_SCREEN_WIDTH+cx];
        const uint8_t reg[5]={w->colbk,w->colpf0,w->colpf1,w->colpf2,w->colpf3};
        const uint16_t *t=(cy<BM_ATARI_PLAYFIELD_HEIGHT&&index>=16u&&index<=20u)?
                          bm_art_materials[reg[index-16u]]:0;
        if(t){
          unsigned tx=(((unsigned)((int)cx-x0))&7u)*2u+(x&1u);
          unsigned ty=(((unsigned)((int)cy-y0))&7u)*2u+(y&1u);
          ++textured;
          if(hd[y*640u+x]!=t[ty*16u+tx])++other_bad;
        }else if(hd[y*640u+x]!=scaled[y*640u+x])++other_bad;
      }
      /* A full grass tile ($00 = PF0) shows the whole texture. */
      if(bm_art_materials[w->colpf0]){
        unsigned r,c;
        for(r=1;r<20u;++r)for(c=1;c<39u;++c){
          int sx=x0+(int)c*8,sy=y0+(int)r*8,dx,dy,clean=1;
          if(w->rows[r][w->source_column+c]!=0u)continue;
          for(dy=0;dy<8&&clean;++dy)for(dx=0;dx<8;++dx)
            if(game.framebuffer.pixels[(sy+dy)*BM_SCREEN_WIDTH+sx+dx]!=17u){clean=0;break;}
          if(!clean)continue;
          ++tiles;
          for(dy=0;dy<16;++dy)for(dx=0;dx<16;++dx)
            if(hd[(unsigned)(2*sy+dy)*640u+(unsigned)(2*sx+dx)]!=
               bm_art_materials[w->colpf0][dy*16+dx])++tile_bad;
        }
      }
    }
    check(textured>0u,"hd draws textured playfield pixels");
    check(!other_bad,"hd: textures where expected, the scaled picture elsewhere");
    check(tiles>0u&&!tile_bad,"hd: a grass tile shows the whole texture");
  }
  /* T36a: the classic renderer lists the objects it draws; a kind in
     bm_sprite_hd_mask is listed but not drawn, and only its own box of
     the picture changes. */
  { static BmFramebuffer with,without;
    BmFrame a={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&with,0,0u};
    BmFrame b={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&without,0,0u};
    unsigned players=0,checked=0,outside=0,inside=0,k;
    bm_game_boot(&game,7u);
    for(f=0;f<4000u;++f){
      in.fire=(f%300u)<20u;bm_game_tick(&game,&in);
      if(game.mode!=BM_STATE_PLAYING||f%97u)continue;
      bm_sprite_hd_mask=0u;bm_renderer_classic.render(&game,&a);
      for(players=0,k=0;k<bm_sprite_list.count;++k){
        const BmSpriteDraw *d=&bm_sprite_list.draw[k];
        if(d->kind!=BM_SPRITE_PLAYER)continue;
        ++players;
        if(d->x!=game.player.x-8||d->y!=game.player.y-5)++outside;
      }
      if(players!=1u)++outside;
      bm_sprite_hd_mask=1u<<BM_SPRITE_PLAYER;bm_renderer_classic.render(&game,&b);
      bm_sprite_hd_mask=0u;
      for(players=0,k=0;k<bm_sprite_list.count;++k)
        players+=bm_sprite_list.draw[k].kind==BM_SPRITE_PLAYER;
      if(players!=1u)++outside;
      for(y=0;y<BM_SCREEN_HEIGHT;++y)for(x=0;x<BM_SCREEN_WIDTH;++x){
        int box=(int)x>=game.player.x-8&&(int)x<game.player.x+8&&
                (int)y>=game.player.y-5&&(int)y<game.player.y+5;
        if(with.pixels[y*BM_SCREEN_WIDTH+x]==without.pixels[y*BM_SCREEN_WIDTH+x])continue;
        if(box)++inside;else ++outside;
      }
      ++checked;
    }
    check(checked>0u&&!outside,"sprite list: one player entry; masking only changes its box");
    check(inside>0u,"masking the player removes it from the classic picture");
  }
  if(failures){printf("%d render test(s) failed\n",failures);return 1;}
  printf("render tests passed\n");return 0;
}
