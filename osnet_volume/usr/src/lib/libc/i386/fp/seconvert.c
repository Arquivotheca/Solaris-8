/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ident  "@(#)seconvert.c	1.2	92/10/19 SMI"

#ifdef __STDC__  
	#pragma weak seconvert = _seconvert 
	#pragma weak sfconvert = _sfconvert 
	#pragma weak sgconvert = _sgconvert 
#endif

#include "synonyms.h"
#include <memory.h>
#include "base_conversion.h"

extern enum fp_direction_type __xgetRD(); 

char           *
seconvert(arg, ndigits, decpt, sign, buf)
	single         *arg;
	int             ndigits, *decpt, *sign;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type ef;

	dm.rd = __xgetRD();	/* Rounding direction. */
	dm.df = floating_form;	/* E format. */
	dm.ndigits = ndigits;	/* Number of significant digits. */
	single_to_decimal(arg, &dm, &dr, &ef);
	*sign = dr.sign;
	switch (dr.fpclass) {
	case fp_normal:
	case fp_subnormal:
		*decpt = dr.exponent + ndigits;
		(void) memcpy(&(buf[0]), &(dr.ds[0]), ndigits);
		buf[ndigits] = 0;
		break;
	case fp_zero:
		*decpt = 1;
		(void) memset(&(buf[0]), '0', ndigits);
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
sfconvert(arg, ndigits, decpt, sign, buf)
	single         *arg;
	int             ndigits, *decpt, *sign;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type ef;

	dm.rd = __xgetRD();	/* Rounding direction. */
	dm.df = fixed_form;	/* F format. */
	dm.ndigits = ndigits;	/* Number of digits after point. */
	single_to_decimal(arg, &dm, &dr, &ef);
	*sign = dr.sign;
	switch (dr.fpclass) {
	case fp_normal:
	case fp_subnormal:
		if (ndigits >= 0)
			*decpt = dr.ndigits - ndigits;
		else
			*decpt = dr.ndigits;
		(void) memcpy(&(buf[0]), &(dr.ds[0]), dr.ndigits);
		buf[dr.ndigits] = 0;
		break;
	case fp_zero:
		*decpt = 0;
		buf[0] = '0';
		if (ndigits > 1) {
			(void) memset(&(buf[0]), '0', ndigits);
			buf[ndigits] = 0;
		} else {
			buf[0] = '0';
			buf[1] = 0;
		}
		break;
	default:
		*decpt = 0;
		__infnanstring(dr.fpclass, ndigits, buf);
		break;
	}
	return buf;		/* For compatibility with fcvt. */
}

char           *
sgconvert(number, ndigit, trailing, buf)
	single         *number;
	int             ndigit, trailing;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type fef;

	dm.rd = __xgetRD();
	dm.df = floating_form;
	dm.ndigits = ndigit;
	single_to_decimal(number, &dm, &dr, &fef);
	__k_gconvert(ndigit, &dr, trailing, buf);
	return (buf);
}
