/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)val_lgrp.c	1.4	97/05/09 SMI"       /* SVr4.0 1.3 */

#include	<sys/types.h>
#include	<stdio.h>
#include	<sys/param.h>
#include	<users.h>
#include	<userdefs.h>
#include	"messages.h"

extern int get_ngm();
extern void errmsg(), exit();
extern char *strtok();

static gid_t grplist[ NGROUPS_UMAX + 1 ];
static ngroups_max = 0;

/* Validate a list of groups */
int	**
valid_lgroup( list, gid )
char *list;
gid_t gid;
{
	register n_invalid = 0, i = 0, j;
	char *ptr;
	struct group *g_ptr;
	int warning;

	if( !list || !*list )
		return( (int **) NULL );

	while( ptr = strtok( ((i || n_invalid)? NULL: list), ",") ) {

		switch (valid_group(ptr, &g_ptr, &warning)) {
		case INVALID:
			errmsg( M_INVALID, ptr, "group id" );
			n_invalid++;
			break;
		case TOOBIG:
			errmsg( M_TOOBIG, "gid", ptr );
			n_invalid++;
			break;
		case UNIQUE:
			errmsg( M_GRP_NOTUSED, ptr );
			n_invalid++;
			break;
		case NOTUNIQUE:
			if( g_ptr->gr_gid == gid ) {
				errmsg( M_SAME_GRP, ptr );
				n_invalid++;
				continue;
			}

			if( !i )
				/* ignore respecified primary  */
				grplist[ i++ ] = g_ptr->gr_gid;
			else {
				/* Keep out duplicates */
				for( j = 0; j < i; j++ ) 
					if( g_ptr->gr_gid == grplist[j] )
						break;

				if( j == i )
					/* Not a duplicate */
					grplist[i++] = g_ptr->gr_gid;
			}
			break;
				
		}
		if (warning)
			warningmsg(warning, ptr);

		if( !ngroups_max )
			ngroups_max = get_ngm();


		if( i >= ngroups_max ) {
			errmsg( M_MAXGROUPS, ngroups_max );
			break;
		}
	}

	/* Terminate the list */
	grplist[ i ] = -1;

	if( n_invalid )
		exit( EX_BADARG );

	return( (int **)grplist );
}
