;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)clrscrn.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	clr_screen  (clrscrn.s)
;
;   Calling Syntax:	clr_screen ()
;
;   Description:	clears the current video page
;			no input arguments; no return code
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

PUBLIC clr_screen

clr_screen:			;clr_screen ( )
     push es			;alternate method of clearing the screen
     pusha			;read the current video mode, 
     mov ax, 40h		;  and reset it to the same.
     mov es, ax
     mov di, 49h		;address of video mode
     mov ah, 0h			;video mode switch function number
     mov al, es:[di]
     int 10h
     popa
     pop es

if FARCODE

     retf
else
     ret

endif

     END
