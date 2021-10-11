/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)in.tftpd.c	1.22	99/10/21 SMI"	/* SVr4.0 1.9   */

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
 *	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Trivial file transfer protocol server.  A top level process runs in
 * an infinite loop fielding new TFTP requests.  A child process,
 * communicating via a pipe with the top level process, sends delayed
 * NAKs for those that we can't handle.  A new child process is created
 * to service each request that we can handle.  The top level process
 * exits after a period of time during which no new requests are
 * received.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/tftp.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <setjmp.h>
#include <syslog.h>
#include <sys/param.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>

#ifdef SYSV
#define	bzero(s, n)	memset((s), 0, (n))
#define	signal(s, f)	sigset(s, f)
#define	setjmp(e)	sigsetjmp(e, 1)
#define	longjmp(e, v)	siglongjmp(e, v)
#define	jmp_buf		sigjmp_buf
#endif /* SYSV */

#ifndef UID_NOBODY
#ifdef SYSV
#define	UID_NOBODY	60001
#define	GID_NOBODY	60001
#else
#define	UID_NOBODY	65534
#define	GID_NOBODY	65534
#endif /* SYSV */
#endif /* UID_NOBODY */

#define	TIMEOUT		5
#define	PKTSIZE		SEGSIZE+4
#define	DELAY_SECS	3
#define	DALLYSECS 60

extern	int optind, getopt();
extern	char *optarg;
extern	int errno;

struct	sockaddr_storage client;
struct	sockaddr_in6 *sin6_ptr;
struct	sockaddr_in *sin_ptr;
struct	sockaddr_in6 *from6_ptr;
struct	sockaddr_in *from_ptr;
int	addrfmly;
int	peer;
int	rexmtval = TIMEOUT;
int	maxtimeout = 5*TIMEOUT;
char	buf[PKTSIZE];
char	ackbuf[PKTSIZE];
struct	sockaddr_storage from;
socklen_t	fromlen;
socklen_t	fromplen;
pid_t	child;			/* pid of child handling delayed replys */
int	delay_fd [2];		/* pipe for communicating with child */
FILE	*file;
struct	delay_info {
	long	timestamp;		/* time request received */
	int	ecode;			/* error code to return */
	struct	sockaddr_storage from;	/* address of client */
};

int	initted = 0;
int	securetftp = 0;
int	debug = 0;
int	disable_pnp = 0;
int	standalone = 0;
char	*filename;
uid_t	uid_nobody = UID_NOBODY;
uid_t	gid_nobody = GID_NOBODY;
int	reqsock = 0;		/* file descriptor of request socket */

/*
 * Default directory for unqualified names
 * Used by TFTP boot procedures
 */
char	*homedir = "/tftpboot";

#ifndef SYSV
void
childcleanup()
{
	wait3((union wait *)0, WNOHANG, (struct rusage *)0);
	(void) signal(SIGCHLD, (void (*)())childcleanup);
}
#endif /* SYSV */

int
main(int argc, char **argv)
{
	register struct tftphdr *tp;
	register int n;
	int c;
	struct	passwd *pwd;		/* for "nobody" entry */
	struct in_addr ipv4addr;
	char abuf[INET6_ADDRSTRLEN];
	socklen_t addrlen;

	openlog("tftpd", LOG_PID, LOG_DAEMON);

	while ((c = getopt(argc, argv, "dspS")) != EOF)
		switch (c) {
		case 'd':		/* enable debug */
			debug++;
			continue;
		case 's':		/* secure daemon */
			securetftp = 1;
			continue;
		case 'p':		/* disable name pnp mapping */
			disable_pnp = 1;
			continue;
		case 'S':
			standalone = 1;
			continue;
		case '?':
		default:
usage:
			fprintf(stderr, "usage:  %s [-spd] [home-directory]\n",
			    argv[0]);
			for (; optind < argc; optind++)
				syslog(LOG_ERR, "bad argument %s",
				    argv[optind]);
			exit(1);
		}

	if (optind < argc)
		if (optind == argc - 1 && *argv [optind] == '/')
			homedir = argv [optind];
		else
			goto usage;

	if (pipe(delay_fd) < 0) {
		syslog(LOG_ERR, "pipe (main): %m");
		exit(1);
	}

#ifdef SYSV
	(void) signal(SIGCHLD, SIG_IGN); /* no zombies please */
#else
	(void) signal(SIGCHLD, (void (*)())childcleanup);
#endif /* SYSV */

	if (standalone) {
		socklen_t clientlen;

		sin6_ptr = (struct sockaddr_in6 *)&client;
		clientlen = sizeof (struct sockaddr_in6);
		reqsock = socket(AF_INET6, SOCK_DGRAM, 0);
		if (reqsock == -1) {
			perror("socket");
			exit(1);
		}
		bzero((char *)&client, clientlen);
		sin6_ptr->sin6_family = AF_INET6;
		sin6_ptr->sin6_port = IPPORT_TFTP;
		if (bind(reqsock, (struct sockaddr *)&client,
		    clientlen) == -1) {
			perror("bind");
			exit(1);
		}
		if (debug)
			printf("running in standalone mode...\n");
	} else {
		/* request socket passed on fd 0 by inetd */
		reqsock = 0;
	}
	if (debug) {
		int on = 1;

		(void) setsockopt(reqsock, SOL_SOCKET, SO_DEBUG,
		    (char *)&on, sizeof (on));
	}

	pwd = getpwnam("nobody");
	if (pwd != NULL) {
		uid_nobody = pwd->pw_uid;
		gid_nobody = pwd->pw_gid;
	}

	(void) chdir(homedir);

	if ((child = fork()) < 0) {
		syslog(LOG_ERR, "fork (main): %m");
		exit(1);
	}

	if (child == 0) {
		delayed_responder();
	} /* child */

	/* close read side of pipe */
	(void) close(delay_fd[0]);


	/*
	 * Top level handling of incomming tftp requests.  Read a request
	 * and pass it off to be handled.  If request is valid, handling
	 * forks off and parent returns to this loop.  If no new requests
	 * are received for DALLYSECS, exit and return to inetd.
	 */

	for (;;) {
		fd_set readfds;
		struct timeval dally;

		FD_ZERO(&readfds);
		FD_SET(reqsock, &readfds);
		dally.tv_sec = DALLYSECS;
		dally.tv_usec = 0;

		n = select(reqsock + 1, &readfds, NULL, NULL, &dally);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "select: %m");
			(void) kill(child, SIGKILL);
			exit(1);
		}
		if (n == 0) {
			/* Select timed out.  Its time to die. */
			if (standalone)
				continue;
			else {
				(void) kill(child, SIGKILL);
				exit(0);
			}
		}
		addrlen = sizeof (from);
		if (getsockname(reqsock, (struct sockaddr  *)&from,
		    &addrlen) < 0) {
			syslog(LOG_ERR, "getsockname: %m");
			exit(1);
		}

		switch (from.ss_family) {
		case AF_INET:
			fromlen = (socklen_t)sizeof (struct sockaddr_in);
			break;
		case AF_INET6:
			fromlen = (socklen_t)sizeof (struct sockaddr_in6);
			break;
		default:
			syslog(LOG_ERR,
			    "Unknown address Family on peer connection %d",
			    from.ss_family);
			exit(1);
		}

		n = recvfrom(reqsock, buf, sizeof (buf), 0,
			(struct sockaddr *)&from, &fromlen);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (standalone)
				perror("recvfrom");
			else
				syslog(LOG_ERR, "recvfrom: %m");
			(void) kill(child, SIGKILL);
			exit(1);
		}

		(void) alarm(0);

		switch (from.ss_family) {
		case AF_INET:
			addrfmly = AF_INET;
			fromplen = sizeof (struct sockaddr_in);
			sin_ptr = (struct sockaddr_in *)&client;
			bzero((char *)&client, fromplen);
			sin_ptr->sin_family = AF_INET;
			break;
		case AF_INET6:
			addrfmly = AF_INET6;
			fromplen = sizeof (struct sockaddr_in6);
			sin6_ptr = (struct sockaddr_in6 *)&client;
			bzero((char *)&client, fromplen);
			sin6_ptr->sin6_family = AF_INET6;
			break;
		default:
			syslog(LOG_ERR,
			    "Unknown address Family on peer connection");
			exit(1);
		}
		peer = socket(addrfmly, SOCK_DGRAM, 0);
		if (peer < 0) {
			if (standalone)
				perror("socket (main)");
			else
				syslog(LOG_ERR, "socket (main): %m");
			(void) kill(child, SIGKILL);
			exit(1);
		}
		if (debug) {
			int on = 1;

			(void) setsockopt(peer, SOL_SOCKET, SO_DEBUG,
			    (char *)&on, sizeof (on));
		}

		if (bind(peer, (struct sockaddr *)&client, fromplen) < 0) {
			if (standalone)
				perror("bind (main)");
			else
				syslog(LOG_ERR, "bind (main): %m");
			(void) kill(child, SIGKILL);
			exit(1);
		}
		if (standalone && debug) {
			sin6_ptr = (struct sockaddr_in6 *)&client;
			from6_ptr = (struct sockaddr_in6 *)&from;
			if (IN6_IS_ADDR_V4MAPPED(&from6_ptr->sin6_addr)) {
				IN6_V4MAPPED_TO_INADDR(&from6_ptr->sin6_addr,
				    &ipv4addr);
				inet_ntop(AF_INET, &ipv4addr, abuf,
				    sizeof (abuf));
			} else {
				inet_ntop(AF_INET6, &from6_ptr->sin6_addr, abuf,
				    sizeof (abuf));
			}
			fprintf(stderr,
			    "request from %s port %d; local port %d\n",
			    abuf, from6_ptr->sin6_port, sin6_ptr->sin6_port);
		}
		tp = (struct tftphdr *)buf;
		tp->th_opcode = ntohs((ushort_t)tp->th_opcode);
		if (tp->th_opcode == RRQ || tp->th_opcode == WRQ)
			tftp(tp, n);

		(void) close(peer);
		(void) fclose(file);
	}
}

delayed_responder()
{
	struct delay_info dinfo;
	long now;

	/* we don't use the descriptors passed in to the parent */
	(void) close(0);
	(void) close(1);
	if (standalone)
		(void) close(reqsock);

	if (setgid(gid_nobody) < 0) {
		if (standalone)
			fprintf(stderr, "setgid failed (delayed responder)\n");
		else
			syslog(LOG_ERR, "setgid(%d): %m", gid_nobody);
		exit(1);
	}
	if (setuid(uid_nobody) < 0) {
		if (standalone)
			fprintf(stderr, "setuid failed (delayed responder)\n");
		else
			syslog(LOG_ERR, "setuid(%d): %m", uid_nobody);
		exit(1);
	}

	/* close write side of pipe */
	(void) close(delay_fd[1]);

	for (;;) {
		int n;

		if ((n = read(delay_fd[0], (char *)&dinfo,
		    sizeof (dinfo))) != sizeof (dinfo)) {
			if (n < 0) {
				if (errno == EINTR)
					continue;
				if (standalone)
					perror("read from pipe "
					    "(delayed responder)");
				else
					syslog(LOG_ERR, "read from pipe: %m");
			}
			exit(1);
		}
		switch (from.ss_family) {
		case AF_INET:
			addrfmly = AF_INET;
			fromplen = sizeof (struct sockaddr_in);
			sin_ptr = (struct sockaddr_in *)&client;
			bzero((char *)&client, fromplen);
			sin_ptr->sin_family = AF_INET;
			break;
		case AF_INET6:
			addrfmly = AF_INET6;
			fromplen = sizeof (struct sockaddr_in6);
			sin6_ptr = (struct sockaddr_in6 *)&client;
			bzero((char *)&client, fromplen);
			sin6_ptr->sin6_family = AF_INET6;
			break;
		}
		peer = socket(addrfmly, SOCK_DGRAM, 0);
		if (peer == -1) {
			if (standalone)
				perror("socket (delayed responder)");
			else
				syslog(LOG_ERR, "socket (delay): %m");
			exit(1);
		}
		if (debug) {
			int on = 1;

			(void) setsockopt(peer, SOL_SOCKET, SO_DEBUG,
			    (char *)&on, sizeof (on));
		}

		if (bind(peer, (struct sockaddr *)&client, fromplen) < 0) {
			if (standalone)
				perror("bind (delayed responder)");
			else
				syslog(LOG_ERR, "bind (delay): %m");
			exit(1);
		}
		if (client.ss_family == AF_INET) {
			from_ptr = (struct sockaddr_in *)&dinfo.from;
			from_ptr->sin_family = AF_INET;
		} else {
			from6_ptr = (struct sockaddr_in6 *)&dinfo.from;
			from6_ptr->sin6_family = AF_INET6;
		}
		/*
		 * Since a request hasn't been received from the client
		 * before the delayed responder process is forked, the
		 * from variable is uninitialized.  So set it to contain
		 * the client address.
		 */
		from = dinfo.from;

		/*
		 * only sleep if DELAY_SECS has not elapsed since
		 * original request was received.  Ensure that `now'
		 * is not earlier than `dinfo.timestamp'
		 */
		now = time(0);
		if ((uint_t)(now - dinfo.timestamp) < DELAY_SECS)
			sleep(DELAY_SECS - (now - dinfo.timestamp));
		nak(dinfo.ecode);
		(void) close(peer);
	} /* for */
	/*NOTREACHED*/
}

int	validate_access();
int	sendfile(), recvfile();

struct formats {
	char	*f_mode;
	int	(*f_validate)();
	int	(*f_send)();
	int	(*f_recv)();
	int	f_convert;
} formats[] = {
	{ "netascii",	validate_access,	sendfile,	recvfile, 1 },
	{ "octet",	validate_access,	sendfile,	recvfile, 0 },
#ifdef notdef
	{ "mail",	validate_user,		sendmail,	recvmail, 1 },
#endif
	{ 0 }
};


/*
 * Handle initial connection protocol.
 */
tftp(tp, size)
	struct tftphdr *tp;
	int size;
{
	register char *cp;
	int first = 1, ecode;
	register struct formats *pf;
	char *mode;
	pid_t pid;
	struct delay_info dinfo;
	int fd;
	static int firsttime = 1;

	filename = cp = (char *)&tp->th_stuff;
	if (debug && standalone) {
		if (tp->th_opcode == RRQ)
			fprintf(stderr, "RRQ for %s\n", cp);
		else
			fprintf(stderr, "WRQ for %s\n", cp);
	}
again:
	while (cp < buf + size) {
		if (*cp == '\0')
			break;
		cp++;
	}
	if (*cp != '\0') {
		nak(EBADOP);
		exit(1);
	}
	if (first) {
		mode = ++cp;
		first = 0;
		goto again;
	}
	for (cp = mode; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	for (pf = formats; pf->f_mode; pf++)
		if (strcmp(pf->f_mode, mode) == 0)
			break;
	if (pf->f_mode == 0) {
		nak(EBADOP);
		exit(1);
	}

	/*
	 * XXX fork a new process to handle this request before
	 * chroot(), otherwise the parent won't be able to create a
	 * new socket as that requires library access to system files
	 * and devices.
	 */
	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "fork (tftp): %m");
		return;
	}

	if (pid)
		return;

	/*
	 * Try to see if we can access the file.  The access can still
	 * fail later if we are running in secure mode because of
	 * the chroot() call.  We only want to execute the chroot()  once.
	 */
	if (securetftp && firsttime) {
		if (chroot(homedir) == -1) {
			syslog(LOG_ERR,
			    "tftpd: cannot chroot to directory %s: %m\n",
			    homedir);
			goto delay_exit;
		}
		else
			firsttime = 0;
		(void) chdir("/");  /* cd to  new root */
	}

	/*
	 * Temporarily set uid/gid to someone who is only
	 * allowed "public" access to the file.
	 */
	if (setegid(gid_nobody) < 0) {
		syslog(LOG_ERR, "setgid(%d): %m", gid_nobody);
		exit(1);
	}
	if (seteuid(uid_nobody) < 0) {
		syslog(LOG_ERR, "setuid(%d): %m", uid_nobody);
		exit(1);
	}
	ecode = (*pf->f_validate)(tp->th_opcode);
	/*
	 * Go back to root access so that the chroot() and the
	 * main loop still work!  Perhaps we should always run as
	 * nobody after doing one chroot()?
	 */
	(void) setegid(0);
	(void) seteuid(0);

	if (ecode) {
		/*
		 * The most likely cause of an error here is that
		 * someone has broadcast an RRQ packet because s/he's
		 * trying to boot and doesn't know who the server is.
		 * Rather then sending an ERROR packet immediately, we
		 * wait a while so that the real server has a better chance
		 * of getting through (in case client has lousy Ethernet
		 * interface).  We write to a child that handles delayed
		 * ERROR packets to avoid delaying service to new
		 * requests.  Of course, we would rather just not answer
		 * RRQ packets that are broadcast, but there's no way
		 * for a user process to determine this.
		 */
delay_exit:
		dinfo.timestamp = time(0);

		/*
		 * If running in secure mode, we map all errors to EACCESS
		 * so that the client gets no information about which files
		 * or directories exist.
		 */
		if (securetftp)
			dinfo.ecode = EACCESS;
		else
			dinfo.ecode = ecode;

		dinfo.from = from;
		if (write(delay_fd[1], (char *)&dinfo, sizeof (dinfo)) !=
		    sizeof (dinfo)) {
			syslog(LOG_ERR, "delayed write failed.");
			(void) kill(child, SIGKILL);
			exit(1);
		}
		exit(0);
	}

	/* we don't use the descriptors passed in to the parent */
	(void) close(0);
	(void) close(1);

	/*
	 * Need to do all file access as someone who is only
	 * allowed "public" access to the file.
	 */
	if (setgid(gid_nobody) < 0) {
		syslog(LOG_ERR, "setgid(%d): %m", gid_nobody);
		exit(1);
	}
	if (setuid(uid_nobody) < 0) {
		syslog(LOG_ERR, "setuid(%d): %m", uid_nobody);
		exit(1);
	}

	/*
	 * try to open file only after setuid/setgid.  Note that
	 * a chroot() has already been done.
	 */
	fd = open(filename,
	    tp->th_opcode == RRQ ? O_RDONLY : (O_WRONLY|O_TRUNC));
	if (fd < 0) {
		nak(errno + 100);
		exit(1);
	}
	file = fdopen(fd, (tp->th_opcode == RRQ)? "r":"w");
	if (file == NULL) {
		nak(errno + 100);
		exit(1);
	}

	if (tp->th_opcode == WRQ)
		(*pf->f_recv)(pf);
	else
		(*pf->f_send)(pf);

	exit(0);
}

/*
 *	Maybe map filename into another one.
 *
 *	For PNP, we get TFTP boot requests for filenames like
 *	<Unknown Hex IP Addr>.<Architecture Name>.   We must
 *	map these to 'pnp.<Architecture Name>'.  Note that
 *	uppercase is mapped to lowercase in the architecture names.
 *
 *	For names <Hex IP Addr> there are two cases.  First,
 *	it may be a buggy prom that omits the architecture code.
 *	So first check if <Hex IP Addr>.<arch> is on the filesystem.
 *	Second, this is how most Sun3s work; assume <arch> is sun3.
 */

char *
pnp_check(origname)
	char *origname;
{
	static char buf [MAXNAMLEN + 1];
	char *arch, *s;
	long ipaddr;
	int len = (origname ? strlen(origname) : 0);
	struct hostent *hp;
	DIR *dir;
	struct dirent *dp;

	if (securetftp || disable_pnp || len < 8 || len > 14)
		return (NULL);

	/*
	 * XXX see if this cable allows pnp; if not, return NULL
	 * Requires YP support for determining this!
	 */

	ipaddr = htonl(strtol(origname, &arch, 16));
	if (!arch || (len > 8 && *arch != '.'))
		return (NULL);
	if (len == 8)
		arch = "SUN3";
	else
		arch++;

	/*
	 * Allow <Hex IP Addr>* filename request to to be
	 * satisfied by <Hex IP Addr><Any Suffix> rather
	 * than enforcing this to be Sun3 systems.  Also serves
	 * to make case of suffix a don't-care.
	 */
	if ((dir = opendir("/tftpboot")) == NULL)
		return (NULL);
	while ((dp = readdir(dir)) != NULL) {
		if (strncmp(origname, dp->d_name, 8) == 0) {
			strcpy(buf, dp->d_name);
			closedir(dir);
			return (buf);
		}
	}
	closedir(dir);

	/*
	 * XXX maybe call YP master for most current data iff
	 * pnp is enabled.
	 */

	hp = gethostbyaddr((char *)&ipaddr, sizeof (ipaddr), AF_INET);

	/*
	 * only do mapping PNP boot file name for machines that
	 * are not in the hosts database.
	 */
	if (!hp) {
		strcpy(buf, "pnp.");
		for (s = &buf [4]; *arch; )
			if (isupper (*arch))
				*s++ = tolower (*arch++);
			else
				*s++ = *arch++;
		return (buf);
	} else {
		return (NULL);
	}
}


/*
 * Try to validate file access. File must file to exist and be publicly
 * readable/writable.
 */
validate_access(mode)
	int mode;
{
	struct stat stbuf;
	char *origfile;

	if (stat(filename, &stbuf) < 0) {
		if (errno != ENOENT)
			return (EACCESS);
		if (mode != RRQ)
			return (ENOTFOUND);

		/* try to map requested filename into a pnp filename */
		origfile = filename;
		filename = pnp_check(origfile);
		if (filename == NULL)
			return (ENOTFOUND);

		if (stat(filename, &stbuf) < 0)
			return (errno == ENOENT ? ENOTFOUND : EACCESS);
		syslog(LOG_NOTICE, "%s -> %s\n", origfile, filename);
	}

	if (mode == RRQ) {
		if ((stbuf.st_mode&S_IROTH) == 0)
			return (EACCESS);
	} else {
		if ((stbuf.st_mode&S_IWOTH) == 0)
			return (EACCESS);
	}
	if ((stbuf.st_mode & S_IFMT) != S_IFREG)
		return (EACCESS);
	return (0);
}

int	timeout;
jmp_buf	timeoutbuf;

timer()
{

	timeout += rexmtval;
	if (timeout >= maxtimeout)
		exit(1);
	longjmp(timeoutbuf, 1);
}

/*
 * Send the requested file.
 */
sendfile(pf)
	struct formats *pf;
{
	struct tftphdr *dp, *r_init();
	struct tftphdr *ap;    /* ack packet */
	int block = 1, size, n;

	signal(SIGALRM, (void (*)())timer);
	dp = r_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		size = readit(file, &dp, pf->f_convert);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((ushort_t)DATA);
		dp->th_block = htons((ushort_t)block);
		timeout = 0;
		(void) setjmp(timeoutbuf);

send_data:
		if (debug && standalone)
			fprintf(stderr, "Sending DATA block %d\n", block);
		if (sendto(peer, (char *)dp, size + 4, 0,
		    (struct sockaddr *)&from,  fromplen) != size + 4) {
			if (debug && standalone)
				perror("sendto (data)");
			if ((errno == ENETUNREACH) ||
			    (errno == EHOSTUNREACH) ||
			    (errno == ECONNREFUSED))
				syslog(LOG_WARNING, "sendto (data): %m");
			else
				syslog(LOG_ERR, "sendto (data): %m");
			goto abort;
		}
		read_ahead(file, pf->f_convert);
		for (;;) {
			alarm(rexmtval); /* read the ack */
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				if ((errno == ENETUNREACH) ||
				    (errno == EHOSTUNREACH) ||
				    (errno == ECONNREFUSED))
					syslog(LOG_WARNING, "recv (ack): %m");
				else
					syslog(LOG_ERR, "recv (ack): %m");
				if (debug && standalone)
					perror("recv (ack)");
				goto abort;
			}
			ap->th_opcode = ntohs((ushort_t)ap->th_opcode);
			ap->th_block = ntohs((ushort_t)ap->th_block);

			if (ap->th_opcode == ERROR) {
				if (debug && standalone)
					fprintf(stderr,
						"received ERROR\n");
				goto abort;
			}


			if (ap->th_opcode == ACK) {
				if (debug && standalone)
					fprintf(stderr,
						"received ACK for block %d\n",
						ap->th_block);
				if (ap->th_block == block) {
					break;
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (ap->th_block == (block -1)) {
					goto send_data;
				}
			}

		}
		block++;
	} while (size == SEGSIZE);
abort:
	(void) fclose(file);
}

justquit()
{
	exit(0);
}


/*
 * Receive a file.
 */
int
recvfile(pf)
	struct formats *pf;
{
	struct tftphdr *dp, *w_init();
	struct tftphdr *ap;    /* ack buffer */
	int block = 0, n, size;

	signal(SIGALRM, (void (*)())timer);
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		timeout = 0;
		ap->th_opcode = htons((ushort_t)ACK);
		ap->th_block = htons((ushort_t)block);
		block++;
		(void) setjmp(timeoutbuf);
send_ack:
		if (debug && standalone)
			fprintf(stderr, "Sending ACK for block %d\n", block);
		if (sendto(peer, ackbuf, 4, 0, (struct sockaddr *)&from,
		    fromplen) != 4) {
			if (debug && standalone)
				perror("sendto (ack)");
			syslog(LOG_ERR, "sendto (ack): %m\n");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for (;;) {
			alarm(rexmtval);
			n = recv(peer, (char *)dp, PKTSIZE, 0);
			alarm(0);
			if (n < 0) { /* really? */
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "recv (data): %m");
				goto abort;
			}
			dp->th_opcode = ntohs((ushort_t)dp->th_opcode);
			dp->th_block = ntohs((ushort_t)dp->th_block);
			if (dp->th_opcode == ERROR) {
				if (debug && standalone)
					fprintf(stderr,
						"received ERROR\n");
				return;
			}
			if (dp->th_opcode == DATA) {
				if (debug && standalone)
					fprintf(stderr,
						"Received DATA block %d\n",
						dp->th_block);
				if (dp->th_block == block) {
					break;   /* normal */
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (dp->th_block == (block-1))
					goto send_ack; /* rexmit */
			}
		}
		/*  size = write(file, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, pf->f_convert);
		if (size != (n-4)) { /* ahem */
			if (size < 0) nak(errno + 100);
			else nak(ENOSPACE);
			goto abort;
		}
	} while (size == SEGSIZE);
	write_behind(file, pf->f_convert);
	(void) fclose(file);	/* close data file */

	ap->th_opcode = htons((ushort_t)ACK);    /* send the "final" ack */
	ap->th_block = htons((ushort_t)(block));
	if (debug && standalone)
		fprintf(stderr, "Sending ACK for block %d\n", block);
	if (sendto(peer, ackbuf, 4, 0, (struct sockaddr *)&from,
	    fromplen) == -1) {
		if (debug && standalone)
			perror("sendto (ack)");
	}
	signal(SIGALRM, (void (*)())justquit); /* just quit on timeout */
	alarm(rexmtval);
	n = recv(peer, buf, sizeof (buf), 0); /* normally times out and quits */
	alarm(0);
	if (n >= 4 &&		/* if read some data */
	    dp->th_opcode == DATA && /* and got a data block */
	    block == dp->th_block) {	/* then my last ack was lost */
		if (debug && standalone)
			fprintf(stderr, "Sending ACK for block %d\n", block);
		/* resend final ack */
		if (sendto(peer, ackbuf, 4, 0, (struct sockaddr *)&from,
		    fromplen) == -1) {
			if (debug && standalone)
				perror("sendto (last ack)");
		}
	}
abort:
	return (0);
}

struct errmsg {
	int	e_code;
	char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ -1,		0 }
};

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 * Handles connected as well as unconnected peer.
 */
nak(error)
	int error;
{
	register struct tftphdr *tp;
	int length;
	register struct errmsg *pe;
	int ret;

	tp = (struct tftphdr *)buf;
	tp->th_opcode = htons((ushort_t)ERROR);
	tp->th_code = htons((ushort_t)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		tp->th_code = EUNDEF;   /* set 'undef' errorcode */
	}
	if (pe->e_msg)
		strcpy(tp->th_msg, pe->e_msg);
	else
		strncpy(tp->th_msg, "UNKNOWN",
			sizeof (buf) - sizeof (struct tftphdr));
	length = strlen(tp->th_msg);
	length += sizeof (struct tftphdr);
	if (debug && standalone)
		fprintf(stderr, "Sending NAK: %s\n", tp->th_msg);

	ret = sendto(peer, buf, length, 0, (struct sockaddr *)&from,
	    fromplen);
	if (ret == -1 && errno == EISCONN) {
		/* Try without an address */
		ret = send(peer, buf, length, 0);
	}
	if (ret == -1) {
		if (standalone)
			perror("sendto (nak)");
		else
			syslog(LOG_ERR, "tftpd: nak: %m\n");
	} else if (ret != length) {
		if (standalone)
			perror("sendto (nak) lost data");
		else
			syslog(LOG_ERR, "tftpd: nak: %d lost\n", length - ret);
	}
}
