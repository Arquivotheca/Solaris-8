/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)apptrace.c	1.2	99/10/26 SMI"

#include <link.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <signal.h>
#include <synch.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <apptrace.h>
#include <libintl.h>
#include <locale.h>
#include <limits.h>
#include "abienv.h"
#include "mach.h"

static Liblist	*bindto_list;
static Liblist	*bindto_excl;
static Liblist	*bindfrom_list;
static Liblist	*bindfrom_excl;
static Liblist	*intlib_list;
static uint_t	pidout;
static Intlist	*trace_list;
static Intlist	*trace_excl;
static Intlist	*verbose_list;
static Intlist	*verbose_excl;
static int	threaded;

/*
 * Required for calls to build_env_list1 where
 * things are added to the end of the list (preserving
 * search order implied by the setting of env variables
 * in apptracecmd.c)
 */
static Liblist	*intlib_listend;

/*
 * These globals are sought and used by interceptlib.c
 * which goes into all interceptor objects.
 */
FILE		*ABISTREAM = stderr;

/*
 * Strings are printed with "%.*s", abi_strpsz, string
 */
int		abi_strpsz = 20;

/*
 * Special function pointers that'll be set up to point at the
 * libc/libthread versions in the _application's_ link map (as opposed
 * to our own).  The reasons for this are somewhat complex but here
 * goes.  fprintf is not re-entrant in the face of signals.  So we'd
 * protect such calls with calls to sigprocmask() to block and unblock
 * signals accordingly.  However, in the threaded world you're
 * supposed to use thr_sigsetmask().  Of course, if we're not threaded
 * then we want sigprocmask() instead.  We're taking advantage of the
 * fact that both functions have the same signature to avoid constant
 * checking...
 *
 * Further complicating things is fact that fprintf is not reentrant
 * in the face of threads and has calls to mutex_lock/unlock in its
 * implementation.  Again, since we don't have libthread in the
 * auditing link map, fprintf will potentially fail in a threaded
 * program.  So we need to have our own implementation (see private.c)
 *
 * Additionally, it is impossible to generalize the programmatic
 * creation of interceptor functions for variable argument list
 * functions.  However, in the case of the printf family, there is a
 * vprintf equivalent.  The interceptors for the printf family live in
 * interceptor.c and they call the appropriate vprintf interface
 * instead of the printf interface that they're intercepting.  The
 * link map issue remains, however, so function pointers for the
 * vprintf family in the application's link map are set up here.
 *
 * The interceptors also need to examine errno which also needs to be
 * extracted from the base link map.
 *
 * All of these pointers are initialized in la_preinit().
 */
int (*abi_sigsetmask)(int, const sigset_t *, sigset_t *);
int (*abi_sigaction)(int, const struct sigaction *, struct sigaction *);
int (*abi_mutex_lock)(mutex_t *);
int (*abi_mutex_unlock)(mutex_t *);

int (*ABI_VFPRINTF)(FILE *, char const *, va_list);
int (*ABI_VFWPRINTF)(FILE *, const wchar_t *, va_list);
int (*ABI_VPRINTF)(char const *, va_list);
int (*ABI_VSNPRINTF)(char *, size_t, char const *, va_list);
int (*ABI_VSPRINTF)(char *, char const *, va_list);
int (*ABI_VSWPRINTF)(wchar_t *, size_t, const wchar_t *, va_list);
int (*ABI_VWPRINTF)(const wchar_t *, va_list);
int *(*__abi_real_errno)(void);
sigset_t abisigset;

#if defined(_LP64)
static char const *libcpath		= "/usr/lib/sparcv9/libc.so.1";
static char const *libthreadpath	= "/usr/lib/sparcv9/libthread.so.1";
static char const *libpthreadpath	= "/usr/lib/sparcv9/libpthread.so.1";
static char const *liblwpthreadpath	= "/usr/lib/lwp/sparcv9/libthread.so.1";
#else
static char const *libcpath		= "/usr/lib/libc.so.1";
static char const *libthreadpath	= "/usr/lib/libthread.so.1";
static char const *libpthreadpath	= "/usr/lib/libpthread.so.1";
static char const *liblwpthreadpath	= "/usr/lib/lwp/libthread.so.1";
#endif

/*
 * With the addition of the /usr/lib/lwp/libthread, the possibility exists
 * that we may be threaded but with a different libthread than was previously
 * assumed.  We'll need to be able to remember which libthread we're running
 * with.
 */
enum whichthreads {
	NOTHREADS = 0x0,
	USE_2LEVEL = 0x1,
	USE_PTHREADS = 0x2,
	USE_LIBLWP = 0x4
};

/* Used as arguments later to dlsym */
static char const *thr_setmask_sym	= "thr_sigsetmask";
static char const *sigprocmask_sym	= "_libc_sigprocmask";
static char const *sigaction_sym	= "sigaction";
static char const *mutex_lock_sym	= "mutex_lock";
static char const *mutex_unlock_sym	= "mutex_unlock";

static char const *vfprintf_sym		= "vfprintf";
static char const *vfwprintf_sym	= "vfwprintf";
static char const *vprintf_sym		= "vprintf";
static char const *vsnprintf_sym	= "vsnprintf";
static char const *vsprintf_sym		= "vsprintf";
static char const *vswprintf_sym	= "vswprintf";
static char const *vwprintf_sym		= "vwprintf";
static char const *errno_sym		= "___errno";

/*
 * The list of functions below are functions for which
 * apptrace.so will not perform any tracing.
 *
 * The user visible failure of tracing these functions
 * is a core dump of the application under observation.
 *
 * This list was originally discovered during sotruss
 * development.  Attempts lacking sufficient determination
 * to shrink this list have failed.
 *
 * There are a number of different kinds of issues here.
 *
 * The .stretX functions have to do with the relationship
 * that the caller and callee has with functions that
 * return structures and the altered calling convention
 * that results.
 *
 * We cannot trace *setjmp because the caller of these routines
 * is not allow to return which is exactly what an interceptor
 * function is going to do.
 *
 * The *context functions are on the list because we cannot trace
 * netscape without them on the list, but the exact mechanics of the
 * failure are not known at this time.
 *
 * The leaf functions *getsp can probably be removed given the
 * presence of an interceptor but that experiment has not been
 * conducted.
 *
 * NOTE: this list *must* be mainted in alphabetical order.
 *	 if this list ever became too long a faster search mechanism
 *	 should be considered.
 */
static char *spec_sym[] = {
#if defined(sparc)
	".stret1",
	".stret2",
	".stret4",
	".stret8",
#endif
	"__getcontext",
	"_getcontext",
	"_getsp",
	"_longjmp",
	"_setcontext",
	"_setjmp",
	"_siglongjmp",
	"_sigsetjmp",
	"_vfork",
	"getcontext",
	"getsp",
	"longjmp",
	"setcontext",
	"setjmp",
	"siglongjmp",
	"sigsetjmp",
	"vfork",
	NULL
};

uint_t
la_version(uint_t version)
{
	char		*str;
	FILE		*fp;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (version > LAV_CURRENT)
		(void) fprintf(stderr,
		    gettext("apptrace: unexpected version: %u\n"), version);

	build_env_list(&bindto_list, "APPTRACE_BINDTO");
	build_env_list(&bindto_excl, "APPTRACE_BINDTO_EXCLUDE");

	build_env_list(&bindfrom_list, "APPTRACE_BINDFROM");
	build_env_list(&bindfrom_excl, "APPTRACE_BINDFROM_EXCLUDE");

	if (checkenv("APPTRACE_PID") != NULL) {
		pidout = 1;
	} else {
		char *str = "LD_AUDIT=";
		char *str2 = "LD_AUDIT64=";
		/*
		 * This disables apptrace output in subsequent exec'ed
		 * processes.
		 */
		(void) putenv(str);
		(void) putenv(str2);
	}

	if ((str = checkenv("APPTRACE_OUTPUT")) != NULL) {
		int fd, newfd, targetfd, lowerlimit;
		struct rlimit rl;

		if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
			(void) fprintf(stderr,
			    gettext("apptrace: getrlimit: %s\n"),
			    strerror(errno));
			exit(EXIT_FAILURE);
		}

		fd = open(str, O_WRONLY|O_CREAT, 0666);
		if (fd == -1) {
			(void) fprintf(stderr,
			    gettext("apptrace: %s: %s\n"),
			    str, strerror(errno));
			exit(EXIT_FAILURE);
		}

		/*
		 * Those fans of dup2 should note that dup2 cannot
		 * be used below because dup2 closes the target file
		 * descriptor.  Thus, if we're apptracing say, ksh
		 * we'd have closed the fd it uses for the history
		 * file (63 on my box).
		 *
		 * fcntl with F_DUPFD returns first available >= arg3
		 * so we iterate from the top until we find a available
		 * fd.
		 *
		 * Not finding an fd after 10 tries is a failure.
		 *
		 * Since the _file member of the FILE structure is an
		 * unsigned char, we must clamp our fd request to
		 * UCHAR_MAX
		 */
		lowerlimit = ((rl.rlim_cur >
		    UCHAR_MAX) ? UCHAR_MAX : rl.rlim_cur) - 10;

		for (targetfd = lowerlimit + 10;
		    targetfd > lowerlimit; targetfd--) {
			if ((newfd = fcntl(fd, F_DUPFD, targetfd)) != -1)
				break;
		}

		if (newfd == -1) {
			(void) fprintf(stderr,
			    gettext("apptrace: F_DUPFD: %s\n"),
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
		(void) close(fd);

		if (fcntl(newfd, F_SETFD, FD_CLOEXEC) == -1) {
			(void) fprintf(stderr,
			    gettext("apptrace: fcntl FD_CLOEXEC: %s\n"),
			    strerror(errno));
			exit(EXIT_FAILURE);
		}

		if ((fp = fdopen(newfd, "w")) != NULL) {
			ABISTREAM = fp;
		} else {
			(void) fprintf(stderr,
			    gettext("apptrace: fdopen: %s\n"),
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

#if defined(_LP64)
	build_env_list1(&intlib_list, &intlib_listend,
	    "APPTRACE_INTERCEPTORS64");
#else
	build_env_list1(&intlib_list, &intlib_listend,
	    "APPTRACE_INTERCEPTORS");
#endif

	/* Set up lists interfaces to trace or ignore */
	env_to_intlist(&trace_list, "APPTRACE_INTERFACES");
	env_to_intlist(&trace_excl, "APPTRACE_INTERFACES_EXCLUDE");
	env_to_intlist(&verbose_list, "APPTRACE_VERBOSE");
	env_to_intlist(&verbose_excl, "APPTRACE_VERBOSE_EXCLUDE");

	return (LAV_CURRENT);
}


/* ARGSUSED1 */
uint_t
la_objopen(Link_map *lmp, Lmid_t lmid, uintptr_t *cookie)
{
	uint_t		flags;
	static int	first = 1;
	char		buf[MAXPATHLEN];

	/*
	 * If this is the first time in, then l_name is the app
	 * and unless the user gave an explict from list
	 * we will trace calls from it.
	 */
	if (first && bindfrom_list == NULL) {
		flags = LA_FLG_BINDFROM | LA_FLG_BINDTO;
		first = 0;
		goto work;
	}

	/*
	 * If we have no bindto_list, then we assume that we
	 * bindto everything (apptrace -T \*)
	 *
	 * Otherwise we make sure that l_name is on the list.
	 */
	flags = 0;
	if (bindto_list == NULL) {
		flags = LA_FLG_BINDTO;
	} else if (check_list(bindto_list, lmp->l_name) != NULL) {
		flags |= LA_FLG_BINDTO;
	}

	/*
	 * If l_name is on the exclusion list, zero the bit.
	 */
	if ((bindto_excl != NULL) &&
	    check_list(bindto_excl, lmp->l_name) != NULL) {
		flags &= ~LA_FLG_BINDTO;
	}

	/*
	 * If l_name is on the bindfrom list then trace
	 */
	if (check_list(bindfrom_list, lmp->l_name) != NULL) {
		flags |= LA_FLG_BINDFROM;
	}

	/*
	 * If l_name is on the exclusion list, zero the bit
	 * else trace, (this allows "-F !foo" to imply
	 * "-F '*' -F !foo")
	 */
	if (check_list(bindfrom_excl, lmp->l_name) != NULL) {
		flags &= ~LA_FLG_BINDFROM;
	} else if (bindfrom_excl != NULL && bindfrom_list == NULL) {
		flags |= LA_FLG_BINDFROM;
	}

work:
	if (threaded == NOTHREADS) {
		if (strcmp(lmp->l_name, libthreadpath) == 0)
			threaded &= USE_2LEVEL;
		if (strcmp(lmp->l_name, libpthreadpath) == 0)
			threaded &= USE_PTHREADS;
		if (strcmp(lmp->l_name, liblwpthreadpath) == 0)
			threaded &= USE_LIBLWP;
	}

	if (flags) {
		*cookie = (uintptr_t)abibasename(lmp->l_name);
		*buf = '\0';
		(void) build_interceptor_path(buf, sizeof (buf), lmp->l_name);
		appendlist(&intlib_list, &intlib_listend, buf, 0);
	}

	return (flags);
}

static void
apptrace_preinit_fail(void)
{
	(void) fprintf(stderr, gettext("apptrace: la_preinit: %s\n"),
	    dlerror());
	exit(EXIT_FAILURE);
}

/* ARGSUSED */
void
la_preinit(uintptr_t *cookie)
{
	void	*h = NULL;

	(void) sigfillset(&abisigset);

	if (threaded != NOTHREADS) {
		if (threaded & USE_LIBLWP)
			h = dlmopen(LM_ID_BASE, liblwpthreadpath,
			    RTLD_LAZY | RTLD_NOLOAD);
		else
			h = dlmopen(LM_ID_BASE, libthreadpath,
			    RTLD_LAZY | RTLD_NOLOAD);
		if (h == NULL)
			apptrace_preinit_fail();
		if ((abi_sigsetmask =
		    (int (*)(int, const sigset_t *, sigset_t *))
		    dlsym(h, thr_setmask_sym)) == NULL)
			apptrace_preinit_fail();
		if ((abi_sigaction =
		    (int (*)(int, const struct sigaction *,
			struct sigaction *)) dlsym(h, sigaction_sym)) == NULL)
			apptrace_preinit_fail();
		if ((abi_mutex_lock =
		    (int (*)(mutex_t *))
		    dlsym(h, mutex_lock_sym)) == NULL)
			apptrace_preinit_fail();
		if ((abi_mutex_unlock =
		    (int (*)(mutex_t *))
		    dlsym(h, mutex_unlock_sym)) == NULL)
			apptrace_preinit_fail();
	}
	h = dlmopen(LM_ID_BASE, libcpath, RTLD_LAZY | RTLD_NOLOAD);
	if (h == NULL)
		apptrace_preinit_fail();

	if (abi_sigsetmask == NULL)
		if ((abi_sigsetmask =
		    (int (*)(int, const sigset_t *, sigset_t *))
		    dlsym(h, sigprocmask_sym)) == NULL)
			apptrace_preinit_fail();

	if (abi_sigaction == NULL)
		if ((abi_sigaction =
		    (int (*)(int, const struct sigaction *,
			struct sigaction *)) dlsym(h, sigaction_sym)) == NULL)
			apptrace_preinit_fail();

	if (abi_mutex_lock == NULL)
		if ((abi_mutex_lock =
		    (int (*)(mutex_t *))
		    dlsym(h, mutex_lock_sym)) == NULL)
			apptrace_preinit_fail();

	if (abi_mutex_unlock == NULL)
		if ((abi_mutex_unlock =
		    (int (*)(mutex_t *))
		    dlsym(h, mutex_unlock_sym)) == NULL)
			apptrace_preinit_fail();

	/* Do printf style pointers */
	if ((ABI_VFPRINTF =
	    (int (*)(FILE *, char const *, va_list))
	    dlsym(h, vfprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((ABI_VFWPRINTF =
	    (int (*)(FILE *, const wchar_t *, va_list))
	    dlsym(h, vfwprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((ABI_VPRINTF =
	    (int (*)(char const *, va_list))
	    dlsym(h, vprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((ABI_VSNPRINTF =
	    (int (*)(char *, size_t, char const *, va_list))
	    dlsym(h, vsnprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((ABI_VSPRINTF =
	    (int (*)(char *, char const *, va_list))
	    dlsym(h, vsprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((ABI_VSWPRINTF =
	    (int (*)(wchar_t *, size_t, const wchar_t *, va_list))
	    dlsym(h, vswprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((ABI_VWPRINTF =
	    (int (*)(const wchar_t *, va_list))
	    dlsym(h, vwprintf_sym)) == NULL)
		apptrace_preinit_fail();

	if ((__abi_real_errno =
	    (int *(*)(void))
	    dlsym(h, errno_sym)) == NULL)
		apptrace_preinit_fail();
}

/* ARGSUSED1 */
#if defined(_LP64)
uintptr_t
la_symbind64(Elf64_Sym *symp, uint_t symndx, uintptr_t *refcook,
    uintptr_t *defcook, uint_t *sb_flags, char const *sym_name)
#else
uintptr_t
la_symbind32(Elf32_Sym *symp, uint_t symndx, uintptr_t *refcook,
    uintptr_t *defcook, uint_t *sb_flags)
#endif
{
#if !defined(_LP64)
	char const *sym_name = (char const *) symp->st_name;
#endif
	int intercept = 0, verbose = 0;
	char *defname = (char *)*defcook;
	uintptr_t ret = symp->st_value;
	uintptr_t (*p)(uintptr_t, int);
	char symname[MAXPATHLEN], *symnamep = symname;
	int  symlen;
	char sobasename[MAXNAMELEN], *sobp;
	char const *symformat = "__abi_%s_%s";
	Liblist *lp;
	uint_t ndx;
	char *str;

#if defined(_LP64)
	if (ELF64_ST_TYPE(symp->st_info) != STT_FUNC)
		goto end;
#else
	/* If we're not looking at a function, bug out */
	if (ELF32_ST_TYPE(symp->st_info) != STT_FUNC)
		goto end;
#endif

	*sb_flags |= LA_SYMB_NOPLTEXIT;

	if (verbose_list != NULL) {
		/* apptrace ... -v verbose_list ... cmd */
		if (check_intlist(verbose_list, sym_name))
			verbose = 1;
	}
	if (verbose_excl != NULL) {
		/* apptrace ... -v !verbose_excl ... cmd */
		if (check_intlist(verbose_excl, sym_name))
			verbose = 0;
		else if (verbose_list == NULL && trace_list == NULL &&
		    trace_excl == NULL)
			/* apptrace -v !verbose_excl cmd */
			intercept = 1;
	}
	if (trace_list != NULL) {
		/* apptrace ... -t trace_list ... cmd */
		if (check_intlist(trace_list, sym_name))
			intercept = 1;
	} else if (verbose_list == NULL && verbose_excl == NULL)
		/* default (implies -t '*'):  apptrace cmd */
		intercept = 1;

	if (trace_excl != NULL) {
		/* apptrace ... -t !trace_excl ... cmd */
		if (check_intlist(trace_excl, sym_name))
			intercept = 0;
	}

	if (verbose == 0 && intercept == 0) {
		*sb_flags |= (LA_SYMB_NOPLTEXIT | LA_SYMB_NOPLTENTER);
		goto end;
	}

	/*
	 * Check to see if this symbol is one of the 'special' symbols.
	 * If so we disable interceptor calls for that symbol.
	 */
	for (ndx = 0; (str = spec_sym[ndx]) != NULL; ndx++) {
		int	cmpval;
		cmpval = strcmp(sym_name, str);
		if (cmpval < 0)
			break;
		if (cmpval == 0) {
			intercept = verbose = 0;
			*sb_flags |= (LA_SYMB_NOPLTEXIT | LA_SYMB_NOPLTENTER);
			break;
		}
	}

	/* Check to see if we have an interceptor for this function */
	if ((intercept || verbose) && intlib_list) {

		(void) strcpy(sobasename, defname);
		if ((sobp = strchr(sobasename, '.')))
			*sobp = 0;

		symlen = snprintf(symname, sizeof (symname),
		    symformat, sobasename, sym_name);
		if (symlen >= sizeof (symname)) {
			if ((symnamep = malloc(symlen + 1)) == NULL) {
				(void) fputs(gettext("apptrace: "
				    "malloc failed\n"), stderr);
				exit(EXIT_FAILURE);
			} else {
				(void) sprintf(symnamep, symformat,
				    sobasename, sym_name);
			}
		}
		for (lp = intlib_list; lp != 0; lp = lp->l_next) {
			p = (uintptr_t (*)(uintptr_t, int))
			    dlsym(lp->l_handle, symnamep);
			if (p) {
				ret = p(symp->st_value, verbose);
				break;
			}
		}
	}

end:
	if (symnamep != symname)
		free(symnamep);
	return (ret);
}

/* ARGSUSED1 */
#if	defined(__sparcv9)
uintptr_t
la_sparcv9_pltenter(Elf64_Sym *symp, uint_t symndx, uintptr_t *refcookie,
    uintptr_t *defcookie, La_sparcv9_regs *regset, uint_t *sb_flags,
    char const *sym_name)
#elif	defined(__sparc)
uintptr_t
la_sparcv8_pltenter(Elf32_Sym *symp, uint_t symndx, uintptr_t *refcookie,
	uintptr_t *defcookie, La_sparcv8_regs *regset, uint_t *sb_flags)
#elif   defined(__i386)
uintptr_t
la_i86_pltenter(Elf32_Sym *symp, uint_t symndx, uintptr_t *refcookie,
	uintptr_t *defcookie, La_i86_regs *regset, uint_t *sb_flags)
#endif
{
	char *defname = (char *)(*defcookie);
	char *refname = (char *)(*refcookie);
#if	!defined(_LP64)
	char const	*sym_name = (char const *)symp->st_name;
#endif

	if (pidout)
		(void) abi_fprintf(ABISTREAM, "%7u:", getpid());

	if ((*sb_flags & LA_SYMB_ALTVALUE) == 0) {
		(void) abi_fprintf(ABISTREAM,
		    "%-8s -> %8s:%s(0x%lx, 0x%lx, 0x%lx)\n",
		    refname, defname, sym_name,
		    (ulong_t)GETARG0(regset), (ulong_t)GETARG1(regset),
		    (ulong_t)GETARG2(regset));
	    (void) fflush(ABISTREAM);
	} else {
		(void) abi_fprintf(ABISTREAM, "%-8s -> %8s:%s(",
		    refname, defname, sym_name);
	}

	return (symp->st_value);
}
