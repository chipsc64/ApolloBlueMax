#ifndef BM_GEOMETRY_H
#define BM_GEOMETRY_H

/* One projection for every Atari player/missile and playfield coordinate.

   EXXON.0/START builds the display list with two "6 blank lines" entries
   ($50,$50) before the first ANTIC mode-4 line, so the playfield starts on
   scanline 8+12 = 20.  In single-line PMG a player byte at offset Y appears
   on scanline Y, hence framebuffer y = PM_Y - 20.

   The mode-4 lines enable HSCROL, so ANTIC fetches a wide line: byte LMS+c
   appears at HPOS 32+4c+HSCROL while the normal-width window starts at
   HPOS 48.  Framebuffer x = (HPOS-48)*2, and the first fully visible byte is
   LMS+4.  The materialised view starts one byte earlier (LMS+3) so that the
   partially visible tile at HSCROL 1..3 is also present.

   This is confirmed by ADA (EXXON.14: column ((HPOS-26)>>2)-2 from LMS,
   row (Y-8)>>3), by A2 placing a GUNC gun on its $F0 tile, by the start
   airfield that keeps P0PF at PF3 during the whole TAKOF roll, and by the
   reference video (plane top on playfield line 145 at F5=165). */
enum {
  BM_PM_LEFT_HPOS = 48,
  BM_PM_TOP_SCANLINE = 20,
  BM_VIEW_LMS_OFFSET = 3
};

static inline int bm_pm_x(int hpos) { return (hpos - BM_PM_LEFT_HPOS) * 2; }
static inline int bm_pm_y(int scanline) { return scanline - BM_PM_TOP_SCANLINE; }
static inline int bm_pm_hpos(int x) { return x / 2 + BM_PM_LEFT_HPOS; }
static inline int bm_pm_scanline(int y) { return y + BM_PM_TOP_SCANLINE; }

#endif
