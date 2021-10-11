;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)mboot.s	1.2	99/04/14 SMI\n"

; Source file for CDROM equivalent of hard disk mboot.

; Normal entry is from EL Torito CDROM boot read of 4 sectors from CDROM.
; mboot is first sector, pboot is second sector.  Verify the presence of
; pboot by checking for a signature and then jump to it.
;
; Entry is also possible by non-El Torito boot (including systems that
; improperly implement the El Torito spec).  This case is indicated by
; the absence of the pboot signature and mboot puts out a warning message
; then hangs.
;
; When booted from a fully-compliant El Torito BIOS, mboot will be
; loaded at linear address 0x7C00 and executed with CS:IP 7C0:0.
; Load via other means might involve a different linear address or
; a different CS:IP.  [We have seen an example of an El Torito BIOS
; that uses 0:7C00.]  So make as few assumptions about load point and
; CS:IP as possible.
;
; The one assumption we do make is that it is safe to put a small
; stack starting downwards from 0:7C00.  That is a fairly standard
; location for a bootstrap stack and is safer than making assumptions
; about where the bootstrap is loaded or what its CS and IP are.
;

bootstk	equ	7c00h
loadpt	equ	0

	.MODEL TINY, C, NEARSTACK
	.CODE
	.386

	org	loadpt
boot	proc	far

;	Set up the stack below the load point
;	Force both data segments to be 0-based
	xor	ax, ax
	mov	ss, ax
	mov	sp, bootstk
	mov	ax, cs
	mov	ds, ax
	mov	es, ax

;	Calculate expected offset for pboot
	call	next
next:
	pop	si
	sub	si, offset next		; offset of 'boot' in CS
	lea	bx, 200h[si]

;	Look for pboot signature 'CDPB'.  If found, calculate the CS:IP
;	such that IP is 0 and jump to pboot.
	cmp	word ptr 2[bx], 04443h	; "CD"
	jne	no_pboot
	cmp	word ptr 4[bx], 04250h	; "PB"
	jne	no_pboot
	shr	bx,4
	add	ax, bx
	push	ax
	push	0
	ret

no_pboot:
;	Put out a string, explaining how to boot the CD.
	lea 	si, msg[si]		; offset of 'msg' in DS
	call	outstr

hang:
	jmp	hang
boot	endp

outstr	proc	near
	lodsb
	or	al, al
	jz	endstr
	call	outchar
	jmp	outstr
endstr:
	ret
outstr	endp

outchar	proc	near
	mov	ah, 0eh
	xor	bx, bx
	int	10h
	ret
outchar	endp

;	This ORG statement places the message at a fixed offset so that
;	build tools could patch it for changing the wording, translating
;	into a different language, making it release specific etc.
	org	loadpt + 80h

msg	db	0dh, 0ah, 0ah, 0ah, 0ah
	db	24 dup ( ' ' ), "Solaris x86 bootable CDROM"
	db	0dh, 0ah, 0ah
	db	15 dup ( ' ' ), "Copyright (c) 1999 by Sun Microsystems, Inc."
	db	0dh, 0ah, 0ah
	db	3 dup ( ' ' ), "Direct CDROM boot failed.  Check the "
	db	"system BIOS to make sure that CDROM"
	db	0dh, 0ah
	db	3 dup ( ' ' ), "boot is enabled then retry the boot, or "
	db	"reboot using the boot diskette."
	db	0dh, 0ah, 0

;	Space for FDISK table
	org	loadpt + 1beh
	db	64 dup (0)

;	Boot signature at end of sector
	db	55h,0aah

	end     boot
