;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)rd_cursr.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	read_cursor  (rd_cursr.s)
;
;   Calling Syntax:	rowcol = read_cursor ( page# )
;
;   Description:	returns the current cursor position on the specified
;			page.  row/col are returned (packed) in AX.
;			AH = row, AL = column
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

PUBLIC read_cursor
EXTERN tmpAX : WORD

read_cursor:			;[row/col] = read_cursor ( page )

     pusha
     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 14h
else
     add bp, 12h		;specify the target page

endif

     mov bh, [bp] 

     mov ah, 03h		;read cursor function number
     int 10h

     mov tmpAX, dx
     popa
     mov ax, tmpAX		;ah=row, al=column

if FARCODE

     retf
else
     ret

endif

     END

