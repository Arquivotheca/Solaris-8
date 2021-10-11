/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ftpd.c	1.60	99/11/19 SMI"	/* SVr4.0 1.6	*/

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
 * 	Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */


/*
 * FTP server.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include <setjmp.h>
#include <netdb.h>
#include <errno.h>
#include <varargs.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdlib.h>
#include <shadow.h>

#include <string.h>

#include <security/pam_appl.h>
int ftpd_conv();
struct pam_conv conv = {
			ftpd_conv,
			NULL
		    };
pam_handle_t *pamh;	/* Authentication descriptor */

char passbuf[256];	/* save password for pam_conv */


/*
 * File containing login names
 * NOT to be used on this machine.
 * Commonly used to disallow uucp.
 */
#define	FTPUSERS	"/etc/ftpusers"

extern	char *crypt();
char	binshellvar[] = "SHELL=/bin/sh";
char	defaultfile[] = "/etc/default/ftpd";
char	bannervar[] = "BANNER=";
char	*bannerval;
char	umaskvar[] = "UMASK=";
int	umaskval = 022;
extern	char *home;		/* pointer to home directory for glob */
extern	FILE *popen(), *fopen(), *freopen();
extern	FILE *_popen();
extern	int  pclose(), fclose();
extern	char *getline();
extern	char cbuf[];
extern	char *globerr;
char	*getbanner();
char	*getversion();
void	getdefaults();
static const char *inet_ntop_native();

struct	sockaddr_storage ctrl_addr;
struct	sockaddr_in *ctrl_sin;
struct	sockaddr_in6 *ctrl_sin6;

struct	sockaddr_storage data_source;
struct	sockaddr_in *data_sin;
struct	sockaddr_in6 *data_sin6;

struct	sockaddr_storage data_dest;
struct	sockaddr_in *data_dest_sin;
struct	sockaddr_in6 *data_dest_sin6;

struct	sockaddr_storage rem_addr;
struct	sockaddr_in *rem_sin;
struct	sockaddr_in6 *rem_sin6;

int	addrfmly;
int	peerfmly;
int	data;
sigjmp_buf	errcatch, urgcatch;
int	logged_in;
struct	passwd *pw;
int	debug;
int	timeout = 900;    /* timeout after 15 minutes of inactivity */
int	logging;
int	guest;
int	wtmp;
int	type;
int	form;
int	stru;			/* avoid C keyword */
int	mode;
int	usedefault = 1;		/* for data transfers */
int	pdata;			/* for passive mode */
int	unique;
int	transflag;
int	socksize = 24 * 1024;	/* larger socket window size for data */
char	tmpline[7];
char	hostname[MAXHOSTNAMELEN + 1];
char	remotehost[MAXHOSTNAMELEN + 1];
char	real_username[132] = "";
char	buf[BUFSIZ*8];		/* larger buffer to speed up binary xfers */

int	max_failed_logins = 3;	/* # of failed logins before disconnect */
int	failed_logins = 0;	/* Tracks number of failed logins */

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

int	lostconn();
int	myoob();
FILE	*getdatasock();
FILE	*dataconn(char *, off_t, char *mode);

int standalone = 0;


/* Type values for various passive modes supported by server */
#define	TYPE_PASV	0
#define	TYPE_EPSV	1
#define	TYPE_LPSV	2

main(argc, argv)
	int argc;
	char *argv[];
{
	int on = 1;
	socklen_t addrlen;
	pid_t pgid;
	char *cp;

	openlog("in.ftpd", LOG_PID | LOG_ODELAY, LOG_DAEMON);
	debug = 0;
	argc--, argv++;
	while (argc > 0 && *argv[0] == '-') {
		for (cp = &argv[0][1]; *cp; cp++)
		switch (*cp) {

		case 'v':
			debug = 1;
			break;

		case 'd':
			debug = 1;
			break;

		case 'l':
			logging = 1;
			break;

		case 's':
			standalone++;
			break;

		case 't':
			timeout = atoi(++cp);
			goto nextopt;
			break;

		default:
			syslog(LOG_WARNING, "Unknown flag -%c ignored.", *cp);
			break;
		}
nextopt:
		argc--, argv++;
	}


	if (standalone) {
		int s, ns;
		socklen_t foo;
		static struct sockaddr_in6 sin6 = { AF_INET6 };
		int option = 1;
		struct servent	*sp;

		sp = getservbyname("ftp", "tcp");
		if (sp == NULL) {
			fprintf(stderr, "in.ftpd: ftp/tcp: unknown service\n");
			exit(1);
		}

		sin6.sin6_port = sp->s_port;

		s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (s < 0) {
			perror("socket");
			exit(1);
		}

		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&option,
		    sizeof (option)) == -1)
			perror("setsockopt (SO_REUSEADDR)");
		if (bind(s, (struct sockaddr *)&sin6, sizeof (sin6)) < 0) {
			perror("bind");
			exit(1);
		}
		if (listen(s, 32) < 0) {
			perror("listen");
			exit(1);
		}

		/* automatically reap all child processes */
		sigset(SIGCHLD, SIG_IGN);

		for (;;) {
			pid_t pid;

			foo = sizeof (struct sockaddr_in6);
			ns = accept(s, (struct sockaddr *)&sin6, &foo);
			if (ns < 0) {
				perror("accept");
				exit(1);
			}
			pid = fork();
			if (pid == -1) {
				perror("fork");
				exit(1);
			}
			if (pid == 0) {
				if (dup2(ns, 0) == -1)
					perror("dup2");
				if (dup2(ns, 1) == -1)
					perror("dup2");
				sigset(SIGCHLD, SIG_DFL);
				break;
			} else {
				(void) close(ns);
			}
		}
	}

	addrlen = sizeof (rem_addr);
	if (getpeername(0, (struct sockaddr *)&rem_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		exit(1);
	}

	/* start setting up user's audit characteristics */
	if (audit_settid(0)) {
		syslog(LOG_ERR, "audit failure");
		exit(1);
	}

	addrlen = sizeof (ctrl_addr);
	if (getsockname(0, (struct sockaddr  *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname: %m");
		exit(1);
	}
	switch (ctrl_addr.ss_family) {
	case AF_INET:
		addrfmly = AF_INET;
		peerfmly = AF_INET;
		ctrl_sin = (struct sockaddr_in *)&ctrl_addr;
		data_sin = (struct sockaddr_in *)&data_source;
		break;
	case AF_INET6:
		addrfmly = AF_INET6;
		ctrl_sin6 = (struct sockaddr_in6 *)&ctrl_addr;
		if (IN6_IS_ADDR_V4MAPPED(&ctrl_sin6->sin6_addr))
			peerfmly = AF_INET;
		else
			peerfmly = AF_INET6;
		data_sin6 = (struct sockaddr_in6 *)&data_source;
		break;
	default:
		syslog(LOG_ERR, "unknown address family %d\n",
		    ctrl_addr.ss_family);
		exit(1);
	}

	if (setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (const char *)&on,
							sizeof (on)) < 0) {
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	}
	(void) freopen("/dev/null", "w", stderr);
	(void) sigset(SIGPIPE, (void (*)())lostconn);
	(void) sigset(SIGCHLD, SIG_IGN);
	if (sigset(SIGURG, (void (*)())myoob) == (void (*)()) -1) {
		syslog(LOG_ERR, "sigset (SIGURG): %m");
	}
	/* handle urgent data inline */
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on,
	    sizeof (on)) < 0) {
		syslog(LOG_ERR, "setsockopt (SO_OOBINLINE): %m");
	}
	pgid = getpid();
	if (ioctl(fileno(stdin), SIOCSPGRP, (char *)&pgid) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCSPGRP): %m");
	}
	dolog(&rem_addr);
	/* do telnet option negotiation here */
	/*
	 * Set up default state
	 */
	logged_in = 0;
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';
	(void) gethostname(hostname, sizeof (hostname));
	getdefaults();
	(void) umask(umaskval);
	reply(220, "%s FTP server (%s) ready.",
		hostname, getbanner());
	for (;;) {
		(void) sigsetjmp(errcatch, 1);
		(void) yyparse();
	}
}

lostconn()
{

	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(-1);
}

static char ttyline[20];

/*
 * Helper function for sgetpwnam().
 */
char *
sgetsave(s)
	char *s;
{
	char *new = (char *)malloc((unsigned)strlen(s) + 1);

	if (new == NULL) {
		reply(553, "Local resource failure");
		dologout(1);
	}
	(void) strcpy(new, s);
	return (new);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
struct passwd *
sgetpwnam(name)
	char *name;
{
	static struct passwd save;
	register struct passwd *p;
	struct spwd *sp;
	int oldeuid;
	char *sgetsave();

	if ((p = getpwnam(name)) == NULL) {
		return (NULL);
	}
	oldeuid = geteuid();
	if (oldeuid && oldeuid != -1)
		seteuid(0);
	if ((sp = getspnam(name)) == NULL) {
	if (oldeuid && oldeuid != -1)
		seteuid(oldeuid);
		return (NULL);
	}
	if (oldeuid && oldeuid != -1)
		seteuid(oldeuid);
	if (save.pw_name) {
		free(save.pw_name);
		free(save.pw_passwd);
		free(save.pw_comment);
		free(save.pw_gecos);
		free(save.pw_dir);
		free(save.pw_shell);
	}
	save = *p;
	save.pw_name = sgetsave(p->pw_name);
	save.pw_passwd = sgetsave(sp->sp_pwdp);
	save.pw_comment = sgetsave(p->pw_comment);
	save.pw_gecos = sgetsave(p->pw_gecos);
	save.pw_dir = sgetsave(p->pw_dir);
	save.pw_shell = sgetsave(p->pw_shell);
	return (&save);
}


/* Log a failed login attempt */
void
login_failed(struct passwd *pw, char *passwd, char *host)
{
	syslog(LOG_WARNING, "%s%s LOGIN FAILED [from %s]",
	    pw ? pw->pw_name : real_username, pw ? "" : " (bogus)",
	    host ? host : "***");
}


/*
 * Attempt to login failed: bump counter; log and disconnect if it reaches
 * maximum.
 */
static void
bump_failure_counter(struct passwd *pw, char *passwd, char *host)
{
	if (++failed_logins >= max_failed_logins) {
		login_failed(pw, passwd, host);
		exit(1);
	}
}


void
pass(passwd)
	char *passwd;
{
	int	error;
	uid_t	euid;
	int	didopen = 0;

	/*
	 * Instead of signalling an error when we got a non-existent
	 * account on USER, return the canonical failure code now
	 * on PASS.  This prevents name guessing.  real_username[]
	 * is set to the argument of USER if it was anything other
	 * than "ftp" or "anonymous", and pw is NULL if the user
	 * couldn't be found by getpwnam().
	 */
	if (real_username[0] && pw == NULL) {
		reply(530, "Login incorrect.");
		bump_failure_counter(pw, passwd, remotehost);
		return;
	}

	if (logged_in || pw == NULL) {
		reply(503, "Login with USER first.");
		bump_failure_counter(pw, passwd, remotehost);
		return;
	}

	strncpy(passbuf, passwd, sizeof (passbuf));
	passbuf[sizeof (passbuf)-1] = '\0';

	(void) sprintf(ttyline, "ftp%ld", getpid());

	if (pam_start("ftp", pw->pw_name, &conv, &pamh) != PAM_SUCCESS) {
		exit(1);
	}
	if (pam_set_item(pamh, PAM_TTY, ttyline) != PAM_SUCCESS) {
		exit(1);
	}
	if (pam_set_item(pamh, PAM_RHOST, remotehost) != PAM_SUCCESS) {
		exit(1);
	}

	/* need effective uid to be 0 to update things */
	euid = geteuid();
	seteuid(0);

	if (!guest) {		/* "ftp" is only account allowed no password */
		if ((error = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK))
							!= PAM_SUCCESS) {
			reply(530, "Login incorrect.");
			audit_ftpd_bad_pw(pw->pw_name);	/* BSM */
			pw = NULL;
			if (error == PAM_MAXTRIES) {
				login_failed(pw, passwd, remotehost);
				exit(1);
			}
			pam_end(pamh, PAM_ABORT);
			pamh = NULL;
			seteuid(euid);
			return;
		}
		if ((error = pam_acct_mgmt(pamh, 0)) != PAM_SUCCESS) {
			switch (error) {
			case PAM_AUTHTOK_EXPIRED:
			case PAM_NEW_AUTHTOK_REQD:
				reply(530, "Password Expired.");
				break;
			case PAM_PERM_DENIED:
				reply(530, "Account Expired.");
				break;
			default:
				reply(530, "Login incorrect.");
				break;
			}
			audit_ftpd_bad_pw(pw->pw_name);	/* BSM */
			pw = NULL;
			pam_end(pamh, PAM_ABORT);
			pamh = NULL;
			seteuid(euid);
			bump_failure_counter(pw, passwd, remotehost);
			return;
		}
		memset(passbuf, 0, sizeof (passbuf));
	}

	/*
	 * We have to write utmp first because pam_setcred changes our uid
	 * and we can't access /etc/utmp
	 */
	logwtmp(ttyline, pw->pw_name, remotehost);
	/* open session processing */
	if (pam_open_session(pamh, 0) != PAM_SUCCESS) {
		reply(530, "Login incorrect.");
		goto bad;
	}
	didopen = 1;

	/* set the effective GID */
	if ((setegid(pw->pw_gid) == -1) ||
	    !pw->pw_name		||
	    (initgroups(pw->pw_name, pw->pw_gid) == -1)) {
		reply(530, "Login incorrect.");
		goto bad;
	}

	if (guest && chroot(pw->pw_dir) < 0) {
		reply(550, "Can't set guest privileges.");
		goto bad;
	}
	if (!guest) {
		if (pam_setcred(pamh, PAM_ESTABLISH_CRED) != PAM_SUCCESS) {
			reply(530, "Login incorrect.");
			goto bad;
		}
	}

	/* only set effective UID here */
	if (seteuid(pw->pw_uid) == -1) {
		reply(530, "Login incorrect.");
		goto bad;
	}

	pam_end(pamh, PAM_SUCCESS);
	pamh = NULL;

	if (guest || chdir(pw->pw_dir)) {
		if (chdir("/")) {
			reply(550, "User %s: can't change directory to %s.",
				pw->pw_name, pw->pw_dir);
			goto bad;
		}
	}
	if (!guest)
		reply(230, "User %s logged in.", pw->pw_name);
	else
		reply(230, "Guest login ok, access restrictions apply.");
	logged_in = 1;

	home = pw->pw_dir;		/* home dir for globbing */
	audit_ftpd_success(pw->pw_name);	/* BSM */
	return;
bad:
	seteuid(0);
	audit_ftpd_failure(pw->pw_name);
	bump_failure_counter(pw, passwd, remotehost);
	/*
	 * We have to remove the utmp entry we made if a failure occured since
	 * with PAM we make an entry before all the error checking has
	 * occurred.
	 */
	logwtmp(ttyline, "", "");
	if (didopen && pam_close_session(pamh, 0) != PAM_SUCCESS)
		exit(1);
	/* pamh will always be set if we get here */
	pam_end(pamh, PAM_ABORT);
	pamh = NULL;
	pw = NULL;
}

/*
 * return a printable type string
 */
char *
print_type(t)
{
	switch (t) {
	    case TYPE_A:	return ("ASCII ");
	    case TYPE_L:
	    case TYPE_I:	return ("Binary ");
	}
	return ("");
}

static char line[BUFSIZ];

retrieve(cmd, name)
	char *cmd, *name;
{
	FILE *fin, *dout;
	struct stat st;
	int (*closefunc)(), tmp;
	void (*oldpipe)();	/* Hold value of SIGPIPE during close */
	int on = 1;

	if (cmd == 0) {
		fin = fopen(name, "r"), closefunc = fclose;
	} else {

		(void) sprintf(line, cmd, name), name = line;
		fin = popen(line, "r"), closefunc = pclose;
	}
	if (fin == NULL) {
		if (errno != 0) {
			reply(550, "%s: %s.", name, strerror(errno));
		} else if (globerr) {
			reply(550, "%s: %s.", name, globerr);
		} else {
			reply(550, "%s: %s.", name, "Unspecified error");
		}
		return;
	}
	st.st_size = 0;
	if (cmd == 0 &&
	    (stat(name, &st) < 0 || (st.st_mode&S_IFMT) != S_IFREG)) {
		reply(550, "%s: not a plain file.", name);
		goto done;
	}
	dout = dataconn(name, st.st_size, "w");
	if (dout == NULL)
		goto done;
	/*
	 * We set SO_KEEPALIVE for data connections
	 * Keepalive strictly is not necessary when we are doing
	 * the sending but it is harmless insurance against broken
	 * TCP/IP stacks.
	 */
	if (setsockopt(fileno(dout), SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof (on)) < 0) {
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	}
	/* turn off timer during data xfer */
	(void) alarm(0);
	if ((tmp = send_data(fin, dout)) > 0 || ((int)ferror(dout)) > 0) {
		reply(550, "%s: %s.", name, strerror(errno));
	} else if (tmp == 0) {
		reply(226, "%sTransfer complete.", print_type(type));
	}
	/* restart timer now that data xfer has completed */
	(void) alarm((unsigned)timeout);
	/*
	 * If the transfer failed because the data connection got aborted,
	 * then the fclose may cause a SIGPIPE trying to flush the buffers
	 * and abort the whole session.  Ignore SIGPIPEs during the fclose.
	 */
	oldpipe = sigset(SIGPIPE, SIG_IGN);
	(void) fclose(dout);
	data = -1;
	pdata = -1;
	sigset(SIGPIPE, oldpipe);
done:
	(*closefunc)(fin);
}

store(name, mode)
	char *name, *mode;
{
	FILE *fout, *din;
	int (*closefunc)(), dochown = 0, tmp;
	char *gunique(), *local;
	int on = 1;

	{
		struct stat st;

		local = name;
		if (stat(name, &st) < 0) {
			dochown++;
		} else if (unique) {
			if ((local = gunique(name)) == NULL) {
				return;
			}
			dochown++;
		}
		fout = fopen(local, mode), closefunc = fclose;
	}
	if (fout == NULL) {
		reply(553, "%s: %s.", local, strerror(errno));
		return;
	}
	din = dataconn(local, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	/*
	 * We set SO_KEEPALIVE for data connections
	 * This keepalive is needed if remote side goes away
	 * without telling us. Otherwise TCP will wait forever for more data.
	 */
	if (setsockopt(fileno(din), SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof (on)) < 0) {
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	}
	/* turn off timer during data xfer */
	(void) alarm(0);
	if ((tmp = receive_data(din, fout)) > 0 || ((int)ferror(fout)) > 0) {
		reply(552, "%s: %s.", local, strerror(errno));
	} else if (tmp == 0 && !unique) {
		reply(226, "Transfer complete.");
	} else if (tmp == 0 && unique) {
		reply(226, "Transfer complete (unique file name:%s).", local);
	}
	/* restart timer now that data xfer has completed */
	(void) alarm((unsigned)timeout);
	(void) fclose(din);
	data = -1;
	pdata = -1;
done:
	if (dochown)
		(void) chown(local, pw->pw_uid, (gid_t)-1);

	(*closefunc)(fout);
}

FILE *
getdatasock(mode)
	char *mode;
{
	int s, on = 1;
	int tries;
	int data_source_len;

	if (data >= 0)
		return (fdopen(data, mode));
	seteuid(0);
	s = socket(addrfmly, SOCK_STREAM, 0);
	if (s < 0) {
		seteuid(pw->pw_uid);
		return (NULL);
	}
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
						sizeof (on)) < 0)
		goto bad;
	/* anchor socket to avoid multi-homing problems */
	switch (addrfmly) {
	case AF_INET:
		ctrl_sin = (struct sockaddr_in *)&ctrl_addr;
		data_sin = (struct sockaddr_in *)&data_source;
		bzero(data_sin, sizeof (struct sockaddr_in));
		data_sin->sin_family = AF_INET;
		data_sin->sin_addr = ctrl_sin->sin_addr;
		data_sin->sin_port = htons(ntohs(ctrl_sin->sin_port) - 1);
		data_source_len = sizeof (struct sockaddr_in);
		break;
	case AF_INET6:
		ctrl_sin6 = (struct sockaddr_in6 *)&ctrl_addr;
		data_sin6 = (struct sockaddr_in6 *)&data_source;
		bzero(data_sin6, sizeof (struct sockaddr_in6));
		data_sin6->sin6_family = AF_INET6;
		data_sin6->sin6_addr = ctrl_sin6->sin6_addr;
		data_sin6->sin6_port = htons(ntohs(ctrl_sin6->sin6_port) - 1);
		data_source_len = sizeof (struct sockaddr_in6);
		break;
	}

#define	MAX_BIND_TRIES 5
	/*
	 * Even though SO_REUSEADDR is set, bind() will fail if some other
	 * in.ftpd process has an open data socket between the bind()
	 * and connect() state.
	 */
	for (tries = 0; tries < MAX_BIND_TRIES; tries++) {
		if (bind(s, (struct sockaddr *)&data_source,
						data_source_len) == -1) {
			if (errno == EADDRINUSE)
				sleep(1);
			else
				goto bad;
		} else
			break;
	}
	if (tries >= MAX_BIND_TRIES)
		goto bad;

	seteuid(pw->pw_uid);
	return (fdopen(s, mode));
bad:
	seteuid(pw->pw_uid);
	(void) close(s);
	return (NULL);
}

FILE *
dataconn(name, size, mode)
	char *name;
	off_t size;
	char *mode;
{
	char sizebuf[32];
	FILE *file;
	int retry = 0;
	struct linger linger;
	char abuf[INET6_ADDRSTRLEN];
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int data_dest_len;

	if (size >= 0)
		(void) sprintf(sizebuf, " (%lld bytes)", (longlong_t)size);
	else
		(void) strcpy(sizebuf, "");
	if (pdata > 0) {
		struct sockaddr_storage from;
		int s;
		socklen_t fromlen;

		if (rem_addr.ss_family == AF_INET)
			fromlen = sizeof (struct sockaddr_in);
		else if (rem_addr.ss_family == AF_INET6)
			fromlen = sizeof (struct sockaddr_in6);

		s = accept(pdata, (struct sockaddr *)&from, &fromlen);
		if (s < 0) {
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return (NULL);
		}
		(void) close(pdata);
		pdata = s;
		if (from.ss_family == AF_INET) {
			sin = (struct sockaddr_in *)&from;
			reply(150, "%sdata connection for %s (%s,%d)%s.",
			    print_type(type),
			    name, inet_ntop_native(AF_INET,
			    &sin->sin_addr, abuf, sizeof (abuf)),
			    ntohs(sin->sin_port), sizebuf);
		} else 	if (from.ss_family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)&from;
			reply(150, "%sdata connection for %s (%s,%d)%s.",
			    print_type(type),
			    name, inet_ntop_native(AF_INET6,
			    &sin6->sin6_addr, abuf, sizeof (abuf)),
			    ntohs(sin6->sin6_port), sizebuf);
		}
		return (fdopen(pdata, mode));
	}
	if (data >= 0) {
		reply(125, "Using existing %sdata connection for %s%s.",
		    print_type(type),
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, mode));
	}
	if (usedefault) {
		if (rem_addr.ss_family == AF_INET) {
			rem_sin = (struct sockaddr_in *)&rem_addr;
			data_dest_sin = (struct sockaddr_in *)&data_dest;
			*data_dest_sin = *rem_sin;
		} else if (rem_addr.ss_family == AF_INET6) {
			rem_sin6 = (struct sockaddr_in6 *)&rem_addr;
			data_dest_sin6 = (struct sockaddr_in6 *)&data_dest;
			*data_dest_sin6 = *rem_sin6;
		}
	}
	usedefault = 1;
	file = getdatasock(mode);
	if (file == NULL) {
		if (data_source.ss_family == AF_INET) {
			data_sin = (struct sockaddr_in *)&data_source;
			reply(425, "Can't create data socket (%s,%d): %s.",
				inet_ntop_native(AF_INET,
				    &data_sin->sin_addr,
				    abuf, sizeof (abuf)),
			    ntohs(data_sin->sin_port),
			    strerror(errno));
		} else 	if (data_source.ss_family == AF_INET6) {
			data_sin6 = (struct sockaddr_in6 *)&data_source;
			reply(425, "Can't create data socket (%s,%d): %s.",
				inet_ntop_native(AF_INET6,
				    &data_sin6->sin6_addr,
				    abuf, sizeof (abuf)),
			    ntohs(data_sin6->sin6_port),
			    strerror(errno));
		}
		return (NULL);
	}
	data = fileno(file);
	(void) setsockopt(data, SOL_SOCKET, SO_SNDBUF, (char *)&socksize,
				sizeof (socksize));
	(void) setsockopt(data, SOL_SOCKET, SO_RCVBUF, (char *)&socksize,
				sizeof (socksize));
	linger.l_onoff = 1;
	linger.l_linger = 60;
	(void) setsockopt(data, SOL_SOCKET, SO_LINGER, (char *)&linger,
				sizeof (linger));

	if (data_dest.ss_family == AF_INET) {
		data_dest_len = sizeof (struct sockaddr_in);
	} else	if (data_dest.ss_family == AF_INET6) {
		data_dest_len = sizeof (struct sockaddr_in6);
	}
	while (connect(data, (struct sockaddr *)&data_dest, data_dest_len)
	    < 0) {
		if (errno == EADDRINUSE && retry < swaitmax) {
			sleep((unsigned)swaitint);
			retry += swaitint;
			continue;
		}
		reply(425, "Can't build data connection: %s.",
		    strerror(errno));
		(void) fclose(file);
		data = -1;
		return (NULL);
	}
	if (data_dest.ss_family == AF_INET) {
		data_dest_sin = (struct sockaddr_in *)&data_dest;
		reply(150, "%sdata connection for %s (%s,%d)%s.",
		    print_type(type),
		    name, inet_ntop_native(AF_INET,
		    &data_dest_sin->sin_addr, abuf, sizeof (abuf)),
		    ntohs(data_dest_sin->sin_port), sizebuf);
	} else 	if (data_dest.ss_family == AF_INET6) {
		data_dest_sin6 = (struct sockaddr_in6 *)&data_dest;
		reply(150, "%sdata connection for %s (%s,%d)%s.",
		    print_type(type),
		    name, inet_ntop_native(AF_INET6,
			&data_dest_sin6->sin6_addr, abuf,
			sizeof (abuf)), ntohs(data_dest_sin6->sin6_port),
			sizebuf);
	}
	return (file);
}

/*
 * Envelope for 'send_data_body'.  Allow data connections to fail without
 * terminating the daemon, but SIGPIPE is set to be ignored so that if
 * one occurs on the data channel we'll just catch the error return on
 * the write rather than causing the whole session to abort.
 */

send_data(instr, outstr)
	FILE *instr;		/* Data being sent */
	FILE *outstr;		/* Connection being transmitted upon */
{
	int value;		/* Return value from send_data_body */
	void (*oldpipe)();	/* Old handler for SIGPIPE */

	oldpipe = sigset(SIGPIPE, SIG_IGN);
	value = send_data_body(instr, outstr);
	sigset(SIGPIPE, oldpipe);
	return (value);
}

/*
 * Tranfer the contents of "instr" to
 * "outstr" peer using the appropriate
 * encapulation of the date subject
 * to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
send_data_body(instr, outstr)
	FILE *instr, *outstr;
{
	int c;
	int netfd, filefd, cnt;

	transflag++;
	if (sigsetjmp(urgcatch, 1)) {
		transflag = 0;
		return (-1);
	}
	switch (type) {

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			if (c == '\n') {
				if (ferror(outstr)) {
					transflag = 0;
					return (1);
				}
				(void) putc('\r', outstr);
			}
			(void) putc(c, outstr);
		}
		transflag = 0;
		if (ferror(instr) || ferror(outstr)) {
			return (1);
		}
		return (0);

	case TYPE_I:
	case TYPE_L:
		netfd = fileno(outstr);
		filefd = fileno(instr);

		while ((cnt = read(filefd, buf, sizeof (buf))) > 0) {
			if (write(netfd, buf, cnt) < 0) {
				transflag = 0;
				return (1);
			}
		}
		transflag = 0;
		return (cnt < 0);
	}
	reply(550, "Unimplemented TYPE %d in send_data", type);
	transflag = 0;
	return (-1);
}

/*
 * Transfer data from peer to
 * "outstr" using the appropriate
 * encapulation of the data subject
 * to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
receive_data(instr, outstr)
	FILE *instr, *outstr;
{
	int c;
	int cnt;


	transflag++;
	if (sigsetjmp(urgcatch, 1)) {
		transflag = 0;
		return (-1);
	}
	switch (type) {

	case TYPE_I:
	case TYPE_L:
		while ((cnt = read(fileno(instr), buf, sizeof (buf))) > 0) {
			if ((write(fileno(outstr), buf, cnt)) != cnt) {
				transflag = 0;
				return (1);
			}
		}
		if ((cnt == 0) && (fsync(fileno(outstr)) == -1)) {
			transflag = 0;
			return (1);
		}
		transflag = 0;
		return (cnt < 0);

	case TYPE_E:
		reply(553, "TYPE E not implemented.");
		transflag = 0;
		return (-1);

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			while (c == '\r') {
				if (ferror(outstr)) {
					transflag = 0;
					return (1);
				}
				if ((c = getc(instr)) != '\n')
					if (putc('\r', outstr) == EOF) {
						transflag = 0;
						return (1);
					}
			}
			if (putc(c, outstr) == EOF) {
				transflag = 0;
				return (1);
			}
		}
		transflag = 0;
		if ((fflush(outstr) == EOF) ||
		    ferror(instr) || ferror(outstr) ||
		    (fsync(fileno(outstr)) == -1))
			return (1);
		return (0);
	}
	transflag = 0;
	fatal("Unknown type in receive_data.");
	/*NOTREACHED*/
}

fatal(s)
	char *s;
{
	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
}

/*VARARGS2*/
reply(n, s, va_alist)
	int n;
	char *s;
	va_dcl
{
	va_list ap;

	va_start(ap);
	printf("%d ", n);
	vfprintf(stdout, s, ap);
	printf("\r\n");
	(void) fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d ", n);
		vsyslog(LOG_DEBUG, s, ap);
	}
	va_end(ap);
}

/*VARARGS2*/
lreply(n, s, va_alist)
	int n;
	char *s;
	va_dcl
{
	va_list ap;

	va_start(ap);
	printf("%d-", n);
	vfprintf(stdout, s, ap);
	printf("\r\n");
	(void) fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d- ", n);
		vsyslog(LOG_DEBUG, s, ap);
	}
	va_end(ap);
}

ack(s)
	char *s;
{
	reply(250, "%s command successful.", s);
}

nack(s)
	char *s;
{
	reply(502, "%s command not implemented.", s);
}

yyerror(s)
	char *s;
{
	char *cp;

	if ((cp = strchr(cbuf, '\n')) != NULL)
		*cp = '\0';
	reply(500, "'%s': command not understood.", cbuf);
}

delete(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		reply(550, "%s: %s.", name, strerror(errno));
		return;
	}
	if ((st.st_mode&S_IFMT) == S_IFDIR) {
		if (rmdir(name) < 0) {
			reply(550, "%s: %s.", name, strerror(errno));
			return;
		}
		goto done;
	}
	if (unlink(name) < 0) {
		reply(550, "%s: %s.", name, strerror(errno));
		return;
	}
done:
	ack("DELE");
}

cwd(path)
	char *path;
{

	if (chdir(path) < 0) {
		reply(550, "%s: %s.", path, strerror(errno));
		return;
	}
	ack("CWD");
}

makedir(name)
	char *name;
{
	struct stat st;
	int dochown = stat(name, &st) < 0;

	if (mkdir(name, 0777) < 0) {
		reply(550, "%s: %s.", name, strerror(errno));
		return;
	}
	if (dochown)
		(void) chown(name, pw->pw_uid, (gid_t)-1);
	reply(257, "MKD command successful.");
}

removedir(name)
	char *name;
{

	if (rmdir(name) < 0) {
		reply(550, "%s: %s.", name, strerror(errno));
		return;
	}
	ack("RMD");
}

pwd()
{
	char path[MAXPATHLEN + 1];

	if (getcwd(path, MAXPATHLEN) == NULL) {
		reply(550, "%s.", path);
		return;
	}
	reply(257, "\"%s\" is current directory.", path);
}

char *
renamefrom(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		reply(550, "%s: %s.", name, strerror(errno));
		return ((char *)0);
	}
	reply(350, "File exists, ready for destination name");
	return (name);
}

renamecmd(from, to)
	char *from, *to;
{

	if (rename(from, to) < 0) {
		reply(550, "rename: %s.", strerror(errno));
		return;
	}
	ack("RNTO");
}

dolog(fromp)
	struct sockaddr_storage *fromp;
{
	time_t t;
	extern char *ctime();
	struct in6_addr ipv6addr;
	char abuf[INET6_ADDRSTRLEN];
	int fromplen;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;

	if (fromp->ss_family == AF_INET) {
		sin = (struct sockaddr_in *)fromp;
		fromplen = sizeof (struct sockaddr_in);
	} else if (fromp->ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)fromp;
		fromplen = sizeof (struct sockaddr_in6);
	}

	if (getnameinfo((const struct sockaddr *) fromp, fromplen, remotehost,
	    sizeof (remotehost), NULL, 0, 0) != 0) {
		if (fromp->ss_family == AF_INET6) {
			inet_ntop_native(AF_INET6, &sin6->sin6_addr,
			    abuf, sizeof (abuf));
		} else if (fromp->ss_family == AF_INET) {
				inet_ntop(AF_INET, &sin->sin_addr,
				    abuf, sizeof (abuf));
		}
		(void) strncpy(remotehost, abuf, sizeof (remotehost));
	}
	if (!logging)
		return;
	t = time((time_t *)0);
	syslog(LOG_INFO, "connection from %s at %s", remotehost, ctime(&t));
}

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
dologout(status)
	int status;
{
	if (logged_in) {
		(void) seteuid(0);
		logwtmp(ttyline, "", "");
		if ((pam_start("ftp", pw->pw_name, NULL, &pamh))
							== PAM_SUCCESS) {
			(void) pam_set_item(pamh, PAM_TTY, ttyline);
			(void) pam_set_item(pamh, PAM_RHOST, remotehost);
			(void) pam_close_session(pamh, 0);
			pam_end(pamh, PAM_SUCCESS);
		}
	}
	/* beware of flushing buffers after a SIGPIPE */
	_exit(status);
}

/*
 * Check user requesting login priviledges.
 * Disallow anyone who does not have a standard
 * shell returned by getusershell() (/etc/shells).
 * Disallow anyone mentioned in the file FTPUSERS
 * to allow people such as uucp to be avoided.
 */
checkuser(name)
	char *name;
{
	char *cp;
	FILE *fd;
	struct passwd *p;
	char *shell;
	int found = 0;
	char *getusershell();
	if ((p = getpwnam(name)) == NULL) {
		audit_ftpd_unknown(name);	/* BSM */
		return (0);
	}
	if ((shell = p->pw_shell) == NULL || *shell == 0)
		shell = "/bin/sh";
	while ((cp = getusershell()) != NULL)
		if (strcmp(cp, shell) == 0)
			break;
	endusershell();
	if (cp == NULL)
		return (0);
	if ((fd = fopen(FTPUSERS, "r")) == NULL)
		return (1);
	while (fgets(line, sizeof (line), fd) != NULL) {
		if ((cp = strchr(line, '\n')) != NULL)
			*cp = '\0';
		if (strcmp(line, name) == 0) {
			found++;
			audit_ftpd_excluded(name);	/* BSM */
			break;
		}
	}
	(void) fclose(fd);
	return (!found);
}

myoob()
{
	char *cp;

	/* only process if transfer occurring */
	if (!transflag) {
		return;
	}
	cp = tmpline;
	if (getline(cp, 7, stdin) == NULL) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	}
	upper(cp);
	if (strcmp(cp, "ABOR\r\n"))
		return;
	tmpline[0] = '\0';
	reply(426, "Transfer aborted. Data connection closed.");
	reply(226, "Abort successful");
	siglongjmp(urgcatch, 1);
}

/*
 * Note: The 530 reply codes could be 4xx codes, except nothing is
 * given in the state tables except 421 which implies an exit.  (RFC959)
 */
passive(passive_mode, proto)
	int passive_mode, proto;
{
	socklen_t len;
	struct sockaddr_storage tmp;
	char *p, *a;
	int isv4;
	int af;
	struct sockaddr_in *tmp_sin;
	struct sockaddr_in6 *tmp_sin6;

	switch (proto) {
		case 0:
			af = rem_addr.ss_family;
			break;
		case 1:
			af = AF_INET;
			break;
		case 2:
			af = AF_INET6;
			break;
	}
	/*
	 * This check is necessary to ensure that a pasv command cannot
	 * be accepted unless the user is logged in previously.
	 */

	if (!logged_in)
		return;

	/*
	 * Attempt to issue a pasv command when the user is logged in
	 * previously but later on another USER command is issued followed
	 * by a pasv command.
	 */
	if (pw == NULL) {
		reply(530, "Please login with USER and PASS.");
		logged_in = 0;
		return;
	}
	pdata = socket(af, SOCK_STREAM, 0);
	if (pdata < 0) {
		reply(530, "Can't open passive connection");
		return;
	}
	if (af == AF_INET) {
		ctrl_sin = (struct sockaddr_in *)&ctrl_addr;
		tmp_sin = (struct sockaddr_in *)&tmp;
		*tmp_sin = *ctrl_sin;
		tmp_sin->sin_port = 0;
		len = sizeof (struct sockaddr_in);
	} else if (af == AF_INET6) {
		ctrl_sin6 = (struct sockaddr_in6 *)&ctrl_addr;
		tmp_sin6 = (struct sockaddr_in6 *)&tmp;
		*tmp_sin6 = *ctrl_sin6;
		tmp_sin6->sin6_port = 0;
		len = sizeof (struct sockaddr_in6);
	}
	seteuid(0);
	if (bind(pdata, (struct sockaddr *)&tmp, len) < 0) {
		seteuid(pw->pw_uid);
		(void) close(pdata);
		pdata = -1;
		reply(530, "Can't open passive connection");
		return;
	}
	seteuid(pw->pw_uid);
	if (getsockname(pdata, (struct sockaddr *)&tmp, &len) < 0) {
		(void) close(pdata);
		pdata = -1;
		reply(530, "Can't open passive connection");
		return;
	}

	if (listen(pdata, 1) < 0) {
		(void) close(pdata);
		pdata = -1;
		reply(530, "Can't open passive connection");
		return;
	}
	if (tmp.ss_family == AF_INET) {
		a = (char *)&tmp_sin->sin_addr;
		p = (char *)&tmp_sin->sin_port;
		isv4 = 1;
	} else if (tmp.ss_family == AF_INET6) {
		a = (char *)&tmp_sin6->sin6_addr;
		p = (char *)&tmp_sin6->sin6_port;
		isv4 = 0;
	}
#define	UC(b) (((int)b) & 0xff)
	switch (passive_mode) {
	case TYPE_PASV:
		if (isv4) {
			reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
			    UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]),
			    UC(p[1]));
		} else {
			reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
			    UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
			    UC(p[0]), UC(p[1]));
		}
		break;
	case TYPE_EPSV:
		switch (proto) {
			case 1:
				if (!isv4) {
					reply(522, "Network protocol mismatch, "
					    "use (2)");
					return;
				}
				break;
			case 2:
				if (isv4) {
					reply(522, "Network protocol mismatch, "
					    "use (1)");
					return;
				}
				break;
		}
		/* Same response regardless if v4 or v6 */
		if (isv4) {
			reply(229, "Entering Extended Passive Mode (|||%d|)",
			    ntohs(tmp_sin->sin_port));
		} else {
			reply(229, "Entering Extended Passive Mode (|||%d|)",
			    ntohs(tmp_sin6->sin6_port));
		}
		break;
	case TYPE_LPSV:
		if (isv4) {
			reply(228, "Entering Long Passive Mode "
			    "(%d,%d,%d,%d,%d,%d,%d,%d,%d)",
			    4, 4, UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]), 2,
			    UC(p[0]), UC(p[1]));
		} else {
			reply(228, "Entering Long Passive Mode "
			    "(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
			    "%d,%d,%d,%d,%d)",
			    6, 16,
			    UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
			    UC(a[4]), UC(a[5]), UC(a[6]), UC(a[7]),
			    UC(a[8]), UC(a[9]), UC(a[10]), UC(a[11]),
			    UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
			    2, UC(p[0]), UC(p[1]));
		}
		break;
	}
}

char *
gunique(local)
	char *local;
{
	static char new[MAXPATHLEN];
	char *cp = strrchr(local, '/');
	int d, count = 0;
	char ext = '1';

	if (cp) {
		*cp = '\0';
	}
	d = access(cp ? local : ".", 2);
	if (cp) {
		*cp = '/';
	}
	if (d < 0) {
		syslog(LOG_ERR, "access (%s): %m", local);
		return (NULL);
	}
	(void) strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			reply(452, "Unique file name cannot be created.");
			return (NULL);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9') {
			ext = '0';
		} else {
			ext++;
		}
		if ((d = access(new, 0)) < 0) {
			break;
		}
		if (ext != '0') {
			cp--;
		} else if (*(cp - 2) == '.') {
			*(cp - 1) = '1';
		} else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}


/*
 * ftpd_conv -	This is the conv (conversation) function called from
 *		a PAM authentication module to print error messages
 *		or garner information from the user.
 *		Currently, we pass into this function a pointer to
 *		the password in appdata_ptr, and just return it if
 *		the command type is PROMPT, otherwise we return an
 *		"login incorrect" message to the client.
 */

static int
ftpd_conv(num_msg, msg, response, appdata_ptr)
	int num_msg;
	struct pam_message **msg;
	struct pam_response **response;
	void *appdata_ptr;
{
	struct  pam_message *m;
	struct  pam_response *r;
	int	i;

	if (num_msg <= 0) {
		return (PAM_CONV_ERR);
	}

	*response = (struct pam_response *)calloc(num_msg,
		    sizeof (struct pam_response));

	if (*response == NULL) {
		return (PAM_BUF_ERR);
	}

	m = *msg;
	r = *response;

	/*
	 * Handle a prompt message accordingly.
	 * appdata_ptr contains the prompt message, e.g. "password:"
	 */
	if (m->msg_style == PAM_PROMPT_ECHO_OFF) {
		if (passbuf[0] != '\0') {
			r->resp = strdup(passbuf);
			if (r->resp == NULL) {
				/* free responses */
				r = *response;
				for (i = 0; i < num_msg; i++, r++) {
					if (r->resp)
						free(r->resp);
				}
				free(*response);
				reply(530, "Login incorrect.");
				*response = NULL;
				return (PAM_BUF_ERR);
			}
		} else
			reply(530, "Login incorrect.");

	}

	return (PAM_SUCCESS);
}

#include	<deflt.h>
#include	"libcmd.h"	/* for defcntl */

void
getdefaults()
{
	if (defopen(defaultfile) == 0) {
		char	*cp;
		int	flags;

		/*
		 * ignore case
		 */
		flags = defcntl(DC_GETFLAGS, 0);
		TURNOFF(flags, DC_CASE);
		defcntl(DC_SETFLAGS, flags);

		if (cp = defread(bannervar))
			bannerval = strdup(cp);
		if (cp = defread(umaskvar))
			umaskval = (int)strtoul(cp, (char **)NULL, 0);
	}
}

char *
getbanner()
{
	char	evalbuf[BUFSIZ];
	static char	buf[BUFSIZ];

	if ((bannerval != (char *)NULL) &&
	    strlen(bannerval) + strlen("eval echo '") + strlen("'\n") + 1
		< sizeof (evalbuf)) {
		FILE *fp;
		char	*curshell, *oldshell;

		strcpy(evalbuf, "eval echo '");
		strcat(evalbuf, bannerval);
		strcat(evalbuf, "'\n");

		/*
		 *	We need to ensure that popen uses a
		 *	Bourne shell, even if a different value
		 *	for SHELL is in our current environment
		 *	so we look, change it if necessary, then
		 *	restore the old value when we're done.
		 */
		if (curshell = getenv("SHELL")) {
			oldshell = strdup(curshell);
			(void) putenv(binshellvar);
		} else
			oldshell = (char *)NULL;
		if (fp = _popen(evalbuf, "r")) {
			char	*p;

			/*
			 *	Pipe I/O atomicity guarantees we
			 *	need only one read.
			 */
			if (fread(buf, 1, sizeof (buf), fp)) {
				p = strrchr(buf, '\n');
				if (p)
					*p = '\0';
			}
			(void) _pclose(fp);
			return (buf);
		}
		if (oldshell)
			(void) putenv(oldshell);
	}

	return (getversion());
}

#include	<sys/utsname.h>

/*
 * Returns a pointer to storage containing the utsname's sysname and release
 * fields (as in "SunOS X.Y").
 */
char *
getversion()
{
	struct utsname u;
	static char buf[2 * SYS_NMLN];	/* large enough for sysname + release */

	if (uname(&u) == -1) {
		syslog(LOG_ERR, "uname: %m");
		(void) strcpy(buf, "SunOS");
	} else {
		(void) strcpy(buf, u.sysname);
		(void) strcat(buf, " ");
		(void) strcat(buf, u.release);
	}
	return (buf);
}

/*
 * This is a wrap-around function for inet_ntop(). In case the af is AF_INET6
 * and the address pointed by src is a IPv4-mapped IPv6 address, it
 * returns printable IPv4 address, not IPv4-mapped IPv6 address. In other cases
 * it behaves just like inet_ntop().
 */
const char *
inet_ntop_native(int af, const void *src, char *dst, size_t size)
{
	struct in_addr src4;
	const char *result;

	if (af == AF_INET6) {
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)src)) {
			IN6_V4MAPPED_TO_INADDR((struct in6_addr *)src, &src4);
			result = inet_ntop(AF_INET, &src4, dst, size);
		} else {
			result = inet_ntop(AF_INET6, src, dst, size);
		}
	} else {
		result = inet_ntop(af, src, dst, size);
	}

	return (result);
}
