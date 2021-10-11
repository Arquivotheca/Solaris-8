/*
 *	Copyright (c) 1989, 1991, 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)func_decim.c	1.8	96/12/04 SMI"

/*LINTLIBRARY*/
#pragma weak func_to_decimal = _func_to_decimal

#include "synonyms.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include "base_conversion.h"
#include <locale.h>

void
func_to_decimal(char **ppc, int nmax, int fortran_conventions,
		decimal_record *pd, enum decimal_string_form *pform,
		char **pechar, int (*pget)(void), int *pnread,
		int (*punget)(int))
{
	char  *cp = *ppc;
	int    current;
	int    nread = 1;	/* Number of characters read so far. */
	char	*good = cp - 1;	/* End of known good token. */
	char	*cp0 = cp;

	current = (*pget) ();	/* Initialize buffer. */
	*cp = (char) current;

#define	ATEOF	current
#define	CURRENT	current
#define	ISSPACE	isspace
#define	NEXT \
	if (nread < nmax) \
		{ cp++; current = (*pget)(); *cp = (char) current; nread++; } \
	else \
		{ current = NULL; };

#include "char_to_decimal.h"
#undef CURRENT
#undef NEXT

	if ((nread < nmax) && (punget != NULL)) {
		/* Push back as many excess characters as possible. */
		while (cp >= *ppc) {
			/* Can't push back EOF. */
			if (*cp != EOF) {
				if ((*punget) (*cp) == EOF)
					break;
			}
			cp--;
			nread--;
		}
	}
	cp++;
	*cp = 0;		/* Terminating null. */
	*pnread = nread;
}
