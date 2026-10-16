#include "bm_pokey.h"
#include <string.h>

/* Polynomial counters, clocked by every machine cycle like the real chip:
   poly4 x^4+x^3+1, poly5 x^5+x^3+1, poly9 x^9+x^5+1, poly17 x^17+x^12+1. */
enum { POLY4=15, POLY5=31, POLY9=511, POLY17=131071 };
static uint8_t poly4[POLY4],poly5[POLY5],poly9[POLY9],poly17[(POLY17+7)/8];
static uint8_t polys_ready;

static void make_poly(uint8_t *bits,unsigned length,unsigned size,unsigned tap,int packed){
  uint32_t lfsr=1u;unsigned i;
  for(i=0;i<length;++i){
    unsigned bit=(unsigned)(lfsr&1u);
    if(packed){if(bit)bits[i>>3]|=(uint8_t)(1u<<(i&7u));}
    else bits[i]=(uint8_t)bit;
    bit=(unsigned)(((lfsr>>(size-1u))^(lfsr>>(tap-1u)))&1u);
    lfsr=((lfsr<<1)|bit)&((1u<<size)-1u);
  }
}
static void make_polys(void){
  if(polys_ready)return;
  make_poly(poly4,POLY4,4u,3u,0);make_poly(poly5,POLY5,5u,3u,0);
  make_poly(poly9,POLY9,9u,5u,0);make_poly(poly17,POLY17,17u,12u,1);
  polys_ready=1u;
}

void bm_pokey_reset(BmPokey *pokey){
  make_polys();memset(pokey,0,sizeof(*pokey));pokey->dirty=1u;
}
void bm_pokey_write(BmPokey *pokey,unsigned reg,uint8_t value){
  if(reg<8u){
    if(reg&1u)pokey->audc[reg>>1]=value;
    else{pokey->audf[reg>>1]=value;pokey->dirty=1u;}
  }else if(reg==BM_POKEY_AUDCTL){pokey->audctl=value;pokey->dirty=1u;}
  else if(reg==BM_POKEY_SKCTL)pokey->skctl=value;
}

/* Divider periods in machine cycles; 0 = channel feeds a 16-bit pair and
   makes no sound of its own. */
static void periods(const BmPokey *p,int32_t period[4]){
  int32_t base=(p->audctl&1u)?114:28;
  period[0]=(p->audctl&0x40u)?(int32_t)p->audf[0]+4:((int32_t)p->audf[0]+1)*base;
  period[1]=((int32_t)p->audf[1]+1)*base;
  period[2]=(p->audctl&0x20u)?(int32_t)p->audf[2]+4:((int32_t)p->audf[2]+1)*base;
  period[3]=((int32_t)p->audf[3]+1)*base;
  if(p->audctl&0x10u){
    int32_t pair=((int32_t)p->audf[1]<<8)|p->audf[0];
    period[1]=(p->audctl&0x40u)?pair+7:(pair+1)*base;period[0]=0;
  }
  if(p->audctl&0x08u){
    int32_t pair=((int32_t)p->audf[3]<<8)|p->audf[2];
    period[3]=(p->audctl&0x20u)?pair+7:(pair+1)*base;period[2]=0;
  }
}

static void underflow(BmPokey *p,unsigned c){
  uint8_t audc=p->audc[c];uint32_t t=p->cycle;
  if(c==2u&&(p->audctl&0x04u))p->highpass[0]=p->out[0];
  if(c==3u&&(p->audctl&0x02u))p->highpass[1]=p->out[1];
  if(!(audc&0x80u)&&!poly5[t%POLY5])return;
  if(audc&0x20u)p->out[c]^=1u;
  else if(audc&0x40u)p->out[c]=poly4[t%POLY4];
  else if(p->audctl&0x80u)p->out[c]=poly9[t%POLY9];
  else{uint32_t i=t%POLY17;p->out[c]=(uint8_t)((poly17[i>>3]>>(i&7u))&1u);}
}

static uint8_t poly_bit(const BmPokey *p,uint8_t audc){
  uint32_t t=p->cycle;
  if(audc&0x40u)return poly4[t%POLY4];
  if(p->audctl&0x80u)return poly9[t%POLY9];
  {uint32_t i=t%POLY17;return (uint8_t)((poly17[i>>3]>>(i&7u))&1u);}
}

/* How each channel is mixed in one render call. */
enum { MIX_OFF, MIX_EVENTS, MIX_FAST };

static unsigned channel_level(const BmPokey *p,unsigned c){
  unsigned on=p->out[c];
  if(c<2u&&(p->audctl&(c?0x02u:0x04u)))on^=p->highpass[c];
  return on?(p->audc[c]&15u)*2u:0u;
}

void bm_pokey_render(BmPokey *p,int16_t *out,unsigned count,uint32_t rate){
  int32_t period[4],fast;
  uint32_t step=(uint32_t)(((uint64_t)BM_POKEY_CLOCK<<16)/rate);
  unsigned n,c,mix[4],steady=0,events=0;
  periods(p,period);
  if(p->dirty){
    for(c=0;c<4u;++c)if(p->counter[c]<=0||p->counter[c]>period[c])p->counter[c]=period[c];
    p->dirty=0u;
  }
  /* Silent channels cost nothing, unless they clock an audible high-pass
     filter.  Dividers faster than one output sample are mixed per sample:
     a pure tone as its average, the polys as one bit per sample. */
  fast=(int32_t)(step>>16);
  for(c=0;c<4u;++c){
    unsigned vol=p->audc[c]&15u;
    int clocks_filter=(c==2u&&(p->audctl&0x04u)&&(p->audc[0]&15u))||
                      (c==3u&&(p->audctl&0x02u)&&(p->audc[1]&15u));
    mix[c]=MIX_OFF;
    if(vol&&(p->audc[c]&0x10u)){steady+=vol*2u;continue;}
    if(!period[c]||(!vol&&!clocks_filter))continue;
    if(period[c]<fast&&!clocks_filter&&!(c<2u&&(p->audctl&(c?0x02u:0x04u)))){
      if(p->audc[c]&0x20u)steady+=vol;else mix[c]=MIX_FAST;
      continue;
    }
    mix[c]=MIX_EVENTS;++events;
  }
  /* Between underflows the level is constant, so a sample without one is
     just a store; only samples with an underflow are integrated. */
  {
    uint32_t base=step>>16,recip[2];unsigned sum=0;
    recip[0]=(256u<<16)/base;recip[1]=(256u<<16)/(base+1u);
    for(c=0;c<4u;++c)if(mix[c]==MIX_EVENTS)sum+=channel_level(p,c);
    for(n=0;n<count;++n){
      int32_t cycles,next=0x7fffffff;unsigned noise=steady;
      p->fraction+=step;cycles=(int32_t)(p->fraction>>16);p->fraction&=0xffffu;
      for(c=0;c<4u;++c)
        if(mix[c]==MIX_FAST&&poly_bit(p,p->audc[c]))noise+=(p->audc[c]&15u)*2u;
      if(events)for(c=0;c<4u;++c)if(mix[c]==MIX_EVENTS&&p->counter[c]<next)next=p->counter[c];
      if(next>cycles){
        for(c=0;c<4u;++c)if(mix[c]==MIX_EVENTS)p->counter[c]-=cycles;
        p->cycle+=(uint32_t)cycles;
        out[n]=(int16_t)((sum+noise)*256u);
        continue;
      }
      {
        int32_t left=cycles;uint32_t acc=0;
        while(left>0){
          int32_t run=left;
          for(c=0;c<4u;++c)if(mix[c]==MIX_EVENTS&&p->counter[c]<run)run=p->counter[c];
          acc+=sum*(uint32_t)run;
          p->cycle+=(uint32_t)run;left-=run;
          for(c=0;c<4u;++c)if(mix[c]==MIX_EVENTS){
            p->counter[c]-=run;
            if(p->counter[c]<=0){underflow(p,c);p->counter[c]+=period[c];}
          }
          for(sum=0,c=0;c<4u;++c)if(mix[c]==MIX_EVENTS)sum+=channel_level(p,c);
        }
        /* 0..120 half-steps of volume, scaled to 0..30720. */
        out[n]=(int16_t)(((acc*recip[(uint32_t)cycles>base])>>16)+noise*256u);
      }
    }
  }
}
