/*
 *	Copyright (c) 1989-1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)__f77_base.c	1.4	96/12/06 SMI"
/* Special Fortran versions that don't set exceptions.	 */

/*LINTLIBRARY*/
#include <sys/types.h>
#include "base_conversion.h"

void
__nox_single_to_decimal(single *px, decimal_mode *pm, decimal_record *pd,
		fp_exception_field_type *ps)
{
	single_equivalence kluge;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize *ps - no exceptions. */
	kluge.x = *px;
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_single(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		__double_to_decimal((double *) px, pm, pd, &ef);
	}
	*ps = ef;
}

void
__nox_double_to_decimal(double *px, decimal_mode *pm, decimal_record *pd,
			fp_exception_field_type *ps)
{
	double_equivalence kluge;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize *ps. */
	kluge.x = *px;
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_double(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		{
			__double_to_decimal(px, pm, pd, &ef);
		}
	}
	*ps = ef;
}

void
__nox_quadruple_to_decimal(quadruple *px, decimal_mode *pm, decimal_record *pd,
			fp_exception_field_type *ps)
{
	quadruple_equivalence kluge;
	fp_exception_field_type ef;

	ef = 0;			/* Initialize *ps. */
#ifdef i386
#define	QMSW 3
#else
#define	QMSW 0
#endif
#ifdef __STDC__
	kluge.x = *px;
#else
	kluge.x.u[QMSW] = px->u[QMSW];
#endif
	pd->sign = kluge.f.msw.sign;
	pd->fpclass = __class_quadruple(px);
	switch (pd->fpclass) {
	case fp_normal:
	case fp_subnormal:
		__quadruple_to_decimal(px, pm, pd, &ef);
	}
	*ps = ef;
}
