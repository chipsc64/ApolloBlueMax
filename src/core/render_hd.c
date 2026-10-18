#include "bm_renderer.h"
#include "bm_profile.h"
#include "bm_tile.h"
#include "bm_art_assets.h"
#include "bm_chbas_assets.h"
#include <string.h>
/* The art tool and the renderer agree on the sprite kinds. */
typedef char bm_art_sprite_kinds_check[(int)BM_ART_SPRITE_KINDS==(int)BM_SPRITE_KINDS?1:-1];

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

/* Eight palette indices as two 32-bit words.  The 68080 reads unaligned
   longwords directly; elsewhere memcpy keeps it portable. */
#if defined(__mc68000__)
typedef uint32_t __attribute__((may_alias)) bm_u32_alias;
static inline void load8(const uint8_t *p,uint32_t *a,uint32_t *b){
  *a=((const bm_u32_alias *)p)[0];*b=((const bm_u32_alias *)p)[1];
}
#else
static inline void load8(const uint8_t *p,uint32_t *a,uint32_t *b){
  memcpy(a,p,4u);memcpy(b,p+4u,4u);
}
#endif

/* Tiles start at x0 + 8k.  Pixels before the first whole tile and after
   the last one go one by one; a whole tile whose eight pixels share one
   index is copied from its texture row (or filled), the rest of the mixed
   tiles go one by one. */
void bm_row_hd_c(const BmRowJob *job){
  const uint8_t *in=job->in;
  uint32_t *top=job->top,*bottom=job->bottom;
  const uint32_t *pair=job->pairs;
  uint32_t x=0u,phase=job->phase;
  while(x<job->count){
    if(!phase&&x+8u<=job->count){
      uint32_t a,b,same;
      load8(in+x,&a,&b);
      same=(uint32_t)in[x]*0x01010101u;
      if(a==same&&b==same){
        const uint32_t *t=job->line_top[in[x]];
        uint32_t *o=top+x,*u=bottom+x;
        if(t){
          const uint32_t *v=job->line_bottom[in[x]];
          o[0]=t[0];o[1]=t[1];o[2]=t[2];o[3]=t[3];o[4]=t[4];o[5]=t[5];o[6]=t[6];o[7]=t[7];
          u[0]=v[0];u[1]=v[1];u[2]=v[2];u[3]=v[3];u[4]=v[4];u[5]=v[5];u[6]=v[6];u[7]=v[7];
        }else{
          uint32_t c=pair[in[x]];
          o[0]=o[1]=o[2]=o[3]=o[4]=o[5]=o[6]=o[7]=c;
          u[0]=u[1]=u[2]=u[3]=u[4]=u[5]=u[6]=u[7]=c;
        }
        x+=8u;
        continue;
      }
    }
    { uint8_t index=in[x];
      const uint32_t *t=job->line_top[index];
      if(t){top[x]=t[phase];bottom[x]=job->line_bottom[index][phase];}
      else{uint32_t v=pair[index];top[x]=v;bottom[x]=v;}
    }
    ++x;phase=(phase+1u)&7u;
  }
}
BmRowConverter bm_row_hd=bm_row_hd_c;
extern BmRowConverter bm_row_scale;

/* Objects with art/sprites images: the classic renderer has left them out;
   they are drawn here at twice their place, in the classic order.  Grey
   pixels take the object's Atari colour; an object behind the playfield
   (GPRIOR 4) only shows where the classic picture has COLBK. */
static uint16_t tint(uint32_t rgb,uint16_t grey){
  uint32_t r=((rgb>>16)&255u)*grey/255u,g=((rgb>>8)&255u)*grey/255u,b=(rgb&255u)*grey/255u;
  return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
}
static void draw_sprites(BmFrame *frame){
  unsigned k;
  for(k=0;k<bm_sprite_list.count;++k){
    const BmSpriteDraw *d=&bm_sprite_list.draw[k];
    const BmArtSprite *a;
    uint32_t colour;
    int r,c;
    if(!((uint32_t)BM_ART_SPRITE_COMPLETE&(1u<<d->kind)))continue;
    a=bm_art_sprites[d->kind][d->variant%BM_ART_SPRITE_VARIANTS];
    if(!a)continue;
    colour=d->colour?bm_atari_pal_rgb[d->colour]:0u;
    for(r=0;r<a->height;++r){
      int y=2*d->y+r;
      uint16_t *line;
      if((unsigned)y>=frame->format.height||y>=2*d->bottom)continue;
      line=(uint16_t *)((uint8_t *)frame->pixels+(unsigned)y*frame->stride);
      for(c=0;c<a->width<<d->wide;++c){
        int x=2*d->x+c;
        unsigned i=(unsigned)(r*a->width+(c>>d->wide));
        uint8_t m=a->mask[i];
        if(!m||(unsigned)x>=frame->format.width)continue;
        if(d->behind){
          uint8_t under=classic.pixels[(unsigned)(y/2)*BM_SCREEN_WIDTH+(unsigned)(x/2)];
          if(under!=BM_CLASSIC_INITIAL_BASE&&under!=BM_CLASSIC_DLI_BASE)continue;
        }
        line[x]=m==1u?a->pixels[i]:
                colour?tint(colour,a->pixels[i]):tint(0xffffffu,a->pixels[i]);
      }
    }
  }
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
  unsigned y,i,any=0u;
  BmRowJob job;
  uint32_t profile;
  if(!packed_ready)pack_textures();
  bm_sprite_hd_mask=BM_ART_SPRITE_COMPLETE;
  bm_renderer_classic.render(g,&source);
  bm_sprite_hd_mask=0u;
  profile=bm_profile_start();
  for(i=0;i<BM_PALETTE_SIZE;++i){uint32_t c=rgb565(classic.palette[i]);pair[i]=c<<16|c;}
  /* XFLSH swaps the playfield colours: keep the flash flat.  The OPTION
     screen is text. */
  for(i=0;i<BM_CLASSIC_COLOUR_COUNT;++i){
    textured[i]=(uint8_t)(g->mode!=BM_STATE_OPTIONS&&!(w->impact_flash_ticks&2u)&&
                          bm_art_materials[registers[i]]);
    any|=textured[i];
  }
  job.pairs=pair;job.line_top=line_top;job.line_bottom=line_bottom;
  job.count=BM_SCREEN_WIDTH;
  job.phase=(uint32_t)(-x0)&7u;          /* (x - x0) & 7 at x = 0 */
  for(y=0;y<BM_SCREEN_HEIGHT;++y){
    job.in=classic.pixels+y*BM_SCREEN_WIDTH;
    job.top=(uint32_t *)((uint8_t *)frame->pixels+(2u*y)*frame->stride);
    job.bottom=(uint32_t *)((uint8_t *)frame->pixels+(2u*y+1u)*frame->stride);
    if(!any||y>=BM_ATARI_PLAYFIELD_HEIGHT){bm_row_scale(&job);continue;}
    { unsigned row=((unsigned)((int)y-y0)&7u)*2u*(BM_ART_TILE/2u);
      for(i=0;i<BM_CLASSIC_COLOUR_COUNT;++i){
        const uint32_t *t=textured[i]?packed[registers[i]]:0;
        line_top[BM_CLASSIC_INITIAL_BASE+i]=t?t+row:0;
        line_bottom[BM_CLASSIC_INITIAL_BASE+i]=t?t+row+BM_ART_TILE/2u:0;
      }
    }
    bm_row_hd(&job);
  }
  draw_sprites(frame);
  bm_profile_stop(BM_PROFILE_CONVERT,profile);
}
const BmRenderer bm_renderer_hd={"hd",{640u,400u,BM_PIXEL_RGB565},hd_render};
