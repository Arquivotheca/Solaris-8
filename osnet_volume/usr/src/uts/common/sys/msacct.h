/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MSACCT_H
#define	_SYS_MSACCT_H

#pragma ident	"@(#)msacct.h	1.7	93/07/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* LWP microstates */
#define	LMS_USER	0	/* running in user mode */
#define	LMS_SYSTEM	1	/* running in sys call or page fault */
#define	LMS_TRAP	2	/* running in other trap */
#define	LMS_TFAULT	3	/* asleep in user text page fault */
#define	LMS_DFAULT	4	/* asleep in user data page fault */
#define	LMS_KFAULT	5	/* asleep in kernel page fault */
#define	LMS_USER_LOCK	6	/* asleep waiting for user-mode lock */
#define	LMS_SLEEP	7	/* asleep for any other reason */
#define	LMS_WAIT_CPU	8	/* waiting for CPU (latency) */
#define	LMS_STOPPED	9	/* stopped (/proc, jobcontrol, or lwp_stop) */

/*
 * NMSTATES must never exceed 17 because of the size restriction
 * of 128 bytes imposed on struct siginfo (see <sys/siginfo.h>).
 */
#define	NMSTATES	10	/* number of microstates */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MSACCT_H */
