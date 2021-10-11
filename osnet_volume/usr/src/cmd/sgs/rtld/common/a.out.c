/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)a.out.c	1.50	99/09/14 SMI"

/*
 * Object file dependent support for a.out format objects.
 */
#include	"_synonyms.h"

#include	<sys/mman.h>
#include	<unistd.h>
#include	<string.h>
#include	<limits.h>
#include	<stdio.h>
#include	<dlfcn.h>
#include	<errno.h>
#include	"_a.out.h"
#include	"cache_a.out.h"
#include	"msg.h"
#include	"_rtld.h"
#include	"profile.h"
#include	"debug.h"

/*
 * Directory search rules for a.out format objects.
 */
static int		aout_search_rules[] = {
	ENVDIRS,	RUNDIRS,	DEFAULT,	0
};

static Pnode		aout_dflt_dirs[] = {
	{ MSG_ORIG(MSG_PTH_USR4LIB),	MSG_PTH_USR4LIB_SIZE,
		LA_SER_DEFAULT,		0,	&aout_dflt_dirs[1] },
	{ MSG_ORIG(MSG_PTH_USRLIB),	MSG_PTH_USRLIB_SIZE,
		LA_SER_DEFAULT,		0,	&aout_dflt_dirs[2] },
	{ MSG_ORIG(MSG_PTH_USRLCLIB),	MSG_PTH_USRLCLIB_SIZE,
		LA_SER_DEFAULT,		0, 0 }
};

static Pnode		aout_secure_dirs[] = {
	{ MSG_ORIG(MSG_PTH_USR4LIB),	MSG_PTH_USR4LIB_SIZE,
		LA_SER_SECURE,		0,	&aout_secure_dirs[1] },
	{ MSG_ORIG(MSG_PTH_USRLIB),	MSG_PTH_USRLIB_SIZE,
		LA_SER_SECURE,		0,	&aout_secure_dirs[2] },
	{ MSG_ORIG(MSG_PTH_USRUCBLIB),	MSG_PTH_USRUCBLIB_SIZE,
		LA_SER_SECURE,		0,	&aout_secure_dirs[3] },
	{ MSG_ORIG(MSG_PTH_USRLCLIB),	MSG_PTH_USRLCLIB_SIZE,
		LA_SER_SECURE,		0, 0 }
};

/*
 * Defines for local functions.
 */
static int		aout_are_u();
static unsigned long	aout_entry_pt();
static Rt_map *		aout_map_so();
static Rt_map *		aout_new_lm();
static int		aout_unmap_so();
static int		aout_needed();
extern Sym *		aout_lookup_sym();
static Sym *		aout_find_sym();
static char *		aout_get_so();
static char *		aout_fix_name();
static void		aout_dladdr();
static Sym *		aout_dlsym_handle();
static int		aout_are_u_compatible();
static int		aout_verify_vers();

/*
 * Functions and data accessed through indirect pointers.
 */
Fct aout_fct = {
	aout_are_u,
	aout_entry_pt,
	aout_map_so,
	aout_new_lm,
	aout_unmap_so,
	aout_needed,
	aout_lookup_sym,
	aout_find_sym,
	aout_reloc,
	aout_search_rules,
	aout_dflt_dirs,
	aout_secure_dirs,
	aout_fix_name,
	aout_get_so,
	aout_dladdr,
	aout_dlsym_handle,
	aout_are_u_compatible,
	aout_verify_vers
};

/*
 * In 4.x, a needed file or a dlopened file that was a simple file name
 * implied that the file be found in the present working directory.  To
 * simulate this lookup within the elf rules it is necessary to add a
 * proceeding `./' to the filename.
 */
static char *
aout_fix_name(const char * name)
{
	char *	_name;		/* temporary name pointer */

	/*
	 * Check for slash in name, if none, prepend "./",
	 * otherwise just return name given.
	 */
	for (_name = (char *)name; *_name; _name++) {
		if (*_name == '/')
			return (strdup(name));
	}

	if ((_name = malloc(strlen(name) + 3)) == 0)
		return (0);
	(void) sprintf(_name, MSG_ORIG(MSG_FMT_4XPATH), name);
	DBG_CALL(Dbg_file_fixname(name, _name));
	return (_name);
}

/*
 * Determine if we have been given an A_OUT file.  Returns 1 if true.
 */
static int
aout_are_u()
{
	struct exec * exec;

	PRF_MCOUNT(6, aout_are_u);
	/* LINTED */
	exec = (struct exec *)fmap->fm_maddr;
	if (fmap->fm_fsize < sizeof (exec) || (exec->a_machtype != M_SPARC) ||
	    (N_BADMAG(*exec))) {
		return (0);
	}
	return (1);
}

/*
 * Return the entry point the A_OUT executable. This is always zero.
 */
static unsigned long
aout_entry_pt()
{
	PRF_MCOUNT(7, aout_entry_pt);
	return (0);
}

/*
 * Unmap a given A_OUT shared object from the address space.
 */
static int
aout_unmap_so(Rt_map * lm)
{
	caddr_t addr;

	PRF_MCOUNT(8, aout_unmap_so);
	addr = (caddr_t)ADDR(lm);
	/* LINTED */
	(void) munmap(addr, max(SIZE(*(struct exec *)addr),
	    N_SYMOFF((*(struct exec *)addr)) + sizeof (struct nlist)));
	return (1);
}

/*
 * Dummy versioning interface - real functionality is only applicable to elf.
 */
static int
aout_verify_vers()
{
	return (1);
}

/*
 * Search through the dynamic section for DT_NEEDED entries and perform one
 * of two functions.  If only the first argument is specified then load the
 * defined shared object, otherwise add the link map representing the
 * defined link map the the dlopen list.
 */
static int
aout_needed(Rt_map * clmp)
{
	Rt_map	*	nlmp = 0;
	void *		need;
	char *		name;

	PRF_MCOUNT(9, aout_needed);

	for (need = &TEXTBASE(clmp)[AOUTDYN(clmp)->v2->ld_need];
	    need != &TEXTBASE(clmp)[0];
	    need = &TEXTBASE(clmp)[((Lnk_obj *)(need))->lo_next]) {
		name = &TEXTBASE(clmp)[((Lnk_obj *)(need))->lo_name];

		if (((Lnk_obj *)(need))->lo_library) {
			/*
			 * If lo_library field is not NULL then this needed
			 * library was linked in using the "-l" option.
			 * Thus we need to rebuild the library name before
			 * trying to load it.
			 */
			Pnode *	dir, *dirlist = (Pnode *)0;
			char *	file;

			/*
			 * Allocate name length plus 20 for full library name.
			 * lib.so.. = 7 + (2 * short) + NULL = 7 + 12 + 1 = 20
			 */
			if ((file = malloc(strlen(name) + 20)) == 0)
				return (0);
			(void) sprintf(file, MSG_ORIG(MSG_FMT_4XLIB), name,
				((Lnk_obj *)(need))->lo_major,
				((Lnk_obj *)(need))->lo_minor);

			DBG_CALL(Dbg_libs_find(file));

			/*
			 * We need to determine what filename will match the
			 * the filename specified (ie, a libc.so.1.2 may match
			 * to a libc.so.1.3).  It's the real pathname that is
			 * recorded in the link maps.  If we are presently
			 * being traced, skip this pathname generation so
			 * that we fall through into load_so() to print the
			 * appropriate diagnostics.  I don't like this at all.
			 */
			if (LIST(clmp)->lm_flags & LML_TRC_ENABLE)
				name = strdup(file);
			else {
				const char *	path = (char *)0;
				for (dir = get_next_dir(&dirlist, clmp, 0); dir;
				    dir = get_next_dir(&dirlist, clmp, 0)) {
					if (dir->p_name == 0)
						continue;

					if (path =
					    aout_get_so(dir->p_name, file))
						break;
				}
				if (!path) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_SYS_OPEN), file,
						strerror(ENOENT));
					return (0);
				}
				name = strdup(path);
			}
		} else {
			/*
			 * If the library is specified as a pathname, see if
			 * it must be fixed to specify the current working
			 * directory (ie. libc.so.1.2 -> ./libc.so.1.2).
			 */
			if ((name = aout_fix_name(name)) == 0)
				return (0);
		}
		DBG_CALL(Dbg_file_needed(name, NAME(clmp)));

		nlmp = load_one(LIST(clmp), name, clmp, MODE(clmp), 0);
		free(name);
		if (((nlmp == 0) || (bound_add(REF_NEEDED, clmp, nlmp) == 0)) &&
		    ((LIST(clmp)->lm_flags & LML_TRC_ENABLE) == 0))
			return (0);
	}
	if (nlmp)
		DBG_CALL(Dbg_file_bind_needed(clmp));

	return (1);
}

static Sym *
aout_symconvert(struct nlist * sp)
{
	static Sym	sym;

	PRF_MCOUNT(11, aout_symconvert);

	sym.st_shndx = 0;
	sym.st_value = sp->n_value;
	switch (sp->n_type) {
		case N_EXT + N_ABS:
			sym.st_shndx = SHN_ABS;
			break;
		case N_COMM:
			sym.st_shndx = SHN_COMMON;
			break;
		case N_EXT + N_UNDF:
			sym.st_shndx = SHN_UNDEF;
			break;
		default:
			break;
	}
	return (&sym);
}

/*
 * Process a.out format commons.
 */
static struct nlist *
aout_find_com(struct nlist * sp, const char * name)
{
	static struct rtc_symb *	rtcp = 0;
	struct rtc_symb *		rs, * trs;
	const char *			sl;
	char *				cp;

	PRF_MCOUNT(12, aout_find_com);
	/*
	 * See if common is already allocated.
	 */
	trs = rtcp;
	while (trs) {
		sl = name;
		cp = trs->rtc_sp->n_un.n_name;
		while (*sl == *cp++)
			if (*sl++ == '\0')
				return (trs->rtc_sp);
		trs = trs->rtc_next;
	}

	/*
	 * If we got here, common is not already allocated so allocate it.
	 */
	if ((rs = malloc(sizeof (struct rtc_symb))) == 0)
		return (0);
	if ((rs->rtc_sp = malloc(sizeof (struct nlist))) == 0)
		return (0);
	trs = rtcp;
	rtcp = rs;
	rs->rtc_next = trs;
	*(rs->rtc_sp) = *sp;
	if ((rs->rtc_sp->n_un.n_name = malloc(strlen(name) + 1)) == 0)
		return (0);
	(void) strcpy(rs->rtc_sp->n_un.n_name, name);
	rs->rtc_sp->n_type = N_COMM;
	if ((rs->rtc_sp->n_value = (long)calloc(rs->rtc_sp->n_value, 1)) == 0)
		return (0);
	return (rs->rtc_sp);
}

/*
 * Find a.out format symbol in the specified link map.  Unlike the sister
 * elf routine we re-calculate the symbols hash value for each link map
 * we're looking at.
 */
static struct nlist *
aout_findsb(const char * aname, Rt_map * lmp, int flag)
{
	const char *	name = aname;
	char *		cp;
	struct fshash *	p;
	int		i;
	struct nlist *	sp;
	unsigned long	hval = 0;

	PRF_MCOUNT(13, aout_findsb);

#define	HASHMASK	0x7fffffff
#define	RTHS		126

	/*
	 * The name passed to us is in ELF format, thus it is necessary to
	 * map this back to the A_OUT format to compute the hash value (see
	 * mapping rules in aout_lookup_sym()).  Basically the symbols are
	 * mapped according to whether a leading `.' exists.
	 *
	 *	elf symbol		a.out symbol
	 * i.	   .bar		->	   .bar		(LKUP_LDOT)
	 * ii.	   .nuts	->	    nuts
	 * iii.	    foo		->	   _foo
	 */
	if (*name == '.') {
		if (!(flag & LKUP_LDOT))
			name++;
	} else
		hval = '_';

	while (*name)
		hval = (hval << 1) + *name++;
	hval = hval & HASHMASK;

	i = hval % (AOUTDYN(lmp)->v2->ld_buckets == 0 ? RTHS :
		AOUTDYN(lmp)->v2->ld_buckets);
	p = LM2LP(lmp)->lp_hash + i;

	if (p->fssymbno != -1)
		do {
			sp = &LM2LP(lmp)->lp_symtab[p->fssymbno];
			cp = &LM2LP(lmp)->lp_symstr[sp->n_un.n_strx];
			name = aname;
			if (*name == '.') {
				if (!(flag & LKUP_LDOT))
					name++;
			} else {
				cp++;
			}
			while (*name == *cp++) {
				if (*name++ == '\0')
					return (sp);	/* found */
			}
			if (p->next == 0)
				return (0);		/* not found */
			else
				continue;
		} while ((p = &LM2LP(lmp)->lp_hash[p->next]) != 0);
	return (0);
}

/*
 * The symbol name we have been asked to look up is in A_OUT format, this
 * symbol is mapped to the appropriate ELF format which is the standard by
 * which symbols are passed around ld.so.1.  The symbols are mapped
 * according to whether a leading `_' or `.' exists.
 *
 *	a.out symbol		elf symbol
 * i.	   _foo		->	    foo
 * ii.	   .bar		->	   .bar		(LKUP_LDOT)
 * iii.	    nuts	->	   .nuts
 */
Sym *
aout_lookup_sym(Slookup * slp, Rt_map ** dlmp, int flag)
{
	char	name[PATH_MAX];

	PRF_MCOUNT(14, aout_lookup_sym);
	DBG_CALL(Dbg_syms_lookup_aout(slp->sl_name));

	if (*slp->sl_name == '_')
		++slp->sl_name;
	else if (*slp->sl_name == '.')
		flag |= LKUP_LDOT;
	else {
		name[0] = '.';
		(void) strcpy(&name[1], slp->sl_name);
		slp->sl_name = name;
	}

	/*
	 * Call the generic lookup routine to cycle through the specified
	 * link maps.
	 */
	return (lookup_sym(slp, dlmp, flag));
}

/*
 * Symbol lookup for an a.out format module.
 */
/* ARGSUSED4 */
static Sym *
aout_find_sym(const char * aname, Rt_map * lmp, Rt_map ** dlmp, int flag,
	unsigned long hash)
{
	struct nlist *	sp;

	PRF_MCOUNT(15, aout_find_sym);
	DBG_CALL(Dbg_syms_lookup(aname, NAME(lmp), MSG_ORIG(MSG_STR_AOUT)));

	if (sp = aout_findsb(aname, lmp, flag)) {
		if (sp->n_value != 0) {
			/*
			 * is it a common?
			 */
			if (sp->n_type == (N_EXT + N_UNDF)) {
				if ((sp = aout_find_com(sp, aname)) == 0)
					return ((Sym *)0);
			}
			*dlmp = lmp;
			return (aout_symconvert(sp));
		}
	}
	return ((Sym *)0);
}

/*
 * Map in an a.out format object.
 * Takes an open file descriptor for the object to map and
 * its pathname; returns a pointer to a Rt_map structure
 * for this object, or 0 on error.
 */
static Rt_map *
aout_map_so(Lm_list * lml, const char * pname, const char * oname)
{
	struct exec *	exec;		/* working area for object headers */
	caddr_t		addr;		/* mmap result temporary */
	struct link_dynamic *ld;	/* dynamic pointer of object mapped */
	size_t		size;		/* size of object */
	const char *	name;		/* actual name stored for pathname */
	Rt_map *	lmp;		/* link map created */
	int		err;
	struct nlist *	nl;

	PRF_MCOUNT(16, aout_map_so);
	/*
	 * Is object the executable?
	 */
	if (pname == (char *)0)
		name = pr_name;
	else
		name = pname;

	/*
	 * Map text and allocate enough address space to fit the whole
	 * library.  Note that we map enough to catch the first symbol
	 * in the symbol table and thereby avoid an "lseek" & "read"
	 * pair to pick it up.
	 */
	/* LINTED */
	exec = (struct exec *)fmap->fm_maddr;
	size = max(SIZE(*exec), N_SYMOFF(*exec) + sizeof (struct nlist));
	if ((addr = (caddr_t)mmap(0, size, PROT_READ | PROT_EXEC, MAP_PRIVATE,
	    fmap->fm_fd, 0)) == (caddr_t)-1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
		    strerror(err));
		return (0);
	}

	/*
	 * Grab the first symbol entry while we've got it mapped aligned
	 * to file addresses.  We assume that this symbol describes the
	 * object's link_dynamic.
	 */
	/* LINTED */
	nl = (struct nlist *)&addr[N_SYMOFF(*exec)];
	/* LINTED */
	ld = (struct link_dynamic *)&addr[nl->n_value];

	/*
	 * Map the initialized data portion of the file to the correct
	 * point in the range of allocated addresses.  This will leave
	 * some portion of the data segment "doubly mapped" on machines
	 * where the text/data relocation alignment is not on a page
	 * boundaries.  However, leaving the file mapped has the double
	 * advantage of both saving the munmap system call and of leaving
	 * us a contiguous chunk of address space devoted to the object --
	 * in case we need to unmap it all later.
	 */
	if ((caddr_t)mmap((caddr_t)(addr + M_SROUND(exec->a_text)),
	    (int)exec->a_data,
	    PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE,
	    fmap->fm_fd, (off_t)exec->a_text) == (caddr_t)-1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), name,
		    strerror(err));
		return (0);
	}

	/*
	 * Allocate pages for the object's bss, if necessary.
	 */
	if (exec->a_bss != 0) {
		if (dz_map(addr + M_SROUND(exec->a_text) + exec->a_data,
		    (int)exec->a_bss, PROT_READ | PROT_WRITE | PROT_EXEC,
		    MAP_FIXED | MAP_PRIVATE) == (caddr_t)-1)
			goto error;
	}

	/*
	 * Create link map structure for newly mapped shared object.
	 */
	ld->v2 = (struct link_dynamic_2 *)((int)ld->v2 + (int)addr);
	if (!(lmp = aout_new_lm(lml, pname, oname, ld, addr, size)))
		goto error;

	return (lmp);

	/*
	 * Error returns: close off file and free address space.
	 */
error:
	(void) munmap((caddr_t)addr, size);
	return (0);
}

/*
 * Create a new Rt_map structure for an a.out format object and
 * initializes all values.
 */
static Rt_map *
aout_new_lm(Lm_list * lml, const char * pname, const char * oname,
	struct link_dynamic * ld, caddr_t addr, size_t size)
{
	Rt_map *	lmp;
	caddr_t 	offset;

	PRF_MCOUNT(17, aout_new_lm);
	DBG_CALL(Dbg_file_aout((pname ? pname : pr_name), (unsigned long)ld,
	    (unsigned long)addr, (unsigned long)size));

	/*
	 * Allocate space.
	 */
	if ((lmp = calloc(sizeof (Rt_map), 1)) == 0)
		return (0);
	if ((AOUTPRV(lmp) = calloc(sizeof (Rt_aoutp), 1)) == 0) {
		free(lmp);
		return (0);
	}
	if ((((Rt_aoutp *)AOUTPRV(lmp))->lm_lpd =
	    calloc(sizeof (struct ld_private), 1)) == 0) {
		free(AOUTPRV(lmp));
		free(lmp);
		return (0);
	}
	if (oname)
		if (list_append(&ALIAS(lmp), strdup(oname)) == 0) {
			free(((Rt_aoutp *)AOUTPRV(lmp))->lm_lpd);
			free(AOUTPRV(lmp));
			free(lmp);
			return (0);
		}

	/*
	 * All fields not filled in were set to 0 by calloc.
	 */
	NAME(lmp) = (char *)pname;
	PATHNAME(lmp) = (char *)pname;
	ADDR(lmp) = (unsigned long)addr;
	MSIZE(lmp) = (unsigned long)size;
	SYMINTP(lmp) = aout_find_sym;
	FCT(lmp) = &aout_fct;
	LIST(lmp) = lml;

	/*
	 * Specific settings for a.out format.
	 */
	if (pname == 0) {
		offset = (caddr_t)MAIN_BASE;
		FLAGS(lmp) |= FLG_RT_FIXED;
	} else
		offset = addr;

	ETEXT(lmp) = (unsigned long)&offset[ld->v2->ld_text];

	AOUTDYN(lmp) = ld;
	if ((RPATH(lmp) = (char *)&offset[ld->v2->ld_rules]) == offset)
		RPATH(lmp) = 0;
	LM2LP(lmp)->lp_symbol_base = addr;
	/* LINTED */
	LM2LP(lmp)->lp_plt = (struct jbind *)(&addr[JMPOFF(ld)]);
	LM2LP(lmp)->lp_rp =
	/* LINTED */
	    (struct relocation_info *)(&offset[RELOCOFF(ld)]);
	/* LINTED */
	LM2LP(lmp)->lp_hash = (struct fshash *)(&offset[HASHOFF(ld)]);
	/* LINTED */
	LM2LP(lmp)->lp_symtab = (struct nlist *)(&offset[SYMOFF(ld)]);
	LM2LP(lmp)->lp_symstr = &offset[STROFF(ld)];
	LM2LP(lmp)->lp_textbase = offset;
	LM2LP(lmp)->lp_refcnt++;
	LM2LP(lmp)->lp_dlp = NULL;

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
 * Function to correct protection settings.
 * Segments are all mapped initially with permissions as given in
 * the segment header, but we need to turn on write permissions
 * on a text segment if there are any relocations against that segment,
 * and them turn write permission back off again before returning control
 * to the program.  This function turns the permission on or off depending
 * on the value of the argument.
 */
int
aout_set_prot(Rt_map * lm, int permission)
{
	int		prot;		/* protection setting */
	caddr_t		et;		/* cached _etext of object */
	size_t		size;		/* size of text segment */

	PRF_MCOUNT(18, aout_set_prot);
	DBG_CALL(Dbg_file_prot(NAME(lm), permission));

	et = (caddr_t)ETEXT(lm);
	size = M_PROUND((unsigned long)(et - TEXTBASE(lm)));
	prot = PROT_READ | PROT_EXEC | permission;
	if (mprotect((caddr_t)TEXTBASE(lm), size, prot) == -1) {
		int	err = errno;

		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MPROT), NAME(lm),
		    strerror(err));
		return (0);
	}
	return (1);
}

/*
 * Build full pathname of shared object from the given directory name and
 * filename.
 */
static char *
aout_get_so(const char * dir, const char * file)
{
	struct db *	dbp;
	char *		path = NULL;

	PRF_MCOUNT(19, aout_get_so);
	if (dbp = lo_cache(dir)) {
		path = ask_db(dbp, file);
	}
	return (path);
}

/*
 * Determine the symbol location of an address within a link-map.  Look for
 * the nearest symbol (whoes value is less than or equal to the required
 * address).  This is the object specific part of dladdr().
 */
static void
aout_dladdr(unsigned long addr, Rt_map * lmp, Dl_info * dlip)
{
	unsigned long	ndx, cnt, base, value, _value;
	struct nlist *	sym;
	const char *	_name;

	PRF_MCOUNT(10, aout_dladdr);

	cnt = ((int)LM2LP(lmp)->lp_symstr - (int)LM2LP(lmp)->lp_symtab) /
		sizeof (struct nlist);
	sym = LM2LP(lmp)->lp_symtab;

	if (FLAGS(lmp) & FLG_RT_FIXED)
		base = 0;
	else
		base = ADDR(lmp);

	for (_value = 0, ndx = 0; ndx < cnt; ndx++, sym++) {
		if (sym->n_type == (N_EXT + N_UNDF))
			continue;

		value = sym->n_value + base;
		if (value > addr)
			continue;
		if (value < _value)
			continue;

		_value = value;
		_name = &LM2LP(lmp)->lp_symstr[sym->n_un.n_strx];

		if (value == addr)
			break;
	}

	if (_value) {
		dlip->dli_sname = _name;
		dlip->dli_saddr = (void *)_value;
	}
}

/*
 * Continue processing a dlsym request.  Lookup the required symbol in each
 * link-map specified by the dlp.  Note, that because this lookup is against
 * individual link-maps we don't need to supply a permit or starting link-map
 * to the lookup routine (see lookup_sym():analyze.c).
 */
Sym *
aout_dlsym_handle(Dl_handle * dlp, Slookup * sl, Rt_map ** _lmp)
{
	Sym *	sym;
	char	buffer[PATH_MAX];

	buffer[0] = '_';
	(void) strcpy(&buffer[1], sl->sl_name);

	if ((sym = dlsym_handle(dlp, sl, _lmp)) != 0)
		return (sym);

	/*
	 * Symbol not found as supplied.  However, most of our symbols will
	 * be in the "C" name space, where the implementation prepends a "_"
	 * to the symbol as it emits it.  Therefore, attempt to find the
	 * symbol with the "_" prepend.
	 */
	sl->sl_name = (const char *)buffer;

	return (dlsym_handle(dlp, sl, _lmp));
}

/* ARGSUSED */
static int
aout_are_u_compatible(Word * what)
{
	return (DBG_IFL_ERR_NONE);
}
