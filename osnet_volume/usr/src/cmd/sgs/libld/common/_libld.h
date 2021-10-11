/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

/*
 * Local include file for ld library.
 */

#ifndef	_LIBLD_DOT_H
#define	_LIBLD_DOT_H

#pragma ident	"@(#)_libld.h	1.66	99/06/01 SMI"

#include	"libld.h"
#include	"conv.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEBUG

/*
 * Types of segment index.
 */
typedef enum {
	LD_PHDR,	LD_INTERP,	LD_TEXT,	LD_DATA,
	LD_DYN,		LD_NOTE,	LD_SUNWBSS,
#if	defined(ELF_TARGET_IA64)
	LD_IA_64_UNDIND,
#endif	/* ELF_TARGET_IA64 */
	LD_EXTRA
} Segment_ndx;

/*
 * Structure to manage archive member caching.  Each archive has an archive
 * descriptor (Ar_desc) associated with it.  This contains pointers to the
 * archive symbol table (obtained by elf_getarsyms(3e)) and an auxiliary
 * structure (Ar_uax[]) that parallels this symbol table.  The member element
 * of this auxiliary table indicates whether the archive member associated with
 * the symbol offset has already been extracted (AREXTRACTED) or partially
 * processed (refer process_member()).
 */
typedef struct ar_mem {
	Elf		*am_elf;	/* elf descriptor for this member */
	char		*am_name;	/* members name */
	char		*am_path;	/* path (ie. lib(foo.o)) */
	Sym		*am_syms;	/* start of global symbols */
	char		*am_strs;	/* associated string table start */
	Xword		am_symn;	/* no. of global symbols */
} Ar_mem;

typedef struct ar_aux {
	Sym_desc	*au_syms;	/* internal symbol descriptor */
	Ar_mem		*au_mem;	/* associated member */
} Ar_aux;

#define	AREXTRACTED	(Ar_mem *)-1

typedef struct ar_desc {
	const char	*ad_name;	/* archive file name */
	Elf		*ad_elf;	/* elf descriptor for the archive */
	Elf_Arsym	*ad_start;	/* archive symbol table start */
	Ar_aux		*ad_aux;	/* auxiliary symbol information */
	int		ad_cnt;		/* no. of command line occurrences */
	dev_t		ad_stdev;	/* device id and inode number for */
	ino_t		ad_stino;	/*	multiple inclusion checks */
} Ar_desc;

/*
 * Structure to manage the update of weak symbols from their associated alias.
 */
typedef	struct wk_desc {
	Sym		*wk_symtab;	/* the .symtab entry */
	Sym		*wk_dynsym;	/* the .dynsym entry */
	Sym_desc	*wk_weak;	/* the original weak symbol */
	Sym_desc	*wk_alias;	/* the real symbol */
} Wk_desc;

/*
 * Structure to manage the support library interfaces.
 */
typedef struct func_list {
	const char	*fl_obj;	/* name of support object */
					/*	function is from */
	void		(*fl_fptr)();	/* function pointer */
} Func_list;

typedef	struct support_list {
	const char	*sup_name;	/* ld_support function name */
	List		sup_funcs;	/* list of support functions */
} Support_list;

/*
 * Structure to manage a sorted output relocation list
 */
typedef struct reloc_list {
	Sym_desc	*rl_key;
	Rel_desc	*rl_rsp;
} Reloc_list;


/*
 * ld heap management structure
 */
typedef struct _ld_heap Ld_heap;
struct _ld_heap {
	Ld_heap		*lh_next;
	void		*lh_free;
	void		*lh_end;
};

#define	HEAPBLOCK	0x68000		/* default allocation block size */
#define	HEAPALIGN	0x8		/* heap blocks alignment requirement */


/*
 * Management of GOT ndx's
 */
typedef struct {
	Xword	gn_addend;	/* addend associated with GOT entry */
	Xword	gn_gotndx;	/* got table index */
} Gotndx;

/*
 * Indexes into the ld_support_funcs[] table.
 */
#define	LD_START	0
#define	LD_ATEXIT	1
#define	LD_FILE		2
#define	LD_SECTION	3


#ifndef	FILENAME_MAX
#define	FILENAME_MAX	BUFSIZ		/* maximum length of a path name */
#endif

#define	REL_AIDESCNO	500		/* size of Active relocation buckets */
#define	REL_OIDESCNO	50		/* size of output relocation buckets */
#define	SYM_IDESCNO	10
#define	SYM_DICTSZ	0x4000

extern Ld_heap		*ld_heap;

extern Ehdr		def_ehdr;

/*
 * Local functions.
 */
extern uintptr_t	add_actrel(Half, Rel_desc *, void *, Ofl_desc *);
extern uintptr_t	add_outrel(Half, Rel_desc *, void *, Ofl_desc *);
extern uintptr_t	add_regsym(Sym_desc *, Ofl_desc *);
extern void 		adj_expandreloc(Ofl_desc *, Rel_desc *);
extern void 		adj_movereloc(Ofl_desc *, Rel_desc *);
extern Ar_desc		*ar_setup(const char *, Elf *, int, Ofl_desc *);
extern void		ar_member(Ar_desc *, Elf_Arsym *, Ar_aux *, Ar_mem *);
#if	defined(sparc)
extern uintptr_t	allocate_got(Ofl_desc *);
extern uintptr_t	assign_got(Sym_desc *);
#endif
extern uintptr_t	assign_got_ndx(List *, Rel_desc *, void *,
				Sym_desc *, Gotndx *, Ofl_desc *);
extern void		assign_plt_ndx(Sym_desc *, Ofl_desc *);
extern uintptr_t	do_activerelocs(Ofl_desc *);
extern void		fillin_gotplt1(Ofl_desc *);
extern Addr		fillin_gotplt2(Ofl_desc *);
extern Gotndx		*find_gotndx(List *, void *);
extern Xword		lcm(Xword, Xword);
extern Listnode		*list_where(List *, Word);
extern void		mach_make_dynamic(Ofl_desc *, size_t *);
extern void		mach_update_odynamic(Ofl_desc *, Dyn **);
extern uintptr_t	mach_dyn_locals(Sym *, const char *, size_t,
				Ifl_desc *, Ofl_desc *);
extern uintptr_t	mach_sym_typecheck(Sym_desc *, Sym *, Ifl_desc *,
				Ofl_desc *);
extern uintptr_t	make_bss(Ofl_desc *, Xword, Xword);
extern uintptr_t	make_got(Ofl_desc *);
extern uintptr_t	make_reloc(Ofl_desc *, Os_desc *);
extern uintptr_t	make_sunwbss(Ofl_desc *, size_t, Xword);
extern uintptr_t	make_sunwdata(Ofl_desc *, size_t, Xword);
extern uintptr_t	make_sunwmove(Ofl_desc *, int);
extern uintptr_t	perform_outreloc(Rel_desc *, Ofl_desc *);
extern Os_desc		*place_section(Ofl_desc *, Is_desc *, int, Word);
extern uintptr_t	process_archive(const char *, int, Ar_desc *,
				Ofl_desc *);
extern Ifl_desc		*process_ifl(const char *, const char *, int, Elf *,
				Half, Ofl_desc *, int *);
extern uintptr_t	process_ordered(Ifl_desc *, Ofl_desc *, Word, Word);
extern uintptr_t	process_section(const char *, Ifl_desc *, Shdr *,
				Elf_Scn *, Word, int, Ofl_desc *);
extern uintptr_t	reloc_local(Rel_desc *, void *, Ofl_desc *);
extern uintptr_t	reloc_local_fptr(Rel_desc *, void *, Ofl_desc *);
extern uintptr_t	reloc_register(Rel_desc *, void *, Ofl_desc *);
extern uintptr_t	reloc_relobj(Boolean, Rel_desc *, void *, Ofl_desc *);
extern void		reloc_remain_entry(Rel_desc *, Os_desc *, Ofl_desc *);
extern void		sort_ordered(Ofl_desc *);
extern uintptr_t	sym_copy(Sym_desc *);
extern uintptr_t	sym_process(Is_desc *, Ifl_desc *, Ofl_desc *);
extern uintptr_t	sym_resolve(Sym_desc *, Sym *, Ifl_desc *, Ofl_desc *,
				int);
extern uintptr_t	sym_spec(Ofl_desc *);
extern uintptr_t	vers_def_process(Is_desc *, Ifl_desc *, Ofl_desc *);
extern uintptr_t	vers_need_process(Is_desc *, Ifl_desc *, Ofl_desc *);
extern int		vers_sym_process(Is_desc *, Ifl_desc *);
extern uintptr_t	vers_check_need(Ofl_desc *);
extern void		vers_promote(Sym_desc *, Word, Ifl_desc *, Ofl_desc *);
extern int		vers_verify(Ofl_desc *);

#if	defined(ELF_TARGET_IA64)
/*
 * IA64 specific functions
 */
extern uintptr_t	process_ia64_unwind(const char *, Ifl_desc *, Shdr *,
				Elf_Scn *, Word, int, Ofl_desc *);
#endif	/* ELF_TARGET_IA64 */

#ifdef	__cplusplus
}
#endif

#endif /* _LIBLD_DOT_H */
