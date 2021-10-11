/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)syntax.c	1.5	93/11/02 SMI"	/* SVr4.0 1.5	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "ctype.h"
#include "string.h"

#include "lp.h"

int
#if	defined(__STDC__)
syn_name (
	char *			str
)
#else
syn_name (str)
	char			*str;
#endif
{
	register char		*p;

	if (!str || !*str)
	  	return(0);

	if (strlen(str) > (size_t) 14)
		return (0);

	for (p = str; *p; p++)
		if (!isalnum(*p) && *p != '_' && *p != '-')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_type (
	char *			str
)
#else
syn_type (str)
	char			*str;
#endif
{
	register char		*p;

	if (!str)
		return(0);

	if (strlen(str) > (size_t) 14)
		return (0);

	for (p = str; *p; p++)
		if (!isalnum(*p) && *p != '-')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_text (
	char *			str
)
#else
syn_text (str)
	char			*str;
#endif
{
	register char		*p;

	if (!str)
		return(0);

	for (p = str; *p; p++)
		if (!isgraph(*p) && *p != '\t' && *p != ' ')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_comment (
	char *			str
)
#else
syn_comment (str)
	char			*str;
#endif
{
	register char		*p;

	if (!str)
		return(0);

	for (p = str; *p; p++)
		if (!isgraph(*p) && *p != '\t' && *p != ' ' && *p != '\n')
			return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_machine_name (
	char *			str
)
#else
syn_machine_name (str)
	char			*str;
#endif
{
	if (!str)
		return(0);

	if (strlen(str) > (size_t) 8)
		return (0);

	return (1);
}

int
#if	defined(__STDC__)
syn_option (
	char *			str
)
#else
syn_option (str)
	char			*str;
#endif
{
	register char		*p;

	if (!str)
		return(0);

	for (p = str; *p; p++)
		if (!isprint(*p))
			return (0);

	return (1);
}
