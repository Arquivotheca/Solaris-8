	.MODEL TINY, C, NEARSTACK
	.386

EXTERN  main_test:NEAR
EXTERN  BootDev:WORD
EXTERN	malloc_util:NEAR
EXTERN	SmallStack:WORD
EXTERN  MyStackAddr:WORD
EXTERN	Malloc_start:WORD

	.CODE
	org 0000h

TSTACK:
	jmp	@Startup
	org	0003h
MDB_SIG	BYTE	'MDBX'		; MDBexec signature bytes
MDB_SIZ	WORD	28		; length (number of 512-byte sectors)
ProgSize WORD	0000h
MDB_VSN	BYTE	'Test 2.4 '	; another ident string
	BYTE	'Copyright (c) 1992-1994 Sun Microsystems, Inc.'
	BYTE	'All Rights Reserved'

@Startup:
	cli
	mov	ax, cs		; initialize segment registers
	mov	ss, ax
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	lea	sp, SmallStack	; enough to jump into malloc and back
	add	sp, 100
	sti
	xor	dh, dh

	mov	BootDev, dx	; previous stage passes boot device in dl
;	mov	ax, ProgSize
;	add	ax, 0003h	; round up plus program load point
;	and	ax, 0fffch	; 4 byte aligned malloc ponit
	mov	ax, 8000h
	mov	Malloc_start, ax
	push	1000h
	call	malloc_util	; create real stack
	mov	sp, ax
	add	sp, 1000h
	mov	MyStackAddr, ax
restart:
	call	main_test	; C entry point

	jmp	restart
    
	END	TSTACK

