/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)stub.c	1.5	93/03/09 SMI"	/* SVr4.0 1.2.1.2	*/
#ifdef PRESVR4
int
rename(char *x, char *y)
{
	return (link(x, y) || unlink(x));
}
#else
static int dummy;  /* used to make compillor warning go away */
#ifdef lint
_______a()
{
	return (dummy++);
}
#endif	/* lint */
#endif
