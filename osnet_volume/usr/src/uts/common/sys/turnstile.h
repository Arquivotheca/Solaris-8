/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_TURNSTILE_H
#define	_SYS_TURNSTILE_H

#pragma ident	"@(#)turnstile.h	1.32	99/10/25 SMI"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/sleepq.h>
#include <sys/mutex.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TS_WRITER_Q	0	/* writer sleepq (exclusive access to sobj) */
#define	TS_READER_Q	1	/* reader sleepq (shared access to sobj) */
#define	TS_NUM_Q	2	/* number of sleep queues per turnstile */

typedef struct turnstile turnstile_t;
struct _sobj_ops;

struct turnstile {
	turnstile_t	*ts_next;	/* next on hash chain */
	turnstile_t	*ts_free;	/* next on freelist */
	void		*ts_sobj;	/* s-object threads are blocking on */
	int		ts_waiters;	/* number of blocked threads */
	pri_t		ts_epri;	/* max priority of blocked threads */
	struct _kthread	*ts_inheritor;	/* thread inheriting priority */
	turnstile_t	*ts_prioinv;	/* next in inheritor's t_prioinv list */
	sleepq_t	ts_sleepq[TS_NUM_Q]; /* read/write sleep queues */
};

#ifdef	_KERNEL

extern turnstile_t *turnstile_lookup(void *);
extern void turnstile_exit(void *);
extern int turnstile_block(turnstile_t *, int, void *, struct _sobj_ops *,
    kmutex_t *);
extern void turnstile_wakeup(turnstile_t *, int, int, struct _kthread *);
extern void turnstile_change_pri(struct _kthread *, pri_t, pri_t *);
extern void turnstile_unsleep(struct _kthread *);
extern void turnstile_stay_asleep(struct _kthread *);
extern void turnstile_pi_recalc(void);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TURNSTILE_H */
