;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)ask_page.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	ask_page  (ask_page.s)
;
;   Calling Syntax:	curpage = ask_page ()
;
;   Description:	no input argument; returns current video page.
;
;   Assumptions:	this routine uses an external variable "tmpAX",
;			that must be defined by the calling routine.
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

PUBLIC  ask_page
EXTERN tmpAX : WORD

ask_page:			;page = ask_page ( )

     push es			;returns current video page
     pusha
     mov ax, 40h                ;address of the BIOS data area
     mov es, ax
     mov di, 62h		;address of video page
     mov ah, 0h			;video mode switch function number
     mov al, es:[di]

     mov tmpAX, ax
     popa
     pop es
     mov ax, tmpAX		;return value: number of current video page

if FARCODE

     retf
else
     ret

endif

     END
