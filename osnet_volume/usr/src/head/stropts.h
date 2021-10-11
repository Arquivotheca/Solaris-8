/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STROPTS_H
#define	_STROPTS_H

#pragma ident	"@(#)stropts.h	1.9	96/04/26 SMI"	/* SVr4.0 1.6	*/

/*
 * Streams user options definitions.
 */

#include <sys/stropts.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)

extern int isastream(int);

extern int getmsg(int, struct strbuf *, struct strbuf *, int *);
extern int putmsg(int, const struct strbuf *, const struct strbuf *, int);

extern int getpmsg(int, struct strbuf *, struct strbuf *, int *, int *);
extern int putpmsg(int, const struct strbuf *, const struct strbuf *, int, int);

/*
 * These three routines are duplicated in unistd.h; duplication necessitated
 * by XPG4.2 compliance/namespace issues.
 */
extern int ioctl(int, int, ...);
extern int fattach(int, const char *);
extern int fdetach(const char *);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _STROPTS_H */
