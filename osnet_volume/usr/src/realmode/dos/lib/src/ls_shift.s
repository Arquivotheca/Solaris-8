;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)ls_shift.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	ls_shift  (ls_shift.s)
;
;   Calling Syntax:	newvalue = ls_shift ( short oldvalue, short bits2shift )
;
;   Description:	performs a left logical shift on a short value.
;		        returns the shifted result.
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

PUBLIC  ls_shift
EXTERN tmpAX : WORD

ls_shift:			;rc = ls_shift ( num, distance )

     pusha
     mov bp, sp

if FARCODE
     
     add bp, 14h
else
     add bp, 12h

endif

     mov ax, [bp]               ;target number
     mov cl, 2[bp]              ;shift distance

     shl ax, cl                 ;perform left logical shift

     mov tmpAX, ax
     popa
     mov ax, tmpAX		;return value: new value of shifted integer

if FARCODE

     retf
else
     ret

endif

     END

