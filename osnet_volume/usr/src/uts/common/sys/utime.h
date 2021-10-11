/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_UTIME_H
#define	_SYS_UTIME_H

#pragma ident	"@(#)utime.h	1.9	97/08/12 SMI"	/* SVr4.0 1.4 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* utimbuf is used by utime(2) */
struct utimbuf {
	time_t actime;		/* access time */
	time_t modtime;		/* modification time */
};

#if defined(_SYSCALL32)

/* Kernel's view of ILP32 utimbuf structure */

struct utimbuf32 {
	time32_t actime;	/* access time */
	time32_t modtime;	/* modification time */
};

#endif	/* _SYSCALL32 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_UTIME_H */
