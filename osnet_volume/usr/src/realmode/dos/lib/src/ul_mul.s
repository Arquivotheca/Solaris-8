;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)ul_mul.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	ul_mul  (ul_mul.s)
;
;   Calling Syntax:	product = ul_mul ( long multiplicand, long multiplier )
;
;   Description:	unsigned long multiplication; multiplicand (long) and
;			multiplier (long).  returns product in EDX:EAX.
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

;    .DATA


    .CODE			 ;code segment begins here

PUBLIC  ul_mul

ul_mul:		         	 ;product = ul_mul ( )

     enter 0, 0
     push bx
     mov bp, sp

if FARCODE
     
     add bp, 8h
else
     add bp, 6h                  ;vla fornow - verify this offset!

endif

     mov eax, [bp]               ;load both halves of dividend
     mov ebx, 4[bp]

     mul ebx                     ;perform unsigned long multiplication

     push eax
     xor eax, eax                ;reset upper end of extended registers
     mov eax, ebx
     pop ax                      ;product loaded in EDX:EAX, but ignore
     pop dx                      ;high-order four bytes
     pop bx
     leave

if FARCODE

     retf
else
     ret

endif

     END


































