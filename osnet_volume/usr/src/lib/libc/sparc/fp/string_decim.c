/*
 *	Copyright (c) 1988 - 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)string_decim.c	1.6	96/12/04 SMI"

/*LINTLIBRARY*/

#pragma weak string_to_decimal = _string_to_decimal

#include "synonyms.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include "base_conversion.h"
#include <locale.h>

void
string_to_decimal(char **ppc, int nmax, int fortran_conventions,
    decimal_record *pd, enum decimal_string_form *pform,
		char **pechar)
{
	char  *cp = *ppc;
	char    current;
	int    nread = 1;	/* Number of characters read so far. */
	char	*cp0 = cp;
	char	*good = cp - 1;	/* End of known good token. */
	current = *cp;

#define	ATEOF 0			/* A string is never at EOF.	 */
#define	CURRENT current
#define	ISSPACE isspace
#define	NEXT \
	if (nread < nmax) \
		{cp++; current = *cp; nread++; } \
	else \
		{current = NULL; };	/* Increment input character and cp. */

#include "char_to_decimal.h"
#undef CURRENT
#undef NEXT
}
