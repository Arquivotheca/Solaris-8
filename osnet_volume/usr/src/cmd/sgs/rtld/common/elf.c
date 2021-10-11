/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elf.c	1.124	99/11/03 SMI"


/*
 * Object file dependent support for ELF objects.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/procfs.h>
#include	<sys/mman.h>
#include	<string.h>
#include	<limits.h>
#include	<dlfcn.h>
#include	"conv.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#include	"debug.h"
#include	"profile.h"

/*
 * Directory search rules for ELF objects.
 */
static int		elf_search_rules[] = {
	ENVDIRS,	RUNDIRS,	DEFAULT,	0
};

static Pnode		elf_dflt_dirs[] = {
#if	defined(_ELF64) && defined(__sparcv9)
	{ MSG_ORIG(MSG_PTH_SP64_USRLIB),	MSG_PTH_SP64_USRLIB_SIZE,
#elif	defined(_ELF64) && defined(__ia64)
	{ MSG_ORIG(MSG_PTH_IA64_USRLIB),	MSG_PTH_IA64_USRLIB_SIZE,
#else
	{ MSG_ORIG(MSG_PTH_USRLIB),		MSG_PTH_USRLIB_SIZE,
#endif
		LA_SER_DEFAULT,			0, 0 }
};

static Pnode		elf_secure_dirs[] = {
#if	defined(_ELF64) && defined(__sparcv9)
	{ MSG_ORIG(MSG_PTH_SP64_USRLIBSE),	MSG_PTH_SP64_USRLIBSE_SIZE,
#elif	defined(_ELF64) && defined(__ia64)
	{ MSG_ORIG(MSG_PTH_IA64_USRLIBSE),	MSG_PTH_IA64_USRLIBSE_SIZE,
#else
	{ MSG_ORIG(MSG_PTH_USRLIBSE),		MSG_PTH_USRLIBSE_SIZE,
#endif
		LA_SER_SECURE,			0, 0 }
};

/*
 * Defines for local functions.
 */
static char *		elf_fix_name(const char *, Rt_map *);
static int		elf_are_u(void);
static void		elf_dladdr(unsigned long, Rt_map *, Dl_info *);
static unsigned long	elf_entry_pt(void);
static char *		elf_get_so(const char *, const char *);
static int		elf_needed(Rt_map *);
static Rt_map *		elf_new_lm(Lm_list *, const char *, const char *, Dyn *,
				unsigned long, unsigned long, unsigned long,
				unsigned long, Phdr *, unsigned int,
				unsigned int, unsigned long, unsigned long);
static Rt_map *		elf_map_so(Lm_list *, const char *, const char *);
static int		elf_unmap_so(Rt_map *);
static int		elf_are_u_compatible(Word *);
static int		elf_verify_vers(const char *, Rt_map *, Rt_map *);

/*
 * Functions and data accessed through indirect pointers.
 */
Fct elf_fct = {
	elf_are_u,
	elf_entry_pt,
	elf_map_so,
	elf_new_lm,
	elf_unmap_so,
	elf_needed,
	lookup_sym,
	elf_find_sym,
	elf_reloc,
	elf_search_rules,
	elf_dflt_dirs,
	elf_secure_dirs,
	elf_fix_name,
	elf_get_so,
	elf_dladdr,
	dlsym_handle,
	elf_are_u_compatible,
	elf_verify_vers
};


/*
 * Redefine NEEDED name if necessary.
 */
static char *
elf_fix_name(const char * name, Rt_map * lmp)
{
	char *	_name;
	size_t	_len;
	int	_exp;

	PRF_MCOUNT(40, elf_fix_name);

	/*
	 * For ABI compliance, if we are asked for ld.so.1, then really give
	 * them libsys.so.1 (the SONAME of libsys.so.1 is ld.so.1).
	 */
#if	defined(_ELF64) && defined(__sparcv9)
	if ((strcmp(name, MSG_ORIG(MSG_PTH_SP64_RTLD)) == 0) ||
#elif	defined(_ELF64) && defined(__ia64)
	if ((strcmp(name, MSG_ORIG(MSG_PTH_IA64_RTLD)) == 0) ||
#else
	if ((strcmp(name, MSG_ORIG(MSG_PTH_RTLD)) == 0) ||
#endif
	    (strcmp(name, MSG_ORIG(MSG_FIL_RTLD)) == 0)) {
		DBG_CALL(Dbg_file_fixname(name, MSG_ORIG(MSG_PTH_LIBSYS)));
		return (strdup(MSG_ORIG(MSG_PTH_LIBSYS)));
	}

	/*
	 * Perform any token expansion of the NEEDED string.
	 */
	_len = strlen(name);
	_name = (char *)name;
	if ((_exp = expand(&_name, &_len, 0, lmp)) == 0)
		return ((char *)0);

	/*
	 * Use of $ORIGIN within a secure application isn't allowed.
	 */
	if ((_exp == 2) && (rtld_flags & RT_FL_SECURE)) {
		if (LIST(lmp)->lm_flags & LML_TRC_ENABLE) {
			if (LIST(lmp)->lm_flags &
			    (LML_TRC_VERBOSE | LML_TRC_SEARCH))
				(void) printf(MSG_INTL(MSG_LDD_FIL_FIND),
				    _name, NAME(lmp));
			(void) printf(MSG_INTL(MSG_LDD_FIL_ILLEGAL), _name);
		} else
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), name,
			    strerror(EACCES));

		free((void *)_name);
		return ((char *)0);
	}
	return (_name);
}

/*
 * Determine if we have been given an ELF file.  Returns 1 if true.
 */
static int
elf_are_u(void)
{
	PRF_MCOUNT(30, elf_are_u);
	if (fmap->fm_fsize < sizeof (Ehdr) ||
	    fmap->fm_maddr[EI_MAG0] != ELFMAG0 ||
	    fmap->fm_maddr[EI_MAG1] != ELFMAG1 ||
	    fmap->fm_maddr[EI_MAG2] != ELFMAG2 ||
	    fmap->fm_maddr[EI_MAG3] != ELFMAG3) {
		return (0);
	}
	return (1);
}

/*
 * The runtime linker employs lazyloading to provide the libraries needed for
 * debugging, preloading .o's and dldump().  As these are seldom used, the
 * standard startup of ld.so.1 doesn't initialize all the information necessary
 * to perform plt relocation on ld.so.1's link-map.  The first time lazy loading
 * is called we get here to perform these initializations:
 *
 *  o	elf_needed() is called to set up the DYNINFO() indexes for each lazy
 *	dependency.  Typically, for all other objects, this is called during
 *	analyze_so(), but as ld.so.1 is set-contained we skip this processing.
 *
 *  o	For intel ld.so.1's JMPSLOT relocations need relative updates. These
 *	are by default skipped thus delaying over 60 relative relocations on
 *	every invokation of ld.so.1.
 */
int
elf_rtld_load(Rt_map * rlmp)
{
	if (LIST(rlmp)->lm_flags & LML_FLG_PLTREL)
		return (1);

	/*
	 * As we need to refer to the DYNINFO() information, insure that it has
	 * been initialized.
	 */
	if (elf_needed(rlmp) == 0)
		return (0);

#if	defined(i386) || defined(__ia64)
	/*
	 * This is a cludge to give ld.so.1 a performance benefit on i386 and
	 * aiA64.  It's based around two factors.
	 *
	 *  o	JMPSLOT relocations (PLT's) actually need a relative
	 *	relocation applied to the GOT entry so that they can find PLT0.
	 *
	 *   o	ld.so.1 does not exersize *any* PLT's before it has made a call
	 *	to elf_lazy_load().  This is because any dynamic dependencies
	 * 	are recorded as lazy dependencies.
	 */
	(void) elf_reloc_relacount((ulong_t)JMPREL(rlmp),
	    (ulong_t)(PLTRELSZ(rlmp) / RELENT(rlmp)),
	    (ulong_t)RELENT(rlmp), (ulong_t)ADDR(rlmp));
#endif

	LIST(rlmp)->lm_flags |= LML_FLG_PLTREL;
	return (1);
}

/*
 * Lazy load an object.
 */
Rt_map *
elf_lazy_load(Rt_map * clmp, uint_t ndx, const char * sym)
{
	Rt_map *	nlmp;
	Dyninfo *	dip = &DYNINFO(clmp)[ndx];
	int		flags = 0;

	if ((nlmp = dip->di_lmp) == 0) {
		char *	name = (char *)STRTAB(clmp) + dip->di_dyn->d_un.d_val;

		DBG_CALL(Dbg_file_lazyload(name, NAME(clmp), sym));

		if ((name = elf_fix_name(name, clmp)) == 0)
			return (0);

		if (dip->di_flags & FLG_DI_GROUP)
			flags |= (FLG_RT_SETGROUP | FLG_RT_HANDLE);

		nlmp = load_one(LIST(clmp), name, clmp, MODE(clmp), flags);
		free(name);

		if (((dip->di_lmp = nlmp) == 0) ||
		    (bound_add(REF_DIRECT, clmp, nlmp) == 0) ||
		    (analyze_so(nlmp) == 0) || (relocate_so(nlmp) == 0))
			return (0);

		/*
		 * Reduce the pending lazy dependency count of the caller,
		 * together with the link-map lists count of objects that still
		 * have lazy dependencies pending.
		 */
		if (--LAZY(clmp) == 0)
			LIST(clmp)->lm_lazy--;
	}
	return (nlmp);
}


/*
 * Return the entry point of the ELF executable.
 */
static unsigned long
elf_entry_pt(void)
{
	PRF_MCOUNT(31, elf_entry_pt);
	return (ENTRY(lml_main.lm_head));
}

/*
 * Unmap a given ELF shared object from the address space.
 */
static int
elf_unmap_so(Rt_map * lmp)
{
	Phdr *	phdr;
	int	cnt, first;
	Xword	addr, msize;

	PRF_MCOUNT(32, elf_unmap_so);

	/*
	 * If this link map represents a relocatable object concatenation then
	 * the image was simply generated in allocated memory.  Free the memory.
	 *
	 * Note: the memory was originally allocated in the libelf:_elf_outmap
	 *	 routine and would have been free in elf_outsync(), but
	 *	 but because we 'interpose' on that routine the memory
	 *	 wasn't free'd at that time.
	 */
	if (FLAGS(lmp) & FLG_RT_IMGALLOC) {
		free((void *)ADDR(lmp));
		return (1);
	}

	/*
	 * Otherwise this object is an mmap()'ed image. Unmap each segment.
	 * Determine the first loadable program header.  Once we've unmapped
	 * this segment we're done.
	 */
	phdr = PHDR(lmp);
	for (first = 0; first < (int)(PHNUM(lmp));
	    phdr = (Phdr *)((unsigned long)phdr + PHSZ(lmp)), first++)
		if (phdr->p_type == PT_LOAD)
			break;

	/*
	 * Segments are unmapped in reverse order as the program headers are
	 * part of the first segment (page).
	 */
	phdr = (Phdr *)((unsigned long)PHDR(lmp) +
		((PHNUM(lmp) - 1) * (PHSZ(lmp))));
	for (cnt = (int)PHNUM(lmp); cnt > first; cnt--) {
		if (phdr->p_type == PT_LOAD) {
			addr = phdr->p_vaddr + ADDR(lmp);
			msize = phdr->p_memsz + (addr - M_PTRUNC(addr));
			(void) munmap((caddr_t)M_PTRUNC(addr), msize);
		}
		phdr = (Phdr *)((unsigned long)phdr - PHSZ(lmp));
	}
	return (1);
}

/*
 * Determine if a dependency requires a particular version and if so verify
 * that the version exists in the dependency.
 */
static int
elf_verify_vers(const char * name, Rt_map * clmp, Rt_map * nlmp)
{
	Verneed *	vnd = VERNEED(clmp);
	int		_num, num = VERNEEDNUM(clmp);
	char *		cstrs = (char *)STRTAB(clmp);
	Lm_list *	lml = LIST(clmp);

	PRF_MCOUNT(64, elf_verify_vers);

	/*
	 * Traverse the callers version needed information and determine if any
	 * specific versions are required from the dependency.
	 */
	for (_num = 1; _num <= num; _num++,
	    vnd = (Verneed *)((Xword)vnd + vnd->vn_next)) {
		Half		cnt = vnd->vn_cnt;
		Vernaux *	vnap;
		char *		nstrs, * need;

		/*
		 * Determine if a needed entry matches this dependency.
		 */
		need = (char *)(cstrs + vnd->vn_file);
		if (strcmp(name, need) != 0)
			continue;

		DBG_CALL(Dbg_ver_need_title(NAME(clmp)));
		if (lml->lm_flags & LML_TRC_VERBOSE)
			(void) printf(MSG_INTL(MSG_LDD_VER_FIND), name);

		/*
		 * Validate that each version required actually exists in the
		 * dependency.
		 */
		nstrs = (char *)STRTAB(nlmp);

		for (vnap = (Vernaux *)((Xword)vnd + vnd->vn_aux); cnt;
		    cnt--, vnap = (Vernaux *)((Xword)vnap + vnap->vna_next)) {
			char *		version, * define;
			Verdef *	vdf = VERDEF(nlmp);
			unsigned long	_num, num = VERDEFNUM(nlmp);
			int		found = 0;

			version = (char *)(cstrs + vnap->vna_name);
			DBG_CALL(Dbg_ver_need_entry(0, need, version));

			for (_num = 1; _num <= num; _num++,
			    vdf = (Verdef *)((Xword)vdf + vdf->vd_next)) {
				Verdaux *	vdap;

				if (vnap->vna_hash != vdf->vd_hash)
					continue;

				vdap = (Verdaux *)((Xword)vdf + vdf->vd_aux);
				define = (char *)(nstrs + vdap->vda_name);
				if (strcmp(version, define) != 0)
					continue;

				found++;
				break;
			}

			/*
			 * If we're being traced print out any matched version
			 * when the verbose (-v) option is in effect.  Always
			 * print any unmatched versions.
			 */
			if (lml->lm_flags & LML_TRC_ENABLE) {
				if (found) {
				    if (!(lml->lm_flags & LML_TRC_VERBOSE))
					continue;

				    (void) printf(MSG_ORIG(MSG_LDD_VER_FOUND),
					need, version, NAME(nlmp));
				} else {
				    (void) printf(MSG_INTL(MSG_LDD_VER_NFOUND),
					need, version);
				}
				continue;
			}

			/*
			 * If the version hasn't been found then this is a
			 * candidiate for a fatal error condition.  Weak
			 * version definition requirements are silently
			 * ignored.  Also, if the image inspected for a version
			 * definition has no versioning recorded at all then
			 * silently ignore this (this provides better backward
			 * compatibility to old images created prior to
			 * versioning being available).  Both of these skipped
			 * diagnostics are available under tracing (see above).
			 */
			if ((found == 0) && (num != 0) &&
			    (!(vnap->vna_flags & VER_FLG_WEAK))) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_VER_NFOUND),
				    need, version, NAME(clmp));
				return (0);
			}
		}
		return (1);
	}
	return (1);
}

/*
 * Search through the dynamic section for DT_NEEDED entries and perform one
 * of two functions.  If only the first argument is specified then load the
 * defined shared object, otherwise add the link map representing the defined
 * link map the the dlopen list.
 */
static int
elf_needed(Rt_map * clmp)
{
	Rt_map *	nlmp = 0;
	Dyn *		dyn;
	unsigned long	ndx = 0;
	int		lazy = 0, flags = 0;
	Word		lmflags = LIST(clmp)->lm_flags;
	char *		name;

	PRF_MCOUNT(33, elf_needed);
	/*
	 * Process each shared object on needed list.
	 */
	if (DYN(clmp) == 0)
		return (1);

	for (dyn = (Dyn *)DYN(clmp); dyn->d_tag != DT_NULL; dyn++, ndx++) {
		Dyninfo *	dip = &DYNINFO(clmp)[ndx];

		switch (dyn->d_tag) {
		case DT_POSFLAG_1:
			if ((dyn->d_un.d_val & DF_P1_LAZYLOAD) &&
			    !(lmflags & LML_FLG_NOLAZYLD))
				lazy = 1;
			if (dyn->d_un.d_val & DF_P1_GROUPPERM)
				flags = (FLG_RT_SETGROUP | FLG_RT_HANDLE);
			continue;
		case DT_NEEDED:
		case DT_USED:
			dip->di_dyn = dyn;
			if (flags)
				dip->di_flags |= FLG_DI_GROUP;

			/*
			 * Don't bring in lazy loaded objects yet unless we've
			 * been asked to attempt to load all available objects
			 * (crle(1) sets LD_FLAGS=loadavail).  Even under
			 * RTLD_NOW we don't process this - RTLD_NOW will cause
			 * relocation processing which in turn might trigger
			 * lazy loading, but its possible that the object has a
			 * lazy loaded file with no bindings (i.e., it should
			 * never have been a dependency in the first place).
			 */
			if (lazy) {
				if ((lmflags & LML_FLG_LOADAVAIL) == 0) {
					LAZY(clmp)++;
					lazy = flags = 0;
					continue;
				} else
					flags |= FLG_RT_NOERROR;
			}
			break;
		default:
			lazy = flags = 0;
			continue;
		}

		name = (char *)STRTAB(clmp) + dyn->d_un.d_val;
		DBG_CALL(Dbg_file_needed(name, NAME(clmp)));

		if ((name = elf_fix_name(name, clmp)) == 0) {
			if (lmflags & LML_TRC_ENABLE) {
				lazy = flags = 0;
				continue;
			} else
				return (0);
		}

		nlmp = load_one(LIST(clmp), name, clmp, MODE(clmp), flags);
		free(name);
		if ((dip->di_lmp = nlmp) == 0) {
			if ((flags & FLG_RT_NOERROR) ||
			    (lmflags & LML_TRC_ENABLE)) {
				lazy = flags = 0;
				continue;
			} else
				return (0);

		} else if (bound_add(REF_NEEDED, clmp, nlmp) == 0) {
			if (lmflags & LML_TRC_ENABLE) {
				lazy = flags = 0;
				continue;
			} else
				return (0);
		}
	}
	if (LAZY(clmp))
		LIST(clmp)->lm_lazy++;

	if (nlmp)
		DBG_CALL(Dbg_file_bind_needed(clmp));

	return (1);
}

/*
 * Compute the elf hash value (as defined in the ELF access library).
 * The form of the hash table is:
 *
 *	|--------------|
 *	| # of buckets |
 *	|--------------|
 *	| # of chains  |
 *	|--------------|
 *	|   bucket[]   |
 *	|--------------|
 *	|   chain[]    |
 *	|--------------|
 */
unsigned long
elf_hash(const char * ename)
{
	unsigned int	hval = 0;

	PRF_MCOUNT(34, elf_hash);

	while (*ename) {
		unsigned int	g;
		hval = (hval << 4) + *ename++;
		if ((g = (hval & 0xf0000000)) != 0)
			hval ^= g >> 24;
		hval &= ~g;
	}
	return ((unsigned long)hval);
}


/*
 * If flag argument has LKUP_SPEC set, we treat undefined symbols of type
 * function specially in the executable - if they have a value, even though
 * undefined, we use that value.  This allows us to associate all references
 * to a function's address to a single place in the process: the plt entry
 * for that function in the executable.  Calls to lookup from plt binding
 * routines do NOT set LKUP_SPEC in the flag.
 */
Sym *
elf_find_sym(const char * ename, Rt_map * lmp, Rt_map ** dlmp,
	int flag, unsigned long hash)
{
	unsigned int	ndx, htmp, buckets;
	Sym *		sym, * symtabptr;
	char *		strtabptr, * name;
	unsigned int *	chainptr;

	PRF_MCOUNT(35, elf_find_sym);

	DBG_CALL(Dbg_syms_lookup(ename, NAME(lmp), MSG_ORIG(MSG_STR_ELF)));


	if (HASH(lmp) == 0)
		return ((Sym *)0);

	buckets = HASH(lmp)[0];
	/* LINTED */
	htmp = (unsigned int)hash % buckets;

	/*
	 * Get the first symbol on hash chain and initialize the string
	 * and symbol table pointers.
	 */
	ndx = HASH(lmp)[htmp + 2];
	chainptr = HASH(lmp) + 2 + buckets;
	strtabptr = STRTAB(lmp);
	symtabptr = SYMTAB(lmp);

	while (ndx) {
		Syminfo *	sip;
		sym = symtabptr + ndx;
		name = strtabptr + sym->st_name;

		/*
		 * Compare the symbol found with the name required.  If the
		 * names don't match continue with the next hash entry.
		 */
		if ((*name++ != *ename) || strcmp(name, &ename[1])) {
			ndx = chainptr[ndx];
			continue;
		}

		/*
		 * If we're only interested in undefined symbols and this is
		 * undefined we're done.
		 */
		if (flag & LKUP_UNDEF) {
			if (sym->st_shndx == SHN_UNDEF) {
				*dlmp = lmp;
				return (sym);
			} else
				return ((Sym *)0);
		}

		/*
		 * If we find a match and the symbol is defined, return the
		 * symbol pointer and the link map in which it was found.
		 */
		if (sym->st_shndx != SHN_UNDEF) {
			/*
			 * If this is a 'DIRECT PASSTHRU' symbol, then the
			 * symbol to return is the DIRECT binding.
			 */
			/* LINTED */
			if ((sip = SYMINFO(lmp)) && DYNINFO(lmp)) {

				sip = (Syminfo *)((unsigned long)sip +
				    ndx * SYMINENT(lmp));

				if ((sip->si_flags & SYMINFO_FLG_DIRECT) &&
				    (sip->si_flags & SYMINFO_FLG_PASSTHRU)) {
					Rt_map *	_dlmp;

					if ((_dlmp = elf_lazy_load(lmp,
					    sip->si_boundto, ename)) != 0)
						return (elf_find_sym(ename,
						    _dlmp, dlmp, flag, hash));
					else
						return (0);
				}
			}
			*dlmp = lmp;
			return (sym);

		/*
		 * If we find a match and the symbol is undefined, the
		 * symbol type is a function, and the value of the symbol
		 * is non zero, then this is a special case.  This allows
		 * the resolution of a function address to the plt[] entry.
		 * See SPARC ABI, Dynamic Linking, Function Addresses for
		 * more details.
		 */
		} else if ((flag & LKUP_SPEC) &&
		    (FLAGS(lmp) & FLG_RT_ISMAIN) && (sym->st_value != 0) &&
		    (ELF_ST_TYPE(sym->st_info) == STT_FUNC)) {
			*dlmp = lmp;
			return (sym);
		}

		/*
		 * Local or undefined symbol.
		 */
		break;
	}

	/*
	 * If here, then no match was found.
	 */
	return ((Sym *)0);
}

static int
elf_map_check(const char * name, caddr_t vaddr, Off size)
{
	prmap_t *	maps, * _maps;
	int		pfd, num, _num;
	caddr_t		eaddr = vaddr + size;
	int		err;

	PRF_MCOUNT(83, elf_map_check);

	/*
	 * If memory reservations have been established for alternative objects
	 * determine if this object falls within the reservation, if it does no
	 * further checking is required.
	 */
	if (rtld_flags & RT_FL_MEMRESV) {
		Rtc_head *	head = (Rtc_head *)config->c_bgn;

		if ((vaddr >= (caddr_t)head->ch_resbgn) &&
		    (eaddr <= (caddr_t)head->ch_resend))
			return (0);
	}

	/*
	 * Determine the mappings presently in use by this process.
	 */
	if ((pfd = pr_open()) == FD_UNAVAIL)
		return (1);

	if (ioctl(pfd, PIOCNMAP, (void *)&num) == -1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_PROC), pr_name,
		    strerror(err));
		return (1);
	}

	if ((maps = malloc((num + 1) * sizeof (prmap_t))) == 0)
		return (1);

	if (ioctl(pfd, PIOCMAP, (void *)maps) == -1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_PROC), pr_name,
		    strerror(err));
		free(maps);
		return (1);
	}

	/*
	 * Determine if the supplied address clashes with any of the present
	 * process mappings.
	 */
	for (_num = 0, _maps = maps; _num < num; _num++, _maps++) {
		caddr_t		_eaddr = _maps->pr_vaddr + _maps->pr_size;
		Lm_list *	lml;
		Listnode *	lnp;

		if ((eaddr < _maps->pr_vaddr) || (vaddr >= _eaddr))
			continue;

		/*
		 * We have a memory clash.  See if one of the known dynamic
		 * dependency mappings represents this space so as to provide
		 * the user a more meaningfull message.
		 */
		for (LIST_TRAVERSE(&dynlm_list, lnp, lml)) {
			Rt_map *	tlmp;

			for (tlmp = lml->lm_head; tlmp;
			    tlmp = (Rt_map *)NEXT(tlmp)) {
				if ((eaddr < (caddr_t)ADDR(tlmp)) ||
				    (vaddr >= (caddr_t)(ADDR(tlmp) +
				    MSIZE(tlmp))))
					continue;

				eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_MAPINUSE),
				    name, EC_ADDR(vaddr), EC_OFF(size),
				    NAME(tlmp));
				return (1);
			}
		}
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_MAPINUSE), name,
		    EC_ADDR(vaddr), EC_OFF(size), MSG_INTL(MSG_STR_UNKNOWN));
		return (1);
	}
	free(maps);
	return (0);
}

static caddr_t
elf_map_it(
	const char *	name,		/* actual name stored for pathname */
	Off		mlen,		/* total mapping claim */
	Ehdr *		ehdr,		/* ELF header of file */
	Phdr **		phdr,		/* first Phdr in file */
	Phdr *		fph,		/* first loadable Phdr */
	Phdr *		lph,		/* last loadable Phdr */
	caddr_t *	paddress,	/* start of padding */
	Off *		plenth,		/* total mapping (including padding) */
	int		fixed)
{
	caddr_t		addr;		/* working mapping address */
	caddr_t		faddr;		/* first program mapping address */
	caddr_t		paddr;		/* pointer to begining of padded */
					/*	image */
	caddr_t		maddr;		/* pointer to mapping claim */
	caddr_t		zaddr;		/* /dev/zero working mapping addr */
	Off		foff;		/* file offset for segment mapping */
	Off		_flen;		/* file length for segment mapping */
	Off		_mlen;		/* memory length for segment mapping */
	Phdr *		pptr;		/* working Phdr */
	int		mperm;		/* initial mmap permissions and flags */
	size_t		plen = 0;	/* leading padding for image */
	size_t		paddsize = rtld_db_priv.rtd_objpad;
	int		mflag, err;

	PRF_MCOUNT(36, elf_map_it);

	/*
	 * The initial padding at the front of the image is rounded up to the
	 * nearest page, so that the actuall image will fall on a page boundry.
	 * Increase the image length by any padding that may be needed.
	 */
	if (paddsize) {
		plen = M_PROUND(paddsize);
		mlen += plen + paddsize;
	}

	/*
	 * Determine whether or not to let system reserve address space based on
	 * whether this is a dynamic executable (addresses in object are fixed)
	 * or a shared object (addresses in object are relative to the objects'
	 * base).  Determine amount of address space to be used.
	 */
	if (fixed) {
		maddr = (caddr_t)S_ALIGN(fph->p_vaddr, syspagsz);
		paddr = maddr - plen;
		mflag = MAP_PRIVATE | MAP_FIXED;
	} else {
		maddr = paddr = 0;
		mflag = MAP_PRIVATE;
	}

	/*
	 * If this image requires a fixed mapping insure that the location isn't
	 * already in use.
	 */
	if (fixed && lml_main.lm_head) {
		if (elf_map_check(name, maddr, mlen) != 0)
			return (0);
	}

	/*
	 * Determine the intial permisions used to map in the first segment.
	 */
	mperm = 0;
	if (fph->p_flags & PF_R)
		mperm |= PROT_READ;
	if (fph->p_flags & PF_X)
		mperm |= PROT_EXEC;
	if (fph->p_flags & PF_W)
		mperm |= PROT_WRITE;

	foff = S_ALIGN(fph->p_offset, syspagsz);

	/*
	 * If this is a fixed image map the first segment only.  Otherwise
	 * map enough address space to hold the program (as opposed to the
	 * file) represented by ld.so.  The amount to be assigned is the
	 * range between the end of the last loadable segment and the
	 * beginning of the first PLUS the alignment of the first segment.
	 * mmap() can assign us any page-aligned address, but the relocations
	 * assume the alignments included in the program header.  As an
	 * optimization, however, let's assume that mmap() will actually
	 * give us an aligned address -- since if it does, we can save
	 * an munmap() later on.  If it doesn't -- then go try it again.
	 */
	if (fixed)
		_mlen = fph->p_memsz + (fph->p_offset - foff);
	else
		_mlen = mlen;

	if ((paddr = (caddr_t)mmap(maddr, _mlen, mperm, mflag,
	    fmap->fm_fd, 0)) == (caddr_t)-1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
		    strerror(err));
		return (0);
	}
	maddr = paddr + plen;
	faddr = (caddr_t)S_ROUND((Off)maddr, fph->p_align);

	/*
	 * Check to see whether alignment skew was really needed.
	 */
	if ((fixed == 0) && (faddr != maddr)) {
		(void) munmap(paddr, mlen);
		paddr = fixed ?
		    (caddr_t)S_ALIGN(fph->p_vaddr, fph->p_align) - plen : 0;
		if ((paddr = (caddr_t)mmap(maddr, mlen + fph->p_align,
		    mperm, mflag, fmap->fm_fd, 0)) == (caddr_t)-1) {
			err = errno;
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
			    strerror(err));
			return (0);
		}
		faddr = maddr = (caddr_t)S_ROUND((Off)(paddr + plen),
		    fph->p_align);
		(void) munmap(paddr, faddr - paddr + plen);
		if ((paddr = (caddr_t)mmap(faddr - plen, mlen, mperm,
		    MAP_FIXED | MAP_PRIVATE, fmap->fm_fd, 0)) == (caddr_t)-1) {
			err = errno;
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
			    strerror(err));
			return (0);
		}
		maddr = paddr + plen;
	}

	/*
	 * Initialize the etext address if this initial segment has no write
	 * access.  Typically this is the only text segment, but its possible
	 * others may exist in which case fm_etext is updated in the segment
	 * loop that occurs later.
	 */
	if (!(fph->p_flags & PF_W))
		fmap->fm_etext = fph->p_vaddr + fph->p_memsz +
		    (unsigned long)(fixed ? 0 : faddr);

	/*
	 * These are passed back up to the calling routine for inclusion in the
	 * link-map which will eventually be created.
	 */
	*paddress = paddr;
	*plenth = mlen;

	/*
	 * If padding is enabled we've now reserved a hole large enough.  Now
	 * the image needs to be 'shifted' down plen bytes into the hole.
	 */
	if (plen) {
		if (dz_map(paddr, maddr - paddr, PROT_NONE,
		    MAP_PRIVATE | MAP_FIXED | MAP_NORESERVE) == (caddr_t)-1)
			return (0);

		if ((maddr = mmap(maddr, mlen - plen, mperm,
		    MAP_FIXED | MAP_PRIVATE, fmap->fm_fd, 0)) == (caddr_t)-1) {
			err = errno;
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
			    strerror(err));
			return (0);
		}
	}

	/*
	 * The first loadable segment is now pointed to by maddr, and since
	 * the first loadable segment contains the elf header and program
	 * headers.  Reset the program header (which will be saved in the
	 * objects link map, and may be used for later unmapping operations) to
	 * this mapping so that we can unmap the original first page on return.
	 */
	/* LINTED */
	*phdr = (Phdr *)((char *)maddr + ((Ehdr *)maddr)->e_ehsize);

	foff = S_ALIGN(fph->p_offset, syspagsz);
	_mlen = fph->p_memsz + (fph->p_offset - foff);
	maddr += M_PROUND(_mlen);
	mlen -= (plen + M_PROUND(_mlen));


	/*
	 * We have the address space reserved, so map each loadable segment.
	 */
	for (pptr = (Phdr *)((Off)fph + ehdr->e_phentsize);
	    pptr <= lph;
	    pptr = (Phdr *)((Off)pptr + ehdr->e_phentsize)) {

		/*
		 * Skip non-loadable segments or segments that don't occupy
		 * any memory.
		 */
		if ((pptr->p_type != PT_LOAD) || (pptr->p_memsz == 0)) {
			if (pptr->p_type == PT_SUNWBSS) {
				addr = (caddr_t)S_ALIGN((Off)(pptr->p_vaddr +
				    faddr), syspagsz);
				foff = S_ALIGN(pptr->p_offset, syspagsz);
				_mlen = pptr->p_memsz + (pptr->p_offset - foff);
				maddr = addr + M_PROUND(_mlen);
				mlen -= maddr - addr;
			}
			continue;
		}

		/*
		 * Set address of this segment relative to our base.
		 */
		addr = (caddr_t)S_ALIGN((Off)(pptr->p_vaddr +
		    (fixed ? 0 : faddr)), syspagsz);

		/*
		 * Determine the mapping protection from the section attributes.
		 * Also determine the etext address from the last loadable
		 * segment which has permissions but no write access.
		 */
		mperm = 0;
		if (pptr->p_flags) {
			if (pptr->p_flags & PF_R)
				mperm |= PROT_READ;
			if (pptr->p_flags & PF_X)
				mperm |= PROT_EXEC;
			if (pptr->p_flags & PF_W)
				mperm |= PROT_WRITE;
			else
				fmap->fm_etext = pptr->p_vaddr + pptr->p_memsz +
				    (unsigned long)(fixed ? 0 : faddr);
		}

		/*
		 * Determine the type of mapping required.
		 */
		if ((pptr->p_filesz == 0) && (pptr->p_flags == 0)) {
			/*
			 * If this segment has no backing file and no flags
			 * specified then it defines a reservation.  At this
			 * point all standard loadable segments will have been
			 * processed.  The segment reservation is mapped
			 * directly from /dev/null.
			 */
			if (nu_map((caddr_t)addr, pptr->p_memsz, PROT_NONE,
			    MAP_FIXED | MAP_PRIVATE) == (caddr_t)-1)
				return (0);

			_mlen = pptr->p_memsz;

		} else if (pptr->p_filesz == 0) {
			/*
			 * If this segment has no backing file then it defines a
			 * nobits segment and is mapped directly from /dev/zero.
			 */
			if (dz_map((caddr_t)addr, pptr->p_memsz, mperm,
			    MAP_FIXED | MAP_PRIVATE) == (caddr_t)-1)
				return (0);

			_mlen = pptr->p_memsz;

		} else {
			/*
			 * This mapping originates from the file.  Determine the
			 * file offset to which the mapping will be directed
			 * (must be aligned) and how much to map (might be more
			 * than the file in the case of .bss).
			 */
			foff = S_ALIGN(pptr->p_offset, syspagsz);
			_flen = pptr->p_filesz + (pptr->p_offset - foff);
			_mlen = pptr->p_memsz + (pptr->p_offset - foff);

			if ((caddr_t)mmap((caddr_t)addr, _flen, mperm,
			    MAP_FIXED | MAP_PRIVATE, fmap->fm_fd, foff) ==
			    (caddr_t)-1) {
				err = errno;
				eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
				    strerror(err));
				return (0);
			}

			/*
			 * If the memory occupancy of the segment overflows the
			 * definition in the file, we need to "zero out" the end
			 * of the mapping we've established, and if necessary,
			 * map some more space from /dev/zero.
			 */
			if (pptr->p_memsz > pptr->p_filesz) {
				long	zlen;

				foff = (Off) (pptr->p_vaddr + pptr->p_filesz +
				    (fixed ? 0 : faddr));
				zaddr = (caddr_t)M_PROUND(foff);
				zero((caddr_t)foff, (long)(zaddr - foff));
				zlen = (pptr->p_vaddr + pptr->p_memsz +
				    (fixed ? 0 : faddr)) - zaddr;
				if (zlen > 0) {
					if (dz_map(zaddr, zlen, mperm,
					    MAP_FIXED | MAP_PRIVATE) ==
					    (caddr_t)-1)
						return (0);
				}
			}
		}

		/*
		 * Unmap anything from the last mapping address to this one and
		 * update the mapping claim pointer.
		 */
		if ((fixed == 0) && (addr - maddr)) {
			(void) munmap(maddr, addr - maddr);
			mlen -= addr - maddr;
		}
		maddr = addr + M_PROUND(_mlen);
		mlen -= maddr - addr;
	}

	if (paddsize) {
		/*
		 * maddr is currently 'page aligned' because the original
		 * calculation for memsize rounded to syspagesize
		 */
		if (dz_map(maddr, paddsize, PROT_NONE,
		    MAP_PRIVATE | MAP_FIXED | MAP_NORESERVE) == (caddr_t)-1)
			return (0);

		mlen -= paddsize;
	}

	/*
	 * Unmap any final reservation.
	 */
	if ((fixed == 0) && (mlen != 0))
		(void) munmap(maddr, mlen);

	return (faddr);
}

/*
 * A null symbol interpretor.  Used if a filtee has no associated filtees.
 */
/* ARGSUSED0 */
static Sym *
elf_null_find_sym(const char * ename, Rt_map * lmp, Rt_map ** dlmp,
    int flag, unsigned long hash)
{
	return ((Sym *)0);
}


/*
 * Find symbol interpreter - filters.
 * This function is called when the symbols from a shared object should
 * be resolved from the shared objects filtees instead of from within ifself.
 *
 * A symbol name of 0 is used to trigger filtee loading.
 */
static Sym *
elf_intp_find_sym(const char * ename, Rt_map * clmp, Rt_map ** dlmp,
    int flag, unsigned long hash)
{
	Rt_map *	nlmp;
	Pnode *		pnp;
	Listnode *	lnp;
	Sym *		sym, * fsym;
	Dl_handle *	dlp;
	int		any = 0;

	PRF_MCOUNT(39, elf_intp_find_sym);

	/*
	 * Check that the symbol is actually defined in the filter.
	 */
	if (ename &&
	    ((fsym = LM_FIND_SYM(clmp)(ename, clmp, dlmp, flag, hash)) == 0))
		return ((Sym *)0);

	/*
	 * If this is the first call to process a filter establish the filtee
	 * list.  Any token expansion is also completed at this point
	 * (i.e., $PLATFORM).
	 */
	if (!(FILTEES(clmp))) {
		if ((FILTEES(clmp) = make_pnode_list(REFNAME(clmp),
		    0, 0, 0, clmp)) == 0) {

			REFNAME(clmp) = (char *)0;
			if (FLAGS(clmp) & FLG_RT_AUX) {
				SYMINTP(clmp) = elf_find_sym;
				return (fsym);
			} else {
				SYMINTP(clmp) = elf_null_find_sym;
				return ((Sym *)0);
			}
		}
	}

	/*
	 * Traverse the filtee list, dlopen()'ing any objects specfied and using
	 * their DLP() list to lookup the symbol.
	 */
	for (any = 0, pnp = FILTEES(clmp); pnp; pnp = pnp->p_next) {
		if (pnp->p_len == 0)
			continue;
		if (pnp->p_info == 0) {
			const char *	filter = pnp->p_name;

			DBG_CALL(Dbg_file_filter(filter, NAME(clmp)));

			/*
			 * Determine if the reference link map is already
			 * loaded.  As an optimization compare the filter with
			 * our interpretor.  The most common filter is
			 * libdl.so.1 (which is a filter on /usr/lib/ld.so.1).
			 */
			nlmp = lml_rtld.lm_head;
			if (strcmp(filter, NAME(nlmp)) == 0) {
				pnp->p_info = (void *)nlmp;
				dlp = hdl_create(&lml_rtld, nlmp, clmp, 0);
			} else {
				int	mode = MODE(clmp);
				int	flags = FLG_RT_HANDLE;

				/*
				 * Establish the mode of the filtee from the
				 * filter.  As this is a dlopen() make sure that
				 * RTLD_GROUP is set and the filtees aren't
				 * global.
				 */
				mode |= (RTLD_GROUP | RTLD_PARENT);
				mode &= ~RTLD_GLOBAL;

				/*
				 * Indicate whether an error message is required
				 * should this filtee not be found based on the
				 * type fo filter.
				 */
				if (FLAGS(clmp) & FLG_RT_AUX)
					flags |= FLG_RT_NOERROR;

				nlmp = pnp->p_info =
				    load_one(LIST(clmp), filter, clmp, mode,
					flags);

				if (nlmp) {
					if (analyze_so(nlmp) == 0)
						return (0);
					if (relocate_so(nlmp) == 0)
						return (0);
				}
			}

			if (nlmp) {
				if (bound_add(REF_SYMBOL, clmp, nlmp) == 0)
					return (0);
			} else {
				if ((LIST(clmp)->lm_flags & LML_TRC_ENABLE) &&
				    (FLAGS(clmp) & FLG_RT_AUX) &&
				    (rtld_flags & RT_FL_WARNFLTR)) {
				    (void) printf(MSG_INTL(MSG_LDD_FIL_NFOUND),
					filter);
				}
				DBG_CALL(Dbg_file_filter(filter, 0));
				pnp->p_len = 0;
				continue;
			}
		}

		/*
		 * If we're just here to trigger filtee loading skip the symbol
		 * lookup so we'll continue looking for additionaly filtees.
		 */
		if (ename != 0) {
			dlp = HANDLE((Rt_map *)pnp->p_info);
			any++;

			/*
			 * Look for the symbol in the handles dependencies.
			 */
			for (LIST_TRAVERSE(&dlp->dl_depends, lnp, nlmp)) {
				/*
				 * If our parent is also a dependency don't look
				 * any further (otherwise we're in a recursive
				 * loop).  This situation only makes sence for
				 * auxiliary filters, where the filtee wishes to
				 * bind to symbols within the filter.
				 */
				if (nlmp == clmp)
					break;
				if ((sym = SYMINTP(nlmp)(ename, nlmp, dlmp,
				    (flag | LKUP_FIRST), hash)) != (Sym *)0)
					return (sym);
			}
		}

		/*
		 * If this object is tagged to terminate filtee processing we're
		 * done.
		 */
		if (FLAGS1((Rt_map *)pnp->p_info) & FL1_RT_ENDFILTE)
			break;
	}

	/*
	 * If we're just here to trigger filtee loading then we're done.
	 */
	if (ename == 0)
		return ((Sym *)0);


	/*
	 * If no filtees have been found for a filter clean up any Pnode
	 * structures and disable their search completely.  For auxiliary
	 * filters we can reselect the symbol search function so that we never
	 * enter this routine again for this object.  For standard filters we
	 * use the null symbol routine.
	 */
	if (any == 0) {
		Pnode *	opnp = 0;

		for (pnp = FILTEES(clmp); pnp; opnp = pnp, pnp = pnp->p_next) {
			if (opnp)
				free((void *)opnp);
		}
		if (opnp)
			free((void *)opnp);

		FILTEES(clmp) = (Pnode *)0;
		REFNAME(clmp) = (char *)0;

		if (FLAGS(clmp) & FLG_RT_AUX) {
			SYMINTP(clmp) = elf_find_sym;
			return (fsym);
		} else {
			SYMINTP(clmp) = elf_null_find_sym;
			return ((Sym *)0);
		}
	}

	/*
	 * If this is a weak filter and the symbol cannot be resolved in the
	 * filtee, use the symbol from within the filter.
	 */
	if (FLAGS(clmp) & FLG_RT_AUX)
		return (fsym);
	else
		return ((Sym *)0);
}

/*
 * Create a new Rt_map structure for an ELF object and initialize
 * all values.
 */
static Rt_map *
elf_new_lm(Lm_list * lml, const char * pname, const char * oname, Dyn * ld,
    unsigned long addr, unsigned long etext, unsigned long msize,
    unsigned long entry, Phdr * phdr, unsigned int phnum, unsigned int phsize,
    unsigned long paddr, unsigned long padimsize)
{
	Rt_map *	lmp;
	unsigned long	base, offset, fltr = 0, audit = 0, cfile = 0, crle = 0;
	Xword		rpath = 0;
	Ehdr *		ehdr = (Ehdr *)addr;
	Phdr *		p;
	int		i;

	PRF_MCOUNT(41, elf_new_lm);
	DBG_CALL(Dbg_file_elf(pname, (unsigned long)ld, addr, msize, entry,
	    (unsigned long)phdr, phnum, get_linkmap_id(lml)));

	/*
	 * Allocate space.
	 */
	if ((lmp = calloc(sizeof (Rt_map), 1)) == 0)
		return (0);
	if ((ELFPRV(lmp) = calloc(sizeof (Rt_elfp), 1)) == 0) {
		free(lmp);
		return (0);
	}
	if (oname) {
		if (list_append(&ALIAS(lmp), strdup(oname)) == 0) {
			free(ELFPRV(lmp));
			free(lmp);
			return (0);
		}
	}

	/*
	 * All fields not filled in were set to 0 by calloc.
	 */
	NAME(lmp) = (char *)pname;
	PATHNAME(lmp) = (char *)pname;
	DYN(lmp) = ld;
	ADDR(lmp) = addr;
	MSIZE(lmp) = msize;
	ENTRY(lmp) = (Addr)entry;
	PHDR(lmp) = (void *)phdr;
	/* LINTED */
	PHNUM(lmp) = (Half)phnum;
	/* LINTED */
	PHSZ(lmp) = (Half)phsize;
	SYMINTP(lmp) = elf_find_sym;
	ETEXT(lmp) = etext;
	FCT(lmp) = &elf_fct;
	LIST(lmp) = lml;
	PADSTART(lmp) = paddr;
	PADIMLEN(lmp) = padimsize;

	/*
	 * If there is a writable segment, record it's starting address.  This
	 * is used later to distinguish a mapping that falls between the end of
	 * the text segment and the start of the data segment.
	 */
	if (ehdr->e_type == ET_EXEC)
		base = 0;
	else
		base = addr;

	if (phdr == 0)
		p = (Phdr *)(base + ehdr->e_phoff);
	else
		p = phdr;

	for (i = 0; i < ehdr->e_phnum; ++i)
		if ((p[i].p_type == PT_LOAD) && (p[i].p_flags & PF_W)) {
			SDATA(lmp) = S_ALIGN(base + p[i].p_vaddr, syspagsz);
			break;
		}

	/*
	 * Fill in rest of the link map entries with info the from the file's
	 * dynamic structure.  If shared object, add base address to each
	 * address; if executable, use address as is.
	 */
	if (ehdr->e_type == ET_EXEC) {
		offset = 0;
		FLAGS(lmp) |= FLG_RT_FIXED;
	} else
		offset = addr;

	/*
	 * Read dynamic structure into an array of ptrs to Dyn unions;
	 * array[i] is pointer to Dyn with tag == i.
	 */
	if (ld) {
		unsigned long	dyncnt = 0;
		Xword		pltpadsz = 0;
		/* CSTYLED */
		for ( ; ld->d_tag != DT_NULL; ++ld, dyncnt++) {
			switch ((Xword)ld->d_tag) {
			case DT_SYMTAB:
				SYMTAB(lmp) = (char *)ld->d_un.d_ptr + offset;
				break;
			case DT_STRTAB:
				STRTAB(lmp) = (char *)ld->d_un.d_ptr + offset;
				break;
			case DT_SYMENT:
				SYMENT(lmp) = ld->d_un.d_val;
				break;
			case DT_FEATURE_1:
				ld->d_un.d_val |= DTF_1_PARINIT;
				if (ld->d_un.d_val & DTF_1_CONFEXP)
					crle = 1;
				break;
			case DT_MOVESZ:
				MOVESZ(lmp) = ld->d_un.d_val;
				lmp->rt_flags |= FLG_RT_MOVE;
				break;
			case DT_MOVEENT:
				MOVEENT(lmp) = ld->d_un.d_val;
				break;
			case DT_MOVETAB:
				MOVETAB(lmp) = (char *)
					ld->d_un.d_ptr + offset;
				break;
			case DT_REL:
			case DT_RELA:
				/*
				 * At this time we can only handle 1 type of
				 * relocation per object.
				 */
				REL(lmp) = (char *)ld->d_un.d_ptr + offset;
				break;
			case DT_RELSZ:
			case DT_RELASZ:
				RELSZ(lmp) = ld->d_un.d_val;
				break;
			case DT_RELENT:
			case DT_RELAENT:
				RELENT(lmp) = ld->d_un.d_val;
				break;
			case DT_RELCOUNT:
			case DT_RELACOUNT:
				/* LINTED */
				RELACOUNT(lmp) = (Word)ld->d_un.d_val;
				break;
			case DT_HASH:
				HASH(lmp) = (unsigned int *)(ld->d_un.d_ptr +
					offset);
				break;
			case DT_PLTGOT:
				PLTGOT(lmp) = (unsigned int *)(ld->d_un.d_ptr +
					offset);
				break;
			case DT_PLTRELSZ:
				PLTRELSZ(lmp) = ld->d_un.d_val;
				break;
			case DT_JMPREL:
				JMPREL(lmp) = (char *)(ld->d_un.d_ptr) + offset;
				break;
			case DT_INIT:
				INIT(lmp) = (void (*)())((unsigned long)
					ld->d_un.d_ptr + offset);
				break;
			case DT_FINI:
				FINI(lmp) = (void (*)())((unsigned long)
					ld->d_un.d_ptr + offset);
				break;
			case DT_RPATH:
				rpath = ld->d_un.d_val;
				break;
			case DT_FILTER:
				fltr = ld->d_un.d_val;
				break;
			case DT_AUXILIARY:
				if (!(rtld_flags & RT_FL_NOAUXFLTR)) {
					fltr = ld->d_un.d_val;
					FLAGS(lmp) |= FLG_RT_AUX;
				}
				break;
			case DT_DEPAUDIT:
				if (!(rtld_flags & RT_FL_NOAUDIT))
					audit = ld->d_un.d_val;
				break;
			case DT_CONFIG:
				cfile = ld->d_un.d_val;
				break;
			case DT_DEBUG:
				/*
				 * DT_DEBUG entries are only created in
				 * dynamic objects that require an interpretor
				 * (ie. all dynamic executables and some shared
				 * objects), and provide for a hand-shake with
				 * debuggers.  This entry is initialized to
				 * zero by the link-editor.  If a debugger has
				 * us and updated this entry set the debugger
				 * flag, and finish initializing the debugging
				 * structure (see setup() also).  Switch off any
				 * configuration object use as most debuggers
				 * can't handle fixed dynamic executables as
				 * dependencies, and we can't handle requests
				 * like object padding for alternative objects.
				 */
				if (ld->d_un.d_ptr)
					rtld_flags |=
					    (RT_FL_DEBUGGER | RT_FL_NOOBJALT);
				ld->d_un.d_ptr = (Addr)&r_debug;
				break;
			case DT_VERNEED:
				VERNEED(lmp) = (Verneed *)((unsigned long)
				    ld->d_un.d_ptr + offset);
				break;
			case DT_VERNEEDNUM:
				/* LINTED */
				VERNEEDNUM(lmp) = (Word)ld->d_un.d_val;
				break;
			case DT_VERDEF:
				VERDEF(lmp) = (Verdef *)((unsigned long)
				    ld->d_un.d_ptr + offset);
				break;
			case DT_VERDEFNUM:
				/* LINTED */
				VERDEFNUM(lmp) = (Word)ld->d_un.d_val;
				break;
			case DT_FLAGS_1:
				if (ld->d_un.d_val & DF_1_GROUP)
					FLAGS(lmp) |=
					    (FLG_RT_SETGROUP | FLG_RT_HANDLE);
				if (ld->d_un.d_val & DF_1_NOW) {
					MODE(lmp) |= RTLD_NOW;
					MODE(lmp) &= ~RTLD_LAZY;
				}
				if (ld->d_un.d_val & DF_1_NODELETE)
					MODE(lmp) |= RTLD_NODELETE;
				if (ld->d_un.d_val & DF_1_INITFIRST)
					FLAGS(lmp) |= FLG_RT_INITFRST;
				if (ld->d_un.d_val & DF_1_NOOPEN)
					FLAGS(lmp) |= FLG_RT_NOOPEN;
				if (ld->d_un.d_val & DF_1_LOADFLTR)
					FLAGS(lmp) |= FLG_RT_LOADFLTR;
				if (ld->d_un.d_val & DF_1_ORIGIN)
					FLAGS(lmp) |= FLG_RT_ORIGIN;
				if (ld->d_un.d_val & DF_1_INTERPOSE)
					FLAGS(lmp) |= FLG_RT_INTRPOSE;
				if (ld->d_un.d_val & DF_1_NODUMP)
					FLAGS(lmp) |= FLG_RT_NODUMP;
				if (ld->d_un.d_val & DF_1_CONFALT)
					crle = 1;
				if ((ld->d_un.d_val & DF_1_DIRECT) &&
				    (!(LIST(lmp)->lm_flags & LML_FLG_NODIRECT)))
					FLAGS(lmp) |= FLG_RT_DIRECT;

				if (ld->d_un.d_val & DF_1_NODEFLIB)
					FLAGS1(lmp) |= FL1_RT_NODEFLIB;
				if (ld->d_un.d_val & DF_1_ENDFILTEE)
					FLAGS1(lmp) |= FL1_RT_ENDFILTE;
				break;
			case DT_SYMINFO:
				/*
				 * If both direct bindings & lazyloading
				 * are disabled then don't save the SYMINFO()
				 * table.  It will not be used.
				 */
				if ((LIST(lmp)->lm_flags &
				    (LML_FLG_NODIRECT | LML_FLG_NOLAZYLD)) !=
				    (LML_FLG_NODIRECT | LML_FLG_NOLAZYLD))
					SYMINFO(lmp) = (Syminfo *)
						((unsigned long)
						ld->d_un.d_ptr + offset);
				break;
			case DT_SYMINENT:
				SYMINENT(lmp) = ld->d_un.d_val;
				break;
			case DT_PLTPAD:
				PLTPAD(lmp) = (void *)ld->d_un.d_ptr;
				break;
			case DT_PLTPADSZ:
				pltpadsz = ld->d_un.d_val;
				break;
			case DT_DEPRECATED_SPARC_REGISTER:
			case M_DT_REGISTER:
				FLAGS(lmp) |= FLG_RT_REGSYMS;
				break;
			case M_DT_PLTRESERVE:
				PLTRESERVE(lmp) = (void *)(ld->d_un.d_ptr +
					offset);
				break;
			}
		}


		if (PLTPAD(lmp)) {
			if (pltpadsz == (Xword)0) {
				/*
				 * Must have accompanying PLTPADSZ entry
				 */
				PLTPAD(lmp) = 0;
			} else {
				if ((FLAGS(lmp) & FLG_RT_FIXED) == 0)
					PLTPAD(lmp) = (void *)(
					    (Addr)PLTPAD(lmp) +
					    ADDR(lmp));
				PLTPADEND(lmp) = (void *)((Addr)PLTPAD(lmp) +
					pltpadsz);
			}
		}

		/*
		 * Allocate Dynamic Info structure
		 */
		if ((DYNINFO(lmp) = calloc(dyncnt, sizeof (Dyninfo))) == 0) {
			remove_so(0, lmp);
			return (0);
		}
		/* LINTED */
		DYNINFOCNT(lmp) = (Word)dyncnt;
	}

	/*
	 * If configuration file use hasn't been disabled, and a configuration
	 * file hasn't already been set via an environment variable, see if any
	 * application specific configuration file is specified.  An LD_CONFIG
	 * setting is used first, but if this image was generated via crle(1)
	 * then a default configuration file is a fall-back.
	 */
	if ((!(rtld_flags & RT_FL_NOCFG)) && (config->c_name == 0)) {
		if (cfile)
			config->c_name = (const char *)(cfile +
			    (char *)STRTAB(lmp));
		else if (crle) {
			rtld_flags |= RT_FL_CONFAPP;
			FLAGS(lmp) |= FLG_RT_ORIGIN;
		}
	}

	if (rpath)
		RPATH(lmp) = (char *)(rpath + (char *)STRTAB(lmp));
	if (fltr) {
		char *	cp;
		/*
		 * The refname is placed into malloc'ed memory so that it is
		 * available to debuggers (dbx) when examining the link-map
		 * inside of a core-file.  The problem is that core files
		 * have an optimization in that they do not dump read-only
		 * segments into the core image.  Since the ref-name is part
		 * of the .dynstr it's not dumped out.
		 */
		if ((cp = malloc(strlen(fltr +
		    (char *)STRTAB(lmp)) + 1)) == 0) {
			remove_so(0, lmp);
			return (0);
		}
		(void) strcpy(cp, fltr + (char *)STRTAB(lmp));
		REFNAME(lmp) = cp;
		SYMINTP(lmp) = elf_intp_find_sym;
	}

	if (rtld_flags & RT_FL_ORIGIN)
		FLAGS(lmp) |= FLG_RT_ORIGIN;

	if (PATHNAME(lmp) && (FLAGS(lmp) & FLG_RT_ORIGIN))
		(void) fullpath(lmp);

	/*
	 * For Intel ABI compatibility.  It's possible that a JMPREL can be
	 * specified without any other relocations (e.g. a dynamic executable
	 * normally only contains .plt relocations).  If this is the case then
	 * no REL, RELSZ or RELENT will have been created.  For us to be able
	 * to traverse the .plt relocations under LD_BIND_NOW we need to know
	 * the RELENT for these relocations.  Refer to elf_reloc() for more
	 * details.
	 */
	if (!RELENT(lmp) && JMPREL(lmp))
		RELENT(lmp) = sizeof (Rel);

	/*
	 * Establish any per-object auditing.  If we're establishing `main's
	 * link-map its too early to go searching for audit objects so just
	 * hold the object name for later (see setup()).
	 */
	if (audit) {
		char *	cp = audit + (char *)STRTAB(lmp);

		if (*cp) {
			if (((AUDITORS(lmp) =
			    calloc(1, sizeof (Audit_desc))) == 0) ||
			    ((AUDITORS(lmp)->ad_name = strdup(cp)) == 0)) {
				remove_so(0, lmp);
				return (0);
			}
			if (NAME(lmp)) {
				if (audit_setup(lmp, AUDITORS(lmp)) == 0) {
					remove_so(0, lmp);
					return (0);
				}
				FLAGS1(lmp) |= AUDITORS(lmp)->ad_flags;
				lml->lm_flags |= LML_FLG_LOCAUDIT;

			}
		}
	}

	/*
	 * Add the mapped object to the end of the link map list.
	 */
	if (lm_append(lml, lmp) == 0) {
		remove_so(0, lmp);
		return (0);
	}
	return (lmp);
}

/*
 * Map in an ELF object.
 * Takes an open file descriptor for the object to map and its pathname; returns
 * a pointer to a Rt_map structure for this object, or 0 on error.
 */
static Rt_map *
elf_map_so(Lm_list * lml, const char * pname, const char * oname)
{
	int		i; 		/* general temporary */
	Off		memsize = 0;	/* total memory size of pathname */
	Off		mentry;		/* entry point */
	Ehdr *		ehdr;		/* ELF header of ld.so */
	Phdr *		phdr;		/* first Phdr in file */
	Phdr *		phdr0;		/* Saved first Phdr in file */
	Phdr *		pptr;		/* working Phdr */
	Phdr *		fph = 0;	/* first loadable Phdr */
	Phdr *		lph;		/* last loadable Phdr */
	Phdr *		lfph = 0;	/* last loadable (filesz != 0) Phdr */
	Phdr *		lmph = 0;	/* last loadable (memsz != 0) Phdr */
	Phdr *		swph = 0;	/* Program header for SUNWBSS */
	Dyn *		mld = 0;	/* DYNAMIC structure for pathname */
	size_t		size;		/* size of elf and program headers */
	caddr_t		faddr;		/* mapping address of pathname */
	Rt_map *	lmp;		/* link map created */
	caddr_t		paddr;		/* start of padded image */
	Off		plen;		/* size of image including padding */
	const char *	name;
	Half		etype;
	int		fixed;

	PRF_MCOUNT(38, elf_map_so);

	/*
	 * Establish the objects name.
	 */
	if (pname == (char *)0)
		name = pr_name;
	else
		name = pname;

	/* LINTED */
	ehdr = (Ehdr *)fmap->fm_maddr;

	/*
	 * If this a relocatable object then special processing is required.
	 */
	if ((etype = ehdr->e_type) == ET_REL)
		return (elf_obj_file(lml, name));

	/*
	 * If this isn't a dynamic executable or shared object we can't process
	 * it.  If this is a dynamic executable then all addresses are fixed.
	 */
	if (etype == ET_EXEC)
		fixed = 1;
	else if (etype == ET_DYN)
		fixed = 0;
	else {
		eprintf(ERR_ELF, MSG_INTL(MSG_GEN_BADTYPE), name,
		    conv_etype_str(etype));
		return (0);
	}

	/*
	 * If our original mapped page was not large enough to hold all the
	 * program headers remap them.
	 */
	size = (size_t)((char *)ehdr->e_phoff +
		(ehdr->e_phnum * ehdr->e_phentsize));
	if (fmap->fm_fsize < size) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_CORTRUNC), name);
		return (0);
	}
	if (size > fmap->fm_msize) {
		(void) munmap((caddr_t)fmap->fm_maddr, fmap->fm_msize);
		if ((fmap->fm_maddr = (char *)mmap(0, size, PROT_READ,
		    MAP_PRIVATE, fmap->fm_fd, 0)) == (char *)-1) {
			int	err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
			    strerror(err));
			return (0);
		}
		fmap->fm_msize = size;
		/* LINTED */
		ehdr = (Ehdr *)fmap->fm_maddr;
	}
	/* LINTED */
	phdr0 = phdr = (Phdr *)((char *)ehdr + ehdr->e_ehsize);

	/*
	 * Get entry point.
	 */
	mentry = ehdr->e_entry;

	/*
	 * Point at program headers and perform some basic validation.
	 */
	for (i = 0, pptr = phdr; i < (int)ehdr->e_phnum; i++,
	    pptr = (Phdr *)((Off)pptr + ehdr->e_phentsize)) {
		if ((pptr->p_type == PT_LOAD) ||
		    (pptr->p_type == PT_SUNWBSS)) {
			if (fph == 0) {
				fph = pptr;
			/* LINTED argument lph is initialized in first pass */
			} else if (pptr->p_vaddr <= lph->p_vaddr) {
				eprintf(ERR_ELF, MSG_INTL(MSG_GEN_INVPRGHDR),
				    name);
				return (0);
			}

			lph = pptr;

			if (pptr->p_memsz)
				lmph = pptr;
			if (pptr->p_filesz)
				lfph = pptr;
			if (pptr->p_type == PT_SUNWBSS)
				swph = pptr;

		} else if (pptr->p_type == PT_DYNAMIC)
			mld = (Dyn *)(pptr->p_vaddr);
	}

	/*
	 * We'd better have at least one loadable segment, together with some
	 * specified file and memory size.
	 */
	if ((fph == 0) || (lmph == 0) || (lfph == 0)) {
		eprintf(ERR_ELF, MSG_INTL(MSG_GEN_NOLOADSEG), name);
		return (0);
	}

	/*
	 * Check that the files size accounts for the loadable sections
	 * we're going to map in (failure to do this may cause spurious
	 * bus errors if we're given a truncated file).
	 */
	if (fmap->fm_fsize < ((size_t)lfph->p_offset + lfph->p_filesz)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_CORTRUNC), name);
		return (0);
	}

	/*
	 * Memsize must be page rounded so that if we add object padding
	 * at the end it will start at the begining of a page.
	 */
	plen = memsize = M_PROUND((lmph->p_vaddr + lmph->p_memsz) -
	    S_ALIGN(fph->p_vaddr, syspagsz));

	/*
	 * Map the complete file if necessary.
	 */
	if (interp && (lml->lm_flags & LML_FLG_BASELM) &&
	    (strcmp(name, interp->i_name) == 0)) {
		/*
		 * If this is the interpreter then it has already been mapped
		 * and we have the address so don't map it again.  Note that
		 * the common occurance of a reference to the interpretor
		 * (libdl -> ld.so.1) will have been caught during filter
		 * initialization (see elf_intp_find_sym()).  However, some
		 * ELF implementations are known to record libc.so.1 as the
		 * interpretor, and thus this test catches this behavior.
		 */
		paddr = faddr = interp->i_faddr;
		/* LINTED */
		phdr0 = phdr = (Phdr *)((char *)faddr + ehdr->e_ehsize);

		for (i = 0, pptr = phdr; i < (int)ehdr->e_phnum; i++,
		    pptr = (Phdr *)((Off)pptr + ehdr->e_phentsize)) {
			if (pptr->p_type != PT_LOAD)
				continue;
			if (!(pptr->p_flags & PF_W)) {
				fmap->fm_etext = (unsigned long)pptr->p_vaddr +
				    (unsigned long)pptr->p_memsz +
				    (unsigned long)(fixed ? 0 : faddr);
			}
		}

	} else if ((fixed == 0) && (rtld_db_priv.rtd_objpad == 0) &&
	    (memsize <= fmap->fm_msize) && ((fph->p_flags & PF_W) == 0)) {
		/*
		 * If the mapping required has already been established from
		 * the initial page we don't need to do anything more.  Reset
		 * the fmap address so then any later files start a new fmap.
		 * This is really an optimization for filters, such as libdl.so,
		 * which should only require one page.
		 */
		paddr = faddr = fmap->fm_maddr;
		fmap->fm_mflags &= ~MAP_FIXED;
		fmap->fm_maddr = 0;
		/* LINTED */
		phdr0 = phdr = (Phdr *)((char *)faddr + ehdr->e_ehsize);

	} else {
		/*
		 * Map the file.
		 */
		if (!(faddr = elf_map_it(name, memsize, ehdr, &phdr,
		    fph, lph, &paddr, &plen, fixed)))
			return (0);
	}

	/*
	 * Calculate absolute base addresses and entry points.
	 */
	if (!fixed) {
		if (mld)
			/* LINTED */
			mld = (Dyn *)((Off)mld + faddr);
		mentry += (Off)faddr;
	}

	/*
	 * Create new link map structure for newly mapped shared object.
	 */
	if (!(lmp = elf_new_lm(lml, pname, oname, mld,
	    (unsigned long)faddr, fmap->fm_etext, memsize, mentry,
	    phdr, ehdr->e_phnum, ehdr->e_phentsize, (unsigned long)paddr,
	    plen))) {
		(void) munmap((caddr_t)faddr, memsize);
		return (0);
	}

	/*
	 * If this shared object contains a PT_SUNWBSS segment mark it here.
	 */
	if (swph) {
		lmp->rt_flags |= FLG_RT_SUNWBSS;
		SUNWBSS(lmp) = phdr + (swph - phdr0);
	}

	return (lmp);
}


/*
 * Function to correct protection settings.
 * Segments are all mapped initially with permissions as given in
 * the segment header, but we need to turn on write permissions
 * on a text segment if there are any relocations against that segment,
 * and them turn write permission back off again before returning control
 * to the program.  This function turns the permission on or off depending
 * on the value of the argument.
 */
int
elf_set_prot(Rt_map * lmp, int permission)
{
	int		i, prot;
	Phdr *		phdr;
	size_t		size;
	unsigned long	addr;

	PRF_MCOUNT(42, elf_set_prot);

	/*
	 * If this is an allocated image (ie. a relocatable object) we can't
	 * mprotect() anything.
	 */
	if (FLAGS(lmp) & FLG_RT_IMGALLOC)
		return (1);

	DBG_CALL(Dbg_file_prot(NAME(lmp), permission));

	phdr = (Phdr *)PHDR(lmp);

	/*
	 * Process all loadable segments.
	 */
	for (i = 0; i < (int)PHNUM(lmp); i++) {
		if ((phdr->p_type == PT_LOAD) &&
		    ((phdr->p_flags & PF_W) == 0)) {
			prot = PROT_READ | permission;
			if (phdr->p_flags & PF_X)
				prot |=  PROT_EXEC;
			addr = (unsigned long)phdr->p_vaddr +
			    ((FLAGS(lmp) & FLG_RT_FIXED) ? 0 : ADDR(lmp));
			size = phdr->p_memsz + (addr - M_PTRUNC(addr));
			if (mprotect((caddr_t)M_PTRUNC(addr), size, prot) ==
			    -1) {
				int	err = errno;
				eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MPROT),
				    NAME(lmp), strerror(err));
				return (0);
			}
		}
		phdr = (Phdr *)((unsigned long)phdr + PHSZ(lmp));
	}
	return (1);
}

/*
 * Build full pathname of shared object from given directory name and filename.
 */
static char *
elf_get_so(const char * dir, const char * file)
{
	static char	pname[PATH_MAX];

	PRF_MCOUNT(43, elf_get_so);
	(void) snprintf(pname, PATH_MAX, MSG_ORIG(MSG_FMT_PATH), dir, file);
	return (pname);
}

/*
 * The copy relocation is recorded in a copy structure which will be applied
 * after all other relocations are carried out.  This provides for copying data
 * that has not yet been relocated itself (ie. pointers in shared objects).
 * This structure also provides a means of binding RTLD_GROUP dependencies to
 * any copy relocations that have been taken from any group members.
 *
 * Perform copy relocations.  If the size of the .bss area available for the
 * copy information is not the same as the source of the data inform the user
 * if we're under ldd(1) control (this checking was only established in 5.3,
 * so by only issuing an error via ldd(1) we maintain the standard set by
 * previous releases).
 */
int
elf_copy_reloc(char * name, Sym * rsym, Rt_map * rlmp, void * radd, Sym * dsym,
    Rt_map * dlmp, const void * dadd)
{
	Rel_copy *	rcp;
	Lm_list *	lml = LIST(rlmp);

	PRF_MCOUNT(44, elf_copy_reloc);
	/*
	 * Allocate a copy entry structure to hold the copy data information.
	 * These structures will be called in setup().
	 */
	if ((rcp = malloc(sizeof (Rel_copy))) == 0) {
		if (!(lml->lm_flags & LML_TRC_WARN))
			return (0);
		else
			return (1);
	}

	rcp->r_name = name;
	rcp->r_rsym = rsym;		/* the new reference symbol and its */
	rcp->r_rlmp = rlmp;		/*	associated link-map */
	rcp->r_dsym = dsym;		/* the original definition */
	rcp->r_radd = radd;
	rcp->r_dadd = dadd;

	if (rsym->st_size > dsym->st_size)
		rcp->r_size = (size_t)dsym->st_size;
	else
		rcp->r_size = (size_t)rsym->st_size;

	if (list_append(&COPY(dlmp), rcp) == 0) {
		if (!(lml->lm_flags & LML_TRC_WARN))
			return (0);
		else
			return (1);
	}
	if (!(FLAGS(dlmp) & FLG_RT_COPYTOOK)) {
		if (list_append(&COPY(rlmp), &COPY(dlmp)) == 0) {
			if (!(lml->lm_flags & LML_TRC_WARN))
				return (0);
			else
				return (1);
		}
		FLAGS(dlmp) |= FLG_RT_COPYTOOK;
	}

	/*
	 * We can only copy as much data as the reference (dynamic executables)
	 * entry allows.  Determine the size from the reference symbol, and if
	 * we are tracing (ldd) warn the user if it differs from the copy
	 * definition.
	 */
	if ((lml->lm_flags & LML_TRC_WARN) &&
	    (rsym->st_size != dsym->st_size)) {
		(void) printf(MSG_INTL(MSG_LDD_CPY_SIZDIF), name, NAME(rlmp),
		    EC_XWORD(rsym->st_size), NAME(dlmp),
		    EC_XWORD(dsym->st_size));
		if (rsym->st_size > dsym->st_size)
			(void) printf(MSG_INTL(MSG_LDD_CPY_INSDATA),
			    NAME(dlmp));
		else
			(void) printf(MSG_INTL(MSG_LDD_CPY_DATRUNC),
			    NAME(rlmp));
	}

	DBG_CALL(Dbg_reloc_apply((Xword)radd, (Xword)rcp->r_size));
	return (1);
}

/*
 * Determine the symbol location of an address within a link-map.  Look for
 * the nearest symbol (whoes value is less than or equal to the required
 * address).  This is the object specific part of dladdr().
 */
static void
elf_dladdr(unsigned long addr, Rt_map * lmp, Dl_info * dlip)
{
	unsigned long	ndx, cnt, base, value, _value;
	Sym *		sym;
	const char *	str, * _name;

	PRF_MCOUNT(37, elf_dladdr);

	/*
	 * If we don't have a .hash table there are no symbols to look at.
	 */
	if (HASH(lmp) == 0)
		return;

	cnt = HASH(lmp)[1];
	str = STRTAB(lmp);
	sym = SYMTAB(lmp);

	if (FLAGS(lmp) & FLG_RT_FIXED)
		base = 0;
	else
		base = ADDR(lmp);

	for (_value = 0, sym++, ndx = 1; ndx < cnt; ndx++, sym++) {
		if (sym->st_shndx == SHN_UNDEF)
			continue;

		value = sym->st_value + base;
		if (value > addr)
			continue;
		if (value < _value)
			continue;

		_value = value;
		_name =  str + sym->st_name;

		/*
		 * Note, because we accept local and global symbols we could
		 * find a section symbol that matches the associated address,
		 * which means that the symbol name will be null.  In this
		 * case continue the search in case we can find a global
		 * symmol of the same value.
		 */
		if ((value == addr) &&
		    (ELF_ST_TYPE(sym->st_info) != STT_SECTION))
			break;
	}

	if (_value) {
		dlip->dli_sname = _name;
		dlip->dli_saddr = (void *)_value;
	}
}

/*
 * This routine is called upon to search for a symbol from the dependencies of
 * the initial link-map.  To maintain lazy loadings goal of reducing the number
 * of objects mapped, any symbol search is first carried out using the objects
 * that already exist in the process (either on a link-map list or Dl_handle).
 * If a symbol can't be found, and lazy dependencies are still pending, this
 * routine loads the dependencies in an attempt to locate the symbol.
 *
 * Only new objects are inspected as we will have already inspected presently
 * loaded objects before calling this routine.  However, a new object may not
 * be new - although the di_lmp might be zero, the object may have been mapped
 * as someone elses dependency.  Thus there's a possibility of some symbol
 * search duplication.
 */
Sym *
elf_lazy_find_sym(Slookup * sl, Rt_map ** _lmp, int flags)
{
	Sym *		sym = 0;
	Dyninfo *	dip;
	List		list = {0, 0};
	Listnode *	lnp, * _lnp;
	Rt_map *	lmp = sl->sl_imap;
	const char *	name = sl->sl_name;

	if (list_append(&list, lmp) == 0)
		return (0);
	FLAGS(lmp) |= FLG_RT_DLSYM;

	for (LIST_TRAVERSE(&list, lnp, lmp)) {
		int	cnt = 0;
		/*
		 * Loop through the DT_NEEDED entries examining each object for
		 * the symbol. If the symbol is not found the object is in turn
		 * added to the 'list', so that its DT_NEEDED entires may be
		 * examined.
		 */
		for (dip = DYNINFO(lmp); cnt < DYNINFOCNT(lmp); cnt++, dip++) {

			if (!(dip->di_dyn) || (dip->di_dyn->d_tag != DT_NEEDED))
				continue;

			if (dip->di_lmp)
				continue;

			/*
			 * If this entry defines a lazy dependency try loading
			 * it - if the file can't be loaded, consider this
			 * non-fatal and continue the search (lazy loaded
			 * dependencies need not exist and their loading should
			 * only be fatal if called from a relocation).
			 */
			if ((dip->di_lmp = elf_lazy_load(lmp, cnt, name)) == 0)
				continue;

			/*
			 * If this object isn't yet a part of the dynamic list
			 * then inspect it for the symbol.  If the symbol isn't
			 * found add the object to the dynamic list so that we
			 * can inspect its dependencies.
			 */
			if (FLAGS(dip->di_lmp) & FLG_RT_DLSYM)
				continue;

			sl->sl_imap = dip->di_lmp;
			if (sym = LM_LOOKUP_SYM(sl->sl_cmap)(sl, _lmp, flags))
				break;

			/*
			 * Some dlsym() operations are already traversing a
			 * link-map (dlopen(0)), and thus there's no need to
			 * build our own dynamic dependency list.
			 */
			if ((flags & LKUP_NODESCENT) == 0) {
				if (list_append(&list, dip->di_lmp) == 0)
					return (0);
				FLAGS(dip->di_lmp) |= FLG_RT_DLSYM;
			}
		}
		if (sym)
			break;
	}

	/*
	 * Cleanup the dynamic list we've built and turn off the link-map flag.
	 */
	_lnp = 0;
	for (LIST_TRAVERSE(&list, lnp, lmp)) {
		FLAGS(lmp) &= ~FLG_RT_DLSYM;
		if (_lnp)
			free(_lnp);
		_lnp = lnp;
	}
	if (_lnp)
		free(_lnp);

	return (sym);
}

/*
 * Determine if an ELF file is compatible.  Returns 0 if true, >0 for error.
 */
static int
elf_are_u_compatible(Word * what)
{
	Ehdr * ehdr;

	/*
	 * Check class and encoding.
	 */
	/* LINTED */
	ehdr = (Ehdr *)fmap->fm_maddr;
	if (ehdr->e_ident[EI_CLASS] != M_CLASS) {
		*what = (Word)ehdr->e_ident[EI_CLASS];
		return (DBG_IFL_ERR_CLASS);
	}

	if (ehdr->e_ident[EI_DATA] != M_DATA) {
		*what = (Word)ehdr->e_ident[EI_DATA];
		return (DBG_IFL_ERR_DATA);
	}

	if ((ehdr->e_type != ET_REL) && (ehdr->e_type != ET_EXEC) &&
	    (ehdr->e_type != ET_DYN)) {
		*what = (Word)ehdr->e_type;
		return (DBG_IFL_ERR_TYPE);
	}

	/*
	 * Check machine type and flags.
	 */
	if (ehdr->e_machine != M_MACH) {
		if (ehdr->e_machine != M_MACHPLUS) {
			*what = ehdr->e_machine;
			return (DBG_IFL_ERR_MACH);
		}
		if ((ehdr->e_flags & M_FLAGSPLUS) == 0) {
			*what = ehdr->e_flags;
			return (DBG_IFL_ERR_BADMATCH);
		}
		if ((ehdr->e_flags & ~flags) & M_FLAGSPLUS_MASK) {
			*what = ehdr->e_flags;
			return (DBG_IFL_ERR_FLAGS);
		}
#if	defined(__sparcv9)
	} else if (ehdr->e_flags & EF_SPARC_EXT_MASK) {
		/*
		 * Check vendor-specific extensions.
		 */
		if (ehdr->e_flags & EF_SPARC_HAL_R1) {
			*what = ehdr->e_flags;
			return (DBG_IFL_ERR_HALFLAG);
		}

		if ((ehdr->e_flags & EF_SPARC_SUN_US3) & ~flags) {
			*what = ehdr->e_flags;
			return (DBG_IFL_ERR_US3);
		}

		/*
		 * Generic check.
		 * All of our 64-bit SPARC's support the US1
		 * (UltraSPARC 1) instructions., so that bit
		 * isn't worth checking for explicitly.
		 */
		if ((ehdr->e_flags & EF_SPARC_EXT_MASK) & ~flags) {
			*what = ehdr->e_flags;
			return (DBG_IFL_ERR_FLAGS);
		}
#endif /* defined(__sparcv9) */
#if	defined(__ia64)
	} else if (ehdr->e_flags & (EF_IA_64_ARCH | EF_IA_64_ABI64)) {
		/*
		 * xxx IA64:
		 * 	The only currently legal value for EF_IA_64_ARCH
		 *	is '0' as defined by the ABI.  However the
		 *	current 'pre-FCS' intel compilers are using this
		 *	field to store there version #.
		 *
		 *	For now - I ignore this field.  When we get
		 *	production compilers it will need to be verified.
		 *
		 *	Per - Asit - 2/25/99
		 */
		/* a-ok */
		;
#endif /* defined(__ia64) */
	} else if (ehdr->e_flags != 0) {
		*what = ehdr->e_flags;
		return (DBG_IFL_ERR_FLAGS);
	}

	/*
	 * Verify ELF version.  ??? is this too restrictive ???
	 */
	if (ehdr->e_version > EV_CURRENT) {
		*what = ehdr->e_version;
		return (DBG_IFL_ERR_LIBVER);
	}

	return (DBG_IFL_ERR_NONE);
}
