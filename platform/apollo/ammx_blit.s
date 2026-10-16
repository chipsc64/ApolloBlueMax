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
