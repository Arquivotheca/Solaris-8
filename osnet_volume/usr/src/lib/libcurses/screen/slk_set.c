/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)slk_set.c	1.8	97/08/15 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<string.h>
#include	<unistd.h>
#include	"curses_inc.h"

/*
 * Set a soft label.
 *
 * n:	label number
 * lab:	the string
 * f:	0, 1, 2 for left, center, right-justification
 */

int
slk_set(int n, char *lab, int f)
{
	SLK_MAP	*slk = SP->slk;
	int	len, slklen = slk->_len, left;
	char		*cp, nlab[LABLEN + 1];

	if ((slk == NULL) || f < 0 || f > 2 || n < 1 || n > slk->_num)
		return (ERR);

	/* 0-indexing internally */
	n--;

	if (lab == NULL) {
		lab = "";

	} else {
		/* chop lengthy label */
		/* LINTED */
		if ((len = (int) strlen(lab)) > slklen)
			lab[len = slklen] = '\0';
	}

	/* make the new display label */
	for (cp = nlab + slklen - 1; cp >= nlab; cp--)
		*cp = ' ';
	nlab[slklen] = '\0';
	if (f == 0)
		left = 0;
	else
		left = (slklen - len) / ((f == 1) ? 2 : 1);

	(void) memcpy(nlab + left, lab, len);

	if (strcmp(slk->_ldis[n], nlab) != 0) {
		(void) memcpy(slk->_lval[n], lab, len + 1);
		(void) memcpy(slk->_ldis[n], nlab, slklen + 1);
		slk->_changed = slk->_lch[n] = TRUE;
	}

	return (OK);
}
