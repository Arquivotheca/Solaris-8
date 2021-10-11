/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pcore.c	1.2	99/09/06 SMI"

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/sysmacros.h>

#include <alloca.h>
#include <rtld_db.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <gelf.h>

#include "Pcontrol.h"
#include "P32ton.h"
#include "Putil.h"

/*
 * Pcore.c - Code to initialize a ps_prochandle from a core dump.  We
 * allocate an additional structure to hold information from the core
 * file, and attach this to the standard ps_prochandle in place of the
 * ability to examine /proc/<pid>/ files.
 */

/*
 * Return the map_info_t for the given virtual address.  We keep a sorted
 * array of pointers in P->core->core_map, so we can binary search.
 */
static map_info_t *
core_lookup(struct ps_prochandle *P, uintptr_t addr)
{
	int mid, lo = 0, hi = P->num_mappings - 1;
	map_info_t *mp;

	while (hi - lo > 1) {
		mid = (lo + hi) / 2;
		if (addr >= P->core->core_map[mid]->map_pmap.pr_vaddr)
			lo = mid;
		else
			hi = mid;
	}

	if (addr < P->core->core_map[hi]->map_pmap.pr_vaddr)
		mp = P->core->core_map[lo];
	else
		mp = P->core->core_map[hi];

	if (addr >= mp->map_pmap.pr_vaddr &&
	    addr < mp->map_pmap.pr_vaddr + mp->map_pmap.pr_size)
		return (mp);

	return (NULL);
}

/*
 * Basic i/o function for reading and writing from the process address space
 * stored in the core file and associated shared libraries.  We compute the
 * appropriate fd and offsets, and let the provided prw function do the rest.
 */
static ssize_t
core_rw(struct ps_prochandle *P, void *buf, size_t n, uintptr_t addr,
    ssize_t (*prw)(int, void *, size_t, off64_t))
{
	ssize_t resid = n;

	while (resid != 0) {
		map_info_t *mp = core_lookup(P, addr);

		uintptr_t mapoff;
		ssize_t len;
		off64_t off;
		int fd;

		if (mp == NULL)
			break;	/* No mapping for this address */

		if (mp->map_external) {
			if (mp->map_file == NULL || mp->map_file->file_fd < 0)
				break;	/* No file or file not open */

			fd = mp->map_file->file_fd;
		} else
			fd = P->asfd;

		mapoff = addr - mp->map_pmap.pr_vaddr;
		len = MIN(resid, mp->map_pmap.pr_size - mapoff);
		off = mp->map_offset + mapoff;

		if ((len = prw(fd, buf, len, off)) <= 0)
			break;

		resid -= len;
		addr += len;
		buf = (char *)buf + len;
	}

	/*
	 * Important: Be consistent with the behavior of i/o on the as file:
	 * writing to an invalid address yields EIO; reading from an invalid
	 * address falls through to returning success and zero bytes.
	 */
	if (resid == n && n != 0 && prw != pread64) {
		errno = EIO;
		return (-1);
	}

	return (n - resid);
}

static ssize_t
Pread_core(struct ps_prochandle *P, void *buf, size_t n, uintptr_t addr)
{
	return (core_rw(P, buf, n, addr, pread64));
}

static ssize_t
Pwrite_core(struct ps_prochandle *P, const void *buf, size_t n, uintptr_t addr)
{
	return (core_rw(P, (void *)buf, n, addr,
	    (ssize_t (*)(int, void *, size_t, off64_t)) pwrite64));
}

static const ps_rwops_t P_core_ops = { Pread_core, Pwrite_core };

/*
 * Return the lwp_info_t for the given lwpid.  If no such lwpid has been
 * encountered yet, allocate a new structure and return a pointer to it.
 */
static lwp_info_t *
lwpid2info(struct ps_prochandle *P, lwpid_t id)
{
	lwp_info_t *lwp = list_next(&P->core->core_lwp_head);
	uint_t i;

	for (i = 0; i < P->core->core_nlwp; i++, lwp = list_next(lwp)) {
		if (lwp->lwp_id == id) {
			P->core->core_lwp = lwp;
			return (lwp);
		}
	}

	if ((lwp = malloc(sizeof (lwp_info_t))) == NULL)
		return (NULL);

	(void) memset(lwp, 0, sizeof (lwp_info_t));
	list_link(lwp, &P->core->core_lwp_head);
	lwp->lwp_id = id;

	P->core->core_lwp = lwp;
	P->core->core_nlwp++;

	return (lwp);
}

/*
 * The core file itself contains a series of NOTE segments containing saved
 * structures from /proc at the time the process died.  For each note we
 * comprehend, we define a function to read it in from the core file,
 * convert it to our native data model if necessary, and store it inside
 * the ps_prochandle.  Each function is invoked by Pfgrab_core() with the
 * seek pointer on P->asfd positioned appropriately.  We populate a table
 * of pointers to these note functions below.
 */

static int
note_pstatus(struct ps_prochandle *P, size_t nbytes)
{
#ifdef _LP64
	if (P->core->core_dmodel == PR_MODEL_ILP32) {
		pstatus32_t ps32;

		if (nbytes < sizeof (pstatus32_t) ||
		    read(P->asfd, &ps32, sizeof (ps32)) != sizeof (ps32))
			goto err;

		pstatus_32_to_n(&ps32, &P->status);

	} else
#endif
	if (nbytes < sizeof (pstatus_t) ||
	    read(P->asfd, &P->status, sizeof (pstatus_t)) != sizeof (pstatus_t))
		goto err;

	P->orig_status = P->status;
	P->pid = P->status.pr_pid;

	return (0);

err:
	dprintf("Pgrab_core: failed to read NT_PSTATUS\n");
	return (-1);
}

static int
note_lwpstatus(struct ps_prochandle *P, size_t nbytes)
{
	lwp_info_t *lwp;
	lwpstatus_t lps;

#ifdef _LP64
	if (P->core->core_dmodel == PR_MODEL_ILP32) {
		lwpstatus32_t l32;

		if (nbytes < sizeof (lwpstatus32_t) ||
		    read(P->asfd, &l32, sizeof (l32)) != sizeof (l32))
			goto err;

		lwpstatus_32_to_n(&l32, &lps);
	} else
#endif
	if (nbytes < sizeof (lwpstatus_t) ||
	    read(P->asfd, &lps, sizeof (lps)) != sizeof (lps))
		goto err;

	if ((lwp = lwpid2info(P, lps.pr_lwpid)) == NULL)
		return (-1);

	/*
	 * Erase a useless and confusing artifact of the kernel implementation:
	 * the lwps which did *not* create the core will show SIGKILL.  We can
	 * be assured this is bogus because SIGKILL can't produce core files.
	 */
	if (lps.pr_cursig == SIGKILL)
		lps.pr_cursig = 0;

	(void) memcpy(&lwp->lwp_status, &lps, sizeof (lps));
	return (0);

err:
	dprintf("Pgrab_core: failed to read NT_LWPSTATUS\n");
	return (-1);
}

static int
note_psinfo(struct ps_prochandle *P, size_t nbytes)
{
#ifdef _LP64
	if (P->core->core_dmodel == PR_MODEL_ILP32) {
		psinfo32_t ps32;

		if (nbytes < sizeof (psinfo32_t) ||
		    read(P->asfd, &ps32, sizeof (ps32)) != sizeof (ps32))
			goto err;

		psinfo_32_to_n(&ps32, &P->psinfo);
	} else
#endif
	if (nbytes < sizeof (psinfo_t) ||
	    read(P->asfd, &P->psinfo, sizeof (psinfo_t)) != sizeof (psinfo_t))
		goto err;

	dprintf("pr_fname = <%s>\n", P->psinfo.pr_fname);
	dprintf("pr_psargs = <%s>\n", P->psinfo.pr_psargs);
	dprintf("pr_wstat = 0x%x\n", P->psinfo.pr_wstat);

	return (0);

err:
	dprintf("Pgrab_core: failed to read NT_PSINFO\n");
	return (-1);
}

static int
note_lwpsinfo(struct ps_prochandle *P, size_t nbytes)
{
	lwp_info_t *lwp;
	lwpsinfo_t lps;

#ifdef _LP64
	if (P->core->core_dmodel == PR_MODEL_ILP32) {
		lwpsinfo32_t l32;

		if (nbytes < sizeof (lwpsinfo32_t) ||
		    read(P->asfd, &l32, sizeof (l32)) != sizeof (l32))
			goto err;

		lwpsinfo_32_to_n(&l32, &lps);
	} else
#endif
	if (nbytes < sizeof (lwpsinfo_t) ||
	    read(P->asfd, &lps, sizeof (lps)) != sizeof (lps))
		goto err;

	if ((lwp = lwpid2info(P, lps.pr_lwpid)) == NULL)
		return (-1);

	(void) memcpy(&lwp->lwp_psinfo, &lps, sizeof (lps));
	return (0);

err:
	dprintf("Pgrab_core: failed to read NT_LWPSINFO\n");
	return (-1);
}

static int
note_platform(struct ps_prochandle *P, size_t nbytes)
{
	char *plat;

	if (P->core->core_platform != NULL)
		return (0);	/* Already seen */

	if (nbytes != 0 && ((plat = malloc(nbytes + 1)) != NULL)) {
		if (read(P->asfd, plat, nbytes) != nbytes) {
			dprintf("Pgrab_core: failed to read NT_PLATFORM\n");
			free(plat);
			return (-1);
		}
		plat[nbytes - 1] = '\0';
		P->core->core_platform = plat;
	}

	return (0);
}

static int
note_utsname(struct ps_prochandle *P, size_t nbytes)
{
	size_t ubytes = sizeof (struct utsname);
	struct utsname *utsp;

	if (P->core->core_uts != NULL || nbytes < ubytes)
		return (0);	/* Already seen or bad size */

	if ((utsp = malloc(ubytes)) == NULL)
		return (-1);

	if (read(P->asfd, utsp, ubytes) != ubytes) {
		dprintf("Pgrab_core: failed to read NT_UTSNAME\n");
		free(utsp);
		return (-1);
	}

	if (_libproc_debug) {
		dprintf("uts.sysname = \"%s\"\n", utsp->sysname);
		dprintf("uts.nodename = \"%s\"\n", utsp->nodename);
		dprintf("uts.release = \"%s\"\n", utsp->release);
		dprintf("uts.version = \"%s\"\n", utsp->version);
		dprintf("uts.machine = \"%s\"\n", utsp->machine);
	}

	P->core->core_uts = utsp;
	return (0);
}

static int
note_cred(struct ps_prochandle *P, size_t nbytes)
{
	prcred_t *pcrp;
	int ngroups;

	if (P->core->core_cred != NULL || nbytes < sizeof (prcred_t))
		return (0);	/* Already seen or bad size */

	ngroups = (nbytes - sizeof (prcred_t)) / sizeof (gid_t) + 1;
	nbytes = sizeof (prcred_t) + (ngroups - 1) * sizeof (gid_t);

	if ((pcrp = malloc(nbytes)) == NULL)
		return (-1);

	if (read(P->asfd, pcrp, nbytes) != nbytes) {
		dprintf("Pgrab_core: failed to read NT_PRCRED\n");
		free(pcrp);
		return (-1);
	}

	if (pcrp->pr_ngroups != ngroups) {
		dprintf("pr_ngroups = %d; resetting to %d based on note size\n",
		    pcrp->pr_ngroups, ngroups);
		pcrp->pr_ngroups = ngroups;
	}

	P->core->core_cred = pcrp;
	return (0);
}

static int
note_auxv(struct ps_prochandle *P, size_t nbytes)
{
	size_t n, i;

#ifdef _LP64
	if (P->core->core_dmodel == PR_MODEL_ILP32) {
		auxv32_t *a32;

		n = nbytes / sizeof (auxv32_t);
		nbytes = n * sizeof (auxv32_t);
		a32 = alloca(nbytes);

		if (read(P->asfd, a32, nbytes) != nbytes) {
			dprintf("Pgrab_core: failed to read NT_AUXV\n");
			return (-1);
		}

		if ((P->auxv = malloc(sizeof (auxv_t) * (n + 1))) == NULL)
			return (-1);

		for (i = 0; i < n; i++)
			auxv_32_to_n(&a32[i], &P->auxv[i]);

	} else {
#endif
		n = nbytes / sizeof (auxv_t);
		nbytes = n * sizeof (auxv_t);

		if ((P->auxv = malloc(nbytes + sizeof (auxv_t))) == NULL)
			return (-1);

		if (read(P->asfd, P->auxv, nbytes) != nbytes) {
			free(P->auxv);
			P->auxv = NULL;
			return (-1);
		}
#ifdef _LP64
	}
#endif

	if (_libproc_debug) {
		for (i = 0; i < n; i++) {
			dprintf("P->auxv[%lu] = ( %d, 0x%lx )\n", (ulong_t)i,
			    P->auxv[i].a_type, P->auxv[i].a_un.a_val);
		}
	}

	/*
	 * Defensive coding for loops which depend upon the auxv array being
	 * terminated by an AT_NULL element: in each case, we've allocated
	 * P->auxv to have an additional element which we force to be AT_NULL:
	 */
	P->auxv[n].a_type = AT_NULL;
	P->auxv[n].a_un.a_val = 0L;

	return (0);
}

#if defined(sparc) || defined(__sparc)
static int
note_xreg(struct ps_prochandle *P, size_t nbytes)
{
	lwp_info_t *lwp = P->core->core_lwp;
	size_t xbytes = sizeof (prxregset_t);
	prxregset_t *xregs;

	if (lwp == NULL || lwp->lwp_xregs != NULL || nbytes < xbytes)
		return (0);	/* No lwp yet, already seen, or bad size */

	if ((xregs = malloc(xbytes)) == NULL)
		return (-1);

	if (read(P->asfd, xregs, xbytes) != xbytes) {
		dprintf("Pgrab_core: failed to read NT_PRXREG\n");
		free(xregs);
		return (-1);
	}

	lwp->lwp_xregs = xregs;
	return (0);
}

static int
note_gwindows(struct ps_prochandle *P, size_t nbytes)
{
	lwp_info_t *lwp = P->core->core_lwp;

	if (lwp == NULL || lwp->lwp_gwins != NULL || nbytes == 0)
		return (0);	/* No lwp yet or already seen or no data */

	if ((lwp->lwp_gwins = malloc(sizeof (gwindows_t))) == NULL)
		return (-1);

	/*
	 * Since the amount of gwindows data varies with how many windows were
	 * actually saved, we just read up to the minimum of the note size
	 * and the size of the gwindows_t type.  It doesn't matter if the read
	 * fails since we have to zero out gwindows first anyway.
	 */
#ifdef _LP64
	if (P->core->core_dmodel == PR_MODEL_ILP32) {
		gwindows32_t g32;

		(void) memset(&g32, 0, sizeof (g32));
		(void) read(P->asfd, &g32, MIN(nbytes, sizeof (g32)));
		gwindows_32_to_n(&g32, lwp->lwp_gwins);

	} else {
#endif
		(void) memset(lwp->lwp_gwins, 0, sizeof (gwindows_t));
		(void) read(P->asfd, lwp->lwp_gwins,
		    MIN(nbytes, sizeof (gwindows_t)));
#ifdef _LP64
	}
#endif
	return (0);
}

#ifdef __sparcv9
static int
note_asrs(struct ps_prochandle *P, size_t nbytes)
{
	lwp_info_t *lwp = P->core->core_lwp;
	int64_t *asrs;

	if (lwp == NULL || lwp->lwp_asrs != NULL || nbytes < sizeof (asrset_t))
		return (0);	/* No lwp yet, already seen, or bad size */

	if ((asrs = malloc(sizeof (asrset_t))) == NULL)
		return (-1);

	if (read(P->asfd, asrs, sizeof (asrset_t)) != sizeof (asrset_t)) {
		dprintf("Pgrab_core: failed to read NT_ASRS\n");
		free(asrs);
		return (-1);
	}

	lwp->lwp_asrs = asrs;
	return (0);
}
#endif	/* __sparcv9 */
#endif	/* __sparc */

/*ARGSUSED*/
static int
note_notsup(struct ps_prochandle *P, size_t nbytes)
{
	dprintf("skipping unsupported note type\n");
	return (0);
}

/*
 * Populate a table of function pointers indexed by Note type with our
 * functions to process each type of core file note:
 */
static int (*nhdlrs[])(struct ps_prochandle *, size_t) = {
	note_notsup,		/*  0	unassigned		*/
	note_notsup,		/*  1	NT_PRSTATUS (old)	*/
	note_notsup,		/*  2	NT_PRFPREG (old)	*/
	note_notsup,		/*  3	NT_PRPSINFO (old)	*/
#if defined(sparc) || defined(__sparc)
	note_xreg,		/*  4	NT_PRXREG		*/
#else
	note_notsup,		/*  4	NT_PRXREG		*/
#endif
	note_platform,		/*  5	NT_PLATFORM		*/
	note_auxv,		/*  6	NT_AUXV			*/
#if defined(sparc) || defined(__sparc)
	note_gwindows,		/*  7	NT_GWINDOWS		*/
#if defined(__sparcv9)
	note_asrs,		/*  8	NT_ASRS			*/
#else
	note_notsup,		/*  8	NT_ASRS			*/
#endif
#else
	note_notsup,		/*  7	NT_GWINDOWS		*/
	note_notsup,		/*  8	NT_ASRS			*/
#endif
	note_notsup,		/*  9	unassigned		*/
	note_pstatus,		/* 10	NT_PSTATUS		*/
	note_notsup,		/* 11	unassigned		*/
	note_notsup,		/* 12	unassigned		*/
	note_psinfo,		/* 13	NT_PSINFO		*/
	note_cred,		/* 14	NT_PRCRED		*/
	note_utsname,		/* 15	NT_UTSNAME		*/
	note_lwpstatus,		/* 16	NT_LWPSTATUS		*/
	note_lwpsinfo		/* 17	NT_LWPSINFO		*/
};

/*
 * Add information on the address space mapping described by the given
 * PT_LOAD program header.  We fill in more information on the mapping later.
 */
static int
core_add_mapping(struct ps_prochandle *P, GElf_Phdr *php)
{
	map_info_t *mp = malloc(sizeof (map_info_t));

	if (mp == NULL) {
		dprintf("Pgrab_core: failed to alloc for mapping at %llx\n",
		    (u_longlong_t)php->p_vaddr);
		return (-1);
	}

	dprintf("mapping base %llx filesz %llu memsz %llu offset %llu\n",
	    (u_longlong_t)php->p_vaddr, (u_longlong_t)php->p_filesz,
	    (u_longlong_t)php->p_memsz, (u_longlong_t)php->p_offset);

	P->num_mappings++;
	list_link(mp, &P->map_head);

	mp->map_offset = php->p_offset;
	mp->map_external = php->p_filesz == 0;
	mp->map_file = NULL;

	mp->map_pmap.pr_vaddr = (uintptr_t)php->p_vaddr;
	mp->map_pmap.pr_size = php->p_memsz;

	if (!mp->map_external && mp->map_offset >= P->core->core_size) {
		dprintf("Pgrab_core: core file may be corrupt -- data for "
		    "mapping at %p is missing\n", (void *)php->p_vaddr);
	}

	/*
	 * The mapping name and offset will hopefully be filled in
	 * by the librtld_db agent.  Unfortunately, if it isn't a
	 * shared library mapping, this information is gone forever.
	 */
	mp->map_pmap.pr_mapname[0] = '\0';
	mp->map_pmap.pr_offset = 0;

	mp->map_pmap.pr_mflags = 0;
	if (php->p_flags & PF_R)
		mp->map_pmap.pr_mflags |= MA_READ;
	if (php->p_flags & PF_W)
		mp->map_pmap.pr_mflags |= MA_WRITE;
	if (php->p_flags & PF_X)
		mp->map_pmap.pr_mflags |= MA_EXEC;

	/*
	 * At the time of adding this mapping, we just zero the pagesize.
	 * Once we've processed more of the core file, we'll have the
	 * pagesize from the auxv's AT_PAGESZ element and we can fill this in.
	 */
	mp->map_pmap.pr_pagesize = 0;

	/*
	 * Unfortunately whether or not the mapping was a System V
	 * shared memory segment is lost.  We use -1 to mark it as not shm.
	 */
	mp->map_pmap.pr_shmid = -1;
	return (0);
}

/*
 * Order mappings based on virtual address.  We use this function as the
 * callback for sorting the array of map_info_t pointers.
 */
static int
core_cmp_mapping(const void *lhsp, const void *rhsp)
{
	const map_info_t *lhs = *((const map_info_t **)lhsp);
	const map_info_t *rhs = *((const map_info_t **)rhsp);

	if (lhs->map_pmap.pr_vaddr == rhs->map_pmap.pr_vaddr)
		return (0);

	return (lhs->map_pmap.pr_vaddr < rhs->map_pmap.pr_vaddr ? -1 : 1);
}

/*
 * Given a virtual address, name the mapping at that address using the
 * specified name, and return the map_info_t pointer.
 */
static map_info_t *
core_name_mapping(struct ps_prochandle *P, uintptr_t addr, const char *name)
{
	map_info_t *mp = core_lookup(P, addr);

	if (mp != NULL) {
		(void) strncpy(mp->map_pmap.pr_mapname, name, PRMAPSZ);
		mp->map_pmap.pr_mapname[PRMAPSZ - 1] = '\0';
	}

	return (mp);
}

/*
 * Perform elf_begin on efp->e_fd and verify the ELF file's type and class.
 */
static int
core_elf_fdopen(elf_file_t *efp, GElf_Half type, int *perr)
{
	if ((efp->e_elf = elf_begin(efp->e_fd, ELF_C_READ, NULL)) == NULL) {
		if (perr != NULL)
			*perr = G_ELF;
		goto err;
	}

	if (elf_kind(efp->e_elf) != ELF_K_ELF || gelf_getehdr(efp->e_elf,
	    &efp->e_hdr) == NULL || efp->e_hdr.e_type != type) {
		if (perr != NULL)
			*perr = G_FORMAT;
		goto err;
	}

#ifndef _LP64
	if (efp->e_hdr.e_ident[EI_CLASS] == ELFCLASS64) {
		if (perr != NULL)
			*perr = G_LP64;
		goto err;
	}
#endif

	return (0);

err:
	(void) elf_end(efp->e_elf);
	efp->e_elf = NULL;
	return (-1);
}

/*
 * Open the specified file and then do a core_elf_fdopen on it.
 */
static int
core_elf_open(elf_file_t *efp, const char *path, GElf_Half type, int *perr)
{
	(void) memset(efp, 0, sizeof (elf_file_t));

	if ((efp->e_fd = open(path, O_RDONLY)) >= 0) {
		if (core_elf_fdopen(efp, type, perr) == 0)
			return (0);

		(void) close(efp->e_fd);
		efp->e_fd = -1;
	}

	return (-1);
}

/*
 * Close the ELF handle and file descriptor.
 */
static void
core_elf_close(elf_file_t *efp)
{
	if (efp->e_elf != NULL) {
		(void) elf_end(efp->e_elf);
		efp->e_elf = NULL;
	}

	if (efp->e_fd != -1) {
		(void) close(efp->e_fd);
		efp->e_fd = -1;
	}
}

/*
 * Given an ELF file for a statically linked executable, locate the likely
 * primary text section and fill in rl_base with its virtual address.
 */
static map_info_t *
core_find_text(struct ps_prochandle *P, Elf *elf, rd_loadobj_t *rlp)
{
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;
	uint_t i;

	if (gelf_getehdr(elf, &ehdr) != NULL) {
		for (i = 0; i < ehdr.e_phnum; i++) {
			if (gelf_getphdr(elf, i, &phdr) != NULL &&
			    phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X)) {
				rlp->rl_base = phdr.p_vaddr;
				return (core_lookup(P, rlp->rl_base));
			}
		}
	}

	return (NULL);
}

/*
 * Given an ELF file and the librtld_db structure corresponding to its primary
 * text mapping, deduce where its data segment was loaded and fill in
 * rl_data_base and prmap_t.pr_offset accordingly.
 */
static map_info_t *
core_find_data(struct ps_prochandle *P, Elf *elf, rd_loadobj_t *rlp)
{
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;

	map_info_t *mp;
	uint_t i, pagemask;

	rlp->rl_data_base = NULL;

	/*
	 * Find the first loadable, writeable Phdr and compute rl_data_base
	 * as the virtual address at which is was loaded.
	 */
	if (gelf_getehdr(elf, &ehdr) != NULL) {
		for (i = 0; i < ehdr.e_phnum; i++) {
			if (gelf_getphdr(elf, i, &phdr) != NULL &&
			    phdr.p_type == PT_LOAD && (phdr.p_flags & PF_W)) {

				rlp->rl_data_base = phdr.p_vaddr;
				if (ehdr.e_type == ET_DYN)
					rlp->rl_data_base += rlp->rl_base;
				break;
			}
		}
	}

	/*
	 * If we didn't find an appropriate phdr or if the address we
	 * computed has no mapping, return NULL.
	 */
	if (rlp->rl_data_base == NULL ||
	    (mp = core_lookup(P, rlp->rl_data_base)) == NULL)
		return (NULL);

	/*
	 * It wouldn't be procfs-related code if we didn't make use of
	 * unclean knowledge of segvn, even in userland ... the prmap_t's
	 * pr_offset field will be the segvn offset from mmap(2)ing the
	 * data section, which will be the file offset & PAGEMASK.
	 */
	pagemask = ~(mp->map_pmap.pr_pagesize - 1);
	mp->map_pmap.pr_offset = phdr.p_offset & pagemask;

	return (mp);
}

/*
 * Librtld_db agent callback for iterating over load object mappings.
 * For each load object, we allocate a new file_info_t, perform naming,
 * and attempt to construct a symbol table for the load object.
 */
static int
core_iter_mapping(const rd_loadobj_t *rlp, struct ps_prochandle *P)
{
	char lname[PATH_MAX];
	file_info_t *fp;
	map_info_t *mp;

	if (Pread_string(P, lname, PATH_MAX, (off_t)rlp->rl_nameaddr) <= 0) {
		dprintf("failed to read name %p\n", (void *)rlp->rl_nameaddr);
		return (1); /* Keep going; forget this if we can't get a name */
	}

	dprintf("rd_loadobj name = \"%s\" rl_base = %p\n",
	    lname, (void *)rlp->rl_base);

	if ((mp = core_lookup(P, rlp->rl_base)) == NULL)
		return (1); /* Keep going; no mapping at this address */

	if ((fp = mp->map_file) == NULL) {
		if ((fp = malloc(sizeof (file_info_t))) == NULL) {
			P->core->core_errno = errno;
			dprintf("failed to malloc mapping data\n");
			return (0); /* Abort */
		}

		(void) memset(fp, 0, sizeof (file_info_t));

		list_link(fp, &P->file_head);
		mp->map_file = fp;
		P->num_files++;

		fp->file_ref = 1;
		fp->file_fd = -1;
	}

	if ((fp->file_lo = malloc(sizeof (rd_loadobj_t))) == NULL) {
		P->core->core_errno = errno;
		dprintf("failed to malloc mapping data\n");
		return (0); /* Abort */
	}

	*fp->file_lo = *rlp;

	/*
	 * Naming dance part 1: if we got a name from librtld_db, then
	 * copy this name to the prmap_t if it is unnamed.  If the file_info_t
	 * is unnamed, name if after the execname or lname as appropriate.
	 */
	if (lname[0] != '\0') {
		if (mp->map_pmap.pr_mapname[0] == '\0') {
			(void) strncpy(mp->map_pmap.pr_mapname, lname, PRMAPSZ);
			mp->map_pmap.pr_mapname[PRMAPSZ - 1] = '\0';
		}

		if (fp->file_lname == NULL) {
			if (strcmp(mp->map_pmap.pr_mapname, "a.out") == 0)
				fp->file_lname = P->execname ?
				    strdup(P->execname) : NULL;
			else
				fp->file_lname = strdup(lname);
		}
	}

	/*
	 * Naming dance part 2: if the mapping is named and the file_info_t
	 * is not, name the file after the execname or mapping as appropriate.
	 */
	if (fp->file_lname == NULL && mp->map_pmap.pr_mapname[0] != '\0') {
		if (strcmp(mp->map_pmap.pr_mapname, "a.out") == 0)
			fp->file_lname = P->execname ?
			    strdup(P->execname) : NULL;
		else
			fp->file_lname = strdup(mp->map_pmap.pr_mapname);
	}

	if (fp->file_lname != NULL)
		fp->file_lbase = basename(fp->file_lname);

	/*
	 * Associate the file and the mapping, and attempt to build
	 * a symbol table for this file.
	 */
	(void) strcpy(fp->file_pname, mp->map_pmap.pr_mapname);
	fp->file_map = mp;

	Pbuild_file_symtab(P, fp);

	if (fp->file_elf == NULL)
		return (1); /* No symbol table; advance to next mapping */

	/*
	 * Locate the start of a data segment associated with this file,
	 * name it after the file, and establish the mp->map_file link:
	 */
	if ((mp = core_find_data(P, fp->file_elf, fp->file_lo)) != NULL) {
		map_info_t *nmp = core_lookup(P,
		    mp->map_pmap.pr_vaddr + mp->map_pmap.pr_size);

		dprintf("found data for %s at %p (pr_offset 0x%llx)\n",
		    fp->file_pname, (void *)fp->file_lo->rl_data_base,
		    mp->map_pmap.pr_offset);

		if (mp->map_file == NULL) {
			mp->map_file = fp;
			fp->file_ref++;
		}

		(void) strcpy(mp->map_pmap.pr_mapname, fp->file_pname);

		/*
		 * If there is a mapping immediately adjacent to the data that
		 * is not labeled as the heap, assume it is a BSS anon mapping.
		 */
		if (nmp != NULL && nmp->map_file == NULL &&
		    nmp->map_pmap.pr_mapname[0] == '\0' &&
		    (nmp->map_pmap.pr_mflags &
		    (MA_READ | MA_WRITE | MA_BREAK)) == (MA_READ | MA_WRITE)) {
			(void) strcpy(nmp->map_pmap.pr_mapname, fp->file_pname);
			nmp->map_pmap.pr_mflags |= MA_ANON;
			nmp->map_file = fp;
			fp->file_ref++;
		}
	}

	return (1); /* Advance to next mapping */
}

/*
 * Callback function for Pfindexec().  In order to confirm a given pathname,
 * we verify that we can open it as an ELF file of type ET_EXEC.
 */
static int
core_exec_open(const char *path, void *efp)
{
	return (core_elf_open(efp, path, ET_EXEC, NULL) == 0);
}

/*
 * Main engine for core file initialization: given an fd for the core file
 * and an optional pathname, construct the ps_prochandle.  The aout_path can
 * either be a suggested executable pathname, or a suggested directory to
 * use as a possible current working directory.
 */
struct ps_prochandle *
Pfgrab_core(int core_fd, const char *aout_path, int *perr)
{
	struct ps_prochandle *P;
	map_info_t *mp, *stk_mp, *brk_mp;
	const char *execname;
	char *interp;
	int i, notes, pagesize;
	uintptr_t addr, base_addr;
	struct stat64 stbuf;

	elf_file_t aout;
	elf_file_t core;

	Elf_Scn *scn, *intp_scn = NULL;
	Elf_Data *dp;

	GElf_Phdr phdr, note_phdr;
	GElf_Shdr shdr;
	GElf_Xword nleft;


	if (elf_version(EV_CURRENT) == EV_NONE) {
		dprintf("libproc ELF version is more recent than libelf\n");
		*perr = G_ELF;
		return (NULL);
	}

	aout.e_elf = NULL;
	aout.e_fd = -1;

	core.e_elf = NULL;
	core.e_fd = core_fd;

	/*
	 * Allocate and initialize a ps_prochandle structure for the core.
	 * There are several key pieces of initialization here:
	 *
	 * 1. The PS_DEAD state flag marks this prochandle as a core file.
	 *    PS_DEAD also thus prevents all operations which require state
	 *    to be PS_STOP from operating on this handle.
	 *
	 * 2. We keep the core file fd in P->asfd since the core file contains
	 *    the remnants of the process address space.
	 *
	 * 3. We set the P->info_valid bit because all information about the
	 *    core is determined by the end of this function; there is no need
	 *    for proc_update_maps() to reload mappings at any later point.
	 *
	 * 4. The read/write ops vector uses our core_rw() function defined
	 *    above to handle i/o requests.
	 */
	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = G_STRANGE;
		return (NULL);
	}

	(void) memset(P, 0, sizeof (struct ps_prochandle));

	P->state = PS_DEAD;
	P->pid = (pid_t)-1;
	P->asfd = core.e_fd;
	P->ctlfd = -1;
	P->statfd = -1;
	P->agentctlfd = -1;
	P->agentstatfd = -1;
	P->info_valid = 1;
	P->ops = &P_core_ops;

	Pinitsym(P);

	/*
	 * Allocate a core_info_t to hang off the ps_prochandle structure.
	 * We keep all core-specific information in this structure.
	 */
	if ((P->core = malloc(sizeof (core_info_t))) == NULL) {
		*perr = G_STRANGE;
		goto err;
	}

	/*
	 * Fstat and open the core file and make sure it is a valid ELF core.
	 */
	if (fstat64(P->asfd, &stbuf) == -1) {
		*perr = G_STRANGE;
		goto err;
	}

	if (core_elf_fdopen(&core, ET_CORE, perr) == -1)
		goto err;

	switch (core.e_hdr.e_ident[EI_CLASS]) {
	case ELFCLASS32:
		P->core->core_dmodel = PR_MODEL_ILP32;
		break;
	case ELFCLASS64:
		P->core->core_dmodel = PR_MODEL_LP64;
		break;
	default:
		*perr = G_FORMAT;
		goto err;
	}

	/*
	 * Finish up initialization of P->core:
	 */
	list_link(&P->core->core_lwp_head, NULL);
	P->core->core_errno = 0;
	P->core->core_map = NULL;
	P->core->core_lwp = NULL;
	P->core->core_nlwp = 0;
	P->core->core_size = stbuf.st_size;
	P->core->core_platform = NULL;
	P->core->core_uts = NULL;
	P->core->core_cred = NULL;

	/*
	 * Now iterate through the program headers in the core file.
	 * We're interested in two types of Phdrs: PT_NOTE (which
	 * contains a set of saved /proc structures), and PT_LOAD (which
	 * represents a memory mapping from the process's address space).
	 * In the case of PT_NOTE, we're interested in the last PT_NOTE
	 * in the core file; currently the first PT_NOTE (if present)
	 * contains /proc structs in the pre-2.6 unstructured /proc format.
	 */
	for (notes = 0, i = 0; i < core.e_hdr.e_phnum; i++) {
		if (gelf_getphdr(core.e_elf, i, &phdr) != NULL) {
			switch (phdr.p_type) {
			case PT_NOTE:
				note_phdr = phdr;
				notes++;
				break;

			case PT_LOAD:
				if (core_add_mapping(P, &phdr) == -1) {
					*perr = G_STRANGE;
					goto err;
				}
				break;
			}
		}
	}

	/*
	 * If we couldn't find anything of type PT_NOTE, or only one PT_NOTE
	 * was present, abort.  The core file is either corrupt or too old.
	 */
	if (notes == 0 || notes == 1) {
		*perr = G_NOTE;
		goto err;
	}

	/*
	 * Advance the seek pointer to the start of the PT_NOTE data
	 */
	if (lseek64(P->asfd, note_phdr.p_offset, SEEK_SET) == (off64_t)-1) {
		dprintf("Pgrab_core: failed to lseek to PT_NOTE data\n");
		*perr = G_STRANGE;
		goto err;
	}

	/*
	 * Now process the PT_NOTE structures.  Each one is preceded by
	 * an Elf{32/64}_Nhdr structure describing its type and size.
	 */
	for (nleft = note_phdr.p_filesz; nleft != 0; ) {
		struct Note {
			Elf64_Nhdr hdr;
			char name[8];
		} n;

		off64_t off;

		/*
		 * Although <sys/elf.h> defines both Elf32_Nhdr and Elf64_Nhdr
		 * as different types, they are both of the same content and
		 * size, so we don't need to worry about 32/64 conversion here.
		 */
		if (read(P->asfd, &n, sizeof (n)) != sizeof (n)) {
			dprintf("Pgrab_core: failed to read ELF note header\n");
			*perr = G_STRANGE;
			goto err;
		}

		if (n.hdr.n_namesz > sizeof (n.name)) {
			dprintf("Pgrab_core: corrupt ELF note header\n");
			*perr = G_STRANGE;
			goto err;
		}

		dprintf("Note hdr \"%s\" n_type=%u n_descsz=%u\n",
		    n.name, n.hdr.n_type, n.hdr.n_descsz);

		off = lseek64(P->asfd, (off64_t)0L, SEEK_CUR);

		/*
		 * Invoke the note handler function from our table
		 */
		if (n.hdr.n_type < sizeof (nhdlrs) / sizeof (nhdlrs[0])) {
			if (nhdlrs[n.hdr.n_type](P, n.hdr.n_descsz) < 0) {
				*perr = G_STRANGE;
				goto err;
			}
		} else
			(void) note_notsup(P, n.hdr.n_descsz);

		/*
		 * Seek past the current note data to the next Elf_Nhdr
		 */
		if (lseek64(P->asfd, off + n.hdr.n_descsz,
		    SEEK_SET) == (off64_t)-1) {
			dprintf("Pgrab_core: failed to seek to next nhdr\n");
			*perr = G_STRANGE;
			goto err;
		}

		/*
		 * Subtract the size of the header and its data from what
		 * we have left to process.
		 */
		nleft -= sizeof (n) + n.hdr.n_descsz;
	}

	/*
	 * For address translation, we allocate an array of pointers to the
	 * map_info_t's already added to P->map_head.  This list is then
	 * sorted by virtual address for fast translation.
	 */
	if ((P->core->core_map = malloc(P->num_mappings *
	    sizeof (map_info_t *))) == NULL) {
		*perr = G_STRANGE;
		goto err;
	}

	if ((pagesize = Pgetauxval(P, AT_PAGESZ)) == -1) {
		pagesize = getpagesize();
		dprintf("AT_PAGESZ missing; defaulting to %d\n", pagesize);
	}

	/*
	 * Iterate through the mappings, adding a pointer to each to the
	 * core_map array.  While we're iterating, reset each pr_pagesize
	 * field to the AT_PAGESZ value from the auxv (or current pagesize).
	 */
	for (mp = list_next(&P->map_head), i = 0; i < P->num_mappings; i++) {
		P->core->core_map[i] = mp;
		mp->map_pmap.pr_pagesize = pagesize;
		mp = list_next(mp);
	}

	qsort(P->core->core_map, P->num_mappings, sizeof (map_info_t *),
	    core_cmp_mapping);

	/*
	 * Locate and label the mappings corresponding to the end of the
	 * heap (MA_BREAK) and the base of the stack (MA_STACK).
	 */
	if (P->status.pr_brksize != 0 && (brk_mp = core_lookup(P,
	    P->status.pr_brkbase + P->status.pr_brksize - 1)) != NULL)
		brk_mp->map_pmap.pr_mflags |= MA_BREAK;
	else
		brk_mp = NULL;

	if ((stk_mp = core_lookup(P, P->status.pr_stkbase)) != NULL)
		stk_mp->map_pmap.pr_mflags |= MA_STACK;

	/*
	 * At this point, we have enough information to look for the
	 * executable and open it: we have access to the auxv, a psinfo_t,
	 * and the ability to read from mappings provided by the core file.
	 */
	(void) Pfindexec(P, aout_path, core_exec_open, &aout);
	dprintf("P->execname = \"%s\"\n", P->execname ? P->execname : "NULL");
	execname = P->execname ? P->execname : "a.out";

	/*
	 * Iterate through the sections, looking for the .dynamic and .interp
	 * sections.  If we encounter them, remember their section pointers.
	 */
	for (scn = NULL; (scn = elf_nextscn(aout.e_elf, scn)) != NULL; ) {
		char *sname;

		if ((gelf_getshdr(scn, &shdr) == NULL) ||
		    (sname = elf_strptr(aout.e_elf, aout.e_hdr.e_shstrndx,
		    (size_t)shdr.sh_name)) == NULL)
			continue;

		if (strcmp(sname, ".interp") == 0)
			intp_scn = scn;
	}

	/*
	 * Get the AT_BASE auxv element.  If this is missing (-1), then
	 * we assume this is a statically-linked executable.
	 */
	base_addr = Pgetauxval(P, AT_BASE);

	/*
	 * In order to get librtld_db initialized, we'll need to identify
	 * and name the mapping corresponding to the run-time linker.  The
	 * AT_BASE auxv element tells us the address where it was mapped,
	 * and the .interp section of the executable tells us its path.
	 * If for some reason that doesn't pan out, just use ld.so.1.
	 */
	if (intp_scn != NULL && (dp = elf_getdata(intp_scn, NULL)) != NULL &&
	    dp->d_size != 0) {
		dprintf(".interp = <%s>\n", (char *)dp->d_buf);
		interp = dp->d_buf;

	} else if (base_addr != (uintptr_t)-1L) {
		if (P->core->core_dmodel == PR_MODEL_LP64)
			interp = "/usr/lib/64/ld.so.1";
		else
			interp = "/usr/lib/ld.so.1";

		dprintf(".interp section is missing or could not be read; "
		    "defaulting to %s\n", interp);
	} else
		dprintf("detected statically linked executable\n");

	/*
	 * If we have an AT_BASE element, name the mapping at that address
	 * using the interpreter pathname.  Name the corresponding data
	 * mapping after the interpreter as well.
	 */
	if (base_addr != (uintptr_t)-1L) {
		elf_file_t intf;

		P->map_ldso = core_name_mapping(P, base_addr, interp);

		if (core_elf_open(&intf, interp, ET_DYN, NULL) == 0) {
			rd_loadobj_t rl;
			map_info_t *dmp;

			rl.rl_base = base_addr;
			dmp = core_find_data(P, intf.e_elf, &rl);

			if (dmp != NULL) {
				dprintf("renamed data at %p to %s\n",
				    (void *)rl.rl_data_base, interp);
				(void) strncpy(dmp->map_pmap.pr_mapname,
				    interp, PRMAPSZ);
				dmp->map_pmap.pr_mapname[PRMAPSZ - 1] = '\0';
			}
		}

		core_elf_close(&intf);
	}

	/*
	 * If we have an AT_ENTRY element, name the mapping at that address
	 * using the special name "a.out" just like /proc does.
	 */
	if ((addr = Pgetauxval(P, AT_ENTRY)) != (uintptr_t)-1L)
		P->map_exec = core_name_mapping(P, addr, "a.out");

	/*
	 * If we're a statically linked executable, then just locate the
	 * executable's text and data and name them after the executable.
	 */
	if (base_addr == (uintptr_t)-1L) {
		map_info_t *tmp, *dmp;
		file_info_t *fp;
		rd_loadobj_t rl;

		if ((tmp = core_find_text(P, aout.e_elf, &rl)) != NULL &&
		    (dmp = core_find_data(P, aout.e_elf, &rl)) != NULL) {
			(void) strncpy(tmp->map_pmap.pr_mapname,
			    execname, PRMAPSZ);
			tmp->map_pmap.pr_mapname[PRMAPSZ - 1] = '\0';
			(void) strncpy(dmp->map_pmap.pr_mapname,
			    execname, PRMAPSZ);
			dmp->map_pmap.pr_mapname[PRMAPSZ - 1] = '\0';
		}

		if ((P->map_exec = tmp) != NULL &&
		    (fp = malloc(sizeof (file_info_t))) != NULL) {

			(void) memset(fp, 0, sizeof (file_info_t));

			list_link(fp, &P->file_head);
			tmp->map_file = fp;
			P->num_files++;

			fp->file_ref = 1;
			fp->file_fd = -1;

			fp->file_lo = malloc(sizeof (rd_loadobj_t));
			fp->file_lname = strdup(execname);

			if (fp->file_lo)
				*fp->file_lo = rl;
			if (fp->file_lname)
				fp->file_lbase = basename(fp->file_lname);

			(void) strcpy(fp->file_pname, mp->map_pmap.pr_mapname);
			fp->file_map = tmp;

			Pbuild_file_symtab(P, fp);

			if (dmp != NULL) {
				dmp->map_file = fp;
				fp->file_ref++;
			}
		}
	}

	(void) elf_end(core.e_elf); /* Leave core fd open */
	core_elf_close(&aout);

	/*
	 * We now have enough information to initialize librtld_db.
	 * After it warms up, we can iterate through the load object chain
	 * in the core, which will allow us to construct the file info
	 * we need to provide symbol information for the other shared
	 * libraries, and also to fill in the missing mapping names.
	 */
	rd_log(_libproc_debug);

	if ((P->rap = rd_new(P)) != NULL) {
		(void) rd_loadobj_iter(P->rap, (rl_iter_f *)
		    core_iter_mapping, P);

		if (P->core->core_errno != 0) {
			errno = P->core->core_errno;
			*perr = G_STRANGE;
			goto err;
		}
	} else
		dprintf("failed to initialize rtld_db agent\n");

	/*
	 * If we previously located a stack or break mapping, and they are
	 * still anonymous, we now assume that they were MAP_ANON mappings.
	 * If brk_mp turns out to now have a name, then the heap is still
	 * sitting at the end of the executable's data+bss mapping: remove
	 * the previous MA_BREAK setting to be consistent with /proc.
	 */
	if (stk_mp != NULL && stk_mp->map_pmap.pr_mapname[0] == '\0')
		stk_mp->map_pmap.pr_mflags |= MA_ANON;
	if (brk_mp != NULL && brk_mp->map_pmap.pr_mapname[0] == '\0')
		brk_mp->map_pmap.pr_mflags |= MA_ANON;
	else if (brk_mp != NULL)
		brk_mp->map_pmap.pr_mflags &= ~MA_BREAK;

	*perr = 0;
	return (P);

err:
	Pfree(P);

	(void) elf_end(core.e_elf);
	core_elf_close(&aout);

	return (NULL);
}

/*
 * Grab a core file using a pathname.  We just open it and call Pfgrab_core().
 */
struct ps_prochandle *
Pgrab_core(const char *core, const char *aout, int gflag, int *perr)
{
	int fd, oflag = (gflag & PGRAB_RDONLY) ? O_RDONLY : O_RDWR;

	if ((fd = open64(core, oflag)) >= 0)
		return (Pfgrab_core(fd, aout, perr));

	if (errno != ENOENT)
		*perr = G_STRANGE;
	else
		*perr = G_NOCORE;

	return (NULL);
}
