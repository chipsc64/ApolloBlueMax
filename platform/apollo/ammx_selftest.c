#include "bm_ammx.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t source_storage[160];
static uint8_t actual_storage[160];
static uint8_t expected_storage[160];
static BmFramebuffer sprite_actual;
static BmFramebuffer sprite_expected;

static uint8_t *align32(uint8_t *p){
  uintptr_t value=(uintptr_t)p;
  return (uint8_t*)((value+31u)&~(uintptr_t)31u);
}

static int equal_guarded(const uint8_t *left,const uint8_t *right,size_t bytes){
  size_t i;
  for(i=0;i<bytes+16u;i++)if((left-8)[i]!=(right-8)[i])return 0;
  return 1;
}

static void report_difference(const char *operation,const uint8_t *actual,
                              const uint8_t *expected,size_t bytes){
  size_t i;
  for(i=0;i<bytes+16u;i++)if((actual-8)[i]!=(expected-8)[i]){
    printf("Blue Max: AMMX %s mismatch at %ld: got %02x expected %02x\n",
           operation,(long)((ptrdiff_t)i-8),(unsigned)(actual-8)[i],
           (unsigned)(expected-8)[i]);
    return;
  }
}

int bm_ammx_selftest(void){
  static const size_t lengths[]={8u,16u,64u};
  static const uint8_t sprite_pixels[]={
    0,2,3,0,5,6,7,8,0,10,11,12,13,0,0,0,
    14,0,16,17,0,19,20,21,22,0,24,25,26,0,0,0,
    27,28,0,30,31,32,0,34,35,36,37,0,39,0,0,0,
    0,41,42,43,44,0,46,47,48,49,0,51,52,0,0,0,
    53,54,55,0,57,58,59,0,61,62,63,64,0,0,0,0
  };
  static const int positions[][2]={
    {100,80},{1,80},{319,80},{100,1},{100,199},{-20,-20},{340,220}
  };
  const BmSprite sprite={sprite_pixels,13,5,16,4,2};
  uint8_t *source=align32(source_storage+32);
  uint8_t *actual=align32(actual_storage+32);
  uint8_t *expected=align32(expected_storage+32);
  size_t test,i,length,position;
  for(test=0;test<sizeof(lengths)/sizeof(lengths[0]);test++){
    length=lengths[test];
    for(i=0;i<length+16u;i++){
      (actual-8)[i]=0xa5u;(expected-8)[i]=0xa5u;
      if(i<length)source[i]=(uint8_t)(i*37u+11u);
    }
    for(i=0;i<length;i++)expected[i]=source[i];
    bm_ammx_copy8(actual,source,(uint32_t)length);
    if(!equal_guarded(actual,expected,length)){
      report_difference("copy",actual,expected,length);return 1;
    }
  }
  length=64u;
  for(i=0;i<length+16u;i++){
    (actual-8)[i]=0x5au;(expected-8)[i]=0x5au;
  }
  for(i=0;i<length;i++){
    source[i]=(i%3u)==0u?0u:(uint8_t)(i+1u);
    if(source[i])expected[i]=source[i];
  }
  bm_ammx_sprite8(actual,source,(uint32_t)length);
  if(!equal_guarded(actual,expected,length)){
    report_difference("sprite",actual,expected,length);return 2;
  }
  for(position=0;position<sizeof(positions)/sizeof(positions[0]);position++){
    for(i=0;i<sizeof(sprite_actual.pixels);i++){
      uint8_t background=(uint8_t)(i*29u+3u);
      sprite_actual.pixels[i]=background;sprite_expected.pixels[i]=background;
    }
    bm_sprite_blit_c(&sprite_expected,&sprite,positions[position][0],positions[position][1]);
    bm_sprite_blit_ammx(&sprite_actual,&sprite,positions[position][0],positions[position][1]);
    for(i=0;i<sizeof(sprite_actual.pixels);i++)if(sprite_actual.pixels[i]!=sprite_expected.pixels[i]){
      printf("Blue Max: AMMX clipped sprite mismatch case %lu at %lu: got %02x expected %02x\n",
             (unsigned long)position,(unsigned long)i,(unsigned)sprite_actual.pixels[i],
             (unsigned)sprite_expected.pixels[i]);
      return 3;
    }
  }
  return 0;
}
