/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)aout.c	1.59	98/07/21 SMI"

/*
 * a.out exec module.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fpu/fpusystm.h>
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
#include <sys/debug.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/auxv.h>
#include <sys/core.h>
#include <sys/vmparam.h>
#include <sys/archsystm.h>
#include <sys/fs/swapnode.h>

#include <vm/anon.h>
#include <vm/as.h>
#include <vm/seg.h>

/*
 * Size of u-area.
 */
#define	USIZE	4*4096

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static int aoutexec(struct vnode *vp, struct execa *uap, struct uarg *args,
    struct intpdata *idatap, int level, long *execsz, int setid,
    caddr_t exec_file, struct cred *cred);
static int get_aout_head(struct vnode **vpp, struct exdata *edp, long *execsz,
    int *isdyn);
static int aoutcore(vnode_t *vp, proc_t *pp, struct cred *credp,
    rlim64_t rlimit, int sig);
#ifdef	_LP64
extern int elf32exec();
#else	/* _LP64 */
extern int elfexec();
#endif	/* _LP64 */
extern int at_flags;

char _depends_on[] = "exec/elfexec";

static struct execsw nesw = {
	aout_nmagicstr,
	2,
	2,
	aoutexec,
	aoutcore
};

static struct execsw zesw = {
	aout_zmagicstr,
	2,
	2,
	aoutexec,
	aoutcore
};

static struct execsw oesw = {
	aout_omagicstr,
	2,
	2,
	aoutexec,
	aoutcore
};

/*
 * Module linkage information for the kernel.
 */
static struct modlexec nexec = {
	&mod_execops, "exec for NMAGIC", &nesw
};

static struct modlexec zexec = {
	&mod_execops, "exec for ZMAGIC", &zesw
};

static struct modlexec oexec = {
	&mod_execops, "exec for OMAGIC", &oesw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&nexec, (void *)&zexec, (void *)&oexec, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*ARGSUSED*/
static int
aoutexec(
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
	register int error;
	struct exdata edp, edpout;
	struct execenv exenv;
	register proc_t *pp = ttoproc(curthread);
	struct vnode *nvp;
	int pagetext, pagedata;
	int dataprot = PROT_ALL;
	int textprot = PROT_ALL & ~PROT_WRITE;
	int isdyn;
	auxv32_t auxv[__KERN_NAUXV_IMPL];
	int num_auxv;
	int i;
	struct user *up = PTOU(pp);

	args->to_model = DATAMODEL_ILP32;
	*execsz = btopr(SINCR) + btopr(SSIZE) + btopr(NCARGS32-1);
	/*
	 * Read in and validate the file header.
	 */
	if (error = get_aout_head(&vp, &edp, execsz, &isdyn)) {
		return (error);
	}

	if (error = chkaout(&edp)) {
		return (error);
	}

	/*
	 * Take a quick look to see if it looks like we will have
	 * enough swap space for the program to get started.  This
	 * is not a guarantee that we will succeed, but it is definitely
	 * better than finding this out after we are committed to the
	 * new memory image.  Maybe what is needed is a way to "prereserve"
	 * swap space for some segment mappings here.
	 *
	 * But with shared libraries the process can make it through
	 * the exec only to have ld.so fail to get the program going
	 * because its mmap's will not be able to succeed if the system
	 * is running low on swap space.  In fact this is a far more
	 * common failure mode, but we cannot do much about this here
	 * other than add some slop to our anonymous memory resources
	 * requirements estimate based on some guess since we cannot know
	 * what else the program will really need to get to a useful state.
	 *
	 * XXX - The stack size (clrnd(SSIZE + btopr(nargc))) should also
	 * be used when checking for swap space.  This requires some work
	 * since nargc is actually determined in exec_args() which is done
	 * after this check and hence we punt for now.
	 *
	 * nargc = SA(nc + (na + 4) * NBPW) + sizeof (struct rwindow);
	 */
	if (CURRENT_TOTAL_AVAILABLE_SWAP < btopr(edp.ux_dsize) + btopr(SSIZE)) {
		return (ENOMEM);
	}

	if (enable_mixed_bcp)
		isdyn = 0;
	if (isdyn) {
		/*
		 * Build a small aux vector
		 */
		auxv32_t *aux = auxv;
		ADDAUX(aux, AT_PAGESZ, PAGESIZE);
		ADDAUX(aux, AT_FLAGS, at_flags);
		/*
		 * Save uid, ruid, gid and rgid information
		 * for the linker.
		 */
		ADDAUX(aux, AT_SUN_UID, cred->cr_uid);
		ADDAUX(aux, AT_SUN_RUID, cred->cr_ruid);
		ADDAUX(aux, AT_SUN_GID, cred->cr_gid);
		ADDAUX(aux, AT_SUN_RGID, cred->cr_rgid);
		/*
		 * Hardware capability flag word (performance hints)
		 */
		ADDAUX(aux, AT_SUN_HWCAP, auxv_hwcap);
		ADDAUX(aux, AT_NULL, 0);
		args->auxsize = 8 * sizeof (auxv32_t);

		/*
		 * Move args to user's stack and destroy the old
		 * user address space.
		 */
		if (error = exec_args(uap, args, idatap, NULL)) {
			if (error == -1) {
				error = ENOEXEC;
				goto done;
			}
			return (error);
		}

		/*
		 * Wedge the aux vector on the end of the environment
		 */
		up->u_auxvp = (uintptr_t)stackaddress(args, args->auxsize);
		error = execpoststack(args, auxv, args->auxsize);
		if (error != 0)
			goto done;

		/*
		 * Copy it to the process's user structure for use by /proc.
		 */
		num_auxv = args->auxsize / (sizeof (auxv32_t));
		ASSERT(num_auxv <= sizeof (up->u_auxv) / sizeof (auxv_t));
		bzero(up->u_auxv, sizeof (up->u_auxv));
		for (i = 0; i < num_auxv; i++) {
			up->u_auxv[i].a_type = auxv[i].a_type;
			up->u_auxv[i].a_un.a_val = (u_int)auxv[i].a_un.a_val;
		}
	} else {
		/*
		 * Load the trap 0 interpreter.
		 */
		if (error = lookupname("/usr/4lib/sbcp", UIO_SYSSPACE, FOLLOW,
		    NULLVPP, &nvp)) {
			goto done;
		}
#ifdef	_LP64
		if (error = elf32exec(nvp, uap, args, idatap, level, execsz,
		    setid, exec_file, cred))
#else	/* _LP64 */
		if (error = elfexec(nvp, uap, args, idatap, level, execsz,
		    setid, exec_file, cred))
#endif	/* _LP64 */
		{
			VN_RELE(nvp);
			return (error);
		}
		VN_RELE(nvp);
	}

	/*
	 * Determine the a.out's characteristics.
	 */
	getexinfo(&edp, &edpout, &pagetext, &pagedata);

	/*
	 * Load the a.out's text and data.
	 */
	if (error = execmap(edp.vp, edp.ux_txtorg, edp.ux_tsize,
	    (size_t)0, edp.ux_toffset, textprot, pagetext))
		goto done;
	if (error = execmap(edp.vp, edp.ux_datorg, edp.ux_dsize,
	    edp.ux_bsize, edp.ux_doffset, dataprot, pagedata))
		goto done;

	exenv.ex_brkbase = (caddr_t)edp.ux_datorg;
	exenv.ex_brksize = edp.ux_dsize + edp.ux_bsize;
	exenv.ex_magic = edp.ux_mag;
	exenv.ex_vp = edp.vp;
	setexecenv(&exenv);
	if (isdyn)
		u.u_exdata = edp;

done:
	if (error != 0)
		psignal(pp, SIGKILL);
	else {
		/*
		 * Ensure that the max fds do not exceed 256 (this is
		 * applicable to 4.x binaries, which is why we only
		 * do it on a.out files).
		 */
		if (u.u_rlimit[RLIMIT_NOFILE].rlim_cur > 256) {
			(void) rlimit(RLIMIT_NOFILE, 256, 256);
		} else if (u.u_rlimit[RLIMIT_NOFILE].rlim_max > 256) {
			(void) rlimit(RLIMIT_NOFILE,
				u.u_rlimit[RLIMIT_NOFILE].rlim_cur, 256);
		}
	}

	return (error);
}

/*
 * Read in and validate the file header.
 */
static int
get_aout_head(
	struct vnode **vpp,
	struct exdata *edp,
	long *execsz,
	int *isdyn)
{
	struct vnode *vp = *vpp;
	struct exec filhdr;
	int error;
	ssize_t resid;
	rlim64_t limit;
	rlim64_t roundlimit;

	if (error = vn_rdwr(UIO_READ, vp, (caddr_t)&filhdr,
	    (ssize_t)sizeof (filhdr), (offset_t)0, UIO_SYSSPACE, 0,
	    (rlim64_t)0, CRED(), &resid))
		return (error);

	if (resid != 0)
		return (ENOEXEC);

	switch (filhdr.a_magic) {
	case OMAGIC:
		filhdr.a_data += filhdr.a_text;
		filhdr.a_text = 0;
		break;
	case ZMAGIC:
	case NMAGIC:
		break;
	default:
		return (ENOEXEC);
	}

	/*
	 * Check total memory requirements (in pages) for a new process
	 * against the available memory or upper limit of memory allowed.
	 *
	 * For the 64-bit kernel, the limit can be set large enough so that
	 * rounding it up to a page can overflow, so we check for btopr()
	 * overflowing here by comparing it with the unrounded limit in pages.
	 */
	*execsz += btopr(filhdr.a_text + filhdr.a_data);
	limit = btop(P_CURLIMIT32(curproc, RLIMIT_VMEM));
	roundlimit = btopr(P_CURLIMIT32(curproc, RLIMIT_VMEM));
	if ((roundlimit > limit && *execsz > roundlimit) ||
	    (roundlimit < limit && *execsz > limit))
		return (ENOMEM);

	edp->ux_mach = filhdr.a_machtype;
	edp->ux_tsize = filhdr.a_text;
	edp->ux_dsize = filhdr.a_data;
	edp->ux_bsize = filhdr.a_bss;
	edp->ux_mag = filhdr.a_magic;
	edp->ux_toffset = gettfile(&filhdr);
	edp->ux_doffset = getdfile(&filhdr);
	edp->ux_txtorg = gettmem(&filhdr);
	edp->ux_datorg = getdmem(&filhdr);
	edp->ux_entloc = (caddr_t)filhdr.a_entry;
	edp->vp = vp;
	*isdyn = filhdr.a_dynamic;

	return (0);
}

/*
 * Create a core image on the file "core".  Writes a struct core
 * followed by the entire data+stack segments and user area.
 */
static int
aoutcore(
	vnode_t *vp,
	proc_t *pp,
	struct cred *credp,
	rlim64_t rlimit,
	int sig)
{
	struct core *corep;
	struct user *up = PTOU(pp);
	off_t offset = 0;
	caddr_t base;
	int error;
	size_t count, len;
	klwp_id_t lwp = ttolwp(curthread);

	ASSERT(pp == curproc);

	up->u_tsize = btopr(up->u_exdata.ux_tsize);
	up->u_dsize = btopr(up->u_exdata.ux_dsize +
	    up->u_exdata.ux_bsize + pp->p_brksize);

	/*
	 * Dump the specific areas of the u area into the new
	 * core structure for examination by debuggers.  The
	 * new format is now independent of the user structure and
	 * only the information needed by the debuggers is included.
	 */
	corep = kmem_zalloc(sizeof (struct core), KM_SLEEP);
	corep->c_magic = CORE_MAGIC;
	corep->c_len = sizeof (struct core);
	getgregs(lwp, &corep->c_regs[0]);
	fp_core(corep);
	corep->c_ucode = lwp->lwp_siginfo.si_code;
	corep->c_exdata = up->u_exdata;
	corep->c_signo = sig;
	corep->c_tsize = ptob(up->u_tsize);
	corep->c_dsize = ptob(up->u_dsize);
	corep->c_ssize = pp->p_stksize;
	len = MIN(MAXCOMLEN, CORE_NAMELEN);
	(void) strncpy(corep->c_cmdname, up->u_comm, len);
	corep->c_cmdname[len] = '\0';

	/*
	 * Write out core file header.
	 */
	if (error = vn_rdwr(UIO_WRITE, vp, (caddr_t)corep,
	    sizeof (struct core), (offset_t)offset, UIO_SYSSPACE,
	    0, rlimit, credp, (ssize_t *)NULL)) {
		kmem_free(corep, sizeof (struct core));
		return (error);
	}
	offset += sizeof (struct core);
	kmem_free(corep, sizeof (struct core));

	/*
	 * Check the sizes against the current ulimit and
	 * don't write a file bigger than ulimit.  If we
	 * can't write everything, we would prefer to
	 * write the stack and not the data rather than
	 * the other way around.
	 */
	if ((rlim64_t)ptob(sizeof (struct user) + up->u_dsize +
		pp->p_stksize) > rlimit) {
		up->u_dsize = 0;
		if ((rlim64_t)ptob(sizeof (struct user) + pp->p_stksize) >
			rlimit)
			pp->p_stksize = 0;
	}

	/*
	 * Write the data and stack to the dump file.
	 */
	if (up->u_dsize) {
		base = (caddr_t)up->u_exdata.ux_datorg;
		count = ptob(up->u_dsize) - ((uintptr_t)base & PAGEOFFSET);
		if (error = core_seg(pp, vp, offset, base, count,
		    rlimit, credp))
			return (error);
		offset += ptob(btopr(count));
	}

	if (pp->p_stksize) {
		if (error = core_seg(pp, vp, offset,
		    pp->p_usrstack - pp->p_stksize,
		    pp->p_stksize, rlimit, credp))
			return (error);
		offset += pp->p_stksize;
	}

	/*
	 * Write the u-area (for those who care).
	 */
	if (error = vn_rdwr(UIO_WRITE, vp, (caddr_t)up, USIZE,
	    (offset_t)offset, UIO_SYSSPACE, 0, rlimit, credp,
	    (ssize_t *)NULL))
		return (error);
	return (error);
}
