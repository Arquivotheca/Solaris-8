/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pkgname.c	1.9	93/05/28 SMI"	/* SVr4.0  1.3.2.2	*/
#include <locale.h>
#include <stdlib.h>
#include "libadm.h"

main(int argc, char *argv[])
{
	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while (--argc > 0) {
		if (pkgnmchk(argv[argc], (char *)0, 1))
			exit(1);
	}
	exit(0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}
