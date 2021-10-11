/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1989,1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_VMSYSTM_H
#define	_SYS_VMSYSTM_H

#pragma ident	"@(#)vmsystm.h	2.49	99/09/19 SMI"

#include <sys/proc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Miscellaneous virtual memory subsystem variables and structures.
 */
#ifdef _KERNEL
extern pgcnt_t	freemem;	/* remaining blocks of free memory */
extern pgcnt_t	avefree;	/* 5 sec moving average of free memory */
extern pgcnt_t	avefree30;	/* 30 sec moving average of free memory */
extern pgcnt_t	deficit;	/* estimate of needs of new swapped in procs */
extern pgcnt_t	nscan;		/* number of scans in last second */
extern pgcnt_t	desscan;	/* desired pages scanned per second */
extern pgcnt_t	slowscan;
extern pgcnt_t	fastscan;
extern pgcnt_t	pushes;		/* number of pages pushed to swap device */

/* writable copies of tunables */
extern pgcnt_t	maxpgio;	/* max paging i/o per sec before start swaps */
extern pgcnt_t	lotsfree;	/* max free before clock freezes */
extern pgcnt_t	cachefree;	/* min free before stealing fs pages */
extern pgcnt_t	desfree;	/* minimum free pages before swapping begins */
extern pgcnt_t	minfree;	/* no of pages to try to keep free via daemon */
extern pgcnt_t	needfree;	/* no of pages currently being waited for */
extern pgcnt_t	throttlefree;	/* point at which we block PG_WAIT calls */
extern pgcnt_t	pageout_reserve; /* point at which we deny non-PG_WAIT calls */
extern pgcnt_t	pages_before_pager; /* XXX */

/*
 * TRUE if the pageout daemon, fsflush daemon or the scheduler.  These
 * processes can't sleep while trying to free up memory since a deadlock
 * will occur if they do sleep.
 */
#define	NOMEMWAIT() (ttoproc(curthread) == proc_pageout || \
			ttoproc(curthread) == proc_fsflush || \
			ttoproc(curthread) == proc_sched)

/* insure non-zero */
#define	nz(x)	((x) != 0 ? (x) : 1)

/*
 * Flags passed by the swapper to swapout routines of each
 * scheduling class.
 */
#define	HARDSWAP	1
#define	SOFTSWAP	2

/*
 * Values returned by valid_usr_range()
 */
#define	RANGE_OKAY	(0)
#define	RANGE_BADADDR	(1)
#define	RANGE_BADPROT	(2)


struct as;
struct page;
struct anon;

extern int maxslp;
extern ulong_t pginrate;
extern ulong_t pgoutrate;
extern void swapout_lwp(klwp_t *);

extern	int valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen,
		int dir);
extern	int valid_usr_range(caddr_t, size_t, uint_t, struct as *, caddr_t);
extern	int useracc(void *, size_t, int);
extern	void map_addr(caddr_t *addrp, size_t len, offset_t off, int align,
    uint_t flags);
extern	void map_addr_proc(caddr_t *addrp, size_t len, offset_t off,
    int align, caddr_t userlimit, struct proc *p);
extern	void vmmeter(int);
extern	int cow_mapin(struct as *, caddr_t, caddr_t, struct page **,
	struct anon **, size_t *, int);

extern	caddr_t	ppmapin(struct page *, uint_t, caddr_t);
extern	void	ppmapout(caddr_t);

extern	int pf_is_memory(pfn_t);
extern	int	pageout_init(void (*proc)(), proc_t *pp, pri_t pri);

extern	void	dcache_flushall(void);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VMSYSTM_H */
