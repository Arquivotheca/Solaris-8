;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)set_page.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	set_page  (set_page.s)
;
;   Calling Syntax:	set_page ( new page# )
;
;   Description:	make the specified video page the current one.
;			No return code.
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

PUBLIC set_page

set_page:			;set_page ( new page# )

     pusha			;resets the current video page
     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 14h
else
     add bp, 12h		;specify the target page

endif

     mov al, [bp]
     mov ah, 05h		;set page function number
     int 10h
     popa

if FARCODE

     retf
else
     ret

endif

     END
