;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)brvd.s	1.1	99/02/07 SMI\n"

; Assembler file for generating El Torito boot record volume descriptor.
;
;	The contents of the descriptor are fixed except for the pointer
;	to the first CDROM (i.e. 2K) sector of the boot catalog.
;

CATALOG_SECTOR	EQU	13h

	.MODEL	COMPACT, C, NEARSTACK
        .CODE
	org	0h
start:

;	Bytes 0 - 1F
        db	0, 'CD001', 1, 'EL TORITO SPECIFICATION', 0, 0

;	Bytes 20 - 3F
	db	32 dup (0)

;	Bytes 40 - 5F
	db	7 dup (0)
	dd	CATALOG_SECTOR
	db	21 dup (0)

;	File will be null-extended to 2K by build tools

	END	start
