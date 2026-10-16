#include "bm_renderer.h"
#include "bm_tile.h"
#include "bm_art_assets.h"
#include <string.h>
/* RENDERER=hd: 640x400 RGB565.  The classic picture decides what is where;
   every playfield pixel whose Atari colour has a texture in art/materials
   is drawn from that 16x16 texture, one texture per tile, so the texture
   scrolls with the landscape.  Everything else (flat colours without a
   texture, sprites, the status line, flashes) is the classic picture 2x2. */
static BmFramebuffer classic;

static uint16_t rgb565(uint32_t rgb){
  return (uint16_t)(((rgb>>8)&0xf800u)|((rgb>>5)&0x07e0u)|((rgb>>3)&0x001fu));
}

/* The textures as pairs of RGB565 pixels: one 32-bit store per Atari
   pixel and output line, as in the scaled renderer.  Row r, pair g. */
static uint32_t packed[256][BM_ART_TILE*BM_ART_TILE/2];
static uint8_t packed_ready;
static void pack_textures(void){
  unsigned c,i;
  /* memcpy keeps the left pixel first in memory on any byte order. */
  for(c=0;c<256u;++c)if(bm_art_materials[c])
    for(i=0;i<BM_ART_TILE*BM_ART_TILE/2u;++i)
      memcpy(&packed[c][i],&bm_art_materials[c][2u*i],sizeof(uint32_t));
  packed_ready=1u;
}

static void hd_render(const BmGame *g,BmFrame *frame){
  const BmWorldBuffer *w=&g->classic_world;
  BmFrame source={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&classic,0,0u};
  const uint8_t registers[BM_CLASSIC_COLOUR_COUNT]={w->colbk,w->colpf0,w->colpf1,
                                                    w->colpf2,w->colpf3};
  /* Per palette index: this line's texture rows (top, bottom), or 0. */
  static const uint32_t *line_top[BM_PALETTE_SIZE],*line_bottom[BM_PALETTE_SIZE];
  uint32_t pair[BM_PALETTE_SIZE];
  uint8_t textured[BM_CLASSIC_COLOUR_COUNT];
  /* Where bm_antic4_blit_playfield puts tile (0,0): fine scroll and QUAKE. */
  int x0=(w->quake_shift?-2:0)+w->fine_phase*2-8,y0=-(int)w->fine_phase*2;
  unsigned x,y,i,any=0u;
  if(!packed_ready)pack_textures();
  bm_renderer_classic.render(g,&source);
  for(i=0;i<BM_PALETTE_SIZE;++i){uint32_t c=rgb565(classic.palette[i]);pair[i]=c<<16|c;}
  /* XFLSH swaps the playfield colours: keep the flash flat.  The OPTION
     screen is text. */
  for(i=0;i<BM_CLASSIC_COLOUR_COUNT;++i){
    textured[i]=(uint8_t)(g->mode!=BM_STATE_OPTIONS&&!(w->impact_flash_ticks&2u)&&
                          bm_art_materials[registers[i]]);
    any|=textured[i];
  }
  for(y=0;y<BM_SCREEN_HEIGHT;++y){
    const uint8_t *in=classic.pixels+y*BM_SCREEN_WIDTH;
    uint32_t *top=(uint32_t *)((uint8_t *)frame->pixels+(2u*y)*frame->stride);
    uint32_t *bottom=(uint32_t *)((uint8_t *)frame->pixels+(2u*y+1u)*frame->stride);
    unsigned phase;
    if(!any||y>=BM_ATARI_PLAYFIELD_HEIGHT){
      for(x=0;x<BM_SCREEN_WIDTH;++x){uint32_t v=pair[in[x]];top[x]=v;bottom[x]=v;}
      continue;
    }
    { unsigned row=((unsigned)((int)y-y0)&7u)*2u*(BM_ART_TILE/2u);
      for(i=0;i<BM_CLASSIC_COLOUR_COUNT;++i){
        const uint32_t *t=textured[i]?packed[registers[i]]:0;
        line_top[BM_CLASSIC_INITIAL_BASE+i]=t?t+row:0;
        line_bottom[BM_CLASSIC_INITIAL_BASE+i]=t?t+row+BM_ART_TILE/2u:0;
      }
    }
    phase=(unsigned)(-x0)&7u;            /* (x - x0) & 7 at x = 0 */
    for(x=0;x<BM_SCREEN_WIDTH;++x){
      uint8_t index=in[x];
      const uint32_t *t=line_top[index];
      if(t){top[x]=t[phase];bottom[x]=line_bottom[index][phase];}
      else{uint32_t v=pair[index];top[x]=v;bottom[x]=v;}
      phase=(phase+1u)&7u;
    }
  }
}
const BmRenderer bm_renderer_hd={"hd",{640u,400u,BM_PIXEL_RGB565},hd_render};
