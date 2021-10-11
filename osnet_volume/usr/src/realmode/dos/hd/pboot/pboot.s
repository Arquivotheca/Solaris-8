;
; Copyright (c) 1993, 1999 Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)pboot.s	1.12	99/05/23 SMI\n"

;
; SOLARIS PARTITION PRIMARY BOOT ("pboot"):
;
; PURPOSE: to locate and load bootblk.
;
; Resides on the first logical sector of the Solaris fdisk partition.
; Loaded by mboot at address 0000:7C00h.
; Limited to 512 bytes total.
;   
; For compatibility with the ROM BIOS, we contain standard DOS boot
;     signature bytes (55h, 0AAh at 1FEh, 1FFh)  
; In the event that our master boot record is inadvertently replaced by
; a standard DOS boot sector, the booting operation will still succeed.
; 
; The remaining portion of the real-mode primary boot (referred to
;     as "bootblk") will reside immediately after the VTOC (2 sectors)
; 
; The first two bytes of bootblk are a jump to the start of the actual code.
; Starting at byte position three, we have embedded two pieces of data:
; - the size of bootblk itself (expressed in number of 512-byte sectors)
; - the version number of bootblk (not currently checked by anything)
; We search the fdisk table, find the active partition, and load and run
;  bootblk from it.
; 
; SYNOPSIS:
;     Begin execution at 0000:7C00h
;     Load/read fdisk table to determine active partition disk coordinates.
;     Load first sector of bootblk from active part, BOOTBLK_OFFSET in
;     Get size of bootblk from second 16-bit word
;     Load the rest at BOOTBLK_ADDR, jump to it.
;     Error handler - prints error message on status line, "hangs";
;       (keyboard still active, so user can <ctl><alt><del>!)
;
; interface from mboot: BootDev in dl; 
;		        pointer to active FDISK tbl entry in ds:si (unused)
; interface to bootblk: BootDev in dl, relsect in cx:bx.
;
;============================================================================ 
;Partition boot record; resides on first physical sector of Solaris partition

        TITLE Solaris_Partition_Boot
       .MODEL SMALL, C, NEARSTACK
       .386
       .CODE

PUBLIC  BootDev, relsect, secPerTrk, secPerCyl
PUBLIC  get_drive_geometry, lbamode

MBOOT_ADDR      equ     7E00h		; where we reload mbr for fdisk tbl
FDISK_START     equ     01BEh		; offset within sector
FD_NUMPART      equ     4
FD_PTESIZE      equ     10h
ACTIVE          equ     80h
SUNIXOS         equ     82h

; number of sectors from FDISK entry where bootblk lives:
; 0 is pboot, 1+2 are VTOC, so 3 is first sector of bootblk
BOOTBLK_OFFSET  equ  	3
BOOTBLK_ADDR	equ	60001000h 	; where bootblk is loaded

N_RETRIES	equ	5    

	org 7c00h

PUBLIC  @Startup
@Startup:
    jmp short bootrun

Version         BYTE    'P2.0'      ;ident string

bootrun:

    assume cs:_TEXT, ds:_TEXT, es:_TEXT, ss:_TEXT
    cli
    mov ax, cs                      ;initialize segment registers
    mov ss, ax
    mov sp, @startup
    mov es, ax
    mov ds, ax
    sti

    mov BootDev, dl                 ; mboot passes boot device in dl

    ; set debug flag based on seeing "both shift down"
    ;
    mov ah, 2		; get shift state
    int 16h
    and al, 3		; isolate shift-key bits
    cmp al, 3
    jne nodbg
    mov debugmode, 1
nodbg:

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
    jmp short read_fdisk	; skip geometry stuff

noLBA:
    mov lbamode, 0

get_drive_geometry:

    mov ah, 0		; first, reset disk subsystem
    int 13h

    mov ah, 08h		; get geometry
    int 13h

    jnc geomok

    mov bp, offset GeomErr
    mov cx, sizeof GeomErr
    jmp fatal_err

geomok:
    ;maximum usable sector number
    xor ax, ax
    mov al, cl
    and al, 03Fh
    mov secPerTrk, al

    ;calculate sectors per cylinder
    inc dh
    mul dh
    mov secPerCyl, ax

read_fdisk:
    ; read fdisk table (again, sigh...can't we either rely on ds:si or
    ; at least rely on it being at 0:7BE?)
    ; read into location pointed to by MBOOT_ADDR

    mov	bx, MBOOT_ADDR	; es:bx -> buffer
    mov cx, 1		; number of sectors
    xor dx, dx		; sector 0
    xor ax, ax
    call read_disk	
    jnc	search_fdisk

    mov bp, offset FdiskErr
    mov cx, sizeof FdiskErr
    jmp short fatal_err

search_fdisk:
    ; Search the fdisk table sequentially to find a physical partition
    ; that is marked as "active" (bootable).
    mov bx, MBOOT_ADDR + FDISK_START

    ;look for an active partition
    mov cx, FD_NUMPART

nxtpart:
    cmp BYTE PTR [bx], ACTIVE
    je got_active_part

    add bx, FD_PTESIZE
    loop nxtpart

    mov bp, offset PartErr
    mov cx, sizeof PartErr
    jmp short fatal_err

got_active_part:
    mov ecx, DWORD PTR 8[bx]    ;relsect = start of active partition
    mov relsect, ecx

    ;read first block of bootblk module
    ;ecx contains relative sector number marking start of active partition
    ;compute bootblk location, put sector number into dx:ax

    add ecx, BOOTBLK_OFFSET
    mov ax, cx
    shr ecx, 16
    mov dx, cx			; dx:ax hax block number
    mov cx, 1			; # of sectors
    les bx, boot_blk		; es:bx -> data (es != cs/ds/ss)
    call read_disk
    jc pboot_err

    ; load the rest of bootblk
    mov cx, es:2[bx]		; get sector size from bootblk header[2]
    call read_disk		; everything else is the same
    jnc call_bootblk

pboot_err:
    mov ax, ds
    mov es, ax
    mov bp, offset LoadErr
    mov cx, sizeof LoadErr
    jmp	short fatal_err

call_bootblk:

    mov dl, BootDev
    mov bx, WORD PTR relsect    ;offset
    mov cx, WORD PTR relsect+2  ;segment

    call DWORD PTR boot_blk

hang:
    jmp short hang

fatal_err:                      ;land of no return.......

    ;bp contains pointer to ASCIIZ error message string,
    ;cx contains string length
    mov bx, 004fh       ;video page, attribute
    call msgout

forever:
    jmp short forever


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

; read_disk: es:bx = buffer, dx:ax = startblk, 
;   cx = count (only cl significant)
; read with LBA or CHS as indicated by variable lbamode
; retry up to N_RETRIES times, return cy if failure

read_disk PROC NEAR
    pusha			; save all regs
	
    mov di, N_RETRIES		; retry count for both LBA and CHS

    cmp lbamode, 0		; LBA or CHS?
    je read_disk_chs

    push cx
    mov bp, offset lbastr
    mov cx, sizeof lbastr
    call debugout
    pop cx

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
    mov dl, BootDev		; drive number
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

    mov bp, offset chsstr
    mov cx, sizeof chsstr
    call debugout

    ; calculate cylinder #
    ; dx:ax = relsect on entry


    div secPerCyl	; ax = cyl, dx = sect in cyl (0 - cylsize-1)
    mov bx, ax          ; bx = cyl

    ; calculate head/sector #
    mov ax, dx          ; ax = sect in cyl (0 - cylsize-1)
    div secPerTrk       ; al = head, ah = 0-rel sect in track
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
    mov dl, BootDev     ; dl = drivenum

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

secPerCyl       WORD  0
secPerTrk       BYTE  0
BootDev         BYTE  0
lbamode		BYTE  0
debugmode	BYTE  0
relsect         DWORD 0
boot_blk        DWORD BOOTBLK_ADDR	; for les, pushd, etc.
solaris_mboot 	DWORD MBOOT_ADDR	; for les, pushd, etc.

GeomErr         BYTE    "Can't read geometry"
FdiskErr        BYTE    "Can't find fdisk table"
PartErr         BYTE    "No active part found"
LoadErr         BYTE    "Can't load bootblk"
lbastr		BYTE	"LBA"
chsstr		BYTE	"CHS"

    	org 7DFEh
	WORD 0AA55h

    END @Startup
