/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ps.c	1.29	99/04/02 SMI"	/* SVr4.0 1.4	*/

/*
 * *******************************************************************
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 * ********************************************************************
 */

/*
 * ps -- print things about processes.
 */

#define	_SYSCALL32

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <procfs.h>
#include <sys/param.h>
#include <sys/ttold.h>
#include <libelf.h>
#include <gelf.h>
#include <locale.h>
#include <wctype.h>
#include <stdarg.h>
#include <sys/proc.h>

#define	NTTYS	2	/* max ttys that can be specified with the -t option */
			/* only one tty can be specified with SunOS ps */
#define	SIZ	30	/* max processes that can be specified with -p and -g */
#define	ARGSIZ	30	/* size of buffer holding args for -t, -p, -u options */

#define	FSTYPE_MAX	8

struct psent {
	psinfo_t *psinfo;
	char *psargs;
	int found;
};

static	int	tplen, maxlen, twidth;
static	char	hdr[81];
static	struct	winsize win;

static	int	retcode = 1;
static	int	lflg;	/* long format */
static	int	uflg;	/* user-oriented output */
static	int	aflg;	/* Display all processes */
static	int	eflg;	/* Display environment as well as arguments */
static	int	gflg;	/* Display process group leaders */
static	int	tflg;	/* Processes running on specific terminals */
static	int	rflg;	/* Running processes only flag */
static	int	Sflg;	/* Accumulated time plus all reaped children */
static	int	xflg;	/* Include processes with no controlling tty */
static	int	cflg;	/* Display command name */
static	int	vflg;	/* Virtual memory-oriented output */
static	int	nflg;	/* Numerical output */
static	int	pflg;	/* Specific process id passed as argument */
static	int	Uflg;	/* Update private database, ups_data */
static	int	errflg;

static	char	*gettty();
static	char	argbuf[ARGSIZ];
static	char	*parg;
static	char	*p1;		/* points to successive option arguments */
static	uid_t	my_uid;
static char	stdbuf[BUFSIZ];

static	int	ndev;		/* number of devices */
static	int	maxdev;		/* number of devl structures allocated */

#define	DNSIZE	14
static	struct devl {		/* device list	 */
	char	dname[DNSIZE];	/* device name	 */
	major_t	major;		/* device number */
	minor_t	minor;
} *devl;

static	int	nmajor;		/* number of remembered major device numbers */
static	int	maxmajor;	/* number of major device numbers allocated */
static	int	*majordev;	/* array of remembered major device numbers */

/*
 * struct for the symbolic wait channel info
 */

static	int	nchans;		/* total # of wait channels */

#define	WNAMESIZ	12
#define	WSNAMESIZ	8
#define	WTSIZ		95

static	struct wchan {
	char		wc_name[WNAMESIZ+1];	/* symbolic name */
	GElf_Addr	wc_addr;		/* addr in kmem */
} *wchanhd;				/* an array sorted by wc_addr */

#define	NWCINDEX	10		/* the size of the index array */

static	GElf_Addr	wchan_index[NWCINDEX];	/* used to speed searches */

/*
 * names listed here get mapped
 */
static	struct wchan_map {
	const	char	*map_from;
	const	char	*map_to;
} wchan_map_list[] = {
	{ "u", "pause" },
	{ NULL, NULL },
};

static	char	*tty[NTTYS];	/* for t option */
static	int	ntty = 0;
static	pid_t	pidsave;
static	int	pidwidth;

static	char	*procdir = "/proc";	/* standard /proc directory */
static	void	usage();		/* print usage message and quit */
static	void	getdev();		/* reconstruct /tmp/ps_data */
static	int	getdevcalled = 0;	/* if == 1, getdev() has been called */
static	void	wrdata(void);
static	void	getarg(void);
static	void	pswrite(int fd, char *bp, unsigned bs);
static	void	prtime(timestruc_t st);
static	void	przom(psinfo_t *psinfo);
static	void	addchan(const char *name, GElf_Addr addr);
static	void	gdev(char *objptr, struct stat *statp, int remember);
static	void	getwchan(void);
static	int	num(char *);
static	int	readata(void);
static	int	preadargs(int, psinfo_t *, char *);
static	int	preadenvs(int, psinfo_t *, char *);
static	int	prcom(int, psinfo_t *, char *);
static	int	psread(int, char *, unsigned int);
static	int	namencnt(char *, int, int);
static	int	pscompare(const void *, const void *);
static	void	getdevdir(char *, int);
static	char	*getchan(GElf_Addr);
static	int	wchancomp(const void *, const void *);
static	char	*err_string(int);

extern int	scrwidth(wchar_t);	/* header file? */

int
main(int argc, char **argv)
{
	psinfo_t info;		/* process information structure from /proc */
	char *psargs = NULL;	/* pointer to buffer for -w and -ww options */
	struct psent *psent;
	int entsize;
	int nent;
	pid_t maxpid;

	char	**ttyp = tty;
	char	*tmp;
	char	*p;
	int	c;
	pid_t	pid;		/* pid: process id */
	pid_t	ppid;		/* ppid: parent process id */
	int	i, found;

	unsigned	size;

	DIR *dirp;
	struct dirent *dentp;
	char	psname[100];
	char	asname[100];
	int	pdlen;

	(void) setlocale(LC_ALL, "");

	my_uid = getuid();

	/*
	 * calculate width of pid fields based on configured MAXPID
	 * (must be at least 5 to retain output format compatibility)
	 */
	maxpid = (pid_t) sysconf(_SC_MAXPID);
	pidwidth = 1;
	while ((maxpid /= 10) > 0)
		++pidwidth;
	pidwidth = pidwidth < 5 ? 5 : pidwidth;

	if (ioctl(1, TIOCGWINSZ, &win) == -1)
		twidth = 80;
	else
		twidth = (win.ws_col == 0 ? 80 : win.ws_col);

	/* add the '-' for BSD compatibility */
	if (argc > 1) {
		if (argv[1][0] != '-' && !isdigit(argv[1][0])) {
			tmp = malloc(strlen(argv[1]) + 2);
			(void) sprintf(tmp, "%s%s", "-", argv[1]);
			argv[1] = tmp;
		}
	}

	setbuf(stdout, stdbuf);
	while ((c = getopt(argc, argv, "lcaengrSt:xuvwU")) != EOF)
		switch (c) {
		case 'g':
			gflg++;	/* include process group leaders */
			break;
		case 'c':	/* display internal command name */
			cflg++;
			break;
		case 'r':	/* restrict output to running processes */
			rflg++;
			break;
		case 'S': /* display time by process and all reaped children */
			Sflg++;
			break;
		case 'x':	/* process w/o controlling tty */
			xflg++;
			break;
		case 'l':	/* long listing */
			lflg++;
			uflg = vflg = 0;
			break;
		case 'u':	/* user-oriented output */
			uflg++;
			lflg = vflg = 0;
			break;
		case 'U':	/* update private database ups_data */
			Uflg++;
			break;
		case 'w':	/* increase display width */
			if (twidth < 132)
				twidth = 132;
			else	/* second w option */
				twidth = NCARGS;
			break;
		case 'v':	/* display virtual memory format */
			vflg++;
			lflg = uflg = 0;
			break;
		case 'a':
			/*
			 * display all processes except process group
			 * leaders and processes w/o controlling tty
			 */
			aflg++;
			gflg++;
			break;
		case 'e':
			/* Display environment along with aguments. */
			eflg++;
			break;
		case 'n':	/* Display numerical output */
			nflg++;
			break;
		case 't':	/* restrict output to named terminal */
#define	TSZ	30
			tflg++;
			gflg++;
			xflg = 0;

			p1 = optarg;
			do {	/* only loop through once (NTTYS = 2) */
				parg = argbuf;
				if (ntty >= NTTYS-1)
					break;
				getarg();
				if ((p = malloc(TSZ)) == NULL) {
					(void) fprintf(stderr,
					    "ps: no memory\n");
					exit(1);
				}
				(void) memset(p, 0, TSZ);
				size = TSZ;
				if (isdigit(*parg)) {
					(void) strcpy(p, "tty");
					size -= 3;
				}

				if (parg && *parg == '?')
					xflg++;

				(void) strncat(p, parg, (int)size);
				*ttyp++ = p;
				ntty++;
			} while (*p1);
			break;
		default:			/* error on ? */
			errflg++;
			break;
		}

	if (errflg)
		usage();

	if (optind + 1 < argc) { /* more than one additional argument */
		(void) fprintf(stderr, "ps: too many arguments\n");
		usage();
	}

	if (optind < argc) { /* user specified a specific proc id */
		pflg++;
		p1 = argv[optind];
		parg = argbuf;
		getarg();
		if (!num(parg)) {
			(void) fprintf(stderr,
	"ps: %s is an invalid non-numeric argument for a process id\n", parg);
			usage();
		}
		pidsave = (pid_t)atol(parg);
		aflg = rflg = xflg = 0;
		gflg++;
	}

	if (tflg)
		*ttyp = 0;

	if (Uflg) {	/* update psfile */

		/* allow update only if permissions for real uid allow it */
		(void) setuid(my_uid);

		getdev();
		getwchan();
		wrdata();
		exit(0);
	}

	if (!readata()) {	/* get data from psfile */
		getdev();
		getwchan();
		wrdata();
	}

	/* allocate an initial guess for the number of processes */
	entsize = 1024;
	psent = malloc(entsize * sizeof (struct psent));
	if (psent == NULL) {
		(void) fprintf(stderr, "ps: no memory\n");
		exit(1);
	}
	nent = 0;	/* no active entries yet */

	if (lflg) {
		(void) sprintf(hdr,
		    " F   UID%*s%*s %%C PRI NI   SZ  RSS    "
		    "WCHAN S TT        TIME COMMAND", pidwidth + 1, "PID",
		    pidwidth + 1, "PPID");
	} else if (uflg) {
		if (nflg)
			(void) sprintf(hdr,
			    "   UID%*s %%CPU %%MEM   SZ  RSS "
			    "TT       S    START  TIME COMMAND",
			    pidwidth + 1, "PID");
		else
			(void) sprintf(hdr,
			    "USER    %*s %%CPU %%MEM   SZ  RSS "
			    "TT       S    START  TIME COMMAND",
			    pidwidth + 1, "PID");
	} else if (vflg) {
		(void) sprintf(hdr,
		    "%*s TT       S  TIME SIZE  RSS %%CPU %%MEM "
		    "COMMAND", pidwidth + 1, "PID");
	} else
		(void) sprintf(hdr, "%*s TT       S  TIME COMMAND",
		    pidwidth + 1, "PID");

	twidth = twidth - strlen(hdr) + 6;
	(void) printf("%s\n", hdr);

	if (twidth > PRARGSZ && (psargs = malloc(twidth)) == NULL) {
		(void) fprintf(stderr, "ps: no memory\n");
		exit(1);
	}

	/*
	 * Determine which processes to print info about by searching
	 * the /proc directory and looking at each process.
	 */
	if ((dirp = opendir(procdir)) == NULL) {
		(void) fprintf(stderr, "ps: cannot open PROC directory %s\n",
		    procdir);
		exit(1);
	}

	(void) strcpy(psname, procdir);
	pdlen = strlen(psname);
	psname[pdlen++] = '/';

	/* for each active process --- */
	while (dentp = readdir(dirp)) {
		int	psfd;	/* file descriptor for /proc/nnnnn/psinfo */
		int	asfd;	/* file descriptor for /proc/nnnnn/as */

		if (dentp->d_name[0] == '.')		/* skip . and .. */
			continue;
		(void) strcpy(psname + pdlen, dentp->d_name);
		(void) strcpy(asname, psname);
		(void) strcat(psname, "/psinfo");
		(void) strcat(asname, "/as");
retry:
		if ((psfd = open(psname, O_RDONLY)) == -1)
			continue;
		asfd = -1;
		if ((psargs != NULL || eflg) &&
		    (asfd = open(asname, O_RDONLY)) == -1) {
			(void) close(psfd);
			continue;
		}

		/*
		 * Get the info structure for the process
		 */
		if (read(psfd, &info, sizeof (info)) != sizeof (info)) {
			int	saverr = errno;

			(void) close(psfd);
			if (asfd > 0)
				(void) close(asfd);
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				(void) fprintf(stderr, "ps: read() on %s: %s\n",
				    psname, err_string(saverr));
			continue;
		}
		(void) close(psfd);

		found = 0;
		if (info.pr_lwp.pr_state == 0)		/* can't happen? */
			goto closeit;
		pid = info.pr_pid;
		ppid = info.pr_ppid;

		/* Display only process from command line */
		if (pflg) {	/* pid in arg list */
			if (pidsave == pid)
				found++;
			else
				goto closeit;
		}

		/*
		 * Omit "uninteresting" processes unless 'g' option.
		 */
		if ((ppid == 1) && !(gflg))
			goto closeit;

		/*
		 * Omit non-running processes for 'r' option
		 */
		if (rflg &&
		    !(info.pr_lwp.pr_sname == 'O' ||
		    info.pr_lwp.pr_sname == 'R'))
			goto closeit;

		if (!found && !tflg && !aflg && info.pr_euid != my_uid)
			goto closeit;

		/*
		 * Read the args for the -w and -ww cases
		 */
		if ((psargs != NULL && preadargs(asfd, &info, psargs) == -1) ||
		    (eflg && preadenvs(asfd, &info, psargs) == -1)) {
			int	saverr = errno;

			(void) close(asfd);
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				(void) fprintf(stderr,
				    "ps: read() on %s: %s\n",
				    asname, err_string(saverr));
			continue;
		}

		if (nent >= entsize) {
			entsize *= 2;
			psent = (struct psent *)realloc((char *)psent,
				entsize * sizeof (struct psent));
			if (psent == NULL) {
				(void) fprintf(stderr, "ps: no memory\n");
				exit(1);
			}
		}
		if ((psent[nent].psinfo = malloc(sizeof (psinfo_t)))
		    == NULL) {
			(void) fprintf(stderr, "ps: no memory\n");
			exit(1);
		}
		*psent[nent].psinfo = info;
		if (psargs == NULL)
			psent[nent].psargs = NULL;
		else {
			if ((psent[nent].psargs = malloc(strlen(psargs)+1))
			    == NULL) {
				(void) fprintf(stderr, "ps: no memory\n");
				exit(1);
			}
			(void) strcpy(psent[nent].psargs, psargs);
		}
		psent[nent].found = found;
		nent++;
closeit:
		if (asfd > 0)
			(void) close(asfd);
	}

	(void) closedir(dirp);

	qsort((char *)psent, nent, sizeof (psent[0]), pscompare);

	for (i = 0; i < nent; i++) {
		struct psent *pp = &psent[i];
		if (prcom(pp->found, pp->psinfo, pp->psargs)) {
			(void) printf("\n");
			retcode = 0;
		}
	}

	return (retcode);
}

static void
usage()		/* print usage message and quit */
{
	static char usage1[] = "ps [ -aceglnrSuUvwx ] [ -t term ] [ num ]";

	(void) fprintf(stderr, "usage: %s\n", usage1);
	exit(1);
}

/*
 * Read the process arguments from the process.
 * This allows >PRARGSZ characters of arguments to be displayed but,
 * unlike pr_psargs[], the process may have changed them.
 */
#define	NARG	100
static int
preadargs(int pfd, psinfo_t *psinfo, char *psargs)
{
	off_t argvoff = (off_t)psinfo->pr_argv;
	size_t len;
	char *psa = psargs;
	int bsize = twidth;
	int narg = NARG;
	off_t argv[NARG];
	off_t argoff;
	off_t nextargoff;
	int i;
#ifdef _LP64
	caddr32_t argv32[NARG];
	int is32 = (psinfo->pr_dmodel != PR_MODEL_LP64);
#endif

	if (psinfo->pr_nlwp == 0 ||
	    strcmp(psinfo->pr_lwp.pr_clname, "SYS") == 0)
		goto out;

	(void) memset(psa, 0, bsize--);
	nextargoff = 0;
	errno = EIO;
	while (bsize > 0) {
		if (narg == NARG) {
			(void) memset(argv, 0, sizeof (argv));
#ifdef _LP64
			if (is32) {
				if ((i = pread(pfd, argv32, sizeof (argv32),
				    argvoff)) <= 0) {
					if (i == 0 || errno == EIO)
						break;
					return (-1);
				}
				for (i = 0; i < NARG; i++)
					argv[i] = argv32[i];
			} else
#endif
				if ((i = pread(pfd, argv, sizeof (argv),
				    argvoff)) <= 0) {
					if (i == 0 || errno == EIO)
						break;
					return (-1);
				}
			narg = 0;
		}
		if ((argoff = argv[narg++]) == 0)
			break;
		if (argoff != nextargoff &&
		    (i = pread(pfd, psa, bsize, argoff)) <= 0) {
			if (i == 0 || errno == EIO)
				break;
			return (-1);
		}
		len = strlen(psa);
		psa += len;
		*psa++ = ' ';
		bsize -= len + 1;
		nextargoff = argoff + len + 1;
#ifdef _LP64
		argvoff += is32? sizeof (caddr32_t) : sizeof (caddr_t);
#else
		argvoff += sizeof (caddr_t);
#endif
	}
	while (psa > psargs && isspace(*(psa-1)))
		psa--;

out:
	*psa = '\0';
	if (strlen(psinfo->pr_psargs) > strlen(psargs))
		(void) strcpy(psargs, psinfo->pr_psargs);

	return (0);
}

/*
 * Read environment variables from the process.
 * Append them to psargs if there is room.
 */
static int
preadenvs(int pfd, psinfo_t *psinfo, char *psargs)
{
	off_t envpoff = (off_t)psinfo->pr_envp;
	int len;
	char *psa;
	char *psainit;
	int bsize;
	int nenv = NARG;
	off_t envp[NARG];
	off_t envoff;
	off_t nextenvoff;
	int i;
#ifdef _LP64
	caddr32_t envp32[NARG];
	int is32 = (psinfo->pr_dmodel != PR_MODEL_LP64);
#endif

	psainit = psa = (psargs != NULL)? psargs : psinfo->pr_psargs;
	len = strlen(psa);
	psa += len;
	bsize = twidth - len - 1;

	if (bsize <= 0 || psinfo->pr_nlwp == 0 ||
	    strcmp(psinfo->pr_lwp.pr_clname, "SYS") == 0)
		return (0);

	nextenvoff = 0;
	errno = EIO;
	while (bsize > 0) {
		if (nenv == NARG) {
			(void) memset(envp, 0, sizeof (envp));
#ifdef _LP64
			if (is32) {
				if ((i = pread(pfd, envp32, sizeof (envp32),
				    envpoff)) <= 0) {
					if (i == 0 || errno == EIO)
						break;
					return (-1);
				}
				for (i = 0; i < NARG; i++)
					envp[i] = envp32[i];
			} else
#endif
				if ((i = pread(pfd, envp, sizeof (envp),
				    envpoff)) <= 0) {
					if (i == 0 || errno == EIO)
						break;
					return (-1);
				}
			nenv = 0;
		}
		if ((envoff = envp[nenv++]) == 0)
			break;
		if (envoff != nextenvoff &&
		    (i = pread(pfd, psa+1, bsize, envoff)) <= 0) {
			if (i == 0 || errno == EIO)
				break;
			return (-1);
		}
		*psa++ = ' ';
		len = strlen(psa);
		psa += len;
		bsize -= len + 1;
		nextenvoff = envoff + len + 1;
#ifdef _LP64
		envpoff += is32? sizeof (caddr32_t) : sizeof (caddr_t);
#else
		envpoff += sizeof (caddr_t);
#endif
	}
	while (psa > psainit && isspace(*(psa-1)))
		psa--;
	*psa = '\0';

	return (0);
}

/*
 * readata reads in the open devices (terminals) and stores
 * info in the devl structure.
 */
static char	psfile[] = "/tmp/ups_data";

static int
readata()
{
	int fd;

	ndev = nmajor = 0;
	if ((fd = open(psfile, O_RDONLY)) == -1)
		return (0);

	if (psread(fd, (char *)&ndev, sizeof (ndev)) == 0 ||
	    (devl = malloc(ndev * sizeof (*devl))) == NULL)
		goto bad;
	maxdev = ndev;
	if (psread(fd, (char *)devl, ndev * sizeof (*devl)) == 0)
		goto bad;

	/* Read symbolic wait channel data. */
	if (psread(fd, (char *)&nchans, sizeof (nchans)) == 0 ||
	    (wchanhd = malloc(nchans * sizeof (struct wchan))) == NULL)
		goto bad;
	if (psread(fd, (char *)wchanhd, nchans * sizeof (struct wchan)) == 0 ||
	    psread(fd, (char *)wchan_index, NWCINDEX * sizeof (GElf_Addr))
	    == 0)
		goto bad;

	if (psread(fd, (char *)&nmajor, sizeof (nmajor)) == 0 ||
	    (majordev = malloc(nmajor * sizeof (int))) == NULL)
		goto bad;
	maxmajor = nmajor;
	if (psread(fd, (char *)majordev, nmajor * sizeof (int)) == 0)
		goto bad;

	(void) close(fd);
	return (1);

bad:
	if (devl)
		free(devl);
	if (wchanhd)
		free(wchanhd);
	if (majordev)
		free(majordev);
	devl = NULL;
	wchanhd = NULL;
	majordev = NULL;
	maxdev = ndev = 0;
	nchans = 0;
	maxmajor = nmajor = 0;
	(void) close(fd);
	return (0);
}

/*
 * getdev() uses getdevdir() to pass pathnames under /dev to gdev()
 * along with a status buffer.
 */
static void
getdev()
{
	FILE *fp;
	char buf[256];
	int dev_seen = 0;

	if (getdevcalled)
		return;
	getdevcalled++;

	ndev = 0;
	if ((fp = fopen("/etc/ttysrch", "r")) == NULL) {
		getdevdir("/dev/term", 1);
		getdevdir("/dev/pts", 1);
		getdevdir("/dev/xt", 1);
		getdevdir("/dev", 0);
	} else {
		while (fgets(buf, sizeof (buf), fp) != NULL) {
			char *dir = buf;
			char *cp;
			int len = strlen(dir) - 1;

			if (len <= 0)
				continue;
			*(dir + len) = '\0';
			while (len > 0 && isspace(*dir))
				len--, dir++;
			if (len == 0 || strncmp(dir, "/dev", 4) != 0)
				continue;
			cp = dir;
			while (len > 0 && !isspace(*cp))
				len--, cp++;
			*cp = '\0';
			if (strcmp(dir, "/dev") == 0) {
				dev_seen = 1;
				getdevdir(dir, 0);
			} else {
				getdevdir(dir, 1);
			}
		}
		(void) fclose(fp);
		if (!dev_seen)
			getdevdir("/dev", 0);
	}
}

/*
 * getdevdir() searches a directory and passes every
 * file it encounters to gdev().
 */
static void
getdevdir(char *dir, int remember)
{
	DIR *dirp;
	struct dirent *dentp;
	struct stat statb;
	char pathname[128];
	char *filename;

	(void) strcpy(pathname, dir);
	filename = pathname + strlen(pathname);
	*filename++ = '/';
	(void) strcpy(filename, ".");
	if ((dirp = opendir(pathname)) != NULL) {
		while (dentp = readdir(dirp)) {
			(void) strcpy(filename, dentp->d_name);
			if (stat(pathname, &statb) == 0)
				gdev(pathname, &statb, remember);
		}
		(void) closedir(dirp);
	}
}

/*
 * gdev() puts device names and ID into the devl structure for character
 * special files in /dev.  The "/dev/" string is stripped from the name
 * and if the resulting pathname exceeds DNSIZE in length then the highest
 * level directory names are stripped until the pathname is DNSIZE or less.
 */
static void
gdev(char *objptr, struct stat *statp, int remember)
{
	int	i;
	int	leng, start;
	static struct devl ldevl[2];
	static int	lndev, consflg;

	if ((statp->st_mode & S_IFMT) == S_IFCHR) {
		/* Get more and be ready for syscon & systty. */
		while (ndev + lndev >= maxdev) {
			maxdev += 100;
			devl = (struct devl *)
			    realloc(devl, sizeof (struct devl) * maxdev);
			if (devl == NULL) {
				(void) fprintf(stderr,
				    "ps: not enough memory for %d devices\n",
				    maxdev);
				exit(1);
			}
		}
		/*
		 * Save systty & syscon entries if the console
		 * entry hasn't been seen.
		 */
		if (!consflg &&
		    (strcmp("/dev/systty", objptr) == 0 ||
		    strcmp("/dev/syscon", objptr) == 0)) {
			(void) strncpy(ldevl[lndev].dname,
			    &objptr[5], DNSIZE);
			ldevl[lndev].major = major(statp->st_rdev);
			ldevl[lndev].minor = minor(statp->st_rdev);
			lndev++;
			return;
		}

		leng = strlen(objptr);
		/* Strip off /dev/ */
		if (leng < DNSIZE + 4)
			(void) strcpy(devl[ndev].dname, &objptr[5]);
		else {
			start = leng - DNSIZE - 1;

			for (i = start; i < leng && (objptr[i] != '/'); i++)
				;
			if (i == leng)
				(void) strncpy(devl[ndev].dname,
				    &objptr[start], DNSIZE);
			else
				(void) strncpy(devl[ndev].dname,
				    &objptr[i+1], DNSIZE);
		}
		devl[ndev].major = major(statp->st_rdev);
		devl[ndev].minor = minor(statp->st_rdev);
		ndev++;
		/*
		 * Put systty & syscon entries in devl when console
		 * is found.
		 */
		if (strcmp("/dev/console", objptr) == 0) {
			consflg++;
			for (i = 0; i < lndev; i++) {
				(void) strncpy(devl[ndev].dname,
				    ldevl[i].dname, DNSIZE);
				devl[ndev].major = ldevl[i].major;
				devl[ndev].minor = ldevl[i].minor;
				ndev++;
			}
			lndev = 0;
		}

		if (remember) {		/* remember the major device number */
			int i;
			int maj = major(statp->st_rdev);

			while (nmajor >= maxmajor) {
				maxmajor += 20;
				majordev = (int *)
				    realloc(majordev, sizeof (int) * maxmajor);
				if (majordev == NULL) {
					(void) fprintf(stderr,
				"ps: not enough memory for %d major numbers\n",
					    maxmajor);
					exit(1);
				}
			}
			for (i = 0; i < nmajor; i++)
				if (maj == majordev[i])
					break;
			if (i == nmajor) {	/* new entry */
				majordev[i] = maj;
				nmajor++;
			}
		}
	}
}

/*
 * Have we seen this tty's major device before?
 * Used to determine if it is useful to rebuild ps_data file.
 */
static int
majorexists(dev_t dev)
{
	int i;
	int maj = major(dev);

	for (i = 0; i < nmajor; i++)
		if (maj == majordev[i])
			return (1);
	return (0);
}

static void
wrdata()
{
	char	tmpname[16];
	char	*tfname;
	int	fd;

	(void) umask(02);
	(void) strcpy(tmpname, "/tmp/ps.XXXXXX");
	if ((tfname = mktemp(tmpname)) == NULL || *tfname == '\0') {
		(void) fprintf(stderr,
		    "ps: mktemp(\"/tmp/ps.XXXXXX\") failed, %s\n",
		    err_string(errno));
		(void) fprintf(stderr,
		    "ps: Please notify your System Administrator\n");
		return;
	}

	if ((fd = open(tfname, O_WRONLY|O_CREAT|O_EXCL, 0664)) < 0) {
		(void) fprintf(stderr,
		    "ps: open(\"%s\") for write failed, %s\n",
		    tfname, err_string(errno));
		(void) fprintf(stderr,
		    "ps: Please notify your System Administrator\n");
		return;
	}

	/*
	 * Make owner root, group sys.
	 */
	(void) fchown(fd, (uid_t)0, (gid_t)3);

	/* write /dev data */
	pswrite(fd, (char *)&ndev, sizeof (ndev));
	pswrite(fd, (char *)devl, ndev * sizeof (*devl));

	/* write symbolic wait channel data */
	pswrite(fd, (char *)&nchans, sizeof (nchans));
	pswrite(fd, (char *)wchanhd, nchans * sizeof (struct wchan));
	pswrite(fd, (char *)wchan_index, NWCINDEX * sizeof (GElf_Addr));

	pswrite(fd, (char *)&nmajor, sizeof (nmajor));
	pswrite(fd, (char *)majordev, nmajor * sizeof (int));

	(void) close(fd);

	if (rename(tfname, psfile) != 0) {
		(void) fprintf(stderr, "ps: rename(\"%s\",\"%s\") failed, %s\n",
		    tfname, psfile, err_string(errno));
		(void) fprintf(stderr,
		    "ps: Please notify your System Administrator\n");
		return;
	}
}

/*
 * getarg() finds the next argument in list and copies arg into argbuf.
 * p1 first pts to arg passed back from getopt routine.  p1 is then
 * bumped to next character that is not a comma or blank -- p1 NULL
 * indicates end of list.
 */

static void
getarg()
{
	char	*parga;
	int c;

	while ((c = *p1) != '\0' && (c == ',' || isspace(c)))
		p1++;

	parga = argbuf;
	while ((c = *p1) != '\0' && c != ',' && !isspace(c)) {
		if (parga < argbuf + ARGSIZ - 1)
			*parga++ = c;
		p1++;
	}
	*parga = '\0';

	while ((c = *p1) != '\0' && (c == ',' || isspace(c)))
		p1++;
}

/*
 * gettty returns the user's tty number or ? if none.
 * ip is where the search left off last time.
 */
static char *
gettty(int *ip, psinfo_t *psinfo)
{
	int i;

	if (psinfo->pr_ttydev != PRNODEV && *ip >= 0) {
		for (i = *ip; i < ndev; i++) {
			if (devl[i].major == major(psinfo->pr_ttydev) &&
			    devl[i].minor == minor(psinfo->pr_ttydev)) {
				*ip = i + 1;
				return (devl[i].dname);
			}
		}
	}
	*ip = -1;
	return (psinfo->pr_ttydev == PRNODEV? "?" : "??");
}

/*
 * Print percent from 16-bit binary fraction [0 .. 1]
 * Round up .01 to .1 to indicate some small percentage (the 0x7000 below).
 */
static void
prtpct(u_short pct)
{
	uint_t value = pct;	/* need 32 bits to compute with */

	value = ((value * 1000) + 0x7000) >> 15;	/* [0 .. 1000] */
	(void) printf("%3u.%u", value / 10, value % 10);
}

/*
 * Print info about the process.
 */
static int
prcom(int found, psinfo_t *psinfo, char *psargs)
{
	char	*cp;
	char	*tp;
	char	*psa;
	long	tm;
	int	i, wcnt, length;
	wchar_t	wchar;
	char	**ttyp, *str;

	/*
	 * If process is zombie, call print routine and return.
	 */
	if (psinfo->pr_nlwp == 0) {
		if (tflg && !found)
			return (0);
		else {
			przom(psinfo);
			return (1);
		}
	}

	/*
	 * Get current terminal.  If none ("?") and 'a' is set, don't print
	 * info.  If 't' is set, check if term is in list of desired terminals
	 * and print it if it is.
	 */
	i = 0;
	tp = gettty(&i, psinfo);
	if (*tp == '?' && psinfo->pr_ttydev != PRNODEV &&
	    !getdevcalled && majorexists(psinfo->pr_ttydev)) {
		getdev();
		/* getwchan(); */
		wrdata();
		i = 0;
		tp = gettty(&i, psinfo);
	}

	if (*tp == '?' && !found && !xflg)
		return (0);

	if (!(*tp == '?' && aflg) && tflg && !found) {
		int match = 0;

		/*
		 * Look for same device under different names.
		 */
		while (i >= 0 && !match) {
			for (ttyp = tty; (str = *ttyp) != 0 && !match; ttyp++)
				if (strcmp(tp, str) == 0)
					match = 1;
			if (!match)
				tp = gettty(&i, psinfo);
		}
		if (!match)
			return (0);
	}

	if (lflg)
		(void) printf("%2x", psinfo->pr_flag & 0377);
	if (uflg) {
		if (!nflg) {
			struct passwd *pwd;

			if ((pwd = getpwuid(psinfo->pr_euid)) != NULL)
								/* USER */
				(void) printf("%-8.8s", pwd->pw_name);
			else
								/* UID */
				(void) printf(" %7.7d", (int)psinfo->pr_euid);
		} else {
			(void) printf(" %5d", (int)psinfo->pr_euid); /* UID */
		}
	} else if (lflg)
		(void) printf(" %5d", (int)psinfo->pr_euid);	/* UID */

	(void) printf("%*d", pidwidth + 1, (int)psinfo->pr_pid); /* PID */
	if (lflg)
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_ppid); /* PPID */
	if (lflg)
		(void) printf("%3d", psinfo->pr_lwp.pr_cpu & 0377); /* CP */
	if (uflg) {
		prtpct(psinfo->pr_pctcpu);			/* %CPU */
		prtpct(psinfo->pr_pctmem);			/* %MEM */
	}
	if (lflg) {
		(void) printf("%4d", psinfo->pr_lwp.pr_pri);	/* PRI */
		(void) printf("%3d", psinfo->pr_lwp.pr_nice);	/* NICE */
	}
	if (lflg || uflg) {
		if (psinfo->pr_flag & SSYS)			/* SZ */
			(void) printf("    0");
		else if (psinfo->pr_size)
			(void) printf("%5lu", (ulong_t)psinfo->pr_size);
		else
			(void) printf("    ?");
		if (psinfo->pr_flag & SSYS)			/* RSS */
			(void) printf("    0");
		else if (psinfo->pr_rssize)
			(void) printf("%5lu", (ulong_t)psinfo->pr_rssize);
		else
			(void) printf("    ?");
	}
	if (lflg) {						/* WCHAN */
		if (psinfo->pr_lwp.pr_sname != 'S') {
			(void) printf("         ");
		} else if (psinfo->pr_lwp.pr_wchan) {
			if (nflg)
				(void) printf(" %+8.8lx",
					(ulong_t)psinfo->pr_lwp.pr_wchan);
			else
				(void) printf(" %+8.8s",
				getchan((GElf_Addr)psinfo->pr_lwp.pr_wchan));
		} else {
			(void) printf("        ?");
		}
	}
	if ((tplen = strlen(tp)) > 9)
		maxlen = twidth - tplen + 9;
	else
		maxlen = twidth;

	if (!lflg)
		(void) printf(" %-8.14s", tp);			/* TTY */
	(void) printf(" %c", psinfo->pr_lwp.pr_sname);		/* STATE */
	if (lflg)
		(void) printf(" %-8.14s", tp);			/* TTY */
	if (uflg)
		prtime(psinfo->pr_start);			/* START */

	/* time just for process */
	tm = psinfo->pr_time.tv_sec;
	if (Sflg) {	/* calculate time for process and all reaped children */
		tm += psinfo->pr_ctime.tv_sec;
		if (psinfo->pr_time.tv_nsec + psinfo->pr_ctime.tv_nsec
		    >= 1000000000)
			tm += 1;
	}

	(void) printf(" %2ld:%.2ld", tm / 60, tm % 60);		/* TIME */

	if (vflg) {
		if (psinfo->pr_flag & SSYS)			/* SZ */
			(void) printf("    0");
		else if (psinfo->pr_size)
			(void) printf("%5lu", (ulong_t)psinfo->pr_size);
		else
			(void) printf("    ?");
		if (psinfo->pr_flag & SSYS)			/* SZ */
			(void) printf("    0");
		else if (psinfo->pr_rssize)
			(void) printf("%5lu", (ulong_t)psinfo->pr_rssize);
		else
			(void) printf("    ?");
		prtpct(psinfo->pr_pctcpu);			/* %CPU */
		prtpct(psinfo->pr_pctmem);			/* %MEM */
	}
	if (cflg) {						/* CMD */
		wcnt = namencnt(psinfo->pr_fname, 16, maxlen);
		(void) printf(" %.*s", wcnt, psinfo->pr_fname);
		return (1);
	}
	/*
	 * PRARGSZ == length of cmd arg string.
	 */
	if (psargs == NULL) {
		psa = &psinfo->pr_psargs[0];
		i = PRARGSZ;
		tp = &psinfo->pr_psargs[PRARGSZ];
	} else {
		psa = psargs;
		i = strlen(psargs);
		tp = psa + i;
	}

	for (cp = psa; cp < tp; /* empty */) {
		if (*cp == 0)
			break;
		length = mbtowc(&wchar, cp, MB_LEN_MAX);
		if (length < 0 || !iswprint(wchar)) {
			(void) printf(" [ %.16s ]", psinfo->pr_fname);
			return (1);
		}
		cp += length;
	}
	wcnt = namencnt(psa, i, maxlen);
#if 0
	/* dumps core on really long strings */
	(void) printf(" %.*s", wcnt, psa);
#else
	(void) putchar(' ');
	(void) fwrite(psa, 1, wcnt, stdout);
#endif
	return (1);
}

/*
 * Special read; removes psfile on read error.
 */
static int
psread(int fd, char *bp, unsigned int bs)
{
	int rbs;

	if ((rbs = read(fd, bp, bs)) != bs) {
		(void) fprintf(stderr,
		    "ps: psread() error on read, rbs=%d, bs=%d, %s\n",
		    rbs, bs, err_string(errno));
		(void) remove(psfile);
		return (0);
	}
	return (1);
}

/*
 * Special write; removes psfile on write error.
 */
static void
pswrite(int fd, char *bp, unsigned bs)
{
	int	wbs;

	if ((wbs = write(fd, bp, bs)) != bs) {
		(void) fprintf(stderr,
		    "ps: pswrite() error on write, wbs=%d, bs=%d, %s\n",
		    wbs, bs, err_string(errno));
		(void) remove(psfile);
	}
}

/*
 * Print starting time of process unless process started more than 24 hours
 * ago, in which case the date is printed.
 */
static void
prtime(timestruc_t st)
{
	char sttim[26];
	static time_t tim = 0L;
	time_t starttime;

	if (tim == 0L)
		tim = time((time_t *)0);
	starttime = st.tv_sec;
	if (tim - starttime > 24*60*60) {
		(void) cftime(sttim, "%b %d", &starttime);
		sttim[7] = '\0';
	} else {
		(void) cftime(sttim, "%H:%M:%S", &starttime);
		sttim[8] = '\0';
	}
	(void) printf("%9.9s", sttim);
}

static void
przom(psinfo_t *psinfo)
{
	long	tm;

	if (lflg)
		(void) printf("%2x", psinfo->pr_flag & 0377);
	if (uflg) {
		struct passwd *pwd;

		if ((pwd = getpwuid(psinfo->pr_euid)) != NULL)
			(void) printf("%-8.8s", pwd->pw_name);	/* USER */
		else
			(void) printf(" %7.7d", (int)psinfo->pr_euid); /* UID */
	} else if (lflg)
		(void) printf(" %5d", (int)psinfo->pr_euid);	/* UID */

	(void) printf("%*d", pidwidth + 1, (int)psinfo->pr_pid); /* PID */
	if (lflg)
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_ppid); /* PPID */
	if (lflg)
		(void) printf("  0");				/* CP */
	if (uflg) {
		prtpct(0);					/* %CPU */
		prtpct(0);					/* %MEM */
	}
	if (lflg) {
		(void) printf("%4d", psinfo->pr_lwp.pr_pri);	/* PRI */
		(void) printf("   ");				/* NICE */
	}
	if (lflg || uflg) {
		(void) printf("    0");				/* SZ */
		(void) printf("    0");				/* RSS */
	}
	if (lflg)
		(void) printf("         ");			/* WCHAN */
	(void) printf("          ");				/* TTY */
	(void) printf("%c", psinfo->pr_lwp.pr_sname);		/* STATE */
	if (uflg)
		(void) printf("         ");			/* START */

	/* time just for process */
	tm = psinfo->pr_time.tv_sec;
	if (Sflg) {	/* calculate time for process and all reaped children */
		tm += psinfo->pr_ctime.tv_sec;
		if (psinfo->pr_time.tv_nsec + psinfo->pr_ctime.tv_nsec
		    >= 1000000000)
			tm += 1;
	}
	(void) printf(" %2ld:%.2ld", tm / 60, tm % 60);		/* TIME */

	if (vflg) {
		(void) printf("    0");				/* SZ */
		(void) printf("    0");				/* RSS */
		prtpct(0);					/* %CPU */
		prtpct(0);					/* %MEM */
	}
	(void) printf(" %.*s", maxlen, " <defunct>");
}

/*
 * Returns true iff string is all numeric.
 */
static int
num(char *s)
{
	int c;

	if (s == NULL)
		return (0);
	c = *s;
	do {
		if (!isdigit(c))
			return (0);
	} while ((c = *++s) != '\0');
	return (1);
}

/*
 * Function to compute the number of printable bytes in a multibyte
 * command string ("internationalization").
 */
static int
namencnt(char *cmd, int eucsize, int scrsize)
{
	int eucwcnt = 0, scrwcnt = 0;
	int neucsz, nscrsz;
	wchar_t	wchar;

	while (*cmd != '\0') {
		if ((neucsz = mbtowc(&wchar, cmd, MB_LEN_MAX)) < 0)
			return (8); /* default to use for illegal chars */
		if ((nscrsz = scrwidth(wchar)) == 0)
			return (8);
		if (eucwcnt + neucsz > eucsize || scrwcnt + nscrsz > scrsize)
			break;
		eucwcnt += neucsz;
		scrwcnt += nscrsz;
		cmd += neucsz;
	}
	return (eucwcnt);
}

static char sym_source[] = "/dev/ksyms";
		/* File from which the wait channels are built */

static void
getwchan()
{
	struct stat	buf;
	int		fd, count, found;
	int		tmp, i;
	Elf		*elf_file;
	Elf_Cmd		cmd;
	Elf_Kind	file_type;
	GElf_Ehdr	ehdr;
	GElf_Shdr	shdr;
	GElf_Sym	sym;
	Elf_Data	*data;
	Elf_Scn		*scn;

	if (stat(sym_source, &buf) == -1) {
		(void) fprintf(stderr, "ps: ");
		perror(sym_source);
		exit(1);
	}
	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr, "ps: Libelf is out of date\n");
		return;
	}
	if ((fd = open((sym_source), O_RDONLY)) == -1) {
		(void) fprintf(stderr, "ps: Cannot read %s\n", sym_source);
		return;
	}
	cmd = ELF_C_READ;
	if ((elf_file = elf_begin(fd, cmd, NULL)) == NULL) {
		(void) fprintf(stderr,
		    "ps: %s: %s\n", sym_source, elf_errmsg(-1));
		return;
	}
	file_type = elf_kind(elf_file);
	if (file_type != ELF_K_ELF) {
		(void) fprintf(stderr,
		    "ps: %s: invalid file type\n", sym_source);
		return;
	}
	if (gelf_getehdr(elf_file, &ehdr) == NULL) {
		(void) fprintf(stderr,
		    "ps: %s: %s\n", sym_source, elf_errmsg(-1));
		return;
	}
	scn = 0;

	found = 0;
	/* find symbol table */
	while ((scn = elf_nextscn(elf_file, scn)) != 0) {
		if (gelf_getshdr(scn, &shdr) == NULL) {
			(void) fprintf(stderr,
			    "ps: %s: %s\n", sym_source, elf_errmsg(-1));
			return;
		}
		if (shdr.sh_type == SHT_SYMTAB) {
			found = 1;
			break;
		}
	}
	if (!found) {
		(void) fprintf(stderr,
		    "ps: %s: could not get symbol table\n", sym_source);
		return;
	}
	if ((data = elf_getdata(scn, NULL)) == NULL) {
		(void) fprintf(stderr,
		    "ps: %s: No symbol table data\n", sym_source);
		return;
	}
	count = shdr.sh_size / shdr.sh_entsize;

	/* fill wchan info */
	for (i = 1; i < count; i++) {
		char *sym_name;
		GElf_Addr sym_addr;

		(void) gelf_getsym(data, i, &sym);
		sym_addr = sym.st_value;
		sym_name = elf_strptr(elf_file, shdr.sh_link, sym.st_name);
		if (sym_addr != (GElf_Addr)0 &&
		    sym_name != NULL &&
		    *sym_name != '\0')
			addchan(sym_name, sym_addr);
	}

	qsort(wchanhd, nchans, sizeof (struct wchan), wchancomp);
	for (i = 0; i < NWCINDEX; i++) {	/* speed up searches */
		tmp = i * nchans;
		wchan_index[i] = wchanhd[tmp / NWCINDEX].wc_addr;
	}
}

/*
 * Add the given channel to the channel list.
 */
static void
addchan(const char *name, GElf_Addr addr)
{
	static int left = 0;
	struct wchan *wp;
	struct wchan_map *mp;

	/* wchan mappings */
	for (mp = wchan_map_list; mp->map_from; mp++) {
		if (*(mp->map_from) != *name)	/* quick check */
			continue;
		if (strncmp(name, mp->map_from, WNAMESIZ) == 0)
			name = mp->map_to;
	}

	if (left == 0) { /* no space left - reallocate old or allocate new */
		if (wchanhd) {
			left = 100;
			wchanhd = (struct wchan *)realloc(wchanhd,
			    (nchans + left) * sizeof (struct wchan));
		} else {
			left = 600;
			wchanhd = malloc(left * sizeof (struct wchan));
		}
		if (wchanhd == NULL) {
			(void) fprintf(stderr,
			    "ps: out of memory allocating wait channels\n");
			nflg++;
			return;
		}
	}
	left--;
	wp = &wchanhd[nchans++];
	(void) strncpy(wp->wc_name, name, WNAMESIZ);
	wp->wc_name[WNAMESIZ] = '\0';
	wp->wc_addr = addr;
}

/*
 * Returns the symbolic wait channel corresponding to chan
 */
static char *
getchan(GElf_Addr chan)
{
	int i, iend;
	char	*prevsym;
	struct wchan *wp;

	prevsym = "???";	/* nothing to begin with */
	if (chan) {
		for (i = 0; i < NWCINDEX; i++)
			if ((unsigned)chan < (unsigned)wchan_index[i])
				break;
		iend = i--;

		if (i < 0)	/* can't be found */
			return (prevsym);
		iend *= nchans;
		iend /= NWCINDEX;
		i *= nchans;
		i /= NWCINDEX;
		wp = &wchanhd[i];
		for (; i < iend; i++, wp++) {
			if ((unsigned)wp->wc_addr > (unsigned)chan)
				break;
			prevsym = wp->wc_name;
		}
	}
	/*
	 * Many values are getting mapped to "_end", which
	 * doesn't make sense.  When we use kvm_nlist, we will
	 * probably get better information.
	 */
	if (strcmp(prevsym, "_end") == 0) {		/* XXX */
		prevsym = "???";
	}
	return (prevsym);
}

/*
 * used in sorting the wait channel array
 */
static int
wchancomp(const void *arg1, const void *arg2)
{
	const struct wchan *w1 = arg1;
	const struct wchan *w2 = arg2;
	ulong_t c1, c2;

	c1 = (ulong_t)w1->wc_addr;
	c2 = (ulong_t)w2->wc_addr;
	if (c1 > c2)
		return (1);
	else if (c1 == c2)
		return (0);
	else
		return (-1);
}

static int
pscompare(const void *v1, const void *v2)
{
	const struct psent *p1 = v1;
	const struct psent *p2 = v2;
	int i;

	if (uflg)
		i = p2->psinfo->pr_pctcpu - p1->psinfo->pr_pctcpu;
	else if (vflg)
		i = p2->psinfo->pr_rssize - p1->psinfo->pr_rssize;
	else
		i = p1->psinfo->pr_ttydev - p2->psinfo->pr_ttydev;
	if (i == 0)
		i = p1->psinfo->pr_pid - p2->psinfo->pr_pid;
	return (i);
}

static char *
err_string(int err)
{
	static char buf[32];
	char *str = strerror(err);

	if (str == NULL)
		(void) sprintf(str = buf, "Errno #%d", err);

	return (str);
}
