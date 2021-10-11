/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)coff.c	1.31	99/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/tss.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/fstyp.h>
#include <sys/acct.h>
#include <sys/sysinfo.h>
#include <sys/reg.h>
#include <sys/var.h>
#include <sys/immu.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/exec.h>
#include <sys/inline.h>
#include <sys/vmparam.h>
#include <sys/elf.h>
#include <sys/vmsystm.h>
#include <sys/auxv.h>
#include <sys/archsystm.h>

#define	IAPX386MAGIC	0514		/* magic number for COFF executables */

static void coffexec_err(struct exdata *, long);
static int getcoffhead(struct vnode *, struct exdata *, long *);
static int getcoffshlibs(struct vnode *, struct exdata *, struct exdata *,
	long *, struct cred *, caddr_t);
static int coffaddaux(struct uarg *, caddr_t, int);

#define	pageable(off, addr, vp) \
	((((long)(off) & PAGEOFFSET) == ((long)(addr) & PAGEOFFSET)) && \
	(!((vp)->v_flag & VNOMAP)))

#include <sys/modctl.h>

static int coffexec(struct vnode *, struct execa *, struct uarg *,
    struct intpdata *, int, long *, int, caddr_t, struct cred *);
static int coffcore(vnode_t *, proc_t *, struct cred *, rlim64_t, int);

extern int elfexec(struct vnode *, struct execa *, struct uarg *,
    struct intpdata *, int, long *, int, caddr_t, struct cred *);
extern int elfcore(vnode_t *, proc_t *, struct cred *, rlim64_t, int);

char _depends_on[] = "exec/elfexec";

static char cbcpname[] = "/usr/lib/cbcp";

static struct execsw esw = {
	coffmagicstr,
	0,
	2,
	coffexec,
	coffcore
};

static struct modlexec modlexec = {
	&mod_execops, "exec module for coff", &esw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlexec, NULL
};

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
coffexec(
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
	struct execenv exenv;
	struct exdata edp;
	int error = 0;
	int i;
	char ldatcache[2 * sizeof (struct exdata)];
	struct exdata *shlb_dat = (struct exdata *)ldatcache, *datp;
	uint_t shlb_datsz;
	int dataprot = PROT_ALL;
	int textprot = PROT_ALL & ~PROT_WRITE;
	struct proc *pp = ttoproc(curthread);
	struct user *up = PTOU(pp);
	struct vnode *nvp = 0;
	int fd;
	int magic = coffmagic;

	*execsz = btopr(SINCR) + btopr(SSIZE) + btopr(NCARGS-1);

	if ((error = getcoffhead(vp, &edp, execsz)) != 0)
		return (error);

	args->to_model = DATAMODEL_NATIVE;

	/*
	 * Check total memory requirements (in pages) for a new process
	 * against the available memory or upper limit of memory allowed.
	 */
	if (*execsz > btopr(P_CURLIMIT32(pp, RLIMIT_VMEM)))
		return (ENOMEM);

	/*
	 * Look at what we got, edp.ux_mag = 410/411/413.
	 *
	 * 410 is RO text.
	 * 411 is separated ID (treated like a 0410).
	 * 413 is RO text in an "aligned" a.out file.
	 */
	switch (edp.ux_mag) {
	case 0410:
	case 0411:
	case 0413:
		break;

	case 0443:
		error = ELIBEXEC;
		break;
	default:
		error = ENOEXEC;
		break;
	}

	if (error)
		return (error);

	/*
	 * For _iBCS2 compatibility, bring in the elf file
	 * /usr/lib/cbcp as an execution "interpreter", to help
	 * process coff files.  Execution will begin in
	 * /usr/lib/cbcp if present, with two additional vectors
	 * AT_SUN_EMUL_ENTRY for the coff entry point and
	 * AT_SUN_EMUL_EXECFD for a file descriptor of the coff
	 * file.
	 */

	if (enable_cbcp) {
		error = lookupname(cbcpname, UIO_SYSSPACE, FOLLOW,
		    NULLVPP, &nvp);

		if (error == 0) {
			if ((error = execopen(&vp, &fd)) != 0) {
				VN_RELE(nvp);
				goto done;
			}
			/*
			 * elfexec will call exec_args, so we don't.
			 */
			error = elfexec(nvp, uap, args, idatap, level, execsz,
			    setid, exec_file, cred);
			VN_RELE(nvp);
			if (error != 0)
				goto done;
			/*
			 * The arguments have been set up, but we
			 * still need to add two additional auxv
			 * vectors.
			 */
			error = coffaddaux(args, edp.ux_entloc, fd);
			if (error != 0)
				goto done;

			edp.ux_entloc = up->u_exdata.ux_entloc;
			magic = elfmagic;

		} else if (error != ENOENT) {
			uprintf("%s: Cannot exec %s\n", exec_file, cbcpname);
			return (error);

		} else {
			/*
			 * didn't find /usr/lib/cbcp so no auxv vectors
			 */
			args->auxsize = 0;
			up->u_auxvp = NULL;
			/*
			 * Move args to the user's stack.
			 */
			if ((error = exec_args(uap, args, idatap,
			    (void **)NULL)) == -1) {
				error = ENOEXEC;
				goto done;
			}
		}

	} else {
		/*
		 * enable_cbcp is 0, so no /usr/lib/cbcp, so no auxv
		 * vectors.
		 */
		args->auxsize = 0;
		up->u_auxvp = NULL;
		/*
		 * Move args to the user's stack.
		 */
		if ((error = exec_args(uap, args, idatap,
		    (void **)NULL)) == -1) {
			error = ENOEXEC;
			goto done;
		}
	}

	if (edp.ux_nshlibs) {
		shlb_datsz = edp.ux_nshlibs * sizeof (struct exdata);

		if (shlb_datsz > sizeof (ldatcache))
			shlb_dat = kmem_alloc(shlb_datsz, KM_SLEEP);
		else
			shlb_dat = (struct exdata *)ldatcache;

		datp = shlb_dat;

		error = getcoffshlibs(vp, &edp, datp, execsz, cred, exec_file);
		if (error)
			goto done;
	}

	/*
	 * Load any shared libraries that are needed.
	 */
	if (edp.ux_nshlibs) {
		for (i = 0; i < edp.ux_nshlibs; i++, datp++) {
			if (error = execmap(datp->vp, datp->ux_txtorg,
			    datp->ux_tsize, (off_t)0, datp->ux_toffset,
			    textprot, pageable(datp->ux_txtorg,
			    datp->ux_toffset, datp->vp))) {
				coffexec_err(++datp, edp.ux_nshlibs - i - 1);
				goto done;
			}

			if (error = execmap(datp->vp, datp->ux_datorg,
			    datp->ux_dsize, (off_t)datp->ux_bsize,
			    datp->ux_doffset, dataprot,
			    pageable(datp->ux_datorg, datp->ux_doffset,
			    datp->vp))) {
				coffexec_err(++datp, edp.ux_nshlibs - i - 1);
				goto done;
			}
			VN_RELE(datp->vp);	/* done with this reference */
		}
	}

	/*
	 * Load the a.out's text and data.
	 */
	if (error = execmap(edp.vp, edp.ux_txtorg,
	    edp.ux_tsize, (off_t)0, edp.ux_toffset, textprot,
			pageable(edp.ux_txtorg, edp.ux_toffset, edp.vp))) {
		goto done;
	}

	if (error = execmap(edp.vp, edp.ux_datorg,
	    edp.ux_dsize, edp.ux_bsize, edp.ux_doffset, dataprot,
			pageable(edp.ux_datorg, edp.ux_doffset, edp.vp))) {
		goto done;
	}

	exenv.ex_brkbase =
		(caddr_t)(edp.ux_datorg + edp.ux_dsize + edp.ux_bsize);
	exenv.ex_brksize = 0;
	exenv.ex_magic = magic;
	exenv.ex_vp = vp;
	setexecenv(&exenv);
done:
	if (error == 0)
		up->u_exdata = edp;	/* XXXX dependency on core file */

	if (edp.ux_nshlibs) {
		if (shlb_dat != (struct exdata *)ldatcache)
			kmem_free(shlb_dat, shlb_datsz);
	}
	if (error)
		psignal(pp, SIGKILL);
	return (error);
}


/*
 * Read the a.out headers.  There must be at least three sections,
 * and they must be .text, .data and .bss (although not necessarily
 * in that order).
 *
 * Possible magic numbers are 0410, 0411 (treated as 0410), and
 * 0413.  If there is no optional UNIX header then magic number
 * 0410 is assumed.
 */

/*
 *   Common object file header.
 */

/*
 * f_magic (magic number)
 *
 *   NOTE:  For 3b-5, the old values of magic numbers
 *	    will be in the optional header in the
 *	    structure "aouthdr" (identical to old
 *	    unix aouthdr).
 */

/*
 * f_flags
 *
 *	F_EXEC		file is executable (i.e. no unresolved
 *			  externel references).
 *	F_AR16WR	this file created on AR16WR machine
 *			  (e.g. 11/70).
 *	F_AR32WR	this file created on AR32WR machine
 *			  (e.g. vax, 386).
 *	F_AR32W		this file created on AR32W machine
 *			  (e.g. 3B, maxi).
 */
#define	F_EXEC		0000002
#define	F_AR16WR	0000200
#define	F_AR32WR	0000400
#define	F_AR32W		0001000

struct filehdr {
	ushort_t f_magic;
	ushort_t f_nscns;	/* number of sections */
	long	f_timdat;	/* time & date stamp */
	long	f_symptr;	/* file pointer to symtab */
	long	f_nsyms;	/* number of symtab entries */
	ushort_t f_opthdr;	/* sizeof(optional hdr) */
	ushort_t f_flags;
};

/*
 *  Common object file section header.
 */

/*
 *  s_name
 */
#define	_TEXT	".text"
#define	_DATA	".data"
#define	_BSS	".bss"
#define	_LIB	".lib"

/*
 * s_flags
 */
#define	STYP_TEXT	0x0020	/* section contains text only */
#define	STYP_DATA	0x0040	/* section contains data only */
#define	STYP_BSS	0x0080	/* section contains bss only  */
#define	STYP_LIB	0x0800	/* section contains lib only  */

struct scnhdr {
	char	s_name[8];	/* section name */
	long	s_paddr;	/* physical address */
	long	s_vaddr;	/* virtual address */
	long	s_size;		/* section size */
	long	s_scnptr;	/* file ptr to raw	*/
				/* data for section	*/
	long	s_relptr;	/* file ptr to relocation */
	long	s_lnnoptr;	/* file ptr to line numbers */
	ushort_t s_nreloc;	/* number of relocation	*/
				/* entries		*/
	ushort_t s_nlnno;	/* number of line	*/
				/* number entries	*/
	long	s_flags;	/* flags */
};

/*
 * Common object file optional unix header.
 */

struct aouthdr {
	short	o_magic;	/* magic number */
	short	o_stamp;	/* stamp */
	long	o_tsize;	/* text size */
	long	o_dsize;	/* data size */
	long	o_bsize;	/* bss size */
	long	o_entloc;	/* entry location */
	long	o_tstart;
	long	o_dstart;
};

/*
 * Get the file header, handling '#!' indirection if required.
 */
static int
getcoffhead(struct vnode *vp, struct exdata *edp, long *execsz)
{
	struct filehdr filhdr;
	struct aouthdr aouthdr;
	struct scnhdr  *scnhdrp, *allocp;
	int    opt_hdr = 0;
	int    scns    = 0;
	off_t	offset;
	int error;
	int nscns;
	ssize_t ssz;
	ssize_t resid;

	if (error = vn_rdwr(UIO_READ, vp, (caddr_t)&filhdr,
	    (ssize_t)sizeof (filhdr), (offset_t)0, UIO_SYSSPACE, 0,
	    (rlim64_t)0, CRED(), &resid))
		return (error);

	if (filhdr.f_magic != IAPX386MAGIC || !(filhdr.f_flags & F_EXEC))
		return (ENOEXEC);

	/*
	 * get what is needed from filhdr now, since it is
	 * not kosher to modify the file page and little is needed.
	 */
	nscns = filhdr.f_nscns;
	offset = sizeof (filhdr) + filhdr.f_opthdr;

	/*
	 * Next, read the optional unix header if present; if not,
	 * then we will assume the file is a 410.
	 */
	if (filhdr.f_opthdr >= sizeof (aouthdr)) {
		if ((error = vn_rdwr(UIO_READ, vp, (caddr_t)&aouthdr,
		    (ssize_t)sizeof (aouthdr), (offset_t)sizeof (filhdr),
		    UIO_SYSSPACE, 0, (rlim64_t)0, CRED(), &resid)) != 0)
			return (error);

		opt_hdr = 1;
		edp->ux_mag = aouthdr.o_magic;
		edp->ux_entloc = (caddr_t)aouthdr.o_entloc;
	}

	/*
	 * Next, read the section headers.  There had better be at
	 * least three: .text, .data and .bss.  The shared library
	 * section is optional; initialize the number needed to 0.
	 */

	edp->ux_nshlibs = 0;

	ssz = nscns * sizeof (*scnhdrp);
	allocp = scnhdrp = kmem_alloc(ssz, KM_SLEEP);
	if (error = vn_rdwr(UIO_READ, vp, (caddr_t)scnhdrp, ssz,
		(offset_t)offset, UIO_SYSSPACE, 0, (rlim64_t)0,
		CRED(), &resid)) {
		kmem_free(allocp, ssz);
		return (error);
	}

	for (; nscns > 0; scnhdrp++, nscns--) {
		switch ((int)scnhdrp->s_flags) {
		case STYP_TEXT:
			scns |= STYP_TEXT;

			if (! opt_hdr) {
				edp->ux_mag = 0410;
				edp->ux_entloc = (caddr_t)scnhdrp->s_vaddr;
			}

			edp->ux_txtorg = (caddr_t)scnhdrp->s_vaddr;
			edp->ux_toffset = scnhdrp->s_scnptr;
			*execsz += btopr(edp->ux_tsize = scnhdrp->s_size);
			break;

		case STYP_DATA:
			scns |= STYP_DATA;
			edp->ux_datorg = (caddr_t)scnhdrp->s_vaddr;
			edp->ux_doffset = scnhdrp->s_scnptr;
			*execsz += btopr(edp->ux_dsize = scnhdrp->s_size);
			break;

		case STYP_BSS:
			scns |= STYP_BSS;
			*execsz += btopr(edp->ux_bsize = scnhdrp->s_size);
			break;

		case STYP_LIB:
			edp->ux_nshlibs = scnhdrp->s_paddr;
			edp->ux_lsize = scnhdrp->s_size;
			edp->ux_loffset = scnhdrp->s_scnptr;
			break;
		}
	}

	/* Make sure we've seen a text, data and bss section */
	if (scns != (STYP_TEXT|STYP_DATA|STYP_BSS)) {
		kmem_free(allocp, ssz);
		return (ENOEXEC);
	}

	edp->vp = vp;
	kmem_free(allocp, ssz);
	return (0);
}

static int
getcoffshlibs(struct vnode *vp, struct exdata *edp, struct exdata *dat_start,
	long *execsz, struct cred *cred, caddr_t exec_file)
{
	struct vnode *nvp;
	unsigned int *bp, *fbp;
	struct	exdata *dat = dat_start;
	unsigned n = 0;
	unsigned *libend;
	char *shlibname;
	struct vattr vattr;
	int error;
	ssize_t resid;

	/* Allocate memory for reading in library section */
	fbp = bp = kmem_alloc(edp->ux_lsize, KM_SLEEP);

	if (error = vn_rdwr(UIO_READ, vp, (caddr_t)bp,
	    (ssize_t)edp->ux_lsize, (offset_t)edp->ux_loffset,
	    UIO_SYSSPACE, 0, (rlim64_t)0, CRED(), &resid)) {
		goto bad;
	}

	edp->ux_nshlibs = 0;	/* Elf may call this code */
	libend = bp + (edp->ux_lsize / sizeof (*bp));

	while (bp < libend) {
		/* Check the validity of the shared lib entry. */
		if (bp[0] * NBPW > edp->ux_lsize ||
		    bp[1] > edp->ux_lsize || bp[0] < 3) {
			error = ELIBSCN;
			goto bad;
		}

		/* Locate the shared lib and get its header info.  */

		shlibname = (caddr_t)(bp + bp[1]);
		bp += bp[0];

		if (lookupname(shlibname, UIO_SYSSPACE, FOLLOW, NULLVPP,
		    &nvp)) {
			uprintf("%s: cannot find library %s\n", exec_file,
								shlibname);
			error = ELIBACC;
			goto bad;
		}

		/*
		 * nvp has a VN_HOLD on it,
		 * so a VN_RELE must happen for all error cases.
		 */

		vattr.va_mask = AT_MODE;
		if (error = VOP_GETATTR(nvp, &vattr, ATTR_EXEC, cred)) {
			VN_RELE(nvp);
			goto bad;
		}

		if ((error = VOP_ACCESS(nvp, VEXEC, 0, cred)) != 0 ||
		    nvp->v_type != VREG ||
		    (vattr.va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0) {
			error = ELIBACC;
			VN_RELE(nvp);
			goto bad;
		}

		error = getcoffhead(nvp, dat, execsz);
		if (error) {
			if (error != ENOMEM)
				error = ELIBBAD;
			VN_RELE(nvp);
			goto bad;
		}

		if (dat->ux_mag != 0443) {
			error = ELIBBAD;
			VN_RELE(nvp);
			goto bad;
		}

		++dat;
		++edp->ux_nshlibs;
		++n;
	}

	kmem_free(fbp, edp->ux_lsize);

	return (0);

bad:
	kmem_free(fbp, edp->ux_lsize);
	coffexec_err(dat_start, (long)n);
	return (error);
}


static void
coffexec_err(struct exdata *shlb_data, long n)
{
	for (; n > 0; --n, ++shlb_data)
		VN_RELE(shlb_data->vp);
}

static int
coffcore(vnode_t *vp, proc_t *pp, struct cred *credp, rlim64_t rlimit, int sig)
{
	/* Just create a core dump just like if an ELF program died */
	return (elfcore(vp, pp, credp, rlimit, sig));
}

static int
coffaddaux(struct uarg *args, caddr_t entloc, int fd)
{
	proc_t *const pp = ttoproc(curthread);
	klwp_t *const lwp = ttolwp(curthread);
	struct user *const up = PTOU(pp);
	int const argsize = NBPW + args->na * NBPW + args->auxsize;
	caddr_t copyofargs;
	auxv_t *aux;
	int naux;
	int error;

	/*
	 * Rearrange the argc, argv and auxv vectors to make room
	 * for two new vectors AT_SUN_EMUL_ENTRY and
	 * AT_SUN_EMUL_EXECFD for _iBCS2 support of coff
	 * executables.
	 */

	copyofargs = kmem_alloc(argsize + 4 * NBPW, KM_SLEEP);
	error = copyin((caddr_t)(up->u_argv - NBPW), copyofargs, argsize);
	if (error)
		goto done;

	aux = (auxv_t *)((caddr_t)copyofargs + argsize);
	aux[0].a_type = AT_SUN_EMUL_ENTRY;
	aux[0].a_un.a_val = (int)entloc;
	aux[1].a_type = AT_SUN_EMUL_EXECFD;
	aux[1].a_un.a_val = fd;


	/*
	 * The aux vectors consist of one 32 bit word of type and
	 * one 32 word of value, hence we move them around 2 * NBPW
	 * which is 8 bytes at a time.
	 */

	error = copyout(copyofargs, (caddr_t)(up->u_argv - 5 * NBPW),
	    argsize + 4 * NBPW);
	if (error)
		goto done;

	/*
	 * Adjust the remembered aux vectors in the user struct.
	 */
	naux = args->auxsize / sizeof (auxv_t);
	up->u_auxv[naux-1].a_type = aux[0].a_type;
	up->u_auxv[naux-1].a_un.a_val = (uint_t)aux[0].a_un.a_val;
	up->u_auxv[naux].a_type = aux[1].a_type;
	up->u_auxv[naux].a_un.a_val = (uint_t)aux[1].a_un.a_val;
	up->u_auxv[naux+1].a_type = AT_NULL;
	up->u_auxv[naux+1].a_un.a_val = 0;

	args->auxsize += 4 * NBPW;
	args->stackend -= 4 * NBPW;
	up->u_argv -= 4 * NBPW;
	up->u_envp -= 4 * NBPW;
	up->u_auxvp -= 4 * NBPW;
	lwptoregs(lwp)->r_usp -= 4 * NBPW;

done:
	kmem_free(copyofargs, argsize + 4 * NBPW);
	return (error);
}
