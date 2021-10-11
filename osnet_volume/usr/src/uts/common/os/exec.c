/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)exec.c	1.121	99/11/20 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/acct.h>
#include <sys/cpuvar.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/vm.h>
#include <sys/vtrace.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/kmem.h>
#include <sys/prsystm.h>
#include <sys/modctl.h>
#include <sys/vmparam.h>
#include <sys/schedctl.h>
#include <sys/utrap.h>
#include <sys/systeminfo.h>
#include <sys/stack.h>
#include <c2/audit.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>

int nullmagic = 0;		/* null magic number */

static int execsetid(struct vnode *, struct vattr *, uid_t *, uid_t *);
static int hold_execsw(struct execsw *);

int auxv_hwcap = 0;	/* auxv AT_SUN_HWCAP value; determined on the fly */
int kauxv_hwcap = 0;	/* analogous kernel version of the same flag */

#if defined(__i386) || defined(__ia64)
extern void ldt_free(proc_t *pp);
#endif /* defined(__i386) || defined(__ia64) */

/*
 * exec() - wrapper around exece providing NULL environment pointer
 */
int
exec(const char *fname, const char **argp)
{
	return (exece(fname, argp, NULL));
}

/*
 * exece() - system call wrapper around exec_common()
 */
int
exece(const char *fname, const char **argp, const char **envp)
{
	int error;

	error = exec_common(fname, argp, envp);
	return (error ? (set_errno(error)) : 0);
}

int
exec_common(const char *fname, const char **argp, const char **envp)
{
	vnode_t *vp = NULL;
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct user *up = PTOU(p);
	long execsz;		/* temporary count of exec size */
	int i;
	int error = 0;
	char exec_file[MAXCOMLEN+1];
	struct pathname pn;
	struct pathname resolvepn;
	struct uarg args;
	struct execa ua;
	k_sigset_t savedmask;

	/*
	 * exec() is not supported for the /proc agent lwp
	 * or for the aslwp.  Return error.
	 */
	if (curthread == p->p_agenttp)
		return (ENOTSUP);
	if (curthread == p->p_aslwptp)
		return (EACCES);

	/*
	 * Inform /proc that an exec() has started.
	 * Hold signals that are ignored by default so that we will
	 * not be interrupted by a signal that will be ignored after
	 * successful completion of gexec().
	 */
	mutex_enter(&p->p_lock);
	prexecstart();
	savedmask = curthread->t_hold;
	sigorset(&curthread->t_hold, &ignoredefault);
	mutex_exit(&p->p_lock);

	/*
	 * Look up path name and remember last component for later.
	 */
	if (error = pn_get((char *)fname, UIO_USERSPACE, &pn))
		goto out;
	pn_alloc(&resolvepn);
	if (error = lookuppn(&pn, &resolvepn, FOLLOW, NULLVPP, &vp)) {
		pn_free(&resolvepn);
		pn_free(&pn);
		goto out;
	}
	bzero(exec_file, MAXCOMLEN+1);
	(void) strncpy(exec_file, pn.pn_path, MAXCOMLEN);
	bzero(&args, sizeof (args));
	args.pathname = resolvepn.pn_path;
	/* don't free resolvepn until we are done with args */
	pn_free(&pn);

	CPU_STAT_ADD_K(cpu_sysinfo.sysexec, 1);

	ua.fname = fname;
	ua.argp = argp;
	ua.envp = envp;

	if (error = gexec(&vp, &ua, &args, NULL, 0, &execsz,
	    exec_file, p->p_cred))
		goto done;
	/*
	 * Free floating point registers (sun4u only)
	 */
	if (lwp)
		lwp_freeregs(lwp, 1);
	/*
	 * Free device context
	 */
	if (curthread->t_ctx)
		freectx(curthread, 1);

	up->u_execsz = execsz;	/* dependent portion should have checked */

	/*
	 * Remember file name for accounting.
	 */
	up->u_acflag &= ~AFORK;
	bcopy(exec_file, up->u_comm, MAXCOMLEN+1);

	/*
	 * Reset stack state to the user stack, clear set of signals
	 * caught on the signal stack, and reset list of signals that
	 * restart system calls; the new program's environment should
	 * not be affected by detritus from the old program.  Any
	 * pending held signals remain held, so don't clear t_hold.
	 */
	mutex_enter(&p->p_lock);
	lwp->lwp_oldcontext = 0;
	sigemptyset(&up->u_signodefer);
	sigemptyset(&up->u_sigonstack);
	sigemptyset(&up->u_sigresethand);
	lwp->lwp_sigaltstack.ss_sp = 0;
	lwp->lwp_sigaltstack.ss_size = 0;
	lwp->lwp_sigaltstack.ss_flags = SS_DISABLE;

#ifdef _LP64
	/*
	 * Make saved resource limit == current resource limit
	 * for all resources.
	 */
	for (i = 0; i < RLIM_NLIMITS; i++)
		up->u_saved_rlimit[i] = up->u_rlimit[i];
#else
	/*
	 * Make saved resource limit == current resource limit
	 * for file size. (See Large File Summit API)
	 */
	up->u_saved_lf_rlimit = up->u_rlimit[RLIMIT_FSIZE];
#endif

	/*
	 * If the action was to catch the signal, then the action
	 * must be reset to SIG_DFL.
	 */
	for (i = 1; i < NSIG; i++) {
		if (up->u_signal[i - 1] != SIG_DFL &&
		    up->u_signal[i - 1] != SIG_IGN) {
			up->u_signal[i - 1] = SIG_DFL;
			sigemptyset(&up->u_sigmask[i - 1]);
			if (sigismember(&ignoredefault, i)) {
				sigdelq(p, NULL, i);
				sigdelq(p, p->p_tlist, i);
			}
		}
	}
	sigorset(&p->p_ignore, &ignoredefault);
	sigdiffset(&p->p_siginfo, &ignoredefault);
	sigdiffset(&p->p_sig, &ignoredefault);
	sigdiffset(&curthread->t_sig, &ignoredefault);
	p->p_flag &= ~(SNOWAIT|SJCTL|SWAITSIG);
	p->p_flag |= SEXECED;
	up->u_signal[SIGCLD - 1] = SIG_DFL;

	/*
	 * Reset lwp id and lwp count to default.
	 * This is a single-threaded process now
	 * and lwp #1 is lwp_wait()able by default.
	 */
	curthread->t_tid = 1;
	curthread->t_proc_flag |= TP_TWAIT;
	p->p_lwptotal = 1;

	/*
	 * Delete the dot4 sigqueues/signotifies.
	 */
	sigqfree(p);

	mutex_exit(&p->p_lock);

	mutex_enter(&p->p_pflock);
	p->p_prof.pr_base = NULL;
	p->p_prof.pr_size = 0;
	p->p_prof.pr_off = 0;
	p->p_prof.pr_scale = 0;
	p->p_prof.pr_samples = 0;
	mutex_exit(&p->p_pflock);

	/*
	 * Remove schedctl data.
	 */
	if (curthread->t_schedctl != NULL)
		schedctl_cleanup(curthread);

#if defined(__i386)
	/* If the process uses a private LDT then change it to default */
	if (p->p_ldt)
		ldt_free(p);
#elif defined(__ia64)
	if (is_ia32_process(p)) {
		extern union hardware_descriptor *ldt_default;

		/* If the process was using a private LDT then free it */
		if (p->p_ldt)
			ldt_free(p);
		(void) map_ia32_ldt(p->p_as, (caddr_t)ldt_default,
				    (caddr_t)IA32_LDT_ADDR,
				    MINLDTSZ * sizeof (struct dscr));
	}
#endif /* defined(__i386) || defined(__ia64) */
#if defined(__sparcv9cpu)
	if (p->p_utraps != NULL)
		utrap_free(p);
#endif

	/*
	 * Close all close-on-exec files.
	 */
	close_exec(P_FINFO(p));
#ifdef TRACE
	trace_process_name((ulong_t)(p->p_pid), u.u_psargs);
#endif	/* TRACE */
	TRACE_3(TR_FAC_PROC, TR_PROC_EXEC, "proc_exec:pid %d as %x name %s",
		p->p_pid, p->p_as, up->u_psargs);
	setregs();

	/* Mark this is an executable vnode */
	mutex_enter(&vp->v_lock);
	vp->v_flag |= VVMEXEC;
	mutex_exit(&vp->v_lock);

done:
	VN_RELE(vp);
	pn_free(&resolvepn);
out:
	/*
	 * Restore the saved signal mask and
	 * inform /proc that the exec() has finished.
	 */
	mutex_enter(&p->p_lock);
	curthread->t_hold = savedmask;
	prexecend();
	mutex_exit(&p->p_lock);
	return (error);
}


/*
 * Perform generic exec duties and switchout to object-file specific
 * handler.
 */
int
gexec(
	struct vnode **vpp,
	struct execa *uap,
	struct uarg *args,
	struct intpdata *idatap,
	int level,
	long *execsz,
	caddr_t exec_file,
	struct cred *cred)
{
	struct vnode *vp;
	proc_t *pp = ttoproc(curthread);
	struct execsw *eswp;
	int error = 0;
	int nocd_flag = 0;
	ssize_t resid;
	uid_t uid, gid;
	struct vattr vattr;
	char magbuf[MAGIC_BYTES];
	int setid;
	struct cred *newcred = NULL;

	/*
	 * If the NOCD flag is set, turn it off and remember the previous
	 * setting so we can restore it if we encounter an error.
	 */
	if (level == 0 && (pp->p_flag & NOCD)) {
		nocd_flag = 1;
		mutex_enter(&pp->p_lock);
		pp->p_flag &= ~NOCD;
		mutex_exit(&pp->p_lock);
	}

	if ((error = execpermissions(*vpp, &vattr, args)) != 0)
		goto bad;

	/* need to open vnode for stateful file systems like rfs */
	if ((error = VOP_OPEN(vpp, FREAD, CRED())) != 0)
		goto bad;
	vp = *vpp;

	/*
	 * Note: to support binary compatibility with SunOS a.out
	 * executables, we read in the first four bytes, as the
	 * magic number is in bytes 2-3.
	 */
	if (error = vn_rdwr(UIO_READ, vp, magbuf, sizeof (magbuf),
	    (offset_t)0, UIO_SYSSPACE, 0, (rlim64_t)0, CRED(), &resid))
		goto bad;
	if (resid != 0)
		goto bad;

	if ((eswp = findexec_by_hdr(magbuf)) == NULL)
		goto bad;

	if (level == 0 && execsetid(vp, &vattr, &uid, &gid)) {
		newcred = crdup(cred);
		newcred->cr_uid = uid;
		newcred->cr_gid = gid;
		newcred->cr_suid = uid;
		newcred->cr_sgid = gid;
		cred = newcred;
	}

	/* SunOS 4.x buy-back */
	if ((vp->v_vfsp->vfs_flag & VFS_NOSUID) &&
	    (vattr.va_mode & (VSUID|VSGID))) {
		cmn_err(CE_NOTE,
		    "!%s, uid %d: setuid execution not allowed, dev=%lx",
		    exec_file, cred->cr_uid, vp->v_vfsp->vfs_dev);
	}

	/*
	 * execsetid() told us whether or not we had to change the
	 * credentials of the process.  It did not tell us whether
	 * the executable is marked setid.  We determine that here.
	 */
	setid = (vp->v_vfsp->vfs_flag & VFS_NOSUID) == 0 &&
	    (vattr.va_mode & (VSUID|VSGID)) != 0;

	args->execswp = eswp; /* Save execsw pointer in uarg for exec_func */

	error = (*eswp->exec_func)(vp, uap, args, idatap, level,
	    execsz, setid, exec_file, cred);
	rw_exit(eswp->exec_lock);
	if (error != 0) {
		if (newcred != NULL)
			crfree(newcred);
		goto bad;
	}

	if (level == 0) {
		mutex_enter(&pp->p_crlock);
		if (newcred != NULL) {
			/*
			 * Free the old credentials, and set the new ones.
			 * Do this for both the process and the (single) thread.
			 */
			crfree(pp->p_cred);
			pp->p_cred = cred;	/* cred already held for proc */
			crhold(cred);		/* hold new cred for thread */
			crfree(curthread->t_cred);
			curthread->t_cred = cred;
		}
		/*
		 * On emerging from a successful exec(), the saved
		 * uid and gid equal the effective uid and gid.
		 */
		cred->cr_suid = cred->cr_uid;
		cred->cr_sgid = cred->cr_gid;
		/*
		 * If the real and effective ids do not match, this
		 * is a setuid process that should not dump core.
		 */
		nocd_flag = (cred->cr_ruid != cred->cr_uid ||
			!groupmember(cred->cr_rgid, cred));
		mutex_exit(&pp->p_crlock);
		if (nocd_flag) {
			mutex_enter(&pp->p_lock);
			pp->p_flag |= NOCD;
			mutex_exit(&pp->p_lock);
		}
		if (setid && (pp->p_flag & STRC) == 0) {
			/*
			 * If process is traced via /proc, arrange to
			 * invalidate the associated /proc vnode.
			 */
			if (pp->p_plist || (pp->p_flag & SPROCTR))
				args->traceinval = 1;
		}
		if (pp->p_flag & STRC)
			psignal(pp, SIGTRAP);
		if (args->traceinval)
			prinvalidate(&pp->p_user);
	}
	return (0);
bad:
	if (error == 0)
		error = ENOEXEC;
	if (nocd_flag) {
		mutex_enter(&pp->p_lock);
		pp->p_flag |= NOCD;
		mutex_exit(&pp->p_lock);
	}
	return (error);
}

extern char *execswnames[];

struct execsw *
allocate_execsw(char *name, char *magic, size_t magic_size)
{
	int i, j;
	char *ename;
	char *magicp;

	mutex_enter(&execsw_lock);
	for (i = 0; i < nexectype; i++) {
		if (execswnames[i] == NULL) {
			ename = kmem_alloc(strlen(name) + 1, KM_SLEEP);
			(void) strcpy(ename, name);
			execswnames[i] = ename;
			/*
			 * Set the magic number last so that we
			 * don't need to hold the execsw_lock in
			 * findexectype().
			 */
			magicp = kmem_alloc(magic_size, KM_SLEEP);
			for (j = 0; j < magic_size; j++)
				magicp[j] = magic[j];
			execsw[i].exec_magic = magicp;
			mutex_exit(&execsw_lock);
			return (&execsw[i]);
		}
	}
	mutex_exit(&execsw_lock);
	return (NULL);
}

/*
 * Find the exec switch table entry with the corresponding magic string.
 */
struct execsw *
findexecsw(char *magic)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (magic && eswp->exec_maglen != 0 &&
		    bcmp(magic, eswp->exec_magic, eswp->exec_maglen) == 0)
			return (eswp);
	}
	return (NULL);
}

/*
 * Find the execsw[] index for the given exec header string by looking for the
 * magic string at a specified offset and length for each kind of executable
 * file format until one matches.  If no execsw[] entry is found, try to
 * autoload a module for this magic string.
 */
struct execsw *
findexec_by_hdr(char *header)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (header && eswp->exec_maglen != 0 &&
		    bcmp(&header[eswp->exec_magoff], eswp->exec_magic,
			    eswp->exec_maglen) == 0) {
			if (hold_execsw(eswp) != 0)
				return (NULL);
			return (eswp);
		}
	}
	return (NULL);	/* couldn't find the type */
}

/*
 * Find the execsw[] index for the given magic string.  If no execsw[] entry
 * is found, try to autoload a module for this magic string.
 */
struct execsw *
findexec_by_magic(char *magic)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (magic && eswp->exec_maglen != 0 &&
		    bcmp(magic, eswp->exec_magic, eswp->exec_maglen) == 0) {
			if (hold_execsw(eswp) != 0)
				return (NULL);
			return (eswp);
		}
	}
	return (NULL);	/* couldn't find the type */
}

static int
hold_execsw(struct execsw *eswp)
{
	char *name;

	rw_enter(eswp->exec_lock, RW_READER);
	while (!LOADED_EXEC(eswp)) {
		rw_exit(eswp->exec_lock);
		name = execswnames[eswp-execsw];
		ASSERT(name);
		if (modload("exec", name) == -1)
			return (-1);
		rw_enter(eswp->exec_lock, RW_READER);
	}
	return (0);
}

static int
execsetid(struct vnode *vp, struct vattr *vattrp, uid_t *uidp, uid_t *gidp)
{
	proc_t *pp = ttoproc(curthread);
	uid_t uid, gid;

	/*
	 * Remember credentials.
	 */
	uid = pp->p_cred->cr_uid;
	gid = pp->p_cred->cr_gid;

	if ((vp->v_vfsp->vfs_flag & VFS_NOSUID) == 0) {
		if (vattrp->va_mode & VSUID)
			uid = vattrp->va_uid;
		if (vattrp->va_mode & VSGID)
			gid = vattrp->va_gid;
	}

	/*
	 * Set setuid/setgid protections if no ptrace() compatibility.
	 * For the super-user, honor setuid/setgid even in
	 * the presence of ptrace() compatibility.
	 */
	if (((pp->p_flag & STRC) == 0 || pp->p_cred->cr_uid == 0) &&
	    (pp->p_cred->cr_uid != uid ||
	    pp->p_cred->cr_gid != gid ||
	    pp->p_cred->cr_suid != uid ||
	    pp->p_cred->cr_sgid != gid)) {
		*uidp = uid;
		*gidp = gid;
		return (1);
	}
	return (0);
}

int
execpermissions(struct vnode *vp, struct vattr *vattrp, struct uarg *args)
{
	int error;
	proc_t *p = ttoproc(curthread);

	vattrp->va_mask = AT_MODE | AT_UID | AT_GID | AT_SIZE;
	if (error = VOP_GETATTR(vp, vattrp, ATTR_EXEC, p->p_cred))
		return (error);
	/*
	 * Check the access mode.
	 * If VPROC, ask /proc if the file is an object file.
	 */
	if ((error = VOP_ACCESS(vp, VEXEC, 0, p->p_cred)) != 0 ||
	    !(vp->v_type == VREG || (vp->v_type == VPROC && pr_isobject(vp))) ||
	    (vattrp->va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0) {
		if (error == 0)
			error = EACCES;
		return (error);
	}

	if ((p->p_plist || (p->p_flag & (STRC|SPROCTR))) &&
	    (error = VOP_ACCESS(vp, VREAD, 0, p->p_cred))) {
		/*
		 * If process is under ptrace(2) compatibility,
		 * fail the exec(2).
		 */
		if (p->p_flag & STRC)
			goto bad;
		/*
		 * Process is traced via /proc.
		 * Arrange to invalidate the /proc vnode.
		 */
		args->traceinval = 1;
	}
	return (0);
bad:
	if (error == 0)
		error = ENOEXEC;
	return (error);
}

/*
 * Map a section of an executable file into the user's
 * address space.
 */
int
execmap(struct vnode *vp, caddr_t addr, size_t len, size_t zfodlen,
    off_t offset, int prot, int page)
{
	int error = 0;
	off_t oldoffset;
	caddr_t zfodbase, oldaddr;
	size_t end, oldlen;
	size_t zfoddiff;
	label_t ljb;
	proc_t *p = ttoproc(curthread);

	oldaddr = addr;
	addr = (caddr_t)((uintptr_t)addr & PAGEMASK);
	if (len) {
		oldlen = len;
		len += ((size_t)oldaddr - (size_t)addr);
		oldoffset = offset;
		offset = (off_t)((uintptr_t)offset & PAGEMASK);
		if (page) {
			spgcnt_t  prefltmem, availm, npages;
			int preread;

			if (valid_usr_range(addr, len, prot, p->p_as,
			    p->p_as->a_userlimit) != RANGE_OKAY) {
				error = ENOMEM;
				goto bad;
			}
			if (error = VOP_MAP(vp, (offset_t)offset,
			    p->p_as, &addr, len, prot, PROT_ALL,
			    MAP_PRIVATE | MAP_FIXED, CRED()))
				goto bad;

			/*
			 * If the segment can fit, then we prefault
			 * the entire segment in.  This is based on the
			 * model that says the best working set of a
			 * small program is all of its pages.
			 */
			npages = (spgcnt_t)btopr(len);
			prefltmem = freemem - desfree;
			preread =
			    (npages < prefltmem && len < PGTHRESH) ? 1 : 0;

			/*
			 * If we aren't prefaulting the segment,
			 * increment "deficit", if necessary to ensure
			 * that pages will become available when this
			 * process starts executing.
			 */
			availm = freemem - lotsfree;
			if (preread == 0 && npages > availm &&
			    deficit < lotsfree) {
				deficit += MIN((pgcnt_t)(npages - availm),
				    lotsfree - deficit);
			}

			if (preread) {
				TRACE_2(TR_FAC_PROC, TR_EXECMAP_PREREAD,
				    "execmap preread:freemem %d size %lu",
				    freemem, len);
				(void) as_fault(p->p_as->a_hat, p->p_as,
				    (caddr_t)addr, len, F_INVAL, S_READ);
			}
#ifdef TRACE
			else {
				TRACE_2(TR_FAC_PROC, TR_EXECMAP_NO_PREREAD,
				    "execmap no preread:freemem %d size %lu",
				    freemem, len);
			}
#endif
		} else {
			if (valid_usr_range(addr, len, prot, p->p_as,
			    p->p_as->a_userlimit) != RANGE_OKAY) {
				error = ENOMEM;
				goto bad;
			}

			if (error = as_map(p->p_as, addr, len,
			    segvn_create, zfod_argsp))
				goto bad;
			/*
			 * Read in the segment in one big chunk.
			 */
			if (error = vn_rdwr(UIO_READ, vp, (caddr_t)oldaddr,
			    oldlen, (offset_t)oldoffset, UIO_USERSPACE, 0,
			    (rlim64_t)0, CRED(), (ssize_t *)0))
				goto bad;
			/*
			 * Now set protections.
			 */
			if (prot != PROT_ALL) {
				(void) as_setprot(p->p_as, (caddr_t)addr,
				    len, prot);
			}
		}
	}

	if (zfodlen) {
		end = (size_t)addr + len;
		zfodbase = (caddr_t)roundup(end, PAGESIZE);
		zfoddiff = (uintptr_t)zfodbase - end;
		if (zfoddiff) {
			if (on_fault(&ljb)) {
				error = EFAULT;
				goto bad;
			}
			(void) uzero((void *)end, zfoddiff);
			no_fault();
		}
		if (zfodlen > zfoddiff) {
			zfodlen -= zfoddiff;
			if (valid_usr_range(zfodbase, zfodlen, prot, p->p_as,
			    p->p_as->a_userlimit) != RANGE_OKAY) {
				error = ENOMEM;
				goto bad;
			}
			if (error = as_map(p->p_as, (caddr_t)zfodbase,
			    zfodlen, segvn_create, zfod_argsp))
				goto bad;
			if (prot != PROT_ALL) {
				(void) as_setprot(p->p_as, (caddr_t)zfodbase,
				    zfodlen, prot);
			}
		}
	}
	return (0);
bad:
	return (error);
}

void
setexecenv(struct execenv *ep)
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct vnode *vp;

	p->p_brkbase = ep->ex_brkbase;
	p->p_brksize = ep->ex_brksize;
	if (p->p_exec)
		VN_RELE(p->p_exec);	/* out with the old */
	vp = p->p_exec = ep->ex_vp;
	if (vp != NULL)
		VN_HOLD(vp);		/* in with the new */

	lwp->lwp_sigaltstack.ss_sp = 0;
	lwp->lwp_sigaltstack.ss_size = 0;
	lwp->lwp_sigaltstack.ss_flags = SS_DISABLE;

	p->p_user.u_execid = (int)ep->ex_magic;
}

int
execopen(struct vnode **vpp, int *fdp)
{
	struct vnode *vp = *vpp;
	file_t *fp;
	int error = 0;
	int filemode = FREAD;

	VN_HOLD(vp);		/* open reference */
	if (error = falloc(NULL, filemode, &fp, fdp)) {
		VN_RELE(vp);
		*fdp = -1;	/* just in case falloc changed value */
		return (error);
	}
	if (error = VOP_OPEN(&vp, filemode, CRED())) {
		VN_RELE(vp);
		setf(*fdp, NULL);
		unfalloc(fp);
		*fdp = -1;
		return (error);
	}
	*vpp = vp;		/* vnode should not have changed */
	fp->f_vnode = vp;
	mutex_exit(&fp->f_tlock);
	setf(*fdp, fp);
	return (0);
}

int
execclose(int fd)
{
	return (closeandsetf(fd, NULL));
}


/*
 * noexec stub function.
 */
/*ARGSUSED*/
int
noexec(
    struct vnode *vp,
    struct execa *uap,
    struct uarg *args,
    struct intpdata *idatap,
    int level,
    long *execsz,
    int setid,
    caddr_t exec_file,
    struct cred *cred)
{
	cmn_err(CE_WARN, "missing exec capability for %s", uap->fname);
	return (ENOEXEC);
}

/*
 * platform-specific cpu partition hook for exec_buildstack
 */
void (*cpupart_exec_hook)() = NULL;

/*
 * Support routines for building a user stack.
 *
 * execve(path, argv, envp) must construct a new stack with the specified
 * arguments and environment variables (see exec_args() for a description
 * of the user stack layout).  To do this, we copy the arguments and
 * environment variables from the old user address space into the kernel,
 * free the old as, create the new as, and copy our buffered information
 * to the new stack.  Our kernel buffer has the following structure:
 *
 *	+-----------------------+ <--- stk_base + stk_size
 *	| string offsets	|
 *	+-----------------------+ <--- stk_offp
 *	|			|
 *	| STK_AVAIL() space	|
 *	|			|
 *	+-----------------------+ <--- stk_strp
 *	| strings		|
 *	+-----------------------+ <--- stk_base
 *
 * When we add a string, we store the string's contents (including the null
 * terminator) at stk_strp, and we store the offset of the string relative to
 * stk_base at --stk_offp.  At strings are added, stk_strp increases and
 * stk_offp decreases.  The amount of space remaining, STK_AVAIL(), is just
 * the difference between these pointers.  If we run out of space, we return
 * an error and exec_args() starts all over again with a buffer twice as large.
 * When we're all done, the kernel buffer looks like this:
 *
 *	+-----------------------+ <--- stk_base + stk_size
 *	| argv[0] offset	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| argv[argc-1] offset	|
 *	+-----------------------+
 *	| envp[0] offset	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| envp[envc-1] offset	|
 *	+-----------------------+
 *	| AT_SUN_PLATFORM offset|
 *	+-----------------------+
 *	| AT_SUN_EXECNAME offset|
 *	+-----------------------+ <--- stk_offp
 *	|			|
 *	| STK_AVAIL() space	|
 *	|			|
 *	+-----------------------+ <--- stk_strp
 *	| AT_SUN_EXECNAME offset|
 *	+-----------------------+
 *	| AT_SUN_PLATFORM offset|
 *	+-----------------------+
 *	| envp[envc-1] string	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| envp[0] string	|
 *	+-----------------------+
 *	| argv[argc-1] string	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| argv[0] string	|
 *	+-----------------------+ <--- stk_base
 */

#define	STK_AVAIL(args)		((char *)(args)->stk_offp - (args)->stk_strp)

/*
 * Add a string to the stack.
 */
static int
stk_add(uarg_t *args, const char *sp, enum uio_seg segflg)
{
	int error;
	size_t len;

	if (STK_AVAIL(args) < sizeof (int))
		return (E2BIG);
	*--args->stk_offp = args->stk_strp - args->stk_base;

	if (segflg == UIO_USERSPACE) {
		error = copyinstr(sp, args->stk_strp, STK_AVAIL(args), &len);
		if (error != 0)
			return (error);
	} else {
		len = strlen(sp) + 1;
		if (len > STK_AVAIL(args))
			return (E2BIG);
		bcopy(sp, args->stk_strp, len);
	}

	args->stk_strp += len;

	return (0);
}

static int
stk_getptr(uarg_t *args, char *src, char **dst)
{
	int error;

	if (args->from_model == DATAMODEL_NATIVE) {
		ulong_t ptr;
		error = fulword(src, &ptr);
		*dst = (caddr_t)ptr;
	} else {
		uint32_t ptr;
		error = fuword32(src, &ptr);
		*dst = (caddr_t)ptr;
	}
	return (error);
}

static int
stk_putptr(uarg_t *args, char *addr, char *value)
{
	if (args->to_model == DATAMODEL_NATIVE)
		return (sulword(addr, (ulong_t)value));
	else
		return (suword32(addr, (uint32_t)value));
}

static int
stk_copyin(execa_t *uap, uarg_t *args, intpdata_t *intp, void **auxvpp)
{
	char *sp;
	int argc, error;
	size_t ptrsize = args->from_ptrsize;
	size_t size, pad;
	char *argv = (char *)uap->argp;
	char *envp = (char *)uap->envp;

	/*
	 * Copy interpreter's name and argument to argv[0] and argv[1].
	 */
	if (intp != NULL && intp->intp_name != NULL) {
		if ((error = stk_add(args, intp->intp_name, UIO_SYSSPACE)) != 0)
			return (error);
		if (intp->intp_arg != NULL &&
		    (error = stk_add(args, intp->intp_arg, UIO_SYSSPACE)) != 0)
			return (error);
		if (args->fname != NULL)
			error = stk_add(args, args->fname, UIO_SYSSPACE);
		else
			error = stk_add(args, uap->fname, UIO_USERSPACE);
		if (error)
			return (error);
		argv += ptrsize;		/* ignore original argv[0] */
	}

	/*
	 * Add argv[] strings to the stack.
	 */
	for (;;) {
		if (stk_getptr(args, argv, &sp))
			return (EFAULT);
		if (sp == NULL)
			break;
		if ((error = stk_add(args, sp, UIO_USERSPACE)) != 0)
			return (error);
		argv += ptrsize;
	}
	argc = (int *)(args->stk_base + args->stk_size) - args->stk_offp;
	args->arglen = args->stk_strp - args->stk_base;

	/*
	 * Add environ[] strings to the stack.
	 */
	if (envp != NULL) {
		for (;;) {
			if (stk_getptr(args, envp, &sp))
				return (EFAULT);
			if (sp == NULL)
				break;
			if ((error = stk_add(args, sp, UIO_USERSPACE)) != 0)
				return (error);
			envp += ptrsize;
		}
	}
	args->na = (int *)(args->stk_base + args->stk_size) - args->stk_offp;
	args->ne = args->na - argc;

	/*
	 * Add AT_SUN_PLATFORM and AT_SUN_EXECNAME strings to the stack.
	 */
	if (auxvpp != NULL && *auxvpp != NULL) {
		if ((error = stk_add(args, platform, UIO_SYSSPACE)) != 0)
			return (error);
		if ((error = stk_add(args, args->pathname, UIO_SYSSPACE)) != 0)
			return (error);
	}

	/*
	 * Compute the size of the stack.  This includes all the pointers,
	 * the space reserved for the aux vector, and all the strings.
	 * The total number of pointers is args->na (which is argc + envc)
	 * plus 4 more: (1) a pointer's worth of space for argc; (2) the NULL
	 * after the last argument (i.e. argv[argc]); (3) the NULL after the
	 * last environment variable (i.e. envp[envc]); and (4) the NULL after
	 * all the strings, at the very top of the stack.
	 */
	size = (args->na + 4) * args->to_ptrsize + args->auxsize +
	    (args->stk_strp - args->stk_base);

	/*
	 * Pad the string section with zeroes to align the stack size.
	 */
	pad = P2NPHASE(size, args->stk_align);

	if (STK_AVAIL(args) < pad)
		return (E2BIG);

	args->usrstack_size = size + pad;

	while (pad-- != 0)
		*args->stk_strp++ = 0;

	args->nc = args->stk_strp - args->stk_base;

	return (0);
}

static int
stk_copyout(uarg_t *args, char *usrstack, void **auxvpp, user_t *up)
{
	size_t ptrsize = args->to_ptrsize;
	ssize_t pslen;
	char *kstrp = args->stk_base;
	char *ustrp = usrstack - args->nc - ptrsize;
	char *usp = usrstack - args->usrstack_size;
	int *offp = (int *)(args->stk_base + args->stk_size);
	int envc = args->ne;
	int argc = args->na - envc;
	int i;

	/*
	 * Record argc for /proc.
	 */
	up->u_argc = argc;

	/*
	 * Put argc on the stack.  Note that even though it's an int,
	 * it always consumes ptrsize bytes (for alignment).
	 */
	if (stk_putptr(args, usp, (char *)argc))
		return (-1);

	/*
	 * Add argc space (ptrsize) to usp and record argv for /proc.
	 */
	up->u_argv = (uintptr_t)(usp += ptrsize);

	/*
	 * Put the argv[] pointers on the stack.
	 */
	for (i = 0; i < argc; i++, usp += ptrsize)
		if (stk_putptr(args, usp, &ustrp[*--offp]))
			return (-1);

	/*
	 * Copy arguments to u_psargs.
	 */
	pslen = MIN(args->arglen, PSARGSZ) - 1;
	for (i = 0; i < pslen; i++)
		up->u_psargs[i] = (kstrp[i] == '\0' ? ' ' : kstrp[i]);
	while (i < PSARGSZ)
		up->u_psargs[i++] = '\0';

	/*
	 * Add space for argv[]'s NULL terminator (ptrsize) to usp and
	 * record envp for /proc.
	 */
	up->u_envp = (uintptr_t)(usp += ptrsize);

	/*
	 * Put the envp[] pointers on the stack.
	 */
	for (i = 0; i < envc; i++, usp += ptrsize)
		if (stk_putptr(args, usp, &ustrp[*--offp]))
			return (-1);

	/*
	 * Add space for envp[]'s NULL terminator (ptrsize) to usp and
	 * remember where the stack ends, which is also where auxv begins.
	 */
	args->stackend = usp += ptrsize;

	/*
	 * Put all the argv[], envp[], and auxv strings on the stack.
	 */
	if (copyout(args->stk_base, ustrp, args->nc))
		return (-1);

	/*
	 * Fill in the aux vector now that we know the user stack addresses
	 * for the AT_SUN_PLATFORM and AT_SUN_EXECNAME strings.
	 */
	if (auxvpp != NULL && *auxvpp != NULL) {
		if (args->to_model == DATAMODEL_NATIVE) {
			auxv_t **a = (auxv_t **)auxvpp;
			ADDAUX(*a, AT_SUN_PLATFORM, (long)&ustrp[*--offp]);
			ADDAUX(*a, AT_SUN_EXECNAME, (long)&ustrp[*--offp]);
		} else {
			auxv32_t **a = (auxv32_t **)auxvpp;
			ADDAUX(*a, AT_SUN_PLATFORM, (int)&ustrp[*--offp]);
			ADDAUX(*a, AT_SUN_EXECNAME, (int)&ustrp[*--offp]);
		}
	}

	return (0);
}

/*
 * Initialize a new user stack with the specified arguments and environment.
 * The initial user stack layout is as follows:
 *
 *	User Stack
 *	+---------------+ <--- curproc->p_usrstack
 *	| NULL		|
 *	+---------------+
 *	|		|
 *	| auxv strings	|
 *	|		|
 *	+---------------+
 *	|		|
 *	| envp strings	|
 *	|		|
 *	+---------------+
 *	|		|
 *	| argv strings	|
 *	|		|
 *	+---------------+ <--- ustrp
 *	|		|
 *	| aux vector	|
 *	|		|
 *	+---------------+ <--- auxv
 *	| NULL		|
 *	+---------------+
 *	| envp[envc-1]	|
 *	+---------------+
 *	| ...		|
 *	+---------------+
 *	| envp[0]	|
 *	+---------------+ <--- envp[]
 *	| NULL		|
 *	+---------------+
 *	| argv[argc-1]	|
 *	+---------------+
 *	| ...		|
 *	+---------------+
 *	| argv[0]	|
 *	+---------------+ <--- argv[]
 *	| argc		|
 *	+---------------+ <--- stack base
 */
int
exec_args(execa_t *uap, uarg_t *args, intpdata_t *intp, void **auxvpp)
{
	size_t size;
	int error;
	proc_t *p = ttoproc(curthread);
	user_t *up = PTOU(p);
	char *usrstack;
	struct as *as;
	void (*funcp)();

	args->from_model = p->p_model;
	if (p->p_model == DATAMODEL_NATIVE) {
		args->from_ptrsize = sizeof (long);
	} else {
		args->from_ptrsize = sizeof (int32_t);
	}

	if (args->to_model == DATAMODEL_NATIVE) {
		args->to_ptrsize = sizeof (long);
		args->ncargs = NCARGS;
		args->stk_align = STACK_ALIGN;
		usrstack = (char *)USRSTACK;
	} else {
		args->to_ptrsize = sizeof (int32_t);
		args->ncargs = NCARGS32;
		args->stk_align = STACK_ALIGN32;
		usrstack = (char *)USRSTACK32;
	}

	ASSERT(P2PHASE((uintptr_t)usrstack, args->stk_align) == 0);

#ifdef sparc
	/*
	 * Make sure user register windows are empty before
	 * attempting to make a new stack.
	 */
	(void) flush_user_windows_to_stack(NULL);
#endif

	for (size = PAGESIZE; ; size *= 2) {
		args->stk_size = size;
		args->stk_base = kmem_alloc(size, KM_SLEEP);
		args->stk_strp = args->stk_base;
		args->stk_offp = (int *)(args->stk_base + size);
		error = stk_copyin(uap, args, intp, auxvpp);
		if (error == 0)
			break;
		kmem_free(args->stk_base, size);
		if (error != E2BIG && error != ENAMETOOLONG)
			return (error);
		if (size >= args->ncargs)
			return (E2BIG);
	}

	size = args->usrstack_size;

	ASSERT(error == 0);
	ASSERT(P2PHASE(size, args->stk_align) == 0);
	ASSERT((ssize_t)STK_AVAIL(args) >= 0);

	if (size > args->ncargs) {
		kmem_free(args->stk_base, args->stk_size);
		return (E2BIG);
	}

	/*
	 * Leave only the current lwp and force the other lwps to exit.
	 * Since exitlwps() waits until all other lwps are dead, if the
	 * calling process has an aslwp, all pending signals from it will
	 * be transferred to this process before continuing past this call.
	 * If another lwp beat us to the punch by calling exit(), bail out.
	 */
	if ((error = exitlwps(0)) != 0) {
		kmem_free(args->stk_base, args->stk_size);
		return (error);
	}

	ASSERT((p->p_flag & ASLWP) == 0);
	ASSERT(p->p_aslwptp == NULL);

	/*
	 * Revoke any doors created by the process.
	 */
	if (p->p_door_list)
		door_exit();

	/*
	 * Release scheduler activations door (if any).
	 */
	if (p->p_sc_door) {
		VN_RELE(p->p_sc_door);
		p->p_sc_door = NULL;
	}

	/*
	 * Delete the POSIX timers.
	 */
	if (p->p_itimer != NULL)
		timer_exit();

#if defined(__ia64)
	if (is_ia32_process(p) && ! (p->p_flag & SVFORK))
		unmap_ia32_ldt(p->p_as, (caddr_t)IA32_LDT_ADDR,
		    (p->p_ldtlimit + 1) * sizeof (struct dscr));
#endif /* defined(__ia64) */

#ifdef C2_AUDIT
	if (audit_active)
		audit_exec(args->stk_base, args->stk_base + args->arglen,
		    args->na - args->ne, args->ne);
#endif
	/*
	 * Destroy the old address space and create a new one.
	 * From here on, any errors are fatal to the exec()ing process.
	 * On error we return -1, which means the caller must SIGKILL
	 * the process.
	 */
	relvm();

	up->u_execsw = args->execswp;

	p->p_brkbase = NULL;
	p->p_brksize = 0;
	p->p_stksize = 0;
	p->p_model = args->to_model;
	p->p_usrstack = usrstack;

	if (p->p_model == DATAMODEL_LP64 || noexec_user_stack)
		p->p_stkprot = PROT_ZFOD & ~PROT_EXEC;
	else
		p->p_stkprot = PROT_ZFOD;

	/*
	 * Call platform-specific cpu partition hook (if set).  We may
	 * want to move to another partition now that our old address
	 * space is gone.
	 */
	if ((funcp = cpupart_exec_hook) != NULL)
		(*funcp)();

	exec_set_sp(size);

	as = as_alloc();
	p->p_as = as;
	if (p->p_model == DATAMODEL_ILP32)
		as->a_userlimit = (caddr_t)USERLIMIT32;
	(void) hat_setup(as->a_hat, HAT_ALLOC);

	/*
	 * Finally, write out the contents of the new stack.
	 */
	error = stk_copyout(args, usrstack, auxvpp, up);
	kmem_free(args->stk_base, args->stk_size);
	return (error);
}
