/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * semaphore.h:
 *
 * definitions for thread synchronization primitives:
 * mutexs, semaphores, condition variables, and readers/writer
 * locks.
 */

#ifndef _SYS_SEMA_IMPL_H
#define	_SYS_SEMA_IMPL_H

#pragma ident	"@(#)sema_impl.h	1.7	97/04/04 SMI"

#ifndef	_ASM
#include <sys/types.h>
#include <sys/machlock.h>
#endif	/* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

/*
 * Semaphores.
 */

typedef struct _sema_impl {
	struct _kthread	*s_slpq;
	int		s_count;
} sema_impl_t;

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SEMA_IMPL_H */
