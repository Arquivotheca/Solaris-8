/*
 *	domainname.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)domainname.c	1.8	94/04/10 SMI"

/*
 * domainname -- get (or set domainname)
 */
#include <stdio.h>

char domainname[256];
extern int errno;

main(argc, argv)
	int argc;
	char *argv[];
{
	int myerrno;

	argc--;
	argv++;
	if (argc) {
		if (setdomainname(*argv, strlen(*argv)))
			perror("setdomainname");
		myerrno = errno;
	} else {
		getdomainname(domainname, sizeof (domainname));
		myerrno = errno;
		printf("%s\n", domainname);
	}
	exit(myerrno);
	/* NOTREACHED */
}
