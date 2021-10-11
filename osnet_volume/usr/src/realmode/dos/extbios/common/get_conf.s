; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;
; ident	"@(#)get_conf.s	1.6	94/05/23 SMI\n"
;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
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

if FARCODE

     retf
else
     ret

endif

     END
