;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)ra_shift.s	1.2	97/03/10 SMI\n"
; 
; Realmode driver C language support.  Calls to this routine are generated
; by the compiler.
; 
    .MODEL SMALL, C, NEARSTACK
    .386
    .CODE			;code segment begins here

; Right logical shift of 32-bit quantity in ax (lo) and dx (hi)
; by cl bits.

PUBLIC  _aNlshr
_aNlshr:			;newvalue = ra_shift ( )

	xor	ch, ch
	jcxz	done
again:
	sar	dx, 1
	rcr	ax, 1
	loop	again
done:
	ret

	END

