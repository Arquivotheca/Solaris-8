/*	Copyright (c) 1993, by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SCHED_H
#define	_SCHED_H

#pragma ident	"@(#)sched.h	1.9	98/01/16 SMI"

#include <sys/types.h>
#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct sched_param {
	int	sched_priority;	/* process execution scheduling priority */
	int	sched_nicelim;	/* nice value limit for SCHED_OTHER policy */
	int	sched_nice;	/* nice value for SCHED_OTHER policy */
	int	sched_pad[6];	/* pad to the same size as pcparms_t of */
				/* sys/priocntl.h */
				/*	sizeof(sched_priority) +	*/
				/*	sizeof(pcparms_t.pc_clparms)	*/
};

/*
 *	POSIX scheduling policies
 */
#define	SCHED_OTHER	0
#define	SCHED_FIFO	1	/* run to completion */
#define	SCHED_RR	2	/* round-robin */
#define	SCHED_SYS	3	/* sys scheduling class */
#define	SCHED_IA	4	/* interactive class */
#define	_SCHED_NEXT	5	/* first unassigned policy number */

/*
 * function prototypes
 */
#if	defined(__STDC__)
int	sched_getparam(pid_t pid, struct sched_param *param);
int	sched_setparam(pid_t pid, const struct sched_param *param);
int	sched_getscheduler(pid_t pid);
int	sched_setscheduler(pid_t pid, int policy,
		const struct sched_param *param);
int	sched_yield(void);
int	sched_get_priority_max(int policy);
int	sched_get_priority_min(int policy);
int	sched_rr_get_interval(pid_t pid, struct timespec *interval);
#else
int	sched_getparam();
int	sched_setparam();
int	sched_getscheduler();
int	sched_setscheduler();
int	sched_yield();
int	sched_get_priority_max();
int	sched_get_priority_min();
int	sched_rr_get_interval();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SCHED_H */
