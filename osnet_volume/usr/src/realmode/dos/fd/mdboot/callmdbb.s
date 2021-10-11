;
; Copyright (c) 1999 Sun Microsystems, Inc.  All rights reserved.
;

; ident	"@(#)callmdbb.s 1.1	99/06/29 SMI\n"

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

Version		BYTE	"    Version 2"

;; this message here because it must be in first 512 bytes
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

    	; Check for LBA BIOS
	mov ah, 041h		; chkext function
	mov bx, 055AAh		; signature to change
	mov cx, 0
	int 13h
	jc noLBA			; carry == failure
	cmp bx, 0AA55h
	jne noLBA			; bad signature in BX == failure
	test cx, 1			; cx & 1 must be true, or...
	jz noLBA			; ...no LBA

	mov lbamode, 1
	jmp short LBA	

noLBA:
	mov lbamode, 0
	;; calculate sec/cyl
	mov	ax, _FD_BPB.VSectorsPerTrack
	mul	_FD_BPB.VNumberOfHeads
	mov	secPerCyl, ax
LBA:
	;;push	es
	;;mov	ax, 40h         ; read current video mode, and reset it 
				; to the same. 
	;;mov	es, ax
	;;mov	bp, 49h         ; address of video mode
	;;mov	ah, 0h          ; video mode switch function number
	;;mov	al, es:[bp]
	;;int	10h

	;;pop	es

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

; read_disk: es:bx = buffer, dx:ax = startblk, 
;   cx = count (only cl significant)
; read with LBA or CHS as indicated by variable lbamode
; retry up to N_RETRIES times, return cy if failure

read_disk PROC NEAR
    pusha			; save all regs
	
    mov di, N_RETRIES		; retry count for both LBA and CHS

    cmp lbamode, 0		; LBA or CHS?
    je read_disk_chs

    ; LBA case: form a packet on the stack and call fn 42h to read
    ; packet, backwards (from hi to lo addresses):
    ; 8-byte LBA
    ; seg:ofs buffer address
    ; byte reserved
    ; byte nblocks
    ; byte reserved
    ; packet size in bytes (>= 0x10)

read_disk_retry_LBA:
    pushd 0		        ; hi 32 bits of 64-bit sector number
    push dx 			; sector address (lo 32 of 64-bit number)
    push ax			;  in dx:ax, so now on stack with lsb lowest 
    push es
    push bx			; seg:ofs of buffer, ends up LSW lowest
    push cx			; reserved byte, sector count (cl)
    push 0010H			; reserved, size (0x10)
    mov ah, 42H			; "read LBA"
    mov si, sp			; (ds already == ss)
    mov dl, byte ptr BootDev	; drive number
    int 13h
    lahf			; save flags
    add sp, 16			; reset stack
    sahf			; restore flags
    jnc readok			; got it
    dec di			; retry loop count
    jnz read_disk_retry_LBA	; try again
    jmp readerr			; exhausted retries; give up

read_disk_chs:
    push bx		; save a few working regs
    push cx		

    ; calculate cylinder #
    ; dx:ax = relsect on entry

    div secPerCyl	; ax = cyl, dx = sect in cyl (0 - cylsize-1)
    mov bx, ax          ; bx = cyl

    ; calculate head/sector #
    mov ax, dx          ; ax = sect in cyl (0 - cylsize-1)
    div byte ptr _FD_BPB.VSectorsPerTrack       ; al = head, ah = 0-rel sect in track
    inc ah              ; ah = 1-rel sector
    xor	cl,cl		; cl = 0
    cmp bx, 0FFh        ; if cyl < 0x100, no hi bits to mess with
    jbe read_disk_do_chs_read		

    mov ch, bh		; ch = hi bits of cyl
    shr cx, 2           ; cl{7:6} = cyl{9:8} 
    and cl, 0C0h	; cl = cyl{9:8} to merge with sect

read_disk_do_chs_read:
    or  cl, ah          ; cl = (sector & cyl bits) or (sector)
    mov ch, bl		; ch = lo cyl bits
    mov dh, al		; dh = head
    mov dl, byte ptr BootDev    ; dl = drivenum

    pop ax		; ax = sector count (orig cx)
    pop	bx		; bx = buf offset

    mov ah, 02		; 02=read (sector count already there)

read_disk_retry_chs:
    mov si, ax		; save readcmd/count for retry
    int 13h
    jnc readok
    mov ah, 0		; reset disk
    int 13h
    mov ax, si		; restore readcmd/count in case of error
    dec	di
    jnz read_disk_retry_chs ; retry, or fall through to read error

readerr:
    stc			; set carry to indicate failure
    jmp short ret_from_read

readok:
    clc

ret_from_read:
    popa		; restore caller's regs
    ret

read_disk ENDP

	; these data are in CS so they stay in first sector
BootDev         WORD  0
secPerCyl       WORD  0
lbamode		BYTE  0

	END @Startup
