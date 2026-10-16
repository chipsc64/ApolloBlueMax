#ifndef BM_PROFILE_H
#define BM_PROFILE_H
#include <stdint.h>
/* Optional profiling with a CPU cycle counter (68080 CCC); the platform
   sets bm_profile_cycles in its diagnostic builds, otherwise it is 0 and
   nothing is measured. */
enum { BM_PROFILE_CLASSIC, BM_PROFILE_CONVERT, BM_PROFILE_SLOTS };
extern uint32_t (*bm_profile_cycles)(void);
extern uint32_t bm_profile_total[BM_PROFILE_SLOTS],bm_profile_count[BM_PROFILE_SLOTS];
static inline uint32_t bm_profile_start(void){
  return bm_profile_cycles?bm_profile_cycles():0u;
}
static inline void bm_profile_stop(unsigned slot,uint32_t start){
  if(!bm_profile_cycles)return;
  bm_profile_total[slot]+=bm_profile_cycles()-start;++bm_profile_count[slot];
}
#endif
