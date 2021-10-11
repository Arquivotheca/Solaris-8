;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)putchar.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	putchar  (putchar.s)
;
;   Calling Syntax:	putchar ( outchar )
;
;   Description:	prints a character at the current cursor location.
;			No return code.
;

;this allows the model to be defined from the command line.
;the default in this case is small.

;ifndef model
;model   textequ <small>
;endif

    .MODEL model, C, NEARSTACK
;    include ..\bioserv.inc     this doesn't work-causes multiple def's
    .386

    .CODE			;code segment begins here

PUBLIC putchar

putchar:			;putchar ( char )

     pusha                         
     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 14h
else
     add bp, 12h

endif

     mov al, [bp]
     mov ah, 0eh
     mov bh, 0
     mov bl, 1bh
     int 10h
     popa

if FARCODE

     retf
else
     ret

endif

     END



