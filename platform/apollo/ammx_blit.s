        xdef    _bm_ammx_copy8
        xdef    _bm_ammx_sprite8

; Internal renderer ABI (not a C compiler ABI):
;   a0 destination, a1 source, d0 byte count; count must be a multiple of 8.
; Wrappers must preserve registers according to the selected C toolchain.

_bm_ammx_copy8:
		store	e0,-(sp)
        tst.l   d0
        beq.s   .copy_done
.copy_loop:
        load    (a1)+,e0
        store   e0,(a0)+
        subq.l  #8,d0
        bne.s   .copy_loop
.copy_done:
		load	(sp)+,e0
        rts

_bm_ammx_sprite8:
        move.l  d2,-(sp)
        moveq   #1,d1
        tst.l   d0
        beq.s   .sprite_done
.sprite_loop:
        load    (a1)+,d2
; AMMX manual: STOREM3 byte mode writes only source bytes that are non-zero.
; vasm 2.0f rejects the documented #1 operand. Its workaround encodes the
; register number as the mode (d1 = mode 1); setting d1 to 1 also covers cores
; which inspect the runtime value.
        storem3 d2,d1,(a0)+
        subq.l  #8,d0
        bne.s   .sprite_loop
.sprite_done:
        move.l  (sp)+,d2
        rts

; 68080 event counters (MOVEC, see Docs/Integer/MOVEC.md): $809 CCC = CPU
; clock counter, $80F DCM = data cache misses.  Encoded as words because
; the assembler does not know these control registers.
        xdef    _bm_cpu_cycles
        xdef    _bm_cpu_cache_misses
_bm_cpu_cycles:
        dc.w    $4e7a,$0809             ; movec CCC,d0
        rts
_bm_cpu_cache_misses:
        dc.w    $4e7a,$080f             ; movec DCM,d0
        rts

; ---------------------------------------------------------------------------
; Row conversion for the 640x400 renderers (see BmRowJob in bm_renderer.h):
;   a0 = job: 0 in, 4 top, 8 bottom, 12 pairs, 16 line_top, 20 line_bottom,
;             24 phase, 28 count.  Pixel pairs are one longword per Atari pixel.
; Both save d2-d7/a2-a6 (C convention); AMMX upper halves are scratch.
        xdef    _bm_ammx_scale_row
        xdef    _bm_ammx_hd_row

; Two pixels per step: two table look-ups, VPERM joins the two longwords
; into one 64-bit value, STORE writes it to both output lines.
_bm_ammx_scale_row:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  a0,a6
        move.l  (a6),a1
        move.l  4(a6),a0
        move.l  8(a6),a3
        move.l  12(a6),a2
        move.l  28(a6),d5
        lsr.l   #1,d5
        beq.s   .scale_done
.scale_loop:
        moveq   #0,d2
        moveq   #0,d3
        move.b  (a1)+,d2
        move.b  (a1)+,d3
        move.l  (a2,d2.l*4),d6
        move.l  (a2,d3.l*4),d7
        vperm   #$4567CDEF,d6,d7,d4
        store   d4,(a0)+
        store   d4,(a3)+
        subq.l  #1,d5
        bne.s   .scale_loop
.scale_done:
        movem.l (sp)+,d2-d7/a2-a6
        rts

; As bm_row_hd_c: at a tile boundary (phase 0) eight equal indices are a
; whole tile: a textured one copies its two texture rows with MOVE16
; (32 bytes each, no alignment needed on the 68080), a flat one fills both
; lines with VPERM + STORE.  Everything else goes pixel by pixel.
_bm_ammx_hd_row:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  a0,a6
        move.l  (a6),a1
        move.l  8(a6),a3
        move.l  12(a6),a2
        move.l  16(a6),a4
        move.l  20(a6),a5
        move.l  24(a6),d4
        move.l  28(a6),d5
        move.l  4(a6),a0
.hd_loop:
        tst.l   d5
        beq     .hd_done
        tst.l   d4
        bne.s   .hd_pixel
        cmp.l   #8,d5
        blo.s   .hd_pixel
        moveq   #0,d2
        move.b  (a1),d2
        move.l  d2,d3
        mulu.l  #$01010101,d3
        move.l  (a1),d0
        cmp.l   d3,d0
        bne.s   .hd_pixel
        move.l  4(a1),d0
        cmp.l   d3,d0
        bne.s   .hd_pixel
        addq.l  #8,a1
        subq.l  #8,d5
        move.l  (a4,d2.l*4),d6
        beq.s   .hd_flat8
        move.l  d6,a6
        move16  (a6)+,(a0)+
        move16  (a6)+,(a0)+
        move.l  (a5,d2.l*4),a6
        move16  (a6)+,(a3)+
        move16  (a6)+,(a3)+
        bra.s   .hd_loop
.hd_flat8:
        move.l  (a2,d2.l*4),d6
        vperm   #$45674567,d6,d6,d7
        store   d7,(a0)+
        store   d7,(a0)+
        store   d7,(a0)+
        store   d7,(a0)+
        store   d7,(a3)+
        store   d7,(a3)+
        store   d7,(a3)+
        store   d7,(a3)+
        bra.s   .hd_loop
.hd_pixel:
        moveq   #0,d2
        move.b  (a1)+,d2
        move.l  (a4,d2.l*4),d6
        beq.s   .hd_flat1
        move.l  d6,a6
        move.l  (a6,d4.l*4),(a0)+
        move.l  (a5,d2.l*4),a6
        move.l  (a6,d4.l*4),(a3)+
        bra.s   .hd_next
.hd_flat1:
        move.l  (a2,d2.l*4),d6
        move.l  d6,(a0)+
        move.l  d6,(a3)+
.hd_next:
        addq.l  #1,d4
        and.l   #7,d4
        subq.l  #1,d5
        bra     .hd_loop
.hd_done:
        movem.l (sp)+,d2-d7/a2-a6
        rts
