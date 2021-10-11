/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__flt_rounds.c	1.5	96/12/06 SMI"

/*LINTLIBRARY*/

/*
 * __flt_rounds() returns the prevailing rounding mode per ANSI C spec:
 *	 0:	toward zero
 *	 1:	to nearest			<<< default
 *	 2:	toward positive infinity
 *	 3:	toward negative infinity
 *	-1:	indeterminable			<<< never returned
 */

#include "synonyms.h"
#include <sys/types.h>
#include <floatingpoint.h>
#include "base_conversion.h"

int
__flt_rounds(void)
{
	int ansi_rd;

	switch (_QgetRD()) {
	case fp_tozero:		ansi_rd = 0; break;
	case fp_positive:	ansi_rd = 2; break;
	case fp_negative:	ansi_rd = 3; break;
	case fp_nearest:	/* FALLTHRU */
	default:		ansi_rd = 1; break;
	}
	return (ansi_rd);
}
