/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TS_H
#define	_SYS_TS_H

#pragma ident	"@(#)ts.h	1.15	98/01/06 SMI"	/* SVr4.0 1.5 */

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * time-sharing dispatcher parameter table entry
 */
typedef struct tsdpent {
	pri_t	ts_globpri;	/* global (class independent) priority */
	int	ts_quantum;	/* time quantum given to procs at this level */
	pri_t	ts_tqexp;	/* ts_umdpri assigned when proc at this level */
				/*   exceeds its time quantum */
	pri_t	ts_slpret;	/* ts_umdpri assigned when proc at this level */
				/*  returns to user mode after sleeping */
	short	ts_maxwait;	/* bumped to ts_lwait if more than ts_maxwait */
				/*  secs elapse before receiving full quantum */
	short	ts_lwait;	/* ts_umdpri assigned if ts_dispwait exceeds  */
				/*  ts_maxwait */
} tsdpent_t;


/*
 * time-sharing class specific thread structure
 */
typedef struct tsproc {
	int		ts_timeleft;	/* time remaining in procs quantum */
	uint_t		ts_dispwait;	/* wall clock seconds since start */
				/*   of quantum (not reset upon preemption */
	pri_t	ts_cpupri;	/* system controlled component of ts_umdpri */
	pri_t	ts_uprilim;	/* user priority limit */
	pri_t	ts_upri;	/* user priority */
	pri_t	ts_umdpri;	/* user mode priority within ts class */
	char	ts_nice;	/* nice value for compatibility */
	char	ts_boost;	/* interactive priority offset */
	unsigned char ts_flags;	/* flags defined below */
	kthread_t *ts_tp;	/* pointer to thread */
	struct tsproc *ts_next;	/* link to next tsproc on list */
	struct tsproc *ts_prev;	/* link to previous tsproc on list */
} tsproc_t;


/* flags */
#define	TSKPRI	0x01	/* thread at kernel mode priority */
#define	TSBACKQ	0x02	/* thread goes to back of disp q when preempted */
#define	TSIA	0x04	/* thread is interactive */
#define	TSIASET	0x08	/* interactive thread is "on" */
#define	TSIANICED	0x10	/* interactive thread has been niced */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TS_H */
