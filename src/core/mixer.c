#include "bm_mixer.h"
#include <string.h>

void bm_mixer_reset(BmMixer *m){memset(m,0,sizeof(*m));}

void bm_mixer_play(BmMixer *m,unsigned voice,const BmSample *sample,uint8_t loop,
                   uint16_t volume){
  BmVoice *v;
  if(voice>=BM_MIXER_VOICES)return;
  v=&m->voice[voice];
  if(!sample||!sample->length){v->active=0u;return;}
  v->sample=sample;v->index=0u;v->frac=0u;v->pitch=256u;v->volume=volume;
  v->loop=loop;v->active=1u;++m->starts;
}
void bm_mixer_release(BmMixer *m,unsigned voice){
  if(voice<BM_MIXER_VOICES)m->voice[voice].loop=0u;
}
void bm_mixer_stop(BmMixer *m,unsigned voice){
  if(voice<BM_MIXER_VOICES)m->voice[voice].active=0u;
}
void bm_mixer_set(BmMixer *m,unsigned voice,uint16_t pitch,uint16_t volume){
  if(voice>=BM_MIXER_VOICES)return;
  m->voice[voice].pitch=pitch;m->voice[voice].volume=volume;
}
uint8_t bm_mixer_active(const BmMixer *m,unsigned voice){
  return voice<BM_MIXER_VOICES&&m->voice[voice].active;
}

void bm_mixer_render(BmMixer *m,int16_t *out,unsigned count,uint32_t rate){
  unsigned n,i;
  if(!rate)return;
  for(n=0;n<BM_MIXER_VOICES;++n){
    BmVoice *v=&m->voice[n];
    const int16_t *data;
    uint32_t length,step,index,frac;
    int32_t volume;
    if(!v->active)continue;
    data=v->sample->data;length=v->sample->length;
    /* 16.16 step: the sample's rate over the output rate, times
       pitch/256.  32-bit only (rates below 65536, pitch below 16x): m68k
       GCC 6.5 dropped the low half of the 64-bit version, so the step
       was 0 on the Apollo. */
    step=(((uint32_t)v->sample->rate<<16)/rate*(v->pitch>4095u?4095u:v->pitch))>>8;
    index=v->index;frac=v->frac;volume=v->volume;
    for(i=0;i<count;++i){
      int32_t s=out[i]+((int32_t)data[index]*volume>>8);
      out[i]=(int16_t)(s>32767?32767:(s<-32768?-32768:s));
      frac+=step;index+=frac>>16;frac&=0xffffu;
      if(index>=length){
        if(!v->loop){v->active=0u;break;}
        index%=length;
      }
    }
    v->index=index;v->frac=frac;
  }
}

int bm_mixer_selftest(void){
  static const int16_t ramp[4]={1000,2000,3000,4000},loud[2]={20000,-20000};
  static const BmSample once={ramp,4u,44100u},half={ramp,4u,22050u},
                        peak={loud,2u,44100u};
  BmMixer m;int16_t out[8];unsigned i;
  bm_mixer_reset(&m);
  for(i=0;i<8u;++i)out[i]=0;
  bm_mixer_play(&m,0u,&once,0u,256u);bm_mixer_render(&m,out,8u,44100u);
  if(out[0]!=1000||out[3]!=4000||out[4]!=0)return 1;           /* stepping */
  if(bm_mixer_active(&m,0u))return 2;                             /* the end */
  for(i=0;i<8u;++i)out[i]=0;
  bm_mixer_play(&m,0u,&half,0u,256u);bm_mixer_render(&m,out,8u,44100u);
  if(out[0]!=1000||out[1]!=1000||out[2]!=2000||out[7]!=4000)return 3;  /* rate */
  for(i=0;i<8u;++i)out[i]=0;
  bm_mixer_play(&m,0u,&once,1u,128u);bm_mixer_render(&m,out,8u,44100u);
  if(out[4]!=500||out[7]!=2000)return 4;                          /* loop */
  out[0]=30000;out[1]=-30000;out[2]=100;out[3]=-100;
  bm_mixer_play(&m,0u,&peak,1u,256u);bm_mixer_render(&m,out,4u,44100u);
  if(out[0]!=32767||out[1]!=-32768)return 5;                      /* saturation */
  if(out[2]!=20100||out[3]!=-20100)return 6;                      /* in range */
  return 0;
}
