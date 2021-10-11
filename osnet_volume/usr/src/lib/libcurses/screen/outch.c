/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)outch.c	1.10	97/06/25 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

int	outchcount;

/* Write out one character to the tty and increment outchcount. */
int
_outch(char c)
{
	return (_outwch((chtype)c));
}

int
_outwch(chtype c)
{
	chtype	o;

#ifdef	DEBUG
#ifndef	LONGDEBUG
	if (outf)
		if (c < ' ' || c == 0177)
			fprintf(outf, "^%c", c^0100);
		else
			fprintf(outf, "%c", c&0177);
#else	/* LONGDEBUG */
	if (outf)
	    fprintf(outf, "_outch: char '%s' term %x file %x=%d\n",
		unctrl(c&0177), SP, cur_term->Filedes, fileno(SP->term_file));
#endif	/* LONGDEBUG */
#endif	/* DEBUG */

	outchcount++;

	/* ASCII code */
	if (!ISMBIT(c))
		(void) putc((int)c, SP->term_file);
	/* international code */
	else if ((o = RBYTE(c)) != MBIT) {
		(void) putc((int)o, SP->term_file);
		if (_csmax > 1 && (((o = LBYTE(c))|MBIT) != MBIT)) {
			SETMBIT(o);
			(void) putc((int)o, SP->term_file);
		}
	}
	return (0);
}
