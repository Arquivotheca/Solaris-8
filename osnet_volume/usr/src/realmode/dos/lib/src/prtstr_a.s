;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)prtstr_a.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	prtstr_attr  (prtstr_a.s)
;
;   Calling Syntax:
;	prtstr_attr ( *msg, strlen, page, row, column, attribute )
;
;   Description:	prints the string on the designated video page,
;			at the specified screen coordinates, in the desired
;			attribute.  No return code.
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

PUBLIC prtstr_attr

prtstr_attr:		;prtstr_attr ( pstr, strlen, page, row, col, attr )

if FARDATA

     push es
     pusha                         
     mov bp, sp			;figure out where our args are....

if FARCODE

     add bp, 16h
else
     add bp, 14h

endif

     mov es, 2[bp]
     mov ax, 1301h
     mov bh, 6[bp]
     mov bl, 12[bp]
     mov cx, 4[bp]
     mov dh, 8[bp]
     mov dl, 10[bp]
     mov di, bp
     mov bp, [bp]
     int 10h
     mov bp, di
     popa
     pop es

else				;for near data
	pusha
	push	es
	mov	bp, sp
	add	bp, 14h

	mov	ax, ds
	mov	es, ax

	mov	ax, 1301h
	mov	bh, 4[bp]
	mov	bl, 10[bp]             ;output attribute
	mov	cx, 2[bp]		;length of output string
	mov	dh, 6[bp]		;output row
	mov	dl, 8[bp]		;output column
	mov	di, bp
	mov	bp, [bp]		;bp points to output string
	int	10h

	pop	es
	popa
endif

if FARCODE

     retf
else
	ret

endif

     END
