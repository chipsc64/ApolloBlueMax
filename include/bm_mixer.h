#ifndef BM_MIXER_H
#define BM_MIXER_H
#include <stdint.h>
/* Sample playback for SOUND=samples: a few voices, mixed into the same
   output as the software POKEY.  The game drives the voices through the
   bm_sound_event_* layer; the platform calls bm_mixer_render for every
   block of samples it renders. */
typedef struct { const int16_t *data; uint32_t length,rate; } BmSample;
enum { BM_MIXER_VOICES=8 };
typedef struct {
  const BmSample *sample;
  uint32_t index,frac;   /* position: index + frac/65536 */
  uint16_t pitch;        /* 256 = the sample's own rate (at most 4095) */
  uint16_t volume;       /* 256 = full scale */
  uint8_t loop,active;
} BmVoice;
/* starts: samples started since the last reset (diagnostics). */
typedef struct { BmVoice voice[BM_MIXER_VOICES]; uint32_t starts; } BmMixer;
void bm_mixer_reset(BmMixer *m);
/* Start a sample from its beginning on a voice (looped or once). */
void bm_mixer_play(BmMixer *m,unsigned voice,const BmSample *sample,uint8_t loop,
                   uint16_t volume);
/* A looped voice plays to the end of its current pass, then stops. */
void bm_mixer_release(BmMixer *m,unsigned voice);
void bm_mixer_stop(BmMixer *m,unsigned voice);
void bm_mixer_set(BmMixer *m,unsigned voice,uint16_t pitch,uint16_t volume);
uint8_t bm_mixer_active(const BmMixer *m,unsigned voice);
/* Add count samples at rate (Hz) to out, saturating. */
void bm_mixer_render(BmMixer *m,int16_t *out,unsigned count,uint32_t rate);
#endif
