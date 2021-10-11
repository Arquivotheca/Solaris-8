;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;

; ident	"@(#)callmdb.s 1.18	95/03/10 SMI\n"

;
;Boot sector for MDB diskette
;
    .MODEL TINY, C, NEARSTACK
;
;NOTE the missing stack declaration
;
    .386

PBYTE   TYPEDEF PTR BYTE

BPB_T   STRUCT                      ;BIOS Parameter Block Declaration

    ;NOTE: This is the format that is supported by DOS 5.0.
    ;It is backward-compatible with earlier versions of DOS.

    VDiskName            BYTE     'SunSoft '
    VBytesPerSector      WORD     0200h          ;same for all diskette types
    VSectorsPerCluster   BYTE     1              ;same for all diskette types
    VReservedSectors     WORD     1              ;size of boot record
    VNumberOfFATs        BYTE     2              ;

;media parameters for various capacity diskettes:   360   720   1.2   1.44
;----------------------------------------------------------------------------
    VRootDirEntries      WORD     224            ;  112   112   224    224
    VTotalSectors        WORD     2880           ;  720  1440  2400   2880
    VMediaDescriptor     BYTE     0F0h           ; 0FDh  0F9h  0F9h   0F0h
    VSectorsPerFAT       WORD     9              ;    2     3     7      9
    VSectorsPerTrack     WORD     18             ;    9     9    15     18
    VNumberOfHeads       WORD     2              ;same for all diskette types
    VHiddenSectorsL      WORD     0              ;same for all diskette types
    VHiddenSectorsH      WORD     0              ;same for all diskette types
;V4 VBoot Record extension area
    VTotalSectorsBig     DWORD    0              ;used if medium >32M
    VPhysicalDriveNum    BYTE     0              ;
    VV4Reserved          BYTE     0
    VExtBootSignature    BYTE     29h
    VVolSerialNumber     DWORD    86h
    VVolumeLabel         BYTE     'SunOS 5.5  '  ;11-char volume label
    VFATType             BYTE     'FAT12   '     ;8-char filesystem type tag
    
    ;NOTE: The following two fields have been added by us (SunSoft) so that
    ;this one program can be used for both floppy and hard disk.
    VbpbOffsetHigh	WORD	0
    VbpbOffsetLow	WORD	0

BPB_T   ENDS


	PUBLIC  @Startup

	PUBLIC  tmpAX 
	PUBLIC	tmpDX      
	PUBLIC  BootDev
	PUBLIC  read_disk
	PUBLIC  LoadErr1, LoadErr1Siz

	EXTERN  solaris_mdboot:NEAR


	.CODE			; code segment begins here

	org	7c00h

@Startup:
	jmp	bootrun
	nop			; this noop is required to fill out the
				; jump instruction. the spec shows that
				; the first three bytes of the boot sector
				; are part of the jump instruction which is
				; then followed by the boot parameter block.
				; lots of programs are counting on this
				; alignment.

_FD_BPB		BPB_T   { }	; take default values from declaration
EXTERNDEF C _FD_BPB: BPB_T

Banner          BYTE    "Solaris Boot Sector"

;; the hex 0xff is a mark which is placed at the end of the version string.
;; this marker can be used to help identify the end of the version string
;; in the binary. without this marker there's nothing to distinguish the
;; end of the Version string and the beginning of LoadErr1.
Version		BYTE	"    Version 1", 0ffh

LoadErr1        BYTE    "Incomplete MDBoot load."
LoadErr1Siz     WORD    sizeof LoadErr1
N_RETRIES       equ     5

bootrun:

;	assume cs:_TEXT, ds:_TEXT, es:_TEXT, ss:_TEXT
	cli
	mov	ax, cs		; initialize segment registers
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	mov	sp, @Startup
	sti
	
	;; the NEC Powermate and Toshiba T4800CT both pass in
	;; bogus values in the dx register, normally these values
	;; are 0 or 1, when booting from the floppy. if the first
	;; read fails BootDev is reset to 0 and the read is tried
	;; again.
	;; this comment has been place here because the old mdboot
	;; would always just zero out the dx register which isn't
	;; the correct either.

	mov	BootDev, dx	; int 19h passes boot device in dl

	push	es
	mov	ax, 40h         ; read current video mode, and reset it 
				; to the same. 
	mov	es, ax
	mov	bp, 49h         ; address of video mode
	mov	ah, 0h          ; video mode switch function number
	mov	al, es:[bp]
	int	10h

	pop	es

	mov	ax, 0920h       ; write screen/character function
	mov	bx, 0007h       ; video page, attribute
	mov	cx, 800h        ; screen size
	int	10h

	;; display the banner at the top of the screen.
	mov	ax, 1301h		; function, cursor mode
	mov	cx, sizeof Banner	; string length
	mov	dx, 0000h		; row, column
	mov	bp, offset Banner	; string pointer
	int	10h

	;; do the same for the version string except print the string
	;; at row 0, column 68
	mov	ax, 1301h
	mov	cx, sizeof Version
	mov	dx, 0043h
	mov	bp, offset Version
	int	10h

	call	solaris_mdboot	; C entry point

hang:
	jmp	hang

fatal_err PROC NEAR

	; di contains pointer to error message string,
	; si contains string length
	; since we have string length, ASCIIZ termination is not required.

	mov	ax, ds
	mov	es, ax

	mov	ax, 1301h       ;function, cursor mode
	mov	bx, 004fh       ;video page, attribute
	mov	cx, si          ;string length
	mov	dx, 1700h       ;row, column
	mov	bp, di          ;string pointer
	int	10h

forever:
	jmp	forever

fatal_err ENDP



read_disk PROC NEAR

	push es
	pusha			; reads a block from the specified device

	mov	bp, sp		; figure out where our args are....
	add	bp, 14h

	mov	si, N_RETRIES	; configurable built-in retry mechanism

rd_retry:
	mov	ah, 02h
	mov	al, 8[bp]	; al contains number of sectors to be read
	mov	bx, 10[bp]	; es:bx contains pointer to target buffer
	mov	ch, 2[bp]	; ch contains cylinder number
	mov	cl, 3[bp]	; bits 8&9 of cylinder
	shl	cl, 6
	or	cl, 6[bp]	; cl contains sector number
	mov	dh, 4[bp]	; dh contains head/track number
	mov	dl, [bp]	; dl contains drive number
	mov	es, 12[bp]

	int	13h
	mov	tmpAX, ax	; ah contains return code
	jnb	rd_done		; retry operation if carry
	dec	si
	jnz	rd_retry
	jmp	rd_exit

rd_done:
	mov	al, 8[bp]	; overwrite returned number of sectors
	mov	tmpAX, ax

rd_exit:
	popa			; 0=no error, other diskette svc. error code
	mov	ax, tmpAX	; return number of sectors read
	pop	es
	ret

read_disk ENDP


       .DATA
     
tmpAX	WORD	0
tmpDX	WORD	0
BootDev	WORD	0

	END @Startup




