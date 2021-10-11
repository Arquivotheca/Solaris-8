;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)readkey.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	read_key  (readkey.s)
;
;   Calling Syntax:	keycode = read_key ( )
;
;   Description:	waits for user input; returns ASCII code for
;			any key pressed.  No input arguments.
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

    .CODE			;code segment begins here

PUBLIC read_key

read_key:			;[scancode/ASCII code] = read_key ( )
     mov ah, 0h			;waits for input
     int 16h			;returns ASCII code for key

     xor ah, ah                 ;ah contains scan code;we want ascii

if FARCODE

     retf
else
     ret

endif

     END
