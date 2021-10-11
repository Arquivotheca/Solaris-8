/*
 * Copyright (c) 1990, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)gconvert.c	1.2	92/10/19 SMI"

/*
 * gcvt  - Floating output conversion to minimal length string
 */

#ifdef __STDC__  
	#pragma weak gconvert = _gconvert 
#endif

#include "synonyms.h"
#include "base_conversion.h"

extern enum fp_direction_type __xgetRD(); 

char           *
gconvert(number, ndigits, trailing, buf)
	double          number;
	int             ndigits, trailing;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type fef;

	dm.rd = __xgetRD();
	dm.df = floating_form;
	dm.ndigits = ndigits;
	double_to_decimal(&number, &dm, &dr, &fef);
	__k_gconvert(ndigits, &dr, trailing, buf);
	return (buf);
}
