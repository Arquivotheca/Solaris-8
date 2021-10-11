;
; Copyright (c) 1999 Sun Microsystems, Inc.
; All rights reserved.
;
; ident	"@(#)pcopy.s 1.1	99/08/18 SMI"
;
; Solaris Primary Boot Subsystem - BIOS Extension Driver Support Routines
;===========================================================================
;
; Current functionality:
; pcopy() - Provides method for drivers using memory mapped cards to copy
; data to/from addresses outside the 1MB realmode limit.
;
; Interrupt vector for PCI BIOS calls
OEM_PCOPY		equ	254

; BIOS call return codes
SUCCESSFUL		equ	0

; Data size override instruction
DATA16	EQU	66H

	.MODEL SMALL, C, NEARSTACK
	.386

	.CODE			;code segment begins here

;
;   Function name:	pcopy
;
;   C calling syntax:	void pcopy(unsigned long src, unsigned long dest,
;                    		unsigned short nbytes, unsigned short flags);
;
;   Description:	reads or writes from src to dest
;
;   Note that the interface for calling int 21h is by passing a pointer
;   to the following structure.
;
;   struct rpcopy {
;	unsigned long src;	/* 32-bit linear src addr */
;	unsigned long dest;	/* 32-bit linear dest addr */
;	unsigned short nbytes;	/* number of bytes */
;	unsigned short flags;	/* width of copy operation */
;   } rp;
;   so this struct would be created on the stack and filled from
;   arguments passed in. A pointer to the structure will be passed for
;   int 21h function call to boot.bin.
;
;
;
PUBLIC  pcopy

pcopy:

	push	bp		;setup stack frame
	mov	bp, sp


	push	bx		;save all modified registers (paranoia)
	push	dx
	push	si
	push	di
	push	ds

	sub sp, 12		; leave space for rpcopy struct

	lea dx, [bp-22]		; store the address of rpcopy in dx

	mov di, dx

	mov ax, [bp+4]		; rp.src = src
	mov [di], ax		;
	mov ax, [bp+6]		;
	mov [di+2], ax		;


	mov ax, [bp+8]		; rp.dest = dest
	mov [di+4], ax		;
	mov ax, [bp+10]		;
	mov [di+6], ax		;


	mov ax, [bp+12]		;rp.nbytes = nbytes 
	mov [di+8], ax		;


	mov ax, [bp+14]		; rp.nflags = nflags
	mov [di+10], ax		;



	mov	ah, OEM_PCOPY	;oem int 21 vector 0xfe (0xff already in use)
	mov	bx, OEM_PCOPY	;set bx to guard against native bios calls

	push ds
	push ss			;
	pop ds			; ds should have ss value since rp is in
				; stack

	mov	al, 0		;clear return value
	int	21h

	pop ds
	add sp, 12		; release the space for rp in stack


	pop	ds		;restore saved registers
	pop	di
	pop	si
	pop	dx
	pop	bx

	leave			;teardown stack frame

	ret

	END
