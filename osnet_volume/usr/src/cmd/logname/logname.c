/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)logname.c	1.7	94/09/09 SMI"	/* SVr4.0 1.4	*/

#include <unistd.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>

main()
{
	char *name;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"		/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if ((name = getlogin()) == NULL)
		return (1);
	(void) puts(name);
	return (0);
}
