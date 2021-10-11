;
;	Copyright (c) 1999 Sun Microsystems, Inc.
;	All Rights Reserved.
;
;	asm_util.asm
;	- pci_getl2, pci_putl2, pci_present2 : provide access to pci
;		configuration space using pci bios int 1ah, func b1
;		for double word access only
;	- acpi_copy : copy boards from boot.bin to bootconf
;	- setjmp, longjmp, calloff, callseg
;				
;
_TEXT   SEGMENT
	db "<@(#)asm_util.s	1.6	99/05/23 SMI>"
_TEXT	ENDS

	TITLE   asm_util

	.386

_TEXT	SEGMENT  WORD USE16 PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD USE16 PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT  WORD USE16 PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD USE16 PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST, _BSS, _DATA
	ASSUME DS: DGROUP, SS: DGROUP
_TEXT	SEGMENT
	ASSUME	CS: _TEXT

PCI_FUNCTION_ID		equ 0b1h
PCI_BIOS_PRESENT	equ 01h
READ_CONFIG_DWORD	equ 0ah
WRITE_CONFIG_DWORD	equ 0dh
SUCCESSFUL		equ 0

;
; pci_getl2 -- read dword from pci configuration space
;
; int retval = pci_getl2(u_char bus, u_char devfunc, u_int off, u_long far *val)
;
PUBLIC  _pci_getl2
_pci_getl2   PROC    FAR
	push	bp
	mov	bp, sp
	push	bx
	push	di
	push	es
	push	ecx

	mov	ah,PCI_FUNCTION_ID
	mov	al,READ_CONFIG_DWORD
	mov	bh,6[bp]	; bus
	mov	bl,8[bp]	; devfunc
	mov	di,10[bp]	; off

	int	1ah
	jc	pci_getl2_bad
	cmp	ah,SUCCESSFUL
	jne	pci_getl2_bad

	mov 	di,12[bp]	; val offset
	mov	es,14[bp]	; val segment
	mov	es:[di], ecx
	xor	ax,ax
	jmp	pci_getl2_cmn
pci_getl2_bad:
	mov	ax,1
pci_getl2_cmn:
	pop	ecx
	pop	es
	pop	di
	pop	bx
	pop	bp
	retf
_pci_getl2   ENDP

;
; pci_putl2 -- write dword to pci configuration space
;
; int pci_putl2(u_char bus, u_char devfunc, u_int off, u_long val)
;
PUBLIC  _pci_putl2
_pci_putl2   PROC    FAR
	push	bp
	mov	bp, sp
	push	bx
	push	di
	push	es
	push	ecx

	mov	ah,PCI_FUNCTION_ID
	mov	al,WRITE_CONFIG_DWORD
	mov	bh,6[bp]	; bus
	mov	bl,8[bp]	; devfunc
	mov	di,10[bp]	; off
	mov	ecx,12[bp]	; val

	int	1ah
	jc	pci_putl2_bad
	cmp	ah,SUCCESSFUL
	jne	pci_putl2_bad
	xor	ax,ax
	jmp	pci_putl2_good
pci_putl2_bad:
	mov	ax,1
pci_putl2_good:

	pop	ecx
	pop	es
	pop	di
	pop	bx
	pop	bp
	retf
_pci_putl2   ENDP

;
; pci_present2 -- check if any pci busses are present
;
; int pci_present2(u_char far *mechanism, u_char far *nbus, u_short *vers)
;
PUBLIC  _pci_present2
_pci_present2   PROC    FAR
	push	bp
	mov	bp, sp
	push	bx
	push	cx
	push	di
	push	es
	push	edx

	mov	ah, PCI_FUNCTION_ID
	mov	al, PCI_BIOS_PRESENT
	int	1ah
	jc	nopci
	cmp	ah, 0
	jne	nopci
	cmp	dl, 'P'
	jne	nopci
	cmp	dh, 'C'
	jne	nopci
	shr	edx, 16
	cmp	dl, 'I'
	jne	nopci
	cmp	dh, ' '
	jne	nopci

	mov 	di,6[bp]	; mechanism offset
	mov	es,8[bp]	; mechanism segment
	mov	es:[di], al
	mov 	di,10[bp]	; nbus offset
	mov	es,12[bp]	; nbus segment
	mov	es:[di], cl
	mov 	di,14[bp]	; vers offset
	mov	es,16[bp]	; vers segment
	mov	es:[di], bx
	mov	ax, 1
	jmp	fin

nopci:	mov	ax, 0
fin:
	pop	edx
	pop	es
	pop	di
	pop	cx
	pop	bx
	pop	bp
	retf
_pci_present2   ENDP

;  Custom version of setjmp/longjmp
;
;  We need to be sure that setjmp/longjmp can handle stack switches.  The
;  Microsoft version may, indeed, do so but I never bother to check it
;  because ANSI does not require that setjmp/longjmp deal with stack switches.
;  Even if the current MS version works, there's no guarantee that new
;  versions will continue to work the same way.  Hence, I've added this
;  version (along with "setjmp.h").

PUBLIC  _setjmp
_setjmp PROC    FAR
	push	bp
	mov	bp, sp		; Build a stack frame and save a couple
	push	es		; .. of registers
	push	di
	mov	di, [bp+6]	; Jmp_buf address to %es:%di
	mov	es, [bp+8]
	mov	ax, [bp+2]	; Save return addr in 1st word of jmp_buf
	mov	es:[di], ax
	mov	ax, [bp+4]
	mov	es:[di+2], ax
	mov	ax, [bp-4]	; Store non-volatile registers in jmp_buf.
	mov	es:[di+4], ax	; .. This includes value of %di and %bp
	mov	es:[di+6], si	; .. we pushed onto stack at entry.
	mov	ax, [bp]
	mov	es:[di+8], ax
	mov	es:[di+10], bp
	mov	es:[di+12], bx
	mov	es:[di+14], ds	; Store segment regs (except %cs) in jmp_buf.
	mov	es:[di+16], ss	; .. This includes the initial value of %es
	mov	ax, [bp-2]	; .. we pushed onto the stack at entry.
	mov	es:[di+18], ax
	xor	ax, ax		; Clear return code, restor regs, & exit
	pop	di
	pop	es
	pop	bp
	retf
_setjmp	ENDP

PUBLIC  _longjmp
_longjmp PROC    FAR
	push	bp
	mov	bp, sp
	mov	di, [bp+6]	; Find jmp_buf and put its address in %es:%di
	mov	es, [bp+8]
	mov	cx, [bp+10]	; Return value in %cx for the time being
	mov	bx, es:[di+12]	; Restore "safe" registers
	mov	si, es:[di+6]
	mov	ds, es:[di+14]
	mov	bp, es:[di+10]
	mov	ss, es:[di+16]	; Switch back to original stack segment.
	mov	sp, bp		; .. with %sp == %bp for the time being to
	mov	ax, es:[di+2]	; .. protect against interrupts
	mov	[bp+4], ax
	mov	ax, es:[di]	; Replace our return address with setjmp's
	mov	[bp+2], ax	; .. return addr!
	mov	bp, es:[di+8]
	mov	dx, es:[di+4]	; Restore %es:%di from values in jmp_buf
	mov	es, es:[di+18]
	mov	di, dx
	mov	ax, cx		; %ax now contains return value
	add	sp, 2		; Skip over %bp in stack frame
	retf
_longjmp ENDP

PUBLIC  _calloff
_calloff PROC    FAR
	mov	ax, 2[bp]
	retf
_calloff ENDP

PUBLIC  _callseg
_callseg PROC    FAR
	mov	ax, 4[bp]
	retf
_callseg ENDP

; parameters for oem_call to do acpi_copy
OEM_CALL		equ	254
ACPI_COPY		equ	253

PUBLIC  _acpi_copy
_acpi_copy PROC    FAR
	push	bp		;setup stack frame
	mov	bp, sp

	push	bx		;save all modified registers (paranoia)
	push	dx
	push	si
	push	di
	push	ds

	mov	ah, OEM_CALL	;oem int 21 vector 0xfe 

	mov	bx, ACPI_COPY	;set bx to guard against native bios calls

	mov	di, [bp+6]	;save the offset of the control struct
	mov	dx, di
	mov	di, [bp+8]	;save the ds
	mov	ds, di

	mov	al, 0		;clear return value
	int	21h

	pop	ds		;restore saved registers
	pop	di
	pop	si
	pop	dx
	pop	bx

	leave			;teardown stack frame
	retf

_acpi_copy ENDP

_TEXT	ENDS

END
