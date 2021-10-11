/*
 * Copyright (c) 1988, 1991, 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)file_decim.c	1.8	97/12/02 SMI"

/*LINTLIBRARY*/

#pragma weak file_to_decimal = _file_to_decimal

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include "base_conversion.h"
#include <locale.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"
#include "libc.h"

/* if the _IOWRT flag is set, this must be a call from sscanf */
#define	mygetc(iop)	((iop->_flag & _IOWRT) ? \
				((*iop->_ptr == '\0') ? EOF : *iop->_ptr++) : \
				GETC(iop))

#define	myungetc(x, iop) ((x == EOF) ? EOF : \
				((iop->_flag & _IOWRT) ? *(--iop->_ptr) : \
					UNGETC(x, iop)))


void
file_to_decimal(char **ppc, int nmax, int fortran_conventions,
		decimal_record *pd, enum decimal_string_form *pform,
		char **pechar, FILE *pf, int *pnread)

{
	char	*cp = *ppc;
	int	current;
	int	nread = 1;	/* Number of characters read so far. */
	char	*good = cp - 1;	/* End of known good token. */
	char	*cp0 = cp;

	current = mygetc(pf);	/* Initialize buffer. */
	*cp = (char)current;

#define	ATEOF	current
#define	CURRENT	current
#define	ISSPACE	isspace

/* if the _IOWRT flag is set, this must be a call from sscanf */
#define	NEXT \
	if (nread < nmax) \
		{ cp++; current = ((pf->_flag & _IOWRT) ? \
				((*pf->_ptr == '\0') ? EOF : *pf->_ptr++) : \
				GETC(pf)); \
				*cp = (char) current; nread++; } \
	else \
		{ current = NULL; };

#include "char_to_decimal.h"
#undef CURRENT
#undef NEXT

	if (nread < nmax) {
		while (cp >= *ppc) {
		/* Push back as many excess */
		/* characters as possible. */
			if (*cp != EOF) {	/* Can't push back EOF. */
				if (myungetc(*cp, pf) == EOF)
					break;
			} cp--;
			nread--;
		}
	}
	cp++;
	*cp = 0;		/* Terminating null. */
	*pnread = nread;

}
