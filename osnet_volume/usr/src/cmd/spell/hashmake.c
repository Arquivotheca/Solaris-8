/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)hashmake.c	1.9	95/03/16 SMI"	/* SVr4.0 1.2	*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <locale.h>
#include <limits.h>

#include "hash.h"

/* ARGSUSED */
void
main(int argc, char **argv)
{
	char word[LINE_MAX];

	/* Set locale environment variables local definitions */
	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	hashinit();
	while (gets(word)) {
		(void) printf("%.*lo\n", (HASHWIDTH+2)/3, hash(word));
	}
	exit(0);
}
