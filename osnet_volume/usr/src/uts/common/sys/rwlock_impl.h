/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_RWLOCK_IMPL_H
#define	_SYS_RWLOCK_IMPL_H

#pragma ident	"@(#)rwlock_impl.h	1.3	98/02/01 SMI"

/*
 * Implementation-private definitions for readers/writer locks.
 */

#ifndef _ASM

#include <sys/rwlock.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct rwlock_impl {
	uintptr_t	rw_wwwh;	/* waiters, write wanted, hold count */
} rwlock_impl_t;

#endif	/* _ASM */

#define	RW_HAS_WAITERS		1
#define	RW_WRITE_WANTED		2
#define	RW_WRITE_LOCKED		4
#define	RW_READ_LOCK		8
#define	RW_WRITE_LOCK(thread)	((uintptr_t)(thread) | RW_WRITE_LOCKED)
#define	RW_HOLD_COUNT		(-RW_READ_LOCK)
#define	RW_HOLD_COUNT_SHIFT	3		/* log2(RW_READ_LOCK) */
#define	RW_READ_COUNT		RW_HOLD_COUNT
#define	RW_OWNER		RW_HOLD_COUNT
#define	RW_LOCKED		RW_HOLD_COUNT
#define	RW_WRITE_CLAIMED	(RW_WRITE_LOCKED | RW_WRITE_WANTED)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RWLOCK_IMPL_H */
