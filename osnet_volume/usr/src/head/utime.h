/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* utimbuf is used by utime(2) */

#ifndef _UTIME_H
#define	_UTIME_H

#pragma ident	"@(#)utime.h	1.8	92/07/14 SMI"	/* SVr4.0 1.3	*/

#include <sys/utime.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern int utime(const char *, const struct utimbuf *);
#else
extern int utime();
#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _UTIME_H */
