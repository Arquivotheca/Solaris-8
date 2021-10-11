;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)prtstr_p.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	prtstr_pos  (prtstr_p.s)
;
;   Calling Syntax:	prtstr_pos ( pstr, strlen, page, row, col )
;
;   Description:	prints the string to the specified video page,
;			at the designated coordinates.  No return code.
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

PUBLIC prtstr_pos

prtstr_pos:			;prtstr_pos ( pstr, strlen, page, row, col )

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
     mov bl, 1bh
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
	mov	di, sp
	add	di, 14h

	mov	ax, ds
	mov	es, ax
	mov	bp, [di]		;bp points to output string

	mov	ah, 13h
	mov	al, 01h
	mov	bh, 4[di]
	mov	bl, 1bh
	mov	cx, 2[di]		;length of output string
	mov	dh, 6[di]		;output row
	mov	dl, 8[di]		;output column
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
