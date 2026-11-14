#include "bm_platform.h"
#include "bm_frame.h"
#include "bm_profile.h"
#include "bm_renderer.h"
#include "bm_ammx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <intuition/intuition.h>
#include <devices/timer.h>
#include <exec/interrupts.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <hardware/intbits.h>

#define REG16(address) (*(volatile uint16_t *)(uintptr_t)(address))
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define REG8(address) (*(volatile uint8_t *)(uintptr_t)(address))

enum {
    VPOSR=0x00dff004u, VHPOSR=0x00dff006u,
    SAGA_MODULO_WRITE=0x00dff1e6u, SAGA_POINTER_WRITE=0x00dff1ecu,
    SAGA_MODE_WRITE=0x00dff1f4u, SAGA_MODULO_READ=0x00dfe1e6u,
    SAGA_POINTER_READ=0x00dfe1ecu, SAGA_MODE_READ=0x00dfe1f4u,
    SAGA_CHUNKY_COLOUR=0x00dff388u, INTREQR=0x00dff01eu,
    INTREQ=0x00dff09cu, JOY1DAT=0x00dff00cu, POTINP=0x00dff016u,
    CIAA_PRA=0x00bfe001u, CIAA_SDR=0x00bfec01u,
    /* Triple buffering: one buffer shown, one queued for the next vblank,
       one to draw into.  Sized for the largest supported frame. */
    VIDEO_BUFFERS=3, NO_BUFFER=255, MAX_FRAME_BYTES=640*400*2
};

/* SAGA display mode word ($DFF1F4): resolution << 8 | pixel format (see
   ApolloLib: 320x200 = $01, 640x400 = $04; CLUT8 = $01, R5G6B5 = $02,
   A8R8G8B8 = $05). */
static uint16_t saga_mode_for(const BmFrameFormat *f){
    uint16_t resolution=0u,format=0u;
    if(f->width==320u&&f->height==200u)resolution=0x01u;
    else if(f->width==640u&&f->height==400u)resolution=0x04u;
    if(f->pixel==BM_PIXEL_CLUT8)format=0x01u;
    else if(f->pixel==BM_PIXEL_RGB565)format=0x02u;
    else if(f->pixel==BM_PIXEL_ARGB32)format=0x05u;
    return resolution&&format?(uint16_t)(resolution<<8|format):0u;
}
static uint32_t frame_bytes(const BmFrameFormat *f){
    return (uint32_t)f->width*f->height*(f->pixel==BM_PIXEL_CLUT8?1u:f->pixel==BM_PIXEL_RGB565?2u:4u);
}

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
struct IntuitionBase *IntuitionBase;

/* The mouse pointer: the game takes the display over, but for the OS the
   Workbench screen stays in front and Intuition keeps drawing its pointer.
   A borderless window over the whole Workbench screen, active and with an
   empty pointer, hides it; it also takes the keys the Shell would get and
   keeps the right mouse button from opening menus.  Closing it restores
   the Workbench pointer. */
static struct Window *pointer_window;
static UWORD *blank_pointer;    /* chip RAM: sprite control words + 1 empty line */
static void pointer_hide(void){
    struct Screen *screen;
    IntuitionBase=(struct IntuitionBase*)OpenLibrary((CONST_STRPTR)"intuition.library",36);
    if(!IntuitionBase)return;
    screen=LockPubScreen(0);
    if(!screen)return;
    pointer_window=OpenWindowTags(0,WA_PubScreen,(ULONG)screen,WA_Left,0,WA_Top,0,
        WA_Width,(ULONG)screen->Width,WA_Height,(ULONG)screen->Height,
        WA_Borderless,TRUE,WA_Activate,TRUE,WA_RMBTrap,TRUE,
        WA_SimpleRefresh,TRUE,WA_NoCareRefresh,TRUE,WA_Backdrop,FALSE,TAG_END);
    UnlockPubScreen(0,screen);
    if(!pointer_window)return;
    blank_pointer=(UWORD*)AllocMem(6u*sizeof(UWORD),MEMF_CHIP|MEMF_CLEAR);
    if(blank_pointer)SetPointer(pointer_window,blank_pointer,1,16,0,0);
}
static void pointer_show(void){
    if(pointer_window){
        if(blank_pointer)ClearPointer(pointer_window);
        CloseWindow(pointer_window);pointer_window=0;
    }
    if(blank_pointer){FreeMem(blank_pointer,6u*sizeof(UWORD));blank_pointer=0;}
    if(IntuitionBase){CloseLibrary((struct Library*)IntuitionBase);IntuitionBase=0;}
}
static struct timerequest eclock_request;
static uint32_t eclock_hz;
/* The display swap's delay after the vertical blank interrupt, in
   microseconds (0: at once).  The V4 takes a new display pointer at once
   (mid-picture: tearing), and its vertical blank interrupt comes about a
   third into the HDMI picture.  At 640x400 a swap 10 ms later falls in the
   blanking period (found with tools/tear_test.c on a V4).  The game can set it with delay=N on start. */
uint32_t bm_swap_delay_us=10000u;

/* The delayed swap: timer.device (UNIT_WAITECLOCK) replies at the E-clock
   time vblank + delay to a port that causes a software interrupt; that
   writes the pending display pointer.  The game keeps running meanwhile. */
volatile uint32_t bm_swap_pointer;      /* pending display pointer, 0: none */
extern void bm_swap_softint(void);
__asm__(".text\n\t.even\n\t.globl _bm_swap_softint\n_bm_swap_softint:\n\t"
        "move.l _bm_swap_pointer,d0\n\tbeq.s 1f\n\t"
        "move.l d0,0xdff1ec\n\tclr.l _bm_swap_pointer\n"
        "1:\tmoveq #0,d0\n\trts\n");
static struct Interrupt swap_interrupt;
static struct MsgPort swap_port;
static struct timerequest swap_request;
static uint8_t swap_open,swap_busy;
static uint64_t eclock64(void){
    struct EClockVal v;ReadEClock(&v);return((uint64_t)v.ev_hi<<32)|v.ev_lo;
}
#if BM_SOUND_TEST
static uint32_t eclock_low(void){return (uint32_t)eclock64();}
#endif

typedef struct {
    uint8_t storage[VIDEO_BUFFERS][MAX_FRAME_BYTES+63];
    uint8_t *video[VIDEO_BUFFERS];
    BmFrameFormat format;
    uint32_t bytes;
    uint8_t shown,queued,drawing;
    uint8_t leaving;                 /* still on screen until the delayed swap */
    uint32_t palette[BM_PALETTE_SIZE],uploaded_palette[BM_PALETTE_SIZE],saved_pointer;
    uint16_t saved_mode,saved_modulo;
    uint32_t vblank_seen;
    int16_t audio_storage[2*AUDIO_RING+16];
    int16_t *ring,mono[AUDIO_CHUNK],extra[AUDIO_CHUNK];
    BmPokey *const *pokeys;
    unsigned pokey_count;
    BmMixer *mixer;                  /* SOUND=samples: sample voices */
    uint64_t audio_t0,audio_written; /* E-clock at sample 0; samples written */
    uint64_t ticks_t0,ticks_done;    /* TICK_RATE=50: E-clock pacing */
    uint8_t audio_on,audio_failed,audio_intena,timer_open;
    /* Diagnostics, printed on exit by the SOUND_TEST build. */
    uint32_t frames,missed,late_frames,signals,resyncs,underruns,max_error;
    uint32_t work_ticks,work_max,render_ticks,render_samples;
    uint32_t sample_peak;            /* SOUND_TEST: loudest sample output */
    /* SOUND_TEST: vblanks seen since start and the shortest/longest gap
       between two (E-clock ticks), to compare with the display's rate. */
    uint32_t vbl_start,vbl_gap_min,vbl_gap_max;
    uint64_t vbl_last;
    /* SOUND_TEST: the beam's line (VPOSR/VHPOSR) when the display pointer
       is written: in the vertical blank no frame can tear. */
    uint32_t beam_count,beam_hist[16];
    uint16_t beam_min,beam_max;
    uint32_t draw_ticks,draw_max,draw_frames;   /* acquire -> present */
    uint64_t draw_start;
    uint32_t draw_misses,miss_start,cpu_last,eclock_last;  /* SOUND_TEST */
    uint64_t cpu_sum,eclock_sum;
    uint64_t work_start,started;
} SagaState;
static SagaState saga;
static void timer_open(SagaState *s);

static uint8_t *align64(uint8_t *p){uintptr_t v=(uintptr_t)p;return(uint8_t*)((v+63u)&~(uintptr_t)63u);}

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

/* A swap still waiting (a late frame): take it back and show it now. */
static void swap_finish(void){
    if(swap_busy){
        if(!CheckIO((struct IORequest*)&swap_request))AbortIO((struct IORequest*)&swap_request);
        while(!CheckIO((struct IORequest*)&swap_request)){}
        WaitIO((struct IORequest*)&swap_request);   /* done: only removes the reply */
        swap_busy=0u;
    }
    if(bm_swap_pointer){REG32(SAGA_POINTER_WRITE)=bm_swap_pointer;bm_swap_pointer=0u;}
}
static void swap_open_timer(void){
    swap_interrupt.is_Node.ln_Type=NT_INTERRUPT;swap_interrupt.is_Node.ln_Pri=32;
    swap_interrupt.is_Node.ln_Name=(char*)"Blue Max swap";
    swap_interrupt.is_Data=0;swap_interrupt.is_Code=(void(*)())bm_swap_softint;
    swap_port.mp_Node.ln_Type=NT_MSGPORT;swap_port.mp_Flags=PA_SOFTINT;
    swap_port.mp_SigTask=(struct Task*)&swap_interrupt;
    swap_port.mp_MsgList.lh_Head=(struct Node*)&swap_port.mp_MsgList.lh_Tail;  /* NewList */
    swap_port.mp_MsgList.lh_Tail=0;
    swap_port.mp_MsgList.lh_TailPred=(struct Node*)&swap_port.mp_MsgList.lh_Head;
    swap_request.tr_node.io_Message.mn_Node.ln_Type=NT_REPLYMSG;
    swap_request.tr_node.io_Message.mn_ReplyPort=&swap_port;
    swap_request.tr_node.io_Message.mn_Length=sizeof(swap_request);
    swap_open=(uint8_t)(OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_WAITECLOCK,
                                   (struct IORequest*)&swap_request,0)==0);
}
static void swap_close_timer(void){
    swap_finish();
    if(swap_open){CloseDevice((struct IORequest*)&swap_request);swap_open=0u;}
}
static int saga_open(BmPlatform *p,const BmFrame *frame){
    unsigned i;int ammx_result;
    uint16_t mode=saga_mode_for(&frame->format);
    if(!mode||frame_bytes(&frame->format)>MAX_FRAME_BYTES){
        printf("Blue Max: display format %ux%u/%d not supported\n",
               (unsigned)frame->format.width,(unsigned)frame->format.height,
               (int)frame->format.pixel);
        return -1;
    }
    /* The Apollo bit (SR bit 11): a ROM-patched AmigaOS such as Coffin
       keeps this task's AMMX registers across task switches only when it
       is set; ApolloOS always does.  It affects only this program. */
    SetSR(0x800u,0x800u);
    ammx_result=bm_ammx_selftest();
    if(ammx_result!=0){printf("Blue Max: AMMX self-test failed (%d)\n",ammx_result);return -1;}
    printf("Blue Max: AMMX self-test passed\n");
#if BM_SOUND_SAMPLES
    { int mixer_result=bm_mixer_selftest();
      if(mixer_result)printf("Blue Max: mixer self-test FAILED (%d)\n",mixer_result);
      else printf("Blue Max: mixer self-test passed\n"); }
#endif
    bm_sprite_set_blitter(bm_sprite_blit_ammx);
    /* The 640x400 conversion: AMMX only when it matches the C reference. */
    if(bm_ammx_row_selftest()==0)bm_ammx_install_row_converters();
    else printf("Blue Max: AMMX row conversion off, using C\n");
    memset(&saga,0,sizeof(saga));
    timer_open(&saga);
    if(saga.timer_open)swap_open_timer();
#if BM_SOUND_TEST
    bm_profile_cycles=eclock_low;
#endif
    saga.format=frame->format;saga.bytes=frame_bytes(&frame->format);
    for(i=0;i<VIDEO_BUFFERS;++i){
        saga.video[i]=align64(saga.storage[i]);memset(saga.video[i],0,saga.bytes);
    }
    saga.shown=0u;saga.queued=NO_BUFFER;saga.drawing=1u;
    vblank_interrupt.is_Node.ln_Type=NT_INTERRUPT;vblank_interrupt.is_Node.ln_Pri=0;
    vblank_interrupt.is_Node.ln_Name=(char*)"Blue Max vblank";
    vblank_interrupt.is_Data=0;vblank_interrupt.is_Code=(void(*)())bm_vblank_server;
    AddIntServer(INTB_VERTB,&vblank_interrupt);saga.vblank_seen=bm_vblank_count;
    if(frame->indexed)remember_palette(frame->indexed);
    for(i=0;i<BM_PALETTE_SIZE;++i)saga.uploaded_palette[i]=0xffffffffu;
    saga.saved_pointer=REG32(SAGA_POINTER_READ);saga.saved_mode=REG16(SAGA_MODE_READ);
    saga.saved_modulo=REG16(SAGA_MODULO_READ);
    /* Match ApolloShow: modulo and mode establish the fetch geometry before
       the display pointer is made live. */
    REG16(SAGA_MODULO_WRITE)=0;REG16(SAGA_MODE_WRITE)=mode;
    if(saga.format.pixel==BM_PIXEL_CLUT8)upload_changed_palette();
    REG32(SAGA_POINTER_WRITE)=(uint32_t)(uintptr_t)saga.video[saga.shown];p->userdata=&saga;
    pointer_hide();
    return 0;
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
        if(s->mixer){
#if BM_SOUND_TEST
            /* The samples alone first, for their peak level. */
            memset(s->extra,0,n*sizeof(int16_t));
            bm_mixer_render(s->mixer,s->extra,n,AUDIO_RATE);
            for(i=0;i<n;++i){
                int32_t v=(int32_t)s->mono[i]+s->extra[i],a=s->extra[i]<0?-s->extra[i]:s->extra[i];
                if((uint32_t)a>s->sample_peak)s->sample_peak=(uint32_t)a;
                s->mono[i]=(int16_t)(v>32767?32767:(v<-32768?-32768:v));
            }
#else
            bm_mixer_render(s->mixer,s->mono,n,AUDIO_RATE);
#endif
        }
        for(i=0;i<n;++i){out[2*i]=s->mono[i];out[2*i+1]=s->mono[i];}
        s->audio_written+=n;s->render_samples+=n;
    }
    s->render_ticks+=(uint32_t)(eclock64()-now);
}

/* The buffer that is neither shown nor queued. */
static void saga_acquire(BmPlatform *p,BmFrame *frame){
    SagaState *s=(SagaState*)p->userdata;uint8_t i;
    for(i=0;i<VIDEO_BUFFERS;++i)
        if(i!=s->shown&&i!=s->queued&&!(bm_swap_pointer&&i==s->leaving))break;
    s->drawing=i;
    if(s->timer_open)s->draw_start=eclock64();
#if BM_SOUND_TEST
    s->miss_start=bm_cpu_cache_misses();
#endif
    if(frame->format.pixel!=BM_PIXEL_CLUT8){
        frame->pixels=s->video[i];
        frame->stride=(uint32_t)frame->format.width*(frame->format.pixel==BM_PIXEL_RGB565?2u:4u);
    }
}
/* The classic frame is drawn in system memory and copied (AMMX); a direct
   frame is already in the buffer.  Either is shown at the next vblank. */
static void saga_present(BmPlatform *p,const BmFrame *frame){
    SagaState *s=(SagaState*)p->userdata;
    if(frame->format.pixel==BM_PIXEL_CLUT8){
        uint32_t profile=bm_profile_start();
        bm_ammx_copy8(s->video[s->drawing],frame->indexed->pixels,s->bytes);
        remember_palette(frame->indexed);
        bm_profile_stop(BM_PROFILE_PRESENT,profile);
    }
    s->queued=s->drawing;
    if(s->timer_open&&s->draw_start){
        uint32_t d=(uint32_t)(eclock64()-s->draw_start);
        s->draw_ticks+=d;if(d>s->draw_max)s->draw_max=d;++s->draw_frames;
#if BM_SOUND_TEST
        s->draw_misses+=bm_cpu_cache_misses()-s->miss_start;
#endif
    }
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
#if BM_SOUND_TEST
    if(s->timer_open&&passed==1u){
        uint64_t t=eclock64();
        if(s->vbl_last){
            uint32_t gap=(uint32_t)(t-s->vbl_last);
            if(!s->vbl_gap_min||gap<s->vbl_gap_min)s->vbl_gap_min=gap;
            if(gap>s->vbl_gap_max)s->vbl_gap_max=gap;
        }else s->vbl_start=bm_vblank_count;
        s->vbl_last=t;
    }else s->vbl_last=0u;
#endif
    if(passed>1u)s->missed+=passed-1u;
    if(s->audio_on)s->work_start=eclock64();
#if BM_SOUND_TEST
    if(s->timer_open){
        uint32_t c=bm_cpu_cycles(),e=(uint32_t)s->work_start;
        if(s->cpu_last){s->cpu_sum+=c-s->cpu_last;s->eclock_sum+=e-s->eclock_last;}
        s->cpu_last=c;s->eclock_last=e;
    }
#endif
    if(s->queued!=NO_BUFFER){
        if(s->format.pixel==BM_PIXEL_CLUT8)upload_changed_palette();
#if BM_SOUND_TEST
        { uint16_t v=REG16(VPOSR),h=REG16(VHPOSR),line=(uint16_t)(((v&7u)<<8)|(h>>8));
          if(!s->beam_count||line<s->beam_min)s->beam_min=line;
          if(line>s->beam_max)s->beam_max=line;
          ++s->beam_hist[line>>6&15u];++s->beam_count; }
#endif
        if(bm_swap_delay_us&&swap_open){
            uint64_t at=eclock64()+(uint32_t)((uint64_t)bm_swap_delay_us*eclock_hz/1000000u);
            swap_finish();
            s->leaving=s->shown;
            bm_swap_pointer=(uint32_t)(uintptr_t)s->video[s->queued];
            swap_request.tr_node.io_Command=TR_ADDREQUEST;
            swap_request.tr_time.tv_secs=(uint32_t)(at>>32);  /* E-clock hi */
            swap_request.tr_time.tv_micro=(uint32_t)at;        /* E-clock lo */
            SendIO((struct IORequest*)&swap_request);swap_busy=1u;
        }else REG32(SAGA_POINTER_WRITE)=(uint32_t)(uintptr_t)s->video[s->queued];
        s->shown=s->queued;s->queued=NO_BUFFER;
    }
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
static void saga_audio_tick(BmPlatform *p,BmPokey *const *pokeys,unsigned count,
                            BmMixer *mixer){
    SagaState *s=(SagaState*)p->userdata;
    if(!s->audio_on&&!s->audio_failed)audio_start(s);
    if(!s->audio_on)return;
    s->pokeys=pokeys;s->pokey_count=count;s->mixer=mixer;
    audio_signal(s,0);audio_write(s);
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
            if(s->vbl_start&&now>s->started){
                /* Vertical blanks per second, in hundredths. */
                uint32_t vbls=bm_vblank_count-s->vbl_start;
                /* The same 64-bit multiply-then-divide as the lines above
                   (right on the V4); the rest in 32 bits. */
                uint32_t centi=(uint32_t)((uint64_t)vbls*100u*eclock_hz/(now-s->started));
                printf("Blue Max vblank: %lu.%02lu Hz, gap %lu..%luus\n",
                    (unsigned long)(centi/100u),(unsigned long)(centi%100u),
                    (unsigned long)((uint64_t)s->vbl_gap_min*1000000u/eclock_hz),
                    (unsigned long)((uint64_t)s->vbl_gap_max*1000000u/eclock_hz));
            }
            if(s->draw_frames&&s->eclock_sum){
                uint32_t mhz=(uint32_t)(s->cpu_sum*eclock_hz/s->eclock_sum/1000000u);
                unsigned k;
                printf("Blue Max cpu: %luMHz cache-misses=%lu/frame while drawing\n",
                    (unsigned long)mhz,(unsigned long)(s->draw_misses/s->draw_frames));
                for(k=0;k<BM_PROFILE_SLOTS;++k)if(bm_profile_count[k])
                    printf("Blue Max time: %s=%luus/frame\n",
                        k==BM_PROFILE_CLASSIC?"classic":k==BM_PROFILE_CONVERT?"convert":"present",
                        (unsigned long)((uint64_t)bm_profile_total[k]*1000000u/eclock_hz/bm_profile_count[k]));
                printf("Blue Max memory: display buffers in %s RAM, game in %s RAM\n",
                    (TypeOfMem(s->video[0])&MEMF_CHIP)?"chip":"fast",
                    (TypeOfMem((void *)s)&MEMF_CHIP)?"chip":"fast");
            }
            if(s->draw_frames)
                printf("Blue Max draw: %ux%u render+present=%luus/frame max=%luus\n",
                    (unsigned)s->format.width,(unsigned)s->format.height,
                    (unsigned long)((uint64_t)s->draw_ticks*1000000u/eclock_hz/s->draw_frames),
                    (unsigned long)((uint64_t)s->draw_max*1000000u/eclock_hz));
            printf("Blue Max audio: signals=%lu resyncs=%lu max_error=%lu underruns=%lu intena=%u\n",
                (unsigned long)s->signals,(unsigned long)s->resyncs,(unsigned long)s->max_error,
                (unsigned long)s->underruns,(unsigned)s->audio_intena);
            if(s->mixer)
                printf("Blue Max samples: started=%lu peak=%lu (32767 = full scale)\n",
                    (unsigned long)s->mixer->starts,(unsigned long)s->sample_peak);
        }
#endif
    }
#if BM_SOUND_TEST
    if(s->beam_count){
        unsigned k;
        printf("Blue Max beam at swap: lines %u..%u, per 64 lines:",(unsigned)s->beam_min,
               (unsigned)s->beam_max);
        for(k=0;k<16u;++k)if(s->beam_hist[k])printf(" %u-%u:%lu",k*64u,k*64u+63u,
               (unsigned long)s->beam_hist[k]);
        printf("\n");
    }
#endif
    if(s->audio_failed)printf("Blue Max: timer.device unavailable, no sound\n");
    if(s->timer_open){CloseDevice((struct IORequest*)&eclock_request);TimerBase=0;}
    RemIntServer(INTB_VERTB,&vblank_interrupt);
    REG16(SAGA_MODULO_WRITE)=s->saved_modulo;
    swap_close_timer();
    pointer_show();             /* Workbench redraws itself behind the window */
    REG32(SAGA_POINTER_WRITE)=s->saved_pointer;REG16(SAGA_MODE_WRITE)=s->saved_mode;
    bm_sprite_set_blitter(0);
    bm_render_set_converters(0,0);
}
BmPlatform bm_platform_create(void){
    BmPlatform p;p.userdata=0;p.open=saga_open;p.poll_input=saga_poll_input;p.acquire=saga_acquire;p.present=saga_present;
    p.audio_tick=saga_audio_tick;p.wait_vblank=saga_wait_vblank;p.close=saga_close;return p;
}
