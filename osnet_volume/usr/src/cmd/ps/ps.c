/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ps.c	1.47	99/06/04 SMI"

/*
 * ps -- print things about processes.
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <procfs.h>
#include <locale.h>
#include <wctype.h>
#include <wchar.h>
#include <libw.h>
#include <stdarg.h>
#include <sys/proc.h>

#define	min(a, b)	((a) > (b) ? (b) : (a))
#define	max(a, b)	((a) < (b) ? (b) : (a))

#define	NTTYS	20	/* initial size of table for -t option  */
#define	SIZ	30	/* initial size of tables for -p, -s, and -g */
#define	ARGSIZ	30	/* size of buffer holding args for -t, -p, -u options */

#define	MAXUGNAME 10	/* max chars in a user/group name or printed u/g id */

/* Structure for storing user or group info */
struct ugdata {
	id_t	id;			/* numeric user-id or group-id */
	char	name[MAXUGNAME+1];	/* user/group name, null terminated */
};

struct ughead {
	size_t	size;		/* number of ugdata structs allocated */
	size_t	nent;		/* number of active entries */
	struct ugdata *ent;	/* pointer to array of actual entries */
};

enum fname {	/* enumeration of field names */
	F_USER,		/* effective user of the process */
	F_RUSER,	/* real user of the process */
	F_GROUP,	/* effective group of the process */
	F_RGROUP,	/* real group of the process */
	F_UID,		/* numeric effective uid of the process */
	F_RUID,		/* numeric real uid of the process */
	F_GID,		/* numeric effective gid of the process */
	F_RGID,		/* numeric real gid of the process */
	F_PID,		/* process id */
	F_PPID,		/* parent process id */
	F_PGID,		/* process group id */
	F_SID,		/* session id */
	F_PSR,		/* bound processor */
	F_LWP,		/* lwp-id */
	F_NLWP,		/* number of lwps */
	F_OPRI,		/* old priority (obsolete) */
	F_PRI,		/* new priority */
	F_F,		/* process flags */
	F_S,		/* letter indicating the state */
	F_C,		/* processor utilization (obsolete) */
	F_PCPU,		/* percent of recently used cpu time */
	F_PMEM,		/* percent of physical memory used (rss) */
	F_OSZ,		/* virtual size of the process in pages */
	F_VSZ,		/* virtual size of the process in kilobytes */
	F_RSS,		/* resident set size of the process in kilobytes */
	F_NICE,		/* "nice" value of the process */
	F_CLASS,	/* scheduler class */
	F_STIME,	/* start time of the process, hh:mm:ss or Month Day */
	F_ETIME,	/* elapsed time of the process, [[dd-]hh:]mm:ss */
	F_TIME,		/* cpu time of the process, [[dd-]hh:]mm:ss */
	F_TTY,		/* name of the controlling terminal */
	F_ADDR,		/* address of the process (obsolete) */
	F_WCHAN,	/* wait channel (sleep condition variable) */
	F_FNAME,	/* file name of command */
	F_COMM,		/* name of command (argv[0] value) */
	F_ARGS		/* name of command plus all its arguments */
};

struct field {
	struct field	*next;		/* linked list */
	int		fname;		/* field index */
	const char	*header;	/* header to use */
	int		width;		/* width of field */
};

static	struct field *fields = NULL;	/* fields selected via -o */
static	struct field *last_field = NULL;
static	int do_header = 0;

/* array of defined fields, in fname order */
struct def_field {
	const char *fname;
	const char *header;
	int width;
	int minwidth;
};

static struct def_field fname[] = {
	/* fname	header		width	minwidth */
	{ "user",	"USER",		8,	8	},
	{ "ruser",	"RUSER",	8,	8	},
	{ "group",	"GROUP",	8,	8	},
	{ "rgroup",	"RGROUP",	8,	8	},
	{ "uid",	"UID",		5,	5	},
	{ "ruid",	"RUID",		5,	5	},
	{ "gid",	"GID",		5,	5	},
	{ "rgid",	"RGID",		5,	5	},
	{ "pid",	"PID",		5,	5	},
	{ "ppid",	"PPID",		5,	5	},
	{ "pgid",	"PGID",		5,	5	},
	{ "sid",	"SID",		5,	5	},
	{ "psr",	"PSR",		3,	2	},
	{ "lwp",	"LWP",		6,	2	},
	{ "nlwp",	"NLWP",		4,	2	},
	{ "opri",	"PRI",		3,	2	},
	{ "pri",	"PRI",		3,	2	},
	{ "f",		"F",		2,	2	},
	{ "s",		"S",		1,	1	},
	{ "c",		"C",		2,	2	},
	{ "pcpu",	"%CPU",		4,	4	},
	{ "pmem",	"%MEM",		4,	4	},
	{ "osz",	"SZ",		4,	4	},
	{ "vsz",	"VSZ",		4,	4	},
	{ "rss",	"RSS",		4,	4	},
	{ "nice",	"NI",		2,	2	},
	{ "class",	"CLS",		4,	2	},
	{ "stime",	"STIME",	8,	8	},
	{ "etime",	"ELAPSED",	11,	7	},
	{ "time",	"TIME",		11,	5	},
	{ "tty",	"TT",		7,	7	},
#ifdef _LP64
	{ "addr",	"ADDR",		16,	8	},
	{ "wchan",	"WCHAN",	16,	8	},
#else
	{ "addr",	"ADDR",		8,	8	},
	{ "wchan",	"WCHAN",	8,	8	},
#endif
	{ "fname",	"COMMAND",	8,	8	},
	{ "comm",	"COMMAND",	80,	8	},
	{ "args",	"COMMAND",	80,	80	},
};

#define	NFIELDS	(sizeof (fname) / sizeof (fname[0]))

static	int	retcode = 1;
static	int	lflg;
static	int	Aflg;
static	int	uflg;
static	int	Uflg;
static	int	Gflg;
static	int	aflg;
static	int	dflg;
static	int	Lflg;
static	int	Pflg;
static	int	yflg;
static	int	pflg;
static	int	fflg;
static	int	cflg;
static	int	jflg;
static	int	gflg;
static	int	sflg;
static	int	tflg;
static	uid_t	tuid = -1;
static	int	errflg;

static	int	ndev;		/* number of devices */
static	int	maxdev;		/* number of devl structures allocated */

#define	DNSIZE	14
static struct devl {			/* device list	 */
	char	dname[DNSIZE];	/* device name	 */
	major_t	major;		/* device number */
	minor_t	minor;
} *devl;

static	int	nmajor;		/* number of remembered major device numbers */
static	int	maxmajor;	/* number of major device numbers allocated */
static	int	*majordev;	/* array of remembered major device numbers */

static	char	**tty = NULL;	/* for t option */
static	size_t	ttysz = 0;
static	int	ntty = 0;

static	pid_t	*pid = NULL;	/* for p option */
static	size_t	pidsz = 0;
static	int	npid = 0;

static	pid_t	*grpid = NULL;	/* for g option */
static	size_t	grpidsz = 0;
static	int	ngrpid = 0;

static	pid_t	*sessid = NULL;	/* for s option */
static	size_t	sessidsz = 0;
static	int	nsessid = 0;

static	int	kbytes_per_page;
static	int	pidwidth;

static	char	*procdir = "/proc";	/* standard /proc directory */
static	int	getdevcalled = 0;	/* if == 1, getdev() has been called */

static	void	usage(void);
static	char	*getarg(char **);
static	void	getdev(void);
static	void	getdevdir(char *, int);
static	void	gdev(char *, struct stat *, int);
static	int	majorexists(dev_t);
static	void	wrdata(void);
static	int	readata(void);
static	char	*parse_format(char *);
static	char	*gettty(psinfo_t *, int *);
static	int	prfind(int, psinfo_t *, char **);
static	void	prcom(psinfo_t *, char *);
static	void	prtpct(u_short, int);
static	void	print_time(time_t, int);
static	void	print_field(psinfo_t *, struct field *, char *);
static	void	pr_fields(psinfo_t *, char *);
static	int	search(pid_t *, int, pid_t);
static	void	add_ugentry(struct ughead *, char *);
static	int	uconv(struct ughead *);
static	int	gconv(struct ughead *);
static	int	ugfind(uid_t, struct ughead *);
static	int	psread(int, char *, unsigned int);
static	void	pswrite(int, char *, unsigned int);
static	void	prtime(timestruc_t, int, int);
static	void	przom(psinfo_t *);
static	int	num(char *);
static	int	namencnt(char *, int, int);
static	char	*err_string(int);
static	void *	Realloc(void *, size_t);

int
main(int argc, char **argv)
{
	struct ughead euid_tbl;		/* table to store selected euid's */
	struct ughead ruid_tbl;		/* table to store selected real uid's */
	struct ughead egid_tbl;		/* table to store selected egid's */
	struct ughead rgid_tbl;		/* table to store selected real gid's */
	char	*p;
	char	*p1;
	char	*parg;
	int	c;
	int	i;
	int	pgerrflg = 0;	/* err flg: non-numeric arg w/p & g options */
	size_t	size;
	DIR	*dirp;
	struct dirent *dentp;
	char	pname[100];
	int	pdlen;
	prheader_t *lpsinfobuf;		/* buffer to contain lpsinfo */
	size_t	lpbufsize;
	pid_t	maxpid;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) memset(&euid_tbl, 0, sizeof (euid_tbl));
	(void) memset(&ruid_tbl, 0, sizeof (ruid_tbl));
	(void) memset(&egid_tbl, 0, sizeof (egid_tbl));
	(void) memset(&rgid_tbl, 0, sizeof (rgid_tbl));

	kbytes_per_page = sysconf(_SC_PAGESIZE) / 1024;

	/*
	 * calculate width of pid fields based on configured MAXPID
	 * (must be at least 5 to retain output format compatibility)
	 */
	maxpid = (pid_t) sysconf(_SC_MAXPID);
	pidwidth = 1;
	while ((maxpid /= 10) > 0)
		++pidwidth;
	pidwidth = pidwidth < 5 ? 5 : pidwidth;

	fname[F_PID].width = fname[F_PPID].width = pidwidth;
	fname[F_PGID].width = fname[F_SID].width = pidwidth;

	while ((c = getopt(argc, argv, "jlfceAadLPyt:p:g:u:U:G:n:s:o:")) != EOF)
		switch (c) {
		case 'l':		/* long listing */
			lflg++;
			break;
		case 'f':		/* full listing */
			fflg++;
			break;
		case 'j':
			jflg++;
			break;
		case 'c':
			/*
			 * Format output to reflect scheduler changes:
			 * high numbers for high priorities and don't
			 * print nice or p_cpu values.  'c' option only
			 * effective when used with 'l' or 'f' options.
			 */
			cflg++;
			break;
		case 'A':		/* list every process */
		case 'e':		/* (obsolete) list every process */
			Aflg++;
			tflg = Gflg = Uflg = uflg = pflg = gflg = sflg = 0;
			break;
		case 'a':
			/*
			 * Same as 'e' except no session group leaders
			 * and no non-terminal processes.
			 */
			aflg++;
			break;
		case 'd':	/* same as e except no session leaders */
			dflg++;
			break;
		case 'L':	/* show lwps */
			Lflg++;
			break;
		case 'P':	/* show bound processor */
			Pflg++;
			break;
		case 'y':	/* omit F & ADDR, report RSS & SZ in Kby */
			yflg++;
			break;
		case 'n':	/* no longer needed; retain as no-op */
			(void) fprintf(stderr,
			    gettext("ps: warning: -n option ignored\n"));
			break;
		case 't':		/* terminals */
#define	TSZ	30
			tflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				p = Realloc(NULL, TSZ);
				/* zero the buffer before using it */
				(void) memset(p, 0, TSZ);
				size = TSZ;
				if (isdigit(*parg)) {
					(void) strcpy(p, "tty");
					size -= 3;
				}
				(void) strncat(p, parg, (int)size);
				if (ntty == ttysz) {
					if ((ttysz *= 2) == 0)
						ttysz = NTTYS;
					tty = Realloc(tty,
					    (ttysz + 1) * sizeof (char *));
				}
				tty[ntty++] = p;
			} while (*p1);
			break;
		case 'p':		/* proc ids */
			pflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				if (num(parg)) {
					if (npid == pidsz) {
						if ((pidsz *= 2) == 0)
							pidsz = SIZ;
						pid = Realloc(pid,
						    pidsz * sizeof (pid_t));
					}
					pid[npid++] = (pid_t)atol(parg);
				} else {
					pgerrflg++;
					(void) fprintf(stderr,
	gettext("ps: %s is an invalid non-numeric argument for -p option\n"),
					    parg);
				}
			} while (*p1);
			break;
		case 's':		/* session */
			sflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				if (num(parg)) {
					if (nsessid == sessidsz) {
						if ((sessidsz *= 2) == 0)
							sessidsz = SIZ;
						sessid = Realloc(sessid,
						    sessidsz * sizeof (pid_t));
					}
					sessid[nsessid++] = (pid_t)atol(parg);
				} else {
					pgerrflg++;
					(void) fprintf(stderr,
	gettext("ps: %s is an invalid non-numeric argument for -s option\n"),
					    parg);
				}
			} while (*p1);
			break;
		case 'g':		/* proc group */
			gflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				if (num(parg)) {
					if (ngrpid == grpidsz) {
						if ((grpidsz *= 2) == 0)
							grpidsz = SIZ;
						grpid = Realloc(grpid,
						    grpidsz * sizeof (pid_t));
					}
					grpid[ngrpid++] = (pid_t)atol(parg);
				} else {
					pgerrflg++;
					(void) fprintf(stderr,
	gettext("ps: %s is an invalid non-numeric argument for -g option\n"),
					    parg);
				}
			} while (*p1);
			break;
		case 'u':		/* effective user name or number */
			uflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				add_ugentry(&euid_tbl, parg);
			} while (*p1);
			break;
		case 'U':		/* real user name or number */
			Uflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				add_ugentry(&ruid_tbl, parg);
			} while (*p1);
			break;
		case 'G':		/* real group name or number */
			Gflg++;
			p1 = optarg;
			do {
				parg = getarg(&p1);
				add_ugentry(&rgid_tbl, parg);
			} while (*p1);
			break;
		case 'o':		/* output format */
			p = optarg;
			while ((p = parse_format(p)) != NULL)
				;
			break;
		default:			/* error on ? */
			errflg++;
			break;
		}

	if (errflg || optind < argc || pgerrflg)
		usage();

	if (!readata()) {	/* get data from psfile */
		getdev();
		wrdata();
	}

	if (tflg)
		tty[ntty] = NULL;
	/*
	 * If an appropriate option has not been specified, use the
	 * current terminal and effective uid as the default.
	 */
	if (!(aflg|Aflg|dflg|Gflg|Uflg|uflg|tflg|pflg|gflg|sflg)) {
		psinfo_t info;
		int procfd;
		char *name;

		/* get our own controlling tty name using /proc */
		(void) sprintf(pname, "%s/self/psinfo", procdir);
		if ((procfd = open(pname, O_RDONLY)) < 0 ||
		    read(procfd, (char *)&info, sizeof (info)) < 0 ||
		    info.pr_ttydev == PRNODEV) {
			(void) fprintf(stderr,
			    gettext("ps: no controlling terminal\n"));
			exit(1);
		}
		(void) close(procfd);

		i = 0;
		name = gettty(&info, &i);
		if (*name == '?' &&
		    !getdevcalled && majorexists(info.pr_ttydev)) {
			getdev();
			wrdata();
			i = 0;
			name = gettty(&info, &i);
		}
		if (*name == '?') {
			(void) fprintf(stderr,
			    gettext("ps: can't find controlling terminal\n"));
			exit(1);
		}
		if (ntty == ttysz) {
			if ((ttysz *= 2) == 0)
				ttysz = NTTYS;
			tty = Realloc(tty,
			    (ttysz + 1) * sizeof (char *));
		}
		tty[ntty++] = name;
		tty[ntty] = NULL;
		tflg++;
		tuid = getuid();
	}
	if (Aflg)
		Gflg = Uflg = uflg = pflg = sflg = gflg = aflg = dflg = 0;
	if (Aflg | aflg | dflg)
		tflg = 0;

	i = 0;		/* prepare to exit on name lookup errors */
	i += uconv(&euid_tbl);
	i += uconv(&ruid_tbl);
	i += gconv(&egid_tbl);
	i += gconv(&rgid_tbl);
	if (i)
		exit(1);

	/* allocate a buffer for lwpsinfo structures */
	lpbufsize = 4096;
	if (Lflg && (lpsinfobuf = malloc(lpbufsize)) == NULL) {
		(void) fprintf(stderr,
		    gettext("ps: no memory\n"));
		exit(1);
	}

	if (fields) {	/* print user-specified header */
		if (do_header) {
			struct field *f;

			for (f = fields; f != NULL; f = f->next) {
				if (f != fields)
					(void) printf(" ");
				switch (f->fname) {
				case F_TTY:
					(void) printf("%-*s",
					    f->width, f->header);
					break;
				case F_FNAME:
				case F_COMM:
				case F_ARGS:
					/*
					 * Print these headers full width
					 * unless they appear at the end.
					 */
					if (f->next != NULL) {
						(void) printf("%-*s",
						    f->width, f->header);
					} else {
						(void) printf("%s",
						    f->header);
					}
					break;
				default:
					(void) printf("%*s",
					    f->width, f->header);
					break;
				}
			}
			(void) printf("\n");
		}
	} else {	/* print standard header */
		if (lflg) {
			if (yflg)
				(void) printf(" S");
			else
				(void) printf(" F S");
		}
		if (fflg) {
			if (lflg)
				(void) printf(" ");
			(void) printf("     UID");
		} else if (lflg)
			(void) printf("   UID");

		(void) printf("%*s", pidwidth + 1,  "PID");
		if (lflg || fflg)
			(void) printf("%*s", pidwidth + 1, "PPID");
		if (jflg)
			(void) printf("%*s%*s", pidwidth + 1, "PGID",
			    pidwidth + 1, "SID");
		if (Lflg)
			(void) printf("   LWP");
		if (Pflg)
			(void) printf(" PSR");
		if (Lflg && fflg)
			(void) printf("  NLWP");
		if (cflg)
			(void) printf("  CLS PRI");
		else if (lflg || fflg) {
			(void) printf("  C");
			if (lflg)
				(void) printf(" PRI NI");
		}
		if (lflg) {
			if (yflg)
				(void) printf("   RSS     SZ    WCHAN");
			else
				(void) printf("     ADDR     SZ    WCHAN");
		}
		if (fflg)
			(void) printf("    STIME");
		if (Lflg)
			(void) printf(" TTY     LTIME CMD\n");
		else
			(void) printf(" TTY      TIME CMD\n");
	}

	/*
	 * Determine which processes to print info about by searching
	 * the /proc directory and looking at each process.
	 */
	if ((dirp = opendir(procdir)) == NULL) {
		(void) fprintf(stderr,
		    gettext("ps: cannot open PROC directory %s\n"),
		    procdir);
		exit(1);
	}

	(void) strcpy(pname, procdir);
	pdlen = strlen(pname);
	pname[pdlen++] = '/';

	/* for each active process --- */
	while (dentp = readdir(dirp)) {
		int	found;
		int	procfd;	/* filedescriptor for /proc/nnnnn/psinfo */
		char	*tp;	/* ptr to ttyname,  if any */
		psinfo_t info;	/* process information from /proc */
		lwpsinfo_t *lwpsinfo;	/* array of lwpsinfo structs */

		if (dentp->d_name[0] == '.')		/* skip . and .. */
			continue;
retry:
		(void) strcpy(pname + pdlen, dentp->d_name);
		(void) strcpy(pname + pdlen + strlen(dentp->d_name), "/psinfo");
		if ((procfd = open(pname, O_RDONLY)) == -1)
			continue;
		/*
		 * Get the info structure for the process and close quickly.
		 */
		if (read(procfd, (char *)&info, sizeof (info)) < 0) {
			int	saverr = errno;

			(void) close(procfd);
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				(void) fprintf(stderr,
				    gettext("ps: read() on %s: %s\n"),
				    pname, err_string(saverr));
			continue;
		}
		(void) close(procfd);

		found = 0;
		if (info.pr_lwp.pr_state == 0)		/* can't happen? */
			continue;

		/*
		 * Omit session group leaders for 'a' and 'd' options.
		 */
		if ((info.pr_pid == info.pr_sid) && (dflg || aflg))
			continue;
		if (Aflg || dflg)
			found++;
		else if (pflg && search(pid, npid, info.pr_pid))
			found++;	/* ppid in p option arg list */
		else if (uflg && ugfind(info.pr_euid, &euid_tbl))
			found++;	/* puid in u option arg list */
		else if (Uflg && ugfind(info.pr_uid, &ruid_tbl))
			found++;	/* puid in U option arg list */
#ifdef NOT_YET
		else if (gflg && ugfind(info.pr_egid, &egid_tbl))
			found++;	/* pgid in g option arg list */
#endif	/* NOT_YET */
		else if (Gflg && ugfind(info.pr_gid, &rgid_tbl))
			found++;	/* pgid in G option arg list */
		else if (gflg && search(grpid, ngrpid, info.pr_pgid))
			found++;	/* grpid in g option arg list */
		else if (sflg && search(sessid, nsessid, info.pr_sid))
			found++;	/* sessid in s option arg list */
		if (!found && !tflg && !aflg)
			continue;
		if (!prfind(found, &info, &tp))
			continue;
		if (Lflg && info.pr_nlwp > 1) {
			int prsz;

			(void) strcpy(pname + pdlen, dentp->d_name);
			(void) strcpy(pname + pdlen + strlen(dentp->d_name),
				"/lpsinfo");
			if ((procfd = open(pname, O_RDONLY)) == -1)
				continue;
			/*
			 * Get the info structures for the lwps.
			 */
			prsz = read(procfd, lpsinfobuf, lpbufsize);
			if (prsz < 0) {
				int	saverr = errno;

				(void) close(procfd);
				if (saverr == EAGAIN)
					goto retry;
				if (saverr != ENOENT)
					(void) fprintf(stderr,
					    gettext("ps: read() on %s: %s\n"),
					    pname, err_string(saverr));
				continue;
			}
			(void) close(procfd);
			if (prsz == lpbufsize) {	/* buffer overflow */
				lpbufsize *= 2;
				free(lpsinfobuf);
				if ((lpsinfobuf = malloc(lpbufsize)) == NULL) {
					(void) fprintf(stderr,
					    gettext("ps: no memory\n"));
					exit(1);
				}
				goto retry;
			}
			if (lpsinfobuf->pr_nent != info.pr_nlwp)
				goto retry;
			lwpsinfo = (lwpsinfo_t *)(lpsinfobuf + 1);
		}
		if (!Lflg || info.pr_nlwp <= 1)
			prcom(&info, tp);
		else {
			int nlwp = 0;

			do {
				info.pr_lwp = *lwpsinfo;
				prcom(&info, tp);
				/* LINTED improper alignment */
				lwpsinfo = (lwpsinfo_t *)((char *)lwpsinfo +
					lpsinfobuf->pr_entsize);
			} while (++nlwp < info.pr_nlwp);
		}
		retcode = 0;
	}

	(void) closedir(dirp);
	return (retcode);
}

static void
usage()		/* print usage message and quit */
{
	static char usage1[] =
	    "ps [ -aAdeflcjLPy ] [ -o format ] [ -t termlist ]";
	static char usage2[] =
	    "\t[ -u userlist ] [ -U userlist ] [ -G grouplist ]";
	static char usage3[] =
	    "\t[ -p proclist ] [ -g pgrplist ] [ -s sidlist ]";
	static char usage4[] =
	    "  'format' is one or more of:";
	static char usage5[] =
	    "\tuser ruser group rgroup uid ruid gid rgid pid ppid pgid sid";
	static char usage6[] =
	    "\tpri opri pcpu pmem vsz rss osz nice class time etime stime";
	static char usage7[] =
	    "\tf s c lwp nlwp psr tty addr wchan fname comm args";

	(void) fprintf(stderr,
	    gettext("usage: %s\n%s\n%s\n%s\n%s\n%s\n%s\n"),
	    gettext(usage1), gettext(usage2), gettext(usage3),
	    gettext(usage4), gettext(usage5), gettext(usage6), gettext(usage7));
	exit(1);
}

/*
 * readata reads in the open devices (terminals) and stores
 * info in the devl structure.
 */
static char	psfile[] = "/tmp/ps_data";

static int
readata()
{
	int fd;

	ndev = nmajor = 0;
	if ((fd = open(psfile, O_RDONLY)) == -1)
		return (0);

	if (psread(fd, (char *)&ndev, sizeof (ndev)) == 0 ||
	    (devl = (struct devl *)malloc(ndev * sizeof (*devl))) == NULL)
		goto bad;
	maxdev = ndev;
	if (psread(fd, (char *)devl, ndev * sizeof (*devl)) == 0)
		goto bad;

	if (psread(fd, (char *)&nmajor, sizeof (nmajor)) == 0 ||
	    (majordev = (int *)malloc(nmajor * sizeof (int))) == NULL)
		goto bad;
	maxmajor = nmajor;
	if (psread(fd, (char *)majordev, nmajor * sizeof (int)) == 0)
		goto bad;

	(void) close(fd);
	return (1);

bad:
	if (devl)
		free(devl);
	if (majordev)
		free(majordev);
	devl = NULL;
	majordev = NULL;
	maxdev = ndev = 0;
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
			gettext("ps: not enough memory for %d devices\n"),
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

			for (i = start; i < leng && objptr[i] != '/'; i++)
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
			gettext("ps: not enough memory for %d major numbers\n"),
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
		    gettext("ps: mktemp(\"/tmp/ps.XXXXXX\") failed, %s\n"),
		    err_string(errno));
		(void) fprintf(stderr,
		    gettext("ps: Please notify your System Administrator\n"));
		return;
	}

	if ((fd = open(tfname, O_WRONLY|O_CREAT|O_EXCL, 0664)) < 0) {
		(void) fprintf(stderr,
		    gettext("ps: open(\"%s\") for write failed, %s\n"),
		    tfname, err_string(errno));
		(void) fprintf(stderr,
		    gettext("ps: Please notify your System Administrator\n"));
		return;
	}

	/*
	 * Make owner root, group sys.
	 */
	(void) fchown(fd, (uid_t)0, (gid_t)3);

	/* write /dev data */
	pswrite(fd, (char *)&ndev, sizeof (ndev));
	pswrite(fd, (char *)devl, ndev * sizeof (*devl));
	pswrite(fd, (char *)&nmajor, sizeof (nmajor));
	pswrite(fd, (char *)majordev, nmajor * sizeof (int));

	(void) close(fd);

	if (rename(tfname, psfile) != 0) {
		(void) fprintf(stderr,
		    gettext("ps: rename(\"%s\",\"%s\") failed, %s\n"),
		    tfname, psfile, err_string(errno));
		(void) fprintf(stderr,
		    gettext("ps: Please notify your System Administrator\n"));
		return;
	}
}

/*
 * getarg() finds the next argument in list and copies arg into argbuf.
 * p1 first pts to arg passed back from getopt routine.  p1 is then
 * bumped to next character that is not a comma or blank -- p1 NULL
 * indicates end of list.
 */
static char *
getarg(char **pp1)
{
	static char argbuf[ARGSIZ];
	char *p1 = *pp1;
	char *parga = argbuf;
	int c;

	while ((c = *p1) != '\0' && (c == ',' || isspace(c)))
		p1++;

	while ((c = *p1) != '\0' && c != ',' && !isspace(c)) {
		if (parga < argbuf + ARGSIZ - 1)
			*parga++ = c;
		p1++;
	}
	*parga = '\0';

	while ((c = *p1) != '\0' && (c == ',' || isspace(c)))
		p1++;

	*pp1 = p1;

	return (argbuf);
}

/*
 * parse_format() takes the argument to the -o option,
 * sets up the next output field structure, and returns
 * a pointer to any further output field specifier(s).
 * As a side-effect, it increments errflg if encounters a format error.
 */
static char *
parse_format(char *arg)
{
	int c;
	char *name;
	char *header = NULL;
	int width = 0;
	struct def_field *df;
	struct field *f;

	while ((c = *arg) != '\0' && (c == ',' || isspace(c)))
		arg++;
	if (c == '\0')
		return (NULL);
	name = arg;
	arg = strpbrk(arg, " \t\r\v\f\n,=");
	if (arg != NULL) {
		c = *arg;
		*arg++ = '\0';
		if (c == '=') {
			char *s;

			header = arg;
			arg = NULL;
			width = strlen(header);
			s = header + width;
			while (s > header && isspace(*--s))
				*s = '\0';
			while (isspace(*header))
				header++;
		}
	}
	for (df = &fname[0]; df < &fname[NFIELDS]; df++)
		if (strcmp(name, df->fname) == 0) {
			if (strcmp(name, "lwp") == 0)
				Lflg++;
			break;
		}
	if (df >= &fname[NFIELDS]) {
		(void) fprintf(stderr,
			gettext("ps: unknown output format: -o %s\n"),
			name);
		errflg++;
		return (arg);
	}
	if ((f = malloc(sizeof (*f))) == NULL) {
		(void) fprintf(stderr,
		    gettext("ps: malloc() for output format failed, %s\n"),
		    err_string(errno));
		exit(1);
	}
	f->next = NULL;
	f->fname = df - &fname[0];
	f->header = header? header : df->header;
	if (width == 0)
		width = df->width;
	if (*f->header != '\0')
		do_header = 1;
	f->width = max(width, df->minwidth);

	if (fields == NULL)
		fields = last_field = f;
	else {
		last_field->next = f;
		last_field = f;
	}

	return (arg);
}

/*
 * gettty returns the user's tty number or ? if none.
 * ip == where the search left off last time
 */
static char *
gettty(psinfo_t *psinfo, int *ip)
{
	int	i;

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
 * Find the process's tty and return 1 if process is to be printed.
 */
static int
prfind(int found, psinfo_t *psinfo, char **tpp)
{
	char	*tp;
	int	i;
	char	**ttyp, *str;

	if (psinfo->pr_nlwp == 0) {
		/* process is a zombie */
		*tpp = "?";
		if (tflg && !found)
			return (0);
		return (1);
	}

	/*
	 * Get current terminal.  If none ("?") and 'a' is set, don't print
	 * info.  If 't' is set, check if term is in list of desired terminals
	 * and print it if it is.
	 */
	i = 0;
	tp = gettty(psinfo, &i);
	if (*tp == '?' && psinfo->pr_ttydev != PRNODEV &&
	    !getdevcalled && majorexists(psinfo->pr_ttydev)) {
		getdev();
		wrdata();
		i = 0;
		tp = gettty(psinfo, &i);
	}
	if (aflg && *tp == '?') {
		*tpp = tp;
		return (0);
	}
	if (tflg && !found) {
		int match = 0;

		/*
		 * Look for same device under different names.
		 */
		while (i >= 0 && !match) {
			for (ttyp = tty; (str = *ttyp) != 0 && !match; ttyp++)
				if (strcmp(tp, str) == 0)
					match = 1;
			if (!match)
				tp = gettty(psinfo, &i);
		}
		if (!match || (tuid != -1 && tuid != psinfo->pr_euid)) {
			*tpp = tp;
			return (0);
		}
	}
	*tpp = tp;
	return (1);
}

/*
 * Print info about the process.
 */
static void
prcom(psinfo_t *psinfo, char *tp)
{
	char	*cp;
	long	tm;
	int	bytesleft;
	int	wcnt, length;
	wchar_t	wchar;
	struct passwd *pwd;

	/*
	 * If process is zombie, call zombie print routine and return.
	 */
	if (psinfo->pr_nlwp == 0) {
		przom(psinfo);
		return;
	}

	/*
	 * If user specified '-o format', print requested fields and return.
	 */
	if (fields) {
		pr_fields(psinfo, tp);
		return;
	}

	if (lflg) {
		if (!yflg)
			(void) printf("%2x", psinfo->pr_flag & 0377); /* F */
		(void) printf(" %c", psinfo->pr_lwp.pr_sname);	/* S */
		if (fflg)
			(void) printf(" ");
	}
	if (fflg) {						/* UID */
		if ((pwd = getpwuid(psinfo->pr_euid)) != NULL)
			(void) printf("%8.8s", pwd->pw_name);
		else
			(void) printf(" %7.7d", (int)psinfo->pr_euid);
	} else if (lflg) {
		(void) printf(" %5d", (int)psinfo->pr_euid);
	}
	(void) printf("%*d", pidwidth + 1, (int)psinfo->pr_pid); /* PID */
	if (lflg || fflg)
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_ppid); /* PPID */
	if (jflg) {
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_pgid);	/* PGID */
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_sid);	/* SID  */
	}
	if (Lflg)
		(void) printf("%6d", (int)psinfo->pr_lwp.pr_lwpid); /* LWP */
	if (Pflg) {
		if (psinfo->pr_lwp.pr_bindpro == PBIND_NONE)	/* PSR */
			(void) printf("   -");
		else
			(void) printf("%4d", psinfo->pr_lwp.pr_bindpro);
	}
	if (Lflg && fflg)
		(void) printf("%6d", psinfo->pr_nlwp);		/* NLWP */
	if (cflg) {
		(void) printf("%5s", psinfo->pr_lwp.pr_clname);	/* CLS  */
		(void) printf("%4d", psinfo->pr_lwp.pr_pri);	/* PRI	*/
	} else if (lflg || fflg) {
		(void) printf("%3d", psinfo->pr_lwp.pr_cpu & 0377); /* C   */
		if (lflg) {					/* PRI NI */
			/*
			 * Print priorities the old way (lower numbers
			 * mean higher priority) and print nice value
			 * for time sharing procs.
			 */
			(void) printf("%4d", psinfo->pr_lwp.pr_oldpri);
			if (psinfo->pr_lwp.pr_oldpri != 0)
				(void) printf("%3d", psinfo->pr_lwp.pr_nice);
			else
				(void) printf(" %2.2s",
				    psinfo->pr_lwp.pr_clname);
		}
	}
	if (lflg) {
		if (yflg) {
			if (psinfo->pr_flag & SSYS)		/* RSS */
				(void) printf("     0");
			else if (psinfo->pr_rssize)
				(void) printf("%6lu",
					(ulong_t)psinfo->pr_rssize);
			else
				(void) printf("     ?");
			if (psinfo->pr_flag & SSYS)		/* SZ */
				(void) printf("      0");
			else if (psinfo->pr_size)
				(void) printf("%7lu",
					(ulong_t)psinfo->pr_size);
			else
				(void) printf("      ?");
		} else {
#ifndef _LP64
			if (psinfo->pr_addr)			/* ADDR */
				(void) printf("%9lx",
					(ulong_t)psinfo->pr_addr);
			else
#endif
				(void) printf("        ?");
			if (psinfo->pr_flag & SSYS)		/* SZ */
				(void) printf("      0");
			else if (psinfo->pr_size)
				(void) printf("%7lu",
				    (ulong_t)psinfo->pr_size / kbytes_per_page);
			else
				(void) printf("      ?");
		}
		if (psinfo->pr_lwp.pr_sname != 'S')		/* WCHAN */
			(void) printf("         ");
#ifndef _LP64
		else if (psinfo->pr_lwp.pr_wchan)
			(void) printf(" %8lx",
				(ulong_t)psinfo->pr_lwp.pr_wchan);
#endif
		else
			(void) printf("        ?");
	}
	if (fflg) {						/* STIME */
		if (Lflg)
			prtime(psinfo->pr_lwp.pr_start, 9, 1);
		else
			prtime(psinfo->pr_start, 9, 1);
	}
	(void) printf(" %-8.14s", tp);				/* TTY */
	if (Lflg) {
		tm = psinfo->pr_lwp.pr_time.tv_sec;
		if (psinfo->pr_lwp.pr_time.tv_nsec > 500000000)
			tm++;
	} else {
		tm = psinfo->pr_time.tv_sec;
		if (psinfo->pr_time.tv_nsec > 500000000)
			tm++;
	}
	(void) printf("%2ld:%.2ld", tm / 60, tm % 60);		/* [L]TIME */

	if (!fflg) {						/* CMD */
		wcnt = namencnt(psinfo->pr_fname, 16, 8);
		(void) printf(" %.*s\n", wcnt, psinfo->pr_fname);
		return;
	}

	/*
	 * PRARGSZ == length of cmd arg string.
	 */
	psinfo->pr_psargs[PRARGSZ-1] = '\0';
	bytesleft = PRARGSZ;
	for (cp = psinfo->pr_psargs; *cp != '\0'; cp += length) {
		length = mbtowc(&wchar, cp, MB_LEN_MAX);
		if (length == 0)
			break;
		if (length < 0 || !iswprint(wchar)) {
			if (length < 0)
				length = 1;
			if (bytesleft <= length) {
				*cp = '\0';
				break;
			}
			/* omit the unprintable character */
			(void) memmove(cp, cp+length, bytesleft-length);
			length = 0;
		}
		bytesleft -= length;
	}
	wcnt = namencnt(psinfo->pr_psargs, PRARGSZ, lflg ? 35 : PRARGSZ);
	(void) printf(" %.*s\n", wcnt, psinfo->pr_psargs);
}

/*
 * Print percent from 16-bit binary fraction [0 .. 1]
 * Round up .01 to .1 to indicate some small percentage (the 0x7000 below).
 */
static void
prtpct(u_short pct, int width)
{
	uint_t value = pct;	/* need 32 bits to compute with */

	value = ((value * 1000) + 0x7000) >> 15;	/* [0 .. 1000] */
	if (value >= 1000)
		value = 999;
	if ((width -= 2) < 2)
		width = 2;
	(void) printf("%*u.%u", width, value / 10, value % 10);
}

static void
print_time(time_t tim, int width)
{
	union {
		char buf[16];
		struct {
			char day[6];
			char hyphen;
			char hour[2];
			char hcolon;
			char min[2];
			char mincolon;
			char sec[2];
			char nullbyte;
		} tm;
	} un;
	char *cp;
	time_t seconds;
	time_t minutes;
	time_t hours;
	time_t days;

	seconds = tim % 60;
	tim /= 60;
	minutes = tim % 60;
	tim /= 60;
	hours = tim % 24;
	days = tim / 24;

	(void) memset(un.buf, ' ', sizeof (un.buf));
	(void) sprintf(un.tm.sec, "%2.2ld", seconds);
	if (hours == 0 && days == 0) {
		(void) sprintf(un.tm.min, "%2ld", minutes);
		un.tm.mincolon = ':';
	} else {
		(void) sprintf(un.tm.min, "%2.2ld", minutes);
		un.tm.mincolon = ':';
		(void) sprintf(un.tm.hour, "%2.2ld", hours);
		un.tm.hcolon = ':';
		if (days != 0) {
			(void) sprintf(un.tm.day, "%6ld", days);
			un.tm.hyphen = '-';
		}
	}
	for (cp = un.buf; *cp == ' '; cp++)
		;
	(void) printf("%*s", width, cp);
}

static void
print_field(psinfo_t *psinfo, struct field *f, char *tp)
{
	static time_t now = 0L;

	int width = f->width;
	struct passwd *pwd;
	struct group *grp;
	time_t then;
	time_t cputime;
	int bytesleft;
	int wcnt;
	wchar_t	wchar;
	char *cp;
	int length;
	u_long mask;
	char c, *csave;

	switch (f->fname) {
	case F_RUSER:
		if ((pwd = getpwuid(psinfo->pr_uid)) != NULL)
			(void) printf("%*s", width, pwd->pw_name);
		else
			(void) printf("%*d", width, (int)psinfo->pr_uid);
		break;
	case F_USER:
		if ((pwd = getpwuid(psinfo->pr_euid)) != NULL)
			(void) printf("%*s", width, pwd->pw_name);
		else
			(void) printf("%*d", width, (int)psinfo->pr_euid);
		break;
	case F_RGROUP:
		if ((grp = getgrgid(psinfo->pr_gid)) != NULL)
			(void) printf("%*s", width, grp->gr_name);
		else
			(void) printf("%*d", width, (int)psinfo->pr_gid);
		break;
	case F_GROUP:
		if ((grp = getgrgid(psinfo->pr_egid)) != NULL)
			(void) printf("%*s", width, grp->gr_name);
		else
			(void) printf("%*d", width, (int)psinfo->pr_egid);
		break;
	case F_RUID:
		(void) printf("%*d", width, (int)psinfo->pr_uid);
		break;
	case F_UID:
		(void) printf("%*d", width, (int)psinfo->pr_euid);
		break;
	case F_RGID:
		(void) printf("%*d", width, (int)psinfo->pr_gid);
		break;
	case F_GID:
		(void) printf("%*d", width, (int)psinfo->pr_egid);
		break;
	case F_PID:
		(void) printf("%*d", width, (int)psinfo->pr_pid);
		break;
	case F_PPID:
		(void) printf("%*d", width, (int)psinfo->pr_ppid);
		break;
	case F_PGID:
		(void) printf("%*d", width, (int)psinfo->pr_pgid);
		break;
	case F_SID:
		(void) printf("%*d", width, (int)psinfo->pr_sid);
		break;
	case F_PSR:
		if (psinfo->pr_lwp.pr_bindpro == PBIND_NONE)
			(void) printf("%*s", width, "-");
		else
			(void) printf("%*d", width, psinfo->pr_lwp.pr_bindpro);
		break;
	case F_LWP:
		(void) printf("%*d", width, (int)psinfo->pr_lwp.pr_lwpid);
		break;
	case F_NLWP:
		(void) printf("%*d", width, psinfo->pr_nlwp);
		break;
	case F_OPRI:
		(void) printf("%*d", width, psinfo->pr_lwp.pr_oldpri);
		break;
	case F_PRI:
		(void) printf("%*d", width, psinfo->pr_lwp.pr_pri);
		break;
	case F_F:
		mask = 0xffffffffUL;
		if (width < 8)
			mask >>= (8 - width) * 4;
		(void) printf("%*lx", width, psinfo->pr_flag & mask);
		break;
	case F_S:
		(void) printf("%*c", width, psinfo->pr_lwp.pr_sname);
		break;
	case F_C:
		(void) printf("%*d", width, psinfo->pr_lwp.pr_cpu);
		break;
	case F_PCPU:
		prtpct(psinfo->pr_pctcpu, width);
		break;
	case F_PMEM:
		prtpct(psinfo->pr_pctmem, width);
		break;
	case F_OSZ:
		(void) printf("%*lu", width,
			(ulong_t)psinfo->pr_size / kbytes_per_page);
		break;
	case F_VSZ:
		(void) printf("%*lu", width, (ulong_t)psinfo->pr_size);
		break;
	case F_RSS:
		(void) printf("%*lu", width, (ulong_t)psinfo->pr_rssize);
		break;
	case F_NICE:
		/* if pr_oldpri is zero, then this class has no nice */
		if (psinfo->pr_lwp.pr_oldpri != 0)
			(void) printf("%*d", width, psinfo->pr_lwp.pr_nice);
		else
			(void) printf("%*.*s", width, width,
				psinfo->pr_lwp.pr_clname);
		break;
	case F_CLASS:
		(void) printf("%*.*s", width, width, psinfo->pr_lwp.pr_clname);
		break;
	case F_STIME:
		prtime(psinfo->pr_start, width, 0);
		break;
	case F_ETIME:
		if (now == 0L)
			now = time((time_t *)0);
		then = psinfo->pr_start.tv_sec;
		if (psinfo->pr_start.tv_nsec > 500000000)
			then++;
		print_time(now - then, width);
		break;
	case F_TIME:
		cputime = psinfo->pr_time.tv_sec;
		if (psinfo->pr_time.tv_nsec > 500000000)
			cputime++;
		print_time(cputime, width);
		break;
	case F_TTY:
		(void) printf("%-*s", width, tp);
		break;
	case F_ADDR:
		(void) printf("%*lx", width, (long)psinfo->pr_addr);
		break;
	case F_WCHAN:
		if (psinfo->pr_lwp.pr_wchan)
			(void) printf("%*lx", width,
				(long)psinfo->pr_lwp.pr_wchan);
		else
			(void) printf("%*.*s", width, width, "-");
		break;
	case F_FNAME:
		/*
		 * Print full width unless this is the last output format.
		 */
		wcnt = namencnt(psinfo->pr_fname, 16, width);
		if (f->next != NULL)
			(void) printf("%-*.*s", width, wcnt, psinfo->pr_fname);
		else
			(void) printf("%-.*s", wcnt, psinfo->pr_fname);
		break;
	case F_COMM:
		csave = strpbrk(psinfo->pr_psargs, " \t\r\v\f\n");
		if (csave) {
			c = *csave;
			*csave = '\0';
		}
		/* FALLTHROUGH */
	case F_ARGS:
		/*
		 * PRARGSZ == length of cmd arg string.
		 */
		psinfo->pr_psargs[PRARGSZ-1] = '\0';
		bytesleft = PRARGSZ;
		for (cp = psinfo->pr_psargs; *cp != '\0'; cp += length) {
			length = mbtowc(&wchar, cp, MB_LEN_MAX);
			if (length == 0)
				break;
			if (length < 0 || !iswprint(wchar)) {
				if (length < 0)
					length = 1;
				if (bytesleft <= length) {
					*cp = '\0';
					break;
				}
				/* omit the unprintable character */
				(void) memmove(cp, cp+length, bytesleft-length);
				length = 0;
			}
			bytesleft -= length;
		}
		wcnt = namencnt(psinfo->pr_psargs, PRARGSZ, width);
		/*
		 * Print full width unless this is the last format.
		 */
		if (f->next != NULL)
			(void) printf("%-*.*s", width, wcnt,
			    psinfo->pr_psargs);
		else
			(void) printf("%-.*s", wcnt,
			    psinfo->pr_psargs);
		if (f->fname == F_COMM && csave)
			*csave = c;
		break;
	}
}

static void
pr_fields(psinfo_t *psinfo, char *tp)
{
	struct field *f;

	for (f = fields; f != NULL; f = f->next) {
		print_field(psinfo, f, tp);
		if (f->next != NULL)
			(void) printf(" ");
	}
	(void) printf("\n");
}

/*
 * Returns 1 if arg is found in array arr, of length num; 0 otherwise.
 */
static int
search(pid_t *arr, int number, pid_t arg)
{
	int i;

	for (i = 0; i < number; i++)
		if (arg == arr[i])
			return (1);
	return (0);
}

/*
 * Add an entry (user, group) to the specified table.
 */
static void
add_ugentry(struct ughead *tbl, char *name)
{
	struct ugdata *entp;

	if (tbl->size == tbl->nent) {	/* reallocate the table entries */
		if ((tbl->size *= 2) == 0)
			tbl->size = 32;		/* first time */
		tbl->ent = Realloc(tbl->ent, tbl->size*sizeof (struct ugdata));
	}
	entp = &tbl->ent[tbl->nent++];
	entp->id = 0;
	(void) strncpy(entp->name, name, MAXUGNAME);
	entp->name[MAXUGNAME] = '\0';
}

static int
uconv(struct ughead *uhead)
{
	struct ugdata *utbl = uhead->ent;
	int n = uhead->nent;
	struct passwd *pwd;
	int i, j;
	int rval = 0;

	/*
	 * Ask the name service for names.
	 */
	for (i = 0; i < n; i++) {
		/*
		 * If name is numeric, ask for numeric id
		 */
		if (utbl[i].name[0] >= '0' &&
		    utbl[i].name[0] <= '9')
			pwd = getpwuid((uid_t)atol(utbl[i].name));
		else
			pwd = getpwnam(utbl[i].name);

		/*
		 * If found, enter found index into tbl array.
		 */
		if (pwd != NULL) {
			utbl[i].id = pwd->pw_uid;
			(void) strncpy(utbl[i].name, pwd->pw_name, MAXUGNAME);
		} else {
			rval++;
			(void) fprintf(stderr,
			    gettext("ps: unknown user %s\n"), utbl[i].name);
			for (j = i + 1; j < n; j++) {
				(void) strncpy(utbl[j-1].name,
				    utbl[j].name, MAXUGNAME);
			}
			n--;
			i--;
		}
	}
	uhead->nent = n;	/* in case it changed */
	return (rval);
}

static int
gconv(struct ughead *ghead)
{
	struct ugdata *gtbl = ghead->ent;
	int n = ghead->nent;
	struct group *grp;
	int i, j;
	int rval = 0;

	/*
	 * Ask the name service for names.
	 */
	for (i = 0; i < n; i++) {
		/*
		 * If name is numeric, ask for numeric id
		 */
		if (gtbl[i].name[0] >= '0' &&
		    gtbl[i].name[0] <= '9')
			grp = getgrgid((gid_t)atol(gtbl[i].name));
		else
			grp = getgrnam(gtbl[i].name);
		/*
		 * If found, enter found index into tbl array.
		 */
		if (grp != NULL) {
			gtbl[i].id = grp->gr_gid;
			(void) strncpy(gtbl[i].name, grp->gr_name, MAXUGNAME);
		} else {
			rval++;
			(void) fprintf(stderr,
			    gettext("ps: unknown group %s\n"), gtbl[i].name);
			for (j = i + 1; j < n; j++) {
				(void) strncpy(gtbl[j-1].name,
				    gtbl[j].name, MAXUGNAME);
			}
			n--;
			i--;
		}
	}
	ghead->nent = n;	/* in case it changed */
	return (rval);
}

/*
 * Return 1 if puid is in table, otherwise 0.
 */
static int
ugfind(id_t id, struct ughead *ughead)
{
	struct ugdata *utbl = ughead->ent;
	int n = ughead->nent;
	int i;

	for (i = 0; i < n; i++)
		if (utbl[i].id == id)
			return (1);
	return (0);
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
		    gettext("ps: psread() error on read, rbs=%d, bs=%d, %s\n"),
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
pswrite(int fd, char *bp, unsigned int bs)
{
	int	wbs;

	if ((wbs = write(fd, bp, bs)) != bs) {
		(void) fprintf(stderr,
		gettext("ps: pswrite() error on write, wbs=%d, bs=%d, %s\n"),
		    wbs, bs, err_string(errno));
		(void) remove(psfile);
	}
}

/*
 * Print starting time of process unless process started more than 24 hours
 * ago, in which case the date is printed.  The date is printed in the form
 * "MMM dd" if old format, else the blank is replaced with an '_' so
 * it appears as a single word (for parseability).
 */
static void
prtime(timestruc_t st, int width, int old)
{
	char sttim[26];
	static time_t tim = 0L;
	time_t starttime;

	if (tim == 0L)
		tim = time((time_t *)0);
	starttime = st.tv_sec;
	if (st.tv_nsec > 500000000)
		starttime++;
	if (tim - starttime > 24*60*60) {
		(void) cftime(sttim, old? "%b %d" : "%b_%d", &starttime);
		sttim[7] = '\0';
	} else {
		(void) cftime(sttim, "%H:%M:%S", &starttime);
		sttim[8] = '\0';
	}
	(void) printf("%*.*s", width, width, sttim);
}

static void
przom(psinfo_t *psinfo)
{
	long	tm;
	struct passwd *pwd;

	if (fields)  {
		pr_fields(psinfo, "?");
		return;
	}

	if (lflg) {	/* F S */
		if (!yflg)
			(void) printf("%2x", psinfo->pr_flag & 0377); /* F */
		(void) printf(" %c", psinfo->pr_lwp.pr_sname);	/* S */
		if (fflg)
			(void) printf(" ");
	}
	if (fflg) {
		if ((pwd = getpwuid(psinfo->pr_euid)) != NULL)
			(void) printf("%8.8s", pwd->pw_name);
		else
			(void) printf(" %7.7d", (int)psinfo->pr_euid);
	} else if (lflg)
		(void) printf(" %5d", (int)psinfo->pr_euid);
	(void) printf("%*d", pidwidth + 1,
	    (int)psinfo->pr_pid);		/* PID */
	if (lflg || fflg)
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_ppid);	/* PPID */
	if (jflg) {
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_pgid);	/* PGID */
		(void) printf("%*d", pidwidth + 1,
		    (int)psinfo->pr_sid);	/* SID  */
	}
	if (Lflg)
		(void) printf("      ");			/* LWP */
	if (Pflg)
		(void) printf("   -");				/* PSR */
	if (Lflg && fflg)
		(void) printf("     0");			/* NLWP */
	if (cflg) {
		(void) printf("     ");		/* zombies have no class */
		(void) printf("%4d   ", psinfo->pr_lwp.pr_pri); /* PRI	*/
	} else if (lflg || fflg) {
		(void) printf("%3d", psinfo->pr_lwp.pr_cpu & 0377); /* C   */
		if (lflg)
			(void) printf("%4d", psinfo->pr_lwp.pr_oldpri);
	}
	if (fflg)					/* STIME */
		(void) printf("         ");
	if (lflg) {
		if (yflg)				/* NI RSS SZ WCHAN */
			(void) printf("                         ");
		else					/* NI ADDR SZ WCHAN */
			(void) printf("                            ");
	}
	tm = psinfo->pr_time.tv_sec;
	if (psinfo->pr_time.tv_nsec > 500000000)
		tm++;
	(void) printf("         %2ld:%.2ld", tm / 60, tm % 60); /* TTY TIME */
	(void) printf(" <defunct>\n");
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
namencnt(char *cmd, int csisize, int scrsize)
{
	int csiwcnt = 0, scrwcnt = 0;
	int ncsisz, nscrsz;
	wchar_t  wchar;
	int	 len;

	while (*cmd != '\0') {
		if ((len = csisize - csiwcnt) > (int)MB_CUR_MAX)
			len = MB_CUR_MAX;
		if ((ncsisz = mbtowc(&wchar, cmd, len)) < 0)
			return (8); /* default to use for illegal chars */
		if ((nscrsz = wcwidth(wchar)) <= 0)
			return (8);
		if (csiwcnt + ncsisz > csisize || scrwcnt + nscrsz > scrsize)
			break;
		csiwcnt += ncsisz;
		scrwcnt += nscrsz;
		cmd += ncsisz;
	}
	return (csiwcnt);
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

/* If allocation fails, die */
static void *
Realloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL) {
		(void) fprintf(stderr, gettext("ps: no memory\n"));
		exit(1);
	}
	return (ptr);
}
