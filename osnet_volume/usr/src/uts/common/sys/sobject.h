/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SOBJECT_H
#define	_SYS_SOBJECT_H

#pragma ident	"@(#)sobject.h	1.9	99/10/25 SMI"

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Type-number definitions for the various synchronization
 * objects defined for the system. The numeric values
 * assigned to the various definitions are explicit, since
 * the synch-object mapping array depends on these values.
 */

typedef enum syncobj {
	SOBJ_NONE	= 0,	/* undefined synchonization object */
	SOBJ_MUTEX	= 1,	/* mutex synchonization object */
	SOBJ_RWLOCK	= 2,	/* readers/writer synchonization object */
	SOBJ_CV		= 3,	/* cond. variable synchonization object */
	SOBJ_SEMA	= 4,	/* semaphore synchonization object */
	SOBJ_USER	= 5,	/* user-level synchronization object */
	SOBJ_USER_PI	= 6,	/* user-level sobj having Prio Inheritance */
	SOBJ_SHUTTLE 	= 7	/* shuttle synchronization object */
} syncobj_t;

/*
 * The following data structure is used to map
 * synchronization object type numbers to the
 * synchronization object's sleep queue number
 * or the synch. object's owner function.
 */
typedef struct _sobj_ops {
	syncobj_t	sobj_type;
	kthread_t	*(*sobj_owner)();
	void		(*sobj_unsleep)(kthread_t *);
	void		(*sobj_change_pri)(kthread_t *, pri_t, pri_t *);
} sobj_ops_t;

#ifdef	_KERNEL

#define	SOBJ_TYPE(sobj_ops)		sobj_ops->sobj_type
#define	SOBJ_OWNER(sobj_ops, sobj)	(*(sobj_ops->sobj_owner))(sobj)
#define	SOBJ_UNSLEEP(sobj_ops, t)	(*(sobj_ops->sobj_unsleep))(t)
#define	SOBJ_CHANGE_PRI(sobj_ops, t, pri)	\
			(*(sobj_ops->sobj_change_pri))(t, pri, &t->t_pri)
#define	SOBJ_CHANGE_EPRI(sobj_ops, t, pri)	\
			(*(sobj_ops->sobj_change_pri))(t, pri, &t->t_epri)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SOBJECT_H */
