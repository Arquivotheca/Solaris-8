/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MAILLOCK_H
#define	_MAILLOCK_H

#pragma ident	"@(#)maillock.h	1.7	99/03/09 SMI"	/* SVr4.0 1.6	*/

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAILDIR		"/var/mail/"
#define	SAVEDIR		"/var/mail/:saved/"

#define	PATHSIZE	1024	/* maximum path length of a lock file */
#define	L_SUCCESS	0
#define	L_NAMELEN	1	/* recipient name > 13 chars */
#define	L_TMPLOCK	2	/* problem creating temp lockfile */
#define	L_TMPWRITE	3	/* problem writing pid into temp lockfile */
#define	L_MAXTRYS	4	/* cannot link to lockfile after N tries */
#define	L_ERROR		5	/* Something other than EEXIST happened */
#define	L_MANLOCK	6	/* cannot set mandatory lock on temp lockfile */

#if defined(__STDC__)
extern int maillock(char *user, int retrycnt);
extern void mailunlock(void);
extern void touchlock(void);
#else
extern int maillock();
extern void mailunlock();
extern void touchlock();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _MAILLOCK_H */
