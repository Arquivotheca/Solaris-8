/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pwd.c	1.7	92/07/14 SMI"	/* SVr4.0 1.14	*/
/*
**	Print working (current) directory
*/

#include	<stdio.h>
#include	<unistd.h>
#include	<limits.h>
#include	<locale.h>
char	name[PATH_MAX+1];

main()
{
	int length;
	(void) setlocale(LC_ALL,"");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);
	if (getcwd(name, PATH_MAX + 1) == (char *)0) {
		fprintf(stderr, 
			gettext("pwd: cannot determine current directory!\n"));
		exit(2);
	}
	length = strlen(name);
	name[length] = '\n';
	write(1, name, length + 1);
	exit(0);
}
