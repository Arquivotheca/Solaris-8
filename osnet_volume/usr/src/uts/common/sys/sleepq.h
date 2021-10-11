/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SLEEPQ_H
#define	_SYS_SLEEPQ_H

#pragma ident	"@(#)sleepq.h	1.22	98/06/07 SMI"

#include <sys/machlock.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Common definition for a sleep queue,
 * be it an old-style sleep queue, or
 * a constituent of a turnstile.
 */

typedef struct sleepq {
	struct _kthread *sq_first;
} sleepq_t;

/*
 * Definition of the head of a sleep queue hash bucket.
 */
typedef struct _sleepq_head {
	sleepq_t	sq_queue;
	disp_lock_t	sq_lock;
} sleepq_head_t;

#ifdef	_KERNEL

#define	NSLEEPQ		512
#define	SQHASHINDEX(X)	\
	(((uint_t)(X) >> 2) + ((uint_t)(X) >> 9) & (NSLEEPQ - 1))
#define	SQHASH(X)	(&sleepq_head[SQHASHINDEX(X)])

extern sleepq_head_t	sleepq_head[NSLEEPQ];

extern void		sleepq_insert(sleepq_t *, struct _kthread *);
extern struct _kthread	*sleepq_wakeone_chan(sleepq_t *, void *);
extern void		sleepq_wakeall_chan(sleepq_t *, void *);
extern void		sleepq_unsleep(struct _kthread *);
extern void		sleepq_dequeue(struct _kthread *);
extern void		sleepq_unlink(struct _kthread **, struct _kthread *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif


#endif	/* _SYS_SLEEPQ_H */
