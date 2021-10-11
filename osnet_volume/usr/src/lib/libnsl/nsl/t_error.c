/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_error.c	1.15	97/08/12 SMI"	/* SVr4.0 1.2 */

#include <xti.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <rpc/trace.h>
#include "timt.h"

/* ARGSUSED1 */
int
_tx_error(const char *s, int api_semantics)
{
	const char *c;
	int n;

	trace1(TR_t_error, 0);
	c = t_strerror(t_errno);
	if (s != (char *)NULL &&
	    *s != '\0') {
		n = strlen(s);
		if (n) {
			(void) write(2, s, (unsigned)n);
			(void) write(2, ": ", 2);
		}
	}
	(void) write(2, c, (unsigned)strlen(c));
	if (t_errno == TSYSERR) {
		(void) write(2, ": ", 2);
		perror("");
	} else
		(void) write(2, "\n", 1);
	trace1(TR_t_error, 1);
	return (0);
}
