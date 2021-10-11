;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)us_mod.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	us_mod  (us_mod.s)
;
;   Calling Syntax:	remainder = us_mod ( ulong dividend, ushort divisor )
;
;   Description:	unsigned word modulo; dividend (long) and divisor
;			(short).  returns remainder (short) in AX.
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


    .CODE			 ;code segment begins here

PUBLIC  us_mod
EXTERN tmpAX : WORD

us_mod:		         	 ;remainder = us_mod ( )

     pusha
     mov bp, sp

if FARCODE
     
     add bp, 14h
else
     add bp, 12h                 ;vla fornow - verify this offset!

endif

     mov ax, [bp]                ;load both halves of dividend
     mov dx, 2[bp]
     mov bx, 4[bp]               ;load divisor

     or bx, bx                   ;check for zerodivide condition
     jz outtahere

     xor cx, cx                  ;break into two division operations

     cmp dx, bx                  ;can bypass upper half division?
     jb lowermod

     xchg ax, cx                 ;save lower half of dividend
     xchg dx, ax                 ;operate on upper half first
     div bx                      ;perform unsigned word division

     xchg ax, cx                 ;replace lower half of dividend

lowermod:

     div bx

     mov tmpAX, dx            ;remainder loaded in AX by divide operation
     popa
     mov ax, tmpAX

outtahere:

if FARCODE

     retf
else
     ret

endif

     END
