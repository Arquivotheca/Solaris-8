;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)rd_disk.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	read_disk  (rd_disk.s)
;
;   Calling Syntax:
;	rc = read_disk ( dev, cyl#, head#, sector#, nsectors, destination )
;
;   Description:	reads sectors from the specified device; all disk
;			parameters must be specified.
;		        returns operation status in AH. (0 = success)
;		        This function has a built-in retry mechanism;
;			it is currently configured for five retries.
;
;   Assumptions:	this routine uses an external variable "tmpAX",
;			that must be defined by the calling routine.
;

;this allows the model to be defined from the command line.
;the default in this case is small.

;ifndef model
;model   textequ <small>
;endif

    .MODEL model, C, NEARSTACK
;    include ..\bioserv.inc     this doesn't work-causes multiple def's
    .386

;    .DATA


    .CODE			;code segment begins here

N_RETRIES		EQU		5t      ;from "bootdefs.h"
PUBLIC read_disk
EXTERN tmpAX : WORD

read_disk:			;read_disk ( drive, cyl#, head#, sector#, nsectors, buffr )

if FARDATA

     push es
     pusha			;reads a block from the specified device

     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 16h
else
     add bp, 14h

endif

     mov dl, [bp]		;dl contains drive number

     mov dh, 4[bp]		;dh contains head/track number
     mov ch, 2[bp]		;ch contains cylinder number
     mov cl, 6[bp]		;cl contains sector number
     mov al, 8[bp]		;al contains number of sectors to be read

     mov bx, 10[bp]		;es:bx contains pointer to target buffer
     mov es, 12[bp]

     mov si, N_RETRIES          ;configurable built-in retry mechanism
     mov ah, 02h                ;vla fornow.....
     mov tmpAX, ax           ;vla fornow.....
     jmp doit

nxt_loop:

     dec si
     cmp si, 0
     je end_loop

;     mov ah, 0h                 ;if read fails for any reason, reset the
;     int 13h                    ;disk before retrying the operation.

doit:
     mov ax, tmpAX
;     mov ah, 02h		;reset diskette subsystem function number
     int 13h

     cmp ah, 0                  ;check function return code
     jne nxt_loop               ;retry operation if return code is non-zero

end_loop:
     xor al, al                 ;discard number of sectors read (fd only)
     mov tmpAX, ax		;ah contains return code
     popa			;0=no error, other diskette svc. error code
     mov ax, tmpAX		;return operation status
     pop es

else
	pusha			;reads a block from the specified device
	push	es

	mov	di, sp		;figure out where our args are....
	add	di, 14h

	mov	ax, ds
	mov	es, ax

	mov	ah, 02h		;read disk subsystem function number
	mov	al, 8[di]	;al contains number of sectors to be read
	mov	bx, 10[di]	;es:bx contains pointer to target buffer
	mov	ch, 2[di]	;ch contains cylinder number
	mov	cl, 6[di]	;cl contains sector number
	mov	dh, 4[di]	;dh contains head/track number
	mov	dl, [di]	;dl contains drive number

	int	13h
	
	xor	al, al		;discard number of sectors read (fd only)
	mov	tmpAX, ax	;ah contains return code
	pop	es
	popa			;0=no error, other disk svc. error code
	mov	ax, tmpAX	;al contains number of sectors read
endif

if FARCODE

     retf
else
     ret

endif

    END
