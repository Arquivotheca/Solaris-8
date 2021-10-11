;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)int.s	1.3	99/01/31 SMI\n"

	.MODEL SMALL, C, NEARSTACK
	.386
	.CODE

EXTERN tmpAX : WORD
;[]------------------------------------------------------------[]
;|	get_time - gets the current time from DOS in number	|
;|	of ticks. According to our DOS book and other code	|
;|	there are 18.2 ticks per second!			|
;[]------------------------------------------------------------[]
PUBLIC	get_time
get_time:
	push	cx
	mov	ax, 00h
	int	1ah
	mov	ax, dx
	mov	dx, cx
	pop	cx
	ret

;[]------------------------------------------------------------[]
;|	clear_screen - clears the screen and sets the 		|
;|	foreground and background colors			|
;[]------------------------------------------------------------[]
PUBLIC	cons_clear_screen
cons_clear_screen:
	push	es
	pusha
	mov	bp, sp
	add	bp, 14h

	mov	ax, 40h
	mov	es, ax		; BIOS data area
	mov	di, 49h		; address of video mode
	mov	ah, 0h		; video mode switch function number
	mov	al, es:[di]	;
	int	10h

	mov	di, 62h		; address of video page
	mov	ax, 0920h
	mov	bh, es:[di]
	mov	bl, 7
	mov	cx, 800h
	int	10h

	popa
	pop	es
	ret

;[]------------------------------------------------------------[]
;|	ask_page - returns the current page number for use	|
;|		in set_cursor calls				|
;[]------------------------------------------------------------[]
PUBLIC	ask_page
ask_page:
	push	es
	pusha
	mov	ax, 40h		; address of the BIOS data area
	mov	es, ax
	mov	di, 62h		; address of video page
	mov	ah, 0h		; video mode switch funtion number
	mov	al, es:[di]
	mov	tmpAX, ax
	popa
	pop	es
	mov	ax, tmpAX
	ret

;[]------------------------------------------------------------[]
;|	bcopy_seg(char *src, char *dst, int size, int segment)	|
;|	strap.c needs to copy the secondary boot program into	|
;|	a diffent 64Kbyte segment than which it resides and	|
;|	therefore needs a bcopy routine which uses the ES seg	|
;|	register.						|
;|	This is a KISS routine.					|
;|[]------------------------------------------------------------[]
PUBLIC bcopy_seg
bcopy_seg:
	pusha
	push	es

	mov	bp, sp
	add	bp, 14h

	; get the arguments
	mov	si, [bp]		; source
	mov	di, 2[bp]		; dest
	mov	cx, 4[bp]		; count
	mov	es, 6[bp]		; segment register for dest

	jmp	start_bcopy
bcopy_top:
	mov	al, [si]
	mov	es:[di], al
	inc	si
	inc	di
	dec	cx
start_bcopy:
	cmp	cx, 0
	jg	bcopy_top

	pop	es
	popa
	ret

;[]------------------------------------------------------------[]
;|	_int86(intr #, in regs, out regs)			|
;|	This routine is based on the _int86 and _int86x 	|
;|	routines found in Microsofts RunTime library. The strap	|
;|	program lives in one 64Kbyte segment so it doesn't need	|
;|	any segment pointers passed. It also only supports the	|
;|	following interrupts:					|
;|	10h - character output					|
;|	13h - disk I/O						|
;|	16h - character input					|
;[]------------------------------------------------------------[]
PUBLIC _int86
_int86:
	pusha			; save all registers
	push	es		; save the segment register
	push	bp

	mov	bp, sp		; get current stack to find args
	add	bp, 16h		; 16bytes for regs, 4 bytes for es & bp
				; and 2 bytes for return pc

	mov	ax, WORD PTR [bp]
	lea	di, intcode
	mov	1[di], al

	mov	ax, ds		; segment pointer for data
	mov	es, ax		; extra seg points to data seg

	mov	di, 2[bp]	; di = union _REGS inr
	mov	ax, 0[di]
	mov	bx, 2[di]
	mov	cx, 4[di]
	mov	dx, 6[di]

intcode:
	int	13h			; bios disk subsystem

	mov	di, 4[bp]		; di = union _REGS outr
	mov	[di], ax
	mov	2[di], bx
	mov	4[di], cx
	mov	6[di], dx
	pushf
	pop	12[di]

	mov	tmpAX, ax
	pop	bp
	pop	es
	popa
	mov	ax, tmpAX
	ret

	END
