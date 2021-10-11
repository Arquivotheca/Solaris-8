/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prcontrol.c	1.28	99/09/14 SMI"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/regset.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/signal.h>
#include <sys/auxv.h>
#include <sys/user.h>
#include <sys/class.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/copyops.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <fs/proc/prdata.h>

static	void	pr_settrace(proc_t *, sigset_t *);
static	int	pr_setfpregs(prnode_t *, prfpregset_t *);
#if defined(sparc) || defined(__sparc)
static	int	pr_setxregs(prnode_t *, prxregset_t *);
#endif
#if defined(__sparcv9)
static	int	pr_setasrs(prnode_t *, asrset_t);
#endif
static	int	pr_setvaddr(prnode_t *, caddr_t);
static	int	pr_clearsig(prnode_t *);
static	int	pr_clearflt(prnode_t *);
static	int	pr_watch(prnode_t *, prwatch_t *, int *);
static	int	pr_agent(prnode_t *, prgregset_t, int *);
static	int	pr_rdwr(proc_t *, enum uio_rw, priovec_t *);
static	int	pr_scred(proc_t *, prcred_t *, cred_t *);
static	void	pauselwps(proc_t *);
static	void	unpauselwps(proc_t *);

typedef union {
	long		sig;		/* PCKILL, PCUNKILL */
	long		nice;		/* PCNICE */
	long		timeo;		/* PCTWSTOP */
	ulong_t		flags;		/* PCRUN, PCSET, PCUNSET */
	caddr_t		vaddr;		/* PCSVADDR */
	siginfo_t	siginfo;	/* PCSSIG */
	sigset_t	sigset;		/* PCSTRACE, PCSHOLD */
	fltset_t	fltset;		/* PCSFAULT */
	sysset_t	sysset;		/* PCSENTRY, PCSEXIT */
	prgregset_t	prgregset;	/* PCSREG, PCAGENT */
	prfpregset_t	prfpregset;	/* PCSFPREG */
#if defined(sparc) || defined(__sparc)
	prxregset_t	prxregset;	/* PCSXREG */
#if defined(__sparcv9)
	asrset_t	asrset;		/* PCSASRS */
#endif
#endif
	prwatch_t	prwatch;	/* PCWATCH */
	priovec_t	priovec;	/* PCREAD, PCWRITE */
	prcred_t	prcred;		/* PCSCRED */
} arg_t;

static	int	pr_control(long, arg_t *, prnode_t *, cred_t *);

static size_t
ctlsize(long cmd, size_t resid)
{
	size_t size = sizeof (long);

	switch (cmd) {
	case PCNULL:
	case PCSTOP:
	case PCDSTOP:
	case PCWSTOP:
	case PCCSIG:
	case PCCFAULT:
		break;
	case PCSSIG:
		size += sizeof (siginfo_t);
		break;
	case PCTWSTOP:
		size += sizeof (long);
		break;
	case PCKILL:
	case PCUNKILL:
	case PCNICE:
		size += sizeof (long);
		break;
	case PCRUN:
	case PCSET:
	case PCUNSET:
		size += sizeof (ulong_t);
		break;
	case PCSVADDR:
		size += sizeof (caddr_t);
		break;
	case PCSTRACE:
	case PCSHOLD:
		size += sizeof (sigset_t);
		break;
	case PCSFAULT:
		size += sizeof (fltset_t);
		break;
	case PCSENTRY:
	case PCSEXIT:
		size += sizeof (sysset_t);
		break;
	case PCSREG:
	case PCAGENT:
		size += sizeof (prgregset_t);
		break;
	case PCSFPREG:
		size += sizeof (prfpregset_t);
		break;
#if defined(sparc) || defined(__sparc)
	case PCSXREG:
		size += sizeof (prxregset_t);
		break;
#if defined(__sparcv9)
	case PCSASRS:
		size += sizeof (asrset_t);
		break;
#endif
#endif
	case PCWATCH:
		size += sizeof (prwatch_t);
		break;
	case PCREAD:
	case PCWRITE:
		size += sizeof (priovec_t);
		break;
	case PCSCRED:
		size += sizeof (prcred_t);
		break;
	default:
		return (0);
	}

	if (size > resid)
		return (0);
	return (size);
}

/*
 * Control operations (lots).
 */
int
prwritectl(vnode_t *vp, uio_t *uiop, cred_t *cr)
{
#define	MY_BUFFER_SIZE \
		100 > 1 + sizeof (arg_t) / sizeof (long) ? \
		100 : 1 + sizeof (arg_t) / sizeof (long)
	long buf[MY_BUFFER_SIZE];
	long *bufp;
	size_t resid = 0;
	size_t size;
	prnode_t *pnp = VTOP(vp);
	int error;
	int locked = 0;

	while (uiop->uio_resid) {
		/*
		 * Read several commands in one gulp.
		 */
		bufp = buf;
		if (resid) {	/* move incomplete command to front of buffer */
			long *tail;

			if (resid >= sizeof (buf))
				break;
			tail = (long *)((char *)buf + sizeof (buf) - resid);
			do {
				*bufp++ = *tail++;
			} while ((resid -= sizeof (long)) != 0);
		}
		resid = sizeof (buf) - ((char *)bufp - (char *)buf);
		if (resid > uiop->uio_resid)
			resid = uiop->uio_resid;
		if (error = uiomove((caddr_t)bufp, resid, UIO_WRITE, uiop))
			return (error);
		resid += (char *)bufp - (char *)buf;
		bufp = buf;

		do {		/* loop over commands in buffer */
			long cmd = bufp[0];
			arg_t *argp = (arg_t *)&bufp[1];

			size = ctlsize(cmd, resid);
			if (size == 0)	/* incomplete or invalid command */
				break;
			/*
			 * Perform the specified control operation.
			 */
			if (!locked) {
				if ((error = prlock(pnp, ZNO)) != 0)
					return (error);
				locked = 1;
			}
			if (error = pr_control(cmd, argp, pnp, cr)) {
				if (error == -1)	/* -1 is timeout */
					locked = 0;
				else
					return (error);
			}
			bufp = (long *)((char *)bufp + size);
		} while ((resid -= size) != 0);

		if (locked) {
			prunlock(pnp);
			locked = 0;
		}
	}
	return (resid? EINVAL : 0);
}

static int
pr_control(long cmd, arg_t *argp, prnode_t *pnp, cred_t *cr)
{
	prcommon_t *pcp;
	proc_t *p;
	int unlocked;
	int error = 0;

	if (cmd == PCNULL)
		return (0);

	pcp = pnp->pr_common;
	p = pcp->prc_proc;
	ASSERT(p != NULL);

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	case PCSTOP:	/* direct process or lwp to stop and wait for stop */
	case PCDSTOP:	/* direct process or lwp to stop, don't wait */
	case PCWSTOP:	/* wait for process or lwp to stop */
	case PCTWSTOP:	/* wait for process or lwp to stop, with timeout */
	    {
		time_t timeo;

		/*
		 * Can't apply to a system process.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			error = EBUSY;
			break;
		}

		if (cmd == PCSTOP || cmd == PCDSTOP)
			pr_stop(pnp);

		if (cmd == PCDSTOP)
			break;

		/*
		 * If an lwp is waiting for itself or its process, don't wait.
		 * The stopped lwp would never see the fact that it is stopped.
		 */
		if ((pcp->prc_flags & PRC_LWP)?
		    (pcp->prc_thread == curthread) : (p == curproc)) {
			if (cmd == PCWSTOP || cmd == PCTWSTOP)
				error = EBUSY;
			break;
		}

		timeo = (cmd == PCTWSTOP)? (time_t)argp->timeo : 0;
		if ((error = pr_wait_stop(pnp, timeo)) != 0)
			return (error);

		break;
	    }

	case PCRUN:	/* make lwp or process runnable */
		error = pr_setrun(pnp, argp->flags);
		break;

	case PCSTRACE:	/* set signal trace mask */
		pr_settrace(p,  &argp->sigset);
		break;

	case PCSSIG:	/* set current signal */
		error = pr_setsig(pnp, &argp->siginfo);
		if (argp->siginfo.si_signo == SIGKILL && error == 0) {
			prunlock(pnp);
			pr_wait_die(pnp);
			return (-1);
		}
		break;

	case PCKILL:	/* send signal */
		error = pr_kill(pnp, (int)argp->sig, cr);
		if (error == 0 && argp->sig == SIGKILL) {
			prunlock(pnp);
			pr_wait_die(pnp);
			return (-1);
		}
		break;

	case PCUNKILL:	/* delete a pending signal */
		error = pr_unkill(pnp, (int)argp->sig);
		break;

	case PCNICE:	/* set nice priority */
		error = pr_nice(p, (int)argp->nice, cr);
		break;

	case PCSENTRY:	/* set syscall entry bit mask */
	case PCSEXIT:	/* set syscall exit bit mask */
		pr_setentryexit(p, &argp->sysset, cmd == PCSENTRY);
		break;

	case PCSET:	/* set process flags */
		error = pr_set(p, argp->flags);
		break;

	case PCUNSET:	/* unset process flags */
		error = pr_unset(p, argp->flags);
		break;

	case PCSREG:	/* set general registers */
	    {
		kthread_t *t = pr_thread(pnp);

		if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
			thread_unlock(t);
			error = EBUSY;
		} else {
			thread_unlock(t);
			mutex_exit(&p->p_lock);
			prsetprregs(ttolwp(t), argp->prgregset, 0);
			mutex_enter(&p->p_lock);
		}
		break;
	    }

	case PCSFPREG:	/* set floating-point registers */
		error = pr_setfpregs(pnp, &argp->prfpregset);
		break;

	case PCSXREG:	/* set extra registers */
#if defined(sparc) || defined(__sparc)
		error = pr_setxregs(pnp, &argp->prxregset);
#else
		error = EINVAL;
#endif
		break;

#if defined(__sparcv9)
	case PCSASRS:	/* set ancillary state registers */
		error = pr_setasrs(pnp, argp->asrset);
		break;
#endif

	case PCSVADDR:	/* set virtual address at which to resume */
		error = pr_setvaddr(pnp, argp->vaddr);
		break;

	case PCSHOLD:	/* set signal-hold mask */
		pr_sethold(pnp, &argp->sigset);
		break;

	case PCSFAULT:	/* set mask of traced faults */
		pr_setfault(p, &argp->fltset);
		break;

	case PCCSIG:	/* clear current signal */
		error = pr_clearsig(pnp);
		break;

	case PCCFAULT:	/* clear current fault */
		error = pr_clearflt(pnp);
		break;

	case PCWATCH:	/* set or clear watched areas */
		error = pr_watch(pnp, &argp->prwatch, &unlocked);
		if (error && unlocked)
			return (error);
		break;

	case PCAGENT:	/* create the /proc agent lwp in the target process */
		error = pr_agent(pnp, argp->prgregset, &unlocked);
		if (error && unlocked)
			return (error);
		break;

	case PCREAD:	/* read from the address space */
		error = pr_rdwr(p, UIO_READ, &argp->priovec);
		break;

	case PCWRITE:	/* write to the address space */
		error = pr_rdwr(p, UIO_WRITE, &argp->priovec);
		break;

	case PCSCRED:	/* set the process credentials */
		error = pr_scred(p, &argp->prcred, cr);
		break;

	}

	if (error)
		prunlock(pnp);
	return (error);
}

#ifdef _SYSCALL32_IMPL

typedef union {
	int32_t		sig;		/* PCKILL, PCUNKILL */
	int32_t		nice;		/* PCNICE */
	int32_t		timeo;		/* PCTWSTOP */
	uint32_t	flags;		/* PCRUN, PCSET, PCUNSET */
	caddr32_t	vaddr;		/* PCSVADDR */
	siginfo32_t	siginfo;	/* PCSSIG */
	sigset_t	sigset;		/* PCSTRACE, PCSHOLD */
	fltset_t	fltset;		/* PCSFAULT */
	sysset_t	sysset;		/* PCSENTRY, PCSEXIT */
	prgregset32_t	prgregset;	/* PCSREG, PCAGENT */
	prfpregset32_t	prfpregset;	/* PCSFPREG */
#if defined(sparc) || defined(__sparc)
	prxregset_t	prxregset;	/* PCSXREG */
#endif
	prwatch32_t	prwatch;	/* PCWATCH */
	priovec32_t	priovec;	/* PCREAD, PCWRITE */
	prcred32_t	prcred;		/* PCSCRED */
} arg32_t;

static	int	pr_control32(int32_t, arg32_t *, prnode_t *, cred_t *);
static	int	pr_setfpregs32(prnode_t *, prfpregset32_t *);

static size_t
ctlsize32(int32_t cmd, size_t resid)
{
	size_t size = sizeof (int32_t);

	switch (cmd) {
	case PCNULL:
	case PCSTOP:
	case PCDSTOP:
	case PCWSTOP:
	case PCCSIG:
	case PCCFAULT:
		break;
	case PCSSIG:
		size += sizeof (siginfo32_t);
		break;
	case PCTWSTOP:
		size += sizeof (int32_t);
		break;
	case PCKILL:
	case PCUNKILL:
	case PCNICE:
		size += sizeof (int32_t);
		break;
	case PCRUN:
	case PCSET:
	case PCUNSET:
		size += sizeof (uint32_t);
		break;
	case PCSVADDR:
		size += sizeof (caddr32_t);
		break;
	case PCSTRACE:
	case PCSHOLD:
		size += sizeof (sigset_t);
		break;
	case PCSFAULT:
		size += sizeof (fltset_t);
		break;
	case PCSENTRY:
	case PCSEXIT:
		size += sizeof (sysset_t);
		break;
	case PCSREG:
	case PCAGENT:
		size += sizeof (prgregset32_t);
		break;
	case PCSFPREG:
		size += sizeof (prfpregset32_t);
		break;
#if defined(sparc) || defined(__sparc)
	case PCSXREG:
		size += sizeof (prxregset_t);
		break;
#endif
	case PCWATCH:
		size += sizeof (prwatch32_t);
		break;
	case PCREAD:
	case PCWRITE:
		size += sizeof (priovec32_t);
		break;
	case PCSCRED:
		size += sizeof (prcred32_t);
		break;
	default:
		return (0);
	}

	if (size > resid)
		return (0);
	return (size);
}

/*
 * Control operations (lots).
 */
int
prwritectl32(struct vnode *vp, struct uio *uiop, cred_t *cr)
{
#define	MY_BUFFER_SIZE32 \
		100 > 1 + sizeof (arg32_t) / sizeof (int32_t) ? \
		100 : 1 + sizeof (arg32_t) / sizeof (int32_t)
	int32_t buf[MY_BUFFER_SIZE32];
	int32_t *bufp;
	size_t resid = 0;
	size_t size;
	prnode_t *pnp = VTOP(vp);
	int error;
	int locked = 0;

	while (uiop->uio_resid) {
		/*
		 * Read several commands in one gulp.
		 */
		bufp = buf;
		if (resid) {	/* move incomplete command to front of buffer */
			int32_t *tail;

			if (resid >= sizeof (buf))
				break;
			tail = (int32_t *)((char *)buf + sizeof (buf) - resid);
			do {
				*bufp++ = *tail++;
			} while ((resid -= sizeof (int32_t)) != 0);
		}
		resid = sizeof (buf) - ((char *)bufp - (char *)buf);
		if (resid > uiop->uio_resid)
			resid = uiop->uio_resid;
		if (error = uiomove((caddr_t)bufp, resid, UIO_WRITE, uiop))
			return (error);
		resid += (char *)bufp - (char *)buf;
		bufp = buf;

		do {		/* loop over commands in buffer */
			int32_t cmd = bufp[0];
			arg32_t *argp = (arg32_t *)&bufp[1];

			size = ctlsize32(cmd, resid);
			if (size == 0)	/* incomplete or invalid command */
				break;
			/*
			 * Perform the specified control operation.
			 */
			if (!locked) {
				if ((error = prlock(pnp, ZNO)) != 0)
					return (error);
				locked = 1;
			}
			if (error = pr_control32(cmd, argp, pnp, cr)) {
				if (error == -1)	/* -1 is timeout */
					locked = 0;
				else
					return (error);
			}
			bufp = (int32_t *)((char *)bufp + size);
		} while ((resid -= size) != 0);

		if (locked) {
			prunlock(pnp);
			locked = 0;
		}
	}
	return (resid? EINVAL : 0);
}

static int
pr_control32(int32_t cmd, arg32_t *argp, prnode_t *pnp, cred_t *cr)
{
	prcommon_t *pcp;
	proc_t *p;
	int unlocked;
	int error = 0;

	if (cmd == PCNULL)
		return (0);

	pcp = pnp->pr_common;
	p = pcp->prc_proc;
	ASSERT(p != NULL);

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	case PCSTOP:	/* direct process or lwp to stop and wait for stop */
	case PCDSTOP:	/* direct process or lwp to stop, don't wait */
	case PCWSTOP:	/* wait for process or lwp to stop */
	case PCTWSTOP:	/* wait for process or lwp to stop, with timeout */
	    {
		time_t timeo;

		/*
		 * Can't apply to a system process.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			error = EBUSY;
			break;
		}

		if (cmd == PCSTOP || cmd == PCDSTOP)
			pr_stop(pnp);

		if (cmd == PCDSTOP)
			break;

		/*
		 * If an lwp is waiting for itself or its process, don't wait.
		 * The lwp will never see the fact that itself is stopped.
		 */
		if ((pcp->prc_flags & PRC_LWP)?
		    (pcp->prc_thread == curthread) : (p == curproc)) {
			if (cmd == PCWSTOP || cmd == PCTWSTOP)
				error = EBUSY;
			break;
		}

		timeo = (cmd == PCTWSTOP)? (time_t)argp->timeo : 0;
		if ((error = pr_wait_stop(pnp, timeo)) != 0)
			return (error);

		break;
	    }

	case PCRUN:	/* make lwp or process runnable */
		error = pr_setrun(pnp, (ulong_t)argp->flags);
		break;

	case PCSTRACE:	/* set signal trace mask */
		pr_settrace(p,  &argp->sigset);
		break;

	case PCSSIG:	/* set current signal */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else {
			int sig = (int)argp->siginfo.si_signo;
			siginfo_t siginfo;

			bzero(&siginfo, sizeof (siginfo));
			siginfo_32tok(&argp->siginfo, (k_siginfo_t *)&siginfo);
			error = pr_setsig(pnp, &siginfo);
			if (sig == SIGKILL && error == 0) {
				prunlock(pnp);
				pr_wait_die(pnp);
				return (-1);
			}
		}
		break;

	case PCKILL:	/* send signal */
		error = pr_kill(pnp, (int)argp->sig, cr);
		if (error == 0 && argp->sig == SIGKILL) {
			prunlock(pnp);
			pr_wait_die(pnp);
			return (-1);
		}
		break;

	case PCUNKILL:	/* delete a pending signal */
		error = pr_unkill(pnp, (int)argp->sig);
		break;

	case PCNICE:	/* set nice priority */
		error = pr_nice(p, (int)argp->nice, cr);
		break;

	case PCSENTRY:	/* set syscall entry bit mask */
	case PCSEXIT:	/* set syscall exit bit mask */
		pr_setentryexit(p, &argp->sysset, cmd == PCSENTRY);
		break;

	case PCSET:	/* set process flags */
		error = pr_set(p, (long)argp->flags);
		break;

	case PCUNSET:	/* unset process flags */
		error = pr_unset(p, (long)argp->flags);
		break;

	case PCSREG:	/* set general registers */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else {
			kthread_t *t = pr_thread(pnp);

			if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
				thread_unlock(t);
				error = EBUSY;
			} else {
				prgregset_t prgregset;
				klwp_t *lwp = ttolwp(t);

				thread_unlock(t);
				mutex_exit(&p->p_lock);
				prgregset_32ton(lwp, argp->prgregset,
					prgregset);
				prsetprregs(lwp, prgregset, 0);
				mutex_enter(&p->p_lock);
			}
		}
		break;

	case PCSFPREG:	/* set floating-point registers */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else
			error = pr_setfpregs32(pnp, &argp->prfpregset);
		break;

	case PCSXREG:	/* set extra registers */
#if defined(sparc) || defined(__sparc)
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else
			error = pr_setxregs(pnp, &argp->prxregset);
#else
		error = EINVAL;
#endif
		break;

	case PCSVADDR:	/* set virtual address at which to resume */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else
			error = pr_setvaddr(pnp, (caddr_t)argp->vaddr);
		break;

	case PCSHOLD:	/* set signal-hold mask */
		pr_sethold(pnp, &argp->sigset);
		break;

	case PCSFAULT:	/* set mask of traced faults */
		pr_setfault(p, &argp->fltset);
		break;

	case PCCSIG:	/* clear current signal */
		error = pr_clearsig(pnp);
		break;

	case PCCFAULT:	/* clear current fault */
		error = pr_clearflt(pnp);
		break;

	case PCWATCH:	/* set or clear watched areas */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else {
			prwatch_t prwatch;

			prwatch.pr_vaddr = argp->prwatch.pr_vaddr;
			prwatch.pr_size = argp->prwatch.pr_size;
			prwatch.pr_wflags = argp->prwatch.pr_wflags;
			prwatch.pr_pad = argp->prwatch.pr_pad;
			error = pr_watch(pnp, &prwatch, &unlocked);
			if (error && unlocked)
				return (error);
		}
		break;

	case PCAGENT:	/* create the /proc agent lwp in the target process */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else {
			prgregset_t prgregset;
			kthread_t *t = pr_thread(pnp);
			klwp_t *lwp = ttolwp(t);
			thread_unlock(t);
			mutex_exit(&p->p_lock);
			prgregset_32ton(lwp, argp->prgregset, prgregset);
			mutex_enter(&p->p_lock);
			error = pr_agent(pnp, prgregset, &unlocked);
			if (error && unlocked)
				return (error);
		}
		break;

	case PCREAD:	/* read from the address space */
	case PCWRITE:	/* write to the address space */
		if (PROCESS_NOT_32BIT(p))
			error = EOVERFLOW;
		else {
			enum uio_rw rw = (cmd == PCREAD)? UIO_READ : UIO_WRITE;
			priovec_t priovec;

			priovec.pio_base = (void *)argp->priovec.pio_base;
			priovec.pio_len = (size_t)argp->priovec.pio_len;
			priovec.pio_offset = (off_t)
				(uint32_t)argp->priovec.pio_offset;
			error = pr_rdwr(p, rw, &priovec);
		}
		break;

	case PCSCRED:	/* set the process credentials */
	    {
		prcred_t prcred;

		prcred.pr_euid = argp->prcred.pr_euid;
		prcred.pr_ruid = argp->prcred.pr_ruid;
		prcred.pr_suid = argp->prcred.pr_suid;
		prcred.pr_egid = argp->prcred.pr_egid;
		prcred.pr_rgid = argp->prcred.pr_rgid;
		prcred.pr_sgid = argp->prcred.pr_sgid;
		prcred.pr_ngroups = 0;
		error = pr_scred(p, &prcred, cr);
		break;
	    }

	}

	if (error)
		prunlock(pnp);
	return (error);
}

#endif	/* _SYSCALL32_IMPL */

/*
 * Return the specific or chosen thread/lwp for a control operation.
 * Returns with the thread locked via thread_lock(t).
 */
kthread_t *
pr_thread(prnode_t *pnp)
{
	prcommon_t *pcp = pnp->pr_common;
	kthread_t *t;

	if (pcp->prc_flags & PRC_LWP) {
		t = pcp->prc_thread;
		ASSERT(t != NULL);
		thread_lock(t);
	} else {
		proc_t *p = pcp->prc_proc;
		t = prchoose(p);	/* returns locked thread */
		ASSERT(t != NULL);
	}

	return (t);
}

static void
pr_timeout(void *arg)
{
	prcommon_t *pcp = arg;

	mutex_enter(&pcp->prc_mutex);
	cv_broadcast(&pcp->prc_wait);
	mutex_exit(&pcp->prc_mutex);
}

/*
 * Direct the process or lwp to stop.
 */
void
pr_stop(prnode_t *pnp)
{
	prcommon_t *pcp = pnp->pr_common;
	proc_t *p = pcp->prc_proc;
	kthread_t *t;

	/*
	 * If already stopped, do nothing; otherwise flag
	 * it to be stopped the next time it tries to run.
	 * If sleeping at interruptible priority, set it
	 * running so it will stop within cv_wait_sig().
	 *
	 * Take care to cooperate with jobcontrol: if an lwp
	 * is stopped due to the default action of a jobcontrol
	 * stop signal, flag it to be stopped the next time it
	 * starts due to a SIGCONT signal.
	 */
	if (pcp->prc_flags & PRC_LWP)
		t = pcp->prc_thread;
	else
		t = p->p_tlist;
	ASSERT(t != NULL);

	do {
		int notify;

		notify = 0;
		thread_lock(t);
		if (!ISTOPPED(t)) {
			t->t_proc_flag |= TP_PRSTOP;
			t->t_sig_check = 1;	/* do ISSIG */
		}
		if (t->t_state == TS_SLEEP &&
		    (t->t_flag & T_WAKEABLE)) {
			if (t->t_wchan0 == NULL)
				setrun_locked(t);
			else if (!VSTOPPED(t)) {
				/*
				 * Mark it virtually stopped.
				 */
				t->t_proc_flag |= TP_PRVSTOP;
				notify = 1;
			}
		}
		/*
		 * force the thread into the kernel
		 * if it is not already there.
		 */
		prpokethread(t);
		thread_unlock(t);
		if (notify && t->t_trace)
			prnotify(t->t_trace);
		if (pcp->prc_flags & PRC_LWP)
			break;
	} while ((t = t->t_forw) != p->p_tlist);

	/*
	 * We do this just in case the thread we asked
	 * to stop is in holdlwps() (called from cfork()).
	 */
	cv_broadcast(&p->p_holdlwps);
}

/*
 * Sleep until the lwp stops, but cooperate with
 * jobcontrol:  Don't wake up if the lwp is stopped
 * due to the default action of a jobcontrol stop signal.
 * If this is the process file descriptor, sleep
 * until all of the process's lwps stop.
 */
int
pr_wait_stop(prnode_t *pnp, time_t timeo)
{
	prcommon_t *pcp = pnp->pr_common;
	proc_t *p = pcp->prc_proc;
	clock_t starttime = timeo? lbolt : 0;
	kthread_t *t;
	int error;

	if (pcp->prc_flags & PRC_LWP) {	/* lwp file descriptor */
		t = pcp->prc_thread;
		ASSERT(t != NULL);
		thread_lock(t);
		while (!ISTOPPED(t) && !VSTOPPED(t)) {
			thread_unlock(t);
			mutex_enter(&pcp->prc_mutex);
			prunlock(pnp);
			error = pr_wait(pcp, timeo, starttime);
			if (error)	/* -1 is timeout */
				return (error);
			if ((error = prlock(pnp, ZNO)) != 0)
				return (error);
			ASSERT(p == pcp->prc_proc);
			ASSERT(t == pcp->prc_thread);
			thread_lock(t);
		}
		thread_unlock(t);
	} else {			/* process file descriptor */
		t = prchoose(p);	/* returns locked thread */
		ASSERT(t != NULL);
		while (!ISTOPPED(t) && !VSTOPPED(t) && !SUSPENDED(t)) {
			thread_unlock(t);
			mutex_enter(&pcp->prc_mutex);
			prunlock(pnp);
			error = pr_wait(pcp, timeo, starttime);
			if (error)	/* -1 is timeout */
				return (error);
			if ((error = prlock(pnp, ZNO)) != 0)
				return (error);
			ASSERT(p == pcp->prc_proc);
			t = prchoose(p);	/* returns locked t */
			ASSERT(t != NULL);
		}
		thread_unlock(t);
	}

	ASSERT(!(pcp->prc_flags & PRC_DESTROY) && p->p_stat != SZOMB &&
	    t != NULL && t->t_state != TS_ZOMB);

	return (0);
}

int
pr_setrun(prnode_t *pnp, ulong_t flags)
{
	prcommon_t *pcp = pnp->pr_common;
	proc_t *p = pcp->prc_proc;
	kthread_t *t;
	klwp_t *lwp;

	/*
	 * Cannot set an lwp running if it is not stopped.
	 * Also, no lwp other than the /proc agent lwp can
	 * be set running so long as the /proc agent lwp exists.
	 */
	t = pr_thread(pnp);	/* returns locked thread */
	if ((!ISTOPPED(t) && !VSTOPPED(t) &&
	    !(t->t_proc_flag & TP_PRSTOP)) ||
	    (p->p_agenttp != NULL &&
	    (t != p->p_agenttp || !(pcp->prc_flags & PRC_LWP)))) {
		thread_unlock(t);
		return (EBUSY);
	}
	thread_unlock(t);
	if (flags & ~(PRCSIG|PRCFAULT|PRSTEP|PRSTOP|PRSABORT))
		return (EINVAL);
	lwp = ttolwp(t);
	if ((flags & PRCSIG) && lwp->lwp_cursig != SIGKILL) {
		/*
		 * Discard current siginfo_t, if any.
		 */
		lwp->lwp_cursig = 0;
		if (lwp->lwp_curinfo) {
			siginfofree(lwp->lwp_curinfo);
			lwp->lwp_curinfo = NULL;
		}
	}
	if (flags & PRCFAULT)
		lwp->lwp_curflt = 0;
	/*
	 * We can't hold p->p_lock when we touch the lwp's registers.
	 * It may be swapped out and we will get a page fault.
	 */
	if (flags & PRSTEP) {
		mutex_exit(&p->p_lock);
		prstep(lwp, 0);
		mutex_enter(&p->p_lock);
	}
	if (flags & PRSTOP) {
		t->t_proc_flag |= TP_PRSTOP;
		t->t_sig_check = 1;	/* do ISSIG */
	}
	if (flags & PRSABORT)
		lwp->lwp_sysabort = 1;
	thread_lock(t);
	if ((pcp->prc_flags & PRC_LWP) || (flags & (PRSTEP|PRSTOP))) {
		/*
		 * Here, we are dealing with a single lwp.
		 */
		if (ISTOPPED(t)) {
			t->t_schedflag |= TS_PSTART;
			setrun_locked(t);
		} else if (flags & PRSABORT) {
			t->t_proc_flag &=
			    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
			setrun_locked(t);
		} else if (!(flags & PRSTOP)) {
			t->t_proc_flag &=
			    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
		}
		thread_unlock(t);
	} else {
		/*
		 * Here, we are dealing with the whole process.
		 */
		if (ISTOPPED(t)) {
			/*
			 * The representative lwp is stopped on an event
			 * of interest.  We demote it to PR_REQUESTED and
			 * choose another representative lwp.  If the new
			 * representative lwp is not stopped on an event of
			 * interest (other than PR_REQUESTED), we set the
			 * whole process running, else we leave the process
			 * stopped showing the next event of interest.
			 */
			kthread_t *tx = NULL;

			if (!(flags & PRSABORT) &&
			    t->t_whystop == PR_SYSENTRY &&
			    t->t_whatstop == SYS_lwp_exit)
				tx = t;		/* remember the exiting lwp */
			t->t_whystop = PR_REQUESTED;
			t->t_whatstop = 0;
			thread_unlock(t);
			t = prchoose(p);	/* returns locked t */
			ASSERT(ISTOPPED(t) || VSTOPPED(t));
			if (VSTOPPED(t) ||
			    t->t_whystop == PR_REQUESTED) {
				thread_unlock(t);
				allsetrun(p);
			} else {
				thread_unlock(t);
				/*
				 * As a special case, if the old representative
				 * lwp was stopped on entry to _lwp_exit()
				 * (and we are not aborting the system call),
				 * we set the old representative lwp running.
				 * We do this so that the next process stop
				 * will find the exiting lwp gone.
				 */
				if (tx != NULL) {
					thread_lock(tx);
					tx->t_schedflag |= TS_PSTART;
					setrun_locked(tx);
					thread_unlock(tx);
				}
			}
		} else {
			/*
			 * No event of interest; set all of the lwps running.
			 */
			if (flags & PRSABORT) {
				t->t_proc_flag &=
				    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
				setrun_locked(t);
			}
			thread_unlock(t);
			allsetrun(p);
		}
	}
	return (0);
}

/*
 * Wait until process/lwp stops or until timer expires.
 */
int
pr_wait(prcommon_t *pcp,	/* prcommon referring to process/lwp */
	time_t timeo,		/* timeout in milliseconds */
	time_t starttime)	/* value of lbolt at start of timer */
{
	int error = 0;
	timeout_id_t id = 0;

	ASSERT(MUTEX_HELD(&pcp->prc_mutex));

	if (timeo > 0) {
		/*
		 * Make sure the millisecond timeout hasn't been reached.
		 */
		clock_t rem = timeo - TICK_TO_MSEC(lbolt - starttime);
		if (rem <= 0) {
			mutex_exit(&pcp->prc_mutex);
			return (-1);
		}
		/*
		 * Turn rem into clock ticks and round up.
		 */
		rem = MSEC_TO_TICK_ROUNDUP(rem);
		id = timeout(pr_timeout, pcp, rem);
	}

	if (!cv_wait_sig(&pcp->prc_wait, &pcp->prc_mutex)) {
		mutex_exit(&pcp->prc_mutex);
		if (timeo > 0)
			(void) untimeout(id);
		return (EINTR);
	}
	mutex_exit(&pcp->prc_mutex);
	if (timeo > 0 && untimeout(id) < 0)
		error = -1;
	return (error);
}

/*
 * Make all threads in the process runnable.
 */
void
allsetrun(proc_t *p)
{
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((t = p->p_tlist) != NULL) {
		do {
			thread_lock(t);
			ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
			t->t_proc_flag &= ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
			if (ISTOPPED(t)) {
				t->t_schedflag |= TS_PSTART;
				setrun_locked(t);
			}
			thread_unlock(t);
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * Wait for the process to die.
 * We do this after sending SIGKILL because we know it will
 * die soon and we want subsequent operations to return ENOENT.
 */
void
pr_wait_die(prnode_t *pnp)
{
	proc_t *p;

	mutex_enter(&pidlock);
	while ((p = pnp->pr_common->prc_proc) != NULL && p->p_stat != SZOMB) {
		if (!cv_wait_sig(&p->p_srwchan_cv, &pidlock))
			break;
	}
	mutex_exit(&pidlock);
}

static void
pr_settrace(proc_t *p, sigset_t *sp)
{
	prdelset(sp, SIGKILL);
	prassignset(&p->p_sigmask, sp);
	if (!sigisempty(&p->p_sigmask))
		p->p_flag |= SPROCTR;
	else if (prisempty(&p->p_fltmask)) {
		user_t *up = prumap(p);
		if (up->u_systrap == 0)
			p->p_flag &= ~SPROCTR;
		prunmap(p);
	}
}

int
pr_setsig(prnode_t *pnp, siginfo_t *sip)
{
	int sig = sip->si_signo;
	prcommon_t *pcp = pnp->pr_common;
	proc_t *p = pcp->prc_proc;
	kthread_t *t;
	klwp_t *lwp;
	int error = 0;

	t = pr_thread(pnp);	/* returns locked thread */
	thread_unlock(t);
	lwp = ttolwp(t);
	if (sig < 0 || sig >= NSIG)
		/* Zero allowed here */
		error = EINVAL;
	else if (lwp->lwp_cursig == SIGKILL)
		/* "can't happen", but just in case */
		error = EBUSY;
	else if ((lwp->lwp_cursig = (uchar_t)sig) == 0) {
		/*
		 * Discard current siginfo_t, if any.
		 */
		if (lwp->lwp_curinfo) {
			siginfofree(lwp->lwp_curinfo);
			lwp->lwp_curinfo = NULL;
		}
	} else {
		kthread_t *tx;
		kthread_t *ty;
		sigqueue_t *sqp;

		/* drop p_lock to do kmem_alloc(KM_SLEEP) */
		mutex_exit(&p->p_lock);
		sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);
		mutex_enter(&p->p_lock);

		if (lwp->lwp_curinfo == NULL)
			lwp->lwp_curinfo = sqp;
		else
			kmem_free(sqp, sizeof (sigqueue_t));
		/*
		 * Copy contents of info to current siginfo_t.
		 */
		bcopy(sip, &lwp->lwp_curinfo->sq_info,
		    sizeof (lwp->lwp_curinfo->sq_info));
		/*
		 * Side-effects for SIGKILL and jobcontrol signals.
		 */
		tx = p->p_aslwptp;
		if (sig == SIGKILL)
			p->p_flag |= SKILLED;
		else if (sig == SIGCONT) {
			sigdelq(p, tx, SIGSTOP);
			sigdelq(p, tx, SIGTSTP);
			sigdelq(p, tx, SIGTTOU);
			sigdelq(p, tx, SIGTTIN);
			if (tx == NULL)
				sigdiffset(&p->p_sig, &stopdefault);
			else {
				sigdiffset(&tx->t_sig, &stopdefault);
				sigdiffset(&p->p_notifsigs,
				    &stopdefault);
				if ((ty = p->p_tlist) != NULL) {
					do {
						sigdelq(p, ty, SIGSTOP);
						sigdelq(p, ty, SIGTSTP);
						sigdelq(p, ty, SIGTTOU);
						sigdelq(p, ty, SIGTTIN);
						sigdiffset(&ty->t_sig,
						    &stopdefault);
					} while ((ty = ty->t_forw) !=
					    p->p_tlist);
				}
			}
		} else if (sigismember(&stopdefault, sig)) {
			sigdelq(p, tx, SIGCONT);
			if (tx == NULL)
				sigdelset(&p->p_sig, SIGCONT);
			else {
				sigdelset(&tx->t_sig, SIGCONT);
				sigdelset(&p->p_notifsigs, SIGCONT);
				if ((ty = p->p_tlist) != NULL) {
					do {
						sigdelq(p, ty, SIGCONT);
						sigdelset(&ty->t_sig,
						    SIGCONT);
					} while ((ty = ty->t_forw) !=
					    p->p_tlist);
				}
			}
		}
		thread_lock(t);
		if (t->t_state == TS_SLEEP &&
		    (t->t_flag & T_WAKEABLE)) {
			/* Set signalled sleeping lwp running */
			setrun_locked(t);
		} else if (t->t_state == TS_STOPPED && sig == SIGKILL) {
			/* If SIGKILL, set stopped lwp running */
			p->p_stopsig = 0;
			t->t_schedflag |= TS_XSTART | TS_PSTART;
			setrun_locked(t);
		}
		t->t_sig_check = 1;	/* so ISSIG will be done */
		thread_unlock(t);
		/*
		 * More jobcontrol side-effects.
		 */
		if (sig == SIGCONT && (tx = p->p_tlist) != NULL) {
			p->p_stopsig = 0;
			do {
				thread_lock(tx);
				if (tx->t_state == TS_STOPPED &&
				    tx->t_whystop == PR_JOBCONTROL) {
					tx->t_schedflag |= TS_XSTART;
					setrun_locked(tx);
				}
				thread_unlock(tx);
			} while ((tx = tx->t_forw) != p->p_tlist);
		}
	}
	return (error);
}

int
pr_kill(prnode_t *pnp, int sig, cred_t *cr)
{
	prcommon_t *pcp = pnp->pr_common;
	proc_t *p = pcp->prc_proc;
	k_siginfo_t info;

	if (sig <= 0 || sig >= NSIG)
		return (EINVAL);

	bzero(&info, sizeof (info));
	info.si_signo = sig;
	info.si_code = SI_USER;
	info.si_pid = ttoproc(curthread)->p_pid;
	info.si_uid = cr->cr_ruid;
	sigaddq(p, (pcp->prc_flags & PRC_LWP)?
	    pcp->prc_thread : NULL, &info, KM_NOSLEEP);

	return (0);
}

int
pr_unkill(prnode_t *pnp, int sig)
{
	prcommon_t *pcp = pnp->pr_common;
	proc_t *p = pcp->prc_proc;
	sigqueue_t *infop = NULL;

	if (sig <= 0 || sig >= NSIG || sig == SIGKILL)
		return (EINVAL);

	if (pcp->prc_flags & PRC_LWP) {
		kthread_t *t = pcp->prc_thread;

		prdelset(&t->t_sig, sig);
		(void) sigdeq(p, t, sig, &infop);
	} else {
		kthread_t *aslwptp = p->p_aslwptp;

		if (aslwptp != NULL) {
			if (sigismember(&p->p_notifsigs, sig))
				prdelset(&p->p_notifsigs, sig);
			else
				prdelset(&aslwptp->t_sig, sig);
			(void) sigdeq(p, aslwptp, sig, &infop);
		} else {
			prdelset(&p->p_sig, sig);
			(void) sigdeq(p, NULL, sig, &infop);
		}
	}
	if (infop)
		siginfofree(infop);

	return (0);
}

int
pr_nice(proc_t *p, int nice, cred_t *cr)
{
	kthread_t *t;
	int err;
	int error = 0;

	t = p->p_tlist;
	do {
		ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
		err = CL_DONICE(t, cr, nice, (int *)NULL);
		if (error == 0)
			error = err;
	} while ((t = t->t_forw) != p->p_tlist);

	return (error);
}

void
pr_setentryexit(proc_t *p, sysset_t *sysset, int entry)
{
	user_t *up = prumap(p);

	if (entry) {
		prassignset(&up->u_entrymask, sysset);
	} else {
		prassignset(&up->u_exitmask, sysset);
	}
	if (!prisempty(&up->u_entrymask) ||
	    !prisempty(&up->u_exitmask)) {
		up->u_systrap = 1;
		p->p_flag |= SPROCTR;
		set_proc_sys(p);	/* set pre and post-sys flags */
	} else {
		up->u_systrap = 0;
		if (sigisempty(&p->p_sigmask) &&
		    prisempty(&p->p_fltmask))
			p->p_flag &= ~SPROCTR;
	}
	prunmap(p);
}

#define	ALLFLAGS	\
	(PR_FORK|PR_RLC|PR_KLC|PR_ASYNC|PR_BPTADJ|PR_MSACCT|PR_MSFORK|PR_PTRACE)

int
pr_set(proc_t *p, long flags)
{
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		return (EBUSY);

	if (flags & ~ALLFLAGS)
		return (EINVAL);

	if (flags & PR_FORK)
		p->p_flag |= SPRFORK;
	if (flags & PR_RLC)
		p->p_flag |= SRUNLCL;
	if (flags & PR_KLC)
		p->p_flag |= SKILLCL;
	if (flags & PR_ASYNC)
		p->p_flag |= SPASYNC;
	if (flags & PR_BPTADJ)
		p->p_flag |= SBPTADJ;
	if (flags & PR_MSACCT)
		if ((p->p_flag & SMSACCT) == 0)
			estimate_msacct(p->p_tlist, gethrtime());
	if (flags & PR_MSFORK)
		p->p_flag |= SMSFORK;
	if (flags & PR_PTRACE) {
		p->p_flag |= STRC;
		/* ptraced process must die if parent dead */
		if (p->p_ppid == 1)
			sigtoproc(p, NULL, SIGKILL);
	}

	return (0);
}

int
pr_unset(proc_t *p, long flags)
{
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		return (EBUSY);

	if (flags & ~ALLFLAGS)
		return (EINVAL);

	if (flags & PR_FORK)
		p->p_flag &= ~SPRFORK;
	if (flags & PR_RLC)
		p->p_flag &= ~SRUNLCL;
	if (flags & PR_KLC)
		p->p_flag &= ~SKILLCL;
	if (flags & PR_ASYNC)
		p->p_flag &= ~SPASYNC;
	if (flags & PR_BPTADJ)
		p->p_flag &= ~SBPTADJ;
	if (flags & PR_MSACCT)
		disable_msacct(p);
	if (flags & PR_MSFORK)
		p->p_flag &= ~SMSFORK;
	if (flags & PR_PTRACE)
		p->p_flag &= ~STRC;

	return (0);
}

static int
pr_setfpregs(prnode_t *pnp, prfpregset_t *prfpregset)
{
	proc_t *p = pnp->pr_common->prc_proc;
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
		thread_unlock(t);
		return (EBUSY);
	}
	if (!prhasfp()) {
		thread_unlock(t);
		return (EINVAL);	/* No FP support */
	}

	/* drop p_lock while touching the lwp's stack */
	thread_unlock(t);
	mutex_exit(&p->p_lock);
	prsetprfpregs(ttolwp(t), prfpregset);
	mutex_enter(&p->p_lock);

	return (0);
}

#ifdef	_SYSCALL32_IMPL
static int
pr_setfpregs32(prnode_t *pnp, prfpregset32_t *prfpregset)
{
	proc_t *p = pnp->pr_common->prc_proc;
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
		thread_unlock(t);
		return (EBUSY);
	}
	if (!prhasfp()) {
		thread_unlock(t);
		return (EINVAL);	/* No FP support */
	}

	/* drop p_lock while touching the lwp's stack */
	thread_unlock(t);
	mutex_exit(&p->p_lock);
	prsetprfpregs32(ttolwp(t), prfpregset);
	mutex_enter(&p->p_lock);

	return (0);
}
#endif	/* _SYSCALL32_IMPL */

#if defined(sparc) || defined(__sparc)
/* ARGSUSED */
static int
pr_setxregs(prnode_t *pnp, prxregset_t *prxregset)
{
	proc_t *p = pnp->pr_common->prc_proc;
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
		thread_unlock(t);
		return (EBUSY);
	}
	thread_unlock(t);

	if (!prhasx(p))
		return (EINVAL);	/* No extra register support */

	/* drop p_lock while touching the lwp's stack */
	mutex_exit(&p->p_lock);
	prsetprxregs(ttolwp(t), (caddr_t)prxregset);
	mutex_enter(&p->p_lock);

	return (0);
}
#endif

#if defined(__sparcv9)
static int
pr_setasrs(prnode_t *pnp, asrset_t asrset)
{
	proc_t *p = pnp->pr_common->prc_proc;
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
		thread_unlock(t);
		return (EBUSY);
	}
	thread_unlock(t);

	/* drop p_lock while touching the lwp's stack */
	mutex_exit(&p->p_lock);
	prsetasregs(ttolwp(t), asrset);
	mutex_enter(&p->p_lock);

	return (0);
}
#endif

static int
pr_setvaddr(prnode_t *pnp, caddr_t vaddr)
{
	proc_t *p = pnp->pr_common->prc_proc;
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	if (!ISTOPPED(t) && !VSTOPPED(t) && !DSTOPPED(t)) {
		thread_unlock(t);
		return (EBUSY);
	}

	/* drop p_lock while touching the lwp's stack */
	thread_unlock(t);
	mutex_exit(&p->p_lock);
	prsvaddr(ttolwp(t), vaddr);
	mutex_enter(&p->p_lock);

	return (0);
}

void
pr_sethold(prnode_t *pnp, sigset_t *sp)
{
	proc_t *p = pnp->pr_common->prc_proc;
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	sigutok(sp, &t->t_hold);
	sigdiffset(&t->t_hold, &cantmask);
	if (t->t_state == TS_SLEEP &&
	    (t->t_flag & T_WAKEABLE) &&
	    (fsig(&p->p_sig, t) || fsig(&t->t_sig, t)))
		setrun_locked(t);
	t->t_sig_check = 1;	/* so thread will see new holdmask */
	thread_unlock(t);
}

void
pr_setfault(proc_t *p, fltset_t *fltp)
{
	prassignset(&p->p_fltmask, fltp);
	if (!prisempty(&p->p_fltmask))
		p->p_flag |= SPROCTR;
	else if (sigisempty(&p->p_sigmask)) {
		user_t *up = prumap(p);
		if (up->u_systrap == 0)
			p->p_flag &= ~SPROCTR;
		prunmap(p);
	}
}

static int
pr_clearsig(prnode_t *pnp)
{
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */
	klwp_t *lwp = ttolwp(t);

	thread_unlock(t);
	if (lwp->lwp_cursig == SIGKILL)
		return (EBUSY);

	/*
	 * Discard current siginfo_t, if any.
	 */
	lwp->lwp_cursig = 0;
	if (lwp->lwp_curinfo) {
		siginfofree(lwp->lwp_curinfo);
		lwp->lwp_curinfo = NULL;
	}

	return (0);
}

static int
pr_clearflt(prnode_t *pnp)
{
	kthread_t *t = pr_thread(pnp);	/* returns locked thread */

	thread_unlock(t);
	ttolwp(t)->lwp_curflt = 0;

	return (0);
}

static int
pr_watch(prnode_t *pnp, prwatch_t *pwp, int *unlocked)
{
	proc_t *p = pnp->pr_common->prc_proc;
	struct as *as = p->p_as;
	uintptr_t vaddr = pwp->pr_vaddr;
	size_t size = pwp->pr_size;
	int wflags = pwp->pr_wflags;
	struct watched_page *pwplist = NULL;
	ulong_t newpage = 0;
	struct watched_area *pwa;
	int error;

	*unlocked = 0;

	/*
	 * Can't apply to a system process.
	 */
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		return (EBUSY);

	/*
	 * Verify that the address range does not wrap
	 * and that only the proper flags were specified.
	 */
	if ((wflags & ~WA_TRAPAFTER) == 0)
		size = 0;
	if (vaddr + size < vaddr ||
	    (wflags & ~(WA_READ|WA_WRITE|WA_EXEC|WA_TRAPAFTER)) != 0 ||
	    ((wflags & ~WA_TRAPAFTER) != 0 && size == 0))
		return (EINVAL);

	/*
	 * Don't let the address range go above as->a_userlimit.
	 * There is no error here, just a limitation.
	 */
	if (vaddr >= (uintptr_t)as->a_userlimit)
		return (0);
	if (vaddr + size > (uintptr_t)as->a_userlimit)
		size = (uintptr_t)as->a_userlimit - vaddr;

	/*
	 * Compute maximum number of pages this will add.
	 */
	if ((wflags & ~WA_TRAPAFTER) != 0) {
		ulong_t pagespan = (vaddr + size) - (vaddr & PAGEMASK);
		newpage = btopr(pagespan);
		if (newpage > 2 * prnwatch)
			return (E2BIG);
	}

	/*
	 * Force the process to be fully stopped.
	 */
	if (p == curproc) {
		prunlock(pnp);
		while (holdwatch() == 0)
			;
		if ((error = prlock(pnp, ZNO)) != 0) {
			continuelwps(p);
			*unlocked = 1;
			return (error);
		}
	} else {
		pauselwps(p);
		while (pr_allstopped(p) > 0) {
			/*
			 * This cv/mutex pair is persistent even
			 * if the process disappears after we
			 * unmark it and drop p->p_lock.
			 */
			kcondvar_t *cv = &pr_pid_cv[p->p_slot];
			kmutex_t *mp = &p->p_lock;

			prunmark(p);
			(void) cv_wait(cv, mp);
			mutex_exit(mp);
			if ((error = prlock(pnp, ZNO)) != 0) {
				/*
				 * Unpause the process if it exists.
				 */
				p = pr_p_lock(pnp);
				mutex_exit(&pr_pidlock);
				if (p != NULL) {
					unpauselwps(p);
					prunlock(pnp);
				}
				*unlocked = 1;
				return (error);
			}
		}
	}

	/*
	 * Drop p->p_lock in order to perform the rest of this.
	 * The process is still locked with the SPRLOCK flag.
	 */
	mutex_exit(&p->p_lock);

	pwa = kmem_alloc(sizeof (struct watched_area), KM_SLEEP);
	pwa->wa_vaddr = (caddr_t)vaddr;
	pwa->wa_eaddr = (caddr_t)vaddr + size;
	pwa->wa_flags = (ulong_t)wflags;

	/*
	 * Allocate enough watched_page structs to use while holding
	 * the process's p_lock.  We will later free the excess.
	 * Allocate one more than we will possible need in order
	 * to simplify subsequent code.
	 */
	if (newpage) {
		struct watched_page *pwpg;

		pwpg = kmem_zalloc(sizeof (struct watched_page),
			KM_SLEEP);
		pwplist = pwpg->wp_forw = pwpg->wp_back = pwpg;
		while (newpage-- > 0) {
			pwpg = kmem_zalloc(sizeof (struct watched_page),
				KM_SLEEP);
			insque(pwpg, pwplist->wp_back);
		}
		pwplist = pwplist->wp_back;
	}

	error = ((pwa->wa_flags & ~WA_TRAPAFTER) == 0)?
		clear_watched_area(p, pwa) :
		set_watched_area(p, pwa, pwplist);

	/*
	 * Free the watched_page structs we didn't use.
	 */
	if (pwplist)
		pr_free_pagelist(pwplist);

	if (p == curproc) {
		setallwatch();
		mutex_enter(&p->p_lock);
		continuelwps(p);
	} else {
		mutex_enter(&p->p_lock);
		unpauselwps(p);
	}

	return (error);
}

/* jobcontrol stopped, but with a /proc directed stop in effect */
#define	JDSTOPPED(t)	\
	((t)->t_state == TS_STOPPED && \
	(t)->t_whystop == PR_JOBCONTROL && \
	((t)->t_proc_flag & TP_PRSTOP))

static int
pr_agent(prnode_t *pnp, prgregset_t prgregset, int *unlocked)
{
	proc_t *p = pnp->pr_common->prc_proc;
	prcommon_t *pcp;
	kthread_t *t;
	kthread_t *ct;
	klwp_t *clwp;
	k_sigset_t smask;
	int cid;
	void *bufp = NULL;
	int error;

	*unlocked = 0;

	/*
	 * Cannot create the /proc agent lwp if the process
	 * is not fully stopped or directed to stop.
	 * There can be only one /proc agent lwp.
	 * A vfork() parent cannot be subjected to an agent lwp.
	 */
	t = prchoose(p);	/* returns locked thread */
	ASSERT(t != NULL);
	if ((!ISTOPPED(t) && !VSTOPPED(t) &&
	    !SUSPENDED(t) && !JDSTOPPED(t)) ||
	    p->p_agenttp != NULL || (p->p_flag & SVFWAIT)) {
		thread_unlock(t);
		return (EBUSY);
	}
	thread_unlock(t);

	mutex_exit(&p->p_lock);
	sigfillset(&smask);
	sigdiffset(&smask, &cantmask);
	clwp = lwp_create(lwp_rtt, 0, 0, p, TS_STOPPED,
	    t->t_pri, smask, NOCLASS);
	if (clwp == NULL) {
		mutex_enter(&p->p_lock);
		return (ENOMEM);
	}
	prsetprregs(clwp, prgregset, 1);
retry:
	cid = t->t_cid;
	(void) CL_ALLOC(&bufp, cid, KM_SLEEP);
	mutex_enter(&p->p_lock);
	if (cid != t->t_cid) {
		/*
		 * Someone just changed this thread's scheduling class,
		 * so try pre-allocating the buffer again.  Hopefully we
		 * don't hit this often.
		 */
		mutex_exit(&p->p_lock);
		CL_FREE(cid, bufp);
		goto retry;
	}
	clwp->lwp_ap = clwp->lwp_arg;
	clwp->lwp_eosys = NORMALRETURN;
	ct = lwptot(clwp);
	ct->t_clfuncs = t->t_clfuncs;
	CL_FORK(t, ct, bufp);
	ct->t_cid = t->t_cid;
	ct->t_proc_flag |= TP_PRSTOP;
	/*
	 * Setting t_sysnum to zero causes post_syscall()
	 * to bypass all syscall checks and go directly to
	 *	if (issig()) psig();
	 * so that the agent lwp will stop in issig_forreal()
	 * showing PR_REQUESTED.
	 */
	ct->t_sysnum = 0;
	ct->t_post_sys = 1;
	ct->t_sig_check = 1;
	p->p_agenttp = ct;
	ct->t_proc_flag &= ~TP_HOLDLWP;
	lwp_create_done(ct);
	/*
	 * Don't return until the agent is stopped on PR_REQUESTED.
	 *
	 * XXX: We should have a way to actually create the agent
	 * lwp in the PR_REQUESTED stopped state rather than
	 * having to wait for it to reach that state.
	 * In the meantime, just ignore EINTR for this wait.
	 */
	pcp = pnp->pr_pcommon;
	mutex_enter(&pcp->prc_mutex);
	prunlock(pnp);
	*unlocked = 1;
	error = pr_wait(pcp, 0, 0);
	if (error == EINTR)
		error = 0;
	return (error? error : -1);
}

static int
pr_rdwr(proc_t *p, enum uio_rw rw, priovec_t *pio)
{
	caddr_t base = (caddr_t)pio->pio_base;
	size_t cnt = pio->pio_len;
	uintptr_t offset = (uintptr_t)pio->pio_offset;
	struct uio auio;
	struct iovec aiov;
	int error = 0;

	if ((p->p_flag & SSYS) || p->p_as == &kas)
		error = EIO;
	else if ((base + cnt) < base || (offset + cnt) < offset)
		error = EINVAL;
	else if (cnt != 0) {
		aiov.iov_base = base;
		aiov.iov_len = cnt;

		auio.uio_loffset = offset;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = cnt;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_llimit = (longlong_t)MAXOFFSET_T;
		auio.uio_fmode = FREAD|FWRITE;

		mutex_exit(&p->p_lock);
		error = prusrio(p, rw, &auio, 0);
		mutex_enter(&p->p_lock);

		/*
		 * We have no way to return the i/o count,
		 * like read() or write() would do, so we
		 * return an error if the i/o was truncated.
		 */
		if (auio.uio_resid != 0 && error == 0)
			error = EIO;
	}

	return (error);
}

static int
pr_scred(proc_t *p, prcred_t *prcred, cred_t *cr)
{
	kthread_t *t;
	cred_t *oldcred;
	cred_t *newcred;
	uid_t oldruid;

	if (!suser(cr))
		return (EPERM);

	if ((uint_t)prcred->pr_euid > MAXUID ||
	    (uint_t)prcred->pr_ruid > MAXUID ||
	    (uint_t)prcred->pr_suid > MAXUID ||
	    (uint_t)prcred->pr_egid > MAXUID ||
	    (uint_t)prcred->pr_rgid > MAXUID ||
	    (uint_t)prcred->pr_sgid > MAXUID)
		return (EINVAL);

	mutex_exit(&p->p_lock);

	/* hold old cred so it doesn't disappear while we dup it */
	mutex_enter(&p->p_crlock);
	crhold(oldcred = p->p_cred);
	mutex_exit(&p->p_crlock);
	newcred = crdup(oldcred);
	oldruid = oldcred->cr_ruid;
	crfree(oldcred);

	newcred->cr_uid  = prcred->pr_euid;
	newcred->cr_ruid = prcred->pr_ruid;
	newcred->cr_suid = prcred->pr_suid;
	newcred->cr_gid  = prcred->pr_egid;
	newcred->cr_rgid = prcred->pr_rgid;
	newcred->cr_sgid = prcred->pr_sgid;

	mutex_enter(&p->p_crlock);
	oldcred = p->p_cred;
	p->p_cred = newcred;
	mutex_exit(&p->p_crlock);
	crfree(oldcred);

	/*
	 * Keep count of processes per uid consistent.
	 */
	if (oldruid != newcred->cr_ruid) {
		mutex_enter(&pidlock);
		upcount_dec(oldruid);
		upcount_inc(newcred->cr_ruid);
		mutex_exit(&pidlock);
	}

	/*
	 * Broadcast the cred change to the threads.
	 */
	mutex_enter(&p->p_lock);
	t = p->p_tlist;
	do {
		t->t_pre_sys = 1; /* so syscall will get new cred */
	} while ((t = t->t_forw) != p->p_tlist);

	return (0);
}

/*
 * Return -1 if the process is the parent of a vfork(1)
 * whose child has yet to terminate or perform an exec(2).
 * Otherwise return 0 if the process is fully stopped, except
 * for the current lwp (if we are operating on our own process).
 * Otherwise return 1 to indicate the process is not fully stopped.
 */
int
pr_allstopped(proc_t *p)
{
	kthread_t *t;
	int rv = 0;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if (p->p_flag & SVFWAIT)	/* waiting for vfork'd child to exec */
		return (-1);

	if ((t = p->p_tlist) != NULL) {
		do {
			if (t == curthread || VSTOPPED(t))
				continue;
			thread_lock(t);
			switch (t->t_state) {
			case TS_ZOMB:
			case TS_STOPPED:
				break;
			case TS_SLEEP:
				if (!(t->t_flag & T_WAKEABLE) ||
				    t->t_wchan0 == NULL)
					rv = 1;
				break;
			default:
				rv = 1;
				break;
			}
			thread_unlock(t);
		} while (rv == 0 && (t = t->t_forw) != p->p_tlist);
	}

	return (rv);
}

/*
 * Cause all lwps in the process to pause (for watchpoint operations).
 */
static void
pauselwps(proc_t *p)
{
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(p != curproc);

	if ((t = p->p_tlist) != NULL) {
		do {
			thread_lock(t);
			t->t_proc_flag |= TP_PAUSE;
			aston(t);
			if (t->t_state == TS_SLEEP &&
			    (t->t_flag & T_WAKEABLE)) {
				if (t->t_wchan0 == NULL)
					setrun_locked(t);
			}
			prpokethread(t);
			thread_unlock(t);
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * undo the effects of pauselwps()
 */
static void
unpauselwps(proc_t *p)
{
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(p != curproc);

	if ((t = p->p_tlist) != NULL) {
		do {
			thread_lock(t);
			t->t_proc_flag &= ~TP_PAUSE;
			if (t->t_state == TS_STOPPED) {
				t->t_schedflag |= TS_UNPAUSE;
				setrun_locked(t);
			}
			thread_unlock(t);
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * Cancel all watched areas.  Called from prclose().
 */
proc_t *
pr_cancel_watch(prnode_t *pnp)
{
	proc_t *p = pnp->pr_pcommon->prc_proc;
	struct as *as;
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock) && (p->p_flag & SPRLOCK));

	if (p->p_warea == NULL)		/* no watchpoints */
		return (p);

	/*
	 * Pause the process before dealing with the watchpoints.
	 */
	if (p == curproc) {
		prunlock(pnp);
		while (holdwatch() == 0)
			;
		p = pr_p_lock(pnp);
		mutex_exit(&pr_pidlock);
		ASSERT(p == curproc);
	} else {
		pauselwps(p);
		while (p != NULL && pr_allstopped(p) > 0) {
			/*
			 * This cv/mutex pair is persistent even
			 * if the process disappears after we
			 * unmark it and drop p->p_lock.
			 */
			kcondvar_t *cv = &pr_pid_cv[p->p_slot];
			kmutex_t *mp = &p->p_lock;

			prunmark(p);
			(void) cv_wait(cv, mp);
			mutex_exit(mp);
			p = pr_p_lock(pnp);  /* NULL if process disappeared */
			mutex_exit(&pr_pidlock);
		}
	}

	if (p == NULL)		/* the process disappeared */
		return (NULL);

	ASSERT(p == pnp->pr_pcommon->prc_proc);
	ASSERT(MUTEX_HELD(&p->p_lock) && (p->p_flag & SPRLOCK));

	if (p->p_warea != NULL) {
		pr_free_watchlist(p->p_warea);
		p->p_warea = NULL;
		p->p_nwarea = 0;
		if ((t = p->p_tlist) != NULL) {
			do {
				t->t_proc_flag &= ~TP_WATCHPT;
				t->t_copyops = &default_copyops;
			} while ((t = t->t_forw) != p->p_tlist);
		}
	}

	if ((as = p->p_as) != NULL) {
		struct watched_page *pwp_first;
		struct watched_page *pwp;

		/*
		 * If this is the parent of a vfork, the watched page
		 * list has been moved temporarily to p->p_wpage.
		 */
		pwp = pwp_first = p->p_wpage? p->p_wpage : as->a_wpage;
		if (pwp != NULL) {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			do {
				pwp->wp_read = 0;
				pwp->wp_write = 0;
				pwp->wp_exec = 0;
				if (pwp->wp_oprot != 0)
					pwp->wp_flags |= WP_SETPROT;
				pwp->wp_prot = pwp->wp_oprot;
			} while ((pwp = pwp->wp_forw) != pwp_first);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
	}

	/*
	 * Unpause the process now.
	 */
	if (p == curproc)
		continuelwps(p);
	else
		unpauselwps(p);

	return (p);
}
