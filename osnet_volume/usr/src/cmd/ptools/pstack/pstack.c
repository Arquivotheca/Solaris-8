/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pstack.c	1.16	99/10/14 SMI"

#include <sys/isa_defs.h>

#ifdef _LP64
#define	_SYSCALL32
#endif

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stack.h>
#include <link.h>
#include <limits.h>
#include <libelf.h>
#include <thread_db.h>
#include <libproc.h>

static	char	*command;
static	int	Fflag;
static	int	is64;
static	GElf_Sym sigh;

/*
 * To keep the list of user-level threads for a multithreaded process.
 */
struct threadinfo {
	struct threadinfo *next;
	id_t	threadid;
	id_t	lwpid;
	prgregset_t regs;
};

static struct threadinfo *thr_head, *thr_tail;

#define	TRUE	1
#define	FALSE	0

#define	MAX_ARGS	8

static	int	object_iter(void *, const prmap_t *, const char *);
static	void	reset_libthread_db(void);
static	int	thr_stack(const td_thrhandle_t *, void *);
static	void	free_threadinfo(void);
static	id_t	find_thread(id_t);
static	int	AllCallStacks(struct ps_prochandle *, int);
static	void	tlhead(id_t, id_t);
static	int	PrintFrame(void *, const prgregset_t, uint_t, const long *);
static	void	PrintSyscall(const lwpstatus_t *, prgregset_t);
static	void	adjust_leaf_frame(struct ps_prochandle *, prgregset_t);
static	void	CallStack(struct ps_prochandle *, const lwpstatus_t *);

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

static	p_td_init_t			p_td_init;
static	p_td_ta_new_t			p_td_ta_new;
static	p_td_ta_delete_t		p_td_ta_delete;
static	p_td_ta_thr_iter_t		p_td_ta_thr_iter;
static	p_td_thr_get_info_t		p_td_thr_get_info;
static	p_td_thr_getgregs_t		p_td_thr_getgregs;

int
main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int errflg = FALSE;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "F")) != EOF) {
		switch (opt) {
		case 'F':
			Fflag = PGRAB_FORCE;
			break;
		default:
			errflg = TRUE;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s [-F] { pid | core } ...\n",
			command);
		(void) fprintf(stderr, "  (show process call stack)\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		exit(2);
	}

	while (--argc >= 0) {
		char *arg;
		int gcode;
		psinfo_t psinfo;
		struct ps_prochandle *Pr;

		if ((Pr = proc_arg_grab(arg = *argv++, PR_ARG_ANY,
		    Fflag, &gcode)) == NULL) {

			(void) fprintf(stderr, "%s: cannot examine %s: %s\n",
			    command, arg, Pgrab_error(gcode));
			retc++;
			continue;
		}

		(void) memcpy(&psinfo, Ppsinfo(Pr), sizeof (psinfo_t));
		proc_unctrl_psinfo(&psinfo);

		if (Pstate(Pr) == PS_DEAD) {
			(void) printf("core '%s' of %d:\t%.70s\n",
			    arg, (int)psinfo.pr_pid, psinfo.pr_psargs);
		} else {
			(void) printf("%d:\t%.70s\n",
			    (int)psinfo.pr_pid, psinfo.pr_psargs);
		}

		is64 = (psinfo.pr_dmodel == PR_MODEL_LP64);

		if (Pgetauxval(Pr, AT_BASE) != -1L && Prd_agent(Pr) == NULL) {
			(void) fprintf(stderr, "%s: warning: librtld_db failed "
			    "to initialize; symbols from shared libraries will "
			    "not be available\n", command);
		}

		if (Pstatus(Pr)->pr_nlwp <= 1) {
			if (AllCallStacks(Pr, FALSE) != 0)
				retc++;
		} else {
			td_thragent_t *Tap;
			int libthread;

			/*
			 * Iterate over the process mappings looking
			 * for libthread and then dlopen the appropriate
			 * libthread_db and get pointers to functions.
			 */
			(void) Pobject_iter(Pr, object_iter, Pr);

			/*
			 * First we need to get a thread agent handle.
			 */
			if (p_td_init == NULL ||
			    p_td_init() != TD_OK ||
			    p_td_ta_new(Pr, &Tap) != TD_OK) /* no libthread */
				libthread = FALSE;
			else {
				/*
				 * Iterate over all threads, calling:
				 *   thr_stack(td_thrhandle_t *Thp, NULL);
				 * for each one to generate the list of threads.
				 */
				(void) p_td_ta_thr_iter(Tap, thr_stack, NULL,
				    TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
				    TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);

				(void) p_td_ta_delete(Tap);
				libthread = TRUE;
			}
			if (AllCallStacks(Pr, libthread) != 0)
				retc++;
			if (libthread)
				free_threadinfo();
			reset_libthread_db();
		}

		Prelease(Pr, 0);
	}

	return (retc);
}

/* ARGSUSED */
static int
object_iter(void *cd, const prmap_t *pmp, const char *object_name)
{
	struct ps_prochandle *Pr = cd;
	char *s1, *s2;
	char libthread_db[PATH_MAX];

	if (strstr(object_name, "/libthread.so.") == NULL)
		return (0);

	/*
	 * We found a libthread.
	 * dlopen() the matching libthread_db and get the thread agent handle.
	 */
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
		return (0);

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

	if (p_td_init == NULL ||
	    p_td_ta_new == NULL ||
	    p_td_ta_delete == NULL ||
	    p_td_ta_thr_iter == NULL ||
	    p_td_thr_get_info == NULL ||
	    p_td_thr_getgregs == NULL)
		reset_libthread_db();

	return (1);
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
}

/*
 * Thread iteration call-back function.
 * Called once for each user-level thread.
 * Used to build the list of all threads.
 */
/* ARGSUSED1 */
static int
thr_stack(const td_thrhandle_t *Thp, void *cd)
{
	td_thrinfo_t thrinfo;
	struct threadinfo *tip;

	if (p_td_thr_get_info(Thp, &thrinfo) != TD_OK)
		return (0);

	tip = malloc(sizeof (struct threadinfo));
	tip->next = NULL;
	tip->threadid = thrinfo.ti_tid;
	tip->lwpid = thrinfo.ti_lid;

	(void) memset(tip->regs, 0, sizeof (prgregset_t));
	(void) p_td_thr_getgregs(Thp, tip->regs);

	if (thr_tail)
		thr_tail->next = tip;
	else
		thr_head = tip;
	thr_tail = tip;

	return (0);
}

static void
free_threadinfo()
{
	struct threadinfo *tip = thr_head;
	struct threadinfo *next;

	while (tip) {
		next = tip->next;
		free(tip);
		tip = next;
	}

	thr_head = thr_tail = NULL;
}

/*
 * Find and eliminate the thread corresponding to the given lwpid.
 */
static id_t
find_thread(id_t lwpid)
{
	struct threadinfo *tip;
	id_t threadid;

	for (tip = thr_head; tip; tip = tip->next) {
		if (lwpid == tip->lwpid) {
			threadid = tip->threadid;
			tip->threadid = 0;
			tip->lwpid = 0;
			return (threadid);
		}
	}
	return (0);
}

static int
ThreadCallStack(struct ps_prochandle *Pr, const lwpstatus_t *psp)
{
	tlhead(find_thread(psp->pr_lwpid), psp->pr_lwpid);
	CallStack(Pr, psp);
	return (0);
}

static int
LwpCallStack(struct ps_prochandle *Pr, const lwpstatus_t *psp)
{
	tlhead(0, psp->pr_lwpid);
	CallStack(Pr, psp);
	return (0);
}

static int
AllCallStacks(struct ps_prochandle *Pr, int dothreads)
{
	pstatus_t status = *Pstatus(Pr);

	(void) memset(&sigh, 0, sizeof (GElf_Sym));
	if (dothreads) {
		(void) Plookup_by_name(Pr, "libthread.so",
		    "sigacthandler", &sigh);
	} else
		(void) Plookup_by_name(Pr, "libc.so", "sigacthandler", &sigh);

	if (status.pr_nlwp <= 1)
		CallStack(Pr, &status.pr_lwp);
	else {
		lwpstatus_t lwpstatus;
		struct threadinfo *tip;
		id_t tid;

		if (dothreads)
			(void) Plwp_iter(Pr, (proc_lwp_f *)ThreadCallStack, Pr);
		else
			(void) Plwp_iter(Pr, (proc_lwp_f *)LwpCallStack, Pr);

		/* for each remaining thread w/o an lwp */
		(void) memset(&lwpstatus, 0, sizeof (lwpstatus));
		for (tip = thr_head; tip; tip = tip->next) {
			if ((tid = tip->threadid) != 0) {
				(void) memcpy(lwpstatus.pr_reg, tip->regs,
					sizeof (prgregset_t));
				tlhead(tid, tip->lwpid);
				CallStack(Pr, &lwpstatus);
			}
			tip->threadid = 0;
			tip->lwpid = 0;
		}
	}
	return (0);
}

static void
tlhead(id_t threadid, id_t lwpid)
{
	if (threadid == 0 && lwpid == 0)
		return;

	(void) printf("-----------------");

	if (threadid && lwpid)
		(void) printf("  lwp# %d / thread# %d  ",
			(int)lwpid, (int)threadid);
	else if (threadid)
		(void) printf("---------  thread# %d  ", (int)threadid);
	else if (lwpid)
		(void) printf("  lwp# %d  ------------", (int)lwpid);

	(void) printf("--------------------\n");
}

static int
PrintFrame(void *cd, const prgregset_t gregs, uint_t argc, const long *argv)
{
	struct ps_prochandle *Pr = cd;
	uintptr_t pc = gregs[R_PC];
	char buff[255];
	GElf_Sym sym;
	uintptr_t start;
	int length = (is64? 16 : 8);
	int i;

	(void) sprintf(buff, "%.*lx", length, (long)pc);
	(void) strcpy(buff + length, " ????????");
	if (Plookup_by_addr(Pr, pc,
	    buff + 1 + length, sizeof (buff) - 1 - length, &sym) == 0)
		start = sym.st_value;
	else
		start = pc;

	(void) printf(" %-17s (", buff);
	for (i = 0; i < argc && i < MAX_ARGS; i++)
		(void) printf((i+1 == argc)? "%lx" : "%lx, ",
			argv[i]);
	if (i != argc)
		(void) printf("...");
	(void) printf((start != pc)?
		") + %lx\n" : ")\n", (long)(pc - start));

	/*
	 * If the frame's pc is in the "sigh" (a.k.a. signal handler, signal
	 * hack, or *sigh* ...) range, then we're about to cross a signal
	 * frame.  The signal number is the first argument to this function.
	 */
	if (pc - sigh.st_value < sigh.st_size) {
		if (sig2str((int)argv[0], buff) == -1)
			(void) strcpy(buff, " Unknown");
		(void) printf(" --- called from signal handler with "
		    "signal %d (SIG%s) ---\n", (int)argv[0], buff);
	}

	return (0);
}

static void
PrintSyscall(const lwpstatus_t *psp, prgregset_t reg)
{
	char sname[32];
	int length = (is64? 16 : 8);
	uint_t i;

	(void) proc_sysname(psp->pr_syscall, sname, sizeof (sname));
	(void) printf(" %.*lx %-8s (", length, (long)reg[R_PC], sname);
	for (i = 0; i < psp->pr_nsysarg; i++)
		(void) printf((i+1 == psp->pr_nsysarg)? "%lx" : "%lx, ",
			(long)psp->pr_sysarg[i]);
	(void) printf(")\n");
}

/* ARGSUSED */
static void
adjust_leaf_frame(struct ps_prochandle *Pr, prgregset_t reg)
{
#if defined(sparc) || defined(__sparc)
	if (is64)
		reg[R_PC] = reg[R_O7];
	else
		reg[R_PC] = (uint32_t)reg[R_O7];
	reg[R_nPC] = reg[R_PC] + 4;
#elif defined(i386) || defined(__i386)
	(void) Pread(Pr, &reg[R_PC], sizeof (prgreg_t), (long)reg[R_SP]);
	reg[R_SP] += 4;
#endif
}

static void
CallStack(struct ps_prochandle *Pr, const lwpstatus_t *psp)
{
	prgregset_t reg;

	(void) memcpy(reg, psp->pr_reg, sizeof (reg));

	if (psp->pr_flags & (PR_ASLEEP|PR_VFORKP)) {
		PrintSyscall(psp, reg);
		adjust_leaf_frame(Pr, reg);
	}

	(void) Pstack_iter(Pr, reg, PrintFrame, Pr);
}
