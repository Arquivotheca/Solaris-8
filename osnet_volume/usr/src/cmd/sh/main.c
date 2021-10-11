/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)main.c	1.28	99/11/09 SMI"

/*
 * UNIX shell
 */

#include	"defs.h"
#include	"sym.h"
#include	"timeout.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/wait.h>
#include	"dup.h"
#include	"sh_policy.h"

#ifdef RES
#include	<sgtty.h>
#endif

pid_t mypid, mypgid, mysid;

static BOOL	beenhere = FALSE;
unsigned char		tmpout[20] = "/tmp/sh-";
struct fileblk	stdfile;
struct fileblk *standin = &stdfile;
int mailchk = 0;

static unsigned char	*mailp;
static long	*mod_time = 0;


#if vax
char **execargs = (char **)(0x7ffffffc);
#endif

#if pdp11
char **execargs = (char **)(-2);
#endif


static int	exfile();
extern unsigned char 	*simple();


main(c, v, e)
int	c;
char	*v[];
char	*e[];
{
	register int	rflag = ttyflg;
	register int	pfshflag = 1;	/* local pfsh flag */
	int		rsflag = 1;	/* local restricted flag */
	register unsigned char *flagc = flagadr;
	struct namnod	*n;

	mypid = getpid();
	mypgid = getpgid(mypid);
	mysid = getsid(mypid);

	/*
	 * initialize storage allocation
	 */

	stakbot = 0;
	addblok((unsigned)0);

	/* If **v equal to "-" (ex. -sh), this is a login shell */
	if (**v == '-') {
		signal(SIGXCPU, SIG_DFL);
		signal(SIGXFSZ, SIG_DFL);
	}

	stdsigs();

	/*
	 * set names from userenv
	 */

	setup_env();

	/*
	 * LC_MESSAGES is set here so that early error messages will
	 * come out in the right style.
	 * Note that LC_CTYPE is done later on and is *not*
	 * taken from the previous environ
	 */

#if defined(SETLOCALE_FIX)	/* compiled for shared lib. 1107147 */
	(void) setlocale(LC_ALL, "");
#endif /* SETLOCALE_FIX */
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

#ifdef DYNAMIC
	/*
	 * pfshflag is zero if SHELL variable is set in environment and
	 * the simple file part of the value is pfsh
	 */
	if (n = findnam("SHELL")) {
		if (eq("pfsh", simple(n->namval))) {
			pfshflag = 0;
		}
	}
	/*
	 * a shell is also pf shell if the simple name of argv(0) is
	 * pfsh or -pfsh in its simple name
	 */
	if (c > 0 && (eq("pfsh", simple(*v)) || eq("-pfsh", simple(*v)))) {
		pfshflag = 0;
	}
	if (pfshflag == 0) {
		flags |= pfshflg;
		secpolicy_init();
	}
#endif	/* DYNAMIC */

	/*
	 * 'rsflag' is zero if SHELL variable is
	 *  set in environment and
	 *  the simple file part of the value.
	 *  is rsh
	 */
	if (n = findnam("SHELL"))
	{
		if (eq("rsh", simple(n->namval)))
			rsflag = 0;
	}

	/*
	 * a shell is also restricted if the simple name of argv(0) is
	 * rsh or -rsh in its simple name
	 */

#ifndef RES

	if (c > 0 && (eq("rsh", simple(*v)) || eq("-rsh", simple(*v))))
		rflag = 0;

#endif

	if (eq("jsh", simple(*v)) || eq("-jsh", simple(*v)))
		flags |= monitorflg;

	hcreate();
	set_dotpath();


	/*
	 * look for options
	 * dolc is $#
	 */
	dolc = options(c, v);

	if (dolc < 2)
	{
		flags |= stdflg;
		{

			while (*flagc)
				flagc++;
			*flagc++ = STDFLG;
			*flagc = 0;
		}
	}
	if ((flags & stdflg) == 0)
		dolc--;

	if ((flags & privflg) == 0) {
		register uid_t euid;
		register gid_t egid;
		register uid_t ruid;
		register gid_t rgid;

		/*
		 * Determine all of the user's id #'s for this process and
		 * then decide if this shell is being entered as a result
		 * of a fork/exec.
		 * If the effective uid/gid do NOT match and the euid/egid
		 * is < 100 and the egid is NOT 1, reset the uid and gid to
		 * the user originally calling this process.
		 */
		euid = geteuid();
		ruid = getuid();
		egid = getegid();
		rgid = getgid();
		if ((euid != ruid) && (euid < 100))
			setuid(ruid);   /* reset the uid to the orig user */
		if ((egid != rgid) && ((egid < 100) && (egid != 1)))
			setgid(rgid);   /* reset the gid to the orig user */
	}

	dolv = (unsigned char **)v + c - dolc;
	dolc--;

	/*
	 * return here for shell file execution
	 * but not for parenthesis subshells
	 */
	if (setjmp(subshell)) {
		freejobs();
		flags |= subsh;
	}

	/*
	 * number of positional parameters
	 */
	replace(&cmdadr, dolv[0]);	/* cmdadr is $0 */

	/*
	 * set pidname '$$'
	 */
	assnum(&pidadr, (long)mypid);

	/*
	 * set up temp file names
	 */
	settmp();

	/*
	 * default internal field separators
	 * Do not allow importing of IFS from parent shell.
	 * setup_env() may have set anything from parent shell to IFS.
	 * Always set the default ifs to IFS.
	 */
	assign(&ifsnod, sptbnl);

	dfault(&mchknod, MAILCHECK);
	mailchk = stoi(mchknod.namval);

	/* initialize OPTIND for getopt */

	n = lookup("OPTIND");
	assign(n, "1");
	/*
	 * make sure that option parsing starts
	 * at first character
	 */
	_sp = 1;

	/* initialize multibyte information */
	setwidth();

	if ((beenhere++) == FALSE)	/* ? profile */
	{
		if ( (!strcmp(v[0], cmdadr)) && (*(simple(cmdadr)) == '-') && (flags & privflg) == 0)
		{			/* system profile */

#ifndef RES

			if ((input = pathopen(nullstr, sysprofile)) >= 0)
				exfile(rflag);		/* file exists */

#endif
			/* user profile */

			if ((input = pathopen(homenod.namval, profile)) >= 0)
			{
				exfile(rflag);
				flags &= ~ttyflg;
			}
		}
		if (rsflag == 0 || rflag == 0) {
			if ((flags & rshflg) == 0) {
				while (*flagc)
					flagc++;
				*flagc++ = 'r';
				*flagc = '\0';
			}
			flags |= rshflg;
		}

		/*
		 * open input file if specified
		 */
		if (comdiv)
		{
			estabf(comdiv);
			input = -1;
		}
		else
		{
			input = ((flags & stdflg) ? 0 : chkopen(cmdadr, 0));

#ifdef ACCT
			if (input != 0)
				preacct(cmdadr);
#endif
			comdiv--;
		}
	}
#ifdef pdp11
	else
		*execargs = (char *)dolv;	/* for `ps' cmd */
#endif


	exfile(0);
	done(0);
}

static int
exfile(prof)
BOOL	prof;
{
	time_t	mailtime = 0;	/* Must not be a register variable */
	time_t 	curtime = 0;

	/*
	 * move input
	 */
	if (input > 0)
	{
		Ldup(input, INIO);
		input = INIO;
	}


	setmode(prof);

	if (setjmp(errshell) && prof)
	{
		close(input);
		(void) endjobs(0);
		return;
	}
	/*
	 * error return here
	 */

	loopcnt = peekc = peekn = 0;
	fndef = 0;
	nohash = 0;
	iopend = 0;

	if (input >= 0)
		initf(input);
	/*
	 * command loop
	 */
	for (;;)
	{
		tdystak(0);
		stakchk();	/* may reduce sbrk */
		exitset();

		if ((flags & prompt) && standin->fstak == 0 && !eof)
		{

			if (mailp)
			{
				time(&curtime);

				if ((curtime - mailtime) >= mailchk)
				{
					chkmail();
					mailtime = curtime;
				}
			}

			/* necessary to print jobs in a timely manner */
			if (trapnote & TRAPSET)
				chktrap();

			prs(ps1nod.namval);

#ifdef TIME_OUT
			alarm(TIMEOUT);
#endif

		}

		trapnote = 0;
		peekc = readwc();
		if (eof) {
			if (endjobs(JOB_STOPPED))
				return;
			eof = 0;
		}

#ifdef TIME_OUT
		alarm(0);
#endif

		{
			register struct trenod *t;
			t = cmd(NL, MTFLG);
			if (t == NULL && flags & ttyflg)
				freejobs();
			else
				execute(t, 0, eflag);
		}

		eof |= (flags & oneflg);

	}
}

chkpr()
{
	if ((flags & prompt) && standin->fstak == 0)
		prs(ps2nod.namval);
}

settmp()
{
	int i;
	i = ltos(mypid);
	serial = 0;
	tmpname = movstr(numbuf + i, &tmpout[TMPNAM]);
}

Ldup(fa, fb)
register int	fa, fb;
{
#ifdef RES

	dup(fa | DUPFLG, fb);
	close(fa);
	ioctl(fb, FIOCLEX, 0);

#else

	if (fa >= 0) {
		if (fa != fb)
		{
			close(fb);
			fcntl(fa, 0, fb); /* normal dup */
			close(fa);
		}
		fcntl(fb, 2, 1);	/* autoclose for fb */
	}

#endif
}


chkmail()
{
	register unsigned char 	*s = mailp;
	register unsigned char	*save;

	long	*ptr = mod_time;
	unsigned char	*start;
	BOOL	flg;
	struct stat	statb;

	while (*s) {
		start = s;
		save = 0;
		flg = 0;

		while (*s) {
			if (*s != COLON) {
				if (*s == '%' && save == 0)
					save = s;

				s++;
			} else {
				flg = 1;
				*s = 0;
			}
		}

		if (save)
			*save = 0;

		if (*start && stat((const char *)start, &statb) >= 0) {
			if (statb.st_size && *ptr &&
			    statb.st_mtime != *ptr) {
				if (save) {
					prs(save+1);
					newline();
				}
				else
					prs(mailmsg);
			}
			*ptr = statb.st_mtime;
		} else if (*ptr == 0)
			*ptr = 1;

		if (save)
			*save = '%';

		if (flg)
			*s++ = COLON;

		ptr++;
	}
}


setmail(mailpath)
	unsigned char *mailpath;
{
	register unsigned char	*s = mailpath;
	register int 	cnt = 1;

	long	*ptr;

	free(mod_time);
	if (mailp = mailpath)
	{
		while (*s)
		{
			if (*s == COLON)
				cnt += 1;

			s++;
		}

		ptr = mod_time = (long *)alloc(sizeof (long) * cnt);

		while (cnt)
		{
			*ptr = 0;
			ptr++;
			cnt--;
		}
	}
}

void
setwidth()
{
	unsigned char *name = lookup("LC_CTYPE")->namval;
	if (!name || !*name)
		name = lookup("LANG")->namval;
#if defined(SETLOCALE_FIX)	/* compiled for shared lib. 1107147 */
	if (!name || !*name)
		(void) setlocale(LC_CTYPE, "C");
	else
		(void) setlocale(LC_CTYPE, (const char *)name);
#endif /* SETLOCALE_FIX */
}

setmode(prof)
{
	/*
	 * decide whether interactive
	 */

	if ((flags & intflg) ||
	    ((flags&oneflg) == 0 &&
	    isatty(output) &&
	    isatty(input)))

	{
		dfault(&ps1nod, (geteuid() ? stdprompt : supprompt));
		dfault(&ps2nod, readmsg);
		flags |= ttyflg | prompt;
		if (mailpnod.namflg != N_DEFAULT)
			setmail(mailpnod.namval);
		else
			setmail(mailnod.namval);
		startjobs();
	}
	else
	{
		flags |= prof;
		flags &= ~prompt;
	}
}

/*
 * A generic call back routine to output error messages from the
 * policy backing functions called by pfsh.
 *
 * msg must contain '\n' if a new line is to be printed.
 */
void
secpolicy_print(int level, const char *msg)
{
	switch (level) {
	case SECPOLICY_WARN:
	default:
		prs(msg);	/* prs() does gettext() */
		return;
	case SECPOLICY_ERROR:
		error(msg);
		break;
	}
}
