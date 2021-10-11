;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)ask_disk.s	1.7	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	ask_disk  (ask_disk.s)
;
;   Calling Syntax:	rc = ask_disk ( drive, drvparm structptr )
;
;   Description:	uses the INT 13h, function 08h call to determine
;			the geometry of the specified device.
;			returns 0 if successful, status code otherwise.
;
;   Restrictions:	supports drive 80h and 81h.
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

PUBLIC  ask_disk
EXTERN tmpAX : WORD

ask_disk:			;rc = ask_disk ( drive, drive parm struct ptr )

if FARDATA
                                ;vla fornow.....check segment loading order
     push es
     push fs
     pusha
     mov si, sp
     add si, 22
     mov dx, ss:[si]		;save drive number in dl
     mov bx, ss:2[si]		;es:bx contains pointer to drive parm struct
     mov ax, ss:4[si]           ;
     mov fs, ax                 ;
     xor ax, ax			;initialize return code register
     mov cx, ax                 ;initialize return data register

     mov ah, 08h		;read drive parameter function number
     int 13h

     cmp ah, 0h                 ;check for error return
     jne fin_load

     mov esi, 000abeh
     mov fs:[bx], esi           ;load magic number
     mov fs:56[bx], dl          ;boot device 
;     mov fs:58[bx],             ;boot controller
;     mov fs:60[bx],             ;boot target/lun      
;     mov fs:62[bx],             ;bytes per sector
     mov si, es:15[di]          ;sectors per track
     mov fs:64[bx], esi
     mov dl, dh
     xor dh, dh
     mov si, dx
     mov fs:68[bx], esi         ;tracks per cylinder
     mov si, es:[di]            ;cylinders per disk
     mov fs:72[bx], esi

fin_load:
     mov tmpAX, ax
     popa
     pop fs
     pop es
     mov ax, tmpAX		;0=no error, other diskette svc. error code
else
	pusha
	push	es
	push	fs
	mov	si, sp
	add	si, 18

	mov	dx, [si]	;save drive number in dl
	mov	di, 2[si]	;si contains pointer to drive param structure
	mov	ax, ds
	mov	fs, ax
	xor	ax, ax		;initialize return code register
	pusha

	mov	ah, 08h		;read drive parameter function number
	int	13

	cmp	ah, 0                 ;check for error return
	jne	fin_load

	mov	esi, 000abeh
	mov	fs:[bx], esi           ;load magic number
	mov	fs:56[bx], dl          ;boot device 
	mov	si, es:15[di]          ;sectors per track
	mov	fs:64[bx], esi
	mov	dl, dh
	xor	dh, dh
	mov	si, dx
	mov	fs:68[bx], esi         ;tracks per cylinder
	mov	si, es:[di]            ;cylinders per disk
	mov	fs:72[bx], esi

fin_load:
	pop	fs
	pop	es
	mov	tmpAX, ax
	popa
	mov	ax, tmpAX	;0=no error, other diskette svc. error code

endif
	ret

     END
