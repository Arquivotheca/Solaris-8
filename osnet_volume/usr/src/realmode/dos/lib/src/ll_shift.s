;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)ll_shift.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	ll_shift  (ll_shift.s)
;
;   Calling Syntax:	newval = ll_shift ( long oldvalue, short shiftnum )
;
;   Description:	performs a left logical shift on a long value.
;			NOTE: uses extended register EAX.
;
;   Assumptions:	this routine uses external variables "tmpAX" and
;			"tmpDX"; these must be defined by the calling routine.
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

PUBLIC  ll_shift
EXTERN  tmpAX:WORD
EXTERN  tmpDX:WORD

ll_shift:			;newvalue = ll_shift ( )

     pusha
     mov bp, sp

if FARCODE
     
     add bp, 14h
else
     add bp, 12h                 ;vla fornow - verify this offset!

endif

     mov eax, [bp]               ;target number
     mov cl, 4[bp]               ;shift distance

     shl eax, cl                 ;perform left logical shift

     push eax
     xor eax, eax                ;clear out upper half of extended register
     pop ax
     mov tmpAX, ax
     pop dx
     mov tmpDX, dx
     xor eax, eax                ;don't leave anything in upper half!
     popa
     mov ax, tmpAX
     mov dx, tmpDX

if FARCODE

     retf
else
     ret

endif

     END

