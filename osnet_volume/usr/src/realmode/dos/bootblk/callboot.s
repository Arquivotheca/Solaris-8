;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)callboot.s	1.15	99/02/11 SMI\n"

; Assembler preamble for UFS version of unified bootblk.
;
;	Unified bootblk program still requires separate startup
;	modules for UFS and PCFS to preserve historical behavior
;	expected by their respective bootstrap sectors.
;
;   Standard load point for bootblk is 5000:1000 at the time of
;   writing.  The bottom 1000 is for use as a stack.  The startup
;   code assumes that the program should be loaded at X000:1000
;   but allows CS:IP to be X100:0000 and adjusts it to X000:1000.
;   This adjustment is because bootblk is used as the El Torito
;   bootstrap and El Torito BIOS executes from XXXX:0000.
;
;   If we want to relax the restriction that the CS must be X000
;   we need to make some other assumption, e.g. that there is
;   already a usable stack (dubious) or that we can find a safe
;   place to put a stack (e.g. just below the initial CS).
;

;   The .DOSSEG directive enables the use of _edata and _end
;   references for clearing BSS.  But it also seems to introduce
;   an extra 0x10 bytes in the code segment which interferes
;   with the .org directive.  The .org has therefore been
;   adjusted for the difference.
    .DOSSEG
    .MODEL COMPACT, C, NEARSTACK
;
;NOTE the missing stack declaration
;
    .386

PBYTE   TYPEDEF PTR BYTE

EXTERN	BootDbg:WORD
EXTERN	prog_stack:WORD
EXTERN	stack_size:WORD

EXTERN  boot_blk:NEAR

        .CODE                       ;code segment begins here
;	We want the code to start at 0x1000 because this program gets
;	loaded at X000:1000 and runs with the first 1000 bytes as its
;	stack.  The .DOSSEG directive somehow perverts the origin by
;	0x10 bytes, so adjust accordingly.  [If anyone reading this
;	knows how to access _edata and _end without .DOSSEG, or how to
;	prevent .DOSSEG from adjusting the origin, please clean this up.]
	org 0FF0h
;	org 1000h

TSTACK:
    jmp @Startup

; WARNING: BootNSect tells pboot how many sectors to read.
; It needs to be adjusted as the size of bootblk changes.
; It would have been much better if the original designers
; had used the byte count here so that it could be compiled
; in directly.
BootNSect   WORD    30h
BootVsn     BYTE    'BB3.0', 0      ;ident string
;	BootFlg can be patched easily without rebuilding
;	the bootstrap.  It gets copied into BootDbg in the
;	data segment (see below and bootblk.h).
BootFlg	WORD	0000h;	Production version
;BootFlg	WORD	00FFh;	Some useful flags
;BootFlg	WORD	8000h; reread boot.bin from diskette

@Startup:
;
;	Need to preserve the contents of DL (boot device).
;
    cli
    mov ax, cs
    and ax, 0F000h
    mov di, ax
    add di, SEG prog_stack
    sub di, SEG jmpdest
    mov	ds, di
    mov es, di
    mov	si, OFFSET prog_stack
    add si, stack_size
    mov ss, di
    mov sp, si
    push ax
    push offset jmpdest
    retf
jmpdest:
    sti

;   Clear BSS to nulls
    EXTRN _edata:BYTE
    EXTRN _end:BYTE
    mov  di, offset DGROUP:_edata
    mov  cx, offset DGROUP:_end
    sub  cx, di
    xor  ax, ax
    cld
    rep  stosb

;   Add easily patched BootFlg into easily accessed BootDbg
    mov  ax, cs:BootFlg
    or	 BootDbg, ax

    xor dh, dh
    push dx;			Boot device
    call boot_blk
    add	sp, 2
    call hang

	PUBLIC	hang
hang	PROC	NEAR
	jmp	hang
hang	ENDP

	PUBLIC	register_prep
register_prep	PROC	NEAR
	xor	eax, eax
	xor	ebx, ebx
	xor	ecx, ecx
	xor	edx, edx
	mov	esi, eax
	mov	edi, eax
	mov	fs, ax
	mov	gs, ax
        ret
register_prep	ENDP

        END     TSTACK
