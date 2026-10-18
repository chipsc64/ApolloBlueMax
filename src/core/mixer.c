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
  v->loop=loop;v->active=1u;
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
    /* 16.16 step: the sample's rate times pitch/256 over the output rate. */
    step=(uint32_t)(((uint64_t)v->sample->rate*v->pitch<<8)/rate);
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
