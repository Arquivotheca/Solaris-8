;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)loader.s	1.6	94/05/23 SMI\n"

;
;Assembler preamble for real-mode smc BEF module.
;
;
;	Static Name Aliases
;
	TITLE   loader

_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: _TEXT, DS: NOTHING, SS: NOTHING, ES: NOTHING
_TEXT      SEGMENT
	PUBLIC	_start
;
;       Loader code for putting in front of .EXE file to relocate it.
;       Build this file by running "masm loader", "link loader" and
;	"exe2bin loader".  Combine it with an EXE file by running
;	"copy /b loader.bin+XXX.exe XXX.com".  The resulting file
;	can be run from DOS as a .COM file or stand alone by
;	loading it into memory and jumping to the start.
;
_start  PROC    FAR
	ORG	0
	call	aftmagic
aftcall:

;	***** REPLACE NEXT TWO LINES BY DESIRED MAGIC ADDRESS AND BYTES *****
	ORG	20H
magic		db	'<Magic-Entry>'
parent		dd	0
target		dd	0
stkframe	dd	0

aftmagic:
	pop	dx
	sub	dx, offset aftcall
	add	dx, offset _end
	shr	dx, 1
	shr	dx, 1
	shr	dx, 1
	shr	dx, 1
	mov	bx, cs
	add	dx, bx
	mov	ds, dx

;	DS now addresses the EXE header.
;       Look for EXE magic word
	cmp     ds:word ptr [0], 5A4DH
	jne     badexe

;	Relocation distance is start of program, so add header size.
	add	dx, ds:word ptr [8]

;	Get the number of relocation table entries
	mov	cx, ds:word ptr [6]

;	Find the start of the relocation table
	mov	bx, ds:word ptr [18H]

nextrel:

;	Get the segment half of the next relocation table entry.
;	Relocate it to get segment half of relocation address.
	mov	ax, ds:word ptr [bx+2]
	add	ax, dx
	mov	es, ax

;	Get the offset half of the relocation address and
;	do the relocation
	mov	di, ds:word ptr [bx]
	add	es:[di], dx

;	Point at next table entry, go relocate it if not done.
	add	bx, 4
	loop	nextrel

;	Finished relocating.  Calculate entry point and jump there.
;	This is fetching the start address as specified in the .EXE
;	header
	mov	ax, ds:word ptr [16H]
	add	ax, dx
	push	ax
	push	ds:word ptr [14H]
;
	retf
badexe:
	jmp	badexe

	ORG	200H
_end:
_start	ENDP
_TEXT	ENDS
END	_start
