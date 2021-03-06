/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * 	Copyright (c) 1996, by Sun Microsystems, Inc.
 * 	All rights reserved.
 */

#pragma	ident	"@(#)ecvt.c	1.10	96/12/20 SMI"

/*LINTLIBRARY*/
/*
 *	ecvt converts to decimal
 *	the number of digits is specified by ndigit
 *	decpt is set to the position of the decimal point
 *	sign is set to 0 for positive, 1 for negative
 *
 */
#ifndef DSHLIB
#pragma weak ecvt = _ecvt
#pragma weak fcvt = _fcvt
#pragma weak qecvt = _qecvt
#pragma weak qfcvt = _qfcvt
#pragma weak qgcvt = _qgcvt
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>
#include <floatingpoint.h>


static char *buf = NULL;

char *
ecvt(double number, int ndigits, int *decpt, int *sign)
{

	if (buf == 0)
		buf = malloc((DECIMAL_STRING_LENGTH));

	return (econvert(number, ndigits, decpt, sign, buf));
}

char *
fcvt(double number, int ndigits, int *decpt, int *sign)
{
	char *ptr, *val;
	char ch;
	int deci_val;

	if (buf == 0)
		buf = malloc(DECIMAL_STRING_LENGTH);

	ptr = fconvert(number, ndigits, decpt, sign, buf);

	val = ptr;
	deci_val = *decpt;

	while ((ch = *ptr) != 0) {
		if (ch != '0') { /* You execute this if there are no */
				    /* leading zero's remaining. */
			*decpt = deci_val; /* If there are leading zero's */
			return (ptr);		/* gets updated. */
		}
		ptr++;
		deci_val--;
	}
	return (val);
}

char *
qecvt(number, ndigits, decpt, sign)
	long double	number;
	int		ndigits;
	int		*decpt;
	int		*sign;
{

	if (buf == 0)
		buf = malloc(DECIMAL_STRING_LENGTH);

	return (qeconvert(&number, ndigits, decpt, sign, buf));
}

char *
qfcvt(long double number, int ndigits, int *decpt, int *sign)
{

	if (buf == 0)
		buf = malloc(DECIMAL_STRING_LENGTH);

	return (qfconvert(&number, ndigits, decpt, sign, buf));
}

char *
qgcvt(long double number, int ndigits, char *buffer)
{
	return (qgconvert(&number, ndigits, 0, buffer));
}
