/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

        .ident  "@(#)_mutex.s 1.4     98/04/12 SMI"
 
#include <sys/asm_linkage.h>
#include <assym.s>
 
        ENTRY(_mutex_unlock_asm)
        movl    4(%esp),%eax
	movl	%eax,%ecx
	addl	$MUTEX_OWNER,%ecx		/ clear owner
	movl	$0,(%ecx)
        addl    $M_LOCK_WORD,%eax
        xorl    %ecx,%ecx
        xchgl   (%eax),%ecx     / clear lock and get old lock into %ecx
        andl    $M_WAITER_MASK,%ecx / was anyone waiting on it?
        movl    %ecx, %eax
        ret
        .size   _mutex_unlock_asm,.-_mutex_unlock_asm

/*
 * _lock_try_adaptive(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 */
	ENTRY(_lock_try_adaptive)
	movl	$1, %eax
	movl	4(%esp), %ecx			/ get mutex addr
	movl	%ecx, %edx
	addl	$MUTEX_LOCK, %ecx		/ get lock addr
	xchgb	%al, (%ecx)			/ try to acquire lock
	xorb	$1, %al				/ 1=failed, 0=success
	je	L1
	movw	%gs, %cx
	testw	%cx, %cx			/ is selector null?
	jz	L1				/ if so, skip setting owner
	movl	%gs:0, %ecx
	movl	%ecx, MUTEX_OWNER(%edx)		/ if succeeded, set owner
L1:
	ret
	SET_SIZE(_lock_try_adaptive)

/*
 * _lock_clear_adaptive(mutex_t *mp)
 *
 * Clear lock and owner. We could also check the owner here.
 */
	ENTRY(_lock_clear_adaptive)
	movl	4(%esp), %eax			/ get mutex addr
	movl	$0, MUTEX_OWNER(%eax)		/ clear owner
	movb	$0, MUTEX_LOCK(%eax)		/ clear lock
	ret
	SET_SIZE(_lock_clear_adaptive)

/*
 * _lock_owner(mutex_t *mp)
 *
 * Return the thread pointer of the owner of the mutex.
 */
	ENTRY(_lock_owner)
	movl	4(%esp), %eax			/ get mutex addr
	movl	MUTEX_OWNER(%eax), %eax		/ get owner
	ret
	SET_SIZE(_lock_owner)

/*
 * _lock_held(mutex_t *mp)
 *
 * Return the lock word
 */
	ENTRY(_lock_held)
	movl	4(%esp), %ecx			/ get mutex addr
	xorl	%eax, %eax
	movb	MUTEX_LOCK(%ecx), %al		/ get owner
	ret
	SET_SIZE(_lock_held)

