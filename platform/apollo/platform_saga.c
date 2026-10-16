#include "bm_platform.h"
#include "bm_frame.h"
#include "bm_ammx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>
#include <exec/interrupts.h>
#include <hardware/intbits.h>

#define REG16(address) (*(volatile uint16_t *)(uintptr_t)(address))
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define REG8(address) (*(volatile uint8_t *)(uintptr_t)(address))

enum {
    SAGA_MODULO_WRITE=0x00dff1e6u, SAGA_POINTER_WRITE=0x00dff1ecu,
    SAGA_MODE_WRITE=0x00dff1f4u, SAGA_MODULO_READ=0x00dfe1e6u,
    SAGA_POINTER_READ=0x00dfe1ecu, SAGA_MODE_READ=0x00dfe1f4u,
    SAGA_CHUNKY_COLOUR=0x00dff388u, INTREQR=0x00dff01eu,
    INTREQ=0x00dff09cu, JOY1DAT=0x00dff00cu, POTINP=0x00dff016u,
    CIAA_PRA=0x00bfe001u, CIAA_SDR=0x00bfec01u,
    SAGA_MODE_320X200_CLUT8=0x0101u,
    FRAME_BYTES=BM_SCREEN_WIDTH*BM_SCREEN_HEIGHT
};

/* SAGA audio channel 0 ($DFF400 + 16*n, see ApolloLib): pointer, length in
   64-bit units, volume L<<8|R, mode (bit 0 16-bit, bit 1 one-shot, bit 2
   stereo), period.  DMACON bit 0 and INTREQ bit 7 (AUD0) as on Paula.
   Measured on a V4: period 80 plays exactly 44336 Hz. */
enum {
    AUD0_POINTER=0x00dff400u, AUD0_LENGTH=0x00dff404u, AUD0_VOLUME=0x00dff408u,
    AUD0_MODE=0x00dff40au, AUD0_PERIOD=0x00dff40cu, DMACON=0x00dff096u,
    INTENA=0x00dff09au, INTENAR=0x00dff01cu,
    AUD0_INTERRUPT=0x0080u, AUD0_MODE_16BIT_STEREO_LOOP=0x0005u,
    AUDIO_PERIOD=80, AUDIO_RATE=3546895/AUDIO_PERIOD, AUDIO_VOLUME=0x8080u,
    /* One looped ring; the frame's samples are written AUDIO_LEAD ahead of
       the play position, which follows from the E-clock. */
    AUDIO_RING=8192, AUDIO_LEAD=2048, AUDIO_CHUNK=1024, AUDIO_MARGIN=64,
    AUDIO_RESYNC_LIMIT=1500
};

/* Vblank: AmigaOS services VERTB itself and clears the request bit, so a
   polling loop can miss it.  A server on the VERTB chain counts instead.
   It must return with Z set (moveq #0,d0) to let the chain continue. */
volatile uint32_t bm_vblank_count;
extern void bm_vblank_server(void);
__asm__(".text\n\t.even\n\t.globl _bm_vblank_server\n_bm_vblank_server:\n\t"
        "addq.l #1,_bm_vblank_count\n\tmoveq #0,d0\n\trts\n");
static struct Interrupt vblank_interrupt;

/* timer.device's E-clock (~709 kHz) is the audio position clock. */
struct Device *TimerBase;
static struct timerequest eclock_request;
static uint32_t eclock_hz;
static uint64_t eclock64(void){
    struct EClockVal v;ReadEClock(&v);return((uint64_t)v.ev_hi<<32)|v.ev_lo;
}

typedef struct {
    uint8_t storage[2][FRAME_BYTES+7];
    uint8_t *video[2];
    uint32_t palette[BM_PALETTE_SIZE],uploaded_palette[BM_PALETTE_SIZE],saved_pointer;
    uint16_t saved_mode,saved_modulo;
    uint8_t active,pending;
    uint32_t vblank_seen;
    int16_t audio_storage[2*AUDIO_RING+16];
    int16_t *ring,mono[AUDIO_CHUNK],extra[AUDIO_CHUNK];
    BmPokey *const *pokeys;
    unsigned pokey_count;
    uint64_t audio_t0,audio_written; /* E-clock at sample 0; samples written */
    uint64_t ticks_t0,ticks_done;    /* TICK_RATE=50: E-clock pacing */
    uint8_t audio_on,audio_failed,audio_intena,timer_open;
    /* Diagnostics, printed on exit by the SOUND_TEST build. */
    uint32_t frames,missed,late_frames,signals,resyncs,underruns,max_error;
    uint32_t work_ticks,work_max,render_ticks,render_samples;
    uint64_t work_start,started;
} SagaState;
static SagaState saga;
static void timer_open(SagaState *s);

static uint8_t *align64bit(uint8_t *p){uintptr_t v=(uintptr_t)p;return(uint8_t*)((v+7u)&~(uintptr_t)7u);}

static void copy_frame_for_saga(uint8_t *destination,const uint8_t *source){
    bm_ammx_copy8(destination,source,FRAME_BYTES);
}

static void remember_palette(const BmFramebuffer *fb){
    unsigned i;
    for(i=0;i<BM_PALETTE_SIZE;++i)saga.palette[i]=fb->palette[i]&0x00ffffffu;
}

static void upload_changed_palette(void){
    unsigned i;
    for(i=0;i<BM_PALETTE_SIZE;++i){
        if(saga.uploaded_palette[i]!=saga.palette[i]){
            REG32(SAGA_CHUNKY_COLOUR)=((uint32_t)i<<24)|saga.palette[i];
            saga.uploaded_palette[i]=saga.palette[i];
        }
    }
}

static int saga_open(BmPlatform *p,const BmFrame *frame){
    const BmFramebuffer *fb=frame->indexed;
    unsigned i;int ammx_result;
    /* The SAGA backend shows the classic 320x200 CLUT8 frame (T35b adds
       the larger direct-colour modes). */
    if(frame->format.pixel!=BM_PIXEL_CLUT8||frame->format.width!=BM_SCREEN_WIDTH||
       frame->format.height!=BM_SCREEN_HEIGHT||!fb){
        printf("Blue Max: display format %ux%u/%d not supported\n",
               (unsigned)frame->format.width,(unsigned)frame->format.height,
               (int)frame->format.pixel);
        return -1;
    }
    ammx_result=bm_ammx_selftest();
    if(ammx_result!=0){printf("Blue Max: AMMX self-test failed (%d)\n",ammx_result);return -1;}
    printf("Blue Max: AMMX self-test passed\n");
    bm_sprite_set_blitter(bm_sprite_blit_ammx);
    memset(&saga,0,sizeof(saga));
    timer_open(&saga);
    saga.video[0]=align64bit(saga.storage[0]);saga.video[1]=align64bit(saga.storage[1]);
    if(((uintptr_t)saga.video[0]&7u)||((uintptr_t)saga.video[1]&7u)){
        printf("Blue Max: SAGA framebuffer 64-bit alignment failed\n");return -1;
    }
    vblank_interrupt.is_Node.ln_Type=NT_INTERRUPT;vblank_interrupt.is_Node.ln_Pri=0;
    vblank_interrupt.is_Node.ln_Name=(char*)"Blue Max vblank";
    vblank_interrupt.is_Data=0;vblank_interrupt.is_Code=(void(*)())bm_vblank_server;
    AddIntServer(INTB_VERTB,&vblank_interrupt);saga.vblank_seen=bm_vblank_count;
    copy_frame_for_saga(saga.video[0],fb->pixels);
    copy_frame_for_saga(saga.video[1],fb->pixels);
    remember_palette(fb);
    for(i=0;i<BM_PALETTE_SIZE;++i)saga.uploaded_palette[i]=0xffffffffu;
    saga.saved_pointer=REG32(SAGA_POINTER_READ);saga.saved_mode=REG16(SAGA_MODE_READ);
    saga.saved_modulo=REG16(SAGA_MODULO_READ);
    /* Match ApolloShow: modulo and mode establish the fetch geometry before
       the display pointer is made live. */
    REG16(SAGA_MODULO_WRITE)=0;REG16(SAGA_MODE_WRITE)=SAGA_MODE_320X200_CLUT8;
    upload_changed_palette();
    REG32(SAGA_POINTER_WRITE)=(uint32_t)(uintptr_t)saga.video[0];p->userdata=&saga;return 0;
}

/* Keyboard: the OS keyboard handler acknowledges every byte, but CIA-A SDR
   keeps the last one.  Raw code = NOT(SDR) rotated right; bit 7 = key up.
   F2/F3/F4 stand for the Atari console keys OPTION/SELECT/START, as in
   Atari emulators; Q quits. */
static uint8_t saga_keys[128],saga_last_sdr=0xffu;
static void saga_poll_keyboard(void){
    uint8_t sdr=REG8(CIAA_SDR),code;
    if(sdr==saga_last_sdr)return;
    saga_last_sdr=sdr;
    code=(uint8_t)~(uint8_t)((sdr>>1)|(sdr<<7));
    saga_keys[code&0x7fu]=(uint8_t)!(code&0x80u);
}
static void saga_poll_input(BmPlatform *p,BmInput *in){
    uint16_t joy=REG16(JOY1DAT),pot=REG16(POTINP);
    uint8_t left=(uint8_t)((joy&0x0200u)!=0),right=(uint8_t)((joy&0x0002u)!=0);
    uint8_t up=(uint8_t)((((joy>>9)^(joy>>8))&1u)!=0);
    uint8_t down=(uint8_t)(((joy^(joy>>1))&1u)!=0);
    uint8_t fire1=(uint8_t)((REG8(CIAA_PRA)&0x80u)==0);
    uint8_t fire2=(uint8_t)((pot&0x4000u)==0);
    (void)p;memset(in,0,sizeof(*in));
    in->x=(int8_t)((int)right-(int)left);in->y=(int8_t)((int)down-(int)up);
    in->fire=fire1;
    in->bomb=fire2;
    saga_poll_keyboard();
    in->option=saga_keys[0x51u];in->select=saga_keys[0x52u];
    in->start=saga_keys[0x53u];
    in->pause=saga_keys[0x40u];      /* PAUSE: space bar (Atari key 33) */
    in->quit=saga_keys[0x10u];       /* Q leaves the game */
}

static void timer_open(SagaState *s){
    struct EClockVal v;
    if(OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_ECLOCK,(struct IORequest*)&eclock_request,0)!=0)return;
    TimerBase=eclock_request.tr_node.io_Device;eclock_hz=ReadEClock(&v);s->timer_open=1u;
    s->ticks_t0=eclock64();s->ticks_done=0u;
}
static uint64_t audio_position(const SagaState *s,uint64_t now){
    return now>s->audio_t0?(now-s->audio_t0)*AUDIO_RATE/eclock_hz:0u;
}
static void audio_start(SagaState *s){
    uintptr_t a=((uintptr_t)s->audio_storage+31u)&~(uintptr_t)31u;
    if(!s->timer_open){s->audio_failed=1u;return;}
    s->ring=(int16_t*)a;memset(s->ring,0,2u*AUDIO_RING*sizeof(int16_t));
    /* AUD0 is polled: a CPU interrupt for it would fire until the next poll
       and stall the game, so it is masked while Blue Max runs. */
    s->audio_intena=(uint8_t)((REG16(INTENAR)&AUD0_INTERRUPT)!=0);
    REG16(INTENA)=AUD0_INTERRUPT;
    REG16(DMACON)=0x0001u;
    REG32(AUD0_POINTER)=(uint32_t)(uintptr_t)s->ring;
    REG32(AUD0_LENGTH)=(uint32_t)(AUDIO_RING*4u/8u);
    REG16(AUD0_VOLUME)=AUDIO_VOLUME;REG16(AUD0_MODE)=AUD0_MODE_16BIT_STEREO_LOOP;
    REG16(AUD0_PERIOD)=AUDIO_PERIOD;REG16(INTREQ)=AUD0_INTERRUPT;
    s->audio_written=AUDIO_MARGIN;s->audio_on=1u;
    REG16(DMACON)=0x8001u;s->audio_t0=s->started=eclock64();
}
/* AUD0 rises when the ring restarts.  Seen at once (in the vblank wait), it
   pins the position to a multiple of the ring and corrects slow drift. */
static void audio_signal(SagaState *s,int fresh){
    uint64_t now,pos,loops;int64_t error;
    if(!(REG16(INTREQR)&AUD0_INTERRUPT))return;
    REG16(INTREQ)=AUD0_INTERRUPT;++s->signals;
    if(!fresh)return;
    now=eclock64();pos=audio_position(s,now);
    loops=(pos+AUDIO_RING/2)/AUDIO_RING;error=(int64_t)pos-(int64_t)(loops*AUDIO_RING);
    if(error<=-AUDIO_RESYNC_LIMIT||error>=AUDIO_RESYNC_LIMIT)return;
    s->audio_t0=(uint64_t)((int64_t)s->audio_t0+error*(int64_t)eclock_hz/AUDIO_RATE);
    ++s->resyncs;
    if(error<0)error=-error;
    if((uint32_t)error>s->max_error)s->max_error=(uint32_t)error;
}
static void audio_write(SagaState *s){
    uint64_t now=eclock64(),play=audio_position(s,now),target=play+AUDIO_LEAD;
    if(s->audio_written<play+AUDIO_MARGIN){++s->underruns;s->audio_written=play+AUDIO_MARGIN;}
    while(s->audio_written<target){
        unsigned off=(unsigned)(s->audio_written%AUDIO_RING),i;
        unsigned n=(unsigned)(target-s->audio_written);
        int16_t *out=s->ring+2u*off;
        if(n>AUDIO_CHUNK)n=AUDIO_CHUNK;
        if(n>AUDIO_RING-off)n=AUDIO_RING-off;
        if(s->pokey_count){
            unsigned p;
            bm_pokey_render(s->pokeys[0],s->mono,n,AUDIO_RATE);
            /* ENHANCED_SOUND: add the other POKEYs, saturating. */
            for(p=1;p<s->pokey_count;++p){
                bm_pokey_render(s->pokeys[p],s->extra,n,AUDIO_RATE);
                for(i=0;i<n;++i){
                    int32_t v=(int32_t)s->mono[i]+s->extra[i];
                    s->mono[i]=(int16_t)(v>32767?32767:v);
                }
            }
        }else memset(s->mono,0,n*sizeof(int16_t));
        for(i=0;i<n;++i){out[2*i]=s->mono[i];out[2*i+1]=s->mono[i];}
        s->audio_written+=n;s->render_samples+=n;
    }
    s->render_ticks+=(uint32_t)(eclock64()-now);
}

static void saga_present(BmPlatform *p,const BmFrame *frame){
    const BmFramebuffer *fb=frame->indexed;
    SagaState *s=(SagaState*)p->userdata;s->pending=(uint8_t)(s->active^1u);
    copy_frame_for_saga(s->video[s->pending],fb->pixels);remember_palette(fb);
}
static unsigned saga_wait_vblank(BmPlatform *p){
    SagaState *s=(SagaState*)p->userdata;uint32_t passed;
    if(bm_vblank_count!=s->vblank_seen)++s->late_frames;
    if(s->audio_on){
        uint64_t now=eclock64();uint32_t w;
        if(s->frames){w=(uint32_t)(now-s->work_start);s->work_ticks+=w;if(w>s->work_max)s->work_max=w;}
        audio_signal(s,0);                   /* raised during the frame: stale */
        while(bm_vblank_count==s->vblank_seen)audio_signal(s,1);
    }else while(bm_vblank_count==s->vblank_seen){}
    passed=bm_vblank_count-s->vblank_seen;s->vblank_seen+=passed;++s->frames;
    if(passed>1u)s->missed+=passed-1u;
    if(s->audio_on)s->work_start=eclock64();
    upload_changed_palette();
    REG32(SAGA_POINTER_WRITE)=(uint32_t)(uintptr_t)s->video[s->pending];s->active=s->pending;
#if BM_TICK_RATE == 50
    /* TICK_RATE=50: the updates follow the E-clock, not the display; on a
       60 Hz screen every sixth vblank has none, at most two per vblank. */
    if(s->timer_open){
        uint64_t due=(eclock64()-s->ticks_t0)*50u/eclock_hz;
        unsigned n=due>s->ticks_done?(unsigned)(due-s->ticks_done):0u;
        if(n>2u){s->ticks_done=due-2u;n=2u;}
        s->ticks_done+=n;
        return n;
    }
#endif
    return 1u;
}
static void saga_audio_tick(BmPlatform *p,BmPokey *const *pokeys,unsigned count){
    SagaState *s=(SagaState*)p->userdata;
    if(!s->audio_on&&!s->audio_failed)audio_start(s);
    if(!s->audio_on)return;
    s->pokeys=pokeys;s->pokey_count=count;audio_signal(s,0);audio_write(s);
}
static void saga_close(BmPlatform *p){
    SagaState *s=(SagaState*)p->userdata;
    if(s->audio_on){
        REG16(DMACON)=0x0001u;REG16(AUD0_VOLUME)=0u;REG16(INTREQ)=AUD0_INTERRUPT;
        if(s->audio_intena)REG16(INTENA)=(uint16_t)(0x8000u|AUD0_INTERRUPT);
#if BM_SOUND_TEST
        if(s->frames>1u&&s->render_samples){
            uint64_t now=eclock64();
            printf("Blue Max timing: seconds=%lu frames=%lu missed=%lu late=%lu work=%luus/frame max=%luus render=%luus/1000 samples\n",
                (unsigned long)((now-s->started)/eclock_hz),(unsigned long)s->frames,
                (unsigned long)s->missed,(unsigned long)s->late_frames,
                (unsigned long)((uint64_t)s->work_ticks*1000000u/eclock_hz/(s->frames-1u)),
                (unsigned long)((uint64_t)s->work_max*1000000u/eclock_hz),
                (unsigned long)((uint64_t)s->render_ticks*1000000000u/eclock_hz/s->render_samples));
            printf("Blue Max audio: signals=%lu resyncs=%lu max_error=%lu underruns=%lu intena=%u\n",
                (unsigned long)s->signals,(unsigned long)s->resyncs,(unsigned long)s->max_error,
                (unsigned long)s->underruns,(unsigned)s->audio_intena);
        }
#endif
    }
    if(s->audio_failed)printf("Blue Max: timer.device unavailable, no sound\n");
    if(s->timer_open){CloseDevice((struct IORequest*)&eclock_request);TimerBase=0;}
    RemIntServer(INTB_VERTB,&vblank_interrupt);
    REG16(SAGA_MODULO_WRITE)=s->saved_modulo;
    REG32(SAGA_POINTER_WRITE)=s->saved_pointer;REG16(SAGA_MODE_WRITE)=s->saved_mode;
    bm_sprite_set_blitter(0);
}
BmPlatform bm_platform_create(void){
    BmPlatform p;p.userdata=0;p.open=saga_open;p.poll_input=saga_poll_input;p.present=saga_present;
    p.audio_tick=saga_audio_tick;p.wait_vblank=saga_wait_vblank;p.close=saga_close;return p;
}
