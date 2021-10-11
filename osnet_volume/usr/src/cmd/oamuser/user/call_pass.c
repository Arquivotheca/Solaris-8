/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)call_pass.c	1.4	92/07/14 SMI"       /* SVr4.0 1.3 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <userdefs.h>

extern int execvp();

int
call_passmgmt( nargv )
char *nargv[];
{
	int ret, cpid, wpid;

	switch( cpid = fork() ) {
	case 0:
		/* CHILD */
		if( freopen("/dev/null", "w+", stdout ) == NULL
			|| freopen("/dev/null", "w+", stderr ) == NULL
			|| execvp( nargv[0], nargv ) == -1 )
			exit( EX_FAILURE );

		break;

	case -1:
		/* ERROR */
		return( EX_FAILURE );

	default:
		/* PARENT */	
			
		while ( (wpid = wait(&ret)) != cpid) {
			if (wpid == -1)
				return( EX_FAILURE );
		}

		ret = ( ret >> 8 ) & 0xff;
	}
	return(ret);
		
}
