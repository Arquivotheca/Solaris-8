/*
 * Copyright (c) 1997,, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)usermod.c	1.10	99/04/15 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <userdefs.h>
#include <errno.h>
#include "users.h"
#include "messages.h"
#include "funcs.h"

/*******************************************************************************
 *  usermod [-u uid [-o] | -g group | -G group [[,group]...] | -d dir [-m]
 *		| -s shell | -c comment | -l new_logname] 
 *		| -f inactive | -e expire ]
 *		[ -A authorization [, authorization ...]]
 *		[ -P profile [, profile ...]]
 *		[ -R role [, role ...]] login
 *
 *	This command adds new user logins to the system.  Arguments are:
 *
 *	uid - an integer less than MAXUID
 *	group - an existing group's integer ID or char string name
 *	dir - a directory
 *	shell - a program to be used as a shell
 *	comment - any text string
 *	skel_dir - a directory
 *	base_dir - a directory
 *	rid - an integer less than 2**16 (USHORT)
 *	login - a string of printable chars except colon (:)
 *	inactive - number of days a login maybe inactive before it is locked
 *	expire - date when a login is no longer valid
 *	authorization - One or more comma separated authorizations defined
 *			in auth_attr(4).
 *	profile - One or more comma separated execution profiles defined
 *		  in prof_attr(4)
 *	role - One or more comma-separated role names defined in user_attr(4)
 *
 ******************************************************************************/

struct userdefs *getusrdef();
extern int **valid_lgroup(), getopt(), isbusy(); 
extern int valid_uid(), check_perm(), create_home(), move_dir();
extern int valid_expire(), edit_group(), call_passmgmt();
extern void errmsg();

static char *nargv[30];		/* arguments for execvp of passmgmt */
static int argindex;			/* argument index into nargv */

static uid_t uid;			/* new uid */
static gid_t gid;			/* gid of new login */
static char *new_logname = NULL;		/* new login name with -l option */
static char *uidstr = NULL;			/* uid from command line */
static char *group = NULL;			/* group from command line */
static char *grps = NULL;			/* multi groups from command line */
static char *dir = NULL;			/* home dir from command line */
static char *shell = NULL;			/* shell from command line */
static char *comment = NULL;			/* comment from command line */
static char *logname = NULL;			/* login name to add */
static char *inactstr = NULL;			/* inactive from command line */
static char *expire = NULL;			/* expiration date from command line */

static char *authstr = NULL;	/* new list of authorizations */
static char *profstr = NULL;	/* new list of profiles */
static char *rolestr = NULL;	/* new list of roles */
static char *usertype;

static struct userdefs *usrdefs;	/* defaults for useradd */

char *cmdname;
static char gidstring[32], uidstring[32];
char inactstring[10];

char *
strcpmalloc(str)
char *str;
{
	char *cp;

	if (str == NULL)
		return NULL;

	return (strcpy((char *)malloc((unsigned int)strlen(str)+1), str));
}
struct passwd *
passwd_cpmalloc(opw)
struct passwd *opw;
{
	struct passwd *npw;

	if (opw == NULL)
		return (NULL);


	npw = (struct passwd *)malloc(sizeof(struct passwd));

	npw->pw_name = strcpmalloc(opw->pw_name);
	npw->pw_passwd = strcpmalloc(opw->pw_passwd);
	npw->pw_uid = opw->pw_uid;
	npw->pw_gid = opw->pw_gid;
	npw->pw_age = strcpmalloc(opw->pw_age);
	npw->pw_comment = strcpmalloc(opw->pw_comment);
	npw->pw_gecos  = strcpmalloc(opw->pw_gecos);
	npw->pw_dir = strcpmalloc(opw->pw_dir);
	npw->pw_shell = strcpmalloc(opw->pw_shell);

	return(npw);
}

main(argc, argv)
int argc;
char **argv;
{
	int ch, ret = EX_SUCCESS, call_pass = 0, oflag = 0;
	int tries, mflag = 0, inact, **gidlist, flag = 0;
	char *ptr;
	struct passwd *pstruct;		/* password struct for login */
	struct passwd *pw;
	struct group *g_ptr;	/* validated group from -g */
	struct stat statbuf;		/* status buffer for stat */
#ifndef att
	FILE *pwf;		/* fille ptr for opened passwd file */
#endif
	int warning;
	char *check_result;

	cmdname = argv[0];

	if( geteuid() != 0 ) {
		errmsg( M_PERM_DENIED );
		exit( EX_NO_PERM );
	}

	opterr = 0;			/* no print errors from getopt */
	/* get user type based on the program name */
	usertype = getusertype(argv[0]);

	while((ch = getopt(argc, argv, "c:d:e:f:G:g:l:mos:u:A:P:R:")) != EOF)
		switch(ch) {
			case 'c':
				comment = optarg;
				flag++;
				break;
			case 'd':
				dir = optarg;
				flag++;
				break;
			case 'e':
				expire = optarg;
				flag++;
				break;
			case 'f':
				inactstr = optarg;
				flag++;
				break;
			case 'G':
				grps = optarg;
				flag++;
				break;
			case 'g':
				group = optarg;
				flag++;
				break;
			case 'l':
				new_logname = optarg;
				flag++;
				break;
			case 'm':
				mflag++;
				flag++;
				break;
			case 'o':
				oflag++;
				flag++;
				break;
			case 's':
				shell = optarg;
				flag++;
				break;
			case 'u':
				uidstr = optarg;
				flag++;
				break;
			case 'A':
				authstr = optarg;
				flag++;
				break;
			case 'P':
				profstr = optarg;
				flag++;
				break;
			case 'R':
				if (is_role(usertype)) {
					errmsg( M_MRUSAGE );
					exit( EX_SYNTAX );
				} else {
					rolestr = optarg;
					flag++;
					break;
				}
			case '?':
				errmsg( M_MUSAGE );
				exit( EX_SYNTAX );
		}

	if( optind != argc - 1 || flag == 0 ) {
		if (is_role(usertype))
			errmsg( M_MRUSAGE );
		else
			errmsg( M_MUSAGE );
		exit( EX_SYNTAX );
	}

	if( (!uidstr && oflag) || (mflag && !dir) ) {
		if (is_role(usertype))
			errmsg( M_MRUSAGE );
		else
			errmsg( M_MUSAGE );
		exit( EX_SYNTAX );
	}

	logname = argv[optind];


#ifdef att
	pw = getpwnam(logname);
#else
	/*
	 * Do this with fgetpwent to make sure we are only looking on local
	 * system (since passmgmt only works on local system).
	 */
	if ((pwf = fopen("/etc/passwd", "r")) == NULL) {
		errmsg( M_OOPS, "open", "/etc/passwd");
		exit(EX_FAILURE);
	}
	while ((pw = fgetpwent(pwf)) != NULL)
		if (strcmp(pw->pw_name, logname) == 0)
			break;

	fclose(pwf);
#endif

	if (pw == NULL) {
		errmsg( M_EXIST, logname );
		exit( EX_NAME_NOT_EXIST );
	}

	pstruct = passwd_cpmalloc(pw);

	if( isbusy(logname) ) {
		errmsg( M_BUSY, logname, "change" );
		exit( EX_BUSY );
	}

	if( new_logname && strcmp( new_logname, logname ) ) {
		switch (valid_login(new_logname, (struct passwd **) NULL,
			&warning)) {
		case INVALID:
			errmsg( M_INVALID, new_logname, "login name" );
			exit( EX_BADARG );
			/*NOTREACHED*/

		case NOTUNIQUE:
			errmsg( M_USED, new_logname );
			exit( EX_NAME_EXISTS );
			/*NOTREACHED*/
		default:
			call_pass = 1;
			break;
		}
		if (warning)
			warningmsg(warning, logname);
	}

	/* get defaults for adding & modifying users */
	usrdefs = getusrdef(usertype);

	if (uidstr) {
		/* convert uidstr to integer */
		errno = 0;
		uid = (uid_t) strtol(uidstr, &ptr, (int) 10);
		if (*ptr || errno == ERANGE) {
			errmsg( M_INVALID, uidstr, "user id" );
			exit( EX_BADARG );
		}

		if( uid != pstruct->pw_uid ) {
			switch( valid_uid( uid, NULL ) ) {
			case NOTUNIQUE:
				if( !oflag ) {
					/* override not specified */
					errmsg( M_UID_USED, uid);
					exit( EX_ID_EXISTS );
				}
				break;
			case RESERVED:
				errmsg( M_RESERVED, uid );
				break;
			case TOOBIG:
				errmsg( M_TOOBIG, "uid", uid );
				exit( EX_BADARG );
				break;
			}

			call_pass = 1;

		} else {
			/* uid's the same, so don't change anything */
			uidstr = NULL;
			oflag = 0;
		}

	} else uid = pstruct->pw_uid;

	if( group ) {
		switch (valid_group( group, &g_ptr, &warning)) {
		case INVALID:
			errmsg( M_INVALID, group, "group id" );
			exit( EX_BADARG );
			/*NOTREACHED*/
		case TOOBIG:
			errmsg( M_TOOBIG, "gid", group );
			exit( EX_BADARG );
			/*NOTREACHED*/
		case UNIQUE:
			errmsg( M_GRP_NOTUSED, group );
			exit( EX_NAME_NOT_EXIST );
			/*NOTREACHED*/
		}
		if (warning)
			warningmsg(warning, group);

		gid = g_ptr->gr_gid;

		/* call passmgmt if gid is different, else ignore group */
		if( gid != pstruct->pw_gid )
			call_pass = 1;
		else group = NULL;

	} else gid = pstruct->pw_gid;

	if( grps && *grps ) {
		if( !(gidlist = valid_lgroup( grps, gid )) )
			exit( EX_BADARG );
	} else
		gidlist = (int **)0;

	if( dir ) {
		if( REL_PATH( dir ) ) {
			errmsg( M_RELPATH, dir );
			exit( EX_BADARG );
		}
		if( strcmp( pstruct->pw_dir, dir ) == 0 ) {
			/* home directory is the same so ignore dflag & mflag */
			dir = NULL;
			mflag = 0;
		} else call_pass = 1;
	}

	if( mflag ) {
		if( stat(dir, &statbuf) == 0 ) {
			/* Home directory exists */
			if( check_perm( statbuf, pstruct->pw_uid,
			    pstruct->pw_gid, S_IWOTH|S_IXOTH ) != 0 ) {
				errmsg( M_NO_PERM, logname, dir);
				exit( EX_NO_PERM );
			}

		} else ret = create_home( dir, NULL, uid, gid );

		if( ret == EX_SUCCESS )
			ret = move_dir(pstruct->pw_dir, dir, logname );

		if( ret != EX_SUCCESS )
			exit( ret );
	}

	if( shell ) {
		if( REL_PATH( shell ) ) {
			errmsg( M_RELPATH, shell );
			exit( EX_BADARG );
		}
		if( strcmp( pstruct->pw_shell, shell) == 0 ) {
			/* ignore s option if shell is not different */
			shell = NULL;
		} else {
			if( stat(shell, &statbuf ) < 0
				|| (statbuf.st_mode & S_IFMT) != S_IFREG 
				|| (statbuf.st_mode & 0555) != 0555 ) {
		
				errmsg( M_INVALID, shell, "shell" );
				exit( EX_BADARG );
			}

			call_pass = 1;
		}
	}

	if( comment )
		/* ignore comment if comment is not different than passwd entry */
		if( strcmp(pstruct->pw_comment, comment) )
			call_pass = 1;
		else comment = NULL;

	/* inactive string is a positive integer */
	if( inactstr ) {
		/* convert inactstr to integer */
		inact = (int) strtol( inactstr, &ptr, (int) 10 );
		if( *ptr || inact < 0 ) {
			errmsg( M_INVALID, inactstr, "inactivity period" );
			exit( EX_BADARG );
		}
		call_pass = 1;
	}

	/* expiration string is a date, newer than today */
	if( expire ) {
		if( *expire
			&& valid_expire( expire, (time_t *)0 ) == INVALID ) {
			errmsg( M_INVALID, expire, "expiration date" );
			exit( EX_BADARG );
		}
		call_pass = 1;
	}

	if (authstr) {
		check_result = check_auth(authstr);
		if (check_result != NULL) {
			errmsg( M_INVALID, check_result, "authorization" );
			exit( EX_BADARG );
		}
		call_pass = 1;
	}

	if (profstr) {
		check_result = check_prof(profstr);
		if (check_result != NULL) {
			errmsg( M_INVALID, check_result, "profile name" );
			exit( EX_BADARG );
		}
		call_pass = 1;
	}

	if (rolestr) {
		check_result = check_role(rolestr);
		if (check_result != NULL) {
			errmsg( M_INVALID, check_result, "role name" );
			exit( EX_BADARG );
		}
		call_pass = 1;
	}

	/* that's it for validations - now do the work */

	if( grps ) {
		/* redefine login's supplentary group memberships */
		ret = edit_group( logname, new_logname, gidlist, 1 );
		if( ret != EX_SUCCESS ) {
			errmsg( M_UPDATE, "modified" );
			exit( ret );
		}
	}

	if( !call_pass ) exit( ret );

	/* only get to here if need to call passmgmt */
	/* set up arguments to  passmgmt in nargv array */
	argindex = 0;
	nargv[argindex++] = "passmgmt";
	nargv[argindex++] = "-m";	/* modify */

	if(comment) {	/* comment */
		nargv[argindex++] = "-c";
		nargv[argindex++] = comment;
	}

	if(dir) {
		/* flags for home directory */
		nargv[argindex++] = "-h";
		nargv[argindex++] = dir;
	}

	if(group) {
		/* set gid flag */
		nargv[argindex++] = "-g";
		(void) sprintf( gidstring, "%ld", gid );
		nargv[argindex++] = gidstring;
	}

	if(shell) { 	/* shell */
		nargv[argindex++] = "-s";
		nargv[argindex++] = shell;
	}

	if(inactstr) {
		nargv[argindex++] = "-f";
		nargv[argindex++] = inactstr;
	}

	if(expire) {
		nargv[argindex++] = "-e";
		nargv[argindex++] = expire;
	}

	if(uidstr) {	/* set uid flag */
		nargv[argindex++] = "-u";
		(void) sprintf( uidstring, "%ld", uid );
		nargv[argindex++] = uidstring;
	}

	if(oflag) nargv[argindex++] = "-o";

	if(new_logname) {	/* redefine login name */
		nargv[argindex++] = "-l";
		nargv[argindex++] = new_logname;
	}

	if (authstr || profstr || rolestr) {
		nargv[argindex++] = "-T";
		nargv[argindex++] = usertype;

		if (authstr) {
			nargv[argindex++] = "-A";
			nargv[argindex++] = authstr;
		}

		if (profstr) {
			nargv[argindex++] = "-P";
			nargv[argindex++] = profstr;
		}

		if (rolestr) {
			nargv[argindex++] = "-R";
			nargv[argindex++] = rolestr;
		}
	}

	/* finally - login name */
	nargv[argindex++] = logname;

	/* set the last to null */
	nargv[argindex++] = NULL;

	/* now call passmgmt */
	ret = PEX_FAILED;
	for( tries = 3; ret != PEX_SUCCESS && tries--; ) {
		switch( ret = call_passmgmt( nargv ) ) {
		case PEX_SUCCESS:
		case PEX_BUSY:
			break;

		case PEX_HOSED_FILES:
			errmsg( M_HOSED_FILES );
			exit( EX_INCONSISTENT );
			break;

		case PEX_SYNTAX:
		case PEX_BADARG:
			/* should NEVER occur that passmgmt usage is wrong */
			if (is_role(usertype))
				errmsg( M_MRUSAGE );
			else
				errmsg( M_MUSAGE );
			exit( EX_SYNTAX );
			break;

		case PEX_BADUID:
			/* uid is used - shouldn't happen but print message anyway */
			errmsg( M_UID_USED, uid );
			exit( EX_ID_EXISTS );
			break;

		case PEX_BADNAME:
			/* invalid loname */
			errmsg( M_USED, logname);
			exit( EX_NAME_EXISTS );
			break;

		default:
			errmsg( M_UPDATE, "modified" );
			exit( ret );
			break;
		}
	}
	if( tries == 0 ) {
		errmsg( M_UPDATE, "modified" );
		exit( ret );
	}

	exit( ret );
	/*NOTREACHED*/
}
