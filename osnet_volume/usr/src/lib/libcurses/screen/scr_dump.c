/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)scr_dump.c	1.7	97/06/25 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdio.h>
#include	"curses_inc.h"

/*
 * Dump a screen image to a file. This routine and scr_reset
 * can be used to communicate the screen image across processes.
 */

int
scr_dump(char *file)
{
	int		rv;
	FILE	*filep;

	if ((filep = fopen(file, "w")) == NULL) {
#ifdef	DEBUG
		if (outf)
			(void) fprintf(outf, "scr_dump: cannot open "
			    "\"%s\".\n", file);
#endif	/* DEBUG */
		return (ERR);
	}
	rv = scr_ll_dump(filep);
	(void) fclose(filep);
	return (rv);
}
