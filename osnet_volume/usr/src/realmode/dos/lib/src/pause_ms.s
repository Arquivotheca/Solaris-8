;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)pause_ms.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	pause_ms  (pause_ms.s)
;
;   Calling Syntax:	pause_ms ( #milliseconds )
;
;   Description:	suspends processing for specified period of time.
;			no return code.
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

PUBLIC pause_ms

pause_ms:			;pause_ms ( #milliseconds )

     pusha			;pause for a specified amount of time
     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 14h
else
     add bp, 12h		;specify naptime (in milliseconds)

endif

     mov ax, [bp]
     mov cx, 1000               ;cx:dx contains number of microsecs
     mul cx                     ;rounded to nearest multiple of
     mov cx, dx                 ;976 microseconds (one RTC tick)
     mov dx, ax
     mov ah, 86h                ;wait function number
     int 15h
     popa

if FARCODE

     retf
else
     ret

endif

     END
