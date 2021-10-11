/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lock_prim.s	1.55	99/08/15 SMI"

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
#include <sys/psr.h>
#include <sys/machlock.h>
#include <sys/machthread.h>
#include <sys/lockstat.h>
#include <sys/privregs.h>

#ifdef DEBUG
#include <sys/machparam.h>
#endif DEBUG

/*
 * ldstub(lp) -- apply ldstub to given byte address.
 * Intended for use by lockstat driver only.
 */
#if defined(lint)

uint8_t
ldstub(uint8_t *addr)
{ return (*addr); }

#else	/* lint */

	ENTRY(ldstub)
	retl
	ldstub	[%o0], %o0
	SET_SIZE(ldstub)

#endif	/* lint */

/*
 * Atomic primitives as described in atomic.h.
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

void
atomic_add_16(uint16_t *target, int16_t delta)
{ *target += delta; }

void
atomic_add_32(uint32_t *target, int32_t delta)
{ *target += delta; }

void
atomic_add_long(ulong_t *target, long delta)
{ *target += delta; }

void
atomic_add_64(uint64_t *target, int64_t delta)
{ *target += delta; }

void
atomic_or_uint(uint_t *target, uint_t bits)
{ *target |= bits; }

void
atomic_or_32(uint32_t *target, uint_t bits)
{ *target |= bits; }

void
atomic_and_uint(uint_t *target, uint_t bits)
{ *target &= bits; }

void
atomic_and_32(uint32_t *target, uint_t bits)
{ *target &= bits; }

uint16_t
atomic_add_16_nv(uint16_t *target, int16_t delta)
{ return (*target += delta); }

uint32_t
atomic_add_32_nv(uint32_t *target, int32_t delta)
{ return (*target += delta); }

ulong_t
atomic_add_long_nv(ulong_t *target, long delta)
{ return (*target += delta); }

uint64_t
atomic_add_64_nv(uint64_t *target, int64_t delta)
{ return (*target += delta); }

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

#define	ATOMIC_HASH_SIZE	1024
#define	ATOMIC_HASH_MASK	ATOMIC_HASH_SIZE - 1
#define	ATOMIC_HASH_SHIFT	6

	!
	! Note: atomic_lock is 1K-aligned so that a single
	! sethi instruction is all we need to get its address.
	!
	.seg	".data"
	.global	atomic_lock
	.align	1024
atomic_lock:
	.skip	ATOMIC_HASH_SIZE
	.size	atomic_lock, ATOMIC_HASH_SIZE
	.seg	".text"

/*
 * ATOMIC_ENTER() leaves %o0, %o1, and %o2 intact.
 * It clobbers %o4 and puts critical state into %o3, %o5, and %g1.
 * Do not touch the latter three before doing an ATOMIC_EXIT()!
 */
#define	ATOMIC_ENTER(lockaddr)						\
	mov	%psr, %o5;		/* %o5 = psr (contains pil) */	\
	andn	%o5, PSR_PIL, %g1;	/* mask out old pil level */	\
	wr	%g1, PIL_MAX << PSR_PIL_BIT, %psr; /* block all ints */	\
	srl	lockaddr, ATOMIC_HASH_SHIFT, %o4;	/* psr delay */	\
	set	atomic_lock, %o3;	/* psr delay */			\
	and	%o4, ATOMIC_HASH_MASK, %o4; /* psr del: %o4 = index */	\
	add	%o3, %o4, %o3;		/* %o3 = &atomic_lock[index] */	\
	ldstub	[%o3], %o4;		/* try lock */			\
	tst	%o4;			/* did we get it? */		\
	bz	9f;			/* yes, drive on */		\
	and	%o5, PSR_PIL, %o5;	/* delay: %o5 = original pil */	\
	save	%sp, -SA(MINFRAME), %sp;				\
	call	atomic_lock_set;	/* go to C for the hard case */	\
	mov	%i3, %o0;						\
	restore;							\
	mov	%psr, %g1;		/* %g1 = psr (right window) */	\
	andn	%g1, PSR_PIL, %g1;	/* mask out old pil level */	\
9:

/*
 * ATOMIC_EXIT() leaves only %o0, %o1 and %o4 intact.
 * It relies on state left in %o3, %o5 and %g1 by ATOMIC_ENTER().
 * NOTE: on "return" from ATOMIC_EXIT() you're in the psr delay.
 * Be careful not to put any code there that breaks the psr delay rules.
 */
#define	ATOMIC_EXIT							\
	ld	[THREAD_REG + T_CPU], %o2; /* get CPU pointer */	\
	clrb	[%o3];							\
	ld	[%o2 + CPU_BASE_SPL], %o3; /* %o3 = base pil */		\
	cmp	%o3, %o5;						\
	bl,a	9f;			/* if base pil is less... */	\
	wr	%g1, %o5, %psr;		/* delay - use original pil */	\
	wr	%g1, %o3, %psr;		/* else use base pil */		\
9:

	ENTRY(cas32)
	ALTENTRY(caslong)
	ALTENTRY(casptr)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ld	[%o0], %o4		! load current value from memory
	cmp	%o1, %o4		! does memory match 'old' arg?
	be,a	2f			! if so, store new value
	st	%o2, [%o0]		! delay - store new value
2:	ATOMIC_EXIT
	nop				! psr delay
	retl				! psr delay
	mov	%o4, %o0		! psr delay - return old value
	SET_SIZE(casptr)
	SET_SIZE(caslong)
	SET_SIZE(cas32)

	ENTRY(cas64)
	save	%sp, -SA(MINFRAME), %sp
	ATOMIC_ENTER(%i0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ldd	[%i0], %l0
	xor	%i1, %l0, %l2
	xor	%i2, %l1, %l3
	orcc	%l2, %l3, %g0
	bnz	2f
	nop
	st	%i3, [%i0]
	st	%i4, [%i0 + 4]
2:	ATOMIC_EXIT
	mov	%l0, %i0		! psr delay - return hi32(old_value)
	mov	%l1, %i1		! psr delay - return lo32(old_value)
	ret				! psr delay
	restore
	SET_SIZE(cas64)

	ENTRY(atomic_add_16)
	ALTENTRY(atomic_add_16_nv)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	lduh	[%o0], %o4		! load current value from memory
	add	%o4, %o1, %o4		! compute new value
	sth	%o4, [%o0]		! delay - store new value
	ATOMIC_EXIT
	sll	%o4, 16, %o4		! psr delay - clean out upper 16 bits
	retl				! psr delay
	srl	%o4, 16, %o0		! psr delay - return new value
	SET_SIZE(atomic_add_16_nv)
	SET_SIZE(atomic_add_16)

	ENTRY(atomic_add_32)
	ALTENTRY(atomic_add_long)
	ALTENTRY(atomic_add_32_nv)
	ALTENTRY(atomic_add_long_nv)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ld	[%o0], %o4		! load current value from memory
	add	%o4, %o1, %o4		! compute new value
	st	%o4, [%o0]		! store new value
	ATOMIC_EXIT
	nop				! psr delay
	retl				! psr delay
	mov	%o4, %o0		! psr delay - return new value
	SET_SIZE(atomic_add_long_nv)
	SET_SIZE(atomic_add_32_nv)
	SET_SIZE(atomic_add_long)
	SET_SIZE(atomic_add_32)

	ENTRY(atomic_add_64)
	ALTENTRY(atomic_add_64_nv)
	save	%sp, -SA(MINFRAME), %sp
	ATOMIC_ENTER(%i0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ldd	[%i0], %l0		! load current value from memory
	addcc	%l1, %i2, %l1		! %l1 = lo32(new_value)
	addx	%l0, %i1, %l0		! %l0 = hi32(new_value)
	std	%l0, [%i0]		! store new value
	ATOMIC_EXIT
	mov	%l0, %i0		! psr delay - return hi32(new_value)
	mov	%l1, %i1		! psr delay - return lo32(new_value)
	ret				! psr delay
	restore
	SET_SIZE(atomic_add_64_nv)
	SET_SIZE(atomic_add_64)

	ENTRY(atomic_or_uint)
	ALTENTRY(atomic_or_32)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ld	[%o0], %o4		! load current value from memory
	or	%o4, %o1, %o4		! compute new value
	st	%o4, [%o0]		! store new value
	ATOMIC_EXIT
	nop				! psr delay
	retl				! psr delay
	mov	%o4, %o0		! psr delay - return new value
	SET_SIZE(atomic_or_32)
	SET_SIZE(atomic_or_uint)

	ENTRY(atomic_and_uint)
	ALTENTRY(atomic_and_32)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ld	[%o0], %o4		! load current value from memory
	and	%o4, %o1, %o4		! compute new value
	st	%o4, [%o0]		! store new value
	ATOMIC_EXIT
	nop				! psr delay
	retl				! psr delay
	mov	%o4, %o0		! psr delay - return new value
	SET_SIZE(atomic_and_32)
	SET_SIZE(atomic_and_uint)

	ENTRY(membar_producer)
	retl
	stbar
	SET_SIZE(membar_producer)

	ENTRY(membar_enter)
	ALTENTRY(membar_exit)
	ALTENTRY(membar_consumer)
	retl
	ldstub	[THREAD_REG + T_LOCK_FLUSH], %g0 ! dummy atomic to flush owner
	SET_SIZE(membar_consumer)
	SET_SIZE(membar_exit)
	SET_SIZE(membar_enter)

#endif	/* lint */

/*
 * lock_try(lp), ulock_try(lp)
 *	- returns non-zero on success.
 *	- doesn't block interrupts so don't use this to spin on a lock.
 *
 *      ulock_try() is for a lock in the user address space.
 *      For all V7/V8 sparc systems they are same since the kernel and
 *      user are mapped in a user' context.
 *      For V9 platforms the lock_try and ulock_try are different impl.
 *
 */

#if defined(lint)

/* ARGSUSED */
int
lock_try(lock_t *lp)
{ return (0); }

/* ARGSUSED */
int
ulock_try(lock_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(lock_try)
	ldstub	[%o0], %o1		! try to set lock, get value in %o1
.lock_try_lockstat_patch_point:
	retl				! lockstat patches to "mov %o0, %o2"
	xor	%o1, 0xff, %o0		! delay - return non-zero if success
	tst	%o0
	bnz	lockstat_enter_wrapper	! always returns 1 (success)
	mov	%o2, %o0
	retl
	clr	%o0
	SET_SIZE(lock_try)

        ENTRY(ulock_try)
 
#ifdef DEBUG
	sethi   %hi(KERNELBASE), %o1
	cmp     %o0, %o1
	blu     1f
	nop
 
	set     2f, %o0
	call    panic
	nop
 
2:
	.asciz  "ulock_try: Argument is above KERNELBASE"
	.align  4
 
1:
#endif DEBUG
	ldstub  [%o0], %o1              ! try to set lock, get value in %o1
	retl
	xor     %o1, 0xff, %o0          ! delay - return non-zero if success
	SET_SIZE(ulock_try)


#endif	/* lint */

/*
 * lock_clear(lp), ulock_clear(lp)
 *	- unlock lock without changing interrupt priority level.
 *
 *      ulock_clear() is for a lock in the user address space.
 *      For all V7/V8 sparc systems they are same since the kernel and
 *      user are mapped in a user' context.
 *      For V9 platforms the lock_clear and ulock_clear are different impl.
 */

#if defined(lint)

/* ARGSUSED */
void
lock_clear(lock_t *lp)
{}

/* ARGSUSED */
void
ulock_clear(lock_t *lp)
{}

#else	/* lint */

	ENTRY(disp_lock_exit_high)
	ALTENTRY(lock_clear)
.lock_clear_lockstat_patch_point:
	retl
	clrb	[%o0]
	mov	1, %o3
	ba	lockstat_exit_wrapper
	mov	LS_SPIN_LOCK_HOLD, %o2
	SET_SIZE(lock_clear)
	SET_SIZE(disp_lock_exit_high)

	ENTRY(ulock_clear)
#ifdef DEBUG
	sethi   %hi(KERNELBASE), %o1
	cmp     %o0, %o1
	blu     1f
	nop
 
	set     2f, %o0
	call    panic
	nop
 
2:
	.asciz  "ulock_clear: argument above KERNELBASE"
	.align 4
 
1:
#endif DEBUG
 
	retl
	clrb    [%o0]
	SET_SIZE(ulock_clear)

#endif	/* lint */

/*
 * lock_set_spl(lp, new_pil, *old_pil_addr)
 * 	Sets pil to new_pil, grabs lp, stores old pil in *old_pil_addr.
 */

#if defined(lint)

/* ARGSUSED */
void
lock_set_spl(lock_t *lp, int new_pil, u_short *old_pil)
{}

#else	/* lint */

	ENTRY(disp_lock_enter)
	mov	%psr, %o3		! load psr (contains pil)
	andn	%o3, PSR_PIL, %o5	! mask out old interrupt level
	wr	%o5, LOCK_LEVEL << PSR_PIL_BIT, %psr ! block disp activity
	set	LOCK_LEVEL << PSR_PIL_BIT, %o1 ! psr delay - set %o1 for spin
	add	THREAD_REG, T_OLDSPL, %o2 ! psr delay - compute old pil addr
	ldstub	[%o0], %o4		! psr delay - try to set lock
	tst	%o4			! did we get it?
	bnz	lock_set_spl_spin	! no, go to C for the hard case
	nop
.disp_lock_enter_lockstat_patch_point:
	retl
	sth	%o3, [%o2]		! delay - store old pil
	SET_SIZE(disp_lock_enter)

	ENTRY(lock_set_spl)
	mov	%psr, %o3
	and	%o1, PSR_PIL, %o1	! mask proposed new value for PIL
	and	%o3, PSR_PIL, %o4	! mask current PIL
	cmp	%o4, %o1		! compare current pil to new pil
	bge	2f			! current pil is already high enough
	andn	%o3, PSR_PIL, %o5	! delay - mask out old interrupt level
	wr	%o5, %o1, %psr		! disable (some) interrupts
	nop; nop			! psr delay
2:
	ldstub	[%o0], %o4		! psr delay - try to set lock
	tst	%o4			! did we get it?
	bnz	lock_set_spl_spin	! no, go to C for the hard case
	nop
.lock_set_spl_lockstat_patch_point:
	retl
	sth	%o3, [%o2]		! delay - store old pil
	SET_SIZE(lock_set_spl)

#endif	/* lint */

/*
 * void
 * lock_set(lp)
 */

#if defined(lint)

/* ARGSUSED */
void
lock_set(lock_t *lp)
{}

#else	/* lint */

	ENTRY(disp_lock_enter_high)
	ALTENTRY(lock_set)
	ldstub	[%o0], %o1		! try to set lock, get value in %o1
	tst	%o1			! did we get it?
	bnz	lock_set_spin		! no, go to C for the hard case
	nop
.lock_set_lockstat_patch_point:
	retl
	nop
	SET_SIZE(lock_set)
	SET_SIZE(disp_lock_enter_high)

#endif	/* lint */

/*
 * lock_clear_splx(lp, s)
 */

#if defined(lint)

/* ARGSUSED */
void
lock_clear_splx(lock_t *lp, int s)
{}

#else	/* lint */

	ENTRY(disp_lock_exit)
	ld	[THREAD_REG + T_CPU], %o2	! get CPU pointer
	ldub	[%o2 + CPU_KPRUNRUN], %o3	! get CPU->cpu_kprunrun
	tst	%o3				! preemption needed?
	bz	.have_cpup			! no, just drop the lock
	lduh	[THREAD_REG + T_OLDSPL], %o1	! delay - load old pil
	save	%sp, -SA(MINFRAME), %sp
	call	disp_lock_exit_nopreempt
	mov	%i0, %o0
	call	kpreempt
	restore	%g0, -1, %o0
	SET_SIZE(disp_lock_exit)

	ENTRY(disp_lock_exit_nopreempt)
	lduh	[THREAD_REG + T_OLDSPL], %o1

	ALTENTRY(lock_clear_splx)
	ld	[THREAD_REG + T_CPU], %o2	! get CPU pointer
.have_cpup:
	mov	%psr, %o5	 		! get old PSR
	ld	[%o2 + CPU_BASE_SPL], %o2	! load base spl
	and	%o1, PSR_PIL, %o1		! mask proposed new value
	andn	%o5, PSR_PIL, %o3		! mask out old PIL
	clrb	[%o0]				! clear lock
	cmp	%o2, %o1			! compare new to base
	bl,a	1f				! use new pri if base is less
	wr	%o3, %o1, %psr 			! delay - use new pri
	wr	%o3, %o2, %psr	 		! use base pri
1:
	mov	1, %o3				! psr delay
.lock_clear_splx_lockstat_patch_point:
	retl 					! psr delay
	mov	LS_SPIN_LOCK_HOLD, %o2		! psr delay
	SET_SIZE(lock_clear_splx)
	SET_SIZE(disp_lock_exit_nopreempt)

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
 * Both mutex_enter() and mutex_exit() are sensitive to preemption.
 * If mutex_enter() gets preempted after grabbing the lock but before
 * setting the owner, priority inheritance cannot function; and if
 * mutex_exit() gets preempted in the window between checking waiters
 * and clearing the lock, we can miss wakeups.  Disabling preemption
 * in the mutex code is prohibitively expensive, so instead we detect
 * mutex preemption by examining the trapped PC in the interrupt path.
 * interrupt_prologue() branches to mutex_critical_fixup() to do the
 * actual work.
 *
 * If we interrupt a thread in mutex_enter() that has done a successful
 * ldstub but has not yet set the owner, we set the owner for it.
 * If we interrupt a thread in mutex_exit() that has not yet cleared
 * the lock, we reset its PC back to the beginning of mutex_exit() so
 * it will check again for waiters when it resumes.
 *
 * mutex_critical_fixup() has the following guilty knowledge:
 *
 *	%i0 is the mutex pointer throughout the critical regions
 *
 *	%i1 is the result of the ldstub in the mutex_enter() and
 *	mutex_tryenter() critical regions
 *
 *	%l1 and %l2 are the trapped PC and nPC, respectively
 *
 *	%l3 and %l5 are available as scratch registers
 *
 *	%g7 is the interrupted thread pointer
 *
 * If you change any of these assumptions, make sure you change the
 * interrupt code and mutex_critical_fixup() as well.
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
	!
	! Register usage:
	!
	! %l1 = trapped PC (may be modified to restart mutex_exit())
	! %l2 = trapped nPC (may be modified to restart mutex_exit())
	! %l3, %l5 = scratch
	!
	ENTRY_NP(mutex_critical_fixup)
	set	.mutex_enter_critical_start, %l5
	sub	%l1, %l5, %l3	! any chance we're in critical region?
	cmp	%l3, .mutex_exit_critical_end - .mutex_enter_critical_start
	bgeu	sys_trap	! nothing to do, resume trap handling
	cmp	%l3, .mutex_exit_critical_start - .mutex_enter_critical_start
	blu	1f		! not in mutex_exit() critical region
	tst	%i1		! delay - any chance %i0 is an unowned lock?
	add	%l5,.mutex_exit_critical_start - .mutex_enter_critical_start,%l1
	b	sys_trap	! we're in critical region: restart mutex_exit()
	add	%l1, 4, %l2	! delay - %l2 = mutex_exit + 4
1:
	bnz	sys_trap			! %i0 is not an unowned lock
	cmp	%l3, .mutex_enter_critical_end - .mutex_enter_critical_start
	bgeu	sys_trap			! not in critical region
	sub	%l3, .mutex_enter_hole_start - .mutex_enter_critical_start, %l3
	cmp	%l3, .mutex_enter_hole_end - .mutex_enter_hole_start
	blu	sys_trap			! in the hole
	sra	THREAD_REG, PTR24_LSB, %l3	! delay - compute owner
	b	sys_trap
	st	%l3, [%i0]			! delay - store owner
	SET_SIZE(mutex_critical_fixup)

	ENTRY(mutex_enter)
	ldstub	[%o0], %o1			! try lock
.mutex_enter_critical_start:
	tst	%o1				! did we get it?
	bnz	mutex_vector_enter		! no - locked or wrong type
	sra	THREAD_REG, PTR24_LSB, %o2	! %o2 = lock+owner
.mutex_enter_lockstat_patch_point:
	retl
	st	%o2, [%o0]			! delay - set lock+owner field
.mutex_enter_hole_start:
	SET_SIZE(mutex_enter)

	ENTRY(mutex_tryenter)
	ldstub	[%o0], %o1			! try lock
.mutex_enter_hole_end:
	tst	%o1				! did we get it?
	bnz	mutex_vector_tryenter		! no - locked or wrong type
	sra	THREAD_REG, PTR24_LSB, %o2	! %o2 = lock+owner
.mutex_tryenter_lockstat_patch_point:
	retl
	st	%o2, [%o0]			! delay - set lock+owner field
.mutex_enter_critical_end:
	SET_SIZE(mutex_tryenter)

	ENTRY(mutex_exit)
.mutex_exit_critical_start:
	ld	[%o0], %o1			! load owner/lock field
	sra	THREAD_REG, PTR24_LSB, %o2	! lock+curthread
	ldub	[%o0 + M_WAITERS], %o3		! load waiters
	sub	%o2, %o1, %o4			! %o4 == 0 iff we're the owner
	orcc	%o3, %o4, %g0			! owner with no waiters?
	bz,a	1f
	clr	[%o0]				! delay - clear owner/lock
.mutex_exit_critical_end:
	b,a	mutex_vector_exit		! go to C for the hard cases
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
	ld	[%g1 + %lo(lockstat_exit_op)], %g1
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

	ENTRY_NP(lockstat_enter_wrapper)
	save	%sp, -SA(MINFRAME), %sp
	ldub	[THREAD_REG + T_LOCKSTAT], %l0
	add	%l0, 1, %l1
	stub	%l1, [THREAD_REG + T_LOCKSTAT]
	sethi	%hi(lockstat_enter_op), %g1
	ld	[%g1 + %lo(lockstat_enter_op)], %g1
	mov	%i0, %o0
	mov	%i7, %o1
	jmpl	%g1, %o7
	mov	THREAD_REG, %o2
	stub	%l0, [THREAD_REG + T_LOCKSTAT]
	ret
	restore	%g0, 1, %o0
	SET_SIZE(lockstat_enter_wrapper)

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

	ENTRY(rw_enter)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	cmp	%o1, RW_WRITER			! entering as writer?
	be	2f				! if so, go do it ...
	ld	[%o0], %o4			! delay: %o4 = old lock value
	ld	[THREAD_REG + T_KPRI_REQ], %o2	! begin THREAD_KPRI_REQUEST()
	inc	%o2				! bump kpri
	st	%o2, [THREAD_REG + T_KPRI_REQ]	! store new kpri
	andcc	%o4, RW_WRITE_CLAIMED, %g0	! write-locked or write-wanted?
	bnz	.rw_enter_hard			! if so, block
	add	%o4, RW_READ_LOCK, %o4		! delay: increment hold count
	st	%o4, [%o0]			! store new lock value
	ATOMIC_EXIT
	nop					! psr delay
.rw_read_enter_lockstat_patch_point:
	retl					! psr delay
	nop					! psr delay
2:
	tst	%o4				! currently locked?
	bnz	.rw_enter_hard			! if so, block
	or	THREAD_REG, RW_WRITE_LOCKED, %o2 ! delay: %o2 = owner
	st	%o2, [%o0]			! store new lock value
	ATOMIC_EXIT
	nop					! psr delay
.rw_write_enter_lockstat_patch_point:
	retl					! psr delay
	nop					! psr delay
.rw_enter_hard:
	ATOMIC_EXIT
	nop					! psr delay
	ba	rw_enter_sleep			! psr delay
	nop					! psr delay
	SET_SIZE(rw_enter)

	ENTRY(rw_exit)
	ATOMIC_ENTER(%o0)	! DO NOT TOUCH %o3, %o5, %g1 until ATOMIC_EXIT
	ld	[%o0], %o4			! %o4 = old lock value
	subcc	%o4, RW_READ_LOCK, %o2		! %o2 = new lock value if reader
	bnz	2f				! single reader, no waiters?
	nop
1:
	st	%o2, [%o0]			! store new lock value
	ld	[THREAD_REG + T_KPRI_REQ], %o1	! begin THREAD_KPRI_RELEASE()
	dec	%o1				! drop kpri
	ATOMIC_EXIT
	st	%o1, [THREAD_REG + T_KPRI_REQ]	! psr delay: store new kpri
.rw_read_exit_lockstat_patch_point:
	retl					! psr delay
	mov	LS_RW_READER_HOLD, %o2		! psr delay: %o2 = event
	ba	lockstat_exit_wrapper		! go do lockstat stuff
	srl	%o4, RW_HOLD_COUNT_SHIFT, %o3	! delay: %o3 = hold count
2:
	andcc	%o4, RW_WRITE_LOCKED, %g0	! are we a writer?
	bnz,a	3f
	or	THREAD_REG, RW_WRITE_LOCKED, %o2 ! delay: %o2 = owner
	cmp	%o2, RW_READ_LOCK		! would lock still be held?
	bge	1b				! if so, go ahead and drop it
	nop
.rw_exit_hard:
	ATOMIC_EXIT
	nop					! psr delay
	ba	rw_exit_wakeup			! psr delay
	nop					! psr delay
3:
	cmp	%o4, %o2			! correct owner w/o waiters?
	bne	.rw_exit_hard			! if not, go to C
	nop
	st	%g0, [%o0]			! clear lock
	ATOMIC_EXIT
	mov	1, %o3				! psr delay: %o3 = hold count
.rw_write_exit_lockstat_patch_point:
	retl					! psr delay
	mov	LS_RW_WRITER_HOLD, %o2		! psr delay: %o2 = event
	SET_SIZE(rw_exit)

#endif

#if defined(lint)

void
lockstat_hot_patch(void)
{}

#else

#define	NOP		0x01000000
#define	RETL		0x81c3e008
#define	DISP22		((1 << 22) - 1)
#define	MOV_o0_o2	0x94100008
#define	BA(src, dest)	0x10800000 + (((dest - src) / 4) & DISP22)

#define	HOT_PATCH(addr, event, mask, active_instr, normal_instr)	\
	set	addr, %o0;		\
	set	normal_instr, %o1;	\
	set	active_instr, %o2;	\
	ldub	[%i0 + event], %o3;	\
	andcc	%o3, mask, %g0;		\
	bnz,a	1f;			\
	mov	%o2, %o1;		\
1:	call	hot_patch_kernel_text;	\
	mov	4, %o2;			\
	stbar

	ENTRY(lockstat_hot_patch)
	save	%sp, -SA(MINFRAME), %sp
	set	lockstat_event, %i0
	HOT_PATCH(.mutex_enter_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_ENTER,
		BA(.mutex_enter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.mutex_tryenter_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_ENTER,
		BA(.mutex_tryenter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.mutex_exit_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_EXIT, NOP, RETL)
	HOT_PATCH(.rw_write_enter_lockstat_patch_point,
		LS_RW_WRITER_HOLD, LSE_ENTER,
		BA(.rw_write_enter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.rw_read_enter_lockstat_patch_point,
		LS_RW_READER_HOLD, LSE_ENTER,
		BA(.rw_read_enter_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.rw_write_exit_lockstat_patch_point,
		LS_RW_WRITER_HOLD, LSE_EXIT,
		BA(.rw_write_exit_lockstat_patch_point,
		lockstat_exit_wrapper), RETL)
	HOT_PATCH(.rw_read_exit_lockstat_patch_point,
		LS_RW_READER_HOLD, LSE_EXIT, NOP, RETL)
	HOT_PATCH(.lock_set_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		BA(.lock_set_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.lock_try_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER, MOV_o0_o2, RETL)
	HOT_PATCH(.lock_clear_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT, NOP, RETL)
	HOT_PATCH(.lock_set_spl_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		BA(.lock_set_spl_lockstat_patch_point,
		lockstat_enter_wrapper), RETL)
	HOT_PATCH(.lock_clear_splx_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT,
		BA(.lock_clear_splx_lockstat_patch_point,
		lockstat_exit_wrapper), RETL)
	HOT_PATCH(.disp_lock_enter_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER,
		BA(.disp_lock_enter_lockstat_patch_point,
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
#if defined(lint)

/* ARGSUSED */
void
asm_mutex_spin_enter(kmutex_t *lp)
{}

#else	/* lint */
	ENTRY_NP(asm_mutex_spin_enter)
	ldstub	[%l6 + M_SPINLOCK], %l5	! try to set lock, get value in %l5
1:
	tst	%l5
	bnz	3f			! lock already held - go spin
	nop
2:	
	jmp	%l7 + 8			! return
	nop
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
#endif	/* lint */

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
#if defined(lint)

/* ARGSUSED */
void
asm_mutex_spin_exit(kmutex_t *lp)
{}

#else	/* lint */
	ENTRY_NP(asm_mutex_spin_exit)
	jmp	%l7 + 8			! return
	clrb	[%l6 + M_SPINLOCK]	! delay - clear lock
	SET_SIZE(asm_mutex_spin_exit)
#endif	/* lint */

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
	set	ONPROC_THREAD, %o2	! TS_ONPROC state
	st	%o2, [%o0 + T_STATE]	! store state
	add	%o1, CPU_THREAD_LOCK, %o3 ! pointer to disp_lock while running
	stbar				! make sure stores are seen in order
	retl
	st	%o3, [%o0 + T_LOCKP]	! delay - store new lock pointer
	SET_SIZE(thread_onproc)

#endif	/* lint */
