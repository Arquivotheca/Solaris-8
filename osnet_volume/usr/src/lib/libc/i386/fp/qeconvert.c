#if !defined(lint) && defined(SCCSIDS)
static char     sccsid[] = "@(#)qeconvert.c 1.4 93/12/23 SMI";
#endif

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifdef __STDC__  
	#pragma weak qeconvert = _qeconvert 
	#pragma weak qfconvert = _qfconvert 
	#pragma weak qgconvert = _qgconvert 
#endif

#include "synonyms.h"
#include "base_conversion.h"

extern enum fp_direction_type __xgetRD(); 

char           *
qeconvert(arg, ndigits, decpt, sign, buf)
	quadruple      *arg;
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
#ifndef	i386
	quadruple_to_decimal(arg, &dm, &dr, &ef);
#else
	extended_to_decimal((extended *)arg, &dm, &dr, &ef);
#endif	/* i386 */
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
qfconvert(arg, ndigits, decpt, sign, buf)
	quadruple      *arg;
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
#ifndef	i386
	quadruple_to_decimal(arg, &dm, &dr, &ef);
#else
	extended_to_decimal((extended *)arg, &dm, &dr, &ef);
#endif	/* i386 */
	*sign = dr.sign;
	*decpt = 0;
	if ((ef & (1 << fp_overflow)) != 0) {	/* Overflowed decimal record. */
		buf[0] = 0;
		return buf;
	}
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
		buf[0] = '0';
		for (i = 1; i < ndigits; i++)
			buf[i] = '0';
		buf[i] = 0;
		break;
	default:
		__infnanstring(dr.fpclass, ndigits, buf);
		break;
	}
	return buf;		/* For compatibility with fcvt. */
}

char           *
qgconvert(number, ndigit, trailing, buf)
	quadruple      *number;
	int             ndigit, trailing;
	char           *buf;
{
	decimal_mode    dm;
	decimal_record  dr;
	fp_exception_field_type fef;

	dm.rd = __xgetRD();
	dm.df = floating_form;
	dm.ndigits = ndigit;
#ifndef	i386
	quadruple_to_decimal(number, &dm, &dr, &fef);
#else
	extended_to_decimal((extended *)number, &dm, &dr, &fef);
#endif	/* i386 */
	__k_gconvert(ndigit, &dr, trailing, buf);
	return (buf);
}
