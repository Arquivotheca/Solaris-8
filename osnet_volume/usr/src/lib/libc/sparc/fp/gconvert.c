/*
 *	Copyright (c) 1990, 1991, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)gconvert.c	1.5	96/12/04 SMI"

/*
 * gcvt  - Floating output conversion to minimal length string
 */

/*LINTLIBRARY*/

#pragma weak gconvert = _gconvert

#include "synonyms.h"
#include "base_conversion.h"
#include <sys/types.h>


char	*
gconvert(double number, int ndigits, int trailing, char *buf)
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type fef;

	dm.rd = _QgetRD();
	dm.df = floating_form;
	dm.ndigits = ndigits;
	double_to_decimal(&number, &dm, &dr, &fef);
	__k_gconvert(ndigits, &dr, trailing, buf);
	return (buf);
}
