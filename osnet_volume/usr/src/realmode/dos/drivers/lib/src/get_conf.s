;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)get_conf.s	1.1	97/01/17 SMI\n"
; 
;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real mode environment that the operating 
; system would normally supply.
;
;   Function name:	get_sysconf  (get_conf.s)
;
;   Calling Syntax:	confp = get_sysconf ()
;
;   Description:	no input argument; returns a far pointer to the
;                       system configuration table stored within the BIOS
;                       data area.
;
;

    .MODEL SMALL, C, NEARSTACK
    .386

;    .DATA


    .CODE			;code segment begins here

PUBLIC  get_sysconf

get_sysconf:			;page = get_sysconf ( )

     push es			;returns pointer to BIOS data
     push bx
     mov ah, 0C0h		;system configuration function number
     int 15h

     mov ax, es
     mov dx, ax
     mov ax, bx
     pop bx
     pop es

     ret
     END
