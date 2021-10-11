;
; Copyright (c) 1995 Sun Microsystems, Inc.  
; All rights reserved.
;

; ident	"@(#)callmdx.s	1.15	95/01/27 SMI\n"

;
; Assembler preamble for mdexec module.
;

	.MODEL TINY, C, NEARSTACK
;
; NOTE the missing stack declaration
;
	.386

	PUBLIC	MDB_SIG
	PUBLIC	MDB_SIZ
	PUBLIC  @Startup
	PUBLIC	displ_err
	PUBLIC	get_drive_geometry
	PUBLIC	register_prep

	PUBLIC	tempSP, tempSS

	PUBLIC	tmpAX, tmpDX
	PUBLIC	tmpSP, tmpSS
	PUBLIC	BootDev, fBootDev
	PUBLIC	MDXdebug

	EXTERN	mdb_exec:NEAR

	EXTERN	GeomErr:BYTE
	EXTERN	nCyl:WORD, secPerCyl:WORD, GeomErrSiz:WORD
	EXTERN	secPerTrk:BYTE, trkPerCyl:BYTE
	EXTERN	cyl:WORD, head:BYTE, sector:BYTE

	.CODE
	org	0000h

@Startup:
	jmp bootrun		;bypass size word

	org	0003h

MDB_SIG	BYTE	'MDBX'		;MDBexec signature bytes
MDB_SIZ	WORD	40		;length (number of 512-byte sectors)
debugflg WORD	0000h
MDB_VSN	BYTE	'MDBoot 2.5 '	;another ident string
	BYTE	'Copyright (c) 1995 Sun Microsystems, Inc. '
	BYTE	'All Rights Reserved.'

bootrun:
	cli
	mov	ax, cs			; initialize segment registers
IFDEF NOTTINY
	add	ax, SEG DGROUP		; must be same seg or use far refs
ENDIF
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	mov	sp, @Startup
	sti

	sub	ah, ah
	mov	al, dl
	mov	BootDev, ax		; previous stage passes boot device in dl
	mov	al, dh
	mov	fBootDev, ax

	mov	ax, cs:debugflg		; copy debug flag from convenient patch
	mov	MDXdebug, ax		;     location to data section

IFDEF NOTTINY
	mov	ax, offset begin_CONST	; verify offset for start of CONST
	mov	si, ax			;     section is really relative to
	shr	ax, 4			;     data segment
	mov	bx, SEG DGROUP
	add	ax, bx
	cmp	ax, CONST		; assume CONST section starts on
	jne	do_C_code		;     a paragraph
	mov	dx, ds
fix_CONST:
	mov	ax, WORD PTR [si]	; while (segment == base_data)
	cmp	ax, bx
	jne	do_C_code
	mov	WORD PTR [si], dx	;   replace seg value with relocated
	inc	si			;       segment value
	inc	si
	jmp	fix_CONST		; end while
ENDIF
	
do_C_code:	
	call	mdb_exec		;C entry point

hang:
	jmp	hang
    

register_prep	PROC NEAR

	xor	eax, eax	; the secondary boot fails intermittently
	xor	ebx, ebx	; when we go to protected mode.  This may
	xor	ecx, ecx	; be caused by having random garbage in
	xor	edx, edx	; registers.  Clearing registers using
	mov	esi, eax	; the UNIX assembler is painful, because
	mov	edi, eax	; we are forced to use "addr16/data16"
	mov	fs, ax		; directives, which are not documented;
	mov	gs, ax		; so, do the work here just before we
				; enter the secondary boot.
	ret
    
register_prep   ENDP


displ_err PROC NEAR

    ;di contains pointer to error message string,
    ;si contains string length
    ;since we have string length, ASCIIZ termination is not required.

	push	es
	pusha
	mov	ax, ds
	mov	es, ax

	mov	ax, 1301h	;function, cursor mode

    IFDEF MONO			;set video page, attribute
	mov	bx, 000Fh
    ELSE
	mov	bx, 004Fh
    ENDIF

	mov	cx, si		;string length
	mov	dx, 1700h	;row, column
	mov	bp, di		;string pointer
	int	10h
	
	popa
	pop	es
	ret
	
displ_err ENDP


; Caveat: This version of the master boot record uses INT 13h, Function 08h
; to determine device geometry.  This function is currently not implemented
; for extended boot devices.  (See MDB spec.)

get_drive_geometry PROC NEAR

	; dl contains BootDev
	mov	ah, 08h
	int	13h

	jc	nogeom

	; maximum usable sector number
	xor	ax, ax
	mov	al, cl
	and	al, 03Fh
	mov	secPerTrk, al

	; maximum usable cylinder number
	shr	cl, 6
	xchg	cl, ch
	mov	nCyl, cx

	; maximum usable head number
	inc	dh
	mov	trkPerCyl, dh

	; calculate sectors per cylinder
	mul	dh
	mov	secPerCyl, ax

	ret

nogeom:
	mov	di, offset GeomErr      ; "Cannot read device geometry."
	mov	si, GeomErrSiz
	call	displ_err
	jmp	$

get_drive_geometry	ENDP


tempSP	WORD	0
tempSS	WORD	0

	.DATA

MDXdebug WORD	0h		; runtime debug switch
BootDev	WORD	0
fBootDev WORD	0

tmpAX	WORD	0
tmpDX	WORD	0
tmpSP	WORD	0
tmpSS	WORD	0


	.FARDATA	FAR_DATA

	.CONST
begin_CONST	equ	$

	
	END @Startup
