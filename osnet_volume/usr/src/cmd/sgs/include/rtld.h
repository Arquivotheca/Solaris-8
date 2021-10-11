/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_RTLD_H
#define	_RTLD_H

#pragma ident	"@(#)rtld.h	1.57	99/09/21 SMI"

/*
 * Global include file for the runtime linker support library.
 */
#include <time.h>
#include <sgs.h>
#include <machdep.h>

#ifdef	_SYSCALL32
#include <inttypes.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Permission structure.  Used to define access with ld.so.1 link maps.
 */
typedef struct permit {
	unsigned long	p_cnt;		/* No. of p_value entries of which */
	unsigned long	p_value[1];	/* there may be more than one */
} Permit;


/*
 * Linked list of directories or filenames (built from colon separated string).
 */
typedef struct pnode {
	const char *	p_name;
	size_t		p_len;
	Word		p_orig;
	void *		p_info;
	struct pnode *	p_next;
} Pnode;

typedef struct rt_map	Rt_map;


/*
 * Private structure for communication between rtld_db and rtld.
 */
#define	R_RTLDDB_VERSION	1	/* current rtld_db/rtld version level */

typedef struct rtld_db_priv {
	long		rtd_version;	/* version no. */
	size_t		rtd_objpad;	/* padding around mmap()ed objects */
	List *		rtd_dynlmlst;	/* pointer to Dynlm_list */
} Rtld_db_priv;

#ifdef _SYSCALL32
typedef struct rtld_db_priv32 {
	Elf32_Word	rtd_version;	/* version no. */
	Elf32_Word	rtd_objpad;	/* padding around mmap()ed objects */
	Elf32_Addr	rtd_dynlmlst;	/* pointer to Dynlm_list */
} Rtld_db_priv32;
#endif	/* _SYSCALL32 */


/*
 * Information for dlopen(), dlsym(), and dlclose() on libraries linked by rtld.
 * Each shared object referred to in a dlopen call has an associated Dl_handle
 * structure.  For each such structure there is a list of the shared objects
 * on which the referenced shared object is dependent.
 */
typedef struct dl_handle {
	Permit *	dl_permit;	/* permit (0 for dlopen(0)) */
	int		dl_usercnt;	/* count of dlopen invocations */
	int		dl_permcnt;	/* count of permit give-aways */
	List		dl_depends;	/* dependencies applicable for dlsym */
	List		dl_parents;	/* parents referenced (RTLD_PARENT) */
} Dl_handle;

#define	HDLHEAD(X)	((Rt_map *)(X)->dl_depends.head->data)

/*
 * Runtime linker private data maintained for each shared object.  Maps are
 * connected to link map lists for `main' and possibly `rtld'.
 */
typedef	struct lm_list {
	Rt_map *	lm_head;
	Rt_map *	lm_tail;
	Dl_handle *	lm_handle;
	Word		lm_flags;
	int		lm_obj;		/* total number of objs on link-map */
	int		lm_init;	/* new obj since last init processing */
	int		lm_lazy;	/* obj with pending lazy dependencies */
} Lm_list;

#ifdef	_SYSCALL32
typedef struct lm_list32 {
	Elf32_Addr	lm_head;
	Elf32_Addr	lm_tail;
	Elf32_Addr	lm_handle;
	Elf32_Word	lm_flags;
	int		lm_obj;
	int		lm_init;
	int		lm_lazy;
} Lm_list32;
#endif /* _SYSCALL32 */

/*
 * Possible Link_map list flags (Lm_list.lm_list)
 */
#define	LML_FLG_BASELM		0x00000001	/* primary link-map */
#define	LML_FLG_RTLDLM		0x00000002	/* rtld link-map */
#define	LML_FLG_NOAUDIT		0x00000004	/* symbol auditing disabled */
#define	LML_FLG_PLTREL		0x00000008	/* deferred plt relocation */
						/* 	initialization */
						/*	(ld.so.1 only) */
#define	LML_FLG_ANALYSIS	0x00000010	/* list analysis underway */
#define	LML_FLG_RELOCATING	0x00000020	/* list relocation underway */
#define	LML_FLG_ENVIRON		0x00000040	/* environ var initialized */
#define	LML_FLG_INTRPOSE	0x00000080	/* interposing objs on list */
#define	LML_FLG_LOCAUDIT	0x00000100	/* local auditors exists for */
						/*	this link-map list */
#define	LML_FLG_LOADAVAIL	0x00000200	/* load anything available */
#define	LML_FLG_IGNERROR	0x00000400	/* ignore errors (crle(1)) */

#define	LML_TRC_SKIP		0x00000800	/* skip first obj (lddstub) */
#define	LML_TRC_ENABLE		0x00001000	/* tracing enabled (ldd) */
#define	LML_TRC_WARN		0x00002000	/* print warnings for undefs */
#define	LML_TRC_VERBOSE		0x00004000	/* verbose (versioning) trace */
#define	LML_TRC_SEARCH		0x00008000	/* trace search paths */

#define	LML_FLG_NOLAZYLD	0x00010000	/* lazy loading disabled */
#define	LML_FLG_NODIRECT	0x00020000	/* direct bindings disabled */

#define	LML_AUD_PREINIT		0x01000000	/* preinit (audit) exists */
#define	LML_AUD_SEARCH		0x02000000	/* objsearch (audit) exists */
#define	LML_AUD_OPEN		0x04000000	/* objopen (audit) exists */
#define	LML_AUD_CLOSE		0x08000000	/* objclose (audit) exists */
#define	LML_AUD_SYMBIND		0x10000000	/* symbind (audit) exists */
#define	LML_AUD_PLTENTER	0x20000000	/* pltenter (audit) exists */
#define	LML_AUD_PLTEXIT		0x40000000	/* pltexit (audit) exists */
#define	LML_AUD_ACTIVITY	0x80000000	/* activity (audit) exists */

#define	LML_MSK_AUDIT		0xff000000	/* audit interfaces mask */
#define	LML_MSK_NEWLM		0xffff0000	/* flags transferable to new */
						/*	link map */

/*
 * .dynamic Information
 *
 * Note: Currently we only track info on NEEDED entries here, but this
 *	 could easily be extended if needed.
 */
#define	FLG_DI_GROUP	0x0001		/* object opens with GROUP perm */

typedef struct dyninfo {
	Rt_map *	di_lmp;		/* link-map */
	uint32_t	di_flags;	/* flags */
	Dyn *		di_dyn;		/* pointer to dynamic entry */
} Dyninfo;

/*
 * Link-map definition.
 */
struct rt_map {
	Link_map	rt_public;	/* public data */
	List		rt_alias;	/* list of linked file names */
	void (*		rt_init)();	/* address of _init */
	void (*		rt_fini)();	/* address of _fini */
	char *		rt_runpath;	/* LD_RUN_PATH and its equivalent */
	Pnode *		rt_runlist;	/*	Pnode structures */
	List		rt_edepends;	/* list of explicit dependencies */
	List		rt_idepends;	/* list of implicit dependencies, ie. */
					/*	bindings outside edepends */
	List		rt_callers;	/* list of callers (parents) */
	Dl_handle *	rt_handle;	/* dlopen handle */
	Permit *	rt_permit;	/* visibility and accessibility */
	List		rt_donors;	/* who gave us a permit */
	unsigned long	rt_msize;	/* total memory mapped */
	unsigned long	rt_etext;	/* etext address */
	unsigned long	rt_padstart;	/* start of image (including padding) */
	unsigned long	rt_padimlen;	/* size of image (including padding */
	struct fct *	rt_fct;		/* file class table for this object */
	Pnode *		rt_filtees;	/* 	Pnode list of REFNAME(lmp) */
	Sym *(*		rt_symintp)();	/* link map symbol interpreter */
	void *		rt_priv;	/* private data, object type specific */
	Lm_list *	rt_list;	/* link map list we belong to */
	uint_t		rt_flags;	/* state flags, see FLG below */
	uint_t		rt_flags1;	/* state flags1, see FL1 below */
	uint_t		rt_count;	/* reference count */
	int		rt_mode;	/* usage mode, see RTLD mode flags */
	uint_t		rt_sortval;	/* temporary buffer to traverse graph */
	dev_t		rt_stdev;	/* device id and inode number for .so */
	ino_t		rt_stino;	/*	multiple inclusion checks */
	char *		rt_pathname;	/* full pathname of loaded object */
	size_t		rt_dirsz;	/*	and its size */
	List		rt_copy;	/* list of copy relocations */
	Audit_desc * 	rt_auditors;	/* audit descriptor array */
	Audit_info * 	rt_audinfo;	/* audit information descriptor */
	Syminfo *	rt_syminfo;	/* elf .syminfo section - here */
					/*	because it is checked in */
					/*	common code */
	unsigned long	rt_sdata;	/* start of data */
	Dyninfo	*	rt_dyninfo;	/* array of dyninfo entries - used */
	uint_t		rt_dyninfocnt;	/* count of dyninfo entries */
	uint_t		rt_relacount;	/* no. of RELATIVE relocations */
	uint_t		rt_idx;		/* hold index within linkmap list */
	uint_t		rt_lazy;	/* lazy dependencies pending */
};

#ifdef _SYSCALL32
/*
 * Structure to allow 64-bit rtld_db to read 32-bit processes out of procfs.
 */
typedef struct rt_map32 {
	Link_map32	rt_public;
	List32		rt_alias;
	uint32_t 	rt_init;
	uint32_t	rt_fini;
	uint32_t	rt_runpath;
	uint32_t	rt_runlist;
	List32		rt_edepends;
	List32		rt_idepends;
	List32		rt_callers;
	uint32_t	rt_handle;
	uint32_t	rt_permit;
	List32		rt_donors;
	uint32_t	rt_msize;
	uint32_t	rt_etext;
	uint32_t	rt_padstart;
	uint32_t	rt_padimlen;
	uint32_t	rt_fct;
	uint32_t	rt_filtees;
	uint32_t	rt_symintp;
	uint32_t	rt_priv;
	uint32_t 	rt_list;
	uint32_t	rt_flags;
	uint32_t	rt_flags1;
	uint32_t	rt_count;
	uint32_t	rt_mode;
	uint32_t	rt_sortval;
	uint32_t	rt_stdev;
	uint32_t	rt_stino;
	uint32_t	rt_pathname;
	uint32_t	rt_dirsz;
	List32		rt_copy;
	uint32_t 	rt_auditors;
	uint32_t 	rt_audinfo;
	uint32_t	rt_syminfo;
	uint32_t	rt_sdata;
	uint32_t 	rt_dyninfo;
	uint32_t 	rt_dyninfocnt;
	uint32_t	rt_relacount;
	uint32_t	rt_idx;
	uint32_t	rt_lazy;
} Rt_map32;

#endif	/* _SYSCALL32 */


#define	REF_NEEDED	1		/* explicit (needed) dependency */
#define	REF_SYMBOL	2		/* implicit (symbol binding) */
					/*	dependency */
#define	REF_DIRECT	3		/* explicit (direct binding) */
					/*	dependency */
#define	REF_DLCLOSE	4		/* dlclose() - (debugging only) */
#define	REF_DELETE	5		/* delete object - (debugging only) */
#define	REF_ORPHAN	6		/* ophan a handle - (debugging only) */


/*
 * Link map state flags.
 */
#define	FLG_RT_ISMAIN	0x00000001	/* object represents main executable */
#define	FLG_RT_ANALYZED	0x00000002	/* object has been analyzed */
#define	FLG_RT_SETGROUP	0x00000004	/* group establishment required */
#define	FLG_RT_COPYTOOK	0x00000008	/* copy relocation taken */
#define	FLG_RT_OBJECT	0x00000010	/* object processing (ie. .o's) */
#define	FLG_RT_BOUND	0x00000020	/* bound to indicator */
#define	FLG_RT_NODUMP	0x00000040	/* object can't be dldump(3x)'ed */
#define	FLG_RT_DELETE	0x00000080	/* object can be deleted */
#define	FLG_RT_IMGALLOC	0x00000100	/* image is allocated (not mmap'ed) */
#define	FLG_RT_INITDONE	0x00000200	/* objects .init has be called */
#define	FLG_RT_AUX	0x00000400	/* filter is an auxiliary filter */
#define	FLG_RT_FIXED	0x00000800	/* image location is fixed */
#define	FLG_RT_PRELOAD	0x00001000	/* object was preloaded */
#define	FLG_RT_ALTER	0x00002000	/* alternative object used */
#define	FLG_RT_RELOCED	0x00004000	/* object has been relocated */
#define	FLG_RT_LOADFLTR	0x00008000	/* trigger filtee loading */
#define	FLG_RT_AUDIT	0x00010000	/* object is an auditor */
#define	FLG_RT_NOERROR	0x00020000	/* no error condition necessary */
#define	FLG_RT_FINIDONE	0x00040000	/* objects .fini done */
#define	FLG_RT_INITFRST 0x00080000	/* execute .init first */
#define	FLG_RT_NOOPEN	0x00100000	/* dlopen() not allowed */
#define	FLG_RT_FINICLCT	0x00200000	/* fini has been collected (tsort) */
#define	FLG_RT_ORIGIN	0x00400000	/* $ORIGIN processing required */
#define	FLG_RT_INTRPOSE	0x00800000	/* object is an INTERPOSER */
#define	FLG_RT_DIRECT	0x01000000	/* object has DIRECT bindings enabled */
#define	FLG_RT_SUNWBSS	0x02000000	/* object with PT_SUNWBSS, not mapped */
#define	FLG_RT_MOVE	0x04000000	/* object needs move operation */
#define	FLG_RT_DLSYM	0x08000000	/* dlsym in progress on object */
#define	FLG_RT_REGSYMS	0x10000000	/* object has DT_REGISTER entries */
#define	FLG_RT_INITCLCT	0x20000000	/* init has been collected (tsort) */
#define	FLG_RT_HANDLE	0x40000000	/* generate a handle for this object */
#define	FLG_RT_RELNOW	0x80000000	/* bind now requested */


#define	FL1_RT_RMPERM	0x00000001	/* permit can be removed */
#define	FL1_RT_PERMRQ	0x00000002	/* permit is required */
#define	FL1_RT_CONFSET	0x00000004	/* object was loaded by crle(1) */
#define	FL1_RT_NODEFLIB	0x00000008	/* ignore default library search */
#define	FL1_RT_ENDFILTE	0x00000010	/* filtee terminates filters search */

#define	FL1_RS_START	0x01000000	/* RESERVATION start for AU flags */
#define	FL1_AU_PREINIT	LML_AUD_PREINIT
#define	FL1_AU_SEARCH	LML_AUD_SEARCH
#define	FL1_AU_OPEN	LML_AUD_OPEN
#define	FL1_AU_CLOSE	LML_AUD_CLOSE
#define	FL1_AU_SYMBIND	LML_AUD_SYMBIND
#define	FL1_AU_PLTENTER	LML_AUD_PLTENTER
#define	FL1_AU_PLTEXIT	LML_AUD_PLTEXIT
#define	FL1_AU_ACTIVITY	LML_AUD_ACTIVITY
#define	FL1_RE_END	0x80000000	/* RESERVATION end for AU flags */

#define	FL1_MSK_AUDIT	LML_MSK_AUDIT

/*
 * Macros for getting to link_map data.
 */
#define	ADDR(X)		((X)->rt_public.l_addr)
#define	NAME(X)		((X)->rt_public.l_name)
#define	DYN(X)		((X)->rt_public.l_ld)
#define	NEXT(X)		((X)->rt_public.l_next)
#define	PREV(X)		((X)->rt_public.l_prev)
#define	REFNAME(X)	((X)->rt_public.l_refname)

/*
 * Macros for getting to linker private data.
 */
#define	ALIAS(X)	((X)->rt_alias)
#define	INIT(X)		((X)->rt_init)
#define	FINI(X)		((X)->rt_fini)
#define	RPATH(X)	((X)->rt_runpath)
#define	RLIST(X)	((X)->rt_runlist)
#define	COUNT(X)	((X)->rt_count)
#define	EDEPENDS(X)	((X)->rt_edepends)
#define	IDEPENDS(X)	((X)->rt_idepends)
#define	CALLERS(X)	((X)->rt_callers)
#define	HANDLE(X)	((X)->rt_handle)
#define	PERMIT(X)	((X)->rt_permit)
#define	DONORS(X)	((X)->rt_donors)
#define	MSIZE(X)	((X)->rt_msize)
#define	ETEXT(X)	((X)->rt_etext)
#define	FCT(X)		((X)->rt_fct)
#define	FILTEES(X)	((X)->rt_filtees)
#define	SYMINTP(X)	((X)->rt_symintp)
#define	LIST(X)		((X)->rt_list)
#define	FLAGS(X)	((X)->rt_flags)
#define	FLAGS1(X)	((X)->rt_flags1)
#define	MODE(X)		((X)->rt_mode)
#define	SORTVAL(X)	((X)->rt_sortval)
#define	PADSTART(X)	((X)->rt_padstart)
#define	PADIMLEN(X)	((X)->rt_padimlen)
#define	PATHNAME(X)	((X)->rt_pathname)
#define	DIRSZ(X)	((X)->rt_dirsz)
#define	COPY(X)		((X)->rt_copy)
#define	AUDITORS(X)	((X)->rt_auditors)
#define	AUDINFO(X)	((X)->rt_audinfo)
#define	RELACOUNT(X)	((X)->rt_relacount)
#define	SYMINFO(X)	((X)->rt_syminfo)
#define	DYNINFO(X)	((X)->rt_dyninfo)
#define	DYNINFOCNT(X)	((X)->rt_dyninfocnt)
#define	SDATA(X)	((X)->rt_sdata)
#define	STDEV(X)	((X)->rt_stdev)
#define	STINO(X)	((X)->rt_stino)
#define	IDX(X)		((X)->rt_idx)
#define	LAZY(X)		((X)->rt_lazy)


/*
 * Flags for lookup_sym (and hence find_sym) routines.
 */
#define	LKUP_DEFT	0x000		/* Simple lookup request */
#define	LKUP_SPEC	0x001		/* Special ELF lookup (allows address */
					/*	resolutions to plt[] entries) */
#define	LKUP_LDOT	0x002		/* Indicates the original A_OUT */
					/*	symbol had a leading `.' */
#define	LKUP_FIRST	0x004		/* Lookup symbol in first link map */
					/*	only */
#define	LKUP_COPY	0x008		/* Lookup symbol for a COPY reloc, do */
					/*	not bind to symbol at head */

#define	LKUP_UNDEF	0x020		/* Lookup undefined symbol */
#define	LKUP_WEAK	0x040		/* Relocation reference is weak */
#define	LKUP_NEXT	0x080		/* Request originates from RTLD_NEXT */
#define	LKUP_NODESCENT	0x100		/* Don't descend through dependencies */

/*
 * Data structure for calling lookup_sym()
 */
typedef struct {
	const char *	sl_name;	/* symbol name */
	Permit *	sl_permit;	/* permit allowed to view */
	Rt_map *	sl_cmap;	/* callers link-map */
	Rt_map *	sl_imap;	/* initial link-map to search */
	unsigned long	sl_rsymndx;	/* referencing reloc symndx */
} Slookup;

/*
 * Prototypes.
 */
extern Lm_list		lml_main;	/* the `main's link map list */
extern Lm_list		lml_rtld;	/* rtld's link map list */
extern Lm_list *	lml_list[];

extern int		bound_add(int, Rt_map *, Rt_map *);
extern int		do_reloc(unsigned char, unsigned char *, Xword *,
			    const char *, const char *);
extern void		elf_plt_write(unsigned long *, unsigned long *,
			    unsigned long *);
extern void		eprintf(Error, const char *, ...);
extern Rt_map *		is_so_loaded(Lm_list *, const char *, int);
extern Sym *		lookup_sym(Slookup *, Rt_map **, int);
extern int		rt_dldump(Rt_map *, const char *, int, int, Addr);

#ifdef	__cplusplus
}
#endif

#endif /* _RTLD_H */
