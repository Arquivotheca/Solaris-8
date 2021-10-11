;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)pboot.s	1.3	99/06/07 SMI\n"

; Source file for CDROM pboot sector.
;
; For Solaris x86, pboot is always on the second sector of the CDROM.
; The El Torito boot catalog initial/default entry indicates a 4-sector
; bootstrap to be read from block 0, which contains mboot, pboot and
; the VTOC.  mboot verifies the presence of pboot and jumps into it
; with CS:IP set to XXXX:0000.  Normally the CS will be 7E0, but pboot
; should not assume that.
;
; The choice of a 4-sector bootstrap read is because El Torito BIOSes
; do not all implement the spec very well.  Microsoft Windows NT 4.0
; uses a 4-sector bootstrap so we generally expect that functionality
; to be well tested.
;
; pboot attempts to read bootblk starting from block 1 of the CDROM.
; The maximum possible size is 15 blocks because the ISO headers
; start at block 16.  Just read it all in one go because it is simpler,
; and possibly quicker, than doing a short read to see the sector count
; embedded in bootblk and then reading the rest.
;
; pboot also checks for a debug request from the user in the form of
; a 'd' typed at the keyboard.  If one is found, it turns on a debug
; flag in bootblk by patching the debug flag location in bootblk.
; The purpose of the flag is to aid in diagnosing CDROM boot problems
; while using the distribution CDROM.
;
; There is a useable stack on entry, but set up the segments to be
; equal to minimize offset confusion.  Use 600h as the stack top because
; that reuses the two extra sectors that were already read in but are
; not needed.
;

loadpt	equ	0
pstack	equ	600h
EXTREAD	equ	42h
BBOFF	equ	1000h
BBSEG	equ	6000h
BBSIZE	equ	15
DBG_ELTORITO equ	1000h	; From bootblk.h
BBSIG	equ	4242h	; bootblk signature ("BB")
DWAIT1	equ	55	; number of clock ticks to wait for debug request
DWAIT2	equ	36	; number of clock ticks to wait after debug request
CDTRIES	equ	3	; number of times to try CD read
CR	equ	0dh	; Carriage return character
LF	equ	0ah	; Line feed character

	.MODEL TINY, C, NEARSTACK
	.CODE
	.386

	org	loadpt
boot	proc	far
	jmp	short start

	db	"CDPB"	; signature used by mboot
	db	0		; version number for future use

start:
	mov	ax, cs
	mov	ds, ax
	mov	ss, ax
	mov	sp, pstack
	push	dx

;	Read 0x3C sectors of bootblk
retry:
	mov	ax, BBSEG
	mov	es, ax
	mov	di, BBOFF
	mov	ax, 1	; bootblk starts at block 1
	mov	cl, BBSIZE
	call	readCD
	cmp	ax, 0
	jz	readDone
	dec	tries
	jg	retry

badRead:
	push	ax
	lea 	si, brmsg
	call	outstr
	pop	ax
	call	out2hex
	lea 	si, crlf
msghng:
	call	outstr
hang:
	jmp	hang

readDone:
;	Check for the bootblk signature.
	lea	si, nobblk
	cmp	word ptr es:4[di], BBSIG
	jne	msghng

; Wait for DWAIT1 clock ticks, looking for 'd' from keyboard
kb_check:
	call	one_tick
	call	peek_kb
	cmp	al, 'd'
	je	dfound
	dec	ticks
	jnz	kb_check
	jmp	go

;	Turn on debug flag in bootblk.  Acknowledge the debug request.
;	Wait for DWAIT2 clock ticks.  Discard any extra 'd's.
dfound:
	or	word ptr es:10[di], DBG_ELTORITO
	lea	si, dbgmsg
	call	outstr
	mov	ticks, DWAIT2
delay:
	call	one_tick
	dec	ticks
	jnz	delay
slurp:
	call	peek_kb
	cmp	al, 'd'
	jne	go
	call	read_kb
	jmp	slurp

; Execute from the start of bootblk.
go:
	pop	dx
	push	BBSEG
	push	BBOFF
	ret
boot	endp

; one_tick() - loop until the clock ticks.  Preserves DX. Destroys AX, CX.
one_tick	proc	near
	push	dx
	call	bios_time
	push	cx
	push	dx
	mov	bp, sp
more:
	call	bios_time
	cmp	cx, 2[bp]
	jne	done
	cmp	dx, [bp]
	je	more
done:
	add	sp, 4
	pop	dx
	ret
one_tick	endp

; bios_time() - returns current time in CX, DX.  Also destroys AX.
; Preserves BX.
bios_time	proc	near
	mov	ah, 0
	int	1ah
	ret
bios_time	endp

; read_kb() - return the next kb char in AL.  Wait if no char ready.
; All other registers preserved.
read_kb	proc	near
	mov	ah, 0
	int	16h
	ret
read_kb	endp

; peek_kb() - return any pending kb char in AL.  FF means no char.
; All other registers preserved.
peek_kb	proc	near
	mov	ah, 1
	int	16h
	jnz	found
	mov	al, 0ffh
found:
	ret
peek_kb	endp

; readCD() - read CL blocks from device DL, starting at block AX into ES:DI.
; Return AX = 0 for success.  Destroys SI, AX, CX.  Preserves BX, DX.
readCD proc near
	sub	sp, 16
	mov si, sp
	xor	ch, ch
	mov	word ptr 0[si], 10h
	mov	2[si], cx	; number of blocks to read
	mov	4[si], di
	mov	6[si], es
	mov	8[si], ax	; block number (lo)
	mov	word ptr 10[si], 0
	mov	word ptr 12[si], 0
	mov	word ptr 14[si], 0
	mov	ah, EXTREAD
	int	13h
	mov	al, 0
	jnc	read_ok
	inc	al
read_ok:
	xchg	al, ah
	add	sp, 16
	ret
readCD endp

; out2hex() - displays hex digits corresponding to AL.
; Destroys AX, BX.  Preserves CX, DX.
out2hex proc    near
        push    ax
        shr     ax, 4
        call    out1hex
        pop     ax
        call    out1hex
        ret
out2hex endp

; out1hex() - displays hex digit corresponding to low nibble of AL.
; Destroys AX, BX.  Preserves CX, DX.
out1hex proc    near
        mov     bx, ax
        and     bx, 0Fh
        mov     al, hexdig[bx]
        call    outchar
        ret
out1hex endp

; outstr() - displays null-terminated string in SI.  Destroys AX, BX.
; Preserves CX, DX.
outstr	proc	near
	lodsb
	or	al, al
	jz	endstr
	call	outchar
	jmp	outstr
endstr:
	ret
outstr	endp

; outchar() - displays char in AL.  Destroys AX, BX.  Preserves CX, DX.
outchar	proc	near
	mov	ah, 0eh
	xor	bx, bx
	int	10h
	ret
outchar	endp

tries	db	CDTRIES
ticks	db	DWAIT1

hexdig  db      "0123456789ABCDEF"
brmsg	db	"pboot: CDROM read error ", 0
nobblk	db	"pboot: cannot find bootblk.", CR, LF, 0
dbgmsg	db	CR, LF, "Solaris CD-ROM boot debug requested.", CR, LF, 0
crlf	db	CR, LF, 0
	org	loadpt + 512
	end     boot
