/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)times.c	1.4	97/08/12 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/debug.h>

/*
 * Return system and user times.
 */

clock_t
times(struct tms *tp)
{
	proc_t *p = ttoproc(curthread);
	struct tms	p_time;
	clock_t ret_lbolt;

	mutex_enter(&p->p_lock);
	p_time.tms_utime = p->p_utime;
	p_time.tms_stime = p->p_stime;
	p_time.tms_cutime = p->p_cutime;
	p_time.tms_cstime = p->p_cstime;
	mutex_exit(&p->p_lock);

	if (copyout(&p_time, tp, sizeof (p_time)))
		return (set_errno(EFAULT));

	ret_lbolt = lbolt;

	return (ret_lbolt == -1 ? 0 : ret_lbolt);
}

#ifdef _SYSCALL32_IMPL

/*
 * We deliberately -don't- return EOVERFLOW on type overflow,
 * since the 32-bit kernel simply wraps 'em around.
 */
clock32_t
times32(struct tms32 *tp)
{
	proc_t	*p = ttoproc(curthread);
	struct tms32	p_time;
	clock32_t	ret_lbolt;

	mutex_enter(&p->p_lock);
	p_time.tms_utime = (clock32_t)p->p_utime;
	p_time.tms_stime = (clock32_t)p->p_stime;
	p_time.tms_cutime = (clock32_t)p->p_cutime;
	p_time.tms_cstime = (clock32_t)p->p_cstime;
	mutex_exit(&p->p_lock);

	if (copyout(&p_time, tp, sizeof (p_time)))
		return (set_errno(EFAULT));

	ret_lbolt = (clock32_t)lbolt;

	return (ret_lbolt == (clock32_t)-1 ? 0 : ret_lbolt);
}

#endif	/* _SYSCALL32_IMPL */
