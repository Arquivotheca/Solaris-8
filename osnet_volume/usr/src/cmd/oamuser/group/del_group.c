/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)del_group.c	1.6	95/04/06 SMI"       /* SVr4.0 1.6 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <grp.h>
#include <unistd.h>
#include <userdefs.h>
#include <errno.h>
#include "users.h"

struct group *fgetgrent();
void putgrent();

extern pid_t getpid();
extern int strcmp(), unlink(), rename();

/* Delete a group from the GROUP file */
int
del_group( group )
char *group;
{
	register deleted;
	FILE *e_fptr, *t_fptr;
	struct group *grpstruct;
	char *tname;
	struct stat sbuf;

	if( (e_fptr = fopen(GROUP, "r")) == NULL )
		return( EX_UPDATE );

	if (fstat(fileno(e_fptr), &sbuf) != 0)
		return( EX_UPDATE );

	if ((tname = tempnam("/etc", "gtmp.")) == NULL) {
		return( EX_UPDATE );
	}
	
	 if( (t_fptr = fopen( tname, "w+" ) ) == NULL )
                return( EX_UPDATE );
	
	/*
	 * Get ownership and permissions correct
	 */

	if (fchmod(fileno(t_fptr), sbuf.st_mode) != 0 ||
	    fchown(fileno(t_fptr), sbuf.st_uid, sbuf.st_gid) != 0) {
		(void)unlink(tname);
		return( EX_UPDATE );
	}

	errno = 0;

	/* loop thru GROUP looking for the one to delete */
	for( deleted = 0; (grpstruct = fgetgrent( e_fptr )); ) {
		
		/* check to see if group is one to delete */
		if( !strcmp( grpstruct->gr_name, group ) )
			deleted = 1;

		else putgrent( grpstruct, t_fptr );

	}

	(void) fclose( e_fptr );
	(void) fclose( t_fptr );

	if( errno == EINVAL ) {
		/* GROUP file contains bad entries */
		(void) unlink( tname );
		return( EX_UPDATE );
	}
	
	/* If deleted, update GROUP file */
	if( deleted && rename( tname, GROUP ) < 0 ) {
		(void) unlink( tname );
		return( EX_UPDATE );
	}

	(void) unlink( tname );

	return( deleted? EX_SUCCESS: EX_NAME_NOT_EXIST );
}
