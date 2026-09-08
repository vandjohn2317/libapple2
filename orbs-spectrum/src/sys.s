;--------------------------------------------------------------------------
; sys.s -- screen, text, keyboard. The only code that knows the ULA.
;
; Conventions (sdcc 4.2, z80):
;   __z88dk_fastcall : the single argument arrives in HL, an 8-bit result
;                      leaves in L, a 16-bit one in HL.
;   Everything else the routines need comes in through the globals below,
;   which C sets before the call. Fewer conventions, fewer surprises.
;--------------------------------------------------------------------------
	.module sys

	.globl	_blit_src
	.globl	_blit_attr
	.globl	_print_ptr
	.globl	_print_attr
	.globl	_kbd
	.globl	_kbd_down
	.globl	kbd_isr
	.globl	_blit_w
	.globl	_blit_h
	.globl	_font_gfx

	.area	_DATA
_blit_src::	.ds 2		; source bitmap
_blit_attr::	.ds 1		; attribute byte to stamp
_print_ptr::	.ds 2		; zero-terminated string
_print_attr::	.ds 1
; Nine "half-rows": the keyboard's eight, then the Kempston stick as a ninth.
;   _kbd       what went DOWN since the game last asked (rising edges)
;   _kbd_down  what is down right now
; A rising edge is latched once and once only, so a key held across a turn
; does not read as a second press -- that is what auto-repeat below is for.
_kbd::		.ds 9
_kbd_down::	.ds 9
_kbd_seen::	.ds 9		; the edges the interrupt has caught, not yet taken
kbd_last::	.ds 9		; what was down on the previous frame
kbd_n::		.ds 1
_blit_w::	.ds 1		; cells across, for fill_attr and bar
_blit_h::	.ds 1		; rows for fill_attr, fill pixels for bar

	.area	_CODE

;--------------------------------------------------------------------------
; HL = (col << 8) | row  ->  HL = screen address of the char cell's top line
; row: 0..23, col: 0..31.  DESTROYS DE and A.
cell_addr:
	ld	a, l		; row
	and	#0x18
	or	#0x40
	ld	d, a
	ld	a, l
	and	#0x07
	rrca
	rrca
	rrca			; row&7 << 5
	or	h		; | col
	ld	e, a
	ex	de, hl
	ret

; HL = (col << 8) | row  ->  HL = attribute address.  DESTROYS DE and A.
attr_addr:
	ld	a, l
	rrca
	rrca
	rrca			; row * 32 -> low 3 bits of row into top
	ld	e, a
	and	#0xE0
	or	h
	ld	l, a
	ld	a, e
	and	#0x03
	or	#0x58
	ld	h, a
	ret

;--------------------------------------------------------------------------
; void sys_cls(uint16_t attr) __z88dk_fastcall   -- L = attribute
_sys_cls::
	ld	a, l
	push	af
	ld	hl, #0x4000
	ld	de, #0x4001
	ld	bc, #0x17FF
	ld	(hl), #0
	ldir
	pop	af
	ld	hl, #0x5800
	ld	de, #0x5801
	ld	bc, #0x02FF
	ld	(hl), a
	ldir
	ret

;--------------------------------------------------------------------------
; void sys_border(uint16_t colour) __z88dk_fastcall
_sys_border::
	ld	a, l
	out	(0xFE), a
	ret

;--------------------------------------------------------------------------
; void sys_halt(void)   -- wait for the next frame interrupt
_sys_halt::
	halt
	ret

;--------------------------------------------------------------------------
; void blit_tile(uint16_t colrow) __z88dk_fastcall
;   16x16 opaque tile from blit_src (32 bytes: 16 lines of left,right),
;   stamped at char (col,row) with blit_attr on its four cells.
_blit_tile::
	push	hl
	call	cell_addr
	ld	de, (_blit_src)
	call	tile_half
	ld	a, l
	add	a, #0x20	; next cell row (rows are even, never cross a third)
	ld	l, a
	call	tile_half
	pop	hl
	jp	stamp_attr4

; HL = screen address, DE = source. Copies 8 lines x 2 bytes. Leaves H as it was.
tile_half:
	ld	b, #8
1$:
	ld	a, (de)
	ld	(hl), a
	inc	de
	inc	l
	ld	a, (de)
	ld	(hl), a
	inc	de
	dec	l
	inc	h
	djnz	1$
	ld	a, h
	sub	#8
	ld	h, a
	ret

; HL = colrow. Writes blit_attr to the 2x2 cells.
stamp_attr4:
	call	attr_addr
	ld	a, (_blit_attr)
	ld	(hl), a
	inc	l
	ld	(hl), a
	ld	de, #31
	add	hl, de
	ld	(hl), a
	inc	l
	ld	(hl), a
	ret

;--------------------------------------------------------------------------
; void blit_masked(uint16_t colrow) __z88dk_fastcall
;   16x16 sprite: blit_src points at 32 bytes of bitmap followed by 32 bytes
;   of mask (1 = keep the screen). dst = (dst & mask) | bitmap.
;   Attributes of the four cells take blit_attr.
_blit_masked::
	push	hl
	call	cell_addr
	ld	de, (_blit_src)
	call	masked_half
	ld	a, l
	add	a, #0x20
	ld	l, a
	call	masked_half
	pop	hl
	jp	stamp_attr4

; HL = screen, DE = bitmap (mask is 32 bytes further on)
masked_half:
	ld	b, #8
1$:
	push	bc
	ld	bc, #32
	ld	a, (de)
	ld	c, a		; bitmap left
	ex	de, hl
	add	hl, bc		; -> mask
	ld	a, (hl)
	ex	de, hl
	and	(hl)
	or	c
	ld	(hl), a
	inc	de
	inc	l
	ld	a, (de)
	ld	c, a
	ex	de, hl
	ld	bc, #32
	add	hl, bc
	ld	a, (hl)
	ex	de, hl
	and	(hl)
	or	c
	ld	(hl), a
	inc	de
	dec	l
	inc	h
	pop	bc
	djnz	1$
	ld	a, h
	sub	#8
	ld	h, a
	ret

;--------------------------------------------------------------------------
; void blit_masked_noattr(uint16_t colrow) __z88dk_fastcall
;   Same, but leaves the attributes alone (an item drawn on a dim floor).
_blit_masked_noattr::
	call	cell_addr
	ld	de, (_blit_src)
	call	masked_half
	ld	a, l
	add	a, #0x20
	ld	l, a
	jp	masked_half

;--------------------------------------------------------------------------
; void clear_tile(uint16_t colrow) __z88dk_fastcall
;   Blank 16x16 pixels, attributes take blit_attr.
_clear_tile::
	push	hl
	call	cell_addr
	call	clear_half
	ld	a, l
	add	a, #0x20
	ld	l, a
	call	clear_half
	pop	hl
	jp	stamp_attr4

clear_half:
	ld	b, #8
1$:
	ld	(hl), #0
	inc	l
	ld	(hl), #0
	dec	l
	inc	h
	djnz	1$
	ld	a, h
	sub	#8
	ld	h, a
	ret

;--------------------------------------------------------------------------
; void set_attr4(uint16_t colrow) __z88dk_fastcall  -- recolour a tile only
_set_attr4::
	jp	stamp_attr4

;--------------------------------------------------------------------------
; void print_str(uint16_t colrow) __z88dk_fastcall
;   Prints print_ptr (ASCII 32..95, lower case folded) with print_attr.
_print_str::
	ld	de, (_print_ptr)
1$:
	ld	a, (de)
	or	a
	ret	z
	inc	de
	cp	#0x61
	jr	c, 2$
	sub	#0x20		; a-z -> A-Z
2$:
	sub	#0x20
	cp	#64
	jr	c, 3$
	xor	a		; anything outside the font prints as a space
3$:
	push	de
	push	hl
	call	print_glyph
	pop	hl
	pop	de
	inc	h
	ld	a, h
	cp	#32
	jr	c, 1$
	ret

; HL = colrow, A = glyph index 0..63.
; The screen address is worked out FIRST: cell_addr destroys DE, and DE is
; where the glyph pointer has to live for the copy.
print_glyph:
	ld	c, a		; glyph index
	push	hl		; colrow, for the attribute
	call	cell_addr	; HL = screen
	push	hl
	ld	l, c
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_font_gfx
	add	hl, de
	ex	de, hl		; DE = glyph
	pop	hl		; HL = screen
	ld	b, #8
1$:
	ld	a, (de)
	ld	(hl), a
	inc	de
	inc	h
	djnz	1$
	pop	hl		; colrow
	call	attr_addr
	ld	a, (_print_attr)
	ld	(hl), a
	ret

;--------------------------------------------------------------------------
; void fill_attr(uint16_t colrow) __z88dk_fastcall
;   blit_attr over blit_w cells by blit_h rows.
_fill_attr::
	call	attr_addr
	ld	a, (_blit_h)
	ld	b, a
1$:
	push	hl
	push	bc
	ld	a, (_blit_w)
	ld	b, a
	ld	a, (_blit_attr)
2$:
	ld	(hl), a
	inc	hl
	djnz	2$
	pop	bc
	pop	hl
	ld	de, #32
	add	hl, de
	djnz	1$
	ret

;--------------------------------------------------------------------------
; void bar(uint16_t colrow) __z88dk_fastcall
;   A 6-pixel-high bar across blit_w cells, filled blit_h pixels from the
;   left (0..width*8-2), attributes blit_attr.
_bar::
	push	hl
	call	cell_addr
	inc	h		; line 1
	ld	a, (_blit_w)
	ld	c, a		; width in cells
	ld	a, (_blit_h)
	ld	e, a		; fill pixels
	ld	b, #6
1$:
	push	bc
	push	hl
	ld	a, b
	cp	#6
	jr	z, 2$
	cp	#1
	jr	z, 2$
	; middle line: edge, fill, empty, edge
	call	bar_line
	jr	3$
2$:
	; top or bottom: solid
	ld	b, c
4$:
	ld	(hl), #0xFF
	inc	l
	djnz	4$
3$:
	pop	hl
	pop	bc
	inc	h
	djnz	1$
	pop	hl
	call	attr_addr
	ld	a, (_blit_attr)
	ld	b, c
5$:
	ld	(hl), a
	inc	l
	djnz	5$
	ret

; HL = line address, C = width cells, E = fill pixels (from pixel 1)
bar_line:
	push	de
	ld	b, c
	ld	d, #0		; pixel index of this byte's first pixel
6$:
	push	bc
	; byte value: bit7 = pixel d, ... build 8 pixels
	ld	c, #0
	ld	b, #8
7$:
	sla	c
	ld	a, d
	; pixel on if (d == 0) or (d == last) or (d >= 1 and d-1 < e)
	or	a
	jr	z, 8$
	; last pixel of the bar?
	push	hl
	ld	h, a
	ld	a, (_blit_w)
	add	a, a
	add	a, a
	add	a, a
	dec	a		; width*8-1
	cp	h
	pop	hl
	jr	z, 8$
	ld	a, d
	dec	a
	cp	e
	jr	nc, 9$
8$:
	set	0, c
9$:
	inc	d
	djnz	7$
	ld	(hl), c
	inc	l
	pop	bc
	djnz	6$
	pop	de
	ret

;--------------------------------------------------------------------------
; kbd_isr -- scan the keyboard and the stick, and latch what went DOWN.
; Called fifty times a second from the frame interrupt.
;
; THE GAME DOES NOT LOOK AT THE KEYBOARD WHILE IT IS DRAWING, and drawing a
; turn takes the best part of half a second. A key pressed and let go inside
; that window used to be a key the game never saw: the press vanished and the
; NEXT one moved the hero, which is what "sometimes the second press
; registers, not the first" is. The interrupt catches the edge instead and
; holds it until the turn loop is ready to ask.
kbd_isr::
	ld	a, #8
	ld	(kbd_n), a
	ld	ix, #kbd_last
	ld	hl, #_kbd_seen
	ld	de, #_kbd_down
	ld	bc, #0xFEFE
1$:
	in	a, (c)
	cpl
	and	#0x1F
	ld	(de), a		; what is down now
	push	bc
	ld	b, a
	ld	a, 0 (ix)	; what was down last frame
	cpl
	and	b		; ...so this is what just went down
	or	(hl)
	ld	(hl), a
	ld	0 (ix), b
	pop	bc
	inc	hl
	inc	de
	inc	ix
	rlc	b
	ld	a, (kbd_n)
	dec	a
	ld	(kbd_n), a
	jr	nz, 1$
	; the Kempston stick, as a ninth half-row. A machine without one answers
	; 0xFF on the port, which is every direction at once: read that as nothing.
	in	a, (0x1F)
	cp	#0xFF
	jr	nz, 2$
	xor	a
2$:
	and	#0x1F
	ld	(de), a
	ld	b, a
	ld	a, 0 (ix)
	cpl
	and	b
	or	(hl)
	ld	(hl), a
	ld	0 (ix), b
	ret

;--------------------------------------------------------------------------
; void kbd_take(void) -- move the latched edges into kbd[] and empty the
; latch, with the interrupt held off so a press cannot land between the read
; and the clear.
_kbd_take::
	di
	ld	hl, #_kbd_seen
	ld	de, #_kbd
	ld	b, #9
1$:
	ld	a, (hl)
	ld	(de), a
	ld	(hl), #0
	inc	hl
	inc	de
	djnz	1$
	ei
	ret
