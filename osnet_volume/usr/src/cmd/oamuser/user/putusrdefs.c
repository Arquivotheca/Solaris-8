/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)putusrdefs.c	1.5	99/04/07 SMI"       /* SVr4.0 1.3 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <userdefs.h>
#include "messages.h"
#include "funcs.h"

FILE *defptr;		/* default file - fptr */

extern void errmsg();
extern char *ctime();
extern void exit();
extern time_t time();
extern int lockf();

/* putusrdef - 
 * 	changes default values in defadduser file
 */
putusrdef( defs, usertype )
struct userdefs *defs;
char *usertype;
{
	time_t timeval;		/* time value from time */

	/* 
	 * file format is:
	 * #<tab>Default values for adduser.  Changed mm/dd/yy hh:mm:ss.
	 * defgroup=m	(m=default group id)
	 * defgname=str1	(str1=default group name)
	 * defparent=str2	(str2=default base directory)
	 * definactive=x	(x=default inactive)
	 * defexpire=y		(y=default expire)
	 */

	if (is_role(usertype)) {
		if((defptr = fopen( DEFROLEFILE, "w")) == NULL) {
			errmsg( M_FAILED );
			return( EX_UPDATE );
		}
	} else {
		if((defptr = fopen( DEFFILE, "w")) == NULL) {
			errmsg( M_FAILED );
			return( EX_UPDATE );
		}
	}

	if(lockf(fileno(defptr), F_LOCK, 0) != 0) {
		/* print error if can't lock whole of DEFFILE */
		errmsg( M_UPDATE, "created" );
		return( EX_UPDATE );
	}

	/* get time */
	timeval = time((time_t *) 0);

	/* write it to file */
	if (is_role(usertype)) {
		if ( fprintf( defptr, "%s%s\n",
		    FHEADER_ROLE, ctime(&timeval)) <= 0) {
			errmsg( M_UPDATE, "created" );
		}
	} else {
		if ( fprintf( defptr, "%s%s\n",
		    FHEADER, ctime(&timeval)) <= 0) {
			errmsg( M_UPDATE, "created" );
		}
	}
	if( fprintf( defptr,
	    "%s%d\n%s%s\n%s%s\n%s%s\n%s%s\n%s%d\n%s%s\n%s%s\n%s%s\n",
		GIDSTR, defs->defgroup,
		GNAMSTR, defs->defgname, PARSTR, defs->defparent,
		SKLSTR, defs->defskel, SHELLSTR, defs->defshell, 
		INACTSTR, defs->definact, EXPIRESTR, defs->defexpire,
		AUTHSTR, defs->defauth, PROFSTR, defs->defprof) <= 0 ) {

		errmsg( M_UPDATE, "created" );
	}
	if (!is_role(usertype)) {
		if ( fprintf( defptr, "%s%s\n",
		    ROLESTR, defs->defrole) <= 0) {
			errmsg( M_UPDATE, "created" );
		}
	}

	(void) lockf(fileno(defptr), F_ULOCK, 0);
	(void) fclose(defptr);

	return( EX_SUCCESS );
}
