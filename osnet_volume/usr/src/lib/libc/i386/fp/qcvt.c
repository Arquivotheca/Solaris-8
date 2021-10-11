#if !defined(lint) && defined(SCCSIDS)
static char     sccsid[] = "@(#)qcvt.c	1.1	92/04/17 SMI";
#endif

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#include <floatingpoint.h>
#include "base_conversion.h"

static char buf[DECIMAL_STRING_LENGTH]; /* defined in floatingpoint.h */

char           *
qecvt(number, ndigits, decpt, sign)
	long double	number;
	int             ndigits;
	int		*decpt;
	int		*sign;
{
	return (qeconvert(&number, ndigits, decpt, sign, buf));
}

char           *
qfcvt(number, ndigits, decpt, sign)
	long double	number;
	int             ndigits;
	int		*decpt;
	int		*sign;
{
	return (qfconvert(&number, ndigits, decpt, sign, buf));
}

char           *
qgcvt(number, ndigits, buffer)
	long double	number;
	int             ndigits;
	char		*buffer;
{
	return (qgconvert(&number, ndigits, 0, buffer));
}
