/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_TASKQ_H
#define	_SYS_TASKQ_H

#pragma ident	"@(#)taskq.h	1.1	98/10/23 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct taskq taskq_t;
typedef void (task_func_t)(void *);

/*
 * Public flags for taskq_create(): bit range 0-15
 */
#define	TASKQ_PREPOPULATE	0x0001
#define	TASKQ_CPR_SAFE		0x0002

#ifdef _KERNEL

extern void		taskq_init(void);
extern taskq_t		*taskq_create(const char *, int, pri_t, int, int, int);
extern void		taskq_destroy(taskq_t *tl);
extern int		taskq_dispatch(taskq_t *, task_func_t, void *, int);
extern void		taskq_wait(taskq_t *);
extern krwlock_t	*taskq_lock(taskq_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TASKQ_H */
