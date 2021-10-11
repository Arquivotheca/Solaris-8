/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rlimit.c	1.19	98/06/30 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/ulimit.h>
#include <sys/debug.h>
#include <vm/as.h>

/*
 * Perhaps ulimit could be moved into a user library, as calls to
 * getrlimit and setrlimit, were it not for binary compatibility
 * restrictions.
 */
long
ulimit(int cmd, long arg)
{
	proc_t *p = curproc;
	long	retval;

	switch (cmd) {

	case UL_GFILLIM: /* Return current file size limit. */
	{
		rlim64_t filesize;

		mutex_enter(&p->p_lock);
		filesize = P_CURLIMIT(p, RLIMIT_FSIZE);
		mutex_exit(&p->p_lock);

		if (get_udatamodel() == DATAMODEL_ILP32) {
			/*
			 * File size is returned in blocks for ulimit.
			 * This function is deprecated and therefore LFS API
			 * didn't define the behaviour of ulimit.
			 * Here we return maximum value of file size possible
			 * so that applications that do not check errors
			 * continue to work.
			 */
			if (filesize > MAXOFF32_T)
				filesize = MAXOFF32_T;
			retval = ((int)filesize >> SCTRSHFT);
		} else
			retval = filesize >> SCTRSHFT;
		break;
	}

	case UL_SFILLIM: /* Set new file size limit. */
	{
		int error = 0;
		rlim64_t lim = (rlim64_t)arg;

		if (lim >= (rlim_infinity_map[RLIMIT_FSIZE] >> SCTRSHFT))
			lim = (rlim64_t)RLIM64_INFINITY;
		else
			lim <<= SCTRSHFT;

		if (error = rlimit(RLIMIT_FSIZE, lim, lim)) {
			return (set_errno(error));
		}
		retval = arg;
		break;
	}

	case UL_GMEMLIM: /* Return maximum possible break value. */
	{
		struct seg *seg;
		struct seg *nextseg;
		struct as *as = p->p_as;
		caddr_t brkend;
		caddr_t brkbase;
		size_t size;

		/*
		 * Find the segment with a virtual address
		 * greater than the end of the current break.
		 */
		nextseg = NULL;
		mutex_enter(&p->p_lock);
		brkbase = (caddr_t)p->p_brkbase;
		brkend = (caddr_t)p->p_brkbase + p->p_brksize;
		mutex_exit(&p->p_lock);

		/*
		 * Since we can't return less than the current break,
		 * initialize the return value to the current break
		 */
		retval = (long)brkend;

		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
		for (seg = as_findseg(as, brkend, 0); seg != NULL;
		    seg = AS_SEGP(as, seg->s_next)) {
			if (seg->s_base >= brkend) {
				nextseg = seg;
				break;
			}
		}

		/*
		 * First, calculate the maximum break value based on
		 * the user's RLIMIT_DATA, but also taking into account
		 * that this value cannot be greater than as->a_userlimit.
		 * We also take care to make sure that we don't overflow
		 * in the calculation
		 */

		/*
		 * Since we are casting the RLIMIT_DATA value to a
		 * ulong (a 32-bit value in the 32-bit kernel) we have
		 * to pass this assertion.
		 */
		ASSERT32(P_CURLIMIT(p, RLIMIT_DATA) <= UINT32_MAX);

		size = (size_t)P_CURLIMIT(p, RLIMIT_DATA);
		if (as->a_userlimit - brkbase > size)
			retval = MAX((size_t)retval, (size_t)(brkbase + size));
					/* don't return less than current */
		else
			retval = (long)as->a_userlimit;

		/*
		 * The max break cannot extend into the next segment
		 */
		if (nextseg != NULL)
			retval = MIN((uintptr_t)retval,
			    (uintptr_t)nextseg->s_base);

		/*
		 * Handle the case where there is an limit on RLIMIT_VMEM
		 */
		if (!UNLIMITED_CUR(p, RLIMIT_VMEM)) {
			/*
			 * Large Files: The following assertion has to pass
			 * through to ensure the correctness of the cast.
			 */
			ASSERT32(P_CURLIMIT(p, RLIMIT_VMEM) <= UINT32_MAX);

			size = (size_t)(P_CURLIMIT(p, RLIMIT_VMEM) & PAGEMASK);

			if (as->a_size < size)
				size -= as->a_size;
			else
				size = 0;
			/*
			 * Take care to not overflow the calculation
			 */
			if (as->a_userlimit - brkend > size)
				retval = MIN((size_t)retval,
				    (size_t)(brkend + size));
		}

		AS_LOCK_EXIT(as, &as->a_lock);

		/* truncate to same boundary as sbrk */

		switch (get_udatamodel()) {
		default:
		case DATAMODEL_ILP32:
			retval = retval & ~(8-1);
			break;
		case DATAMODEL_LP64:
			retval = retval & ~(16-1);
			break;
		}
		break;
	}

	case UL_GDESLIM: /* Return approximate number of open files */
		ASSERT(P_CURLIMIT(p, RLIMIT_NOFILE) <= INT_MAX);
		retval = (rlim_t)P_CURLIMIT(p, RLIMIT_NOFILE);
		break;

	default:
		return (set_errno(EINVAL));

	}
	return (retval);
}

#ifdef _SYSCALL32_IMPL

int
ulimit32(int cmd, int arg)
{
	return ((int)ulimit(cmd, (long)arg));
}

#endif	/* _SYSCALL32_IMPL */

#if defined(_ILP32) || defined(_SYSCALL32_IMPL)

/*
 * Large Files: getrlimit returns RLIM_SAVED_CUR or RLIM_SAVED_MAX when
 * rlim_cur or rlim_max is not representable in 32-bit rlim_t. These
 * values are just tokens which will be used in setrlimit to set the
 * correct limits. The current limits are saved in the saved_rlimit members
 * in user structures when the token is returned. setrlimit restores
 * the limit values to these saved values when the token is passed.
 * Consider the following common scenario of the apps:
 *
 * 		limit = getrlimit();
 *		savedlimit = limit;
 * 		limit = limit1;
 *		setrlimit(limit)
 *		// execute all processes in the new rlimit state.
 *		setrlimit(savedlimit) // restore the old values.
 *
 * Most apps don't check error returns from getrlimit or setrlimit
 * and this is why we return tokens when the correct value
 * cannot be represented in rlim_t. For more discussion refer to
 * the LFS API document.
 *
 * An additional problem in the 64-bit world is that most of the VM
 * related variables have the same problem.  So the system has been
 * generalized to deal with -all- limits in the same way.
 */
int
getrlimit32(int resource, struct rlimit32 *rlp)
{
	struct rlimit32 rlim32;
	struct rlimit64 rlim64;
	struct proc *p = curproc;
	struct user *up = PTOU(p);
	int savecur = 0;
	int savemax = 0;

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return (set_errno(EINVAL));

#ifdef _LP64
	rlim64 = up->u_rlimit[resource];
#else
	mutex_enter(&p->p_lock);
	rlim64 = up->u_rlimit[resource];
	mutex_exit(&p->p_lock);
#endif

	if (rlim64.rlim_max > (rlim64_t)UINT32_MAX) {

		if (rlim64.rlim_max == RLIM64_INFINITY)
			rlim32.rlim_max = RLIM32_INFINITY;
		else {
			savemax = 1;
			rlim32.rlim_max = RLIM32_SAVED_MAX;
			ASSERT32(resource == RLIMIT_FSIZE);
		}

		if (rlim64.rlim_cur == RLIM64_INFINITY)
			rlim32.rlim_cur = RLIM32_INFINITY;
		else if (rlim64.rlim_cur == rlim64.rlim_max) {
			savecur = 1;
			rlim32.rlim_cur = RLIM32_SAVED_MAX;
			ASSERT32(resource == RLIMIT_FSIZE);
		} else if (rlim64.rlim_cur > (rlim64_t)UINT32_MAX) {
			savecur = 1;
			rlim32.rlim_cur = RLIM32_SAVED_CUR;
			ASSERT32(resource == RLIMIT_FSIZE);
		} else
			rlim32.rlim_cur = rlim64.rlim_cur;

		/*
		 * save the current limits in user structure.
		 */
#ifdef _LP64
		mutex_enter(&p->p_lock);
		if (savemax)
			up->u_saved_rlimit[resource].rlim_max = rlim64.rlim_max;
		if (savecur)
			up->u_saved_rlimit[resource].rlim_cur = rlim64.rlim_cur;
		mutex_exit(&p->p_lock);
#else
		if (resource == RLIMIT_FSIZE) {
			mutex_enter(&p->p_lock);
			if (savemax)
				up->u_saved_lf_rlimit.rlim_max =
				    rlim64.rlim_max;
			if (savecur)
				up->u_saved_lf_rlimit.rlim_cur =
				    rlim64.rlim_cur;
			mutex_exit(&p->p_lock);
		}
#endif
	} else {
		ASSERT(rlim64.rlim_cur <= (rlim64_t)UINT32_MAX);
		rlim32.rlim_max = rlim64.rlim_max;
		rlim32.rlim_cur = rlim64.rlim_cur;
	}

	if (copyout(&rlim32, rlp, sizeof (rlim32)))
		return (set_errno(EFAULT));

	return (0);
}

/*
 * See comments above getrlimit32(). When the tokens are passed in the
 * rlimit structure the values are considered equal to the values
 * stored in saved_rlimit members of user structure.
 * When the user passes RLIM_INFINITY to set the resource limit to
 * unlimited internally understand this value as RLIM64_INFINITY and
 * let rlimit() do the job.
 */
int
setrlimit32(int resource, struct rlimit32 *rlp)
{
	struct rlimit32 rlim32;
	struct rlimit64 rlim64;
	struct rlimit64 saved_rlim;
	int	error;
	struct proc *p = ttoproc(curthread);
	struct user *up = PTOU(p);

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return (set_errno(EINVAL));
	if (copyin(rlp, &rlim32, sizeof (rlim32)))
		return (set_errno(EFAULT));

#ifdef _LP64
	saved_rlim = up->u_saved_rlimit[resource];
#else
	/*
	 * Disallow resource limit tunnelling
	 */
	if (resource == RLIMIT_FSIZE) {
		mutex_enter(&p->p_lock);
		saved_rlim = up->u_saved_lf_rlimit;
		mutex_exit(&p->p_lock);
	} else {
		saved_rlim.rlim_max = (rlim64_t)rlim32.rlim_max;
		saved_rlim.rlim_cur = (rlim64_t)rlim32.rlim_cur;
	}
#endif

	switch (rlim32.rlim_cur) {
	case RLIM32_INFINITY:
		rlim64.rlim_cur = RLIM64_INFINITY;
		break;
	case RLIM32_SAVED_CUR:
		rlim64.rlim_cur = saved_rlim.rlim_cur;
		break;
	case RLIM32_SAVED_MAX:
		rlim64.rlim_cur = saved_rlim.rlim_max;
		break;
	default:
		rlim64.rlim_cur = (rlim64_t)rlim32.rlim_cur;
		break;
	}

	switch (rlim32.rlim_max) {
	case RLIM32_INFINITY:
		rlim64.rlim_max = RLIM64_INFINITY;
		break;
	case RLIM32_SAVED_MAX:
		rlim64.rlim_max = saved_rlim.rlim_max;
		break;
	case RLIM32_SAVED_CUR:
		rlim64.rlim_max = saved_rlim.rlim_cur;
		break;
	default:
		rlim64.rlim_max = (rlim64_t)rlim32.rlim_max;
		break;
	}

	if (error = rlimit(resource, rlim64.rlim_cur, rlim64.rlim_max))
		return (set_errno(error));
	return (0);
}

#endif	/* _ILP32 && _SYSCALL32_IMPL */

int
getrlimit64(int resource, struct rlimit64 *rlp)
{
	struct rlimit64 rlim64;
	struct proc *p = ttoproc(curthread);
	struct user *up = PTOU(p);

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return (set_errno(EINVAL));

#ifdef _LP64
	rlim64 = up->u_rlimit[resource];
#else
	mutex_enter(&p->p_lock);
	rlim64 = up->u_rlimit[resource];
	mutex_exit(&p->p_lock);
#endif

	if (copyout(&rlim64, rlp, sizeof (rlim64)))
		return (set_errno(EFAULT));
	return (0);
}

int
setrlimit64(int resource, struct rlimit64 *rlp)
{
	struct rlimit64 rlim64;
	int	error;

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return (set_errno(EINVAL));
	if (copyin(rlp, &rlim64, sizeof (rlim64)))
		return (set_errno(EFAULT));
	if (error = rlimit(resource, rlim64.rlim_cur, rlim64.rlim_max))
		return (set_errno(error));
	return (0);
}
