;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)catalog.s	1.3	99/06/07 SMI\n"

; Assembler file for generating El Torito boot catalog.
;
;	The location of the boot catalog is defined in the Boot Record
;	Volume Descriptor (see brvd.s).
;
;	At present the boot catalog consists of just the required
;	Validation Entry and Initial/Default entry.  Other entries
;	might be added in future.
;
;	The Initial/Default entry describes the location of the start
;	of the bootstrap on the CD in 2K sectors (set by LOADPT below)
;	the length of the bootstrap in 512K sectors (set by NSECT), and
;	the memory segment address at which the bootstrap is to be
;	loaded (set by LOADSEG).  Execution starts at seg:0.
;

;
; Standard values for Solaris CD.
;
; Originally we used entries that directly load bootblk from the CD.
; But we found that some El Torito BIOSes had only partial
; implementations of the El Torito spec.  So now we place boot code in
; mboot and pboot, tell the BIOS to read the first 4 sectors of the CD
; to the default location and use pboot to read bootblk.  That makes
; our Boot Catalog look very similar to those of other operating
; systems that use El Torito, decreasing the chance of encountering
; incompatible BIOSes.
;
LOADSEG	EQU	0	; Default load segment (7C0)
LOADPT	EQU	0	; Block number of mboot
NSECT		EQU	4

	.MODEL	TINY, C, NEARSTACK
        .CODE
	org	0h
start:

; Start of Validation Entry (bytes 0 - 1F)

	db	1		; Header ID (fixed)
	db	0		; Platform ID (0 => x86)
	dw	0		; Reserved

;	ID string (24 bytes)
	db	'Sun Microsystems Inc.', 0, 0, 0

	dw	0		; Checksum (will be calculated later)
	db	55h, 0AAh	; Key bytes (fixed)

; End of Validation Entry (bytes 0 - 1F)


; Start of Initial/Default Entry (bytes 20 - 3F)

	db	88h		; Boot indicator (88h => bootable)
	db	0		; Media type (0 => no emulation)
	dw	LOADSEG		; Memory segment where bootstrap goes

;	The El Torito spec says that the system type should match the
;	system type in the boot image.  This reference presumably is
;	intended to apply only to hard disk emulations.
	db	0		; System type

	db	0		; Reserved
	dw	NSECT		; Sector count in bootstrap or image
	dd	LOADPT		; Bootstrap/image offset on CDROM
	db	14h dup (0)	; Reserved

; End of Initial/Default Entry (bytes 20 - 3F)

; File will be null-extended to 2K by build tools

	END	start
