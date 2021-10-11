/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)ldd.c	1.36	99/09/14 SMI"

/*
 * Print the list of shared objects required by a dynamic executable or shared
 * object.
 *
 * usage is: ldd [-c] [-d | -r] [-i] [-f] [-L] [-l] [-s] [-v] file(s)
 *
 * ldd opens the file and verifies the information in the elf header.
 * If the file is a dynamic executable, we set up some environment variables
 * and exec(2) the file.  If the file is a shared object, we preload the
 * file with a dynamic executable stub. The runtime linker (ld.so.1) actually
 * provides the diagnostic output, according to the environment variables set.
 *
 * If neither -d nor -r is specified, we set only LD_TRACE_LOADED_OBJECTS_[AE].
 * The runtime linker will print the pathnames of all dynamic objects it
 * loads, and then exit.  Note that we distiguish between ELF and AOUT objects
 * when setting this environment variable - AOUT executables cause the mapping
 * of sbcp, the dependencies of which the user isn't interested in.
 *
 * If -d or -r is specified, we also set LD_WARN=1; the runtime linker will
 * perform its normal relocations and issue warning messages for unresolved
 * references. It will then exit.
 * If -r is specified, we set LD_BIND_NOW=1, so that the runtime linker
 * will perform all relocations, otherwise (under -d) the runtime linker
 * will not perform PLT (function) type relocations.
 *
 * If -i is specified, we set LD_INIT=1. The order of inititialization
 * sections to be executed is printed. We also set LD_WARN=1.
 *
 * If -f is specified, we will run ldd as root on executables that have
 * an unsercure runtime linker that does not live under the "/usr/lib"
 * directory.  By default we will not let this happen.
 *
 * If -l is specified it generates a warning for any auxiliary filter not found.
 * Prior to 2.8 this forced any filters to load (all) their filtees.  This is
 * now the default, however missing auxiliary filters don't generate any error
 * diagniostic.  See also -L.
 *
 * If -L is specified we revert to lazy loading, thus any filtee or lazy
 * dependency loading is deferred until relocations cause loading.  Without
 * this option we set LD_LOADFLTR=1, thus forcing any filters to load (all)
 * their filtees, and LD_FLAGS=nolazyload thus forcing immediate processing of
 * any lazy loaded dependencies.
 *
 * If -s is specified we also set LD_TRACE_SEARCH_PATH=1, thus enabling
 * the runtime linker to indicate the search algorithm used.
 *
 * If -v is specified we also set LD_VERBOSE=1, thus enabling the runtime
 * linker to indicate all object dependencies (not just the first object
 * loaded) together with any versionig requirements.
 *
 * If -c is specified we also set LD_NOCONFIG=1, thus disabling any
 * configuration file use.
 */
#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<libelf.h>
#include	<gelf.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<wait.h>
#include	<locale.h>
#include	<errno.h>
#include	"machdep.h"
#include	"paths.h"
#include	"conv.h"
#include	"a.out.h"
#include	"msg.h"

static int	elf_check(int, char *, char *, Elf *, int);
static int	aout_check(int, char *, char *, int, int);
static int	run(int, char *, char *, char *);


/*
 * The following size definitions provide for allocating space for the string,
 * or the string position at which any modifications to the variable will occur.
 */
#define	LD_PRELOAD_SIZE		11
#define	LD_LOAD_SIZE		27
#define	LD_PATH_SIZE		23
#define	LD_BIND_SIZE		13
#define	LD_VERB_SIZE		12
#define	LD_WARN_SIZE		9
#define	LD_CONF_SIZE		13
#define	LD_FLTR_SIZE		13
#define	LD_LAZY_SIZE		10
#define	LD_INIT_SIZE		9

static const char
	*prefile =	MSG_ORIG(MSG_STR_EMPTY),
	*preload =	MSG_ORIG(MSG_LD_PRELOAD),
	*prestr =	MSG_ORIG(MSG_LD_PRELOAD);

static char
	* bind =	"LD_BIND_NOW= ",
	* load_elf =	"LD_TRACE_LOADED_OBJECTS_E= ",
	* load_aout =	"LD_TRACE_LOADED_OBJECTS_A= ",
	* path =	"LD_TRACE_SEARCH_PATHS= ",
	* verb =	"LD_VERBOSE= ",
	* warn =	"LD_WARN= ",
	* conf =	"LD_NOCONFIG= ",
	* fltr =	"LD_LOADFLTR= ",
	* lazy =	"LD_FLAGS=nolazyload",
	* init =	"LD_INIT= ",
	* load;


main(int argc, char **argv)
{
	char *fname, *cname = argv[0];

	Elf	*elf;
	int	cflag = 0, dflag = 0, fflag = 0, iflag = 0, Lflag = 0;
	int	lflag = 0, rflag = 0, sflag = 0, vflag = 0;
	int	nfile, var, error = 0;

	/*
	 * Establish locale.
	 */
	(void) setlocale(LC_MESSAGES, MSG_ORIG(MSG_STR_EMPTY));
	(void) textdomain(MSG_ORIG(MSG_SUNW_OST_SGS));

	/*
	 * verify command line syntax and process arguments
	 */
	opterr = 0;				/* disable getopt error mesg */

	while ((var = getopt(argc, argv, "cdfiLlrsv")) != EOF) {
		switch (var) {
		case 'c' :			/* enable config search */
			cflag = 1;
			break;
		case 'd' :			/* perform data relocations */
			dflag = 1;
			if (rflag)
				error++;
			break;
		case 'f' :
			fflag = 1;
			break;
		case 'L' :
			Lflag = 1;
			break;
		case 'l' :
			lflag = 1;
			break;
		case 'i' :			/* print the order of .init */
			iflag = 1;
			break;
		case 'r' :			/* perform all relocations */
			rflag = 1;
			if (dflag)
				error++;
			break;
		case 's' :			/* enable search path output */
			sflag = 1;
			break;
		case 'v' :			/* enable verbose output */
			vflag = 1;
			break;
		default :
			error++;
			break;
		}
		if (error)
			break;
	}
	if (error) {
		(void) fprintf(stderr, MSG_INTL(MSG_ARG_USAGE), cname);
		exit(1);
	}

	/*
	 * Determine if LD_PRELOAD is already set, if so we'll continue to
	 * analyze each object with this setting.
	 */
	if ((fname = getenv(MSG_ORIG(MSG_LD_PRELOAD))) != 0) {
		prefile = fname;
		if ((fname = (char *)malloc(strlen(prefile) +
		    LD_PRELOAD_SIZE + 1)) == 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_MALLOC), cname);
			exit(1);
		}
		(void) sprintf(fname, "%s=%s", preload, prefile);
		prestr = fname;
	}

	/*
	 * Set the appropriate relocation environment variables (Note unsetting
	 * the environment variables is done just in case the user already
	 * has these in their environment ... sort of thing the test folks
	 * would do :-)
	 */
	warn[LD_WARN_SIZE - 1] = (dflag || rflag) ? '1' : '\0';
	bind[LD_BIND_SIZE - 1] = (rflag) ? '1' : '\0';
	path[LD_PATH_SIZE - 1] = (sflag) ? '1' : '\0';
	verb[LD_VERB_SIZE - 1] = (vflag) ? '1' : '\0';
	fltr[LD_FLTR_SIZE - 1] = (Lflag) ? '\0' : (lflag) ? '2' : '1';
	init[LD_INIT_SIZE - 1] = (iflag) ? '1' : '\0';
	conf[LD_CONF_SIZE - 1] = (cflag) ? '1' : '\0';

	if (Lflag)
		lazy[LD_LAZY_SIZE - 1] = '\0';

	if ((putenv(warn) != 0) || (putenv(bind) != 0) || (putenv(path) != 0) ||
	    (putenv(verb) != 0) || (putenv(fltr) != 0) || (putenv(conf) != 0) ||
	    (putenv(init) != 0) || (putenv(lazy) != 0)) {
		(void) fprintf(stderr, MSG_INTL(MSG_ENV_FAILED), cname);
		exit(1);
	}

	/*
	 * coordinate libelf's version information
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr, MSG_INTL(MSG_ELF_LIBELF), cname,
		    EV_CURRENT);
		exit(1);
	}

	/*
	 * Loop through remaining arguments.  Note that from here on there
	 * are no exit conditions so that we can process a list of files,
	 * any error condition is retained for a final exit status.
	 */
	nfile = argc - optind;
	for (; optind < argc; optind++) {
		fname = argv[optind];
		/*
		 * Open file (do this before checking access so that we can
		 * provide the user with better diagnostics).
		 */
		if ((var = open(fname, O_RDONLY)) == -1) {
			int	err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN), cname,
			    fname, strerror(err));
			error = 1;
			continue;
		}

		/*
		 * Get the files elf descriptor and process it as an elf or
		 * a.out (4.x) file.
		 */
		elf = elf_begin(var, ELF_C_READ, (Elf *)0);
		switch (elf_kind(elf)) {
		case ELF_K_AR :
			(void) fprintf(stderr, MSG_INTL(MSG_USP_NODYNORSO),
			    cname, fname);
			error = 1;
			break;
		case ELF_K_COFF:
			(void) fprintf(stderr, MSG_INTL(MSG_USP_UNKNOWN),
			    cname, fname);
			error = 1;
			break;
		case ELF_K_ELF:
			if (elf_check(nfile, fname, cname, elf, fflag) != NULL)
				error = 1;
			break;
		default:
			/*
			 * This is either an unknown file or an aout format
			 */
			if (aout_check(nfile, fname, cname, var, fflag) != NULL)
				error = 1;
			break;
		}
		(void) elf_end(elf);
		(void) close(var);
	}
	return (error);
}



static int
is_runnable(GElf_Ehdr *ehdr)
{
	if ((ehdr->e_ident[EI_CLASS] == M_CLASS) &&
	    (ehdr->e_ident[EI_DATA] == M_DATA))
		return (ELFCLASS32);

#if	defined(sparc)
	if ((ehdr->e_machine == EM_SPARCV9) &&
	    (ehdr->e_ident[EI_DATA] == M_DATA) &&
	    (conv_sys_eclass() == ELFCLASS64))
		return (ELFCLASS64);
#elif	defined(i386) || defined(__ia64)
	if ((ehdr->e_machine == EM_IA_64) &&
	    (ehdr->e_ident[EI_DATA] == ELFDATA2LSB) &&
	    (conv_sys_eclass() == ELFCLASS64))
		return (ELFCLASS64);
#endif

	return (ELFCLASSNONE);
}


static int
elf_check(int nfile, char *fname, char *cname, Elf *elf, int fflag)
{
	GElf_Ehdr 	ehdr;
	GElf_Phdr 	phdr;
	int		dynamic, cnt;
	int		run_class;

	/*
	 * verify information in file header
	 */
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		(void) fprintf(stderr, MSG_INTL(MSG_ELF_GETEHDR),
			cname, fname, elf_errmsg(-1));
		return (1);
	}

	/*
	 * check class and encoding
	 */
	if ((run_class = is_runnable(&ehdr)) == ELFCLASSNONE) {
		(void) fprintf(stderr, MSG_INTL(MSG_ELF_CLASSDATA),
			cname, fname);
		return (1);
	}

	/*
	 * check type
	 */
	if ((ehdr.e_type != ET_EXEC) && (ehdr.e_type != ET_DYN) &&
	    (ehdr.e_type != ET_REL)) {
		(void) fprintf(stderr, MSG_INTL(MSG_ELF_BADMAGIC),
			cname, fname);
		return (1);
	}
	if ((run_class == ELFCLASS32) && (ehdr.e_machine != M_MACH)) {
		if (ehdr.e_machine != M_MACHPLUS) {
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_MACHTYPE),
				cname, fname);
			return (1);
		}
		if ((ehdr.e_flags & M_FLAGSPLUS) == 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_MACHFLAGS),
				cname, fname);
			return (1);
		}
	}

	/*
	 * Check that the file is executable.  Dynamic executables must be
	 * executable to be exec'ed.  Shared objects need not be executable to
	 * be mapped with a dynamic executable, however, by convention they're
	 * supposed to be executable.
	 */
	if (access(fname, X_OK) != 0) {
		if (ehdr.e_type == ET_EXEC) {
			(void) fprintf(stderr, MSG_INTL(MSG_USP_NOTEXEC_1),
				cname, fname);
			return (1);
		}
		(void) fprintf(stderr, MSG_INTL(MSG_USP_NOTEXEC_2), cname,
		    fname);
	}

	/*
	 * read program header and check for dynamic section and interpreter
	 */
	for (dynamic = 0, cnt = 0; cnt < (int)ehdr.e_phnum; cnt++) {
		if (gelf_getphdr(elf, cnt, &phdr) == NULL) {
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_GETPHDR),
				cname, fname, elf_errmsg(-1));
			return (1);
		}

		if (phdr.p_type == PT_DYNAMIC) {
			dynamic = 1;
			break;
		}

		/*
		 * If fflag is not set, and euid == root, and the interpreter
		 * does not live under /usr/lib or /etc/lib then don't allow
		 * ldd to execute the image.  This prevents someone creating a
		 * `trojan horse' by substituting their own interpreter that
		 * could preform privileged operations when ldd is against it.
		 */
		if (!fflag && (phdr.p_type == PT_INTERP) && (geteuid() == 0)) {
			/*
			 * Does the interpreter live under a trusted directory.
			 */
			char *interpreter = elf_getident(elf, 0) +
				phdr.p_offset;

			if ((strncmp(interpreter, LIBDIR, LIBDIRLEN) != 0) &&
			    (strncmp(interpreter, ETCDIR, ETCDIRLEN) != 0)) {
				(void) fprintf(stderr, MSG_INTL(MSG_USP_ELFINS),
					cname, fname, interpreter);
				return (1);
			}
		}
	}

	/*
	 * Catch the case of a static executable (ie, an ET_EXEC that has a set
	 * of program headers but no PT_DYNAMIC).
	 */
	if (ehdr.e_phnum && !dynamic) {
		(void) fprintf(stderr, MSG_INTL(MSG_USP_NODYNORSO), cname,
		    fname);
		return (1);
	}

	load = load_elf;

	/*
	 * Run the required program (shared and relocatable objects require the
	 * use of lddstub).
	 */
	if (ehdr.e_type == ET_EXEC)
		return (run(nfile, cname, fname, fname));
	else {
		if (run_class == ELFCLASS32)
			return (run(nfile, cname, fname,
			    (char *)MSG_ORIG(MSG_PTH_LDDSTUB)));
		else {
			/* run_class == ELFCLASS64 */
			if (ehdr.e_machine == EM_IA_64)
				return (run(nfile, cname, fname,
				    (char *)MSG_ORIG(MSG_PTH_LDDSTUBIA64)));
			else
				return (run(nfile, cname, fname,
				    (char *)MSG_ORIG(MSG_PTH_LDDSTUBSPARCV9)));
		}
	}
}


static int
aout_check(int nfile, char *fname, char *cname, int fd, int fflag)
{
	struct exec	aout;
	int		err;

	if (lseek(fd, 0, SEEK_SET) != 0) {
		err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_LSEEK), cname, fname,
		    strerror(err));
		return (1);
	}
	if (read(fd, (char *)&aout, sizeof (struct exec)) !=
	    sizeof (struct exec)) {
		err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_READ), cname, fname,
		    strerror(err));
		return (1);
	}
	if (aout.a_machtype != M_SPARC) {
		(void) fprintf(stderr, MSG_INTL(MSG_USP_UNKNOWN), cname, fname);
		return (1);
	}
	if (N_BADMAG(aout) || !aout.a_dynamic) {
		(void) fprintf(stderr, MSG_INTL(MSG_USP_NODYNORSO), cname,
		    fname);
		return (1);
	}
	if (!fflag && (geteuid() == 0)) {
		(void) fprintf(stderr, MSG_INTL(MSG_USP_AOUTINS), cname, fname);
		return (1);
	}

	/*
	 * Run the required program.
	 */
	if ((aout.a_magic == ZMAGIC) &&
	    (aout.a_entry <= sizeof (struct exec))) {
		load = load_elf;
		return (run(nfile, cname, fname,
		    (char *)MSG_ORIG(MSG_PTH_LDDSTUB)));
	} else {
		load = load_aout;
		return (run(nfile, cname, fname, fname));
	}
}


/*
 * Run the required program, setting the preload and trace environment
 * variables accordingly.
 */
static int
run(int nfile, char *cname, char *fname, char *ename)
{
	char		*str;
	char		ndx;
	int		pid, status;
	const char	*format = "%s=./%s %s";

	if (fname != ename) {
		for (str = fname; *str; str++)
			if (*str == '/') {
				format = (const char *)"%s=%s %s";
				break;
		}
		if ((str = (char *)malloc(strlen(fname) +
		    strlen(prefile) + LD_PRELOAD_SIZE + 4)) == 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_MALLOC), cname);
			exit(1);
		}

		/*
		 * When using ldd(1) to analyze a shared object we preload the
		 * shared object with lddstub.  Any additional preload
		 * requirements are added after the object being analyzed, this
		 * allows us to skip the first object but produce diagnostics
		 * for each other preloaded object.
		 */
		(void) sprintf(str, format, preload, fname, prefile);
		ndx = '2';

		if (putenv(str) != 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_ENV_FAILED), cname);
			return (1);
		}
	} else
		ndx = '1';

	if ((pid = fork()) == -1) {
		int	err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_FORK), cname,
		    strerror(err));
		return (1);
	}

	if (pid) {				/* parent */
		while (wait(&status) != pid)
			;
		if (WIFSIGNALED(status)) {
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_EXEC), cname,
			    fname);
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_EXEC_SIG),
			    (WSIGMASK & status), ((status & WCOREFLG) ?
			    MSG_INTL(MSG_SYS_EXEC_CORE) :
			    MSG_ORIG(MSG_STR_EMPTY)));
			status = 1;
		} else if (WHIBYTE(status)) {
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_EXEC), cname,
			    fname);
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_EXEC_STAT),
			    WHIBYTE(status));
			status = 1;
		}
	} else {				/* child */
		load[LD_LOAD_SIZE - 1] = ndx;
		if (putenv(load) != 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_ENV_FAILED), cname);
			return (1);
		}

		if (nfile > 1)
			(void) printf("%s:\n", fname);
		(void) fflush(stdout);
		if ((execl(ename, ename, (char *)0)) == -1) {
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_EXEC), cname,
			    fname);
			perror(ename);
			_exit(0);
			/* NOTREACHED */
		}
	}

	/*
	 * If there is more than one filename to process make sure the
	 * preload environment variable is reset (this makes sure we remove
	 * any preloading that had been established to process a shared object).
	 */
	if ((nfile > 1) && (fname != ename)) {
		if (putenv((char *)prestr) != 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_ENV_FAILED), cname);
			return (1);
		}
		free(str);
	}
	return (status);
}

const char *
_ldd_msg(Msg mid)
{
	return (gettext(MSG_ORIG(mid)));
}
