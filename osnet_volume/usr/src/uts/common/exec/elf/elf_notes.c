/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf_notes.c	1.12	99/03/23 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/elf.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>
#include <sys/procfs.h>
#include <sys/regset.h>
#include <sys/auxv.h>
#include <sys/exec.h>
#include <sys/prsystm.h>
#include <sys/utsname.h>
#include <vm/as.h>
#include <vm/rm.h>
#include <sys/modctl.h>
#include <sys/systeminfo.h>
#include <sys/machelf.h>
#include "elf_impl.h"

extern	int	elfnote(vnode_t *, off_t *, int, int, caddr_t,
			rlim64_t, struct cred *);

void
setup_note_header(Phdr *v, proc_t *p)
{
	int nlwp = p->p_lwpcnt;
	size_t size;
	prcred_t *pcrp;

	v[0].p_type = PT_NOTE;
	v[0].p_flags = PF_R;
	v[0].p_filesz = (sizeof (Note) * (5 + 2 * nlwp))
	    + roundup(sizeof (psinfo_t), sizeof (Word))
	    + roundup(sizeof (pstatus_t), sizeof (Word))
	    + roundup(strlen(platform) + 1, sizeof (Word))
	    + roundup(__KERN_NAUXV_IMPL * sizeof (aux_entry_t), sizeof (Word))
	    + roundup(sizeof (utsname), sizeof (Word))
	    + nlwp * roundup(sizeof (lwpsinfo_t), sizeof (Word))
	    + nlwp * roundup(sizeof (lwpstatus_t), sizeof (Word));

	size = sizeof (prcred_t) + sizeof (gid_t) * (ngroups_max - 1);
	pcrp = kmem_alloc(size, KM_SLEEP);
	prgetcred(p, pcrp);

	if (pcrp->pr_ngroups != 0) {
		v[0].p_filesz += sizeof (Note) + roundup(sizeof (prcred_t) +
		    sizeof (gid_t) * (pcrp->pr_ngroups - 1), sizeof (Word));
	} else {
		v[0].p_filesz += sizeof (Note) +
		    roundup(sizeof (prcred_t), sizeof (Word));
	}

	kmem_free(pcrp, size);

	if ((size = prhasx(p)? prgetprxregsize(p) : 0) != 0)
		v[0].p_filesz += nlwp * sizeof (Note)
		    + nlwp * roundup(size, sizeof (Word));

#if defined(sparc) || defined(__sparc)
	/*
	 * Figure out the number and sizes of register windows.
	 */
	{
		kthread_t *t = p->p_tlist;
		do {
			if ((size = prnwindows(ttolwp(t))) != 0) {
				size = sizeof (gwindows_t) -
				    (SPARC_MAXREGWINDOW - size) *
				    sizeof (struct rwindow);
				v[0].p_filesz += sizeof (Note) +
				    roundup(size, sizeof (Word));
			}
		} while ((t = t->t_forw) != p->p_tlist);
	}
#if defined(__sparcv9)
	/*
	 * Space for the Ancillary State Registers.
	 */
	if (p->p_model == DATAMODEL_LP64)
		v[0].p_filesz += nlwp * sizeof (Note)
		    + nlwp * roundup(sizeof (asrset_t), sizeof (Word));
#endif /* __sparcv9 */
#endif /* sparc */
}

int
write_elfnotes(proc_t *p, int sig,
	vnode_t *vp, off_t *offsetp, rlim64_t rlimit, cred_t *credp)
{
	union {
		psinfo_t	psinfo;
		pstatus_t	pstatus;
		lwpsinfo_t	lwpsinfo;
		lwpstatus_t	lwpstatus;
#if defined(sparc) || defined(__sparc)
		gwindows_t	gwindows;
#if defined(__sparcv9)
		asrset_t	asrset;
#endif /* __sparcv9 */
#endif /* sparc */
		char		xregs[1];
		aux_entry_t	auxv[__KERN_NAUXV_IMPL];
		prcred_t	pcred;
	} *bigwad;

	size_t xregsize = prhasx(p)? prgetprxregsize(p) : 0;
	size_t crsize = sizeof (prcred_t) + sizeof (gid_t) * (ngroups_max - 1);
	size_t bigsize = MAX(sizeof (*bigwad), MAX(xregsize, crsize));

	kthread_t *t;
	klwp_t *lwp;
	user_t *up;
	int i;
	int nlwp;
	int error;

	bigwad = kmem_alloc(bigsize, KM_SLEEP);

	/*
	 * The order of the elfnote entries should be same here
	 * and in the gcore(1) command.  Synchronization is
	 * needed between the kernel and gcore(1).
	 */

	/*
	 * Get the psinfo, and set the wait status to indicate that a core was
	 * dumped.  We have to forge this since p->p_wcode is not set yet.
	 */
	mutex_enter(&p->p_lock);
	prgetpsinfo(p, &bigwad->psinfo);
	mutex_exit(&p->p_lock);
	bigwad->psinfo.pr_wstat = wstat(CLD_DUMPED, sig);

	error = elfnote(vp, offsetp, NT_PSINFO, sizeof (bigwad->psinfo),
	    (caddr_t)&bigwad->psinfo, rlimit, credp);
	if (error)
		goto done;

	mutex_enter(&p->p_lock);
	/*
	 * Restore current signal information
	 * in order to get a correct pstatus.
	 */
	lwp = ttolwp(curthread);
	ASSERT(lwp->lwp_cursig == 0);
	lwp->lwp_cursig = (uchar_t)sig;
	curthread->t_whystop = PR_FAULTED;	/* filthy */
	prgetstatus(p, &bigwad->pstatus);
	bigwad->pstatus.pr_lwp.pr_why = 0;
	curthread->t_whystop = 0;
	lwp->lwp_cursig = 0;
	mutex_exit(&p->p_lock);
	error = elfnote(vp, offsetp, NT_PSTATUS, sizeof (bigwad->pstatus),
	    (caddr_t)&bigwad->pstatus, rlimit, credp);
	if (error)
		goto done;

	error = elfnote(vp, offsetp, NT_PLATFORM, strlen(platform) + 1,
	    platform, rlimit, credp);
	if (error)
		goto done;

	up = PTOU(p);
	for (i = 0; i < __KERN_NAUXV_IMPL; i++) {
		bigwad->auxv[i].a_type = up->u_auxv[i].a_type;
		bigwad->auxv[i].a_un.a_val = up->u_auxv[i].a_un.a_val;
	}
	error = elfnote(vp, offsetp, NT_AUXV, sizeof (bigwad->auxv),
	    (caddr_t)bigwad->auxv, rlimit, credp);
	if (error)
		goto done;

	error = elfnote(vp, offsetp, NT_UTSNAME, sizeof (utsname),
	    (caddr_t)&utsname, rlimit, credp);
	if (error)
		goto done;

	prgetcred(p, &bigwad->pcred);

	if (bigwad->pcred.pr_ngroups != 0) {
		crsize = sizeof (prcred_t) +
		    sizeof (gid_t) * (bigwad->pcred.pr_ngroups - 1);
	} else
		crsize = sizeof (prcred_t);

	error = elfnote(vp, offsetp, NT_PRCRED, crsize,
	    (caddr_t)&bigwad->pcred, rlimit, credp);
	if (error)
		goto done;

	t = curthread;
	nlwp = p->p_lwpcnt;
	do {
		ASSERT(nlwp != 0);
		nlwp--;
		lwp = ttolwp(t);

		mutex_enter(&p->p_lock);
		prgetlwpsinfo(t, &bigwad->lwpsinfo);
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_LWPSINFO,
		    sizeof (bigwad->lwpsinfo), (caddr_t)&bigwad->lwpsinfo,
		    rlimit, credp);
		if (error)
			goto done;

		mutex_enter(&p->p_lock);
		if (t == curthread) {
			/*
			 * Restore current signal information
			 * in order to get a correct lwpstatus.
			 */
			lwp->lwp_cursig = (uchar_t)sig;
			t->t_whystop = PR_FAULTED;	/* filthy */
			prgetlwpstatus(t, &bigwad->lwpstatus);
			bigwad->lwpstatus.pr_why = 0;
			t->t_whystop = 0;
			lwp->lwp_cursig = 0;
		} else {
			prgetlwpstatus(t, &bigwad->lwpstatus);
		}
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_LWPSTATUS,
		    sizeof (bigwad->lwpstatus), (caddr_t)&bigwad->lwpstatus,
		    rlimit, credp);
		if (error)
			goto done;

#if defined(sparc) || defined(__sparc)
		/*
		 * Unspilled SPARC register windows.
		 */
		{
			size_t size = prnwindows(lwp);

			if (size != 0) {
				size = sizeof (gwindows_t) -
				    (SPARC_MAXREGWINDOW - size) *
				    sizeof (struct rwindow);
				prgetwindows(lwp, &bigwad->gwindows);
				error = elfnote(vp, offsetp, NT_GWINDOWS,
				    size, (caddr_t)&bigwad->gwindows,
				    rlimit, credp);
				if (error)
					goto done;
			}
		}
#if defined(__sparcv9)
		/*
		 * Ancillary State Registers.
		 */
		if (p->p_model == DATAMODEL_LP64) {
			prgetasregs(lwp, bigwad->asrset);
			error = elfnote(vp, offsetp, NT_ASRS,
			    sizeof (asrset_t), (caddr_t)bigwad->asrset,
			    rlimit, credp);
			if (error)
				goto done;
		}
#endif /* __sparcv9 */
#endif /* sparc */

		if (xregsize) {
			prgetprxregs(lwp, bigwad->xregs);
			error = elfnote(vp, offsetp, NT_PRXREG,
			    xregsize, bigwad->xregs, rlimit, credp);
			if (error)
				goto done;
		}
	} while ((t = t->t_forw) != curthread);
	ASSERT(nlwp == 0);

done:
	kmem_free(bigwad, bigsize);
	return (error);
}
