/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)qcvt.c	1.4	96/12/06 SMI"

/* LINTLIBRARY */

#include <floatingpoint.h>

static char buf[DECIMAL_STRING_LENGTH]; /* defined in floatingpoint.h */

char *
qecvt(long double number, int ndigits, int *decpt, int *sign)
{
	return (qeconvert(&number, ndigits, decpt, sign, buf));
}

char *
qfcvt(long double number, int ndigits, int *decpt, int *sign)
{
	return (qfconvert(&number, ndigits, decpt, sign, buf));
}

char *
qgcvt(long double number, int ndigits, char *buffer)
{
	return (qgconvert(&number, ndigits, 0, buffer));
}
