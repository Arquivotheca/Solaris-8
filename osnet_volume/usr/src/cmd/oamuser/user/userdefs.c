/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)userdefs.c	1.4	99/04/07 SMI"       /* SVr4.0 1.4 */

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdio.h>
#include	<string.h>
#include	<userdefs.h>
#include	<limits.h>
#include	"userdisp.h"
#include	"funcs.h"

#ifndef PATH_MAX
#define	PATH_MAX	1024		/* max # of characters in a path name */
#endif

#define	AUTH_MAX	1024	/* max # of characters in list of authorizations */
#define	PROF_MAX	1024	/* max # of characters in list of profiles */
#define	ROLE_MAX	1024	/* max # of characters in list of roles */

/* Print out a NL when the line gets too long */
#define PRINTNL()	\
	if( outcount > 40 ) { \
		outcount = 0; \
		(void) fprintf( fptr, "\n" ); \
	}

#define	SKIPWS(ptr)	while( *ptr && *ptr == ' ' || *ptr == '\t' ) ptr++

/* returns from scan() */
#define	S_GROUP	2
#define	S_GNAME	3
#define	S_PARENT	4
#define	S_SKEL	5
#define	S_SHELL	6
#define	S_INACT	7
#define	S_EXPIRE	8
#define	S_AUTH	9
#define	S_PROF	10
#define	S_ROLE	11

void copy_to_nl();
extern long strtol();

struct userdefs defaults = {
	DEFRID, DEFGROUP, DEFGNAME, DEFPARENT, DEFSKL,
	DEFSHL, DEFINACT, DEFEXPIRE, DEFAUTH, DEFPROF,
	DEFROLE
};

static char gname[ MAXGLEN + 1 ];
static char basedir[ PATH_MAX + 1 ];
static char skeldir[ PATH_MAX + 1 ];
static char shellnam[ PATH_MAX + 1 ];
static char expire[ MAXDLEN + 1 ];
static char auth[ AUTH_MAX + 1 ];
static char prof[ PROF_MAX + 1 ];
static char role[ ROLE_MAX + 1 ];

FILE *defptr;		/* default file - fptr */

static
scan( start_p )
char **start_p;
{
	register char *cur_p = *start_p;
	register sz = 0, rc;

	if( !*cur_p || *cur_p == '\n' || *cur_p == '#' )
		return( 0 );

	/* Something more complicated could be put in here */
	
	if( !strncmp( cur_p, GIDSTR, (sz = strlen( GIDSTR ) ) ) )
		rc = S_GROUP;
	else if( !strncmp( cur_p, GNAMSTR, (sz = strlen( GNAMSTR ) ) ) )
		rc = S_GNAME;
	else if( !strncmp( cur_p, PARSTR, (sz = strlen( PARSTR ) ) ) )
		rc = S_PARENT;
	else if( !strncmp( cur_p, SKLSTR, (sz = strlen( SKLSTR ) ) ) )
		rc = S_SKEL;
	else if( !strncmp( cur_p, SHELLSTR, (sz = strlen( SHELLSTR ) ) ) )
		rc = S_SHELL;
	else if( !strncmp( cur_p, INACTSTR, (sz = strlen( INACTSTR ) ) ) )
		rc = S_INACT;
	else if( !strncmp( cur_p, EXPIRESTR, (sz = strlen( EXPIRESTR ) ) ) )
		rc = S_EXPIRE;
	else if( !strncmp( cur_p, AUTHSTR, (sz = strlen( AUTHSTR ) ) ) )
		rc = S_AUTH;
	else if( !strncmp( cur_p, PROFSTR, (sz = strlen( PROFSTR ) ) ) )
		rc = S_PROF;
	else if( !strncmp( cur_p, ROLESTR, (sz = strlen( ROLESTR ) ) ) )
		rc = S_ROLE;

	*start_p = cur_p + sz;

	return( rc );
}

/* getusrdef - access the user defaults file.  If it doesn't exist,
 *		then returns default values of (values in userdefs.h):
 *		defrid = 100
 *		defgroup = 1
 *		defgname = other
 *		defparent = /home
 *		defskel	= /usr/sadm/skel
 *		defshell = /bin/sh
 *		definact = 0
 *		defexpire = 0
 *		defauth = 0
 *		defprof = 0
 *		defrole = 0
 *
 *	If getusrdef() is unable to access the defaults file, it
 *	returns a NULL pointer.
 *
 * 	If user defaults file exists, then getusrdef uses values
 *  in it to override the above values.
 */

struct userdefs *
getusrdef(char *usertype)
{
	char instr[ 512 ], *ptr;
	
	if (is_role(usertype)) {
		if( (defptr = fopen( DEFROLEFILE, "r") ) == NULL) {
			defaults.defshell = DEFROLESHL;
			defaults.defprof = DEFROLEPROF;
			return( &defaults );
		}
	} else {
		if( (defptr = fopen( DEFFILE, "r") ) == NULL)
			return( &defaults );
	}

	while( fgets( instr, sizeof( instr ), defptr ) != (char*)0 ) {
		ptr = instr;

		SKIPWS(ptr);

		if( *ptr == '#' )
			continue;

		switch( scan( &ptr ) ) {
		case S_GROUP:
			defaults.defgroup = strtol( ptr , (char **)0, 10 );
			break;
		case S_GNAME:
			copy_to_nl( gname, ptr );
			defaults.defgname = gname;
			break;
		case S_PARENT:
			copy_to_nl( basedir, ptr );
			defaults.defparent = basedir;
			break;
		case S_SKEL:
			copy_to_nl( skeldir, ptr );
			defaults.defskel = skeldir;
			break;
		case S_SHELL:
			copy_to_nl( shellnam, ptr );
			defaults.defshell = shellnam;
			break;
		case S_INACT:
			defaults.definact = strtol( ptr, (char **)0, 10 );
			break;
		case S_EXPIRE:
			copy_to_nl( expire, ptr );
			defaults.defexpire = expire;
			break;
		case S_AUTH:
			copy_to_nl( auth, ptr );
			defaults.defauth = auth;
			break;
		case S_PROF:
			copy_to_nl( prof, ptr );
			defaults.defprof = prof;
			break;
		case S_ROLE:
			copy_to_nl( role, ptr );
			defaults.defrole = role;
			break;
		}
	}

	(void) fclose(defptr);

	return( &defaults );
}

void
copy_to_nl( to, from )
register char *to, *from;
{
	if( *from != '\n' )
		do {
			*to++ = *from++;
		} while( *from && *from != '\n' );
	*to = '\0';
}


void
dispusrdef( fptr, flags, usertype )
FILE *fptr;
unsigned flags;
char *usertype;
{
	struct userdefs *deflts = getusrdef(usertype);
	register outcount = 0;

	/* Print out values */

	if( flags & D_GROUP ) {
		outcount += fprintf( fptr, "group=%s,%ld  ",
			deflts->defgname, deflts->defgroup );
		PRINTNL();
	}

	if( flags & D_BASEDIR ) {
		outcount += fprintf( fptr, "basedir=%s  ", deflts->defparent );
		PRINTNL();
	}

	if( flags & D_RID ) {
		outcount += fprintf( fptr, "rid=%ld  ", deflts->defrid );
		PRINTNL();
	}

	if( flags & D_SKEL ) {
		outcount += fprintf( fptr, "skel=%s  ", deflts->defskel );
		PRINTNL();
	}

	if( flags & D_SHELL ) {
		outcount += fprintf( fptr, "shell=%s  ", deflts->defshell );
		PRINTNL();
	}

	if( flags & D_INACT ) {
		outcount += fprintf( fptr, "inactive=%d  ", deflts->definact );
		PRINTNL();
	}

	if( flags & D_EXPIRE ) {
		outcount += fprintf( fptr, "expire=%s  ", deflts->defexpire );
		PRINTNL();
	}

	if( flags & D_AUTH ) {
		outcount += fprintf( fptr, "auths=%s  ", deflts->defauth );
		PRINTNL();
	}

	if( flags & D_PROF ) {
		outcount += fprintf( fptr, "profiles=%s  ", deflts->defprof );
		PRINTNL();
	}

	if(( flags & D_ROLE ) &&
	    (!is_role(usertype))) {
		outcount += fprintf( fptr, "roles=%s  ", deflts->defrole );
		PRINTNL();
	}

	if( outcount > 0 )
		(void) fprintf( fptr, "\n" );
}
