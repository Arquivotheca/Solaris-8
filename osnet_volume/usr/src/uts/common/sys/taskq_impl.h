/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_TASKQ_IMPL_H
#define	_SYS_TASKQ_IMPL_H

#pragma ident	"@(#)taskq_impl.h	1.1	98/10/23 SMI"

#include <sys/taskq.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct task {
	struct task	*task_next;
	struct task	*task_prev;
	task_func_t	*task_func;
	void		*task_arg;
} task_t;

/*
 * taskq implementation flags: bit range 16-31
 */
#define	TASKQ_ACTIVE	0x00010000

#define	TASKQ_NAMELEN	31

struct taskq {
	char		tq_name[TASKQ_NAMELEN + 1];
	kmutex_t	tq_lock;
	krwlock_t	tq_threadlock;
	kcondvar_t	tq_dispatch_cv;
	kcondvar_t	tq_wait_cv;
	int		tq_flags;
	int		tq_active;
	int		tq_nthreads;
	int		tq_nalloc;
	int		tq_minalloc;
	int		tq_maxalloc;
	task_t		*tq_freelist;
	task_t		tq_task;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TASKQ_IMPL_H */
