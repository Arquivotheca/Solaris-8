/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)_psignal.c	1.3	98/05/10 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

/*
 * Print the name of the signal indicated
 * along with the supplied message.
 */
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "libc.h"

void
_psignal(unsigned int sig, char *s)
{
	char *c;
	size_t n;

	c = "Unknown signal";
	if (sig < NSIG)
		c = sys_siglist[sig];
	n = strlen(s);
	if (n) {
		(void) write(2, s, n);
		(void) write(2, ": ", (size_t)2);
	}
	(void) write(2, c, strlen(c));
	(void) write(2, "\n", (size_t)1);
}
