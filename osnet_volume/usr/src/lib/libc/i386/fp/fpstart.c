/*
 *	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
 *	UNIX System Laboratories, Inc.
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)fpstart.c	1.6	98/10/01 SMI"

/*
 * Establish the default settings for the floating-point state for a C language
 * program:
 *	rounding mode		-- round to nearest default by OS,
 *	exceptions enabled	-- all masked
 *	sticky bits		-- all clear by default by OS.
 *      precision control       -- double extended
 * Set _fp_hw according to what floating-point hardware is available.
 * Set __flt_rounds according to the rounding mode.
 */
#include	<sys/sysi86.h>	/* for SI86FPHW definition	*/
#include	"synonyms.h"

extern int	sysi86();
extern int	__fltrounds();

long	_fp_hw;			/* default: bss: 0 == no hardware  */
int 	__flt_rounds;		/* ANSI rounding mode */

#pragma weak _fpstart = __fpstart
void
__fpstart()
{
	long	cw;	/* coprocessor control word */

	__flt_rounds = __fltrounds();	/* ANSI rounding is rnd-to-near */

	(void) sysi86(SI86FPHW, &_fp_hw);	/* query OS for HW status */
	_fp_hw &= 0xff;				/* mask off all but last byte */


	/*
	 * At this point the hardware environment (established by UNIX) is:
	 * round to nearest, all sticky bits clear, divide-by-zero, overflow
	 * and invalid op exceptions enabled.
	 * Precision control is set to double.
	 * Disable all exceptions and set precision control to double extended.
	 */
	(void) _getcw(&cw);
	cw |= 0x33f;
	(void) _putcw(cw);
}
