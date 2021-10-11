/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

/*
 * Implementation-private header.  An application should not include
 * this header directly.  The definitions contained here are standards
 * namespace safe.  The timespec_t and timestruc_t structures as defined
 * in <sys/time_impl.h>, contain member names that break X/Open and POSIX
 * namespace when included by <sys/stat.h> or <sys/siginfo.h>.  This
 * header was created to provide namespace safe definitions that are
 * made visible only in the X/Open and POSIX compilation environments.
 */

#ifndef _SYS_TIME_STD_IMPL_H
#define	_SYS_TIME_STD_IMPL_H

#pragma ident	"@(#)time_std_impl.h	1.2	98/02/13 SMI"

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_TIME_T
#define	_TIME_T
typedef	long	time_t;		/* time of day in seconds */
#endif	/* _TIME_T */

typedef	struct	_timespec {
	time_t	__tv_sec;	/* seconds */
	long	__tv_nsec;	/* and nanoseconds */
} _timespec_t;

typedef	struct	_timespec	_timestruc_t;	/* definition per SVr4 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIME_STD_IMPL_H */
