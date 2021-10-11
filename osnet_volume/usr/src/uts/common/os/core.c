/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)core.c	1.47	99/03/31 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/prsystm.h>
#include <sys/vnode.h>
#include <sys/var.h>
#include <sys/file.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/exec.h>
#include <sys/debug.h>
#include <sys/stack.h>
#include <sys/kmem.h>
#include <sys/core.h>
#include <sys/corectl.h>
#include <sys/cmn_err.h>
#include <vm/as.h>

/*
 * Log information about global core dumps to syslog.
 */
static void
core_log(int error, const char *why, const char *path)
{
	proc_t *p = curproc;
	pid_t pid = p->p_pid;
	char *fn = PTOU(p)->u_comm;

	if (!(core_options & CC_GLOBAL_LOG))
		return;

	if (path == NULL)
		cmn_err(CE_NOTE, "core_log: %s[%d] %s", fn, pid, why);
	else if (error == 0)
		cmn_err(CE_NOTE, "core_log: %s[%d] %s: %s", fn, pid, why, path);
	else
		cmn_err(CE_NOTE, "core_log: %s[%d] %s, errno=%d: %s",
		    fn, pid, why, error, path);
}

/*
 * Private version of vn_remove().
 * Refuse to unlink a directory or an unwritable file.
 */
static int
remove_core_file(char *fp)
{
	vnode_t *vp = NULL;		/* entry vnode */
	vnode_t *dvp;			/* ptr to parent dir vnode */
	pathname_t pn;			/* name of entry */
	vfs_t *dvfsp;
	int error;

	if ((error = pn_get(fp, UIO_SYSSPACE, &pn)) != 0)
		return (error);
	if ((error = lookuppn(&pn, NULL, NO_FOLLOW, &dvp, &vp)) != 0) {
		pn_free(&pn);
		return (error);
	}

	/*
	 * Succeed if there is no file.
	 * Fail if the file is not a regular file.
	 * Fail if the filesystem is mounted read-only.
	 * Fail if the file is not writeable.
	 */
	if (vp == NULL)
		error = 0;
	else if (vp->v_type != VREG)
		error = EACCES;
	else if ((dvfsp = dvp->v_vfsp) != NULL &&
	    (dvfsp->vfs_flag & VFS_RDONLY))
		error = EROFS;
	else if ((error = VOP_ACCESS(vp, VWRITE, 0, CRED())) == 0) {
		VN_RELE(vp);
		vp = NULL;
		error = VOP_REMOVE(dvp, pn.pn_path, CRED());
	}

	pn_free(&pn);
	if (vp != NULL)
		VN_RELE(vp);
	VN_RELE(dvp);
	return (error);
}

/*
 * Install the specified held cred into the process, and return a pointer to
 * the held cred which was previously the value of p->p_cred.
 */
static cred_t *
set_cred(proc_t *p, cred_t *newcr)
{
	cred_t *oldcr = p->p_cred;

	/*
	 * Place a hold on the existing cred, and then install the new
	 * cred into the proc structure.
	 */
	crhold(oldcr);
	mutex_enter(&p->p_crlock);
	p->p_cred = newcr;
	mutex_exit(&p->p_crlock);

	/*
	 * If the real uid is changing, keep the per-user process
	 * counts accurate.
	 */
	if (oldcr->cr_ruid != newcr->cr_ruid) {
		mutex_enter(&pidlock);
		upcount_dec(oldcr->cr_ruid);
		upcount_inc(newcr->cr_ruid);
		mutex_exit(&pidlock);
	}

	/*
	 * Broadcast the new cred to all the other threads.  The old
	 * cred can be safely returned because we have a hold on it.
	 */
	crset(p, newcr);
	return (oldcr);
}

static int
do_core(char *fp, int sig, int global)
{
	proc_t *p = curproc;
	cred_t *credp = CRED();
	rlim64_t rlimit = P_CURLIMIT(p, RLIMIT_CORE);
	vnode_t *vp;
	vattr_t vattr;
	int error = 0;
	struct execsw *eswp;
	mode_t perms = 0600;
	cred_t *ocredp = NULL;
	int is_setid = 0;

	if (rlimit == 0)
		return (EFBIG);

	/*
	 * If NOCD is set, or if the effective, real, and saved ids do
	 * not match up, no one but the super-user is allowed to view
	 * this core file.  Set the credentials and the owner to root.
	 */
	if ((p->p_flag & NOCD) ||
	    credp->cr_uid != credp->cr_ruid ||
	    credp->cr_uid != credp->cr_suid ||
	    credp->cr_gid != credp->cr_rgid ||
	    credp->cr_gid != credp->cr_sgid) {
		/*
		 * Because this is insecure against certain forms of file
		 * system attack, do it only if set-id core files have been
		 * enabled via corectl(CC_GLOBAL_SETID | CC_PROCESS_SETID).
		 */
		if ((global && !(core_options & CC_GLOBAL_SETID)) ||
		    (!global && !(core_options & CC_PROCESS_SETID)))
			return (ENOTSUP);

		is_setid = 1;
	}

	/*
	 * If we are doing a global core dump or a set-id core dump,
	 * duplicate the current cred, change the uids to match kcred,
	 * and then install the new cred into the process for vn_open.
	 */
	if (global || is_setid) {
		credp = crdup(credp);
		credp->cr_uid = kcred->cr_uid;
		credp->cr_ruid = kcred->cr_ruid;
		credp->cr_suid = kcred->cr_suid;
		ocredp = set_cred(p, credp);
	}

	/*
	 * First remove any existing core file, then
	 * open the new core file with (O_EXCL|O_CREAT).
	 *
	 * The reasons for doing this are manifold:
	 *
	 * For security reasons, we don't want root processes
	 * to dump core through a symlink because that would
	 * allow a malicious user to clobber any file on
	 * the system if s/he could convince a root process,
	 * perhaps a set-uid root process that s/he started,
	 * to dump core in a directory writable by that user.
	 * Similar security reasons apply to hard links.
	 * For symmetry we do this unconditionally, not
	 * just for root processes.
	 *
	 * If the process has the core file mmap()d into the
	 * address space, we would be modifying the address
	 * space that we are trying to dump if we did not first
	 * remove the core file.  (The command "file core"
	 * is the canonical example of this possibility.)
	 *
	 * Opening the core file with O_EXCL|O_CREAT ensures than
	 * two concurrent core dumps don't clobber each other.
	 * One is bound to lose; we don't want to make both lose.
	 */
	if ((error = remove_core_file(fp)) == 0)
		error = vn_open(fp, UIO_SYSSPACE, FWRITE|FTRUNC|FEXCL|FCREAT,
		    perms, &vp, CRCREAT, u.u_cmask);

	/*
	 * Don't dump a core file owned by "nobody".
	 */
	vattr.va_mask = AT_UID;
	if (error == 0 &&
	    (error = VOP_GETATTR(vp, &vattr, 0, credp)) == 0 &&
	    vattr.va_uid != credp->cr_uid) {
		(void) remove_core_file(fp);
		error = EACCES;
	}

	/*
	 * Now that vn_open is complete, reset the process's credentials if
	 * we changed them, and make 'credp' point to the root cred created
	 * above.  We use 'credp' to do i/o on the core file below, but leave
	 * p->p_cred set to the original credential to allow the core file
	 * to record this information.
	 */
	if (ocredp != NULL)
		credp = set_cred(p, ocredp);

	if (error == 0) {
		int closerr;
#ifdef sparc
		(void) flush_user_windows_to_stack(NULL);
#endif /* sparc */
#ifdef SUN_SRC_COMPAT
		u.u_acflag |= ACORE;
#endif
		if ((eswp = u.u_execsw) == NULL ||
		    (eswp = findexec_by_magic(eswp->exec_magic)) == NULL) {
			error = ENOSYS;
		} else {
			error = (eswp->exec_core)(vp, p, credp, rlimit, sig);
			rw_exit(eswp->exec_lock);
		}

		closerr = VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, credp);
		if (error == 0)
			error = closerr;
		VN_RELE(vp);
	}

	/*
	 * If we had to change credentials above, free the crdup'd cred.
	 */
	if (ocredp != NULL)
		crfree(credp);

	return (error);
}

/*
 * Convert a core name pattern to a pathname.
 */
static int
expand_string(const char *pat, char *fp, int size)
{
	proc_t *p = curproc;
	char buf[24];
	int len;
	char *s;
	char c;

	while ((c = *pat++) != '\0') {
		if (size < 2)
			return (ENAMETOOLONG);
		if (c != '%') {
			size--;
			*fp++ = c;
			continue;
		}
		if ((c = *pat++) == '\0') {
			size--;
			*fp++ = '%';
			break;
		}
		switch (c) {
		case 'p':	/* pid */
			(void) sprintf((s = buf), "%d", p->p_pid);
			break;
		case 'u':	/* effective uid */
			(void) sprintf((s = buf), "%d", p->p_cred->cr_uid);
			break;
		case 'g':	/* effective gid */
			(void) sprintf((s = buf), "%d", p->p_cred->cr_gid);
			break;
		case 'f':	/* exec'd filename */
			s = PTOU(p)->u_comm;
			break;
		case 'n':	/* system nodename */
			s = utsname.nodename;
			break;
		case 'm':	/* machine (sun4u, etc) */
			s = utsname.machine;
			break;
		case 't':	/* decimal value of time(2) */
			(void) sprintf((s = buf), "%ld", hrestime.tv_sec);
			break;
		case '%':
			(void) strcpy((s = buf), "%");
			break;
		default:
			s = buf;
			buf[0] = '%';
			buf[1] = c;
			buf[2] = '\0';
			break;
		}
		len = (int)strlen(s);
		if ((size -= len) <= 0)
			return (ENAMETOOLONG);
		(void) strcpy(fp, s);
		fp += len;
	}

	*fp = '\0';
	return (0);
}

int
core(int sig)
{
	proc_t *p = curproc;
	klwp_t *lwp = ttolwp(curthread);
	refstr_t *rp;
	char *fp;
	int error1 = 1;
	int error2 = 1;
	k_sigset_t sigmask;
	k_sigset_t sighold;

	/* core files suppressed? */
	if (!(core_options & (CC_PROCESS_PATH|CC_GLOBAL_PATH)))
		return (1);

	/*
	 * Block all signals except SIGHUP, SIGINT, SIGKILL, and SIGTERM.
	 * These signals are allowed to interrupt the core dump.
	 * SIGQUIT is not allowed because it is supposed to make a core.
	 */
	mutex_enter(&p->p_lock);
	sigmask = curthread->t_hold;	/* remember for later */
	sigfillset(&sighold);
	if (!sigismember(&sigmask, SIGHUP))
		sigdelset(&sighold, SIGHUP);
	if (!sigismember(&sigmask, SIGINT))
		sigdelset(&sighold, SIGINT);
	if (!sigismember(&sigmask, SIGKILL))
		sigdelset(&sighold, SIGKILL);
	if (!sigismember(&sigmask, SIGTERM))
		sigdelset(&sighold, SIGTERM);
	curthread->t_hold = sighold;
	mutex_exit(&p->p_lock);

	/*
	 * Undo any watchpoints.
	 */
	if (p->p_as && p->p_as->a_wpage)
		pr_free_my_pagelist();

	/*
	 * The presence of a current signal prevents file i/o
	 * from succeeding over a network.  We copy the current
	 * signal information to the side and cancel the current
	 * signal so that the core dump will succeed.
	 */
	ASSERT(lwp->lwp_cursig == sig);
	lwp->lwp_cursig = 0;
	if (lwp->lwp_curinfo == NULL)
		bzero(&lwp->lwp_siginfo, sizeof (k_siginfo_t));
	else {
		bcopy(&lwp->lwp_curinfo->sq_info,
		    &lwp->lwp_siginfo, sizeof (k_siginfo_t));
		siginfofree(lwp->lwp_curinfo);
		lwp->lwp_curinfo = NULL;
	}

	/*
	 * Convert the core file name patterns into path names
	 * and call do_core() to write the core files.
	 */
	fp = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	if (core_options & CC_PROCESS_PATH) {
		mutex_enter(&p->p_lock);
		if ((rp = p->p_corefile) != NULL)
			refstr_hold(rp);
		mutex_exit(&p->p_lock);
		if (rp != NULL) {
			error1 = expand_string(refstr_value(rp),
				fp, MAXPATHLEN);
			if (error1 == 0)
				error1 = do_core(fp, sig, 0);
			refstr_rele(rp);
		}
	}
	if (core_options & CC_GLOBAL_PATH) {
		mutex_enter(&core_lock);
		if ((rp = core_file) != NULL)
			refstr_hold(rp);
		mutex_exit(&core_lock);
		if (rp == NULL)
			core_log(0, "no global core file pattern exists", NULL);
		else {
			error2 = expand_string(refstr_value(rp),
				fp, MAXPATHLEN);
			if (error2 != 0)
				core_log(0, "global core file pattern too long",
				    refstr_value(rp));
			else if ((error2 = do_core(fp, sig, 1)) == 0)
				core_log(0, "core dumped", fp);
			else if (error2 == ENOTSUP)
				core_log(0, "setid process, core not dumped",
				    fp);
			else
				core_log(error2, "core dump failed", fp);
			refstr_rele(rp);
		}
	}
	kmem_free(fp, MAXPATHLEN);

	/*
	 * Restore the signal hold mask.
	 */
	mutex_enter(&p->p_lock);
	curthread->t_hold = sigmask;
	mutex_exit(&p->p_lock);

	/*
	 * Return non-zero if no core file was created.
	 */
	return (error1 != 0 && error2 != 0);
}

/*
 * Maximum chunk size for dumping core files,
 * size in pages, patchable in /etc/system
 */
uint_t	core_chunk = 32;

/*
 * Common code to core dump process memory.
 */
int
core_seg(proc_t *p, vnode_t *vp, off_t offset, caddr_t addr, size_t size,
    rlim64_t rlimit, cred_t *credp)
{
	caddr_t eaddr;
	caddr_t base;
	size_t len;
	int err = 0;

	eaddr = addr + size;
	for (base = addr; base < eaddr; base += len) {
		len = eaddr - base;
		if (as_memory(p->p_as, &base, &len) != 0)
			return (0);
		/*
		 * Reduce len to a reasonable value so that we don't
		 * overwhelm the VM system with a monstrously large
		 * single write and cause pageout to stop running.
		 */
		if (len > (size_t)core_chunk * PAGESIZE)
			len = (size_t)core_chunk * PAGESIZE;
		err = vn_rdwr(UIO_WRITE, vp, base, len,
		    (offset_t)offset + (base - addr), UIO_USERSPACE, 0,
		    rlimit, credp, 0);
		if (err == 0) {
			/*
			 * Give pageout a chance to run.
			 * Also allow core dumping to be interruptible.
			 */
			err = delay_sig(1);
		}
		if (err)
			return (err);
	}
	return (0);
}
