/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 */

/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rcp.c	1.24	99/07/18 SMI"

#define	_FILE_OFFSET_BITS  64

/*
 * rcp
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/acl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pwd.h>
#include <netdb.h>
#include <wchar.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#ifdef SYSV
#include <string.h>
#else
#include <strings.h>
#endif /* SYSV */
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef SYSV
#define	rindex	strrchr
#define	index	strchr
#endif /* SYSV */

/*
 * It seems like Berkeley got these from pathnames.h?
 */
#define	_PATH_RSH	"/usr/bin/rsh"
#define	_PATH_CP	"/usr/bin/cp"
#define	_PATH_BSHELL	"/usr/bin/sh"

#define	ACL_FAIL	1
#define	ACL_OK		0


extern int errno;
struct passwd *pwd;
int errs, pflag, port, rem, userid;
int zflag;
int iamremote, iamrecursive, targetshouldbedirectory;
int aclflag;

void lostconn();
static char	*search_char();
char *removebrackets();

#define	RCP_ACL	"/usr/lib/sunw,rcp"	/* see PSARC/1993/004/opinion */
#define	CMDNEEDS	20
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */
char cmd_sunw[CMDNEEDS + sizeof (RCP_ACL) - 3];
				/* as above and "/usr/lib/sunw," */

int	socksize = 24 * 1024;	/* socket  buffer size for performance */

typedef struct _buf {
	int	cnt;
	char	*buf;
} BUF;

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ch, fflag, tflag;
	char *targ, *colon();
	struct passwd *getpwuid();

	(void) setlocale(LC_ALL, "");

	port = htons(IPPORT_CMDSERVER);

	if (strcmp(argv[0], RCP_ACL) == 0)
		aclflag = 1;

	if (!(pwd = getpwuid(userid = getuid()))) {
		(void) fprintf(stderr, "rcp: unknown user %d.\n", userid);
		exit(1);
	}

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, "adfkprtx")) != EOF)
		switch (ch) {
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':			/* "from" */
			fflag = 1;
			if (aclflag)
				/* ok response */
				(void) write(rem, "", 1);
			break;
		case 'p':			/* preserve access/mod times */
			++pflag;
			break;
		case 'r':
			++iamrecursive;
			break;
		case 't':			/* "to" */
			tflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (fflag) {
		iamremote = 1;
		(void) response();
		(void) setuid(userid);
		source(argc, argv);
		exit(errs);
	}

	if (tflag) {
		iamremote = 1;
		(void) setuid(userid);
		sink(argc, argv);
		exit(errs);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	rem = -1;
	(void) sprintf(cmd, "rcp%s%s%s%s",
	    iamrecursive ? " -r" : "",
	    pflag ? " -p" : "",
	    zflag ? " -z" : "",
	    targetshouldbedirectory ? " -d" : "");
	(void) sprintf(cmd_sunw, "%s%s%s%s%s", RCP_ACL,
	    iamrecursive ? " -r" : "",
	    pflag ? " -p" : "",
	    zflag ? " -z" : "",
	    targetshouldbedirectory ? " -d" : "");
	(void) signal(SIGPIPE, lostconn);

	if (targ = colon(argv[argc - 1]))
		toremote(targ, argc, argv);
	else {
		tolocal(argc, argv);
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs);
	/* NOTREACHED */
}

toremote(targ, argc, argv)
	char *targ;
	int argc;
	char **argv;
{
	int i;
	char *bp, *host, *src, *suser, *thost, *tuser;
	char *colon();
	char resp;

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	if (thost = search_char(argv[argc - 1], '@')) {
		*thost++ = 0;
		tuser = argv[argc - 1];
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser))
			exit(1);
	} else {
		thost = argv[argc - 1];
		tuser = NULL;
	}
	thost = removebrackets(thost);

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = search_char(argv[i], '@');
			if (!(bp = malloc((uint_t)(strlen(_PATH_RSH) +
				    strlen(argv[i]) + strlen(src) +
				    (tuser ? strlen(tuser) : 0) +
				    strlen(thost) +
				    strlen(targ)) + CMDNEEDS + 20)))
					nospace();
			if (host) {
				*host++ = 0;
				host = removebrackets(host);
				suser = argv[i];
				if (*suser == '\0')
					suser = pwd->pw_name;
				else if (!okname(suser))
					continue;
				(void) sprintf(bp,
				    "%s %s -l %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, host, suser, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			} else {
				host = removebrackets(argv[i]);
				(void) sprintf(bp, "%s %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, host, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			}
			(void) susystem(bp);
			(void) free(bp);
		} else {			/* local to remote */
			if (rem == -1) {
				if (!(bp = malloc((uint_t)strlen(targ) +
				    CMDNEEDS + 20)))
					nospace();
				/*
				 * ACL support: try to find out if the remote
				 * site is running acl cognizant version of
				 * rcp. A special binary name is used for this
				 * purpose.
				 */
				aclflag = 1;
				(void) sprintf(bp, "%s -t %s", cmd_sunw, targ);
				host = thost;



					rem = rcmd_af(&host, port, pwd->pw_name,
					    tuser ? tuser : pwd->pw_name,
					    bp, 0, AF_INET6);
				if (rem < 0)
					exit(1);
				/*
				 * This is similar to routine response().
				 * If response is not ok, treat the other
				 * side as non-acl rcp.
				 */
				if (read(rem, &resp, sizeof (resp))
				    != sizeof (resp))
					lostconn();
				if (resp != 0) {
					/*
					 * Not OK:
					 * The other side is running
					 * non-acl rcp. Try again with
					 * normal stuff
					 */
					aclflag = 0;
					(void) sprintf(bp, "%s -t %s", cmd,
					    targ);
						close(rem);
						rem = rcmd_af(&host, port,
						    pwd->pw_name,
						    tuser ? tuser :
						    pwd->pw_name, bp, 0,
						    AF_INET6);
					if (rem < 0)
						exit(1);
					if (response() < 0)
						exit(1);
				}
				/* everything should be fine now */
				(void) free(bp);
				(void) setuid(userid);
			}
			source(1, argv+i);
		}
	}
}

tolocal(argc, argv)
	int argc;
	char **argv;
{
	int i;
	char *bp, *host, *src, *suser;
	char *colon();
	char resp;
	int rc;

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {	/* local to local */
			if (!(bp = malloc((uint_t)(strlen(_PATH_CP) +
			    strlen(argv[i]) + strlen(argv[argc - 1])) + 20)))
				nospace();
			(void) sprintf(bp, "%s%s%s%s %s %s", _PATH_CP,
			    iamrecursive ? " -r" : "",
			    pflag ? " -p" : "",
			    zflag ? " -z" : "",
			    argv[i], argv[argc - 1]);
			(void) susystem(bp);
			(void) free(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		host = search_char(argv[i], '@');
		if (host) {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = pwd->pw_name;
			else if (!okname(suser))
				continue;
		} else {
			host = argv[i];
			suser = pwd->pw_name;
		}
		host = removebrackets(host);
		if (!(bp = malloc((uint_t)(strlen(src)) + CMDNEEDS + 20)))
			nospace();

		/*
		 * ACL support: try to find out if the remote site is
		 * running acl cognizant version of rcp.
		 */
		aclflag = 1;
		(void) sprintf(bp, "%s -f %s", cmd_sunw, src);
		rem = rcmd_af(&host, port, pwd->pw_name, suser,
			    bp, 0, AF_INET6);

		if (rem < 0) {
			(void) free(bp);
			continue;
		}

		/*
		 * The remote system is supposed to send an ok response.
		 * If there are any data other than "ok", it must be error
		 * messages from the remote system. We can assume the
		 * remote system is running non-acl version rcp.
		 */
		if (read(rem, &resp, sizeof (resp)) != sizeof (resp))
			lostconn();
		if (resp != 0) {
			/*
			 * NOT ok:
			 * The other side is running non-acl rcp.
			 * Try again with normal stuff
			 */
			aclflag = 0;
			(void) sprintf(bp, "%s -f %s", cmd, src);
				close(rem);
				rem = rcmd_af(&host, port, pwd->pw_name, suser,
				    bp, 0, AF_INET6);
			if (rem < 0) {
				(void) free(bp);
				continue;
			}
		}

		(void) free(bp);

#ifdef SYSV
		/* In SYSV, saved setuid will allow us to return */
		(void) seteuid(userid);
#else
		(void) setreuid(0, userid);
#endif /* SYSV */
		sink(1, argv + argc - 1);
#ifdef SYSV
		(void) seteuid(0);
#else
		(void) setreuid(userid, 0);
#endif /* SYSV */
		(void) close(rem);
		rem = -1;
	}
}


verifydir(cp)
	char *cp;
{
	struct stat stb;

	if (stat(cp, &stb) >= 0) {
		if ((stb.st_mode & S_IFMT) == S_IFDIR)
			return;
		errno = ENOTDIR;
	}
	error("rcp: %s: %s.\n", cp, strerror(errno));
	exit(1);
}

char *
colon(cp)
	register char *cp;
{
	boolean_t is_bracket_open = B_FALSE;

	for (; *cp; ++cp) {
		if (*cp == '[')
			is_bracket_open = B_TRUE;
		else if (*cp == ']')
			is_bracket_open = B_FALSE;
		else if (*cp == ':' && !is_bracket_open)
			return (cp);
		else if (*cp == '/')
			return (0);
	}
	return (0);
}

okname(cp0)
	char *cp0;
{
	register char *cp = cp0;
	register int c;

	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			goto bad;
	} while (*++cp);
	return (1);
bad:
	(void) fprintf(stderr, "rcp: invalid user name %s\n", cp0);
	return (0);
}


char *
removebrackets(str)
	char *str;
{
	char *newstr = str;

	if ((str[0] == '[') && (str[strlen(str) - 1] == ']')) {
		newstr = str + 1;
		str[strlen(str) - 1] = '\0';
	}
	return (newstr);
}

susystem(s)
	char *s;
{
	int status, pid, w;
	register void (*istat)(), (*qstat)();

	if ((pid = vfork()) == 0) {
		(void) setuid(userid);
		execl(_PATH_BSHELL, "sh", "-c", s, (char *)0);
		_exit(127);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	while ((w = wait(&status)) != pid && w != -1)
		;
	if (w == -1)
		status = -1;
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	return (status);
}

source(argc, argv)
	int argc;
	char **argv;
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i, amt;
	int x, readerr, f;
	char *last, *name, buf[BUFSIZ];
	BUF *allocbuf();

	for (x = 0; x < argc; x++) {
		name = argv[x];
		if ((f = open(name, O_RDONLY, 0)) < 0) {
			error("rcp: %s: %s\n", name, strerror(errno));
			continue;
		}
		if (fstat(f, &stb) < 0)
			goto notreg;
		switch (stb.st_mode&S_IFMT) {

		case S_IFREG:
			break;

		case S_IFDIR:
			if (iamrecursive) {
				(void) close(f);
				rsource(name, &stb);
				continue;
			}
			/* FALLTHROUGH */
		default:
notreg:			(void)close(f);
			error("rcp: %s: not a plain file\n", name);
			continue;
		}
		last = rindex(name, '/');
		if (last == 0)
			last = name;
		else
			last++;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void) sprintf(buf, "T%ld 0 %ld 0\n", stb.st_mtime,
			    stb.st_atime);
			(void) write(rem, buf, strlen(buf));
			if (response() < 0) {
				(void) close(f);
				continue;
			}
		}
		(void) sprintf(buf, "C%04o %lld %s\n", stb.st_mode & 07777,
		    (longlong_t)stb.st_size, last);
		(void) write(rem, buf, strlen(buf));
		if (response() < 0) {
			(void) close(f);
			continue;
		}

		/* ACL support: send */
		if (aclflag) {
			/* get acl from f and send it over */
			if (sendacl(f) == ACL_FAIL) {
				(void) close(f);
				continue;
			}
		}


		if ((bp = allocbuf(&buffer, f, BUFSIZ)) == 0) {
			(void) close(f);
			continue;
		}
		readerr = 0;
		(void) setsockopt(rem, SOL_SOCKET, SO_SNDBUF,
		    (char *)&socksize, sizeof (socksize));
		amt = (off_t)bp->cnt;
		for (i = 0; i < stb.st_size; i += bp->cnt) {
			if (i > stb.st_size - amt)
				amt = stb.st_size - i;
			if (readerr == 0 && read(f, bp->buf, amt) != amt)
				readerr = errno;
			(void) write(rem, bp->buf, amt);
		}
		(void) close(f);
		if (readerr == 0)
			(void) write(rem, "", 1);
		else
			error("rcp: %s: %s\n", name, strerror(readerr));
		(void) response();
	}
}

rsource(name, statp)
	char *name;
	struct stat *statp;
{
	DIR *d;
	struct dirent *dp;
	char *last, *vect[1], path[MAXPATHLEN];

	if (!(d = opendir(name))) {
		error("rcp: %s: %s\n", name, strerror(errno));
		return;
	}
	last = rindex(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void) sprintf(path, "T%ld 0 %ld 0\n", statp->st_mtime,
		    statp->st_atime);
		(void) write(rem, path, strlen(path));
		if (response() < 0) {
			closedir(d);
			return;
		}
	}
	(void) sprintf(path, "D%04o %d %s\n", statp->st_mode&07777, 0, last);
	(void) write(rem, path, strlen(path));

	/* acl support for directory */
	if (aclflag) {
		/* get acl from f and send it over */
		if (sendacl(d->dd_fd) == ACL_FAIL) {
			(void) closedir(d);
			return;
		}
	}

	if (response() < 0) {
		closedir(d);
		return;
	}

	while (dp = readdir(d)) {
		if (dp->d_ino == 0)
			continue;
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;
		if ((uint_t)strlen(name) + 1 + strlen(dp->d_name) >=
		    MAXPATHLEN - 1) {
			error("%s/%s: name too long.\n", name, dp->d_name);
			continue;
		}
		(void) sprintf(path, "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	closedir(d);
	(void) write(rem, "E\n", 2);
	(void) response();
}

response()
{
	register char *cp;
	char ch, resp, rbuf[BUFSIZ];

	if (read(rem, &resp, sizeof (resp)) != sizeof (resp))
		lostconn();

	cp = rbuf;
	switch (resp) {
	case 0:				/* ok */
		return (0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:				/* error, followed by err msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &ch, sizeof (ch)) != sizeof (ch))
				lostconn();
			*cp++ = ch;
		} while (cp < &rbuf[BUFSIZ] && ch != '\n');

		if (!iamremote)
			(void) write(2, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/*NOTREACHED*/
}

void
lostconn()
{
	if (!iamremote)
		(void) fprintf(stderr, "rcp: lost connection\n");
	exit(1);
}

sink(argc, argv)
	int argc;
	char **argv;
{
	register char *cp;
	static BUF buffer;
	struct stat stb;
	struct timeval tv[2];
	BUF *bp, *allocbuf();
	off_t i, j;
	char ch, *targ, *why;
	int amt, count, exists, first, mask, mode;
	off_t size;
	int ofd, setimes, targisdir, wrerr;
	char *np, *vect[1], buf[BUFSIZ];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void) umask(mask);
	if (argc != 1) {
		error("rcp: ambiguous target\n");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	(void) write(rem, "", 1);

	if (stat(targ, &stb) == 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
		targisdir = 1;
	for (first = 1; ; first = 0) {
		cp = buf;
		if (read(rem, cp, 1) <= 0)
			return;

		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (read(rem, &ch, sizeof (ch)) != sizeof (ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void) write(2, buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			errs++;
			continue;
		}
		if (buf[0] == 'E') {
			(void) write(rem, "", 1);
			return;
		}

		if (ch == '\n')
			*--cp = 0;

#define	getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			(void) write(rem, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				error("%s\n", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");
		size = 0;
		while (isdigit(*cp))
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			int need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (!(namebuf = malloc((uint_t)need)))
					error("out of memory\n");
			}
			(void) sprintf(namebuf, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		}
		else
			np = targ;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			if (exists) {
				if ((stb.st_mode&S_IFMT) != S_IFDIR) {
					if (aclflag) {
						/*
						 * consume acl in the pipe
						 * fd = -1 to indicate the
						 * special case
						 */
						if (recvacl(-1, exists, pflag)
						    == ACL_FAIL) {
							goto bad;
						}
					}
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void) chmod(np, mode);
			} else if (mkdir(np, mode) < 0)
				goto bad;

			/* acl support for directories */
			if (aclflag) {
				int dfd;

				if ((dfd = open(np, O_RDONLY)) == -1)
					goto bad;

				/* get acl and set it to ofd */
				if (recvacl(dfd, exists, pflag) == ACL_FAIL) {
					(void) close(dfd);
					goto bad;
				}
				(void) close(dfd);
			}

			vect[0] = np;
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(np, tv) < 0)
				    error("rcp: can't set times on %s: %s\n",
					np, strerror(errno));
			}
			continue;
		}

		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			error("rcp: %s: %s\n", np, strerror(errno));
			continue;
		}

		/*
		 * If the output file exists we have to force zflag off
		 * to avoid erroneously seeking past old data.
		 */
		zopen(ofd, zflag && !exists);

		if (exists && pflag)
			(void) fchmod(ofd, mode);

		(void) setsockopt(rem, SOL_SOCKET, SO_RCVBUF,
		    (char *)&socksize, sizeof (socksize));
		(void) write(rem, "", 1);

		/*
		 * ACL support: receiving
		 */
		if (aclflag) {
			/* get acl and set it to ofd */
			if (recvacl(ofd, exists, pflag) == ACL_FAIL) {
				(void) close(ofd);
				continue;
			}
		}


		if ((bp = allocbuf(&buffer, ofd, BUFSIZ)) == 0) {
			(void) close(ofd);
			continue;
		}
		cp = bp->buf;
		count = 0;
		wrerr = 0;
		for (i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(rem, cp, amt);
				if (j <= 0) {
					int sverrno = errno;

					/*
					 * Connection to supplier lost.
					 * Truncate file to correspond
					 * to amount already transferred.
					 *
					 * Note that we must call ftruncate()
					 * before any call to error() (which
					 * might result in a SIGPIPE and
					 * sudden death before we have a chance
					 * to correct the file's size).
					 */
					size = lseek(ofd, 0, SEEK_CUR);
					if ((ftruncate(ofd, size)  == -1) &&
					    (errno != EINVAL) &&
					    (errno != EACCES))
#define		TRUNCERR	"rcp: can't truncate %s: %s\n"
						error(TRUNCERR, np,
						    strerror(errno));
					error("rcp: %s\n",
					    j ? strerror(sverrno) :
					    "dropped connection");
					(void) close(ofd);
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				cp = bp->buf;
				if (wrerr == 0 &&
				    zwrite(ofd, cp, count) < 0)
					wrerr++;
				count = 0;
			}
		}
		if (count != 0 && wrerr == 0 &&
		    zwrite(ofd, bp->buf, count) < 0)
			wrerr++;
		if (zclose(ofd) < 0)
			wrerr++;


		if ((ftruncate(ofd, size)  == -1) && (errno != EINVAL) &&
		    (errno != EACCES))
			error(TRUNCERR, np, strerror(errno));
		(void) close(ofd);
		(void) response();
		if (setimes) {
			setimes = 0;
			if (utimes(np, tv) < 0)
				error("rcp: can't set times on %s: %s\n",
				    np, strerror(errno));
		}
		if (wrerr)
			error("rcp: %s: %s\n", np, strerror(errno));
		else
			(void) write(rem, "", 1);
	}
screwup:
	error("rcp: protocol screwup: %s\n", why);
	exit(1);
}

#ifndef roundup
#define	roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif /* !roundup */

BUF *
allocbuf(bp, fd, blksize)
	BUF *bp;
	int fd, blksize;
{
	struct stat stb;
	int size;

	if (fstat(fd, &stb) < 0) {
		error("rcp: fstat: %s\n", strerror(errno));
		return (0);
	}
	size = roundup(stb.st_blksize, blksize);
	if (size == 0)
		size = blksize;
	if (bp->cnt < size) {
		if (bp->buf != 0)
			free(bp->buf);
		bp->buf = (char *)malloc((uint_t)size);
		if (!bp->buf) {
			error("rcp: malloc: out of memory\n");
			return (0);
		}
	}
	bp->cnt = size;
	return (bp);
}

/* VARARGS1 */
error(fmt, a1, a2, a3)
	char *fmt;
	int a1, a2, a3;
{
	static FILE *fp;

	++errs;
	if (!fp && !(fp = fdopen(rem, "w")))
		return;
	(void) fprintf(fp, "%c", 0x01);
	(void) fprintf(fp, fmt, a1, a2, a3);
	(void) fflush(fp);
	if (!iamremote)
		(void) fprintf(stderr, fmt, a1, a2, a3);
}

nospace()
{
	(void) fprintf(stderr, "rcp: out of memory.\n");
	exit(1);
}


usage()
{
	(void) fprintf(stderr,
	    "usage: rcp [-p] f1 f2; or: rcp [-rp] f1 ... fn directory\n");
	exit(1);
}


/*
 * sparse file support
 */

static off_t zbsize;
static off_t zlastseek;

/* is it ok to try to create holes? */
zopen(fd, flag)
	int fd;
{
	struct stat st;

	zbsize = 0;
	zlastseek = 0;

	if (flag &&
		fstat(fd, &st) == 0 &&
		(st.st_mode & S_IFMT) == S_IFREG)
		zbsize = st.st_blksize;
}

/* write and/or seek */
zwrite(fd, buf, nbytes)
	int fd;
	register char *buf;
	register int nbytes;
{
	off_t block = zbsize ? zbsize : nbytes;

	do {
		if (block > nbytes)
			block = nbytes;
		nbytes -= block;

		if (!zbsize || notzero(buf, block)) {
			register int n, count = block;
			extern int errno;

			do {
				if ((n = write(fd, buf, count)) < 0)
					return (-1);
				buf += n;
			} while ((count -= n) > 0);
			zlastseek = 0;
		} else {
			if (lseek(fd, (off_t)block, SEEK_CUR) < 0)
				return (-1);
			buf += block;
			zlastseek = 1;
		}
	} while (nbytes > 0);

	return (0);
}

/* write last byte of file if necessary */
zclose(fd)
	int fd;
{
	zbsize = 0;

	if (zlastseek && (lseek(fd, (off_t)-1, SEEK_CUR) < 0 ||
		zwrite(fd, "", 1) < 0))
		return (-1);
	else
		return (0);
}

/* return true if buffer is not all zeros */
notzero(p, n)
	register char *p;
	register int n;
{
	register int result = 0;

	while ((int)p & 3 && --n >= 0)
		result |= *p++;

	while ((n -= 4 * sizeof (int)) >= 0) {
		result |= ((int *)p)[0];
		result |= ((int *)p)[1];
		result |= ((int *)p)[2];
		result |= ((int *)p)[3];
		if (result)
			return (result);
		p += 4 * sizeof (int);
	}
	n += 4 * sizeof (int);

	while (--n >= 0)
		result |= *p++;

	return (result);
}

/*
 * New functions to support ACLs
 */

/*
 * Get acl from f and send it over.
 * ACL record includes acl entry count, acl text length, and acl text.
 */
static int
sendacl(int f)
{
	int		rc;
	int		aclcnt;
	aclent_t	*aclbufp;
	int		aclsize;
	char		*acltext;
	char		buf[BUFSIZ];

	if ((aclcnt = facl(f, GETACLCNT, 0, NULL)) < 0) {
		error("can't get acl count \n");
		return (ACL_FAIL);
	}

	/* send the acl count over */
	(void) sprintf(buf, "A%ld\n", aclcnt);
	(void) write(rem, buf, strlen(buf));

	/* only send acl when it is non-trivial */
	if (aclcnt > MIN_ACL_ENTRIES) {
		aclsize = aclcnt * sizeof (aclent_t);
		if ((aclbufp = (aclent_t *)malloc(aclsize)) == NULL) {
			error("rcp: cant allocate memory: aclcnt %d\n", aclcnt);
			exit(1);
		}
		if ((rc = facl(f, GETACL, aclcnt, aclbufp)) < 0) {
			error("rcp: failed to get acl\n");
			return (ACL_FAIL);
		}
		acltext = acltotext(aclbufp, aclcnt);
		if (acltext == NULL) {
			error("rcp: failed to convert to text\n");
			return (ACL_FAIL);
		}

		/* send ACLs over: send the length first */
		(void) sprintf(buf, "A%ld\n", strlen(acltext));
		(void) write(rem, buf, strlen(buf));
		(void) write(rem, acltext, strlen(acltext));
		free(acltext);
		free(aclbufp);
		if (response() < 0)
			return (ACL_FAIL);

	}
	return (ACL_OK);
}

/*
 * Use this routine to get acl entry count and acl text size (in bytes)
 */
static int
getaclinfo(int *cnt)
{
	char		buf[BUFSIZ];
	char		*cp;
	char		ch;

	/* get acl count */
	cp = buf;
	if (read(rem, cp, 1) <= 0)
		return (ACL_FAIL);
	if (*cp++ != 'A') {
		error("rcp: expect an ACL record, but got %c\n", *cp);
		return (ACL_FAIL);
	}
	do {
		if (read(rem, &ch, sizeof (ch)) != sizeof (ch)) {
			error("rcp: lost connection ..\n");
			return (ACL_FAIL);
		}
		*cp++ = ch;
	} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
	if (ch != '\n') {
		error("rcp: ACL record corrupted \n");
		return (ACL_FAIL);
	}
	cp = &buf[1];
	getnum(*cnt);
	if (*cp != '\n') {
		error("rcp: ACL record corrupted \n");
		return (ACL_FAIL);
	}
	return (ACL_OK);
}


/*
 * Receive acl from the pipe and set it to f
 */
static int
recvacl(int f, int exists, int preserve)
{
	int		aclcnt;		/* acl entry count */
	int		aclsize;	/* acl text length */
	int		rc;		/* return code */
	int		j;
	char		*tp;
	char		*acltext;	/* external format */
	aclent_t	*aclbufp;	/* internal format */

	/* get acl count */
	if (getaclinfo(&aclcnt) != ACL_OK)
		return (ACL_FAIL);

	if (aclcnt > MIN_ACL_ENTRIES) {
		/* get acl text size */
		if (getaclinfo(&aclsize) != ACL_OK)
			return (ACL_FAIL);
		if ((acltext = malloc(aclsize + 1)) == NULL) {
			error("rcp: cant allocate memory: %d\n", aclsize);
			return (ACL_FAIL);
		}

		tp = acltext;
		do {
			j = read(rem, tp, aclsize);
			if (j <= 0) {
				error("rcp: %s\n", j ? strerror(errno) :
				    "dropped connection");
				exit(1);
			}
			aclsize -= j;
			tp += j;
		} while (aclsize > 0);

		if (preserve || !exists) {
			aclbufp = aclfromtext(acltext, &aclcnt);
			if (f != -1) {
				if ((rc =
				    facl(f, SETACL, aclcnt, aclbufp)) < 0) {
					error("rcp: failed to set acl\n");
					return (ACL_FAIL);
				}
			}
			/* -1 means that just consume the data in the pipe */
			free(aclbufp);
		}
		free(acltext);
		(void) write(rem, "", 1);
	}
	return (ACL_OK);
}


static char *
search_char(cp, chr)
unsigned char	*cp;
unsigned char	chr;
{
	int	len;

	while (*cp) {
		if (*cp == chr)
			return ((char *)cp);
		if ((len = mblen((char *)cp, MB_CUR_MAX)) <= 0)
			len = 1;
		cp += len;
	}
	return (0);
}
