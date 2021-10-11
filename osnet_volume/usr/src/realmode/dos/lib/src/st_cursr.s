;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)st_cursr.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	set_cursor  (st_cursr.s)
;
;   Calling Syntax:	set_cursor ( page, row, column )
;
;   Description:	updates cursor position on the specified page
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

PUBLIC set_cursor

set_cursor:			;set_cursor ( page, row, column )

     pusha			;NOTE: screen coordinates are zero-based!
     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 14h
else
     add bp, 12h		;specify the target page

endif

     mov bh, [bp]		;target page
     mov dh, 2[bp]		;target row
     mov dl, 4[bp]		;target column

     mov ah, 02h		;set cursor function number
     int 10h
     popa

if FARCODE

     retf
else
     ret

endif

     END
