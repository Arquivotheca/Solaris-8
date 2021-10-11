/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)longname.c	1.8	97/06/25 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

/* This routine returns the long name of the terminal. */

#include <sys/types.h>
#include <string.h>

char *
longname(void)
{
	extern	char ttytype[];
	char	*cp = strrchr(ttytype, '|');

	if (cp)
		return (++cp);
	else
		return (ttytype);
}
