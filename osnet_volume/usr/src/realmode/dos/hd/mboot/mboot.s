;
; Copyright (c) 1993-1999 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)mboot.s	1.16	99/04/06 SMI\n"

; 
; SOLARIS MASTER BOOT:
; 
; PURPOSE: loads the primary boot from the active fdisk partition.
;           in effect, this routine mimics the functionality of INT 19h.
; 
; resides on the first physical sector of the hard drive media.
; loaded by INT 19h (ROM bootstrap loader) at address 7C00h
; limited to 512 bytes total, including embedded fdisk table.
;   
; for compatibility with the ROM BIOS, we contain standard DOS structures:
;     
;      the fdisk partition table (at offset 1BEh-1FEh)
;      boot signature bytes (55h, 0AAh at 1FEh, 1FFh)  
; 
; the above two entities are required in order to be compatible with 
; the manner in which the DOS BIOS has always performed its boot operation.
; In the event that our master boot record is inadvertently replaced by
; a standard DOS boot sector, the booting operation will still succeed!
;
; This master boot record uses the relsect/numsect fields of the partition
; table entry, to compute the start of the active partition; therefore,
; it is geometry independent.  This means that the drive could be "built"
; on a system with a disk controller that uses a given disk geometry, but
; would run on any other controller.
; 
; SYNOPSIS:
;     begins execution at 0:7C00h
;     relocates to 0:600h (to get out of the way!)
;     reads fdisk table to locate bootable partition
;     load boot record from the active fdisk partition at 7C00h
;     verify boot record signature bytes
;     jump to/execute the SOLARIS PARTITION PRIMARY BOOT
;     error handler - can either reboot, or invoke INT 18h.
;  
; interface from DOS INT 19h:  BootDev in DL
; (this fails sometimes, so we look for a signature to determine whether
;  to rely on DL from the floppy boot, or if we should assume 0x80 from 
;  the BIOS)
;
; interface to partition boot: BootDev in DL
;
;============================================================================== 
;Master boot record: resides on first physical sector of device

	TITLE Solaris_Master_Boot
	.MODEL SMALL, C, NEARSTACK
;
;NOTE the missing stack declaration
;
	.386P

	.CODE                       ;code segment begins here

PUBLIC  BootDev

	org 600h

RELOC_ADDR      equ     600h
FDISK_START     equ     01BEh
PBOOT_ADDR      equ     00007C00h	; assumes loaded at segment 0
BOOT_SIG        equ     0AA55h
N_RETRIES       equ     5    

FD_NUMPART      equ     4
FD_PTESIZE      equ     10h
ACTIVE          equ     80h
SUNIXOS         equ     82h         ;our Solaris "systid" code


PUBLIC  @Startup
@Startup:
    jmp bootrun

Version         BYTE    'M3.0'      ;ident string

bootrun:

    assume cs:_TEXT, ds:_TEXT, es:_TEXT, ss:_TEXT
    cli                             ;don't bother me now!

    ;prepare to relocate ourselves
    cld                             ;prepare for relocation
    mov si, 7C00h                   
    mov di, RELOC_ADDR

    ;set up segment registers
    mov ax, cs                      ;initialize segment registers
    mov ss, ax
    mov sp, si                      ;stack starts down from 7C00
    mov es, ax
    mov ds, ax

    push cx                         ;save possible signature on stack
    mov cx, 100h
    rep movsw
    pop cx                          ;restore saved cx
    jmp new_home-7c00h+RELOC_ADDR   ; running at 7c00, jump to 600-rel addr

new_home:

    sti                             ;re-enable interrupts

    ; assuming boot device number is in dl has caused problems in the past
    ; since we still don't absolutely have to rely on it, I've just
    ; removed the now-pointless code to check for the FACE-CAFE signature
    ; from mdexec, which doesn't do anything anymore, but left the assumption
    ; that BootDev is 0x80 and nothing but.  If we ever need to have BIOS
    ; load us from a drive not numbered 0x80, we'll need to uncomment the
    ; following line; otherwise, the initialized value of BootDev, namely
    ; 0x80, will be used for disk accesses.
    ;
    ; mov BootDev, dl

    ; set debug flag based on seeing "both shift down"
    ;
    mov ah, 2		; get shift state
    int 16h
    and al, 3		; isolate shift-key bits
    cmp al, 3
    jne nodbg
    mov debugmode, 1	; set to 1

nodbg:

    ; Search the fdisk table sequentially to find a physical partition
    ; that is marked as "active" (bootable).

    mov bx, RELOC_ADDR + FDISK_START
    mov cx, FD_NUMPART

nxtpart:
    cmp BYTE PTR [bx], ACTIVE
    je got_active_part
    add bx, FD_PTESIZE
    loop nxtpart

noparts:                  
    mov bp, offset NoActiveErrMsg
    mov cx, sizeof NoActiveErrMsg
    jmp fatal_err

got_active_part:

    mov ah, 0			; reset disk
    int 13h

    push bx			; save partition pointer

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

    mov bp, offset lbastring		
    mov cx, sizeof lbastring
    call debugout

    ; LBA case: form a packet on the stack and call fn 42h to read
    ; packet, backwards (from hi to lo addresses):
    ; 8-byte LBA
    ; seg:ofs buffer address
    ; byte reserved
    ; byte nblocks
    ; byte reserved
    ; packet size in bytes (>= 0x10)

    pop bx			; restore partition pointer
    push bx			; and save again
    mov cx, N_RETRIES		; retry count
retryLBA:
    pushd 0		        ; hi 32 bits of 64-bit sector number
    push dword ptr [bx+8] 	; relsect (lo 32 of 64-bit number)
    push dword ptr [solaris_priboot]	; seg:ofs of buffer
    push 0001			; reserved, one block
    push 0010H			; reserved, size (0x10)
    mov ah, 42H			; "read LBA"
    mov si, sp			; (ds already == ss)
    int 13h
    lahf			; save flags
    add sp, 16			; restore stack
    sahf			; restore flags
    jnc readok			; got it
    mov ah, 0			; reset disk
    int 13h
    loop retryLBA		; try again
    jmp readerr			; exhausted retries; give up

noLBA:

    mov bp, offset chsstring
    mov cx, sizeof chsstring
    call debugout

    pop bx			; restore partition pointer
    push bx			; and save again

    ;get BIOS disk parameters
    mov dl, BootDev
    mov ah, 08h
    int 13h

    jnc geomok

    ; error reading geom; die
    mov bp, offset GeomErrMsg
    mov cx, sizeof GeomErrMsg
    jmp fatal_err

geomok:
    ; calculate sectors per track
    mov al, cl		; ah doesn't matter; mul dh will set it
    and al, 03Fh
    mov secPerTrk, al

    ; calculate sectors per cylinder
    inc dh
    mul dh
    mov secPerCyl, ax

    ; calculate cylinder #

    mov ax, [bx+8]	; ax = loword(relsect)
    mov dx, [bx+10]	; dx:ax = relsect
    div secPerCyl	; ax = cyl, dx = sect in cyl (0 - cylsize-1)
    mov bx, ax          ; bx = cyl

    ; calculate head/sector #
    mov ax, dx          ; ax = sect in cyl (0 - cylsize-1)
    div secPerTrk       ; al = head, ah = 0-rel sect in track
    inc ah              ; ah = 1-rel sector

    xor	cl,cl		; cl = 0
    mov ch, bh		; ch = hi bits of cyl (if any)
    shr cx, 2		; cl{7:6} = cyl{9:8} (if any)
    and cl, 0C0h	; cl = cyl{9:8} to merge with sect (if any)

    or  cl, ah		; cl{7:6} = cyl bits, cl{5:0} = sect
    mov ch, bl		; ch = lo cyl bits
    mov dh, al		; dh = head
    mov dl, BootDev	; dl = drivenum
    les bx, solaris_priboot  ; es:bx points to buffer

    mov si, N_RETRIES
retry_noLBA:
    mov ax, 0201H	; 02=read, sector count = 1

    int 13h
    jnc readok
    mov ah, 0			; reset disk
    int 13h
    dec	si
    cmp si,0
    jne retry_noLBA	; retry, or fall through to read error

readerr:
    mov bp, offset ReadErrMsg
    mov cx, sizeof ReadErrMsg
    jmp fatal_err

readok:
    ;verify boot record signature    
    mov bx, PBOOT_ADDR
    cmp WORD PTR [bx+510], BOOT_SIG
    je sigok

    mov bp, offset SigErrMsg
    mov cx, sizeof SigErrMsg
    jmp fatal_err              

sigok:
    mov dl, BootDev 			; pass BootDev to next boot phase
    pop si				; and pass partition pointer ds:si
    call DWORD PTR solaris_priboot  	; we never return from this call!

    mov bp, offset ReturnErrMsg
    mov cx, sizeof ReturnErrMsg

fatal_err:                      ;land of no return.......

    ;bp contains pointer to ASCIIZ error message string,
    ;cx contains string length
    mov bx, 004fh       ;video page, attribute
    call msgout

;forever:
;    jmp forever
    int 18h


debugout:
    ;; call with string pointer in es:bp, len in cx
    cmp debugmode, 0
    je debugout_ret		; skip if not in debug mode

    mov bx, 001fh		; page, attr (white on blue) 

    ; alternate entry for fatal_err
msgout:
    pusha
    mov ax, 1301h
    mov dx, 1700h		; row, col
    int 10h

    mov al, 7			; BEL
    mov cx, 1
    int 10h
    
    mov ah, 0			; get key
    int 16h
    popa

debugout_ret:
    ret

secPerTrk       BYTE 0
secPerCyl       WORD 0
solaris_priboot DWORD PBOOT_ADDR
BootDev         BYTE 80h		; assumes drive 80 (see comment above)
debugmode	BYTE 0

GeomErrMsg      BYTE    "Can't read geometry"
NoActiveErrMsg  BYTE    "No active partition"
ReadErrMsg      BYTE    "Can't read PBR"
SigErrMsg       BYTE    "Bad PBR sig"
ReturnErrMsg	BYTE	"!!!"
lbastring	BYTE	"LBA"
chsstring	BYTE	"CHS"

; For debugging:  Here's a representative FDISK table entry
;
; 	org	RELOC_ADDR+1BEh
;		db	80h,01h,01h,00h,82h,0feh,7fh,04h,3fh,00h,00h,00h,86h,0fah,3fh,00h
         org 	RELOC_ADDR+1FEh

         WORD    0AA55h

    END @Startup
