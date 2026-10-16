#include "bm_ammx.h"
#include "bm_renderer.h"
#include <stdio.h>
#include <string.h>
/* The AMMX row converters against the C reference: random lines with whole
   tiles of one index (textured and flat), mixed tiles, every tile phase,
   and guard words around the output.  Returns 0 when they match. */
enum { WIDTH=320, GUARD=4 };
static uint8_t in[WIDTH];
static uint32_t pairs[256],texture[5][16];
static const uint32_t *top_rows[256],*bottom_rows[256];
static uint32_t expected[2][WIDTH+2*GUARD],actual[2][WIDTH+2*GUARD];
static uint32_t seed=1983u;
static uint32_t next(void){seed=seed*1664525u+1013904223u;return seed>>8;}

static void wrap_scale(const BmRowJob *job){bm_ammx_scale_row(job);}
static void wrap_hd(const BmRowJob *job){bm_ammx_hd_row(job);}

static int run(BmRowConverter reference,BmRowConverter fast,const char *name,
               uint32_t phase){
  BmRowJob job;unsigned line,i;
  for(line=0;line<2u;++line)for(i=0;i<WIDTH+2u*GUARD;++i)
    expected[line][i]=actual[line][i]=0xdeadbeefu;
  job.in=in;job.pairs=pairs;job.line_top=top_rows;job.line_bottom=bottom_rows;
  job.phase=phase;job.count=WIDTH;
  job.top=expected[0]+GUARD;job.bottom=expected[1]+GUARD;reference(&job);
  job.top=actual[0]+GUARD;job.bottom=actual[1]+GUARD;fast(&job);
  for(line=0;line<2u;++line)for(i=0;i<WIDTH+2u*GUARD;++i)
    if(expected[line][i]!=actual[line][i]){
      printf("Blue Max: AMMX %s row mismatch (phase %lu, line %u, word %d)\n",
             name,(unsigned long)phase,line,(int)i-GUARD);
      return 1;
    }
  return 0;
}

int bm_ammx_row_selftest(void){
  unsigned i,t,round;uint32_t phase;
  for(i=0;i<256u;++i)pairs[i]=next()*2654435761u;
  for(t=0;t<5u;++t)for(i=0;i<16u;++i)texture[t][i]=next()^(next()<<12);
  /* Indices 16..20 are the playfield; 16 and 18 textured, the rest flat. */
  top_rows[16]=texture[0];bottom_rows[16]=texture[0]+8;
  top_rows[18]=texture[2];bottom_rows[18]=texture[2]+8;
  for(round=0;round<4u;++round){
    for(i=0;i<WIDTH;++i){
      uint32_t r=next();
      in[i]=(uint8_t)(round<2u?16u+(r%5u):(r&255u));
    }
    /* Whole tiles of one index at several places. */
    for(i=0;i<WIDTH;i+=24u){
      unsigned k,start=i+(round&3u);uint8_t v=(uint8_t)(16u+(next()%5u));
      for(k=0;k<16u&&start+k<WIDTH;++k)in[start+k]=v;
    }
    for(phase=0;phase<8u;++phase){
      if(run(bm_row_scale_c,wrap_scale,"scale",phase))return 1;
      if(run(bm_row_hd_c,wrap_hd,"hd",phase))return 2;
    }
  }
  return 0;
}
void bm_ammx_install_row_converters(void){
  bm_render_set_converters(wrap_scale,wrap_hd);
}
