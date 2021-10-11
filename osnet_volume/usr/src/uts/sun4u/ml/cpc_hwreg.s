/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpc_hwreg.s	1.1	99/08/15 SMI"

/*
 * Routines for manipulating the UltraSPARC performance
 * counter registers (%pcr and %pic)
 */

#include <sys/asm_linkage.h>

#if defined(lint) || defined(__lint)

#include <sys/cpc_ultra.h>

/*ARGSUSED*/
void
ultra_setpcr(uint64_t pcr)
{}

/*ARGSUSED*/
void
ultra_setpic(uint64_t pic)
{}

uint64_t
ultra_getpic(void)
{ return (0); }

#else	/* lint || __lint */

	ENTRY(ultra_setpcr)
#if !defined(__sparcv9)
	sllx	%o0, 32, %o0	! upper 32-bits in lower 32-bits of %o0
	srl	%o1, 0, %o1	! clean lower 32-bits in %o1
	or	%o0, %o1, %o0
#endif
	retl
	wr	%o0, %pcr
	SET_SIZE(ultra_setpcr)

	ENTRY(ultra_setpic)
#if !defined(__sparcv9)
	sllx	%o0, 32, %o0	! upper 32-bits in lower 32-bits of %o0
	srl	%o1, 0, %o1	! clean lower 32-bits in %o1
	or	%o0, %o1, %o0
#endif
	retl
	wr	%o0, %pic
	SET_SIZE(ultra_setpic)

	ENTRY(ultra_getpic)
#if defined(__sparcv9)
	retl
	rd	%pic, %o0
#else
	rd	%pic, %o1
	srlx	%o1, 32, %o0	! put upper 32-bits in lower 32-bits of %o0
	retl
	srl	%o1, 0, %o1	! clean lower 32-bits in %o1
#endif
	SET_SIZE(ultra_getpic)

#endif	/* lint || __lint */
