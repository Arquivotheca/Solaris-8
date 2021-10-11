;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)rl_shift.s	1.2	97/03/10 SMI\n"
;
; Realmode driver C language support.  Calls to this routine are generated
; by the compiler.
;
    .MODEL SMALL, C, NEARSTACK
    .386
    .CODE			;code segment begins here

; Right logical shift of 32-bit quantity in ax (lo) and dx (hi)
; by cl bits.

PUBLIC  _aNulshr
_aNulshr:			;newvalue = rl_shift ( )

	xor	ch, ch
	jcxz	done
again:
	shr	dx, 1
	rcr	ax, 1
	loop	again
done:
	ret

	END
