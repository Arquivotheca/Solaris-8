/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lenlist.c	1.3	90/03/01 SMI"	/* SVr4.0 1.4	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

/**
 ** lenlist() - COMPUTE LENGTH OF LIST
 **/

int
#if	defined(__STDC__)
lenlist (
	char **			list
)
#else
lenlist (list)
	char			**list;
#endif
{
	register char **	pl;

	if (!list)
		return (0);
	for (pl = list; *pl; pl++)
		;
	return (pl - list);
}
