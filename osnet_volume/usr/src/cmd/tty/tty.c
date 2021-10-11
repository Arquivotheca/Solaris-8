/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tty.c	1.8	94/09/16 SMI"	/* SVr4.0 1.3	*/

/*
** Type tty name
*/

#include	<locale.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/stermio.h>

char	*ttyname();

main(argc, argv)
char **argv;
{
	register char	*p;
	register int	i;
	int		lflg	= 0;
	int		sflg	= 0;


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);
	while ((i = getopt(argc, argv, "ls")) != EOF)
		switch (i) {
		case 'l':
			lflg = 1;
			break;
		case 's':
			sflg = 1;
			break;
		case '?':
			(void) printf(gettext("Usage: tty [-l] [-s]\n"));
			return (2);
		}
	p = ttyname(0);
	if (!sflg)
		(void) printf("%s\n", (p? p: gettext("not a tty")));
	if (lflg) {
		if ((i = ioctl(0, STWLINE, 0)) == -1)
			(void) printf(gettext(
			    "not on an active synchronous line\n"));
		else
			(void) printf(gettext("synchronous line %d\n"), i);
	}
	return (p? 0: 1);
}
