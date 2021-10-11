/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)rmfiles.c	1.3	92/07/14 SMI"       /* SVr4.0 1.6 */

#include <sys/types.h>
#include <stdio.h>
#include <userdefs.h>
#include <errno.h>
#include "messages.h"

#define 	SBUFSZ	256

extern void errmsg();
extern int rmdir();
extern char *prerrno();

static char sptr[SBUFSZ];	/* buffer for system call */

int
rm_files(homedir, user)
char *homedir;			/* home directory to remove */
char *user;
{
	/* delete all files belonging to owner */
	(void) sprintf( sptr,"rm -rf %s", homedir );
	if( system(sptr) != 0 ) {
		errmsg( M_RMFILES );
		return( EX_HOMEDIR );
	}

	return( EX_SUCCESS );
}

