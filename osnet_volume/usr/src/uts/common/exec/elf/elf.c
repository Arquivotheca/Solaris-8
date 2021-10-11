/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf.c	1.86	99/05/04 SMI"

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
#include <sys/auxv.h>
#include <sys/exec.h>
#include <sys/prsystm.h>
#include <vm/as.h>
#include <vm/rm.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <sys/modctl.h>
#include <sys/systeminfo.h>
#include <sys/vmparam.h>
#include <sys/machelf.h>
#include "elf_impl.h"

extern int at_flags;

static int getelfhead(struct vnode *, Ehdr *, caddr_t *, ssize_t *);
static size_t elfsize(Ehdr *, caddr_t);
static int mapelfexec(struct vnode *, Ehdr *, caddr_t,
	Phdr **, Phdr **, Phdr **,
	caddr_t *, intptr_t *, size_t, long *);

/*ARGSUSED*/
int
elfexec(
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
	caddr_t		phdrbase = NULL;
	caddr_t 	base = 0;
	ssize_t		dlnsize;
	aux_entry_t	*aux;
	int		error;
	ssize_t		resid;
	int		fd = -1;
	intptr_t	voffset;
	struct vnode	*nvp;
	Phdr	*dyphdr = NULL;
	Phdr	*stphdr = NULL;
	Phdr	*uphdr = NULL;
	Phdr	*junk = NULL;
	size_t		len;
	ssize_t		phdrsize;
	int		postfixsize = 0;
	int		i, hsize;
	Phdr	*phdrp;
	int		hasu = 0;
	int		hasdy = 0;
	struct proc *p = ttoproc(curthread);
	struct user *up = PTOU(p);
	struct bigwad {
		Ehdr	ehdr;
		aux_entry_t	elfargs[__KERN_NAUXV_IMPL];
		char		dl_name[MAXPATHLEN];
		struct vattr	vattr;
		struct execenv	exenv;
	} *bigwad;	/* kmem_alloc this behemoth so we don't blow stack */
	Ehdr	*ehdrp;
	char		*dlnp;
	rlim64_t	limit;
	rlim64_t	roundlimit;

	ASSERT(p->p_model == DATAMODEL_ILP32 || p->p_model == DATAMODEL_LP64);

	bigwad = kmem_alloc(sizeof (struct bigwad), KM_SLEEP);
	ehdrp = &bigwad->ehdr;
	dlnp = bigwad->dl_name;

	/*
	 * Obtain ELF and program header information.
	 */
	if ((error = getelfhead(vp, ehdrp, &phdrbase, &phdrsize)) != 0)
		goto out;

	/*
	 * Put data model that we're exec-ing to into the args passed to
	 * exec_args(), so it will know what it is copying to on new stack.
	 * Now that we know whether we are exec-ing a 32-bit or 64-bit
	 * executable, we can set execsz with the appropriate NCARGS.
	 */
#ifdef	_LP64
	if (ehdrp->e_ident[EI_CLASS] == ELFCLASS32) {
#if defined(__ia64)
		p->p_isa = (ehdrp->e_machine == EM_386) ? _ISA_IA32 : _ISA_IA64;
#endif
		args->to_model = DATAMODEL_ILP32;
		*execsz = btopr(SINCR) + btopr(SSIZE) + btopr(NCARGS32-1);
	} else {
#if defined(__ia64)
		p->p_isa = _ISA_IA64;
#endif
		args->to_model = DATAMODEL_LP64;
		*execsz = btopr(SINCR) + btopr(SSIZE) + btopr(NCARGS64-1);
	}
#else	/* _LP64 */
	args->to_model = DATAMODEL_ILP32;
	*execsz = btopr(SINCR) + btopr(SSIZE) + btopr(NCARGS-1);
#endif	/* _LP64 */
	/*
	 * Determine aux size now so that stack can be built
	 * in one shot (except actual copyout of aux image)
	 * and still have this code be machine independent.
	 */
	hsize = ehdrp->e_phentsize;
	phdrp = (Phdr *)phdrbase;
	for (i = ehdrp->e_phnum; i > 0; i--) {
		switch (phdrp->p_type) {
		case PT_INTERP:
			hasdy = 1;
			break;
		case PT_PHDR:
			hasu = 1;
		}
		phdrp = (Phdr *)((caddr_t)phdrp + hsize);
	}

	/*
	 * If dynamic, aux vector will have at least 11 entries for
	 * the 9 aux types inserted below and 2 for AT_SUN_PLATFORM
	 * and AT_SUN_EXECNAME, inserted by exec_args().
	 * In addition, there are either 4 or 1 entries depending
	 * on whether there is a program header or not.
	 * This may be increased by exec_args if there are ISA-specific
	 * types (included in __KERN_NAUXV_IMPL).
	 */
	if (hasdy)
		args->auxsize = (11 + (hasu ? 4 : 1)) * sizeof (aux_entry_t);
	else
		args->auxsize = 0;

	aux = bigwad->elfargs;
	/*
	 * Move args to the user's stack.
	 */
	if ((error = exec_args(uap, args, idatap, (void **)&aux)) != 0) {
		if (error == -1) {
			error = ENOEXEC;
			goto bad;
		}
		goto out;
	}

	/*
	 * If this is an ET_DYN executable (shared object),
	 * determine its memory size so that mapelfexec() can load it.
	 */
	if (ehdrp->e_type == ET_DYN)
		len = elfsize(ehdrp, phdrbase);
	else
		len = 0;

	if ((error = mapelfexec(vp, ehdrp, phdrbase, &uphdr, &dyphdr,
	    &stphdr, &base, &voffset, len, execsz)) != 0)
		goto bad;

	if (uphdr != NULL && dyphdr == NULL)
		goto bad;

	if (dyphdr != NULL) {
		size_t len;

		dlnsize = dyphdr->p_filesz;

		if (dlnsize > MAXPATHLEN || dlnsize <= 0)
			goto bad;

		/*
		 * Read in "interpreter" pathname.
		 */
		if ((error = vn_rdwr(UIO_READ, vp, dlnp, dyphdr->p_filesz,
		    (offset_t)dyphdr->p_offset, UIO_SYSSPACE, 0, (rlim64_t)0,
		    CRED(), &resid)) != 0) {
			uprintf("%s: Cannot obtain interpreter pathname\n",
			    exec_file);
			goto bad;
		}

		if (resid != 0 || dlnp[dlnsize - 1] != '\0')
			goto bad;

		if ((error = lookupname(dlnp, UIO_SYSSPACE, FOLLOW,
		    NULLVPP, &nvp)) != 0) {
			uprintf("%s: Cannot find %s\n", exec_file, dlnp);
			goto bad;
		}

		/*
		 * Setup the "aux" vector.
		 */
		if (uphdr) {
			if (ehdrp->e_type == ET_DYN)
				/* don't use the first page */
				bigwad->exenv.ex_brkbase = (caddr_t)PAGESIZE;
			else
				bigwad->exenv.ex_brkbase = base;
			bigwad->exenv.ex_brksize = 0;
			bigwad->exenv.ex_magic = elfmagic;
			bigwad->exenv.ex_vp = vp;
			setexecenv(&bigwad->exenv);

			ADDAUX(aux, AT_PHDR, uphdr->p_vaddr + voffset);
			ADDAUX(aux, AT_PHENT, ehdrp->e_phentsize);
			ADDAUX(aux, AT_PHNUM, ehdrp->e_phnum);
			ADDAUX(aux, AT_ENTRY, ehdrp->e_entry + voffset);
		} else {
			if ((error = execopen(&vp, &fd)) != 0) {
				VN_RELE(nvp);
				goto bad;
			}

			ADDAUX(aux, AT_EXECFD, fd);
		}

		if ((error = execpermissions(nvp, &bigwad->vattr, args)) != 0) {
			VN_RELE(nvp);
			uprintf("%s: Cannot execute %s\n", exec_file, dlnp);
			goto bad;
		}

		/*
		 * Now obtain the ELF header along with the entire program
		 * header contained in "nvp".
		 */
		kmem_free(phdrbase, phdrsize);
		phdrbase = NULL;
		if ((error = getelfhead(nvp, ehdrp, &phdrbase,
		    &phdrsize)) != 0) {
			VN_RELE(nvp);
			uprintf("%s: Cannot read %s\n", exec_file, dlnp);
			goto bad;
		}

		/*
		 * Determine memory size of the "interpreter's" loadable
		 * sections.  This size is then used to obtain the virtual
		 * address of a hole, in the user's address space, large
		 * enough to map the "interpreter".
		 */
		if ((len = elfsize(ehdrp, phdrbase)) == 0) {
			VN_RELE(nvp);
			uprintf("%s: Nothing to load in %s\n", exec_file, dlnp);
			goto bad;
		}
		error = mapelfexec(nvp, ehdrp, phdrbase, &junk, &junk,
		    &junk, NULL, &voffset, len, execsz);

		VN_RELE(nvp);
		if (error || junk != NULL) {
			uprintf("%s: Cannot map %s\n", exec_file, dlnp);
			goto bad;
		}

		/*
		 * Note: AT_SUN_PLATFORM was filled in via exec_args()
		 */
		ADDAUX(aux, AT_BASE, voffset);
		ADDAUX(aux, AT_FLAGS, at_flags);
		ADDAUX(aux, AT_PAGESZ, PAGESIZE);
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
		postfixsize = (char *)aux - (char *)bigwad->elfargs;
		ASSERT(postfixsize == args->auxsize);
		ASSERT(postfixsize <= __KERN_NAUXV_IMPL * sizeof (aux_entry_t));
	}

	/*
	 * For the 64-bit kernel, the limit is big enough that rounding it up
	 * to a page can overflow the 64-bit limit, so we check for btopr()
	 * overflowing here by comparing it with the unrounded limit in pages.
	 * If it hasn't overflowed, compare the exec size with the rounded up
	 * limit in pages.  Otherwise, just compare with the unrounded limit.
	 */
	limit = btop(P_CURLIMIT(p, RLIMIT_VMEM));
	roundlimit = btopr(P_CURLIMIT(p, RLIMIT_VMEM));
	if ((roundlimit > limit && *execsz > roundlimit) ||
	    (roundlimit < limit && *execsz > limit)) {
		error = ENOMEM;
		goto bad;
	}

	bzero(up->u_auxv, sizeof (up->u_auxv));
	up->u_auxvp = NULL;
	if (postfixsize) {
		int num_auxv;

		/*
		 * Copy the aux vector to the user stack.
		 */
		up->u_auxvp = (uintptr_t)stackaddress(args, postfixsize);
		error = execpoststack(args, bigwad->elfargs, postfixsize);
		if (error)
			goto bad;

		/*
		 * Copy auxv to the process's user structure for use by /proc.
		 */
		num_auxv = postfixsize / sizeof (aux_entry_t);
		ASSERT(num_auxv <= sizeof (up->u_auxv) / sizeof (auxv_t));
		aux = bigwad->elfargs;
		for (i = 0; i < num_auxv; i++) {
			up->u_auxv[i].a_type = aux[i].a_type;
			up->u_auxv[i].a_un.a_val = (aux_val_t)aux[i].a_un.a_val;
		}
	}

	/*
	 * XXX -- should get rid of this stuff.
	 */
	up->u_exdata.ux_mag = 0413;
	up->u_exdata.ux_entloc = (caddr_t)(ehdrp->e_entry + voffset);

	if (!uphdr) {
		bigwad->exenv.ex_brkbase = base;
		bigwad->exenv.ex_brksize = 0;
		bigwad->exenv.ex_magic = elfmagic;
		bigwad->exenv.ex_vp = vp;
		setexecenv(&bigwad->exenv);
	}

	ASSERT(error == 0);
	goto out;

bad:
	if (fd != -1)		/* did we open the a.out yet */
		(void) execclose(fd);

	psignal(p, SIGKILL);

	if (error == 0)
		error = ENOEXEC;
out:
	if (phdrbase != NULL)
		kmem_free(phdrbase, phdrsize);
	kmem_free(bigwad, sizeof (struct bigwad));
	return (error);
}

/*
 * Compute the memory size requirement for the ELF file.
 */
static size_t
elfsize(Ehdr *ehdrp, caddr_t phdrbase)
{
	size_t	len;
	Phdr	*phdrp = (Phdr *)phdrbase;
	int	hsize = ehdrp->e_phentsize;
	int	first = 1;
	uintptr_t loaddr = 0;
	uintptr_t hiaddr = 0;
	uintptr_t lo, hi;
	int	i;

	for (i = ehdrp->e_phnum; i > 0; i--) {
		if (phdrp->p_type == PT_LOAD) {
			lo = phdrp->p_vaddr;
			hi = lo + phdrp->p_memsz;
			if (first) {
				loaddr = lo;
				hiaddr = hi;
				first = 0;
			} else {
				if (loaddr > lo)
					loaddr = lo;
				if (hiaddr < hi)
					hiaddr = hi;
			}
		}
		phdrp = (Phdr *)((caddr_t)phdrp + hsize);
	}

	len = hiaddr - (loaddr & PAGEMASK);
	len = roundup(len, PAGESIZE);

	return (len);
}

/*
 * Read in the ELF header and program header table.
 */
static int
getelfhead(
	struct vnode *vp,
	Ehdr *ehdr,
	caddr_t *phdrbase,
	ssize_t *phdrsizep)
{
	int error;
	ssize_t resid;
	ssize_t phdrsize;

	/*
	 * We got here by the first two bytes in ident,
	 * now read the entire ELF header.
	 */
	if ((error = vn_rdwr(UIO_READ, vp, (caddr_t)ehdr,
	    sizeof (Ehdr), (offset_t)0, UIO_SYSSPACE, 0,
	    (rlim64_t)0, CRED(), &resid)) != 0)
		return (error);

	if (resid != 0 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
#if	defined(_ILP32) || defined(_ELF32_COMPAT)
	    ehdr->e_ident[EI_CLASS] != ELFCLASS32 ||
#else	/* _ILP32 || _ELF32_COMPAT */
	    (ehdr->e_ident[EI_CLASS] != ELFCLASS32 &&
	    ehdr->e_ident[EI_CLASS] != ELFCLASS64) ||
#endif	/* _ILP32 || _ELF32_COMPAT */
	    (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) ||
	    !elfheadcheck(ehdr->e_ident[EI_DATA], ehdr->e_machine,
		ehdr->e_flags) ||
	    ehdr->e_phentsize == 0 || (ehdr->e_phentsize & 3))
		return (ENOEXEC);

	/*
	 * Determine the size of the program header table
	 * and read it in.
	 */
	*phdrsizep = phdrsize = ehdr->e_phnum * ehdr->e_phentsize;
	*phdrbase = kmem_alloc(phdrsize, KM_SLEEP);
	return (vn_rdwr(UIO_READ, vp, *phdrbase, phdrsize,
		(offset_t)ehdr->e_phoff, UIO_SYSSPACE, 0, (rlim64_t)0,
		CRED(), &resid));
}

static int
mapelfexec(
	struct vnode *vp,
	Ehdr *ehdr,
	caddr_t phdrbase,
	Phdr **uphdr,
	Phdr **dyphdr,
	Phdr **stphdr,
	caddr_t *base,
	intptr_t *voffset,
	size_t len,
	long *execsz)
{
	Phdr *phdr;
	int i, prot, error;
	caddr_t addr;
	size_t zfodsz;
	int ptload = 0;
	int page;
	off_t offset;
	int hsize = ehdr->e_phentsize;

	if (ehdr->e_type == ET_DYN) {
		/*
		 * Obtain the virtual address of a hole in the
		 * address space to map the "interpreter".
		 */
		map_addr(&addr, len, (offset_t)0, 1, 0);
		if (addr == NULL)
			return (ENOMEM);
		*voffset = (intptr_t)addr;
	} else
		*voffset = 0;

	phdr = (Phdr *)phdrbase;
	for (i = (int)ehdr->e_phnum; i > 0; i--) {
		switch (phdr->p_type) {
		case PT_LOAD:
			if ((*dyphdr != NULL) && (*uphdr == NULL))
				return (0);

			ptload = 1;
			prot = PROT_USER;
			if (phdr->p_flags & PF_R)
				prot |= PROT_READ;
			if (phdr->p_flags & PF_W)
				prot |= PROT_WRITE;
			if (phdr->p_flags & PF_X)
				prot |= PROT_EXEC;

			addr = (caddr_t)phdr->p_vaddr + *voffset;
			zfodsz = (size_t)phdr->p_memsz - phdr->p_filesz;

			offset = phdr->p_offset;
			if (((uintptr_t)offset & PAGEOFFSET) ==
			    ((uintptr_t)addr & PAGEOFFSET) &&
			    (!(vp->v_flag & VNOMAP)))
				page = 1;
			else
				page = 0;

			if (error = execmap(vp, addr, phdr->p_filesz,
			    zfodsz, phdr->p_offset, prot, page))
				goto bad;

			if (base != NULL && addr >= *base)
				*base = addr + phdr->p_memsz;

			*execsz += btopr(phdr->p_memsz);
			break;

		case PT_INTERP:
			if (ptload)
				goto bad;
			*dyphdr = phdr;
			break;

		case PT_SHLIB:
			*stphdr = phdr;
			break;

		case PT_PHDR:
			if (ptload)
				goto bad;
			*uphdr = phdr;
			break;

		case PT_NULL:
		case PT_DYNAMIC:
		case PT_NOTE:
			break;

		default:
			break;
		}
		phdr = (Phdr *)((caddr_t)phdr + hsize);
	}
	return (0);
bad:
	if (error == 0)
		error = ENOEXEC;
	return (error);
}

#define	WRT(vp, base, count, offset, rlimit, credp) \
	vn_rdwr(UIO_WRITE, vp, (caddr_t)base, count, (offset_t)offset, \
	UIO_SYSSPACE, 0, (rlim64_t)rlimit, credp, (ssize_t *)NULL)

extern void setup_old_note_header(Phdr *v, proc_t *p);
extern int write_old_elfnotes(proc_t *p, int sig,
	vnode_t *vp, off_t *offsetp, rlim64_t rlimit, cred_t *credp);
extern void setup_note_header(Phdr *v, proc_t *p);
extern int write_elfnotes(proc_t *p, int sig,
	vnode_t *vp, off_t *offsetp, rlim64_t rlimit, cred_t *credp);

int
elfnote(
	vnode_t *vp,
	off_t *offsetp,
	int type,
	int descsz,
	caddr_t desc,
	rlim64_t rlimit,
	struct cred *credp)
{
	Note note;
	int error;
	offset_t loffset;

	bzero(&note, sizeof (note));
	bcopy("CORE", note.name, 4);
	note.nhdr.n_type = type;
	note.nhdr.n_namesz = 5;	/* ABI says this is strlen(note.name)+1 */
	note.nhdr.n_descsz = roundup(descsz, sizeof (Word));
	loffset = (offset_t)*offsetp;
	if (error = WRT(vp, &note, sizeof (note), loffset, rlimit, credp))
		return (error);
	*offsetp += sizeof (note);
	loffset = (offset_t)*offsetp;
	if (error = WRT(vp, desc, note.nhdr.n_descsz, loffset, rlimit, credp))
		return (error);
	*offsetp += note.nhdr.n_descsz;
	return (0);
}

/*
 * Set elfexec:dump_shared=0 in /etc/system to prevent
 * any shared segments from being dumped.
 */
#ifdef _ELF32_COMPAT
extern	int	dump_shared;
#else
int	dump_shared = 1;
#endif	/* _ELF32_COMPAT */

int
elfcore(vnode_t *vp, proc_t *p, struct cred *credp, rlim64_t rlimit, int sig)
{
	off_t offset, poffset;
	int error, i, nhdrs;
	int overflow = 0;
	struct seg *seg;
	struct as *as = p->p_as;
	union {
		Ehdr ehdr;
		Phdr phdr[1];
	} *bigwad;
	size_t bigsize;
	size_t hdrsz;
	Ehdr *ehdr;
	Phdr *v;

	/*
	 * Make sure we have everything we need (registers, etc.).
	 * All other lwps have already stopped and are in an orderly state.
	 */
	ASSERT(p == ttoproc(curthread));
	prstop(0, 0);

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
	nhdrs = prnsegs(as, 0) + 2;		/* two CORE note sections */
	AS_LOCK_EXIT(as, &as->a_lock);
	hdrsz = nhdrs * sizeof (Phdr);

	bigsize = MAX(sizeof (*bigwad), hdrsz);
	bigwad = kmem_alloc(bigsize, KM_SLEEP);

	ehdr = &bigwad->ehdr;
	bzero(ehdr, sizeof (*ehdr));

	ehdr->e_ident[EI_MAG0] = ELFMAG0;
	ehdr->e_ident[EI_MAG1] = ELFMAG1;
	ehdr->e_ident[EI_MAG2] = ELFMAG2;
	ehdr->e_ident[EI_MAG3] = ELFMAG3;
	ehdr->e_ident[EI_CLASS] = ELFCLASS;
	ehdr->e_type = ET_CORE;

#if !defined(_LP64) || defined(_ELF32_COMPAT)

#if defined(sparc) || defined(__sparc)
	ehdr->e_ident[EI_DATA] = ELFDATA2MSB;
	ehdr->e_machine = EM_SPARC;
#elif defined(i386) || defined(__i386) || defined(__ia64)
	ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr->e_machine = EM_386;
#else
#error "no recognized machine type is defined"
#endif

#else	/* !defined(_LP64) || defined(_ELF32_COMPAT) */

#if defined(sparc) || defined(__sparc)
	ehdr->e_ident[EI_DATA] = ELFDATA2MSB;
	ehdr->e_machine = EM_SPARCV9;
#elif defined(__ia64)
	ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr->e_machine = EM_IA_64;
#else
#error "no recognized 64-bit machine type is defined"
#endif

#endif	/* !defined(_LP64) || defined(_ELF32_COMPAT) */

	ehdr->e_version = EV_CURRENT;
	ehdr->e_phoff = sizeof (Ehdr);
	ehdr->e_ehsize = sizeof (Ehdr);
	ehdr->e_phentsize = sizeof (Phdr);
	ehdr->e_phnum = (unsigned short)nhdrs;
	if (error = WRT(vp, ehdr, sizeof (Ehdr), 0LL, rlimit, credp))
		goto done;

	offset = sizeof (Ehdr);
	poffset = sizeof (Ehdr) + hdrsz;

	v = &bigwad->phdr[0];
	bzero(v, hdrsz);

	setup_old_note_header(&v[0], p);
	v[0].p_offset = poffset;
	poffset += v[0].p_filesz;

	setup_note_header(&v[1], p);
	v[1].p_offset = poffset;
	poffset += v[1].p_filesz;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
	i = 2;
	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			uint_t prot;
			size_t size;

			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if ((size = (size_t)(naddr - saddr)) == 0)
				continue;
			if (i == nhdrs) {
				overflow++;
				continue;
			}
			v[i].p_type = PT_LOAD;
			v[i].p_vaddr = (Addr)saddr;
			v[i].p_memsz = size;
			if (prot & PROT_WRITE)
				v[i].p_flags |= PF_W;
			if (prot & PROT_READ)
				v[i].p_flags |= PF_R;
			if (prot & PROT_EXEC)
				v[i].p_flags |= PF_X;
			if ((prot & PROT_READ) &&
			    (prot & (PROT_WRITE|PROT_EXEC)) != PROT_EXEC) {
				/*
				 * Don't dump a shared memory segment
				 * that has an underlying mapped file.
				 * Do, however, dump SYSV shared memory.
				 * (Unless dump_shared has been set to 0.)
				 */
				vnode_t *mvp;
				if (SEGOP_GETTYPE(seg, saddr) != MAP_SHARED ||
				    (dump_shared != 0 &&
				    (seg->s_ops != &segvn_ops ||
				    SEGOP_GETVP(seg, seg->s_base, &mvp) != 0 ||
				    mvp == NULL || mvp->v_type != VREG))) {
					v[i].p_offset = poffset;
					v[i].p_filesz = size;
					poffset += size;
				}
			}
			i++;
		}
		ASSERT(tmp == NULL);
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	if (overflow) {
		cmn_err(CE_WARN, "elfcore: segment overflow");
		error = EIO;
		goto done;
	}
	if (i < nhdrs) {
		cmn_err(CE_WARN, "elfcore: segment underflow");
		error = EIO;
		goto done;
	}

	error = WRT(vp, v, hdrsz, (offset_t)offset, rlimit, credp);
	if (error)
		goto done;
	offset += hdrsz;

	error = write_old_elfnotes(p, sig, vp, &offset, rlimit, credp);
	if (error)
		goto done;

	error = write_elfnotes(p, sig, vp, &offset, rlimit, credp);
	if (error)
		goto done;

	for (i = 2; !error && i < nhdrs; i++) {
		if (v[i].p_filesz == 0)
			continue;
		error = core_seg(p, vp, v[i].p_offset, (caddr_t)v[i].p_vaddr,
		    v[i].p_filesz, rlimit, credp);
	}

done:
	kmem_free(bigwad, bigsize);
	return (error);
}

#ifndef	_ELF32_COMPAT

static struct execsw esw = {
#ifdef	_LP64
	elf64magicstr,
#else	/* _LP64 */
	elf32magicstr,
#endif	/* _LP64 */
	0,
	5,
	elfexec,
	elfcore
};

static struct modlexec modlexec = {
	&mod_execops, "exec module for elf", &esw
};

#ifdef	_LP64
extern int elf32core(vnode_t *vp, proc_t *p, struct cred *credp,
			rlim64_t rlimit, int sig);
extern int elf32exec(struct vnode *vp, struct execa *uap, struct uarg *args,
			struct intpdata *idatap, int level, long *execsz,
			int setid, caddr_t exec_file, struct cred *cred);

static struct execsw esw32 = {
	elf32magicstr,
	0,
	5,
	elf32exec,
	elf32core
};

static struct modlexec modlexec32 = {
	&mod_execops, "32-bit exec module for elf", &esw32
};
#endif	/* _LP64 */

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlexec,
#ifdef	_LP64
	(void *)&modlexec32,
#endif	/* _LP64 */
	NULL
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

#endif	/* !_ELF32_COMPAT */
