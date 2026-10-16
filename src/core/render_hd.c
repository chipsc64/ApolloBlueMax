#include "bm_renderer.h"
#include "bm_tile.h"
#include "bm_art_assets.h"
/* RENDERER=hd: 640x400 RGB565.  The classic picture decides what is where;
   every playfield pixel whose Atari colour has a texture in art/materials
   is drawn from that 16x16 texture, one texture per tile, so the texture
   scrolls with the landscape.  Everything else (flat colours without a
   texture, sprites, the status line, flashes) is the classic picture 2x2. */
static BmFramebuffer classic;

static uint16_t rgb565(uint32_t rgb){
  return (uint16_t)(((rgb>>8)&0xf800u)|((rgb>>5)&0x07e0u)|((rgb>>3)&0x001fu));
}

static void hd_render(const BmGame *g,BmFrame *frame){
  const BmWorldBuffer *w=&g->classic_world;
  BmFrame source={{BM_SCREEN_WIDTH,BM_SCREEN_HEIGHT,BM_PIXEL_CLUT8},&classic,0,0u};
  const uint8_t registers[BM_CLASSIC_COLOUR_COUNT]={w->colbk,w->colpf0,w->colpf1,
                                                    w->colpf2,w->colpf3};
  const uint16_t *texture[BM_CLASSIC_COLOUR_COUNT]={0,0,0,0,0};
  uint16_t colour[BM_PALETTE_SIZE];
  /* Where bm_antic4_blit_playfield puts tile (0,0): fine scroll and QUAKE. */
  int x0=(w->quake_shift?-2:0)+w->fine_phase*2-8,y0=-(int)w->fine_phase*2;
  unsigned x,y,i,textured=0u;
  bm_renderer_classic.render(g,&source);
  for(i=0;i<BM_PALETTE_SIZE;++i)colour[i]=rgb565(classic.palette[i]);
  /* XFLSH swaps the playfield colours: keep the flash flat.  The OPTION
     screen is text. */
  if(g->mode!=BM_STATE_OPTIONS&&!(w->impact_flash_ticks&2u))
    for(i=0;i<BM_CLASSIC_COLOUR_COUNT;++i)
      if((texture[i]=bm_art_materials[registers[i]])!=0)textured=1u;
  for(y=0;y<BM_SCREEN_HEIGHT;++y){
    const uint8_t *in=classic.pixels+y*BM_SCREEN_WIDTH;
    uint16_t *top=(uint16_t *)((uint8_t *)frame->pixels+(2u*y)*frame->stride);
    uint16_t *bottom=(uint16_t *)((uint8_t *)frame->pixels+(2u*y+1u)*frame->stride);
    unsigned row=((unsigned)((int)y-y0)&7u)*2u*BM_ART_TILE;
    int play=textured&&y<BM_ATARI_PLAYFIELD_HEIGHT;
    for(x=0;x<BM_SCREEN_WIDTH;++x){
      uint8_t index=in[x];
      const uint16_t *t;
      if(play&&index>=BM_CLASSIC_INITIAL_BASE&&
         index<BM_CLASSIC_INITIAL_BASE+BM_CLASSIC_COLOUR_COUNT&&
         (t=texture[index-BM_CLASSIC_INITIAL_BASE])!=0){
        unsigned column=((unsigned)((int)x-x0)&7u)*2u;
        top[2u*x]=t[row+column];top[2u*x+1u]=t[row+column+1u];
        bottom[2u*x]=t[row+BM_ART_TILE+column];bottom[2u*x+1u]=t[row+BM_ART_TILE+column+1u];
      }else{
        uint16_t c=colour[index];
        top[2u*x]=c;top[2u*x+1u]=c;bottom[2u*x]=c;bottom[2u*x+1u]=c;
      }
    }
  }
}
const BmRenderer bm_renderer_hd={"hd",{640u,400u,BM_PIXEL_RGB565},hd_render};
