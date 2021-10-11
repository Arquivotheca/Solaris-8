/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ckyorn.c	1.7	99/01/06 SMI"	/* SVr4.0 1.1 */
/*LINTLIBRARY*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include "libadm.h"

static char *choices[] = { "y", "n", NULL };
static char *vchoices[] = { "y", "n", "yes", "no", NULL };

#define	TMPSIZ	7
#define	REQMSG	"Input is required."
#define	ERRMSG	"Please enter yes or no."
#define	HLPMSG	\
	"To respond in the affirmative, enter y, yes, Y, or YES. \
	To respond in the negative, enter n, no, N, or NO."

int
ckyorn_val(char *str)
{
	int	i;
	char	*pt,
		temp[TMPSIZ+1];

	(void) strncpy(temp, str, TMPSIZ);
	for (pt = temp; *pt; pt++) {
		if (isupper((unsigned char)*pt))
			*pt = tolower((unsigned char)*pt);
	}
	for (i = 0; vchoices[i]; ) {
		if (strcmp(temp, vchoices[i++]) == 0)
			return (0);
	}
	return (-1);
}

void
ckyorn_err(char *error)
{
	puterror(stdout, ERRMSG, error);
}

void
ckyorn_hlp(char *help)
{
	puthelp(stdout, HLPMSG, help);
}

int
ckyorn(char *yorn, char *defstr, char *error, char *help, char *prompt)
{
	int	n;
	char	input[MAX_INPUT];

	if (!prompt)
		prompt = "Yes or No";
start:
	putprmpt(stderr, prompt, choices, defstr);
	if (getinput(input))
		return (1);

	n = (int)strlen(input);
	if (n == 0) {
		if (defstr) {
			(void) strcpy(yorn, defstr);
			return (0);
		}
		puterror(stderr, REQMSG, error);
		goto start;
	}
	if (strcmp(input, "?") == 0) {
		puthelp(stderr, HLPMSG, help);
		goto start;
	}
	if (ckquit && (strcmp(input, "q") == 0))
		return (3);

	if (ckyorn_val(input)) {
		puterror(stderr, ERRMSG, error);
		goto start;
	}
	(void) strcpy(yorn, input);
	return (0);
}
