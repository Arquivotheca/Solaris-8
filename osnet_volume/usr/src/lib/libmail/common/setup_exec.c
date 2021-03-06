/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setup_exec.c	1.7	99/03/09	SMI"		/* SVr4.0 1.8 */
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <ctype.h>
#include "libmail.h"

#define	TRUE	1
#define	FALSE	0

char **
setup_exec(char *s)
{
	char	*p = s, *q;
	static char	*argvec[256]; /* is this enough? */
	int	i = 0;
	int	stop;
	int	ignorespace = FALSE;

	/* Parse up string into arg. vec. for subsequent exec. Assume */
	/* whitespace delimits args. Any non-escaped double quotes will */
	/* be used to group multiple whitespace-delimited tokens into */
	/* a single exec arg. */
	p = skipspace(p);
	while (*p) {
		q = p;
		stop = FALSE;
		while (*q && (stop == FALSE)) {
		    again:
			switch (*q) {
			case '\\':
				/* Slide command string 1 char to left */
				strmove(q, q+1);
				break;
			case '"':
				ignorespace = ((ignorespace == TRUE) ?
								FALSE : TRUE);
				/* Slide command string 1 char to left */
				strmove(q, q+1);
				goto again;
			default:
				if (isspace((int)*q) &&
				    (ignorespace == FALSE)) {
					stop = TRUE;
					continue;
				}
				break;
			}
			q++;
		}
		if (*q == '\0') {
			argvec[i++] = p;
			break;
		}
		*q++ = '\0';
		argvec[i++] = p;
		p = skipspace(q);
	}
	argvec[i] = NULL;
	if (i == 0) {
		return (NULL);
	}
	return (argvec);
}
