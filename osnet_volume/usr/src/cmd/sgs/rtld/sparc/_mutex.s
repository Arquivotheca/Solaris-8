/*
 *	Copyright (c) 1991,1992 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)_mutex.s	1.5	92/08/31 SMI"


/*
 * This file impliments the system calls required to support mutex's.  Once
 * This stuff gets into libc we can get rid of this file (and SYS.h,
 * PIC.h ...).
 */
#if	!defined(lint)

#include <sys/asm_linkage.h>
#include "SYS.h"

	.file	"mutex.s"

/*
 * int
 * lwp_cond_broadcast(condvar_t * cvp)
 */
	SYSCALL(lwp_cond_broadcast)
	RET
	SET_SIZE(lwp_cond_broadcast)

/*
 * int
 * lwp_cond_signal(condvar_t * cvp)
 */
	SYSCALL(lwp_cond_signal)
	RET
	SET_SIZE(lwp_cond_signal)

/*
 * int
 * lwp_cond_wait(condvar_t * cvp, mutex_t * mp, struct timeval * tv)
 */
	SYSCALL(lwp_cond_wait);
	RET
	SET_SIZE(lwp_cond_wait)

/*
 * void
 * lwp_mutex_lock(mutex_t *)
 */
	SYSCALL_INTRRESTART(lwp_mutex_lock)
	RET
	SET_SIZE(lwp_mutex_lock)

/*
 * void
 * lwp_mutex_unlock(mutex_t *)
 */
	SYSCALL(lwp_mutex_unlock)
	RET
	SET_SIZE(lwp_mutex_unlock)

/*
 * lock_try(unsigned char *)
 */
	ENTRY(_lock_try)
	ldstub	[%o0], %o1
	retl
	xor	%o1, 0xff, %o0
	SET_SIZE(_lock_try)

/*
 * lock_clear(unsigned char *)
 */
	ENTRY(_lock_clear)
	retl
	clrb	[%o0]
	SET_SIZE(_lock_clear)


#else

/*
 * Define each routine, using all its arguments, just to shut lint up.
 */
#include <thread.h>

extern void	_halt();

int
lwp_cond_broadcast(condvar_t * cvp)
{
	return ((int)cvp->wanted);
}

int
lwp_cond_signal(condvar_t * cvp)
{
	return ((int)cvp->wanted);
}

int
lwp_cond_wait(condvar_t * cvp, mutex_t * mp, struct timeval * tv)
{
	return ((int)((int)cvp->wanted + (int)mp->wanted + (int)tv->tv_sec));
}

void
lwp_mutex_lock(mutex_t * mp)
{
	mp = NULL;
	_halt();
}

void
lwp_mutex_unlock(mutex_t * mp)
{
	mp = NULL;
}

int
_lock_try(unsigned char * lp)
{
	return ((int)*lp);
}

void
_lock_clear(unsigned char * lp)
{
	*lp = 0;
}

#endif
