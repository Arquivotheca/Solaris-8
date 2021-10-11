/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_mutex.s	1.11	98/06/05 SMI"
        .file "_mutex.s"
 
#include <sys/asm_linkage.h>
 
#include <sys/synch32.h>
#include <sys/errno.h>
#include <sys/synch.h>
#include "assym.h"

	ANSI_PRAGMA_WEAK(mutex_lock,function);
	ANSI_PRAGMA_WEAK(mutex_unlock,function);
	ANSI_PRAGMA_WEAK(mutex_trylock,function);

	.weak   _ti_mutex_lock;
	.type   _ti_mutex_lock, #function
	_ti_mutex_lock = _mutex_lock
	.weak   _ti_mutex_unlock;
	.type   _ti_mutex_unlock, #function
	_ti_mutex_unlock = _mutex_unlock
	.weak   _ti_mutex_trylock;
	.type   _ti_mutex_trylock, #function
	_ti_mutex_trylock = _mutex_trylock

/*
 * Returns > 0 if there are waiters for this lock.
 * Returns 0 if there are no waiters for this lock.
 * Could seg-fault if the lock memory which contains waiter info is freed.
 * The seg-fault is handled by libthread and the PC is advanced beyond faulting
 * instruction.
 *
 * int
 * _mutex_unlock_asm (mp)
 *      mutex_t *mp;
 */
	ENTRY(_mutex_unlock_asm)
	clr	%o5
	stn	%g0, [%o0 + MUTEX_OWNER]	! clear owner
	membar	#LoadStore|#StoreStore
	swap	[%o0 + MUTEX_LOCK], %o5		! clear lock/get waiter field
						! atomically.
	and	%o5, WAITER_MASK, %o5		! check for waiters
	retl
	mov	%o5, %o0			! return waiters into %o0
	SET_SIZE(_mutex_unlock_asm)

/*
 * _lock_try_adaptive(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 */
	ENTRY(_lock_try_adaptive)
	ldstub	[%o0 + MUTEX_LOCK], %o1		! try lock
	membar	#LoadLoad
	tst	%o1				! did we get it?
	bz,a	1f				! yes
	stn	%g7, [%o0 + MUTEX_OWNER]	! delay (annulled) - set owner
1:
	retl					! return
	xor	%o1, 0xff, %o0			! delay - set return value
	SET_SIZE(_lock_try_adaptive)

/*
 * _lock_clear_adaptive(mutex_t *mp)
 *
 * Clear lock and owner. We could also check the owner here.
 */
	ENTRY(_lock_clear_adaptive)
	membar	#LoadStore|#StoreStore
	stn	%g0, [%o0 + MUTEX_OWNER]	! clear owner
	retl					! return
	clrb	[%o0 + MUTEX_LOCK]		! clear lock
	SET_SIZE(_lock_clear_adaptive)

/*
 * _lock_owner(mutex_t *mp)
 *
 * Return the thread pointer of the owner of the mutex.
 */
	ENTRY(_lock_owner)
	retl
	ldn	[%o0 + MUTEX_OWNER], %o0	! delay - get owner
	SET_SIZE(_lock_owner)

/*
 * _lock_held(mutex_t *mp)
 *
 * Return lock byte
 */
	ENTRY(_lock_held)
	retl
	ldub	[%o0 + MUTEX_LOCK], %o0
	SET_SIZE(_lock_held)

/*
 * int
 * _mutex_unlock(mp)
 *      mutex_t *mp;
 */
        ENTRY(_mutex_unlock)
	ldub	[%o0 + MUTEX_TYPE], %o1
	andcc	%o1, USYNC_PROCESS_ROBUST, %g0 
	be,a	1f
	clr     %o5
	ba	2f
	mov	%o7, %o3			! save return address
1:	stn     %g0, [%o0 + MUTEX_OWNER]        ! clear owner
	membar  #LoadStore|#StoreStore
	swap    [%o0 + MUTEX_LOCK], %o5         ! clear lock/get waiter field
	and     %o5, WAITER_MASK, %o5           ! check for waiters
	tst	%o5
	bz	3f
	nop
	mov	%o7, %o3			! save return address
	call	_mutex_wakeup
	mov	%o3, %o7			! restore return address
2:	call	_mutex_unlock_robust
	mov	%o3, %o7			! restore return address
3:	retl                            	! return
	mov	%g0, %o0
        SET_SIZE(_mutex_unlock)

/*
 * _mutex_lock(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 */
	ENTRY(_mutex_lock)
	ldub	[%o0 + MUTEX_TYPE], %o1
	andcc	%o1, USYNC_PROCESS_ROBUST, %g0 
	be,a	1f
	ldstub  [%o0 + MUTEX_LOCK], %o1         ! try lock
	ba	2f	
	mov	%o7, %o3			! save return address
1:	membar  #LoadLoad
	tst     %o1                             ! did we get it?
	bz,a    3f                              ! yes
	stn     %g7, [%o0 + MUTEX_OWNER]        ! delay (annulled) - set owner
	mov	%o7, %o3			! save return address
2:	call	_cmutex_lock
	mov	%o3, %o7			! restore return address
3:	retl
	mov     %g0, %o0
	SET_SIZE(_mutex_lock)


/*
 * _mutex_trylock(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 */
	ENTRY(_mutex_trylock)
	ldub	[%o0 + MUTEX_TYPE], %o1
	andcc	%o1, USYNC_PROCESS_ROBUST, %g0 
	be,a	1f
	ldstub	[%o0 + MUTEX_LOCK], %o1		! try lock
	ba	2f	
	mov	%o7, %o3			! save return address
1:	membar  #LoadLoad
	tst	%o1				! did we get it?
	bz,a	3f				! yes
	stn	%g7, [%o0 + MUTEX_OWNER]	! delay (annulled) - set owner
	retl					! return
	mov     EBUSY, %o0
2:	call	_mutex_trylock_robust
	mov	%o3, %o7			! restore return address
3:	retl
	mov     %g0, %o0
	SET_SIZE(_mutex_trylock)
