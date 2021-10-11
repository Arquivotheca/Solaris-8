;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)crt0.s	1.3	97/03/10 SMI\n"
; 
;  C Startup for real mode drivers:
;
;  This file contains the C startup routine for Solaris x86 real mode drivers
;  (".bef" files).  Entry is at the label BEF_entry.  The function to be
;  performed is indicated by the contents of register AX:
;
;                   BEF_LEGACYPROBE (0x1)  ..  To probe for devices
;                   BEF_INSTALLONLY (0x2)  ..  To install driver as bios ext.
;
;  The ES:DI registers address a callback vector containing read-only data.
;  The startup routine saves the callback vector (see "callback.c"), loads
;  the data segment registers, and calls the main driver routine:
;
;      int dispatch (int func)
;
;  where "func" is the driver function code (from AX) noted above.  The
;  "dispatch" routine performs the indicated function and delivers an integer
;  return value which is passed back to the caller.  The return address is a
;  far address on the top of the stack at the time of entry.
;
;  Different stacks are used for the caller and bef, so we have to
;  copy arguments etc between them.

	title   crt0
	.DOSSEG

	_text	segment  word public 'code'   ; Various incantations required
	_text	ends                          ; .. by the assembler
	const	segment  word public 'const'
	const	ends
	_data	segment  word public 'data'

extrn	_putc_:near
	
	public	_befunc
	public	_befvec
	public  _befptr
	public	_putvec
	public	sps
	public	spo

_putvec	dw	(0)
_befunc	dw	(0)
_befvec dw	2 dup (0)
_befptr dw	2 dup (0)
	_data	ends

	_bss	segment  word public 'bss'
	public	sustack
	public  esustk
sustack	db	4000 dup (?)
esustk	label	word
	_bss ends

DGROUP	group   const, _bss, _data
	assume  cs:_text, ds:_data, es:nothing

_text	segment
	extrn	_dispatch:near

	public	_crt0              ; >>> Load Point <<<
_crt0	proc near
;	Older-style BEFs were entered here.  Return an error if attempted.
	mov	ax, 0FFFFh
	retf


	org	_crt0+0Ch          ; BEF_MAGIC word to identify this as a
;	DevConf-style BEFs are entered following this magic word.
magic:	dword 	2E626566h          ; .. 2.6 driver

BEF_entry:	; Label is for documentation only.  It is not really used.

	; ---- setup the other segments based on cs ----
	push	ds
	mov     bx, cs
	add     bx, seg _befptr
	sub     bx, seg _crt0
	mov     ds, bx
	pop	_befptr+2	   ; Save "backdoor" interface ptr
	mov     _befptr, dx
	mov     _befunc, ax	   ; Old entry, save func code for error
	mov	_befvec, di	   ; New entry, save vector address for 
	mov	_befvec+2, es      ; .. callback processing.
	mov     es, bx

	mov	cs:spo, sp	   ; Save stack pointer offset 
	mov	cs:sps, ss	   ; ... and segment for _exit (see below)
	mov     ss, bx		   ; Load up new stack
	lea     sp, esustk
	or	ax, ax
	je	short go
	lea	cx, cs:_putc_
 	mov	_putvec, cx

go:	
;	Clear BSS (from _edata to _end)
	EXTRN	__edata:BYTE
	EXTRN 	__end:BYTE
	mov	di, offset DGROUP:__edata
	mov	cx, offset DGROUP:__end
	sub	cx, di
	xor	ax, ax
	cld
	rep	stosb		; clear it all to 0

	push	_befunc
	call	_dispatch	   ; Call the dispatch routine
	add	sp, 2

	push	ax		   ; Return to server
	call	_exit
_crt0   endp

; /*
;  *  void exit (int code)
;  *
;  *    This routine simulates the UNIX "exit" system call by returning its
;  *    exit "code" argument to the server that loaded the current driver.
;  *    It does this by resetting the stack pointer to its value at the time
;  *    of entry and then issuing a "retf".
;  */

	public	_exit
_exit	proc	near

	pop	ax		   ; Remove return address
	pop	ax		   ; Load exit code
	mov	ss, cs:sps	   ; Restore stack pointer segment
	mov	sp, cs:spo	   ; Restore stack pointer offset
	retf			   ; Return to server

;	The following data items are in the code segment.
sps	dw	(0)		   ; Stack pointer segment save area
spo	dw	(0)		   ; Stack pointer offset save area
	db	"@(#)crt0.s	1.3	97/03/10 SMI\n" ; SID

_exit	endp
_text	ends
	end
