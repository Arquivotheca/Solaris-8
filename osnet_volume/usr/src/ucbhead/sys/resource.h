/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef _SYS_RESOURCE_H
#define	_SYS_RESOURCE_H

#pragma ident	"@(#)resource.h	1.3	97/06/17 SMI"	/* SVr4.0 1.4	*/

#ifndef _SYS_RUSAGE_H
#include <sys/rusage.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Process priority specifications to get/setpriority.
 */
#define	PRIO_MIN	-20
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Resource limits
 * RLIMIT_RSS removed so RLIMIT_NOFILE takes the value of 5
 * to stay compatible with svr4
 */
#define	RLIMIT_CPU	0		/* cpu time in milliseconds */
#define	RLIMIT_FSIZE	1		/* maximum file size */
#define	RLIMIT_DATA	2		/* data size */
#define	RLIMIT_STACK	3		/* stack size */
#define	RLIMIT_CORE	4		/* core file size */
#define	RLIMIT_NOFILE	5		/* maximum descriptor index + 1 */

#define	RLIM_NLIMITS	7		/* number of resource limits */

#define	RLIM_INFINITY	0x7fffffff

struct rlimit {
	int	rlim_cur;		/* current (soft) limit */
	int	rlim_max;		/* maximum value for rlim_cur */
};

#if defined(__STDC__)
extern int getpriority(int, int);
extern int setpriority(int, int, int);
extern int setrlimit(int, const struct rlimit *);
extern int getrlimit(int, struct rlimit *);
#else
extern int getpriority();
extern int setpriority();
extern int setrlimit();
extern int getrlimit();
#endif

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_RESOURCE_H */
