
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#pragma	ident	"@(#)puthelp.c	1.6	97/07/22 SMI"	/* SVr4.0 1.1 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "libadm.h"

void
puthelp(FILE *fp, char *defmesg, char *help)
{
	char	*tmp;
	size_t	n;

	tmp = NULL;
	if (help == NULL) {
		/* use default message since no help was provided */
		help = defmesg ? defmesg : "No help available.";
	} else if (defmesg != NULL) {
		n = strlen(help);
		if (help[0] == '~') {
			/* prepend default message */
			tmp = calloc(n+strlen(defmesg)+1, sizeof (char));
			(void) strcpy(tmp, defmesg);
			(void) strcat(tmp, "\n");
			++help;
			(void) strcat(tmp, help);
			help = tmp;
		} else if (n && (help[n-1] == '~')) {
			/* append default message */
			tmp = calloc(n+strlen(defmesg)+2, sizeof (char));
			(void) strcpy(tmp, help);
			tmp[n-1] = '\0';
			(void) strcat(tmp, "\n");
			(void) strcat(tmp, defmesg);
			help = tmp;
		}
	}
	(void) puttext(fp, help, ckindent, ckwidth);
	(void) fputc('\n', fp);
	if (tmp)
		free(tmp);
}
