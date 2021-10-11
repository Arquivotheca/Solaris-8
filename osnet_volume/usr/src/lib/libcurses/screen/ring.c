/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)ring.c	1.9	97/08/22 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int
_ring(bool bf)
{
	static	char	offsets[2] = {45 /* flash_screen */, 1 /* bell */ };
	char	**str_array = (char **) cur_strs;
#ifdef	DEBUG
	if (outf)
		fprintf(outf, "_ring().\n");
#endif	/* DEBUG */
	_PUTS(str_array[offsets[bf]] ? str_array[offsets[bf]] :
	    str_array[offsets[1 - bf]], 0);
	(void) fflush(SP->term_file);
	if (_INPUTPENDING)
		(void) doupdate();
	return (OK);
}
