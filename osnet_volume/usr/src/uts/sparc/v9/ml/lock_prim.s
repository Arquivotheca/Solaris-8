/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lock_prim.s	1.35	99/07/26 SMI"

#if defined(lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/t_lock.h>
#include <sys/mutex.h>
#include <sys/mutex_impl.h>
#include <sys/rwlock_impl.h>
#include <sys/asm_linkage.h>
#include <sys/machlock.h>
#include <sys/machthread.h>
#include <sys/lockstat.h>

/* #define DEBUG */

#ifdef DEBUG
#include <sys/machparam.h>
#endif /* DEBUG */

/************************************************************************
 *		ATOMIC OPERATIONS
 */

/*
 * uint8_t	ldstub(uint8_t *cp)
 *
 * Store 0xFF at the specified location, and return its previous content.
 */

#if defined(lint)
uint8_t
ldstub(uint8_t *cp)
{
	uint8_t	rv;
	rv = *cp;
	*cp = 0xFF;
	return rv;
}
#else	/* lint */

	ENTRY(ldstub)
	retl
	ldstub	[%o0], %o0
	SET_SIZE(ldstub)

#endif	/* lint */

/*
 * uint32_t	swapl(uint32_t *lp, uint32_t nv)
 *
 * store a new value into a 32-bit cell, and return the old value.
 */

#if defined(lint)
uint32_t
swapl(uint32_t *lp, uint32_t nv)
{
	uint32_t	rv;
	rv = *lp;
	*lp = nv;
	return rv;
}
#else	/* lint */

	ENTRY(swapl)
	swap	[%o0], %o1
	retl
	mov	%o1, %o0
	SET_SIZE(swapl)

#endif	/* lint */

/*
 * uint32_t cas32(uint32_t *target, uint32_t cmp, uint32_t new);
 * ulong_t caslong(ulong_t *target, ulong_t cmp, ulong_t new);
 * uint64_t cas64(uint64_t *target, uint64_t cmp, uint64_t new);
 * void	*casptr(void *target, void *cmp, void *new);
 *
 * store a new value into a 32/64-bit cell and return the old value.
 */

#if defined(lint)

uint32_t
cas32(uint32_t *target, uint32_t cmp, uint32_t new)
{
	uint32_t old;

	if ((old = *target) == cmp)
		*target = new;
	return (old);
}

ulong_t
caslong(ulong_t *target, ulong_t cmp, ulong_t new)
{
	ulong_t old;

	if ((old = *target) == cmp)
		*target = new;
	return (old);
}

uint64_t
cas64(uint64_t *target, uint64_t cmp, uint64_t new)
{
	uint64_t old;

	if ((old = *target) == cmp)
		*target = new;
	return (old);
}

void *
casptr(void *target, void *cmp, void *new)
{
	void *old;

	if ((old = *(void **)target) == cmp)
		*(void **)target = new;
	return (old);
}

#else	/* lint */

	ENTRY(cas32)
	cas	[%o0], %o1, %o2
	retl
	mov	%o2, %o0
	SET_SIZE(cas32)

	ENTRY(casptr)
	ALTENTRY(caslong)
	casn	[%o0], %o1, %o2
	retl
	mov	%o2, %o0
	SET_SIZE(caslong)
	SET_SIZE(casptr)

	ENTRY(cas64)
#ifdef __sparcv9
	casx	[%o0], %o1, %o2
	retl
	mov	%o2, %o0
#else
	sllx	%o1, 32, %g1
	srl	%o2, 0, %o2
	sllx	%o3, 32, %g2
	srl	%o4, 0, %o4
	or	%g1, %o2, %g1
	or	%g2, %o4, %g2
	casx	[%o0], %g1, %g2
	srlx	%g2, 32, %o0
	retl
	srl	%g2, 0, %o1
#endif
	SET_SIZE(cas64)

#endif	/* lint */

/************************************************************************
 *		MEMORY BARRIERS -- see atomic.h for full descriptions.
 */

#if defined(lint)

void
membar_enter(void)
{}

void
membar_exit(void)
{}

void
membar_producer(void)
{}

void
membar_consumer(void)
{}

#else	/* lint */

#ifdef SF_ERRATA_51
	.align 32
	ENTRY(membar_return)
	retl
	nop
	SET_SIZE(membar_return)
#define	MEMBAR_RETURN	ba,pt %icc, membar_return
#else
#define	MEMBAR_RETURN	retl
#endif

	ENTRY(membar_enter)
	MEMBAR_RETURN
	membar	#StoreLoad|#StoreStore
	SET_SIZE(membar_enter)

	ENTRY(membar_exit)
	MEMBAR_RETURN
	membar	#LoadStore|#StoreStore
	SET_SIZE(membar_exit)

	ENTRY(membar_producer)
	MEMBAR_RETURN
	membar	#StoreStore
	SET_SIZE(membar_producer)

	ENTRY(membar_consumer)
	MEMBAR_RETURN
	membar	#LoadLoad
	SET_SIZE(membar_consumer)

#endif	/* lint */

/************************************************************************
 *		MINIMUM LOCKS
 */

#if defined(lint)

/*
 * lock_try(lp), ulock_try(lp)
 *	- returns non-zero on success.
 *	- doesn't block interrupts so don't use this to spin on a lock.
 *	- uses "0xFF is busy, anything else is free" model.
 *
 *      ulock_try() is for a lock in the user address space.
 *      For all V7/V8 sparc systems they are same since the kernel and
 *      user are mapped in a user' context.
 *      For V9 platforms the lock_try and ulock_try are different impl.
 */

int
lock_try(lock_t *lp)
{
	return (0xFF ^ ldstub(lp));
}

void
lock_set(lock_t *lp)
{
	extern void lock_set_spin(lock_t *);

	if (!lock_try(lp))
		lock_set_spin(lp);
	membar_enter();
}

void
lock_clear(lock_t *lp)
{
	membar_exit();
	*lp = 0;
}

int
ulock_try(lock_t *lp)
{
	return (0xFF ^ ldstub(lp));
}

void
ulock_clear(lock_t *lp)
{
	membar_exit();
	*lp = 0;
}

#else	/* lint */

	.align	32
	ENTRY(lock_try)
	ldstub	[%o0], %o1		! try to set lock, get value in %o1
	brnz,pn	%o1, 1f
	membar	#LoadLoad
.lock_try_lockstat_patch_point:
	retl
	or	%o0, 1, %o0		! ensure lo32 != 0
1:
	retl
	clr	%o0
	SET_SIZE(lock_try)

	.align	32
	ENTRY(lock_set)
	ALTENTRY(disp_lock_enter_high)
	ldstub	[%o0], %o1
	brnz,pn	%o1, lock_set_spin	! go to C for the hard case
	membar	#LoadLoad
.lock_set_lockstat_patch_point:
	retl
	nop
	SET_SIZE(disp_lock_enter_high)
	SET_SIZE(lock_set)

	ENTRY(lock_clear)
	ALTENTRY(disp_lock_exit_high)
	membar	#LoadStore|#StoreStore
.lock_clear_lockstat_patch_point:
	retl
	clrb	[%o0]
	SET_SIZE(disp_lock_exit_high)
	SET_SIZE(lock_clear)

	.align	32
	ENTRY(ulock_try)
	ldstuba	[%o0]ASI_USER, %o1	! try to set lock, get value in %o1
	xor	%o1, 0xff, %o0		! delay - return non-zero if success
	retl
	  membar	#LoadLoad
	SET_SIZE(ulock_try)

	ENTRY(ulock_clear)
	membar  #LoadStore|#StoreStore
	retl
	  stba	%g0, [%o0]ASI_USER	! clear lock
	SET_SIZE(ulock_clear)

#endif	/* lint */


/*
 * lock_set_spl(lp, new_pil, *old_pil_addr)
 * 	Sets pil to new_pil, grabs lp, stores old pil in *old_pil_addr.
 */

#if defined(lint)

/* ARGSUSED */
void
lock_set_spl(lock_t *lp, int new_pil, u_short *old_pil_addr)
{
	extern int splr(int);
	extern void lock_set_spl_spin(lock_t *, int, u_short *, int);
	int old_pil;

	old_pil = splr(new_pil);
	if (!lock_try(lp)) {
		lock_set_spl_spin(lp, new_pil, old_pil_addr, old_pil);
	} else {
		*old_pil_addr = (u_short)old_pil;
		membar_enter();
	}
}

#else	/* lint */

	ENTRY(disp_lock_enter)
	rdpr	%pil, %o3			! %o3 = current pil
	wrpr	%g0, LOCK_LEVEL, %pil		! raise pil to LOCK_LEVEL
	ldstub	[%o0], %o4			! try the lock
	brnz,pn	%o4, 1f				! if we missed, go to C
	membar	#LoadLoad
.disp_lock_enter_lockstat_patch_point:
	retl
	sth	%o3, [THREAD_REG + T_OLDSPL]	! delay - save original pil
1:
	set	LOCK_LEVEL, %o1
	ba,pt	%xcc, lock_set_spl_spin
	add	THREAD_REG, T_OLDSPL, %o2
	SET_SIZE(disp_lock_enter)

	ALTENTRY(lock_set_spl)
	rdpr	%pil, %o3			! %o3 = current pil
	cmp	%o3, %o1			! is current pil high enough?
	bl,a,pt %icc, 1f			! if not, write %pil in delay
	wrpr	%g0, %o1, %pil
1:
	ldstub	[%o0], %o4			! try the lock
	brnz,pn	%o4, lock_set_spl_spin		! go to C for the miss case
	membar	#LoadLoad
.lock_set_spl_lockstat_patch_point:
	retl
	sth	%o3, [%o2]			! delay - save original pil
	SET_SIZE(lock_set_spl)

#endif	/* lint */

/*
 * lock_clear_splx(lp, s)
 */

#if defined(lint)

void
lock_clear_splx(lock_t *lp, int s)
{
	extern void splx(int);

	lock_clear(lp);
	splx(s);
}

#else	/* lint */

	ENTRY(disp_lock_exit)
	ldn	[THREAD_REG + T_CPU], %o2	! %o2 = CPU pointer
	ldub	[%o2 + CPU_KPRUNRUN], %o3	! %o3 = CPU->cpu_kprunrun
	brz,pt	%o3, .have_cpup			! no preemption needed
	lduh	[THREAD_REG + T_OLDSPL], %o1	! %o1 = old pil
	save	%sp, -SA(MINFRAME), %sp
	call	disp_lock_exit_nopreempt
	mov	%i0, %o0
	call	kpreempt
	restore	%g0, -1, %o0
	SET_SIZE(disp_lock_exit)

	ENTRY(disp_lock_exit_nopreempt)
	lduh	[THREAD_REG + T_OLDSPL], %o1

	ALTENTRY(lock_clear_splx)
	ldn	[THREAD_REG + T_CPU], %o2	! get CPU pointer
.have_cpup:
	membar	#LoadStore|#StoreStore
	ld	[%o2 + CPU_BASE_SPL], %o2
	clrb	[%o0]				! clear lock
	cmp	%o2, %o1			! compare new to base
	movl	%xcc, %o1, %o2			! use new pri if base is less
.lock_clear_splx_lockstat_patch_point:
	retl
	wrpr	%g0, %o2, %pil
	SET_SIZE(lock_clear_splx)
	SET_SIZE(disp_lock_exit_nopreempt)

	ENTRY(lock_clear_lockstat)
	mov	LS_SPIN_LOCK_HOLD, %o2
	ba,pt	%xcc, lockstat_exit_wrapper
	mov	1, %o3
	SET_SIZE(lock_clear_lockstat)

#endif	/* lint */

/*
 * mutex_enter() and mutex_exit().
 * 
 * These routines handle the simple cases of mutex_enter() (adaptive
 * lock, not held) and mutex_exit() (adaptive lock, held, no waiters).
 * If anything complicated is going on we punt to mutex_vector_enter().
 *
 * mutex_tryenter() is similar to mutex_enter() but returns zero if
 * the lock cannot be acquired, nonzero on success.
 *
 * If mutex_exit() gets preempted in the window between checking waiters
 * and clearing the lock, we can miss wakeups.  Disabling preemption
 * in the mutex code is prohibitively expensive, so instead we detect
 * mutex preemption by examining the trapped PC in the interrupt path.
 * If we interrupt a thread in mutex_exit() that has not yet cleared
 * the lock, pil_interrupt() resets its PC back to the beginning of
 * mutex_exit() so it will check again for waiters when it resumes.
 *
 * The lockstat code below is activated when the lockstat driver
 * calls lockstat_hot_patch() to hot-patch the kernel mutex code.
 * Note that we don't need to test lockstat_event_mask here -- we won't
 * patch this code in unless we're gathering ADAPTIVE_HOLD lockstats.
 */

#if defined (lint)

/* ARGSUSED */
void
mutex_enter(kmutex_t *lp)
{}

/* ARGSUSED */
int
mutex_tryenter(kmutex_t *lp)
{ return (0); }

/* ARGSUSED */
void
mutex_exit(kmutex_t *lp)
{}

#else
	.align	32
	ENTRY(mutex_enter)
	mov	THREAD_REG, %o1
	casn	[%o0], %g0, %o1			! try to acquire as adaptive
	brnz,pn	%o1, mutex_vector_enter		! locked or wrong type
	membar	#LoadLoad
.mutex_enter_lockstat_patch_point:
	retl
	nop
	ALTENTRY(lockstat_enter_wrapper)
	save	%sp, -SA(MINFRAME), %sp
	ldub	[THREAD_REG + T_LOCKSTAT], %l0
	add	%l0, 1, %l1
	stub	%l1, [THREAD_REG + T_LOCKSTAT]
	sethi	%hi(lockstat_enter_op), %g1
	ldn	[%g1 + %lo(lockstat_enter_op)], %g1
	mov	%i0, %o0
	mov	%i7, %o1
	jmpl	%g1, %o7
	mov	THREAD_REG, %o2
	stub	%l0, [THREAD_REG + T_LOCKSTAT]
	ret
	restore	%g0, 1, %o0			! for mutex_tryenter / lock_try
	SET_SIZE(lockstat_enter_wrapper)
	SET_SIZE(mutex_enter)

	.align	32
	ENTRY(mutex_tryenter)
	mov	THREAD_REG, %o1
	casn	[%o0], %g0, %o1			! try to acquire as adaptive
	brnz,pn	%o1, mutex_vector_tryenter	! locked or wrong type
	membar	#LoadLoad
.mutex_tryenter_lockstat_patch_point:
	retl
	or	%o0, 1, %o0			! ensure lo32 != 0
	SET_SIZE(mutex_tryenter)

	.global	mutex_exit_critical_size
	.global	mutex_exit_critical_start

mutex_exit_critical_size = .mutex_exit_critical_end - mutex_exit_critical_start

	.align	32

	ENTRY(mutex_exit)
mutex_exit_critical_start:		! If we are interrupted, restart here
	ldn	[%o0], %o1		! get the owner field
	membar	#LoadStore|#StoreStore
	cmp	THREAD_REG, %o1		! do we own lock with no waiters?
	be,a,pt	%ncc, 1f		! if so, drive on ...
	stn	%g0, [%o0]		! delay: clear lock if we owned it
.mutex_exit_critical_end:		! for pil_interrupt() hook
	ba,a,pt	%xcc, mutex_vector_exit	! go to C for the hard cases
1:
.mutex_exit_lockstat_patch_point:
	retl
	mov	LS_ADAPTIVE_MUTEX_HOLD, %o2
	mov	1, %o3
	ALTENTRY(lockstat_exit_wrapper)
	save	%sp, -SA(MINFRAME), %sp
	ldub	[THREAD_REG + T_LOCKSTAT], %l0
	add	%l0, 1, %l1
	stub	%l1, [THREAD_REG + T_LOCKSTAT]
	sethi	%hi(lockstat_exit_op), %g1
	ldn	[%g1 + %lo(lockstat_exit_op)], %g1
	mov	%i0, %o0			! lock
	mov	%i7, %o1			! caller
	mov	%i2, %o2			! event
	mov	%i3, %o3			! refcnt
	jmpl	%g1, %o7
	mov	THREAD_REG, %o4			! owner
	stub	%l0, [THREAD_REG + T_LOCKSTAT]
	ret
	restore
	SET_SIZE(lockstat_exit_wrapper)
	SET_SIZE(mutex_exit)

#endif	/* lint */

/*
 * rw_enter() and rw_exit().
 * 
 * These routines handle the simple cases of rw_enter (write-locking an unheld
 * lock or read-locking a lock that's neither write-locked nor write-wanted)
 * and rw_exit (no waiters or not the last reader).  If anything complicated
 * is going on we punt to rw_enter_sleep() and rw_exit_wakeup(), respectively.
 */
#if defined(lint)

/* ARGSUSED */
void
rw_enter(krwlock_t *lp, krw_t rw)
{}

/* ARGSUSED */
void
rw_exit(krwlock_t *lp)
{}

#else

	.align	16
	ENTRY(rw_enter)
	cmp	%o1, RW_WRITER			! entering as writer?
	be,a,pn	%icc, 2f			! if so, go do it ...
	or	THREAD_REG, RW_WRITE_LOCKED, %o5 ! delay: %o5 = owner
	ld	[THREAD_REG + T_KPRI_REQ], %o3	! begin THREAD_KPRI_REQUEST()
	ldn	[%o0], %o4			! %o4 = old lock value
	inc	%o3				! bump kpri
	st	%o3, [THREAD_REG + T_KPRI_REQ]	! store new kpri
	andcc	%o4, RW_WRITE_CLAIMED, %g0	! write-locked or write-wanted?
	bnz,pn	%xcc, rw_enter_sleep		! if so, block
	add	%o4, RW_READ_LOCK, %o5		! delay: increment hold count
1:
	casn	[%o0], %o4, %o5			! try to grab read lock
	cmp	%o4, %o5			! did we get it?
	bne,pn	%xcc, rw_enter_sleep		! if not, go to C
	membar	#LoadLoad
.rw_read_enter_lockstat_patch_point:
	retl
	nop
2:
	casn	[%o0], %g0, %o5			! try to grab write lock
	brnz,pn	%o5, rw_enter_sleep		! if we didn't get it, go to C
	membar	#LoadLoad
.rw_write_enter_lockstat_patch_point:
	retl
	nop
	SET_SIZE(rw_enter)

	.align	16
	ENTRY(rw_exit)
	ldn	[%o0], %o4			! %o4 = old lock value
	membar	#LoadStore|#StoreStore		! membar_exit()
	subcc	%o4, RW_READ_LOCK, %o5		! %o5 = new lock value if reader
	bnz,pn	%xcc, 2f			! single reader, no waiters?
	clr	%o1
1:
	ld	[THREAD_REG + T_KPRI_REQ], %g1	! begin THREAD_KPRI_RELEASE()
	srl	%o4, RW_HOLD_COUNT_SHIFT, %o3	! %o3 = hold count (lockstat)
	mov	LS_RW_READER_HOLD, %o2		! %o2 = lockstat event
	casn	[%o0], %o4, %o5			! try to drop lock
	cmp	%o4, %o5			! did we succeed?
	bne,pn	%xcc, rw_exit_wakeup		! if not, go to C
	dec	%g1				! delay: drop kpri
.rw_read_exit_lockstat_patch_point:
	retl
	st	%g1, [THREAD_REG + T_KPRI_REQ]	! delay: store new kpri
2:
	andcc	%o4, RW_WRITE_LOCKED, %g0	! are we a writer?
	bnz,a,pt %xcc, 3f
	or	THREAD_REG, RW_WRITE_LOCKED, %o4 ! delay: %o4 = owner
	cmp	%o5, RW_READ_LOCK		! would lock still be held?
	bge,pt	%xcc, 1b			! if so, go ahead and drop it
	nop
	ba,pt	%xcc, rw_exit_wakeup		! otherwise, wake waiters
	nop
3:
	casn	[%o0], %o4, %o1			! try to drop write lock
	cmp	%o4, %o1			! did we succeed?
	bne,pn	%xcc, rw_exit_wakeup		! if not, go to C
	mov	LS_RW_WRITER_HOLD, %o2		! delay: %o2 = lockstat event
.rw_write_exit_lockstat_patch_point:
	retl
	mov	1, %o3				! delay: %o3 = hold count
	SET_SIZE(rw_exit)

#endif

#if defined(lint)

void
lockstat_hot_patch(void)
{}

#else

#define	RETL			0x81c3e008
#define	NOP			0x01000000
#define	DISP19			((1 << 19) - 1)
#define	BAPTXCC(src, dest)	0x10680000 + (((dest - src) / 4) & DISP19)
#define	ANNUL			0x20000000

#define	HOT_PATCH(addr, event, mask, active_instr, normal_instr)	\
	set	addr, %o0;		\
	set	normal_instr, %o1;	\
	set	active_instr, %o2;	\
	ldub	[%i0 + event], %o3;	\
	andcc	%o3, mask, %g0;		\
	movnz	%icc, %o2, %o1;		\
	call	hot_patch_kernel_text;	\
	mov	4, %o2;			\
	membar	#Sync

	ENTRY(lockstat_hot_patch)
	save	%sp, -SA(MINFRAME), %sp
	set	lockstat_event, %i0
	HOT_PATCH(.mutex_enter_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_ENTER, NOP, RETL)
	HOT_PATCH(.mutex_tryenter_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_ENTER,
		ANNUL + BAPTXCC(.mutex_tryenter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.mutex_exit_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_EXIT, NOP, RETL)
	HOT_PATCH(.rw_write_enter_lockstat_patch_point,
		LS_RW_WRITER_HOLD, LSE_ENTER,
		BAPTXCC(.rw_write_enter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.rw_read_enter_lockstat_patch_point,
		LS_RW_READER_HOLD, LSE_ENTER,
		BAPTXCC(.rw_read_enter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.rw_write_exit_lockstat_patch_point,
		LS_RW_WRITER_HOLD, LSE_EXIT,
		BAPTXCC(.rw_write_exit_lockstat_patch_point,
		lockstat_exit_wrapper), RETL)
	HOT_PATCH(.rw_read_exit_lockstat_patch_point,
		LS_RW_READER_HOLD, LSE_EXIT,
		BAPTXCC(.rw_read_exit_lockstat_patch_point,
		lockstat_exit_wrapper), RETL)
	HOT_PATCH(.lock_set_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		BAPTXCC(.lock_set_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.lock_try_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		ANNUL + BAPTXCC(.lock_try_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.lock_clear_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT,
		BAPTXCC(.lock_clear_lockstat_patch_point,
		lock_clear_lockstat), RETL)
	HOT_PATCH(.lock_set_spl_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		BAPTXCC(.lock_set_spl_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.lock_clear_splx_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT,
		BAPTXCC(.lock_clear_splx_lockstat_patch_point,
		lock_clear_lockstat), RETL)
	HOT_PATCH(.disp_lock_enter_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		BAPTXCC(.disp_lock_enter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	ret
	restore
	SET_SIZE(lockstat_hot_patch)

#endif	/* lint */

/*
 * asm_mutex_spin_enter(mutex_t *)
 *
 * For use by assembly interrupt handler only.
 * Does not change spl, since the interrupt handler is assumed to be
 * running at high level already.
 * Traps may be off, so cannot panic.
 * Does not keep statistics on the lock.
 *
 * Entry:	%l6 - points to mutex
 * 		%l7 - address of call (returns to %l7+8)
 * Uses:	%l6, %l5
 */
#ifndef lint
	.align 16
	ENTRY_NP(asm_mutex_spin_enter)
	ldstub	[%l6 + M_SPINLOCK], %l5	! try to set lock, get value in %l5
1:
	tst	%l5
	bnz	3f			! lock already held - go spin
	nop
2:	
	jmp	%l7 + 8			! return
	membar	#LoadLoad
	!
	! Spin on lock without using an atomic operation to prevent the caches
	! from unnecessarily moving ownership of the line around.
	!
3:
	ldub	[%l6 + M_SPINLOCK], %l5
4:
	tst	%l5
	bz,a	1b			! lock appears to be free, try again
	ldstub	[%l6 + M_SPINLOCK], %l5	! delay slot - try to set lock

	sethi	%hi(panicstr) , %l5
	ld	[%l5 + %lo(panicstr)], %l5
	tst 	%l5
	bnz	2b			! after panic, feign success
	nop
	b	4b
	ldub	[%l6 + M_SPINLOCK], %l5	! delay - reload lock
	SET_SIZE(asm_mutex_spin_enter)
#endif /* lint */

/*
 * asm_mutex_spin_exit(mutex_t *)
 *
 * For use by assembly interrupt handler only.
 * Does not change spl, since the interrupt handler is assumed to be
 * running at high level already.
 *
 * Entry:	%l6 - points to mutex
 * 		%l7 - address of call (returns to %l7+8)
 * Uses:	none
 */
#ifndef lint
	ENTRY_NP(asm_mutex_spin_exit)
	membar	#LoadStore|#StoreStore
	jmp	%l7 + 8			! return
	clrb	[%l6 + M_SPINLOCK]	! delay - clear lock
	SET_SIZE(asm_mutex_spin_exit)
#endif /* lint */

/*
 * thread_onproc()
 * Set thread in onproc state for the specified CPU.
 * Also set the thread lock pointer to the CPU's onproc lock.
 * Since the new lock isn't held, the store ordering is important.
 * If not done in assembler, the compiler could reorder the stores.
 */
#if defined(lint)

void
thread_onproc(kthread_id_t t, cpu_t *cp)
{
	t->t_state = TS_ONPROC;
	t->t_lockp = &cp->cpu_thread_lock;
}

#else	/* lint */

	ENTRY(thread_onproc)
	set	TS_ONPROC, %o2		! TS_ONPROC state
	st	%o2, [%o0 + T_STATE]	! store state
	add	%o1, CPU_THREAD_LOCK, %o3 ! pointer to disp_lock while running
	retl				! return
	stn	%o3, [%o0 + T_LOCKP]	! delay - store new lock pointer
	SET_SIZE(thread_onproc)

#endif	/* lint */
