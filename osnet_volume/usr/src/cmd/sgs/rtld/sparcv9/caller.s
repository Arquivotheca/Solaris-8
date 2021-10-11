/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Return the pc of the calling routine.
 */
#pragma ident	"@(#)caller.s	1.5	98/07/10 SMI"

#if	defined(lint)

unsigned long
caller()
{
	return (0);
}

/* ARGSUSED */
void
set_sparc_g1(unsigned long val)
{
	return;
}

/* ARGSUSED */
void
set_sparc_g2(unsigned long val)
{
	return;
}

/* ARGSUSED */
void
set_sparc_g3(unsigned long val)
{
	return;
}

/* ARGSUSED */
void
set_sparc_g4(unsigned long val)
{
	return;
}

/* ARGSUSED */
void
set_sparc_g5(unsigned long val)
{
	return;
}

/* ARGSUSED */
void
set_sparc_g6(unsigned long val)
{
	return;
}

/* ARGSUSED */
void
set_sparc_g7(unsigned long val)
{
	return;
}
	
#else

#include	<sys/asm_linkage.h>

	.file	"caller.s"

	ENTRY(caller)
	retl
	  mov	%i7, %o0
	SET_SIZE(caller)

	ENTRY(set_sparc_g1)
	retl
	  mov	%o0, %g1
	SET_SIZE(set_sparc_g1)

	ENTRY(set_sparc_g2)
	retl
	  mov	%o0, %g2
	SET_SIZE(set_sparc_g2)

	ENTRY(set_sparc_g3)
	retl
	  mov	%o0, %g3
	SET_SIZE(set_sparc_g3)

	ENTRY(set_sparc_g4)
	retl
	  mov	%o0, %g4
	SET_SIZE(set_sparc_g4)

	ENTRY(set_sparc_g5)
	retl
	  mov	%o0, %g5
	SET_SIZE(set_sparc_g5)
	
	ENTRY(set_sparc_g6)
	retl
	  mov	%o0, %g6
	SET_SIZE(set_sparc_g6)

	ENTRY(set_sparc_g7)
	retl
	  mov	%o0, %g7
	SET_SIZE(set_sparc_g7)
#endif
