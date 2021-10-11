/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)logdir.c	1.1	99/01/11 SMI"

/*
 *	logdir()
 *
 *	This routine does not use the getpwent(3) library routine
 *	because the latter uses the stdio package.  The allocation of
 *	storage in this package destroys the integrity of the shell's
 *	storage allocation.
 */

#include <fcntl.h>	/* O_RDONLY */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define	BUFSIZ	160

static char line[BUFSIZ+1];

static char *
passwdfield(char *p)
{
	while (*p && *p != ':')
		++p;
	if (*p)
		*p++ = 0;
	return (p);
}

char *
logdir(char *name)
{
	char	*p;
	int	i, j;
	int	pwf;

	/* attempt to open the password file */
	if ((pwf = open("/etc/passwd", O_RDONLY)) == -1)
		return (0);

	/* find the matching password entry */
	do {
		/* get the next line in the password file */
		i = read(pwf, line, BUFSIZ);
		for (j = 0; j < i; j++)
			if (line[j] == '\n')
				break;
		/* return a null pointer if the whole file has been read */
		if (j >= i)
			return (0);
		line[++j] = 0;			/* terminate the line */
		/* point at the next line */
		(void) lseek(pwf, (long)(j - i), 1);
		p = passwdfield(line);		/* get the logname */
	} while (*name != *line ||	/* fast pretest */
	    strcmp(name, line) != 0);
	(void) close(pwf);

	/* skip the intervening fields */
	p = passwdfield(p);
	p = passwdfield(p);
	p = passwdfield(p);
	p = passwdfield(p);

	/* return the login directory */
	(void) passwdfield(p);
	return (p);
}
