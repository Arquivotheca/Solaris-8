/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _INET_LED_H
#define	_INET_LED_H

#pragma ident	"@(#)led.h	1.32	98/07/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	UNIX5_3
#define	SYS5

/*
 * After a server forks, should the child or the parent go back to listen
 * for new requests ?  If this is set, the parent does the work and the child
 * listens.  This assumes that ignoring SIGCLD will allow the parent to
 * ignore the child and not need to do any waits or other cleanup.
 */
#define	PARENT_WORKS_AFTER_FORK

/* Maximum buffer size that should be placed on the stack (local variables) */
#define	MAX_STACK_BUF	512
#define	TLI_STACK_BUF_SIZE	MAX_STACK_BUF

#include <sys/types.h>

#define	LONG_SIGN_BIT	(0x80000000L)

typedef	unsigned char	*DP;
typedef	char		*IDP;
typedef	struct msgb	*MBLKP;
typedef	struct msgb	**MBLKPP;
typedef int		*ERRP;
typedef	char		*USERP;

/* Used only for debugging to find the caller of a function, not required */
#ifndef	RET_ADDR
#define	RET_ADDR(addr_of_first_arg)	(((pfi_t *)addr_of_first_arg)[-1])
#endif

/*
 * Intel can handle unaligned access. However, the checksum routine
 * assumes that the source is 16 bit aligned so we always make sure
 * that packet headers are 16 bit aligned.
 */
#if defined(__i386)
#define	OK_16PTR(p)	(!((uintptr_t)(p) & 0x1))
#define	OK_32PTR(p)	OK_16PTR(p)
#else /* __386 */
#define	OK_16PTR(p)	(!((uintptr_t)(p) & 0x1))
#define	OK_32PTR(p)	(!((uintptr_t)(p) & 0x3))
#endif /* __386 */

#define	noshare

#define	stream_open	open
#define	stream_close	close
#define	stream_ioctl	ioctl
#define	stream_read	read
#define	stream_write	write
#define	stream_poll	poll

#ifdef _KERNEL

#include <sys/param.h>

#include <sys/errno.h>

#include <sys/time.h>

#define	SVR4_STYLE	1

#define	globaldef
#define	globalref	extern

#define	NATIVE_ALLOC
#define	NATIVE_ALLOC_KMEM

/* #define	MI_HRTIMING */
#ifdef	MI_HRTIMING
#include <sys/time.h>
typedef	struct mi_hrt_s {
	hrtime_t hrt_time;	/* Local form of high res timer. */
	int	hrt_opcnt;	/* Number of operations timed. */
	int	hrt_inccnt;	/* Number of INCREMENT operations performed. */
	int	hrt_inccost;	/* Cost per INCREMENT (in usecs). */
} mi_hrt_t;
#define	MI_HRT_DCL(t)			mi_hrt_t t;
#define	MI_HRT_CLEAR(t)			{ (t).hrt_time = 0;		\
					(t).hrt_opcnt = 0;		\
					(t).hrt_inccnt = 0;		\
					(t).hrt_inccost = 0; }
#define	MI_HRT_SET(t)			(t).hrt_time = gethrtime();
#define	MI_HRT_IS_SET(t)		((int)(t).hrt_time != 0)
/*
 * Store the average number of usecs per operation in u based on the time
 * accumulated in t.  Calibrate the cost per increment if it hasn't already
 * been done.
 */
#define	MI_HRT_TO_USECS(t, u)						\
		{							\
		if ((t).hrt_inccost == 0) {				\
			int	_i1;					\
			MI_HRT_DCL(_tmp)				\
			MI_HRT_DCL(_cost)				\
			MI_HRT_CLEAR(_tmp);				\
			MI_HRT_CLEAR(_cost);				\
			MI_HRT_SET(_cost);				\
/*CSTYLED*/								\
			for (_i1 = 1000; --_i1; ) {			\
				MI_HRT_SET(_tmp);			\
				MI_HRT_INCREMENT(_tmp, _cost, 0);	\
			}						\
			MI_HRT_SET(_tmp);				\
			MI_HRT_CLEAR(_tmp);				\
			MI_HRT_INCREMENT(_tmp, _cost, 0);		\
			(t).hrt_inccost = (int)_tmp.hrt_time / 1000;	\
		}							\
		u = (t).hrt_opcnt ?					\
			(((int)((t).hrt_time) - ((t).hrt_inccost *	\
				(t).hrt_inccnt)) / ((t).hrt_opcnt))	\
			: 0;						\
		}
#define	MI_HRT_OPS(t)			(t).hrt_opcnt
#define	MI_HRT_OHD(t)			((t).hrt_inccnt * (t).hrt_inccost)
/* Accumulate statistics from a finished timer into a global one. */
#define	MI_HRT_ACCUMULATE(into, from)				\
		{ MI_HRT_DCL(_tmptime)				\
		_tmptime = into + from;				\
		into = _tmptime;				\
		(into).hrt_opcnt += (from).hrt_opcnt;		\
		(into).hrt_inccnt += (from).hrt_inccnt; }
/* Increment a local timer by the current time minus the start time. */
#define	MI_HRT_INCREMENT(into, start, inc)			\
		{ MI_HRT_DCL(_tmp1) MI_HRT_DCL(_tmp2)		\
		MI_HRT_SET(_tmp1);				\
		_tmp2 = _tmp1 - start;				\
		_tmp1 = into + _tmp2;				\
		into = _tmp1;					\
		(into).hrt_opcnt += inc;			\
		(into).hrt_inccnt += 1; }
#else	/* MI_HRTIMING */
#define	MI_HRT_DCL(t)			/* */
#define	MI_HRT_CLEAR(t)			/* */
#define	MI_HRT_SET(t)			/* */
#define	MI_HRT_IS_SET(t)		B_FALSE
#define	MI_HRT_ACCUMULATE(into, from)	/* */
#define	MI_HRT_INCREMENT(to, from, inc)	/* */
#endif	/* MI_HRTIMING */

/* #define	SYNC_CHK	*/
#ifdef	SYNC_CHK
#define	SYNC_CHK_DCL		boolean_t _sync_chk;
#define	SYNC_CHK_IN(ptr, str)						\
	if (ptr->_sync_chk)						\
		mi_panic("Sync Chk: %s: not alone! 0x%x\n", str, ptr);	\
	ptr->_sync_chk = B_TRUE;
#define	SYNC_CHK_OUT(ptr, str)						\
	if (ptr) {							\
		if (!ptr->_sync_chk)					\
			mi_panic("Sync Chk: %s: not in! 0x%x\n", str, ptr); \
		ptr->_sync_chk = B_FALSE;				\
	}
#else
#define	SYNC_CHK_DCL			/* */
#define	SYNC_CHK_IN(ptr, str)		/* */
#define	SYNC_CHK_OUT(ptr, str)		/* */
#endif	/* SYNC_CHK */

/*
 * backwards compatability for now
 */
#define	become_writer(q, mp, func) qwriter(q, mp, (pfv_t)func, PERIM_OUTER)
#define	become_exclusive(q, mp, func) qwriter(q, mp, (pfv_t)func, PERIM_INNER)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_LED_H */
