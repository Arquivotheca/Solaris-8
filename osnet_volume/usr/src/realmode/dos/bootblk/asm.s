;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)asm.s	1.7	99/02/11 SMI\n"

; Assembler preamble for PCFS version of unified bootblk.
;
;	Unified bootblk program still requires separate startup
;	modules for UFS and PCFS to preserve historical behavior
;	expected by their respective bootstrap sectors.
;
;	The program gets loaded at XXXX:0000 and executed with
;	CS:IP set accordingly.  The .DOSSEG directive enables
;	the use of _edata and _end references for clearing BSS.
;	But it also seems to introduce an extra 0x10 bytes in
;	the code segment.  These bytes are stripped off by
;	exe2bin, causing a mismatch between the way the program
;	was built and the way it is invoked.  We need to change
;	the CS to XXXX - 1 to fix it up.
	.DOSSEG
	.MODEL SMALL, C, NEARSTACK

	.386

EXTERN  boot_blk:NEAR
EXTERN	malloc_util:NEAR
EXTERN	prog_stack:BYTE
EXTERN	stack_size:WORD
EXTERN	BootDbg:WORD

	.CODE			    ;code segment begins here
	org 0h

TSTACK:
	jmp	@Startup
	org	3h

MDB_SIG	BYTE	'MDBX'		; MDBexec signature bytes
MDB_SIZ	WORD	46		; length (number of 512-byte sectors)
ProgSize WORD	0000h		; size of program, exe2bin fills this in
BootFlg	WORD	0000h           ; Edit/patch here for debugging
;BootFlg	WORD	80ffh           ; Edit/patch here for debugging
MDB_VSN	BYTE	'Strap 2.0 '	; another ident string
	BYTE	'Copyright (c) 1999 by Sun Microsystems, Inc.'
	BYTE	'All Rights Reserved'

;[]------------------------------------------------------------[]
;|	Startup code - initialize the minimum set of regs	|
;|	and then jump to our main C routine.			|
;[]------------------------------------------------------------[]
@Startup:
	cli
	mov	ax, cs		; initialize segment registers
	dec	ax
	mov	di, ax
	add	di, SEG stack_size
	sub	di, SEG @Startup
	mov	ds, di
	mov	es, di
	lea	cx, DGROUP:prog_stack
	add	cx, stack_size
	mov	ss, di
	mov	sp, cx
	mov	fs, di
	mov	gs, di

;	Adjust the CS:IP to match how the program was built
	push	ax
	push	offset jmpdest
	retf
jmpdest:
	sti

;	Clear BSS to nulls
	EXTRN	_edata:BYTE
	EXTRN	_end:BYTE
	mov	di, offset DGROUP:_edata
	mov	cx, offset DGROUP:_end
	sub	cx, di
	xor	ax, ax
	cld
	rep	stosb

;	Add easily patched BootFlg into easily accessed BootDbg
	mov	ax, cs:BootFlg
	or	BootDbg, ax

	xor	dh, dh		; clear upper byte of dx register which
				; may have some garbage in it.
	mov	si, dx		; previous stage passes boot device in dl

	push	si		; boot device
	call	boot_blk	; C entry point
	add	sp, 2
	call	hang

	PUBLIC	hang
hang	PROC	NEAR
	jmp	hang
hang	ENDP

	PUBLIC	register_prep
register_prep	PROC	NEAR
	xor	eax, eax
	xor	ebx, ebx
	xor	ecx, ecx
	xor	edx, edx
	mov	esi, eax
	mov	edi, eax
	mov	fs, ax
	mov	gs, ax
        ret
register_prep	ENDP

	END	TSTACK

