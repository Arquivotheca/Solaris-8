/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fcall.c	1.8	99/10/25 SMI"

#define	_SYSCALL32

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stack.h>
#include <signal.h>
#include <limits.h>
#include <sys/isa_defs.h>
#include <proc_service.h>
#include <fnmatch.h>
#include <libproc.h>
#include "ramdata.h"
#include "systable.h"
#include "print.h"
#include "proto.h"

/*
 * Functions supporting library function call tracing.
 */

typedef struct {
	struct ps_prochandle *ph;
	prmap_t	*pmap;
	int	nmap;
} ph_map_t;

/*
 * static functions in this file.
 */
static void function_entry(struct ps_prochandle *,
		struct bkpt *, struct callstack *);
static void function_return(struct ps_prochandle *, struct callstack *);
static int object_iter(void *, const prmap_t *, const char *);
static int symbol_iter(void *, const GElf_Sym *, const char *);
static uintptr_t get_return_address(struct ps_prochandle *, uintptr_t *);
static int get_arguments(struct ps_prochandle *, long *argp);
static uintptr_t previous_fp(struct ps_prochandle *, uintptr_t, uintptr_t *);
static int lwp_stack_traps(void *cd, const lwpstatus_t *Lsp);
static int thr_stack_traps(const td_thrhandle_t *Thp, void *cd);
static struct bkpt *create_bkpt(struct ps_prochandle *, uintptr_t, int, int);
static void set_deferred_brekpoints(struct ps_prochandle *Pr);

#define	DEF_MAXCALL	16	/* initial value of Stk->maxcall */

#define	FAULT_ADDR	((uintptr_t)(0-8))

#define	HASHSZ	2048
#define	bpt_hash(addr)	((((addr) >> 13) ^ ((addr) >> 2)) & 0x7ff)

/*
 * The dynamic linker handle for libthread_db
 */
static	void	*tdb_handle;

/*
 * Functions we need from libthread_db
 */
typedef	td_err_e
	(*p_td_init_t)(void);
typedef	td_err_e
	(*p_td_ta_new_t)(void *, td_thragent_t **);
typedef	td_err_e
	(*p_td_ta_delete_t)(td_thragent_t *);
typedef	td_err_e
	(*p_td_ta_thr_iter_t)(const td_thragent_t *, td_thr_iter_f *, void *,
		td_thr_state_e, int, sigset_t *, unsigned);
typedef td_err_e
	(*p_td_thr_get_info_t)(const td_thrhandle_t *, td_thrinfo_t *);
typedef td_err_e
	(*p_td_thr_getgregs_t)(const td_thrhandle_t *, prgregset_t);
typedef td_err_e
	(*p_td_ta_map_lwp2thr_t)(const td_thragent_t *, lwpid_t,
		td_thrhandle_t *th_p);

static	p_td_init_t			p_td_init;
static	p_td_ta_new_t			p_td_ta_new;
static	p_td_ta_delete_t		p_td_ta_delete;
static	p_td_ta_thr_iter_t		p_td_ta_thr_iter;
static	p_td_thr_get_info_t		p_td_thr_get_info;
static	p_td_thr_getgregs_t		p_td_thr_getgregs;
static	p_td_ta_map_lwp2thr_t		p_td_ta_map_lwp2thr;

/*
 * Establishment of breakpoints on traced library functions.
 */
void
establish_breakpoints(struct ps_prochandle *Pr)
{
	if (Dynpat == NULL)
		return;

	/* allocate the breakpoint hash table */
	if (bpt_hashtable == NULL) {
		bpt_hashtable = malloc(HASHSZ * sizeof (struct bkpt *));
		(void) memset(bpt_hashtable, 0,
			HASHSZ * sizeof (struct bkpt *));
	}

	/*
	 * Set special rtld_db event breakpoints, first time only.
	 */
	if (Rdb_agent == NULL &&
	    (Rdb_agent = Prd_agent(Pr)) != NULL) {
		rd_notify_t notify;
		struct bkpt *Bp;

		(void) rd_event_enable(Rdb_agent, 1);
		if (rd_event_addr(Rdb_agent, RD_PREINIT, &notify) == RD_OK &&
		    (Bp = create_bkpt(Pr, notify.u.bptaddr, 0, 1)) != NULL)
			Bp->flags |= BPT_PREINIT;
		if (rd_event_addr(Rdb_agent, RD_POSTINIT, &notify) == RD_OK &&
		    (Bp = create_bkpt(Pr, notify.u.bptaddr, 0, 1)) != NULL)
			Bp->flags |= BPT_POSTINIT;
		if (rd_event_addr(Rdb_agent, RD_DLACTIVITY, &notify) == RD_OK &&
		    (Bp = create_bkpt(Pr, notify.u.bptaddr, 0, 1)) != NULL)
			Bp->flags |= BPT_DLACTIVITY;
	}

	/*
	 * Tell libproc to update its mappings.
	 */
	Pupdate_maps(Pr);

	/*
	 * Iterate over the shared objects, creating breakpoints.
	 */
	(void) Pobject_iter(Pr, object_iter, Pr);

	/*
	 * Now actually set all the breakpoints we just created.
	 */
	set_deferred_brekpoints(Pr);
}

/*
 * Initial establishment of stacks in a newly-grabbed process.
 */
void
establish_stacks(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	char mapfile[64];
	int mapfd;
	struct stat statb;
	prmap_t *Pmap = NULL;
	int nmap = 0;
	ph_map_t ph_map;

	(void) sprintf(mapfile, "/proc/%d/rmap", (int)Psp->pr_pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0 ||
	    statb.st_size < sizeof (prmap_t) ||
	    (Pmap = malloc(statb.st_size)) == NULL ||
	    (nmap = pread(mapfd, Pmap, statb.st_size, 0L)) <= 0 ||
	    (nmap /= sizeof (prmap_t)) == 0) {
		if (Pmap != NULL)
			free(Pmap);
		Pmap = NULL;
		nmap = 0;
	}
	if (mapfd >= 0)
		(void) close(mapfd);

	/*
	 * Iterate over lwps, establishing stacks.
	 */
	ph_map.ph = Pr;
	ph_map.pmap = Pmap;
	ph_map.nmap = nmap;
	(void) Plwp_iter(Pr, lwp_stack_traps, &ph_map);
	if (Pmap != NULL)
		free(Pmap);

	if (!has_libthread)
		return;

	/*
	 * Iterate over unbound threads, establishing stacks.
	 */
	if (Thr_agent != NULL)
		(void) p_td_ta_delete(Thr_agent);
	if ((p_td_init() != TD_OK || p_td_ta_new(Pr, &Thr_agent) != TD_OK)) {
		Thr_agent = NULL;
		return;
	}
	(void) p_td_ta_thr_iter(Thr_agent, thr_stack_traps, Pr,
		TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
}

static int not_consist = FALSE;

static void
do_symbol_iter(struct ps_prochandle *Pr,
	const char *object_name, struct dynpat *Dyp)
{
	if (*Dyp->Dp->prt_name == '\0')
		object_name = PR_OBJ_EXEC;

	/*
	 * Always search the dynamic symbol table.
	 */
	(void) Psymbol_iter(Pr, object_name,
		PR_DYNSYM, BIND_WEAK|BIND_GLOBAL|TYPE_FUNC,
		symbol_iter, Dyp);

	/*
	 * Search the static symbol table if this is the
	 * executable file or if we are being asked to
	 * report internal calls within the library.
	 */
	if (object_name == PR_OBJ_EXEC || Dyp->internal)
		(void) Psymbol_iter(Pr, object_name,
			PR_SYMTAB, BIND_ANY|TYPE_FUNC,
			symbol_iter, Dyp);
}

static void
reset_libthread_db()
{
	if (tdb_handle)
		(void) dlclose(tdb_handle);
	tdb_handle = NULL;
	p_td_init = NULL;
	p_td_ta_new = NULL;
	p_td_ta_delete = NULL;
	p_td_ta_thr_iter = NULL;
	p_td_thr_get_info = NULL;
	p_td_thr_getgregs = NULL;
	p_td_ta_map_lwp2thr = NULL;
	has_libthread = FALSE;
}

static void
setup_libthread_db(struct ps_prochandle *Pr, const char *object_name)
{
	char *s1, *s2;
	char libthread_db[PATH_MAX];

	/*
	 * We found a libthread.
	 * dlopen() the matching libthread_db and get function pointers.
	 */
	if (tdb_handle)		/* been here already */
		return;

	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_NATIVE) {
		(void) strcpy(libthread_db, object_name);
		s1 = strstr(object_name, ".so.");
		s2 = strstr(libthread_db, ".so.");
		(void) strcpy(s2, "_db");
		s2 += 3;
		(void) strcpy(s2, s1);
	} else {
#ifdef _SYSCALL32
		/*
		 * The victim process is 32-bit, we are 64-bit.
		 * We have to find the 64-bit version of libthread_db
		 * that matches the victim's 32-bit version of libthread.
		 */
		(void) strcpy(libthread_db, object_name);
		s1 = strstr(object_name, "/libthread.so.");
		s2 = strstr(libthread_db, "/libthread.so.");
		(void) strcpy(s2, "/64");
		s2 += 3;
		(void) strcpy(s2, s1);
		s1 = strstr(s1, ".so.");
		s2 = strstr(s2, ".so.");
		(void) strcpy(s2, "_db");
		s2 += 3;
		(void) strcpy(s2, s1);
#else
		return (0);
#endif	/* _SYSCALL32 */
	}

	if ((tdb_handle = dlopen(libthread_db, RTLD_LAZY|RTLD_GLOBAL)) == NULL)
		return;

	has_libthread = TRUE;
	p_td_init = (p_td_init_t)
		dlsym(tdb_handle, "td_init");
	p_td_ta_new = (p_td_ta_new_t)
		dlsym(tdb_handle, "td_ta_new");
	p_td_ta_delete = (p_td_ta_delete_t)
		dlsym(tdb_handle, "td_ta_delete");
	p_td_ta_thr_iter = (p_td_ta_thr_iter_t)
		dlsym(tdb_handle, "td_ta_thr_iter");
	p_td_thr_get_info = (p_td_thr_get_info_t)
		dlsym(tdb_handle, "td_thr_get_info");
	p_td_thr_getgregs = (p_td_thr_getgregs_t)
		dlsym(tdb_handle, "td_thr_getgregs");
	p_td_ta_map_lwp2thr = (p_td_ta_map_lwp2thr_t)
		dlsym(tdb_handle, "td_ta_map_lwp2thr");

	if (p_td_init == NULL ||
	    p_td_ta_new == NULL ||
	    p_td_ta_delete == NULL ||
	    p_td_ta_thr_iter == NULL ||
	    p_td_thr_get_info == NULL ||
	    p_td_thr_getgregs == NULL ||
	    p_td_ta_map_lwp2thr == NULL)
		reset_libthread_db();
}

static int
object_iter(void *cd, const prmap_t *pmp, const char *object_name)
{
	char name[100];
	struct ps_prochandle *Pr = cd;
	struct dynpat *Dyp;
	struct dynlib *Dp;
	const char *str;
	char *s;
	int i;

	if ((pmp->pr_mflags & MA_WRITE) || !(pmp->pr_mflags & MA_EXEC))
		return (0);

	if (strstr(object_name, "/libthread.so.") != NULL)
		setup_libthread_db(Pr, object_name);

	for (Dp = Dyn; Dp != NULL; Dp = Dp->next)
		if (strcmp(object_name, Dp->lib_name) == 0 ||
		    (strcmp(Dp->lib_name, "a.out") == 0 &&
		    strcmp(pmp->pr_mapname, "a.out") == 0))
			break;

	if (Dp == NULL) {
		Dp = malloc(sizeof (struct dynlib));
		(void) memset(Dp, 0, sizeof (struct dynlib));
		if (strcmp(pmp->pr_mapname, "a.out") == 0) {
			Dp->lib_name = strdup(pmp->pr_mapname);
			Dp->match_name = strdup(pmp->pr_mapname);
			Dp->prt_name = strdup("");
		} else {
			Dp->lib_name = strdup(object_name);
			if ((str = strrchr(object_name, '/')) != NULL)
				str++;
			else
				str = object_name;
			(void) strncpy(name, str, sizeof (name) - 2);
			name[sizeof (name) - 2] = '\0';
			if ((s = strstr(name, ".so")) != NULL)
				*s = '\0';
			Dp->match_name = strdup(name);
			(void) strcat(name, ":");
			Dp->prt_name = strdup(name);
		}
		Dp->next = Dyn;
		Dyn = Dp;
	}

	if (Dp->built ||
	    (not_consist && strcmp(Dp->prt_name, "ld:") != 0))	/* kludge */
		return (0);

	if (hflag && not_consist)
		(void) fprintf(stderr, "not_consist is TRUE, building %s\n",
			Dp->lib_name);

	Dp->base = pmp->pr_vaddr;
	Dp->size = pmp->pr_size;

	/*
	 * For every dynlib pattern that matches this library's name,
	 * iterate through all of the library's symbols looking for
	 * matching symbol name patterns.
	 */
	for (Dyp = Dynpat; Dyp != NULL; Dyp = Dyp->next) {
		for (i = 0; i < Dyp->nlibpat; i++) {
			if (fnmatch(Dyp->libpat[i], Dp->match_name, 0) != 0)
				continue;	/* no match */

			/*
			 * Require an exact match for the executable (a.out)
			 * and for the dynamic linker (ld.so.1).
			 */
			if ((strcmp(Dp->match_name, "a.out") == 0 ||
			    strcmp(Dp->match_name, "ld") == 0) &&
			    strcmp(Dyp->libpat[i], Dp->match_name) != 0)
				continue;

			/*
			 * Set Dyp->Dp to Dp so symbol_iter() can use it.
			 */
			Dyp->Dp = Dp;
			do_symbol_iter(Pr, object_name, Dyp);
			Dyp->Dp = NULL;
		}
	}

	Dp->built = TRUE;
	return (0);
}

/*
 * Search for an existing breakpoint at the 'pc' location.
 */
static struct bkpt *
get_bkpt(uintptr_t pc)
{
	struct bkpt *Bp;

	for (Bp = bpt_hashtable[bpt_hash(pc)]; Bp != NULL; Bp = Bp->next)
		if (pc == Bp->addr)
			break;

	return (Bp);
}

/*
 * Create a breakpoint at 'pc', if one is not there already.
 * 'ret' is true when creating a function return breakpoint, in which case
 * fail and return NULL if the breakpoint would be created in writeable data.
 * If 'set' it true, set the breakpoint in the process now.
 */
static struct bkpt *
create_bkpt(struct ps_prochandle *Pr, uintptr_t pc, int ret, int set)
{
	uint_t hix = bpt_hash(pc);
	struct bkpt *Bp;
	const prmap_t *pmp;

	for (Bp = bpt_hashtable[hix]; Bp != NULL; Bp = Bp->next)
		if (pc == Bp->addr)
			return (Bp);

	/*
	 * Don't set return breakpoints on writeable data
	 * or on any space other than executable text.
	 */
	if (ret &&
	    ((pmp = Paddr_to_text_map(Pr, pc)) == NULL ||
	    !(pmp->pr_mflags & MA_EXEC) ||
	    (pmp->pr_mflags & MA_WRITE)))
		return (NULL);

	/* create a new unnamed breakpoint */
	Bp = malloc(sizeof (struct bkpt));
	Bp->sym_name = NULL;
	Bp->dyn = NULL;
	Bp->addr = pc;
	Bp->instr = 0;
	Bp->flags = 0;
	if (set && Psetbkpt(Pr, Bp->addr, &Bp->instr) == 0)
		Bp->flags |= BPT_ACTIVE;
	Bp->next = bpt_hashtable[hix];
	bpt_hashtable[hix] = Bp;

	return (Bp);
}

/*
 * Set all breakpoints that haven't been set yet.
 */
static void
set_deferred_brekpoints(struct ps_prochandle *Pr)
{
	struct bkpt *Bp;
	int i;

	for (i = 0; i < HASHSZ; i++) {
		for (Bp = bpt_hashtable[i]; Bp != NULL; Bp = Bp->next) {
			if (!(Bp->flags & BPT_ACTIVE) &&
			    !(Bp->flags & BPT_EXCLUDE) &&
			    Psetbkpt(Pr, Bp->addr, &Bp->instr) == 0)
				Bp->flags |= BPT_ACTIVE;
		}
	}
}

static int
symbol_iter(void *cd, const GElf_Sym *sym, const char *sym_name)
{
	struct dynpat *Dyp = cd;
	struct dynlib *Dp = Dyp->Dp;
	uintptr_t pc = sym->st_value;
	struct bkpt *Bp;
	int i;

	/*
	 * Arbitrarily omit "_start" from the executable.
	 * (Avoid indentation before main().)
	 */
	if (*Dp->prt_name == '\0' && strcmp(sym_name, "_start") == 0)
		return (0);

	/*
	 * Arbitrarily omit "_rt_boot" from the dynamic linker.
	 * (Avoid indentation before main().)
	 */
	if (strcmp(Dp->match_name, "ld") == 0 &&
	    strcmp(sym_name, "_rt_boot") == 0)
		return (0);

	/*
	 * For each pattern in the array of symbol patterns,
	 * if the pattern matches the symbol name, then
	 * create a breakpoint at the function in question.
	 */
	for (i = 0; i < Dyp->nsympat; i++) {
		if (fnmatch(Dyp->sympat[i], sym_name, 0) != 0)
			continue;

		if ((Bp = create_bkpt(Proc, pc, 0, 0)) == NULL)	/* can't fail */
			return (0);

		/*
		 * New breakpoints receive a name now.
		 * For existing breakpoints, prefer the subset name if possible,
		 * else prefer the shorter name.
		 */
		if (Bp->sym_name == NULL) {
			Bp->sym_name = strdup(sym_name);
		} else if (strstr(Bp->sym_name, sym_name) != NULL ||
		    strlen(Bp->sym_name) > strlen(sym_name)) {
			free(Bp->sym_name);
			Bp->sym_name = strdup(sym_name);
		}
		Bp->dyn = Dp;
		Bp->flags |= Dyp->flag;
		if (Dyp->exclude)
			Bp->flags |= BPT_EXCLUDE;
		else if (Dyp->internal || *Dp->prt_name == '\0')
			Bp->flags |= BPT_INTERNAL;
		return (0);
	}

	return (0);
}

/* For debugging only ---- */
void
report_htable_stats()
{
	const pstatus_t *Psp = Pstatus(Proc);
	struct callstack *Stk;
	struct bkpt *Bp;
	uint_t Min = 1000000;
	uint_t Max = 0;
	uint_t Avg = 0;
	uint_t Total = 0;
	uint_t i, j;
	uint_t bucket[HASHSZ];

	if (Dynpat == NULL || !hflag)
		return;

	(void) memset(bucket, 0, sizeof (bucket));

	for (i = 0; i < HASHSZ; i++) {
		j = 0;
		for (Bp = bpt_hashtable[i]; Bp != NULL; Bp = Bp->next)
			j++;
		if (j < Min)
			Min = j;
		if (j > Max)
			Max = j;
		if (j < HASHSZ)
			bucket[j]++;
		Total += j;
	}
	Avg = (Total + HASHSZ / 2) / HASHSZ;
	(void) fprintf(stderr, "truss hash table statistics --------\n");
	(void) fprintf(stderr, "    Total = %u\n", Total);
	(void) fprintf(stderr, "      Min = %u\n", Min);
	(void) fprintf(stderr, "      Max = %u\n", Max);
	(void) fprintf(stderr, "      Avg = %u\n", Avg);
	for (i = 0; i < HASHSZ; i++)
		if (bucket[i])
			(void) fprintf(stderr, "    %3u buckets of size %d\n",
				bucket[i], i);

	(void) fprintf(stderr, "truss-detected stacks --------\n");
	for (Stk = callstack; Stk != NULL; Stk = Stk->next) {
		(void) fprintf(stderr,
			"    base = 0x%.8lx  end = 0x%.8lx  size = %ld\n",
			(ulong_t)Stk->stkbase,
			(ulong_t)Stk->stkend,
			(ulong_t)(Stk->stkend - Stk->stkbase));
	}
	(void) fprintf(stderr, "primary unix stack --------\n");
	(void) fprintf(stderr,
		"    base = 0x%.8lx  end = 0x%.8lx  size = %ld\n",
		(ulong_t)Psp->pr_stkbase,
		(ulong_t)(Psp->pr_stkbase + Psp->pr_stksize),
		(ulong_t)Psp->pr_stksize);
}

static void
make_lwp_stack(struct ps_prochandle *Pr, const lwpstatus_t *Lsp,
	prmap_t *Pmap, int nmap)
{
	const pstatus_t *Psp = Pstatus(Pr);
	uintptr_t sp = Lsp->pr_reg[R_SP];
	id_t lwpid = Lsp->pr_lwpid;
	struct callstack *Stk;
	td_thrhandle_t th;
	td_thrinfo_t thrinfo;

	if (data_model != PR_MODEL_LP64)
		sp = (uint32_t)sp;

	/* check to see if we already have this stack */
	if (sp == 0)
		return;
	for (Stk = callstack; Stk != NULL; Stk = Stk->next)
		if (sp >= Stk->stkbase && sp < Stk->stkend)
			return;

	Stk = malloc(sizeof (struct callstack));
	Stk->next = callstack;
	callstack = Stk;
	nstack++;
	Stk->tref = 0;
	Stk->tid = 0;
	Stk->ncall = 0;
	Stk->maxcall = DEF_MAXCALL;
	Stk->stack = malloc(DEF_MAXCALL * sizeof (*Stk->stack));

	/* primary stack */
	if (sp >= Psp->pr_stkbase && sp < Psp->pr_stkbase + Psp->pr_stksize) {
		Stk->stkbase = Psp->pr_stkbase;
		Stk->stkend = Stk->stkbase + Psp->pr_stksize;
		return;
	}

	/* alternate stack */
	if ((Lsp->pr_altstack.ss_flags & SS_ONSTACK) &&
	    sp >= (uintptr_t)Lsp->pr_altstack.ss_sp &&
	    sp < (uintptr_t)Lsp->pr_altstack.ss_sp
	    + Psp->pr_lwp.pr_altstack.ss_size) {
		Stk->stkbase = (uintptr_t)Lsp->pr_altstack.ss_sp;
		Stk->stkend = Stk->stkbase + Lsp->pr_altstack.ss_size;
		return;
	}

	/* thread stacks? */
	if (has_libthread && Thr_agent == NULL) {
		if (p_td_init() != TD_OK ||
		    p_td_ta_new(Pr, &Thr_agent) != TD_OK) {
			has_libthread = FALSE;
			Thr_agent = NULL;
		}
	}
	if (Thr_agent != NULL &&
	    p_td_ta_map_lwp2thr(Thr_agent, lwpid, &th) == TD_OK &&
	    p_td_thr_get_info(&th, &thrinfo) == TD_OK &&
	    sp >= (uintptr_t)thrinfo.ti_stkbase - thrinfo.ti_stksize &&
	    sp < (uintptr_t)thrinfo.ti_stkbase) {
		/* The bloody fools got this backwards! */
		Stk->stkend = (uintptr_t)thrinfo.ti_stkbase;
		Stk->stkbase = Stk->stkend - thrinfo.ti_stksize;
		return;
	}

	/* last chance -- try the raw memory map */
	for (; nmap; nmap--, Pmap++) {
		if (sp >= Pmap->pr_vaddr &&
		    sp < Pmap->pr_vaddr + Pmap->pr_size) {
			Stk->stkbase = Pmap->pr_vaddr;
			Stk->stkend = Pmap->pr_vaddr + Pmap->pr_size;
			return;
		}
	}

	callstack = Stk->next;
	nstack--;
	free(Stk->stack);
	free(Stk);
}

static void
make_thr_stack(struct ps_prochandle *Pr,
	const td_thrhandle_t *Thp, prgregset_t reg)
{
	const pstatus_t *Psp = Pstatus(Pr);
	td_thrinfo_t thrinfo;
	uintptr_t sp = reg[R_SP];
	struct callstack *Stk;

	if (data_model != PR_MODEL_LP64)
		sp = (uint32_t)sp;

	/* check to see if we already have this stack */
	if (sp == 0)
		return;
	for (Stk = callstack; Stk != NULL; Stk = Stk->next)
		if (sp >= Stk->stkbase && sp < Stk->stkend)
			return;

	Stk = malloc(sizeof (struct callstack));
	Stk->next = callstack;
	callstack = Stk;
	nstack++;
	Stk->tref = 0;
	Stk->tid = 0;
	Stk->ncall = 0;
	Stk->maxcall = DEF_MAXCALL;
	Stk->stack = malloc(DEF_MAXCALL * sizeof (*Stk->stack));

	/* primary stack */
	if (sp >= Psp->pr_stkbase && sp < Psp->pr_stkbase + Psp->pr_stksize) {
		Stk->stkbase = Psp->pr_stkbase;
		Stk->stkend = Stk->stkbase + Psp->pr_stksize;
		return;
	}

	if (p_td_thr_get_info(Thp, &thrinfo) == TD_OK &&
	    sp >= (uintptr_t)thrinfo.ti_stkbase - thrinfo.ti_stksize &&
	    sp < (uintptr_t)thrinfo.ti_stkbase) {
		/* The bloody fools got this backwards! */
		Stk->stkend = (uintptr_t)thrinfo.ti_stkbase;
		Stk->stkbase = Stk->stkend - thrinfo.ti_stksize;
		return;
	}

	callstack = Stk->next;
	nstack--;
	free(Stk->stack);
	free(Stk);
}

static struct callstack *
find_lwp_stack(struct ps_prochandle *Pr, uintptr_t sp)
{
	const pstatus_t *Psp = Pstatus(Pr);
	char mapfile[64];
	int mapfd;
	struct stat statb;
	prmap_t *Pmap = NULL;
	prmap_t *pmap = NULL;
	int nmap = 0;
	struct callstack *Stk = NULL;

	/*
	 * Get the address space map.
	 */
	(void) sprintf(mapfile, "/proc/%d/rmap", (int)Psp->pr_pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0 ||
	    statb.st_size < sizeof (prmap_t) ||
	    (Pmap = malloc(statb.st_size)) == NULL ||
	    (nmap = pread(mapfd, Pmap, statb.st_size, 0L)) <= 0 ||
	    (nmap /= sizeof (prmap_t)) == 0) {
		if (Pmap != NULL)
			free(Pmap);
		if (mapfd >= 0)
			(void) close(mapfd);
		return (NULL);
	}
	(void) close(mapfd);

	for (pmap = Pmap; nmap--; pmap++) {
		if (sp >= pmap->pr_vaddr &&
		    sp < pmap->pr_vaddr + pmap->pr_size) {
			Stk = malloc(sizeof (struct callstack));
			Stk->next = callstack;
			callstack = Stk;
			nstack++;
			Stk->stkbase = pmap->pr_vaddr;
			Stk->stkend = pmap->pr_vaddr + pmap->pr_size;
			Stk->tref = 0;
			Stk->tid = 0;
			Stk->ncall = 0;
			Stk->maxcall = DEF_MAXCALL;
			Stk->stack = malloc(DEF_MAXCALL * sizeof (*Stk->stack));
			break;
		}
	}

	free(Pmap);
	return (Stk);
}

static struct callstack *
find_stack(struct ps_prochandle *Pr, uintptr_t sp)
{
	const pstatus_t *Psp = Pstatus(Pr);
	id_t lwpid = Psp->pr_lwp.pr_lwpid;
#if defined(sparc) || defined(__sparc)
	prgreg_t tref = has_libthread? Psp->pr_lwp.pr_reg[R_G7] : lwpid;
#elif defined(i386) || defined(__i386) || defined(__ia64)
	prgreg_t tref = has_libthread? Psp->pr_lwp.pr_reg[GS] : lwpid;
#endif
	struct callstack *Stk = NULL;
	td_thrhandle_t th;
	td_thrinfo_t thrinfo;
	td_err_e error;

	/* primary stack */
	if (sp >= Psp->pr_stkbase && sp < Psp->pr_stkbase + Psp->pr_stksize) {
		Stk = malloc(sizeof (struct callstack));
		Stk->next = callstack;
		callstack = Stk;
		nstack++;
		Stk->stkbase = Psp->pr_stkbase;
		Stk->stkend = Stk->stkbase + Psp->pr_stksize;
		Stk->tref = 0;
		Stk->tid = 0;
		Stk->ncall = 0;
		Stk->maxcall = DEF_MAXCALL;
		Stk->stack = malloc(DEF_MAXCALL * sizeof (*Stk->stack));
		return (Stk);
	}

	/* alternate stack */
	if ((Psp->pr_lwp.pr_altstack.ss_flags & SS_ONSTACK) &&
	    sp >= (uintptr_t)Psp->pr_lwp.pr_altstack.ss_sp &&
	    sp < (uintptr_t)Psp->pr_lwp.pr_altstack.ss_sp
	    + Psp->pr_lwp.pr_altstack.ss_size) {
		Stk = malloc(sizeof (struct callstack));
		Stk->next = callstack;
		callstack = Stk;
		nstack++;
		Stk->stkbase = (uintptr_t)Psp->pr_lwp.pr_altstack.ss_sp;
		Stk->stkend = Stk->stkbase +
			Psp->pr_lwp.pr_altstack.ss_size;
		Stk->tref = 0;
		Stk->tid = 0;
		Stk->ncall = 0;
		Stk->maxcall = DEF_MAXCALL;
		Stk->stack = malloc(DEF_MAXCALL * sizeof (*Stk->stack));
		return (Stk);
	}

	/* thread stacks? */
	if (has_libthread && Thr_agent == NULL) {
		if ((error = p_td_init()) != TD_OK) {
			(void) fprintf(stderr,
				"td_init() failed with %d\n", error);
			has_libthread = FALSE;
			Thr_agent = NULL;
		} else if ((error = p_td_ta_new(Pr, &Thr_agent)) != TD_OK) {
			(void) fprintf(stderr,
				"td_ta_new() failed with %d\n", error);
			has_libthread = FALSE;
			Thr_agent = NULL;
		}
	}
	if (Thr_agent == NULL)
		return (find_lwp_stack(Pr, sp));

	if ((error = p_td_ta_map_lwp2thr(Thr_agent, lwpid, &th)) != TD_OK) {
		if (hflag)
			(void) fprintf(stderr,
				"cannot get thread handle for "
				"lwp#%d, error=%d, tref=0x%.8lx\n",
				(int)lwpid, error, (long)tref);
		return (NULL);
	}

	if ((error = p_td_thr_get_info(&th, &thrinfo)) != TD_OK) {
		if (hflag)
			(void) fprintf(stderr,
				"cannot get thread info for "
				"lwp#%d, error=%d, tref=0x%.8lx\n",
				(int)lwpid, error, (long)tref);
		return (NULL);
	}

	if (sp >= (uintptr_t)thrinfo.ti_stkbase - thrinfo.ti_stksize &&
	    sp < (uintptr_t)thrinfo.ti_stkbase) {
		Stk = malloc(sizeof (struct callstack));
		Stk->next = callstack;
		callstack = Stk;
		nstack++;
		/* The bloody fools got this backwards! */
		Stk->stkend = (uintptr_t)thrinfo.ti_stkbase;
		Stk->stkbase = Stk->stkend - thrinfo.ti_stksize;
		Stk->tref = tref;
		Stk->tid = thrinfo.ti_tid;
		Stk->ncall = 0;
		Stk->maxcall = DEF_MAXCALL;
		Stk->stack = malloc(DEF_MAXCALL * sizeof (*Stk->stack));
		return (Stk);
	}

	/* stack bounds failure -- complain bitterly */
	if (hflag) {
		(void) fprintf(stderr,
			"sp not within thread stack: "
			"sp=0x%.8lx stkbase=0x%.8lx stkend=0x%.8lx\n",
			(ulong_t)sp,
			(ulong_t)thrinfo.ti_stkbase,
			(ulong_t)thrinfo.ti_stkbase + thrinfo.ti_stksize);
	}

	return (NULL);
}

static void
get_tid(struct ps_prochandle *Pr, struct callstack *Stk)
{
	const pstatus_t *Psp = Pstatus(Pr);
	id_t lwpid = Psp->pr_lwp.pr_lwpid;
#if defined(sparc) || defined(__sparc)
	prgreg_t tref = has_libthread? Psp->pr_lwp.pr_reg[R_G7] : lwpid;
#elif defined(i386) || defined(__i386) || defined(__ia64)
	prgreg_t tref = has_libthread? Psp->pr_lwp.pr_reg[GS] : lwpid;
#endif
	td_thrhandle_t th;
	td_thrinfo_t thrinfo;
	td_err_e error;

	if (has_libthread && Thr_agent == NULL &&
	    (p_td_init() != TD_OK || p_td_ta_new(Pr, &Thr_agent) != TD_OK)) {
		has_libthread = FALSE;
		Thr_agent = NULL;
	}
	if (Thr_agent == NULL) {
		Stk->tref = tref;
		Stk->tid = lwpid;
	}
	if (tref == Stk->tref)
		return;

	Stk->tref = tref;
	Stk->tid = lwpid;

	if ((error = p_td_ta_map_lwp2thr(Thr_agent, lwpid, &th)) != TD_OK) {
		if (hflag)
			(void) fprintf(stderr,
				"cannot get thread handle for "
				"lwp#%d, error=%d, tref=0x%.8lx\n",
				(int)lwpid, error, (long)tref);
	} else if ((error = p_td_thr_get_info(&th, &thrinfo)) != TD_OK) {
		if (hflag)
			(void) fprintf(stderr,
				"cannot get thread info for "
				"lwp#%d, error=%d, tref=0x%.8lx\n",
				(int)lwpid, error, (long)tref);
	} else {
		Stk->tid = thrinfo.ti_tid;
	}
}

static struct callstack *
callstack_info(struct ps_prochandle *Pr, uintptr_t sp, uintptr_t fp, int makeid)
{
	struct callstack *Stk;
	uintptr_t trash;

	if (sp == 0 ||
	    Pread(Pr, &trash, sizeof (trash), sp) != sizeof (trash))
		return (NULL);

	for (Stk = callstack; Stk != NULL; Stk = Stk->next)
		if (sp >= Stk->stkbase && sp < Stk->stkend)
			break;

	/*
	 * If we didn't find the stack, do it the hard way.
	 */
	if (Stk == NULL) {
		uintptr_t stkbase = sp;
		uintptr_t stkend;
		uint_t minsize;

#if defined(i386) || defined(__ia64)
		minsize = 2 * sizeof (uintptr_t);	/* fp + pc */
#else
#ifdef _LP64
		if (data_model != PR_MODEL_LP64)
			minsize = SA32(MINFRAME32);
		else
			minsize = SA64(MINFRAME64);
#else
		minsize = SA(MINFRAME);
#endif
#endif	/* i386 */
		stkend = sp + minsize;

		while (Stk == NULL && fp != 0 && fp >= sp) {
			stkend = fp + minsize;
			for (Stk = callstack; Stk != NULL; Stk = Stk->next)
				if ((fp >= Stk->stkbase && fp < Stk->stkend) ||
				    (stkend > Stk->stkbase &&
				    stkend <= Stk->stkend))
					break;
			if (Stk == NULL)
				fp = previous_fp(Pr, fp, NULL);
		}

		if (Stk != NULL)	/* the stack grew */
			Stk->stkbase = stkbase;
	}

	if (Stk == NULL && makeid)	/* new stack */
		Stk = find_stack(Pr, sp);

	if (Stk == NULL)
		return (NULL);

	/*
	 * Ensure that there is room for at least one more entry.
	 */
	if (Stk->ncall == Stk->maxcall) {
		Stk->maxcall *= 2;
		Stk->stack = realloc(Stk->stack,
		    Stk->maxcall * sizeof (*Stk->stack));
	}

	if (makeid)
		get_tid(Pr, Stk);

	return (Stk);
}

/*
 * Reset the breakpoint information (called on successful exec()).
 */
void
reset_breakpoints(struct ps_prochandle *Pr)
{
	struct dynlib *Dp;
	struct bkpt *Bp;
	struct callstack *Stk;
	int i;

	if (Dynpat == NULL)
		return;

	/* destroy all previous dynamic library information */
	while ((Dp = Dyn) != NULL) {
		Dyn = Dp->next;
		free(Dp->lib_name);
		free(Dp->match_name);
		free(Dp->prt_name);
		free(Dp);
	}

	/* destroy all previous breakpoint trap information */
	if (bpt_hashtable != NULL) {
		for (i = 0; i < HASHSZ; i++) {
			while ((Bp = bpt_hashtable[i]) != NULL) {
				bpt_hashtable[i] = Bp->next;
				if (Bp->sym_name)
					free(Bp->sym_name);
				free(Bp);
			}
		}
	}

	/* destroy all the callstack information */
	while ((Stk = callstack) != NULL) {
		callstack = Stk->next;
		free(Stk->stack);
		free(Stk);
	}

	/* we are not a multi-threaded process anymore */
	if (Thr_agent != NULL)
		(void) p_td_ta_delete(Thr_agent);
	Thr_agent = NULL;
	reset_libthread_db();

	/* tell libproc to clear out its mapping information */
	Preset_maps(Pr);
	Rdb_agent = NULL;

	/* Reestablish the symbols from the executable */
	(void) establish_breakpoints(Pr);
}

/*
 * Clear breakpoints from the process (called before Prelease()).
 */
void
clear_breakpoints(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	struct bkpt *Bp;
	int i;

	if (Dynpat == NULL)
		return;

	/* Get all lwps quietly out of the breakpointed state */
	(void) Pstop(Pr, 1000);
	while (Pstate(Pr) == PS_STOP &&
	    Psp->pr_lwp.pr_why == PR_FAULTED &&
	    Psp->pr_lwp.pr_what == FLTBPT &&
	    function_trace(Pr, 0, 1) == 0) {
		(void) Psetrun(Pr, 0, PRCFAULT|PRSTOP);
		(void) Pwait(Pr, 0);
	}

	/* Change all breakpoint traps back to normal instructions */
	report_htable_stats();	/* report stats first */
	for (i = 0; i < HASHSZ; i++) {
		while ((Bp = bpt_hashtable[i]) != NULL) {
			bpt_hashtable[i] = Bp->next;
			if (Bp->flags & BPT_ACTIVE)
				(void) Pdelbkpt(Pr, Bp->addr, Bp->instr);
			if (Bp->sym_name)
				free(Bp->sym_name);
			free(Bp);
		}
	}

	if (Thr_agent != NULL)
		(void) p_td_ta_delete(Thr_agent);
	Thr_agent = NULL;
}

/*
 * Reestablish the breakpoint traps in the process.
 * Called after resuming from a vfork() in the parent.
 */
void
reestablish_traps(struct ps_prochandle *Pr)
{
	struct bkpt *Bp;
	ulong_t instr;
	int i;

	if (Dynpat == NULL)
		return;

	for (i = 0; i < HASHSZ; i++) {
		for (Bp = bpt_hashtable[i]; Bp != NULL; Bp = Bp->next) {
			if ((Bp->flags & BPT_ACTIVE) &&
			    Psetbkpt(Pr, Bp->addr, &instr) != 0)
				Bp->flags &= ~BPT_ACTIVE;
		}
	}
}

static void
show_function_call(struct ps_prochandle *Pr,
	struct callstack *Stk, struct dynlib *Dp, struct bkpt *Bp)
{
	const pstatus_t *Psp = Pstatus(Pr);
	long arg[8];
	int narg;
	int i;

	narg = get_arguments(Pr, arg);
	if (Psp->pr_nlwp > 1 && Stk != NULL)
		make_pname(Pr, Stk->tid);
	putpname();
	timestamp(Psp);
	if (Stk != NULL) {
		for (i = 1; i < Stk->ncall; i++) {
			(void) fputc(' ', stdout);
			(void) fputc(' ', stdout);
		}
	}
	(void) printf("-> %s%s(", Dp->prt_name, Bp->sym_name);
	for (i = 0; i < narg; i++) {
		(void) printf("0x%lx", arg[i]);
		if (i < narg-1) {
			(void) fputc(',', stdout);
			(void) fputc(' ', stdout);
		}
	}
	(void) printf(")\n");
	Flush();
}

/* ARGSUSED */
static void
show_function_return(struct ps_prochandle *Pr, long rval, int stret,
	struct callstack *Stk, struct dynlib *Dp, struct bkpt *Bp)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int i;

	if (Psp->pr_nlwp > 1)
		make_pname(Pr, Stk->tid);
	putpname();
	timestamp(Psp);
	for (i = 0; i < Stk->ncall; i++) {
		(void) fputc(' ', stdout);
		(void) fputc(' ', stdout);
	}
	(void) printf("<- %s%s() = ", Dp->prt_name, Bp->sym_name);
	if (stret) {
		(void) printf("struct return\n");
	} else if (data_model == PR_MODEL_LP64) {
		if (rval >= (64 * 1024) || -rval >= (64 * 1024))
			(void) printf("0x%lx\n", rval);
		else
			(void) printf("%ld\n", rval);
	} else {
		int rval32 = (int)rval;
		if (rval32 >= (64 * 1024) || -rval32 >= (64 * 1024))
			(void) printf("0x%x\n", rval32);
		else
			(void) printf("%d\n", rval32);
	}
	Flush();
}

/*
 * Called to deal with function-call tracing.
 * Return 0 on normal success, 1 to indicate a BPT_HANG success,
 * and -1 on failure (not tracing functions or unknown breakpoint).
 */
int
function_trace(struct ps_prochandle *Pr, int first, int clear)
{
	const pstatus_t *Psp = Pstatus(Pr);
	uintptr_t pc = Psp->pr_lwp.pr_reg[R_PC];
	uintptr_t sp = Psp->pr_lwp.pr_reg[R_SP];
	uintptr_t fp = Psp->pr_lwp.pr_reg[R_FP];
	struct bkpt *Bp;
	struct dynlib *Dp;
	struct callstack *Stk;
	int rval = 0;

	if (Dynpat == NULL)
		return (-1);

	if (data_model != PR_MODEL_LP64) {
		pc = (uint32_t)pc;
		sp = (uint32_t)sp;
		fp = (uint32_t)fp;
	}

	if ((Bp = get_bkpt(pc)) == NULL) {
		if (hflag)
			(void) fprintf(stderr,
				"function_trace(): "
				"cannot find breakpoint for pc: 0x%.8lx\n",
				(ulong_t)pc);
		return (-1);
	}

	if ((Bp->flags & (BPT_PREINIT|BPT_POSTINIT|BPT_DLACTIVITY)) && !clear) {
		rd_event_msg_t event_msg;

		if (hflag) {
			if (Bp->flags & BPT_PREINIT)
				(void) fprintf(stderr, "function_trace(): "
					"rtld_db RD_PREINIT breakpoint\n");
			if (Bp->flags & BPT_POSTINIT)
				(void) fprintf(stderr, "function_trace(): "
					"rtld_db RD_POSTINIT breakpoint\n");
			if (Bp->flags & BPT_DLACTIVITY)
				(void) fprintf(stderr, "function_trace(): "
					"rtld_db RD_DLACTIVITY breakpoint\n");
		}
		if (rd_event_getmsg(Rdb_agent, &event_msg) == RD_OK) {
			if (event_msg.type == RD_DLACTIVITY) {
				if (event_msg.u.state == RD_CONSISTENT)
					establish_breakpoints(Pr);
				if (event_msg.u.state == RD_ADD) {
					if (hflag)
						(void) fprintf(stderr,
							"RD_DLACTIVITY/RD_ADD "
							"state reached\n");
					not_consist = TRUE;	/* kludge */
					establish_breakpoints(Pr);
					not_consist = FALSE;
				}
			}
			if (hflag) {
				char *et;
				char buf[32];

				switch (event_msg.type) {
				case RD_NONE:
					et = "RD_NONE";
					break;
				case RD_PREINIT:
					et = "RD_PREINIT";
					break;
				case RD_POSTINIT:
					et = "RD_POSTINIT";
					break;
				case RD_DLACTIVITY:
					et = "RD_DLACTIVITY";
					break;
				default:
					(void) sprintf(et = buf, "0x%x",
						event_msg.type);
					break;
				}
				(void) fprintf(stderr,
					"event_msg.type = %s ", et);
				switch (event_msg.u.state) {
				case RD_NOSTATE:
					et = "RD_NOSTATE";
					break;
				case RD_CONSISTENT:
					et = "RD_CONSISTENT";
					break;
				case RD_ADD:
					et = "RD_ADD";
					break;
				case RD_DELETE:
					et = "RD_DELETE";
					break;
				default:
					(void) sprintf(et = buf, "0x%x",
						event_msg.u.state);
					break;
				}
				(void) fprintf(stderr,
					"event_msg.u.state = %s\n", et);
			}
		}
	}

	Dp = Bp->dyn;

	if ((Stk = callstack_info(Pr, sp, fp, 1)) == NULL) {
		if (Dp != NULL && !clear) {
			show_function_call(Pr, NULL, Dp, Bp);
			if ((Bp->flags & BPT_HANG) && !first)
				rval = 1;
		}
	} else if (!clear) {
		if (Dp != NULL) {
			function_entry(Pr, Bp, Stk);
			if ((Bp->flags & BPT_HANG) && !first)
				rval = 1;
		} else {
			function_return(Pr, Stk);
		}
	}

	(void) Pxecbkpt(Pr, Bp->instr);
	if (rval || clear) {	/* leave process stopped and abandoned */
#if defined(__i386)
		/*
		 * Leave it stopped in a state that a stack trace is reasonable.
		 */
		if (rval && Bp->instr == 0x55) {	/* pushl %ebp */
			/* step it over the movl %esp,%ebp */
			(void) Psetrun(Pr, 0, PRCFAULT|PRSTEP);
			(void) Pwait(Pr, 0);
		}
#endif
		if (Bp->flags & BPT_ACTIVE)
			(void) Pdelbkpt(Pr, Bp->addr, Bp->instr);
		Bp->flags &= ~BPT_ACTIVE;
		(void) Psetrun(Pr, 0, PRCFAULT|PRSTOP);
		(void) Pwait(Pr, 0);
	}
	return (rval);
}

static void
function_entry(struct ps_prochandle *Pr, struct bkpt *Bp, struct callstack *Stk)
{
	const pstatus_t *Psp = Pstatus(Pr);
	uintptr_t sp = Psp->pr_lwp.pr_reg[R_SP];
	uintptr_t rpc = get_return_address(Pr, &sp);
	struct dynlib *Dp = Bp->dyn;
	int oldframe = FALSE;
	int i;

#ifdef _LP64
	if (data_model != PR_MODEL_LP64) {
		sp = (uint32_t)sp;
		rpc = (uint32_t)rpc;
	}
#endif

	/*
	 * If the sp is not within the stack bounds, forget it.
	 * If the symbol's 'internal' flag is false,
	 * don't report internal calls within the library.
	 */
	if (!(sp >= Stk->stkbase && sp < Stk->stkend) ||
	    (!(Bp->flags & BPT_INTERNAL) &&
	    rpc >= Dp->base && rpc < Dp->base + Dp->size))
		return;

	for (i = 0; i < Stk->ncall; i++) {
		if (sp >= Stk->stack[i].sp) {
			Stk->ncall = i;
			if (sp == Stk->stack[i].sp)
				oldframe = TRUE;
			break;
		}
	}

	if (!oldframe) {
		(void) create_bkpt(Pr, rpc, 1, 1); /* may or may not be set */
		Stk->stack[Stk->ncall].sp = sp;	/* record it anyeay */
		Stk->stack[Stk->ncall].pc = rpc;
		Stk->stack[Stk->ncall].fcn = Bp;
	}
	Stk->ncall++;

	show_function_call(Pr, Stk, Dp, Bp);
}

/*
 * We are here because we hit an unnamed breakpoint.
 * Attempt to match this up with a return pc on the stack
 * and report the function return.
 */
static void
function_return(struct ps_prochandle *Pr, struct callstack *Stk)
{
	const pstatus_t *Psp = Pstatus(Pr);
	uintptr_t sp = Psp->pr_lwp.pr_reg[R_SP];
	uintptr_t fp = Psp->pr_lwp.pr_reg[R_FP];
	int i;

#ifdef _LP64
	if (data_model != PR_MODEL_LP64) {
		sp = (uint32_t)sp;
		fp = (uint32_t)fp;
	}
#endif

	if (fp < sp + 8)
		fp = sp + 8;

	for (i = Stk->ncall - 1; i >= 0; i--) {
		if (sp <= Stk->stack[i].sp && fp > Stk->stack[i].sp) {
			Stk->ncall = i;
			break;
		}
	}

#if defined(i386) || defined(__ia64)
	if (i < 0) {
		/* probably __mul64() or friends -- try harder */
		int j;
		for (j = 0; i < 0 && j < 8; j++) {	/* up to 8 args */
			sp -= 4;
			for (i = Stk->ncall - 1; i >= 0; i--) {
				if (sp <= Stk->stack[i].sp &&
				    fp > Stk->stack[i].sp) {
					Stk->ncall = i;
					break;
				}
			}
		}
	}
#endif

	if (i >= 0) {
		show_function_return(Pr, Psp->pr_lwp.pr_reg[R_R0], 0,
			Stk, Stk->stack[i].fcn->dyn, Stk->stack[i].fcn);
	}
}

#if defined(sparc) || defined(__sparc)
#define	FPADJUST	0
#elif defined(i386) || defined(__i386) || defined(__ia64)
#define	FPADJUST	4
#endif

static void
trap_one_stack(struct ps_prochandle *Pr, prgregset_t reg)
{
	struct dynlib *Dp;
	struct bkpt *Bp;
	struct callstack *Stk;
	GElf_Sym sym;
	char sym_name[32];
	uintptr_t sp = reg[R_SP];
	uintptr_t pc = reg[R_PC];
	uintptr_t fp;
	uintptr_t rpc;
	uint_t nframe = 0;
	uint_t maxframe = 8;
	struct {
		uintptr_t sp;		/* %sp within called function */
		uintptr_t pc;		/* %pc within called function */
		uintptr_t rsp;		/* the return sp */
		uintptr_t rpc;		/* the return pc */
	} *frame = malloc(maxframe * sizeof (*frame));

	/*
	 * Gather stack frames bottom to top.
	 */
	while (sp != 0) {
		fp = sp;	/* remember higest non-null sp */
		frame[nframe].sp = sp;
		frame[nframe].pc = pc;
		sp = previous_fp(Pr, sp, &pc);
		frame[nframe].rsp = sp;
		frame[nframe].rpc = pc;
		if (++nframe == maxframe) {
			maxframe *= 2;
			frame = realloc(frame, maxframe * sizeof (*frame));
		}
	}

	/*
	 * Scan for function return breakpoints top to bottom.
	 */
	while (nframe--) {
		/* lookup the called function in the symbol tables */
		if (Plookup_by_addr(Pr, frame[nframe].pc, sym_name,
		    sizeof (sym_name), &sym) != 0)
			continue;

		pc = sym.st_value;	/* entry point of the function */
		rpc = frame[nframe].rpc;	/* caller's return pc */

		/* lookup the function in the breakpoint table */
		if ((Bp = get_bkpt(pc)) == NULL || (Dp = Bp->dyn) == NULL)
			continue;

		if (!(Bp->flags & BPT_INTERNAL) &&
		    rpc >= Dp->base && rpc < Dp->base + Dp->size)
			continue;

		sp = frame[nframe].rsp + FPADJUST;  /* %sp at time of call */
		if ((Stk = callstack_info(Pr, sp, fp, 0)) == NULL)
			continue;	/* can't happen? */

		if (create_bkpt(Pr, rpc, 1, 1) != NULL) {
			Stk->stack[Stk->ncall].sp = sp;
			Stk->stack[Stk->ncall].pc = rpc;
			Stk->stack[Stk->ncall].fcn = Bp;
			Stk->ncall++;
		}
	}

	free(frame);
}

static int
lwp_stack_traps(void *cd, const lwpstatus_t *Lsp)
{
	ph_map_t *ph_map = (ph_map_t *)cd;
	struct ps_prochandle *Pr = ph_map->ph;
	prgregset_t reg;

	(void) memcpy(reg, Lsp->pr_reg, sizeof (prgregset_t));
	make_lwp_stack(Pr, Lsp, ph_map->pmap, ph_map->nmap);
	trap_one_stack(Pr, reg);

	return (0);
}

static int
thr_stack_traps(const td_thrhandle_t *Thp, void *cd)
{
	struct ps_prochandle *Pr = (struct ps_prochandle *)cd;
	prgregset_t reg;

	/*
	 * We have already dealt with all the lwps.
	 * We only care about unbound threads here (TD_PARTIALREG).
	 */
	if (p_td_thr_getgregs(Thp, reg) != TD_PARTIALREG)
		return (0);

	make_thr_stack(Pr, Thp, reg);
	trap_one_stack(Pr, reg);

	return (0);
}

#ifdef sparc

static uintptr_t
previous_fp(struct ps_prochandle *Pr, uintptr_t sp, uintptr_t *rpc)
{
	uintptr_t fp = 0;
	uintptr_t pc = 0;

#ifdef _LP64
	if (data_model == PR_MODEL_LP64) {
		struct rwindow64 rwin;
		if (Pread(Pr, &rwin, sizeof (rwin), sp + STACK_BIAS)
		    == sizeof (rwin)) {
			fp = (uintptr_t)rwin.rw_fp;
			pc = (uintptr_t)rwin.rw_rtn;
		}
		if (fp != 0 &&
		    Pread(Pr, &rwin, sizeof (rwin), fp + STACK_BIAS)
		    != sizeof (rwin))
			fp = pc = 0;
	} else {
		struct rwindow32 rwin;
#else	/* _LP64 */
		struct rwindow rwin;
#endif	/* _LP64 */
		if (Pread(Pr, &rwin, sizeof (rwin), sp) == sizeof (rwin)) {
			fp = (uint32_t)rwin.rw_fp;
			pc = (uint32_t)rwin.rw_rtn;
		}
		if (fp != 0 &&
		    Pread(Pr, &rwin, sizeof (rwin), fp) != sizeof (rwin))
			fp = pc = 0;
#ifdef _LP64
	}
#endif
	if (rpc)
		*rpc = pc;
	return (fp);
}

/* ARGSUSED1 */
static uintptr_t
get_return_address(struct ps_prochandle *Pr, uintptr_t *psp)
{
	instr_t inst;
	const pstatus_t *Psp = Pstatus(Pr);
	uintptr_t rpc;

	rpc = (uintptr_t)Psp->pr_lwp.pr_reg[R_O7] + 8;
	if (data_model != PR_MODEL_LP64)
		rpc = (uint32_t)rpc;

	/* check for structure return (bletch!) */
	if (Pread(Pr, &inst, sizeof (inst), rpc) == sizeof (inst) &&
	    inst < 0x1000)
		rpc += sizeof (instr_t);

	return (rpc);
}

static int
get_arguments(struct ps_prochandle *Pr, long *argp)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int i;

	if (data_model != PR_MODEL_LP64)
		for (i = 0; i < 4; i++)
			argp[i] = (uint_t)Psp->pr_lwp.pr_reg[R_O0+i];
	else
		for (i = 0; i < 4; i++)
			argp[i] = (long)Psp->pr_lwp.pr_reg[R_O0+i];
	return (4);
}

#endif	/* sparc */

#ifdef i386

static uintptr_t
previous_fp(struct ps_prochandle *Pr, uintptr_t fp, uintptr_t *rpc)
{
	uintptr_t frame[2];
	uintptr_t trash[2];

	if (Pread(Pr, frame, sizeof (frame), fp) != sizeof (frame) ||
	    (frame[0] != 0 &&
	    Pread(Pr, trash, sizeof (trash), frame[0]) != sizeof (trash)))
		frame[0] = frame[1] = 0;

	if (rpc)
		*rpc = frame[1];
	return (frame[0]);
}

/*
 * Examine the instruction at the return location of a function call
 * and return the byte count by which the stack is adjusted on return.
 * It the instruction at the return location is an addl, as expected,
 * then adjust the return pc by the size of that instruction so that
 * we will place the return breakpoint on the following instruction.
 * This allows programs that interrogate their own stacks and record
 * function calls and arguments to work correctly even while we interfere.
 * Return the count on success, -1 on failure.
 */
static int
return_count(struct ps_prochandle *Pr, uintptr_t *ppc)
{
	uintptr_t pc = *ppc;
	struct bkpt *Bp;
	int count;
	uchar_t instr[6];	/* instruction at pc */

	if ((count = Pread(Pr, instr, sizeof (instr), pc)) < 0)
		return (-1);

	/* find the replaced instruction at pc (if any) */
	if ((Bp = get_bkpt(pc)) != NULL && (Bp->flags & BPT_ACTIVE))
		instr[0] = (uchar_t)Bp->instr;

	if (count != sizeof (instr) &&
	    (count < 3 || instr[0] != 0x83))
		return (-1);

	/*
	 * A bit of disassembly of the instruction is required here.
	 */
	if (instr[1] != 0xc4) {	/* not an addl mumble,%esp inctruction */
		count = 0;
	} else if (instr[0] == 0x81) {	/* count is a longword */
		count = instr[2]+(instr[3]<<8)+(instr[4]<<16)+(instr[5]<<24);
		*ppc += 6;
	} else if (instr[0] == 0x83) {	/* count is a byte */
		count = instr[2];
		*ppc += 3;
	} else {		/* not an addl inctruction */
		count = 0;
	}

	return (count);
}

static uintptr_t
get_return_address(struct ps_prochandle *Pr, uintptr_t *psp)
{
	uintptr_t sp = *psp;
	uintptr_t rpc;
	int count;

	*psp += 4;	/* account for popping the stack on return */
	if (Pread(Pr, &rpc, sizeof (rpc), sp) != sizeof (rpc))
		return (0);
	if ((count = return_count(Pr, &rpc)) < 0)
		count = 0;
	*psp += count;		/* expected sp on return */
	return (rpc);
}

static int
get_arguments(struct ps_prochandle *Pr, long *argp)
{
	uintptr_t frame[5];	/* return pc + 4 args */
	int narg;
	int count;
	int i;

	narg = Pread(Pr, frame, sizeof (frame),
		(uintptr_t)Pstatus(Pr)->pr_lwp.pr_reg[R_SP]);
	narg -= sizeof (long);
	if (narg <= 0)
		return (0);
	narg /= sizeof (long);	/* no more than 4 */

	/*
	 * Given the return PC, determine the number of arguments.
	 */
	if ((count = return_count(Pr, &frame[0])) < 0)
		narg = 0;
	else {
		count /= sizeof (long);
		if (narg > count)
			narg = count;
	}

	for (i = 0; i < narg; i++)
		argp[i] = frame[i+1];

	return (narg);
}

#endif	/* i386 */
