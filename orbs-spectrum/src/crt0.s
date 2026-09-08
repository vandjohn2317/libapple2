;--------------------------------------------------------------------------
; crt0.s -- Orbs of the Overlord, ZX Spectrum.
;
; Entry at 0x8000 (the first thing in _CODE; this file links first).
; The BASIC loader does CLEAR 32767: LOAD "" CODE: RANDOMIZE USR 32768.
;
; Memory map:
;   0x4000-0x5AFF  screen
;   0x5B00-0x7FFF  variables (_DATA, _INITIALIZED)   -- contended, data only
;   0x8000-0xF9FF  code, graphics, text (_CODE, _INITIALIZER)
;   0xFA00-0xFCFF  stack
;   0xFDFD         IM2 handler stub (jp _isr)
;   0xFE00-0xFF00  IM2 vector table, 257 bytes of 0xFD
;--------------------------------------------------------------------------
	.module crt0
	.globl	_main
	.globl	_isr_frames
	.globl	kbd_isr
	.globl	l__DATA
	.globl	s__DATA
	.globl	l__INITIALIZER
	.globl	s__INITIALIZED
	.globl	s__INITIALIZER

	.area	_HOME
	.area	_CODE
init::
	di
	ld	sp, #0xFD00
	; The IM2 table: every entry points at 0xFDFD, whatever the bus says.
	ld	hl, #0xFE00
	ld	de, #0xFE01
	ld	bc, #256
	ld	(hl), #0xFD
	ldir
	ld	a, #0xC3		; jp
	ld	(0xFDFD), a
	ld	hl, #isr
	ld	(0xFDFE), hl
	ld	a, #0xFE
	ld	i, a
	im	2
	call	gsinit
	ei
	call	_main
1$:
	halt
	jr	1$

; The frame interrupt: count frames and read the keyboard. Everything else
; the game does happens in the main loop, paced off this counter -- but the
; keys have to be caught here, because the main loop spends most of a turn
; drawing and would miss a press that came and went while it did.
isr:
	push	af
	push	bc
	push	de
	push	hl
	push	ix		; the compiler keeps its frame pointer here
	ld	hl, #_isr_frames
	inc	(hl)
	call	kbd_isr
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	ei
	reti

	;; Ordering of segments for the linker.
	.area	_HOME
	.area	_CODE
	.area	_INITIALIZER
	.area   _GSINIT
	.area   _GSFINAL

	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area   _BSS
	.area   _HEAP

	.area	_DATA
_isr_frames::
	.ds	1

	.area   _GSINIT
gsinit::
	; Default-initialized global variables.
	ld	bc, #l__DATA
	ld	a, b
	or	a, c
	jr	Z, zeroed_data
	ld	hl, #s__DATA
	ld	(hl), #0x00
	dec	bc
	ld	a, b
	or	a, c
	jr	Z, zeroed_data
	ld	e, l
	ld	d, h
	inc	de
	ldir
zeroed_data:
	; Explicitly initialized global variables.
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	Z, gsinit_next
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir
gsinit_next:

	.area   _GSFINAL
	ret
