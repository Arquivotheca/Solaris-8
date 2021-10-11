/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_IA_H
#define	_SYS_IA_H

#pragma ident	"@(#)ia.h	1.3	98/01/06 SMI"

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	dcmn_err	if (ia_debug) cmn_err
#define	IA_OFF_QUANTUM	1
#define	IA_OFF_PRIORITY	20
#define	iaumdpri	(iapp->ia_umdpri)
#define	iamedumdpri	(ia_maxumdpri >> 1)

/*
 * interactive dispatcher parameter table entry
 */
typedef struct iadpent {
	pri_t	ia_globpri;	/* global (class independent) priority */
	long	ia_quantum;	/* time quantum given to procs at this level */
	pri_t	ia_tqexp;	/* ia_umdpri assigned when proc at this level */
				/*   exceeds its time quantum */
	pri_t	ia_slpret;	/* ia_umdpri assigned when proc at this level */
				/*  returns to user mode after sleeping */
	short	ia_maxwait;	/* bumped to ia_lwait if more than ia_maxwait */
				/*  secs elapse before receiving full quantum */
	short	ia_lwait;	/* ia_umdpri assigned if ia_dispwait exceeds  */
				/*  ia_maxwait */
} iadpent_t;


/*
 * time-sharing class specific thread structure
 */
typedef struct iaproc {
	long	ia_timeleft;	/* time remaining in procs quantum */
	pri_t	ia_cpupri;	/* system controlled component of ia_umdpri */
	pri_t	ia_uprilim;	/* user priority limit */
	pri_t	ia_upri;	/* user priority */
	pri_t	ia_umdpri;	/* user mode priority within ia class */
	char	ia_nice;	/* nice value for compatibility */
	unsigned char ia_flags;	/* flags defined below */
	short	ia_dispwait;	/* number of wall clock seconds since start */
				/*   of quantum (not reset upon preemption */
	kthread_t *ia_tp;	/* pointer to thread */
	int	*ia_pstatp;	/* pointer to p_stat */
	pri_t	*ia_pprip;	/* pointer to t_pri */
	uint_t	*ia_pflagp;	/* pointer to p_flag */
	struct iaproc *ia_next;	/* link to next iaproc on list */
	struct iaproc *ia_prev;	/* link to previous iaproc on list */
	int	ia_mode;	/* interactive on/off */
} iaproc_t;


/* flags */
#define	IAKPRI	0x01	/* thread at kernel mode priority */
#define	IABACKQ	0x02	/* thread goes to back of disp q when preempted */
#define	IASLEPT	0x04	/* thread had long-term suspend - give new slice */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IA_H */
