;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)clrscr_c.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	clr_screen_attr  (clrscr_c.s)
;
;   Calling Syntax:	clr_screen_attr ( attribute )
;
;   Description:	clears the current video page with the specified color.
;		        no return code.
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

PUBLIC clr_screen_attr

clr_screen_attr:		;clr_screen_attr ( )

     push es			;alternate method of clearing the screen
     pusha			;read the current video mode, 
     mov bp, sp

if FARCODE

     add bp, 16h
else
     add bp, 14h

endif

     mov ax, 40h		;and reset it to the same.
     mov es, ax
     mov di, 49h		;address of video mode
     mov ah, 0h			;video mode switch function number
     mov al, es:[di]
     int 10h

     mov di, 62h                ;address of video page
     mov ax, 0920h
     mov bh, es:[di]
     mov bl, [bp]
     mov cx, 800h
     int 10h

     popa
     pop es

if FARCODE

     retf
else
     ret

endif

     END

