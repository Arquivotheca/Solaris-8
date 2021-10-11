/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mod_group.c	1.5	92/07/14 SMI"       /* SVr4.0 1.6 */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <userdefs.h>
#include <errno.h>
#include "users.h"

struct group *fgetgrent();
void putgrent();

extern pid_t getpid();
extern int strcmp(), unlink(), rename();

/* Modify group to new gid and/or new name */
int
mod_group( group, gid, newgroup )
char *group;
gid_t gid;
char *newgroup;
{
	register modified = 0;
	char *tname;
	FILE *e_fptr, *t_fptr;
	struct group *g_ptr;
	struct stat sbuf;

	if( (e_fptr = fopen(GROUP, "r")) == NULL )
		return( EX_UPDATE );

	if (fstat(fileno(e_fptr), &sbuf) != 0)
		return( EX_UPDATE );

	if ((tname = tempnam("/etc", "gtmp.")) == NULL) {
		return( EX_UPDATE );
	}

	if( (t_fptr = fopen( tname, "w+")) == NULL )
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
	while( (g_ptr = fgetgrent( e_fptr )) != NULL ) {
		
		/* check to see if group is one to modify */
		if( !strcmp( g_ptr->gr_name, group ) ) {
			if( newgroup != NULL ) g_ptr->gr_name = newgroup;
			if( gid != -1 ) g_ptr->gr_gid = gid;
			modified++;
		}
		putgrent( g_ptr, t_fptr );

	}

	(void) fclose( e_fptr );
	(void) fclose( t_fptr );

	if( errno == EINVAL ) {
		/* GROUP file contains bad entries */
		(void) unlink( tname );
		return( EX_UPDATE );
	}
	if( modified && rename( tname, GROUP ) < 0 ) {
		(void) unlink( tname );
		return( EX_UPDATE );
	}

	(void) unlink( tname );

	if (modified)
		return( EX_SUCCESS );
	else
		return( EX_NAME_NOT_EXIST );
}
