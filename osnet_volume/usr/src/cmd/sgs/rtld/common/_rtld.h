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

#ifndef	__RTLD_H
#define	__RTLD_H

#pragma ident	"@(#)_rtld.h	1.106	99/11/03 SMI"

/*
 * Common header for run-time linker.
 */
#include <sys/types.h>
#include <stdarg.h>
#include <synch.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <link.h>
#include <rtld.h>
#include <sgs.h>
#include <machdep.h>
#include <rtc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Types of directory search rules.
 */
#define	ENVDIRS 1
#define	RUNDIRS 2
#define	DEFAULT 3


/*
 * Data structure for file class specific functions and data.
 */
typedef struct fct {
	int (*		fct_are_u_this)();	/* determine type of object */
	unsigned long (*fct_entry_pt)();	/* get entry point */
	Rt_map * (*	fct_map_so)();		/* map in a shared object */
	Rt_map * (*	fct_new_lm)();		/* make a new link map */
	int (*		fct_unmap_so)();	/* unmap a shared object */
	int (*		fct_ld_needed)();	/* determine needed objects */
	Sym * (*	fct_lookup_sym)();	/* initialize symbol lookup */
	Sym * (*	fct_find_sym)();	/* find symbol in load map */
	int (*		fct_reloc)();		/* relocate shared object */
	int *		fct_search_rules;	/* search path rules */
	Pnode *		fct_dflt_dirs;		/* list of default dirs to */
						/*	search */
	Pnode *		fct_secure_dirs;	/* list of secure dirs to */
						/*	search (set[ug]id) */
	char * (*	fct_fix_name)();	/* transpose name */
	char * (*	fct_get_so)();		/* get shared object */
	void (*		fct_dladdr)();		/* get symbolic address */
	Sym * (*	fct_dlsym)();		/* process dlsym request */
	int (*		fct_are_u_compat)();	/* is file compatible  */
	int (*		fct_verify_vers)();	/* verify versioning (ELF) */
} Fct;


/*
 * Macros for getting to the file class table.
 */
#define	LM_ENTRY_PT(X)		((X)->rt_fct->fct_entry_pt)
#define	LM_UNMAP_SO(X)		((X)->rt_fct->fct_unmap_so)
#define	LM_NEW_LM(X)		((X)->rt_fct->fct_new_lm)
#define	LM_LD_NEEDED(X)		((X)->rt_fct->fct_ld_needed)
#define	LM_LOOKUP_SYM(X)	((X)->rt_fct->fct_lookup_sym)
#define	LM_FIND_SYM(X)		((X)->rt_fct->fct_find_sym)
#define	LM_RELOC(X)		((X)->rt_fct->fct_reloc)
#define	LM_SEARCH_RULES(X)	((X)->rt_fct->fct_search_rules)
#define	LM_DFLT_DIRS(X)		((X)->rt_fct->fct_dflt_dirs)
#define	LM_SECURE_DIRS(X)	((X)->rt_fct->fct_secure_dirs)
#define	LM_FIX_NAME(X)		((X)->rt_fct->fct_fix_name)
#define	LM_GET_SO(X)		((X)->rt_fct->fct_get_so)
#define	LM_DLADDR(X)		((X)->rt_fct->fct_dladdr)
#define	LM_DLSYM(X)		((X)->rt_fct->fct_dlsym)
#define	LM_VERIFY_VERS(X)	((X)->rt_fct->fct_verify_vers)

/*
 * Size of buffer for building error messages.
 */
#define	ERRSIZE		2048		/* MAXPATHLEN * 2 */

/*
 * Configuration file information.
 */
typedef struct config {
	const char *	c_name;
	Addr		c_bgn;
	Addr		c_end;
	Word *		c_hashtbl;
	Word *		c_hashchain;
	const char *	c_strtbl;
	Rtc_obj *	c_objtbl;
} Config;

/*
 * Register symbol list.
 */
typedef struct reglist {
	Rt_map *	rl_lmp;		/* defining object */
	Sym *		rl_sym;		/* regsym */
	struct reglist *rl_next;	/* next entry */
} Reglist;

/*
 * Data structure to hold interpreter information.
 */
typedef struct interp {
	char *		i_name;		/* interpreter name */
	caddr_t		i_faddr;	/* address interpreter is mapped at */
} Interp;

/*
 * Data structure used to keep track of copy relocations.  These relocations
 * are collected during initial relocation processing and maintained on the
 * COPY(lmp) list of the defining object.  Each copy list is also added to the
 * COPY(lmp) of the head object (normally the application dynamic executable)
 * from which they will be processed after all relocations are done.
 *
 * The use of RTLD_GROUP will also reference individual objects COPY(lmp) lists
 * in case a bound symbol must be assigned to it actual copy relocation.
 */
typedef struct rel_copy	{
	const char *	r_name;		/* symbol name */
	Sym *		r_rsym;		/* reference symbol table entry */
	Rt_map *	r_rlmp;		/* reference link map */
	Sym *		r_dsym;		/* definition symbol table entry */
	void *		r_radd;		/* copy to address */
	const void *	r_dadd;		/* copy from address */
	unsigned long	r_size;		/* copy size bytes */
} Rel_copy;

/*
 * Data structure to hold initial file mapping information.  Used to
 * communicate during initial object mapping and provide for error recovery.
 */
typedef struct fil_map {
	int		fm_fd;		/* File descriptor */
	char *		fm_maddr;	/* Address of initial mapping */
	size_t		fm_msize;	/* Size of initial mapping */
	int		fm_mflags;	/* mmaping flags */
	size_t		fm_fsize;	/* Actual file size */
	unsigned long	fm_etext;	/* End of text segment */
} Fmap;

/*
 * File descriptor availability flag.
 */
#define	FD_UNAVAIL	-1


/*
 * Status flags for rtld_flags
 */
#define	RT_FL_THREADS	0x00000001	/* are threads enabled */
#define	RT_FL_WARNFLTR	0x00000002	/* warn of missing filtees (ldd) */
#define	RT_FL_LOADFLTR	0x00000004	/* force loading of filtees */
#define	RT_FL_RELNOW	0x00000008	/* bind now requested */
#define	RT_FL_NOBIND	0x00000010	/* carry out plt binding */
#define	RT_FL_NOVERSION	0x00000020	/* disable version checking */
#define	RT_FL_SECURE	0x00000040	/* setuid/segid flag */
#define	RT_FL_APPLIC	0x00000080	/* are we executing user code */

#define	RT_FL_CONFGEN	0x00000200	/* don't relocate initiating object */
					/*	set by crle(1). */
#define	RT_FL_CONFAPP	0x00000400	/* application specific configuration */
					/*	cache required */

#define	RT_FL_DEBUGGER	0x00000800	/* a debugger is monitoring us */
#define	RT_FL_AUNOTIF	0x00001000	/* audit activity going on */
#define	RT_FL_DBNOTIF	0x00002000	/* bindings activity going on */
#define	RT_FL_DELNEEDED	0x00004000	/* link-map deletions required */
#define	RT_FL_DELINPROG	0x00008000	/* link-map deletions in progress */
#define	RT_FL_NOAUXFLTR	0x00010000	/* disable auxiliary filters */
#define	RT_FL_NOAUDIT	0x00040000	/* disable auditing */

#define	RT_FL_ATEXIT	0x00080000	/* we're shutting down */
#define	RT_FL_INIT	0x00100000	/* print .init order */
#define	RT_FL_BREADTH	0x00200000	/* use breadth-first for .init/.fini */
#define	RT_FL_INITFIRST	0x00400000	/* processing a DT_INITFIRST object */
#define	RT_FL_CLEANUP	0x00800000	/* cleanup processing is required */
#define	RT_FL_EXECNAME	0x01000000	/* AT_SUN_EXECNAME vector is avail */
#define	RT_FL_ORIGIN	0x02000000	/* ORIGIN processing is required */
#define	RT_FL_NOCFG	0x04000000	/* disable config file use */
#define	RT_FL_NODIRCFG	0x08000000	/* disable directory config use */
#define	RT_FL_NOOBJALT	0x10000000	/* disable object alternative use */
#define	RT_FL_DIRCFG	0x20000000	/* directory config info available */
#define	RT_FL_OBJALT	0x40000000	/* object alternatives are available */
#define	RT_FL_MEMRESV	0x80000000	/* memory reservation established */


/*
 * Binding flags for the bindguard routines
 */
#define	THR_FLG_BIND	0x00000001	/* BINDING bindguard flag */
#define	THR_FLG_MALLOC	0x00000002	/* MALLOC bindguard flag */
#define	THR_FLG_PRINT	0x00000004	/* PRINT bindguard flag */
#define	THR_FLG_BOUND	0x00000008	/* BOUNDTO bindguard flag */
#define	THR_FLG_STRERR	0x00000010	/* libc:strerror bindguard flag */
#define	THR_FLG_INIT	0x00000020	/* .INIT bindguard flag */
#define	THR_FLG_MASK	(THR_FLG_BIND | THR_FLG_MALLOC | \
			THR_FLG_PRINT | THR_FLG_BOUND | THR_FLG_INIT)
					/* mask for all THR_FLG flags */

#define	ROUND(x, a)	(((int)(x) + ((int)(a) - 1)) & \
				~((int)(a) - 1))

/*
 * Macro to control librtld_db interface information.
 */
#define	rd_event(e, s, func) \
	r_debug.r_state = (r_state_e)s; \
	r_debug.r_rdevent = e; \
	func; \
	r_debug.r_rdevent = RD_NONE;

#define	RT_SORT_FWD	0x00		/* topological sort (.fini) */
#define	RT_SORT_REV	0x01		/* reverse topological sort (.init) */
#define	RT_SORT_DELETE	0x10		/* process FLG_RT_DELNEED objects */
					/*	only (called via dlclose()) */

/*
 * Print buffer.
 */
typedef struct {
	char *	pr_buf;		/* pointer to beginning of buffer */
	char *	pr_cur;		/* pointer to next free char in buffer */
	size_t	pr_len;		/* buffer size */
	int	pr_fd;		/* output fd */
} Prfbuf;

/*
 * dlopen() handle list size.
 */
#define	HDLISTSZ	101	/* prime no. for hashing */


/*
 * Data declarations.
 */
extern rwlock_t		bindlock;	/* readers/writers binding lock */
extern rwlock_t		initlock;	/* readers/writers .init lock */
extern rwlock_t		malloclock;	/* readers/writers malloc lock */
extern rwlock_t		printlock;	/* readers/writers print lock */
extern rwlock_t		boundlock;	/* readers/writers BOUNDTO lock */
extern Sxword		ti_version;	/* version of thread interface */

extern List		dynlm_list;	/* dynamic list of link-maps */
extern char **		environ;	/* environ pointer */

extern int		dyn_plt_ent_size; /* Size of dynamic plt's */
extern unsigned long	flags;		/* machine specific file flags */
extern const char *	preload_objs;	/* preloadable file list */
extern const char *	pr_name;	/* file name of executing process */
extern struct r_debug	r_debug;	/* debugging information */
extern Rtld_db_priv	rtld_db_priv;	/* rtld/rtld_db information */
extern char *		lasterr;	/* string describing last error */
extern Interp *		interp;		/* ELF executable interpreter info */
extern const char *	rt_name;	/* name of the dynamic linker */
extern int		bind_mode;	/* object binding mode (RTLD_LAZY?) */
extern const char *	envdirs;	/* env variable LD_LIBRARY_PATH */
extern Pnode *		envlist;	/*	and its associated Pnode list */
extern List		hdl_list[];	/* dlopen() handle list */
extern size_t		syspagsz;	/* system page size */
extern char *		platform; 	/* platform name */
extern size_t		platform_sz; 	/* platform name string size */
extern Isa_desc *	isa;		/* isalist descriptor */
extern Uts_desc *	uts;		/* utsname descriptor */
extern int		rtld_flags;	/* status flags for RTLD */
extern Fmap *		fmap;		/* Initial file mapping info */

extern Fct		elf_fct;	/* ELF file class dependent data */

#if	defined(sparc) && !defined(__sparcv9)
extern Fct		aout_fct;	/* a.out (4.x) file class dependent */
					/*	data */
#endif

extern const char * 	locale;		/* locale environment setting */

#if	defined(__ia64)
extern int		find_fptr(Rt_map *, Addr *);
#endif

extern Config *		config;		/* configuration structure */
extern const char *	locale;		/* locale environment setting */

extern const char *	audit_objs;	/* LD_AUDIT objects */
extern uint_t		audit_argcnt;	/* no. of stack args to copy */
extern Audit_desc *	auditors;	/* global auditors */

extern char **		_environ;

extern const char *	dbg_str;	/* debugging tokens */
extern const char *	dbg_file;	/* debugging directed to a file */

extern Reglist *	reglist;	/* list of register symbols */

/*
 * Function declarations.
 */
extern void		addfree(void *, size_t);
extern int		analyze_so(Rt_map *);
extern Fct *		are_u_this(const char *);
extern void		atexit_fini(void);
extern int		bind_guard(int);
extern int		bind_clear(int);
extern int		bufprint(Prfbuf *, const char *, ...);
extern void		call_fini(Lm_list *, Rt_map **);
extern void		call_init(Rt_map **);
extern unsigned long	caller(void);
extern void *		calloc(size_t, size_t);
extern void		cleanup(void);
extern int		dbg_setup(const char *);
extern int		dlclose_core(Dl_handle *, Rt_map *, Rt_map *);
extern char *		dlerror(void);
extern Sym *		dlsym_handle(Dl_handle *, Slookup *, Rt_map **);
extern void *		dlsym_core(void *, const char *, Rt_map *);
extern Dl_handle *	dlmopen_core(Lm_list *, const char *, int, Rt_map *,
				uint_t);
extern size_t		doprf(const char *, va_list, Prfbuf *);
extern int		dowrite(Prfbuf *);
extern void		dz_init(int);
extern caddr_t		dz_map(caddr_t, size_t, int, int);
extern int		elf_config(Rt_map *);
extern Rtc_obj *	elf_config_ent(const char *, Word, int, const char **);
extern unsigned long	elf_hash(const char *);
extern ulong_t		elf_reloc_relative(ulong_t, ulong_t, ulong_t,
				ulong_t, ulong_t, ulong_t);
extern ulong_t		elf_reloc_relacount(ulong_t, ulong_t,
				ulong_t, ulong_t);
extern void		eprintf(Error, const char *, ...);
extern int		expand(char **, size_t *, char **, Rt_map *);
extern size_t		fullpath(Rt_map *);
extern Lmid_t		get_linkmap_id(Lm_list *);
extern Pnode *		get_next_dir(Pnode **, Rt_map *, int);
int			hdl_add(Dl_handle *, List *, Rt_map *, Rt_map *);
Dl_handle *		hdl_create(Lm_list *, Rt_map *, Rt_map *, int);
void			hdl_free(Dl_handle *);
extern Listnode *	list_append(List *, const void *);
extern int		list_delete(List *, void *);
extern int		lm_append(Lm_list *, Rt_map *);
extern void		lm_delete(Lm_list *, Rt_map *);
extern Rt_map *		load_one(Lm_list *, const char *, Rt_map *, int, int);
extern caddr_t		nu_map(caddr_t, size_t, int, int);
extern void *		malloc(size_t);
extern Pnode *		make_pnode_list(const char *, Half, int, Pnode *,
				Rt_map *);
extern void		move_data(Rt_map *);
extern void		perm_free(Permit *);
extern Permit *		perm_get(void);
extern int		perm_one(Permit *);
extern int		perm_test(Permit *, Permit *);
extern Permit *		perm_set(Permit *, Permit *);
extern Permit *		perm_unset(Permit *, Permit *);
extern int		pr_open(void);
extern int		readenv(const char **, int);
extern int		relocate_so(Rt_map *);
extern int		remove_hdl(Dl_handle *, Rt_map *, Rt_map *);
extern void		remove_so(Lm_list *, Rt_map *);
extern int		rt_atfork(void (*)(void), void (*)(void),
				void (*)(void));
extern int		rt_mutex_lock(mutex_t *, sigset_t *);
extern int		rt_mutex_unlock(mutex_t *, sigset_t *);
extern void		rtld_db_dlactivity(void);
extern void		rtld_db_preinit(void);
extern void		rtld_db_postinit(void);
extern void		security(uid_t, uid_t, gid_t, gid_t);
extern void		set_environ(Rt_map *);
extern int		setup(Rt_map *, unsigned long, unsigned long);
extern Rt_map **	tsort(Rt_map *, int, int);
extern void		zero(caddr_t, size_t);

#ifdef PRF_RTLD
extern int		profile_setup(Link_map *);
extern const char *	profile_name;
#endif

#if	defined(sparc)
/*
 * SPARC Register symbol support.
 */
extern int		elf_regsyms(Rt_map *);
extern void		set_sparc_g1(unsigned long);
extern void		set_sparc_g2(unsigned long);
extern void		set_sparc_g3(unsigned long);
extern void		set_sparc_g4(unsigned long);
extern void		set_sparc_g5(unsigned long);
extern void		set_sparc_g6(unsigned long);
extern void		set_sparc_g7(unsigned long);
#endif /* defined(sparc) */

extern long		_sysconfig(int);

#ifdef	__cplusplus
}
#endif

#endif /* __RTLD_H */
