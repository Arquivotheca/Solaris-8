/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.45	99/05/04 SMI"

/*
 * adb - main command loop and error/interrupt handling
 */
#include <stdio.h>
#include "adb.h"
#include "fpascii.h"
#include <kvm.h>
#include <setjmp.h>
#include <sys/systeminfo.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

jmp_buf exitbuffer;
#define	setexit() setjmp(exitbuffer)
#define	reset()   longjmp(exitbuffer, 1)

int	infile;
int	maxpos;
int	wtflag;
int	cmd_line_prompt = 0;		/* 1 if -P is used */
static	void	onintr();
#ifndef	KADB
void	setpathlist();
char	*getenv(const char *name);
#endif	KADB
int	exitflg;
extern int adb_debug;

int	eof;

char	*Ipath = NULL;
char	*prompt = NULL;
char	*swapfil = NULL;
kvm_t	*kvmd;
/*
 * Subdirectory with ISA-specific stuff
 * Defaults to "." unless specified otherwise in setsym()
 */
char	*isadir = ".";

void
main(argc, argv)
	register char **argv;
	int argc;
{
	extern char *optarg;
	extern optind, outfile;
#ifndef sparc
	extern mc68881, fpa_avail, fpa_disasm;
#endif /* !sparc */
	int c;

#ifndef sparc
#ifndef KADB
	fpa_disasm = fpa_avail = is_fpa_avail();
	if (fpa_avail)
		db_printf(2, "main: FPU operations available");
	else
		db_printf(2, "main: FPU operations NOT available");
#endif /* !KADB */
#endif /* !sparc */

another:
	while ((c = getopt(argc, argv, "wkFV:D:P:I:")) != EOF) {
		switch (c) {
		case 'w':
			wtflag = 2;	/* suitable for open() */
			break;
		case 'k':
			kernel = LIVE;	/* at least */
			db_printf(9, "main: debugging kernel");
			break;
		case 'F':
			Fflag = 1;	/* force the takeover */
			break;
		case 'D':
			adb_debug = atoi(optarg);
			if (adb_debug < 0) {
				adb_debug = 0;
				error("must have adb_debug_level >= 0");
			} else if (adb_debug > ADB_DEBUG_MAX)
				adb_debug = ADB_DEBUG_MAX;
			db_printf(9, "main: adb_debug=%D", adb_debug);
			break;
		case 'P':
			if (strlen(optarg) >= PROMPT_LEN_MAX - 1)
				error("too many characters in the prompt");
			prompt = optarg;
			cmd_line_prompt = 1;
			db_printf(9, "main: prompt='%s'", prompt);
			break;
		case 'I':
			Ipath = optarg;
			break;
#if	defined(sparc) && !defined(KADB)
		case 'V':
		{
			change_dismode(atoi(optarg), 1);
			break;
		}
#endif
		default:
			printf("usage: adb [-w] [-k] [-V mode] [-I dir]"
			    " [-P prompt] [objfile [corfil [swapfil]]]\n");
			exit(2);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		symfil = argv[0];
	else if (kernel) {
		symfil = LIVE_KERNEL_NAMELIST;
		argc++;
	}
	db_printf(9, "main: object file='%s'", symfil);

	if (argc > 1)
		corfil = argv[1];
	else if (kernel) {
		corfil = LIVE_KERNEL_COREFILE;	/* criterion for LIVE kernel */
		argc++;
	}
	db_printf(9, "main: core file='%s'", corfil);

	if (argc > 2) {
		if (kernel) {
			swapfil = argv[2];
			db_printf(9, "main: swap file='%s'", swapfil);
		} else {
			printf("swapfile only valid if -k\n");
			exit(2);
		}
	}

	xargc = argc + 1;

	kopen();		/* init libkvm code, if kernel */
	setsym();
	setcor();
	setvar();
	setty();
#ifndef	KADB
	if (Ipath == NULL)
		setpathlist();		/* setup full default path for macros */
#endif	KADB
	adb_pgrpid = getpgid(0); 	/* svr4 getpgid = getpgrp in 4.0 */
	db_printf(2, "main: adb_pgrpid=%D", adb_pgrpid);
	if ((sigint = (void (*)(int)) signal(SIGINT, SIG_IGN)) != SIG_IGN) {
		sigint = onintr;
		signal(SIGINT, onintr);
	}
	sigqit = (void (*)(int)) signal(SIGQUIT, SIG_IGN);
	setexit();
	adbtty();
	db_printf(9, "main: executing=%D", executing);
	if (executing)
		delbp();
	executing = 0;
	db_printf(9, "main: set executing=0");
	for (;;) {
		killbuf();
		if (errflg != NULL) {
			printf("%s\n", errflg);
			errflg = NULL;
			exitflg = 1;
		}
		if (interrupted) {
			interrupted = 0;
			if (prompt == NULL)
				(void) printf("adb\n");
		}
		lp = 0;
		if (prompt != NULL && !infile)
			(void) write(outfile, prompt, strlen(prompt) + 1);
		(void) rdc();
		lp--;
		if (eof) {
			if (infile) {
				iclose(-1, 0);
				eof = 0;
				reset();
				/*NOTREACHED*/
			} else
				done();
		} else
			exitflg = 0;
		(void) command((char *)NULL, lastcom);
		if (lp && lastc != '\n')
			error("newline expected");
	}
}

void
done(void)
{
	db_printf(9, "done: called");
	endpcs();
	printc('\n');
	exit(exitflg);
}

void
chkerr(void)
{
	if (errflg != NULL || interrupted)
		error(errflg);
}

void
kopen(void)
{
	db_printf(9, "kopen: called");
	if (kvmd != NULL)
		(void) kvm_close(kvmd);
	if (kernel &&
	    ((kvmd = kvm_open(symfil, corfil, swapfil, wtflag, NULL)) ==
	    NULL) &&
	    ((kvmd = kvm_open(symfil, corfil, swapfil, 0, "Cannot adb -k")) ==
	    NULL)) {
		exit(2);
	}
}

static void
doreset(void)
{
	db_printf(9, "doreset: called");
	iclose(0, 1);
	oclose();
	reset();
	/*NOTREACHED*/
}

void
error(s)
	char *s;
{
	errflg = s;
	doreset();
	/*NOTREACHED*/
}

static void
onintr(sig)
	int sig;
{
	(void) signal(sig, onintr);

	db_printf(2, "onintr: sig=%D", sig);
	interrupted = 1;
	lastcom = 0;
	doreset();
	/*NOTREACHED*/
}

void
shell(void)
{
	int rc, status, unixpid;
	void (*oldsig)();
	char *argp = lp;
	char *shell = getenv("SHELL");
#ifdef VFORK
	char oldstlp;
#endif /* VFORK */

	db_printf(9, "shell: called");
	if (shell == 0)
		shell = "/bin/sh";
	while (lastc != '\n')
		(void) rdc();
#ifndef VFORK
	if ((unixpid = fork()) == 0)
#else /* VFORK */
	db_printf(9, "shell: using vfork() instead of fork()");
	oldstlp = *lp;
	if ((unixpid = vfork()) == 0)
#endif /* !VFORK */
	{
		signal(SIGINT, sigint);
		signal(SIGQUIT, sigqit);
		*lp = 0;
		execl(shell, "sh", "-c", argp, 0);
		_exit(16);
	}
#ifdef VFORK
	lp = oldstlp;
#endif /* VFORK */
	if (unixpid == -1)
		error("try again");
	oldsig = (void (*)(int)) signal(SIGINT, SIG_IGN);
	while ((rc = wait(&status)) != unixpid && rc != -1)
		continue;
	signal(SIGINT, oldsig);
	setty();
	printf("!");
	lp--;
}

#ifndef	KADB
char IpathArray[MAXNAMELEN];

/*
 * This routine builds the default search path "Ipath".
 * The platform independent directory can be, and is, hardcoded.
 * The work is in building the default platform dependent path.
 */
void
setpathlist()
{
	char default_template[MAXNAMELEN];
	char platform_name[MAXNAMELEN];		/* Really big */
	struct nlist adb_nl[] = {
		{ "platform"},
#define	X_PLATFORM	0
		{ "" },
	};

	Ipath = IpathArray;

	(void) sprintf(default_template, "%s/%s:%s/%s",
			"/usr/platform/%s/lib/adb", isadir,
			"/usr/lib/adb", isadir);

	/*
	 * Should any of the support routines fail (sysinfo, malloc,...) it
	 * does little good to complain.  Just set the search path to the
	 * invariant part of the path.
	 */
	(void) sprintf(Ipath, "%s/%s", "/usr/lib/adb", isadir);

	/*
	 * If looking at a kernel image, get the platform name from the image,
	 * otherwise get it from sysinfo(2).  This way kernel core images can
	 * be moved from platform to platform and locate the correct macros for
	 * the image.
	 */
	if ((kernel && (kvm_nlist(kvmd, adb_nl) == 0) &&
	    ((kvm_read(kvmd, adb_nl[X_PLATFORM].n_value,
	    platform_name, MAXNAMELEN)) != -1)) ||
	    (sysinfo(SI_PLATFORM, platform_name, MAXNAMELEN) <= MAXNAMELEN))
		(void) sprintf(Ipath, default_template, platform_name);
}
#endif	!KADB
