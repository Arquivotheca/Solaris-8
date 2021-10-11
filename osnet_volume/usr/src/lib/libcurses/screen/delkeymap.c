/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)delkeymap.c	1.7	97/06/25 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdlib.h>
#include	"curses_inc.h"

/*
 *	Delete a key table
 */

void
delkeymap(TERMINAL *terminal)
{
	_KEY_MAP	**kpp, *kp;
	int		numkeys = terminal->_ksz;

	/* free key slots */
	for (kpp = terminal->_keys; numkeys-- > 0; kpp++) {
		kp = *kpp;
		if (kp->_sends == ((char *) (kp + sizeof (_KEY_MAP))))
			free(kp);
	}

	if (terminal->_keys != NULL) {
		free(terminal->_keys);
		if (terminal->internal_keys != NULL)
			free(terminal->internal_keys);
	}
	_blast_keys(terminal);
}
