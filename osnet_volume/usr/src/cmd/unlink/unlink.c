/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)unlink.c	1.7	98/03/11 SMI"	/* SVr4.0 1.4	*/
#include <locale.h>

main(argc, argv) char *argv[]; {

	char *p;
	int res = 0;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	p = gettext("Usage: /usr/sbin/unlink name\n");

	if (argc != 2) {
		write(2, p, strlen(p));
		exit(1);
	}

	if (res = unlink(argv[1]))
		perror("unlink");

	exit(res);
}
