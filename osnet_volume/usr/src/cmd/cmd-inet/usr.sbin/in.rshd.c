/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)in.rshd.c	1.37	99/10/25 SMI"	/* SVr4.0 1.8 */

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1996,1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * 	Copyright (c) 1983-1989  AT&T.
 *	All rights reserved.
 *
 */


#define	_FILE_OFFSET_BITS 64

/*
 * remote shell server:
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <netdb.h>
#include <syslog.h>

#ifdef SYSV
#include <sys/resource.h>
#include <sys/filio.h>
#include <shadow.h>
#include <stdlib.h>

#include <security/pam_appl.h>

#define	killpg(a, b)	kill(-(a), (b))
#define	rindex strrchr
#define	index strchr
#endif	/* SYSV */

#ifndef NCARGS
#define	NCARGS	5120
#endif /* NCARGS */

static void error(char *, ...);
static void doit(int, struct sockaddr_storage *);
static void getstr(char *, int, char *);

static int legalenvvar(char *);

/* Function decls. for functions not in any header file.  (Grrrr.) */
extern int audit_rshd_setup(void);
extern int audit_rshd_success(char *, char *, char *, char *);
extern int audit_rshd_fail(char *, char *, char *, char *, char *);
extern int audit_settid(int);

static pam_handle_t *pamh;
static int retval;

/*ARGSUSED*/
void
main(argc, argv)
	int argc;
	char **argv;
{
	struct linger linger;
	int on = 1, fromlen;
	struct sockaddr_storage from;

	openlog("rsh", LOG_PID | LOG_ODELAY, LOG_DAEMON);
	(void) audit_rshd_setup();	/* BSM */
	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, (socklen_t *)&fromlen)
	    < 0) {
		(void) fprintf(stderr, "%s: ", argv[0]);
		perror("getpeername");
		_exit(1);
	}

	if (audit_settid(0) != 0) {
		perror("settid");
		exit(1);
	}

	if (setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof (on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(0, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	doit(dup(0), &from);
	/* NOTREACHED */
}

static char	username[20] = "USER=";
static char	homedir[64] = "HOME=";
static char	shell[64] = "SHELL=";

#ifdef SYSV
static char	*envinit[] =
		{homedir, shell, (char *)0, username, (char *)0,
		(char *)0, (char *)0, (char *)0, (char *)0,
		(char *)0, (char *)0, (char *)0, (char *)0,
		(char *)0, (char *)0, (char *)0, (char *)0,
		(char *)0, (char *)0, (char *)0, (char *)0,
		(char *)0};
#define	ENVINIT_PATH	2	/* position of PATH in envinit[] */
#define	ENVINIT_TZ	4	/* position of TZ in envinit[] */
#define	PAM_ENV_ELIM	16	/* allow 16 PAM environment variables */
#define	USERNAME_LEN	16	/* maximum number of characters in user name */

/*
 *	See PSARC opinion 1992/025
 */
static char	userpath[] = "PATH=/usr/bin:";
static char	rootpath[] = "PATH=/usr/sbin:/usr/bin";
#else
static char	*envinit[] =
	    {homedir, shell, "PATH=:/usr/ucb:/bin:/usr/bin", username, 0};
#endif /* SYSV */

static char cmdbuf[NCARGS+1];
static char hostname [MAXHOSTNAMELEN + 1];

static void
doit(f, fromp)
	int f;
	struct sockaddr_storage *fromp;
{
	char *cp;
	char locuser[USERNAME_LEN + 1], remuser[USERNAME_LEN + 1];

	struct passwd *pwd;
#ifdef SYSV
	char *tz, *tzenv;
	struct spwd *shpwd;
	struct stat statb;
#endif /* SYSV */

	int s;
	struct in_addr ipv4addr;
	short port;
	pid_t pid;
	int pv[2], cc;
	char buf[BUFSIZ], sig;
	int one = 1;
	int v = 0;
	int err = 0;
	int idx = 0, end_env = 0;
	char **pam_env;
	char abuf[INET6_ADDRSTRLEN];
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int fromplen;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef SYSV
	(void) sigset(SIGCHLD, SIG_IGN);
#endif /* SYSV */
#ifdef DEBUG
	{ int t = open("/dev/tty", 2);
	    if (t >= 0) {
#ifdef SYSV
		(void) setsid();
#else
		(void) ioctl(t, TIOCNOTTY, (char *)0);
#endif SYSV
		(void) close(t);
	    }
	}
#endif
	if (fromp->ss_family == AF_INET) {
		sin = (struct sockaddr_in *)fromp;
		port = ntohs((ushort_t)sin->sin_port);
		fromplen = sizeof (struct sockaddr_in);
	} else if (fromp->ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)fromp;
		port = ntohs((ushort_t)sin6->sin6_port);
		fromplen = sizeof (struct sockaddr_in6);
	} else {
		syslog(LOG_ERR, "wrong address family\n");
		exit(1);
	}
	if (port >= IPPORT_RESERVED || port < (uint_t)(IPPORT_RESERVED/2)) {
		syslog(LOG_NOTICE, "connection from bad port\n");
		exit(1);
	}
	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if ((cc = read(f, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			(void) shutdown(f, 1+1);
			exit(1);
		}
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}
	(void) alarm(0);
	if (port != 0) {
		int lport;

getport:
		/*
		 * 0 means that rresvport_af() will bind to a port in
		 * the anonymous priviledged port range.
		 */
		lport = 0;
		s = rresvport_af(&lport, fromp->ss_family);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
		if (port >= IPPORT_RESERVED) {
			syslog(LOG_ERR, "2nd port not reserved\n");
			exit(1);
		}
		if (fromp->ss_family == AF_INET) {
			sin->sin_port = htons((ushort_t)port);
		} else if (fromp->ss_family == AF_INET6) {
			sin6->sin6_port = htons((ushort_t)port);
		}
		if (connect(s, (struct sockaddr *)fromp, fromplen) < 0) {
			if (errno == EADDRINUSE) {
				close(s);
				goto getport;
			}
			syslog(LOG_INFO, "connect second port: %m");
			exit(1);
		}
	}
	(void) dup2(f, 0);
	(void) dup2(f, 1);
	(void) dup2(f, 2);

	if (getnameinfo((const struct sockaddr *) fromp, fromplen, hostname,
	    sizeof (hostname), NULL, 0, 0) != 0) {
		if (fromp->ss_family == AF_INET6) {
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				struct in_addr ipv4_addr;

				IN6_V4MAPPED_TO_INADDR(&sin6->sin6_addr,
				    &ipv4_addr);
				inet_ntop(AF_INET, &ipv4_addr, abuf,
				    sizeof (abuf));
			} else {
				inet_ntop(AF_INET6, &sin6->sin6_addr,
				    abuf, sizeof (abuf));
			}
		} else if (fromp->ss_family == AF_INET) {
				inet_ntop(AF_INET, &sin->sin_addr,
				    abuf, sizeof (abuf));
			}
		(void) strncpy(hostname, abuf, sizeof (hostname));
	}
	getstr(remuser, sizeof (remuser), "remuser");
	getstr(locuser, sizeof (locuser), "locuser");
	getstr(cmdbuf, sizeof (cmdbuf), "command");

	/*
	 * Note that there is no rsh conv functions at present.
	 */
	if ((err = pam_start("rsh", locuser, NULL, &pamh)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_start() failed: %s\n",
			pam_strerror(0, err));
		exit(1);
	}
	if ((err = pam_set_item(pamh, PAM_RHOST, hostname)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item() failed: %s\n",
			pam_strerror(pamh, err));
		exit(1);
	}
	if ((err = pam_set_item(pamh, PAM_RUSER, remuser)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item() failed: %s\n",
			pam_strerror(pamh, err));
		exit(1);
	}

	pwd = getpwnam(locuser);
	shpwd = getspnam(locuser);
	if ((pwd == NULL) || (shpwd == NULL)) {
		error("permission denied.\n");
		(void) audit_rshd_fail("Login incorrect", hostname,
			remuser, locuser, cmdbuf);	/* BSM */
		exit(1);
	}

	/*
	 * maintain 2.1 and 4.* and BSD semantics with anonymous rshd
	 */
	if (shpwd->sp_pwdp != 0 && *shpwd->sp_pwdp != '\0' &&
	    (v = pam_authenticate(pamh, 0)) != PAM_SUCCESS) {
		error("permission denied\n");
		(void) audit_rshd_fail("Permission denied", hostname,
			remuser, locuser, cmdbuf);	/* BSM */
		pam_end(pamh, v);
		exit(1);
	}

	if ((v = pam_acct_mgmt(pamh, 0)) != PAM_SUCCESS) {
		switch (v) {
		case PAM_NEW_AUTHTOK_REQD:
			error("password expired\n");
			(void) audit_rshd_fail("Password expired", hostname,
				remuser, locuser, cmdbuf); /* BSM */
			break;
		case PAM_PERM_DENIED:
			error("account expired\n");
			(void) audit_rshd_fail("Account expired", hostname,
				remuser, locuser, cmdbuf); /* BSM */
			break;
		case PAM_AUTHTOK_EXPIRED:
			error("password expired\n");
			(void) audit_rshd_fail("Password expired", hostname,
				remuser, locuser, cmdbuf); /* BSM */
			break;
		default:
			error("login incorrect\n");
			(void) audit_rshd_fail("Permission denied", hostname,
				remuser, locuser, cmdbuf); /* BSM */
			break;
		}
		pam_end(pamh, PAM_ABORT);
		exit(1);
	}

	if (chdir(pwd->pw_dir) < 0) {
		(void) chdir("/");
#ifdef notdef
		error("No remote directory.\n");

		exit(1);
#endif
	}

	/*
	 * XXX There is no session management currently being done
	 */

	(void) write(2, "\0", 1);
	if (port) {
		if (pipe(pv) < 0) {
			error("Can't make pipe.\n");
			pam_end(pamh, PAM_ABORT);
			exit(1);
		}
		pid = fork();
		if (pid == (pid_t)-1)  {
error("Fork (to start shell) failed on server.  Please try again later.\n");
			pam_end(pamh, PAM_ABORT);
			exit(1);
		}

#ifndef MAX
#define	MAX(a, b) (((uint_t)(a) > (uint_t)(b)) ? (a) : (b))
#endif /* MAX */

		if (pid) {
			int width = MAX(s, pv[0]) + 1;
			fd_set ready;
			fd_set readfrom;

			(void) close(0); (void) close(1); (void) close(2);
			(void) close(f); (void) close(pv[1]);
			(void) FD_ZERO(&ready);
			(void) FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			if (ioctl(pv[0], FIONBIO, (char *)&one) == -1)
				syslog(LOG_INFO, "ioctl FIONBIO: %m");
			/* should set s nbio! */
			do {
				ready = readfrom;
				if (select(width, &ready, (fd_set *)0,
				    (fd_set *)0, (struct timeval *)0) < 0)
					break;
				if (FD_ISSET(s, &ready)) {
					if (read(s, &sig, 1) <= 0)
						FD_CLR(s, &readfrom);
					else
						(void) killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					errno = 0;
					cc = read(pv[0], buf, sizeof (buf));
					if (cc <= 0) {
						(void) shutdown(s, 1+1);
						FD_CLR(pv[0], &readfrom);
					} else
						(void) write(s, buf, cc);
				}
			} while (FD_ISSET(s, &readfrom) ||
				    FD_ISSET(pv[0], &readfrom));
			exit(0);
		}
		/* setpgrp(0, getpid()); */
		(void) setsid();	/* Should be the same as above. */
		(void) close(s); (void) close(pv[0]);
		(void) dup2(pv[1], 2);
		(void) close(pv[1]);
	}
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = "/bin/sh";
	(void) close(f);

	/*
	 * write audit record before making uid switch
	 */
	(void) audit_rshd_success(hostname, remuser, locuser, cmdbuf); /* BSM */

	/* set the real (and effective) GID */
	if (setgid(pwd->pw_gid) == -1) {
		error("Invalid gid.\n");
		pam_end(pamh, PAM_ABORT);
		exit(1);
	}

	/*
	 * Initialize the supplementary group access list.
	 */
	if (!locuser) {
		error("Initgroup failed.\n");
		pam_end(pamh, PAM_ABORT);
		exit(1);
	}
	if (initgroups(locuser, pwd->pw_gid) == -1) {
		error("Initgroup failed.\n");
		pam_end(pamh, PAM_ABORT);
		exit(1);
	}

	if ((retval = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS) {
		error("Insufficent credentials.\n");
		pam_end(pamh, retval);
		exit(1);
	}

	/* set the real (and effective) UID */
	if (setuid(pwd->pw_uid) == -1) {
		error("Invalid uid.\n");
		pam_end(pamh, PAM_ABORT);
		exit(1);
	}

	/* Change directory only after becoming the appropriate user. */
	if (chdir(pwd->pw_dir) < 0) {
		(void) chdir("/");
#ifdef notdef
		error("No remote directory.\n");
		exit(1);
#endif
	}

#ifdef	SYSV
	if (pwd->pw_uid)
		envinit[ENVINIT_PATH] = userpath;
	else
		envinit[ENVINIT_PATH] = rootpath;
	if (tzenv = getenv("TZ")) {
		/*
		 *	In the line below, 4 is strlen("TZ=") + 1 null byte.
		 *	We have to malloc the space because it's difficult to
		 *	compute the maximum size of a timezone string.
		 */
		tz = (char *)malloc(strlen(tzenv) + 4);
		if (tz) {
			(void) strcpy(tz, "TZ=");
			(void) strcat(tz, tzenv);
			envinit[ENVINIT_TZ] = tz;
		}
	}
#endif	/* SYSV */
	(void) strncat(homedir, pwd->pw_dir, sizeof (homedir)-6);
	(void) strncat(shell, pwd->pw_shell, sizeof (shell)-7);
	(void) strncat(username, pwd->pw_name, sizeof (username)-6);

	/*
	 * add PAM environment variables set by modules
	 * -- only allowed 16 (PAM_ENV_ELIM)
	 * -- check to see if the environment variable is legal
	 */
	for (end_env = 0; envinit[end_env] != 0; end_env++)
		;
	if ((pam_env = pam_getenvlist(pamh)) != 0) {
		while (pam_env[idx] != 0) {
			if (idx < PAM_ENV_ELIM &&
			    legalenvvar(pam_env[idx])) {
				envinit[end_env + idx] = pam_env[idx];
			}
			idx++;
		}
	}

	pam_end(pamh, PAM_SUCCESS);

	cp = rindex(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
#ifdef	SYSV
	/*
	 * rdist has been moved to /usr/bin, so /usr/ucb/rdist might not
	 * be present on a system.  So if it doesn't exist we fall back
	 * and try for it in /usr/bin.  We take care to match the space
	 * after the name because the only purpose of this is to protect
	 * the internal call from old rdist's, not humans who type
	 * "rsh foo /usr/ucb/rdist".
	 */
#define	RDIST_PROG_NAME	"/usr/ucb/rdist -Server"
	if (strncmp(cmdbuf, RDIST_PROG_NAME, strlen(RDIST_PROG_NAME)) == 0) {
		if (stat("/usr/ucb/rdist", &statb) != 0) {
			(void) strncpy(cmdbuf + 5, "bin", 3);
		}
	}
#endif
	(void) execle(pwd->pw_shell, cp, "-c", cmdbuf, (char *)0, envinit);
	perror(pwd->pw_shell);
	exit(1);
}

static void
getstr(buf, cnt, err)
	char *buf;
	int cnt;
	char *err;
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (cnt-- == 0) {
			error("%s too long\n", err);
			exit(1);
		}
	} while (c != 0);
}

/*VARARGS1*/
static void
error(char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	buf[0] = 1;
	va_start(ap, fmt);
	(void) vsprintf(buf+1, fmt, ap);
	va_end(ap);
	(void) write(2, buf, strlen(buf));
}

static char *illegal[] = {
	"SHELL=",
	"HOME=",
	"LOGNAME=",
#ifndef NO_MAIL
	"MAIL=",
#endif
	"CDPATH=",
	"IFS=",
	"PATH=",
	"USER=",
	"TZ=",
	0
};

/*
 * legalenvvar - can PAM modules insert this environmental variable?
 */

static int
legalenvvar(char *s)
{
	register char **p;

	for (p = illegal; *p; p++)
		if (strncmp(s, *p, strlen(*p)) == 0)
			return (0);

	if (s[0] == 'L' && s[1] == 'D' && s[2] == '_')
		return (0);

	return (1);
}
