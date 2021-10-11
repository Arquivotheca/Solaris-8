/*
 * Copyright (c) 1996-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)uadmin.c	1.20	99/10/19 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/swap.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/var.h>
#include <sys/uadmin.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <vm/seg_kmem.h>
#include <sys/modctl.h>
#include <sys/callb.h>
#include <sys/dumphdr.h>
#include <sys/debug.h>
#include <sys/ftrace.h>
#include <sys/cmn_err.h>
#include <sys/panic.h>
#include <sys/rce.h>

/*
 * Administrivia system call.
 */

#define	BOOTSTRLEN	256

extern ksema_t fsflush_sema;
kmutex_t ualock;

int
uadmin(int cmd, int fcn, uintptr_t mdep)
{
	int error = 0;
	int rv = 0;
	int locked = 0;
	char *bootstr = NULL;
	char bootstrbuf[BOOTSTRLEN + 1];
	size_t len;

	/*
	 * Check cmd arg (fcn is system dependent & defaulted in mdboot())
	 * if wrong.
	 */
	switch (cmd) {
	case A_SWAPCTL:		/* swapctl checks permissions itself */
		if (get_udatamodel() == DATAMODEL_NATIVE)
			error = swapctl(fcn, (void *)mdep, &rv);
#if defined(_SYSCALL32_IMPL)
		else
			error = swapctl32(fcn, (void *)mdep, &rv);
#endif /* _SYSCALL32_IMPL */
		return (error ? (set_errno(error)) : rv);

	case A_FTRACE:
	case A_SHUTDOWN:
	case A_REBOOT:
	case A_REMOUNT:
	case A_FREEZE:
	case A_DUMP:
		if (!suser(CRED()))
			return (set_errno(EPERM));
		break;

	default:
		return (set_errno(EINVAL));
	}

	switch (cmd) {
	case A_SHUTDOWN:
	case A_REBOOT:
	case A_DUMP:
		/*
		 * Copy in the boot string now.
		 * We will release our address space so we can't do it later.
		 */
		len = 0;
		if ((bootstr = (char *)mdep) != NULL &&
		    copyinstr(bootstr, bootstrbuf, BOOTSTRLEN, &len) == 0) {
			bootstrbuf[len] = 0;
			bootstr = bootstrbuf;
		} else {
			bootstr = NULL;
		}
		if (cmd == A_DUMP)
			break; /* Don't grab ualock if we're trying to panic */
		/* FALLTHROUGH */
	case A_REMOUNT:
		if (!mutex_tryenter(&ualock))
			return (0);
		locked = 1;
	}

	switch (cmd) {
	case A_SHUTDOWN:
	{
		register struct proc *p;
		struct vnode *exec_vp;

		/*
		 * Release (almost) all of our own resources.
		 */
		p = ttoproc(curthread);
		if ((error = exitlwps(0)) != 0)
			return (set_errno(error));
		mutex_enter(&p->p_lock);
		p->p_flag |= SNOWAIT;
		sigfillset(&p->p_ignore);
		curthread->t_lwp->lwp_cursig = 0;
		if (p->p_exec) {
			exec_vp = p->p_exec;
			p->p_exec = NULLVP;
			mutex_exit(&p->p_lock);
			VN_RELE(exec_vp);
		} else {
			mutex_exit(&p->p_lock);
		}
		closeall(P_FINFO(curproc));
		relvm();

		/*
		 * Kill all processes except kernel daemons and ourself.
		 * Make a first pass to stop all processes so they won't
		 * be trying to restart children as we kill them.
		 */
		mutex_enter(&pidlock);
		for (p = practive; p != NULL; p = p->p_next) {
			if (p->p_exec != NULLVP &&	/* kernel daemons */
			    p->p_as != &kas &&
			    p->p_stat != SZOMB) {
				mutex_enter(&p->p_lock);
				p->p_flag |= SNOWAIT;
				sigtoproc(p, NULL, SIGSTOP);
				mutex_exit(&p->p_lock);
			}
		}
		p = practive;
		while (p != NULL) {
			if (p->p_exec != NULLVP &&	/* kernel daemons */
			    p->p_as != &kas &&
			    p->p_stat != SIDL &&
			    p->p_stat != SZOMB) {
				mutex_enter(&p->p_lock);
				if (sigismember(&p->p_sig, SIGKILL)) {
					mutex_exit(&p->p_lock);
					p = p->p_next;
				} else {
					sigtoproc(p, NULL, SIGKILL);
					mutex_exit(&p->p_lock);
					(void) cv_timedwait(&p->p_srwchan_cv,
					    &pidlock, lbolt + hz);
					p = practive;
				}
			} else {
				p = p->p_next;
			}
		}
		mutex_exit(&pidlock);

		VN_RELE(u.u_cdir);
		if (u.u_rdir)
			VN_RELE(u.u_rdir);

		u.u_cdir = rootdir;
		u.u_rdir = NULL;

		/*
		 * Allow fsflush to finish running and then prevent it
		 * from ever running again so that vfs_unmountall() and
		 * vfs_syncall() can acquire the vfs locks they need.
		 */
		sema_p(&fsflush_sema);

		/*
		 * SRM hook: provided for flush of cached SRM accounting
		 * data to nonvolatile storage during uadmin shutdown.
		 * It is true that vfs_syncall() below calls sync() which
		 * calls vfs_sync() and hence another SRM_FLUSH() but by
		 * then it may be too late due to the vfs_unmountall().
		 * We don't seek to do a SRM_FLUSH() for a uadmin reboot
		 * because no vfs_sync is attempted either - this is in
		 * accord with the uadmin(2) man page. This hook is placed
		 * after the sema_p(&fsflush_sema) so that fsflush is
		 * suspended and this is now our last chance to flush data
		 * right up to the present.
		 */
		SRM_FLUSH(SH_NOW);

		vfs_unmountall();

		(void) VFS_MOUNTROOT(rootvfs, ROOT_UNMOUNT);

		vfs_syncall();

		(void) callb_execute_class(CB_CL_UADMIN, NULL);

		dump_messages();

		/* FALLTHROUGH */
	}

	case A_REBOOT:
		mdboot(cmd, fcn, bootstr);
		/* no return expected */
		break;

	case A_REMOUNT:
		/* remount root file system */
		(void) VFS_MOUNTROOT(rootvfs, ROOT_REMOUNT);
		break;

	case A_FREEZE:
	{
		/* XXX: declare in some header file */
		extern int cpr(int);

		if (modload("misc", "cpr") == -1)
			return (set_errno(ENOTSUP));

		/*
		 * SRM hook: for flush of cached SRM accounting	data to
		 * nonvolatile storage during uadmin shutdown.
		 * By passing SH_SUSPEND as an argument, SRM will flush the
		 * data and it will not flush again (even when SRM_FLUSH is
		 * called) until it sees another SRM_FLUSH with SH_RESUME
		 * as an argument. It is used to avoid a deadlock situation
		 * when suspending the system.
		 */
		SRM_FLUSH(SH_SUSPEND);

		error = cpr(fcn);

		SRM_FLUSH(SH_RESUME);

		return (error ? (set_errno(error)) : 0);
	}

	case A_FTRACE:
	{
		switch (fcn) {
		case AD_FTRACE_START:
			(void) FTRACE_START();
			return (0);
		case AD_FTRACE_STOP:
			(void) FTRACE_STOP();
			return (0);
		default:
			return (set_errno(EINVAL));
		}
		/*NOTREACHED*/
	}

	case A_DUMP:
	{
		if (fcn == AD_NOSYNC) {
			in_sync = 1;
			return (0);
		}

		panic_bootfcn = fcn;
		panic_bootstr = bootstr;
		panic_forced = 1;

		panic("forced crash dump initiated at user request");
		/*NOTREACHED*/
	}

	default:
		error = EINVAL;
	}

	if (locked)
		mutex_exit(&ualock);

	return (error ? (set_errno(error)) : 0);
}
