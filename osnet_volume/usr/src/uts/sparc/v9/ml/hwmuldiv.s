/*
 *	Copyright (c) 1988-1991 by Sun Microsystems, Inc.
 */

#ident	"@(#)hwmuldiv.s	1.8	95/03/30 SMI"	/* From SunOS 4.1 1.6 */

#if defined(lint)
#include <sys/types.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#endif  /* lint */

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/simulate.h>

/*
 * C run time subroutines.
 *
 *	Those beginning in `.' are not callable from C and hence do not
 *	get lint prototypes.
 */

#if !defined(lint)

/*
 * Versions of .mul .umul .div .udiv .rem .urem written using the
 * appropriate SPARC instructions.
 */
	RTENTRY(.mul)
	smul	%o0, %o1, %o0
	retl
	rd	%y, %o1
	SET_SIZE(.mul)

	RTENTRY(.umul)
	umul	%o0, %o1, %o0
	retl
	rd	%y, %o1
	SET_SIZE(.umul)

	RTENTRY(.udiv)
	wr	%g0, %g0, %y
	retl
	udiv	%o0, %o1, %o0
	SET_SIZE(.udiv)

	RTENTRY(.div)
	sra	%o0, 31, %o2
	wr	%g0, %o2, %y
	sdivcc	%o0, %o1, %o0
	bvs,a	1f
	xnor	%o0, %g0, %o0	! Corbett Correction Factor
1:	retl
	nop
	SET_SIZE(.div)

	RTENTRY(.urem)
	wr	%g0, %g0, %y
	udiv	%o0, %o1, %o2
	umul	%o2, %o1, %o2
	retl
	sub	%o0, %o2, %o0
	SET_SIZE(.urem)

	RTENTRY(.rem)
	sra	%o0, 31, %o4
	wr	%o4, %g0, %y
	sdivcc	%o0, %o1, %o2
	bvs,a	1f
	xnor	%o2, %g0, %o2	! Corbett Correction Factor
1:	smul	%o2, %o1, %o2
	retl
	sub	%o0, %o2, %o0
	SET_SIZE(.rem)

#endif	/* lint */
