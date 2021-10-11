/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sigaltstack.c	1.5	96/10/08 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/fault.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/debug.h>

sigaltstack(struct sigaltstack *ssp, struct sigaltstack *oss)
{
	register klwp_t *lwp = ttolwp(curthread);
	struct sigaltstack ss;

	/*
	 * User's oss and ss might be the same address, so copyin first and
	 * save before copying out.
	 */
	if (ssp) {
		if (lwp->lwp_sigaltstack.ss_flags & SS_ONSTACK)
			return (set_errno(EPERM));
		if (copyin((caddr_t)ssp, (caddr_t)&ss, sizeof (ss)))
			return (set_errno(EFAULT));
		if (ss.ss_flags & ~SS_DISABLE)
			return (set_errno(EINVAL));
		if (!(ss.ss_flags & SS_DISABLE) && ss.ss_size < MINSIGSTKSZ)
			return (set_errno(ENOMEM));
	}

	if (oss) {
		if (copyout((caddr_t)&lwp->lwp_sigaltstack,
		    (caddr_t)oss, sizeof (struct sigaltstack)))
			return (set_errno(EFAULT));
	}

	if (ssp)
		lwp->lwp_sigaltstack = ss;

	return (0);
}

#ifdef _LP64
int
sigaltstack32(struct sigaltstack32 *ssp, struct sigaltstack32 *oss)
{
	register klwp_t *lwp = ttolwp(curthread);
	struct sigaltstack   *ss;
	struct sigaltstack32 ss32, oss32;


	/*
	 * User's oss and ss might be the same address, so copyin first and
	 * save before copying out.
	 */
	if (ssp) {
		if (lwp->lwp_sigaltstack.ss_flags & SS_ONSTACK)
			return (set_errno(EPERM));
		if (copyin((caddr_t)ssp, (caddr_t)&ss32, sizeof (ss32)))
			return (set_errno(EFAULT));
		if (ss32.ss_flags & ~SS_DISABLE)
			return (set_errno(EINVAL));
		if (!(ss32.ss_flags & SS_DISABLE) && ss32.ss_size < MINSIGSTKSZ)
			return (set_errno(ENOMEM));
	}

	if (oss) {
		/*
		 * copy to ILP32 struct before copyout.
		 */
		ss = &lwp->lwp_sigaltstack;
		oss32.ss_sp    = (caddr32_t)ss->ss_sp;
		oss32.ss_size  = (size32_t)ss->ss_size;
		oss32.ss_flags = ss->ss_flags;

		if (copyout((caddr_t)&oss32, (caddr_t)oss, sizeof (oss32)))
			return (set_errno(EFAULT));
	}

	if (ssp) {
		ss = &lwp->lwp_sigaltstack;
		ss->ss_sp = (void *)ss32.ss_sp;
		ss->ss_size = (size_t)ss32.ss_size;
		ss->ss_flags = ss32.ss_flags;
	}

	return (0);
}
#endif /* _LP64 */
