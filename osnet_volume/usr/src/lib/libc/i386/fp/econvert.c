/*
 * Copyright (c) 1989, 1991, by Sun Microsystems, Inc.
 */

#ident  "@(#)econvert.c	1.2	92/10/19 SMI"

#ifdef __STDC__  
	#pragma weak econvert = _econvert 
	#pragma weak fconvert = _fconvert 
#endif

#include "synonyms.h"
#include "base_conversion.h"

extern enum fp_direction_type __xgetRD(); 

void 
__infnanstring(cl, ndigits, buf)
	enum fp_class_type cl;
	int             ndigits;
	char           *buf;
/*
 * Given a fp_class_type cl = inf or nan, copies the appropriate string into
 * *buf, choosing "Inf" or "Infinity" according to ndigits, the desired
 * output string length.
 */

{
	if (cl == fp_infinity) {
		if (ndigits < 8)
			(void) memcpy(buf, "Inf", 4);
		else
			(void) memcpy(buf, "Infinity", 9);
	__inf_written=1;
	} else {
		(void) memcpy(buf, "NaN", 4);
	__nan_written=1;
	}
}

char           *
econvert(arg, ndigits, decpt, sign, buf)
	double          arg;
	int             ndigits, *decpt, *sign;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type ef;
	int             i;

	dm.rd = __xgetRD();	/* Rounding direction. */
	dm.df = floating_form;	/* E format. */
	dm.ndigits = ndigits;	/* Number of significant digits. */
	double_to_decimal(&arg, &dm, &dr, &ef);
	*sign = dr.sign;
	switch (dr.fpclass) {
	case fp_normal:
	case fp_subnormal:
		*decpt = dr.exponent + ndigits;
		for (i = 0; i < ndigits; i++)
			buf[i] = dr.ds[i];
		buf[ndigits] = 0;
		break;
	case fp_zero:
		*decpt = 1;
		for (i = 0; i < ndigits; i++)
			buf[i] = '0';
		buf[ndigits] = 0;
		break;
	default:
		*decpt = 0;
		__infnanstring(dr.fpclass, ndigits, buf);
		break;
	}
	return buf;		/* For compatibility with ecvt. */
}

char           *
fconvert(arg, ndigits, decpt, sign, buf)
	double          arg;
	int             ndigits, *decpt, *sign;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type ef;
	int             i;

	dm.rd = __xgetRD();	/* Rounding direction. */
	dm.df = fixed_form;	/* F format. */
	dm.ndigits = ndigits;	/* Number of digits after point. */
	double_to_decimal(&arg, &dm, &dr, &ef);
	*sign = dr.sign;
	switch (dr.fpclass) {
	case fp_normal:
	case fp_subnormal:
		if (ndigits >= 0)
			*decpt = dr.ndigits - ndigits;
		else
			*decpt = dr.ndigits;
		for (i = 0; i < dr.ndigits; i++)
			buf[i] = dr.ds[i];
		buf[dr.ndigits] = 0;
		break;
	case fp_zero:
		*decpt = 0;
		buf[0] = '0';
		for (i = 1; i < ndigits; i++)
			buf[i] = '0';
		buf[i] = 0;
		break;
	default:
		*decpt = 0;
		__infnanstring(dr.fpclass, ndigits, buf);
		break;
	}
	return buf;		/* For compatibility with fcvt. */
}
