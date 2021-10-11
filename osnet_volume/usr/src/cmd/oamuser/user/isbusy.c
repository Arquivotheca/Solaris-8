/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)isbusy.c	1.5	98/12/02 SMI"	/* SVr4.0 1.3 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <utmpx.h>

#ifndef TRUE
#define	TRUE 1
#define	FALSE 0
#endif

int isbusy(char *);

/* Is this login being used */
int
isbusy(char *login)
{
	struct utmpx *utxptr;

	setutxent();
	while ((utxptr = getutxent()) != NULL)
		/*
		 * If login is in the utmp file, and that process
		 * isn't dead, then it "is_busy()"
		 */
		if ((strncmp(login, utxptr->ut_user,
		    sizeof (utxptr->ut_user)) == 0) && \
			utxptr->ut_type != DEAD_PROCESS)
			return (TRUE);

	return (FALSE);
}
