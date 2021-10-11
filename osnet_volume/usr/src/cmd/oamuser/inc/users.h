/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)users.h	1.5	97/05/09	SMI"	/* SVr4.0 1.4 */

#include <pwd.h>
#include <grp.h>

#define	GROUP		"/etc/group"

/* validation returns */
#define	NOTUNIQUE	0	/* not unique */
#define	RESERVED	1	/* reserved */
#define	UNIQUE		2	/* is unique */
#define	TOOBIG		3	/* number too big */
#define	INVALID		4

/*
 * Note: constraints checking for warning (release 2.6),
 * and these may be enforced in the future releases.
 */
#define	WARN_NAME_TOO_LONG	0x1
#define	WARN_BAD_GROUP_NAME	0x2
#define	WARN_BAD_LOGNAME_CHAR	0x4
#define	WARN_BAD_LOGNAME_FIRST	0x8
#define	WARN_NO_LOWERCHAR	0x10

/* Exit codes from passmgmt(1) */
#define	PEX_SUCCESS	0
#define	PEX_NO_PERM	1
#define	PEX_SYNTAX	2
#define	PEX_BADARG	3
#define	PEX_BADUID	4
#define	PEX_HOSED_FILES	5
#define	PEX_FAILED	6
#define	PEX_MISSING	7
#define	PEX_BUSY	8
#define	PEX_BADNAME	9

#define	REL_PATH(x)	(x && *x != '/')

/*
 * interfaces available from the library
 */
extern int valid_login(char *, struct passwd **, int *);
extern int valid_gname(char *, struct group **, int *);
extern int valid_group(char *, struct group **, int *);
extern void warningmsg(int, char *);
