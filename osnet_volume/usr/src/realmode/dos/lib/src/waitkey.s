;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)waitkey.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	waitkey  (waitkey.s)
;
;   Calling Syntax:	waitkey ()
;
;   Description:	wait for user to press a key; does not return ASCII
;			key code.  No input arguments, no return code.
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

PUBLIC wait_key

wait_key:			;wait_key ( )
     mov ah, 0h
     int 16h			;waits for a character from keyboard

if FARCODE

     retf
else
     ret

endif

     END
