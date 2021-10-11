#if !defined(lint) && defined(SCCSIDS)
static char     sccsid[] = "@(#)string_decim.c 1.2 92/10/16 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ifdef __STDC__  
	#pragma weak string_to_decimal = _string_to_decimal 
#endif

#include "synonyms.h"
#include <ctype.h>
#include <stdio.h>
#include "base_conversion.h"
#ifndef PRE41
#include <locale.h>
#endif

void
string_to_decimal(ppc, nmax, fortran_conventions, pd, pform, pechar)
	char          **ppc;
	int             nmax;
	int             fortran_conventions;
	decimal_record *pd;
	enum decimal_string_form *pform;
	char          **pechar;

{
	register char  *cp = *ppc;
	register int    current;
	register int    nread = 1;	/* Number of characters read so far. */
	char           *cp0 = cp;
	char           *good = cp - 1;	/* End of known good token. */

	current = *cp;

#define ATEOF 0			/* A string is never at EOF.	 */
#define CURRENT current
#define NEXT \
	if (nread < nmax) \
		{cp++ ; current = *cp ; nread++ ;} \
	else \
		{current = NULL ; } ;	/* Increment input character and cp. */

#include "char_to_decimal.h"
#undef CURRENT
#undef NEXT
}
