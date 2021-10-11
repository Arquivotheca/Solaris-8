;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)ask_time.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	ask_time  (ask_time.s)
;
;   Calling Syntax:	ticks = ask_time ()
;
;   Description:	returns the system time in clock ticks.
;			Value is returned in DX:AX.
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

PUBLIC  ask_time

ask_time:			;ticks = ask_time ( )

     push cx
     
     mov ax, 00h                ;read system clock function number
     int 1ah

     mov ax, dx                 ;low word of tick count
     mov dx, cx                 ;high word of tick count

     pop cx

if FARCODE

     retf
else
     ret

endif

     END
