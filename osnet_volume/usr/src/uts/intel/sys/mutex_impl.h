/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MUTEX_IMPL_H
#define	_SYS_MUTEX_IMPL_H

#pragma ident	"@(#)mutex_impl.h	1.7	99/05/04 SMI"

#ifndef	_ASM
#include <sys/types.h>
#include <sys/machlock.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

/*
 * mutex_enter() assumes that the mutex is adaptive and tries to grab the
 * lock by doing a atomic compare and exchange on the first word of the mutex.
 * If the compare and exchange fails, it means that either (1) the lock is a
 * spin lock, or (2) the lock is adaptive but already held.
 * mutex_vector_enter() distinguishes these cases by looking at the mutex
 * type, which is encoded in the low-order bits of the owner field.
 */
typedef union mutex_impl {
	/*
	 * Adaptive mutex.
	 */
	struct adaptive_mutex {
		uintptr_t _m_owner;	/* 0-3/0-7 owner and waiters bit */
#ifndef _LP64
		uintptr_t _m_filler;	/* 4-7 unused */
#endif
	} m_adaptive;

	/*
	 * Spin Mutex.
	 */
	struct spin_mutex {
#ifdef	_LP64
		lock_t	m_dummylock;	/* 1	dummy lock (always set) */
		lock_t	m_spinlock;	/* 1	real lock */
		ushort_t m_filler;	/* 2-3	unused */
		ushort_t m_oldspl;	/* 4-5	old pil value */
		ushort_t m_minspl;	/* 6-7	min pil val if lock held */
#else
		lock_t	m_dummylock;	/* 0	dummy lock (always set) */
		lock_t	m_spinlock;	/* 1	real lock */
		ushort_t m_filler;	/* 2-3	unused */
		ushort_t m_oldspl;	/* 4-5	old pil value */
		ushort_t m_minspl;	/* 6-7	min pil val if lock held */
#endif
	} m_spin;

} mutex_impl_t;

#define	m_owner	m_adaptive._m_owner

#define	MUTEX_WAITERS		0x1
#define	MUTEX_DEAD		0x6
#define	MUTEX_THREAD		(-0x8)

#define	MUTEX_OWNER(lp)		((kthread_id_t)((lp)->m_owner & MUTEX_THREAD))
#define	MUTEX_NO_OWNER		((kthread_id_t)NULL)

#define	MUTEX_SET_WAITERS(lp)						\
{									\
	uintptr_t old;							\
	while ((old = (lp)->m_owner) != 0 &&				\
	    casip(&(lp)->m_owner, old, old | MUTEX_WAITERS) != old)	\
		continue;						\
}

#define	MUTEX_HAS_WAITERS(lp)			((lp)->m_owner & MUTEX_WAITERS)
#define	MUTEX_CLEAR_LOCK_AND_WAITERS(lp)	(lp)->m_owner = 0

#define	MUTEX_SET_TYPE(lp, type)
#define	MUTEX_TYPE_ADAPTIVE(lp)	(((lp)->m_owner & MUTEX_DEAD) == 0)
#define	MUTEX_TYPE_SPIN(lp)	((lp)->m_spin.m_dummylock == LOCK_HELD_VALUE)

#define	MUTEX_DESTROY(lp)	\
	(lp)->m_owner = ((uintptr_t)curthread | MUTEX_DEAD)

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MUTEX_IMPL_H */
