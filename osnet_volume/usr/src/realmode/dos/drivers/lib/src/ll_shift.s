;
; Copyright (c) 1997-1999 by Sun Microsystems, Inc.
; All rights reserved.
;
;
; ident "@(#)ll_shift.s	1.4	99/06/02 SMI\n"
;
; Realmode driver C language support.  Calls to this routine are generated
; by the compiler.
; 
    .MODEL SMALL, C, NEARSTACK
    .386
    .CODE			;code segment begins here

PUBLIC  _aNlshl
_aNlshl:			; Shift DX:AX left by CX bits and return
				; the result in DX:AX 

	jcxz	done
again:
	shl	dx, 1
	shl	ax, 1
	jnc	no_dx
	or	dx, 1
no_dx:
	dec	cx
	jnz	again
done:
	ret
	END

