/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)in.rlogind.c	1.33	99/10/21 SMI"	/* SVr4.0 1.9 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

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
 * 	(c) 1986 to 1998  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * remote login server:
 *	remuser\0
 *	locuser\0
 *	terminal info\0
 *	data
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/poll.h>
#ifdef SYSV
#include <sys/stream.h>
#include <sys/stropts.h>
#endif SYSV

#include <netinet/in.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <syslog.h>
#include <string.h>
#ifndef SYSV
#include <utmp.h>
#endif
#ifdef SYSV
#include <unistd.h>
#include <sac.h>	/* for SC_WILDC */
#include <utmpx.h>
#include <sys/filio.h>
#include <sys/logindmux.h>
#include <sys/rlioctl.h>
#include <sys/termios.h>
#include <sys/telioctl.h>
#include <sys/tihdr.h>
#include <arpa/inet.h>
#include <security/pam_appl.h>
#define	bzero(s, n)	  memset((s), 0, (n))
#define	bcopy(a, b, c)	  memcpy(b, a, c)
#define	signal(s, f)	  sigset(s, f)
#include <malloc.h>
#else
void	*malloc();
#endif SYSV

extern	errno;
int	reapchild();
struct	passwd *getpwnam();
static void	protocol(int, int);
static void	rmut();
static void	doit(int, struct sockaddr_storage *);
static int	readstream(int, char *, int);
static void	fatal(int, char *), fatalperror(int, char *, int);
static int	send_oob(int fd, char *ptr, int count);
static int	removemod(int f, char *modname);

extern int audit_settid(int);	/* set terminal ID */

static int
issock(int fd)
{
	struct stat stats;

	if (fstat(fd, &stats) == -1)
		return (0);
	return (S_ISSOCK(stats.st_mode));
}

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	int on = 1, fromlen;
	struct sockaddr_storage from;
	int issocket;

	openlog("rlogind", LOG_PID | LOG_AUTH, LOG_AUTH);

	issocket = issock(0);
	if (!issocket)
		fatal(0, "stdin is not a socket file descriptor");

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, (socklen_t *)&fromlen)
		< 0) {
		fprintf(stderr, "%s: ", argv[0]);
		perror("getpeername");
		_exit(1);
	}

	if (audit_settid(0)) {	/* set terminal ID */
		fprintf(stderr, "%s: ", argv[0]);
		perror("audit");
		exit(1);
	}

	if (setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof (on)) < 0) {
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	}
	doit(0, &from);
	/*NOTREACHED*/
}

static void	cleanup();
int	netf;			/* fd of the TCP socket */
int	ptymaster;		/* fd of the master side of the pty */
int	nsize = 0;		/* bytes read prior to pushing rlmod */
static char	*rlbuf;		/* buffer where nbytes are read to */
extern	errno;
char	*line;
extern	char	*inet_ntoa();
#ifdef SYSV
int	stopmode = TIOCPKT_DOSTOP;
char	stopc = CTRL('s');
char	startc = CTRL('q');
#endif SYSV

static struct winsize win = { 0, 0, 0, 0 };
pid_t pid;
static char hostname [MAXHOSTNAMELEN + 1];

static void
doit(f, fromp)
	int f;
	struct sockaddr_storage *fromp;
{
	int i, p, t, on = 1;
	char c;

	char abuf[INET6_ADDRSTRLEN];
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int fromplen;
	in_port_t port;
#ifdef SYSV
	extern char *ptsname();
	struct termios tp;
#endif SYSV

	if (!(rlbuf = (char *)malloc(BUFSIZ))) {
		syslog(LOG_ERR, "rlbuf malloc failed\n");
		exit(1);
	}
	alarm(60);
	read(f, &c, 1);
	if (c != 0)
		exit(1);
	alarm(0);
	if (fromp->ss_family == AF_INET) {
		sin = (struct sockaddr_in *)fromp;
		port = sin->sin_port = ntohs((ushort_t)sin->sin_port);
		fromplen = sizeof (struct sockaddr_in);
	} else if (fromp->ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)fromp;
		port = sin6->sin6_port = ntohs((ushort_t)sin6->sin6_port);
		fromplen = sizeof (struct sockaddr_in6);
	} else {
		syslog(LOG_ERR, "unknown address family %d\n",
		    fromp->ss_family);
		fatal(f, "Permission denied");
	}
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
		} else	if (fromp->ss_family == AF_INET) {
				inet_ntop(AF_INET, &sin->sin_addr,
				    abuf, sizeof (abuf));
			}
		(void) strncpy(hostname, abuf, sizeof (hostname));
	    }
	/*
	 * Allow connections only from the "ephemeral" reserved
	 * ports (ports 512 - 1023) by checking the remote port
	 * because other utilities (e.g. in.ftpd) can be used to
	 * allow a unprivileged user to originate a connection
	 * from a privileged port and provide untrustworthy
	 * authentication.
	 */

	if ((port >= (in_port_t)IPPORT_RESERVED) ||
	    (port < (in_port_t)(IPPORT_RESERVED/2)))
		fatal(f, "Permission denied");


	write(f, "", 1);
#ifdef SYSV
	if ((p = open("/dev/ptmx", O_RDWR)) == -1) {
		fatalperror(f, "open /dev/ptmx", errno);
	}
	if (grantpt(p) == -1)
		fatal(f, "could not grant slave pty");
	if (unlockpt(p) == -1)
		fatal(f, "could not unlock slave pty");
	if ((line = ptsname(p)) == NULL)
		fatal(f, "could not enable slave pty");
	if ((t = open(line, O_RDWR)) == -1)
		fatal(f, "could not open slave pty");
	if (ioctl(t, I_PUSH, "ptem") == -1)
		fatalperror(f, "ioctl I_PUSH ptem", errno);
	if (ioctl(t, I_PUSH, "ldterm") == -1)
		fatalperror(f, "ioctl I_PUSH ldterm", errno);
	if (ioctl(t, I_PUSH, "ttcompat") == -1)
		fatalperror(f, "ioctl I_PUSH ttcompat", errno);
	/*
	 * POP the sockmod and push the rlmod module.
	 *
	 * Note that sockmod has to be removed since readstream assumes
	 * a "raw" TPI endpoint (e.g. it uses getmsg).
	 */
	if (removemod(f, "sockmod") < 0)
		fatalperror(f, "couldn't remove sockmod", errno);

	if (ioctl(f, I_PUSH, "rlmod") < 0)
		fatalperror(f, "ioctl I_PUSH rlmod", errno);

	/*
	 * readstream will do a getmsg till it receives
	 * M_PROTO type T_DATA_REQ from rloginmodopen()
	 * indicating all data on the stream prior to pushing rlmod has
	 * been drained at the stream head.
	 */
	if ((nsize = readstream(f, rlbuf, BUFSIZ)) < 0)
		fatalperror(f, "readstream failed\n", errno);
	/*
	 * Make sure the pty doesn't modify the strings passed
	 * to login as part of the "rlogin protocol."  The login
	 * program should set these flags to apropriate values
	 * after it has read the strings.
	 */
	if (ioctl(t, TCGETS, &tp) == -1)
		fatalperror(f, "ioctl TCGETS", errno);
	tp.c_lflag &= ~(ECHO|ICANON);
	tp.c_oflag &= ~(XTABS|OCRNL);
	tp.c_iflag &= ~(IGNPAR|ICRNL);
	if (ioctl(t, TCSETS, &tp) == -1)
		fatalperror(f, "ioctl TCSETS", errno);

#else
	for (c = 'p'; c <= 's'; c++) {
		struct stat stb;
		line = "/dev/ptyXX";
		line[strlen("/dev/pty")] = c;
		line[strlen("/dev/ptyp")] = '0';
		if (stat(line, &stb) < 0)
			break;
		for (i = 0; i < 16; i++) {
			line[strlen("/dev/ptyp")] = "0123456789abcdef"[i];
			p = open(line, 2);
			if (p > 0)
				goto gotpty;
		}
	}
	fatal(f, "Out of ptys");
	/*NOTREACHED*/
gotpty:
	line[strlen("/dev/")] = 't';
#ifdef DEBUG
	{
		int tt = open("/dev/tty", 2);
		if (tt > 0) {
			ioctl(tt, TIOCNOTTY, 0);
			close(tt);
		}
	}
#endif


	t = open(line, 2);
	if (t < 0)
		fatalperror(f, line, errno);
	{
		struct sgttyb b;
		int zero = 0;
		gtty(t, &b);
		b.sg_flags = RAW|ANYP;
		stty(t, &b);
		/*
		 * Turn off PASS8 mode, since "login" no longer does so.
		 */
		ioctl(t, TIOCLSET, &zero);
	}
#endif SYSV
	/*
	 * System V ptys allow the TIOC{SG}WINSZ ioctl to be
	 * issued on the master side of the pty.  Luckily, that's
	 * the only tty ioctl we need to do do, so we can close the
	 * slave side in the parent process after the fork.
	 */
	(void) ioctl(p, TIOCSWINSZ, &win);
	netf = f;
	ptymaster = p;
	pid = fork();
	if (pid < 0)
		fatalperror(f, "", errno);
	if (pid == 0) {
#ifdef SYSV
		int tt;
		struct utmpx ut;

		/* System V login expects a utmp entry to already be there */
		bzero((char *)&ut, sizeof (ut));
		(void) strncpy(ut.ut_user, ".rlogin", sizeof (ut.ut_user));
		(void) strncpy(ut.ut_line, line, sizeof (ut.ut_line));
		ut.ut_pid = getpid();
		ut.ut_id[0] = 'r';
		ut.ut_id[1] = (char)SC_WILDC;
		ut.ut_id[2] = (char)SC_WILDC;
		ut.ut_id[3] = (char)SC_WILDC;
		ut.ut_type = LOGIN_PROCESS;
		ut.ut_exit.e_termination = 0;
		ut.ut_exit.e_exit = 0;
		(void) time(&ut.ut_tv.tv_sec);
		if (makeutx(&ut) == NULL)
			syslog(LOG_INFO, "in.rlogind:\tmakeutx failed");

		/* controlling tty */
		if (setsid() == -1)
			fatalperror(f, "setsid", errno);
		if ((tt = open(line, O_RDWR)) == -1)
			fatalperror(f, "could not reopen slave pty", errno);

		close(f), close(p), close(t);
		dup2(tt, 0), dup2(tt, 1), dup2(tt, 2);
		close(tt);
		execl("/bin/login", "login", "-d", line, "-r", hostname,
			(char *)0);
#else
		close(f), close(p);
		dup2(t, 0), dup2(t, 1), dup2(t, 2);
		close(t);
		}
		execl("/bin/login", "login", "-r", hostname, (char *)0);
#endif /* SYSV */
		fatalperror(2, "/bin/login", errno);
		/*NOTREACHED*/
	}
	close(t);
	(void) ioctl(f, FIONBIO, &on);
	(void) ioctl(p, FIONBIO, &on);
#ifndef SYSV
	ioctl(p, TIOCPKT, &on);
#endif
	signal(SIGTSTP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGCHLD, (void (*)()) cleanup);
#ifdef	SYSV
	setpgrp();
#else
	setpgrp(0, 0);
#endif
	protocol(f, p);
	cleanup();
}

static char	oobdata[] = {(char)TIOCPKT_WINDOW};

/*
 * Handle a "control" request (signaled by magic being present)
 * in the data stream.  For now, we are only willing to handle
 * window size changes.
 */
control(pty, cp, n)
	int pty;
	char *cp;
	int n;
{
	struct winsize w;

	if (n < 4+sizeof (w) || cp[2] != 's' || cp[3] != 's')
		return (0);
	oobdata[0] &= ~TIOCPKT_WINDOW;	/* we know he heard */

	bcopy(cp + 4, (char *)&w, sizeof (w));
	w.ws_row = ntohs(w.ws_row);
	w.ws_col = ntohs(w.ws_col);
	w.ws_xpixel = ntohs(w.ws_xpixel);
	w.ws_ypixel = ntohs(w.ws_ypixel);
	(void) ioctl(pty, TIOCSWINSZ, &w);
	return (4+sizeof (w));
}

#ifndef SYSV
/*
 * rlogin "protocol" machine.
 */
static void
protocol(f, p)
	int f, p;
{
	char pibuf[1024], fibuf[1024], *pbp, *fbp;
	register pcc = 0, fcc = 0;
	int cc, wsize;
	char cntl;

	/*
	 * Must ignore SIGTTOU, otherwise we'll stop
	 * when we try and set slave pty's window shape
	 * (our controlling tty is the master pty).
	 */
	(void) signal(SIGTTOU, SIG_IGN);
	/* indicate new rlogin */
	if (send(f, oobdata, 1, MSG_OOB) < 0) {
		fatalperror(f, "send", errno);
	}

	for (;;) {
		fd_set ibits, obits, ebits;
		int numfds;

		numfds = ((p > f) ? p : f) + 1;

		FD_ZERO(&ibits);
		FD_ZERO(&obits);
		FD_ZERO(&ebits);

		if (fcc)
			FD_SET(p, &obits);
		else
			FD_SET(f, &ibits);
		if (pcc >= 0)
			if (pcc)
				FD_SET(f, &obits);
			else
				FD_SET(p, &ibits);
		FD_SET(p, &ebits);
		if (select(numfds, &ibits, &obits, &ebits, 0) < 0) {
			if (errno == EINTR)
				continue;
			fatalperror(f, "select", errno);
		}

		if (!FD_ISSET(p, &ibits) && !FD_ISSET(f, &ibits) &&
		    !FD_ISSET(p, &obits) && !FD_ISSET(f, &obits) &&
		    !FD_ISSET(p, &ebits)) {

			/* shouldn't happen... */
			sleep(5);
			continue;
		}
#define	pkcontrol(c)	((c)&(TIOCPKT_FLUSHWRITE|TIOCPKT_NOSTOP|TIOCPKT_DOSTOP))
		if (FD_ISSET(p, &ebits)) {
			cc = read(p, &cntl, 1);
			if (cc == 1 && pkcontrol(cntl)) {
				cntl |= oobdata[0];
				send(f, &cntl, 1, MSG_OOB);
				if (cntl & TIOCPKT_FLUSHWRITE) {
					pcc = 0;
					FD_CLR(p, &ebits);
				}
			}
		}
		if (FD_ISSET(f, &ibits)) {
			fcc = read(f, fibuf, sizeof (fibuf));
			if (fcc < 0 && errno == EWOULDBLOCK)
				fcc = 0;
			else {
				register char *cp;
				int left, n;

				if (fcc <= 0)
					break;
				fbp = fibuf;

			top:
				for (cp = fibuf; cp < fibuf+fcc-1; cp++)
					if (cp[0] == magic[0] &&
					    cp[1] == magic[1]) {
						left = fcc - (cp-fibuf);
						n = control(p, cp, left);
						if (n) {
							left -= n;
							if (left > 0)
								bcopy(cp+n,
									cp,
									left);
							fcc -= n;
							goto top; /* n^2 */
						} /* if (n) */
					} /* for (cp = ) */
				} /* else */
		} /* if f is readable */

		if ((FD_ISSET(p, &obits) && fcc)) {
			wsize = fcc;
			do {
				cc = write(p, fbp, wsize);
				wsize /= 2;
			} while (cc < 0 && errno == EWOULDBLOCK && wsize);
			if (cc > 0) {
				fcc -= cc;
				fbp += cc;
			}
		}

		if (FD_ISSET(p, &ibits)) {
			pcc = read(p, pibuf, sizeof (pibuf));
			pbp = pibuf;
			if (pcc < 0 && errno == EWOULDBLOCK)
				pcc = 0;

			else if (pcc <= 0)
				/* error or EOF on the pty.  hang up */
				break;

			else if (pibuf[0] == 0)
				pbp++, pcc--;
			else {
				if (pkcontrol(pibuf[0])) {
					pibuf[0] |= oobdata[0];
					send(f, &pibuf[0], 1, MSG_OOB);
				}
				pcc = 0;
			}
		}
		if (FD_ISSET(f, &obits) && (pcc > 0)) {
			wsize = pcc;
			do {
				cc = write(f, pbp, wsize);
				wsize /= 2;
			} while (cc < 0 && errno == EWOULDBLOCK && wsize);
			if (cc > 0) {
				pcc -= cc;
				pbp += cc;
			}
		}
	}
}
#else
/*
 * rlogin "protocol" machine.
 */
static void
protocol(f, p)
	int f, p;
{
	struct	stat	buf;
	struct 	protocol_arg	rloginp;
	struct	strioctl	rloginmod;
	int	ptmfd;	/* fd of logindmux coneected to ptmx */
	int	netfd;	/* fd of logindmux connected to netf */

	/*
	 * Must ignore SIGTTOU, otherwise we'll stop
	 * when we try and set slave pty's window shape
	 * (our controlling tty is the master pty).
	 */
	(void) signal(SIGTTOU, SIG_IGN);
	/* indicate new rlogin */
	if (send_oob(f, oobdata, 1) < 0) {
		fatalperror(f, "send_oob", errno);
	}

	/*
	 * Open logindmux driver and link netf and ptmx
	 * underneath logindmux.
	 */

	if ((ptmfd = open("/dev/logindmux", O_RDWR)) == -1) {
		fatalperror(f, "open /dev/logindmux", errno);
	}

	if ((netfd = open("/dev/logindmux", O_RDWR)) == -1) {
		fatalperror(f, "open /dev/logindmux", errno);
	}

	if (ioctl(ptmfd, I_LINK, p) < 0)
		fatal(f, "ioctl I_LINK of /dev/ptmx failed\n");

	if (ioctl(netfd, I_LINK, f) < 0)
		fatal(f, "ioctl I_LINK of tcp connection failed\n");

	/*
	 * Figure out the device number of the ptm's mux fd, and pass that
	 * to the net's mux.
	 */
	fstat(ptmfd, &buf);
	rloginp.dev = buf.st_rdev;
	rloginp.flag = 0;

	rloginmod.ic_cmd = LOGDMX_IOC_QEXCHANGE;
	rloginmod.ic_timout = -1;
	rloginmod.ic_len = sizeof (struct protocol_arg);
	rloginmod.ic_dp = (char *)&rloginp;

	if (ioctl(netfd, I_STR, &rloginmod) < 0)
		fatal(netfd, "ioctl LOGDMX_IOC_QEXCHANGE of netfd failed\n");

	/*
	 * Figure out the device number of the net's mux fd, and pass that
	 * to the ptm's mux.
	 */
	fstat(netfd, &buf);
	rloginp.dev = buf.st_rdev;
	rloginp.flag = 1;

	rloginmod.ic_cmd = LOGDMX_IOC_QEXCHANGE;
	rloginmod.ic_timout = -1;
	rloginmod.ic_len = sizeof (struct protocol_arg);
	rloginmod.ic_dp = (char *)&rloginp;

	if (ioctl(ptmfd, I_STR, &rloginmod) < 0)
		fatal(netfd, "ioctl LOGDMXZ_IOC_QEXCHANGE of ptmfd failed\n");
	/*
	 * Send an ioctl type RL_IOC_ENABLE to reenable the
	 * message queue and reinsert the data read from streamhead
	 * at the time of pushing rloginmod module.
	 * We need to send this ioctl even if no data was read earlier
	 * since we need to reenable the message queue of rloginmod module.
	 */
	rloginmod.ic_cmd = RL_IOC_ENABLE;
	rloginmod.ic_timout = -1;
	if (nsize) {
		rloginmod.ic_len = nsize;
		rloginmod.ic_dp = rlbuf;
	} else {
		rloginmod.ic_len = 0;
		rloginmod.ic_dp = NULL;
	}

	if (ioctl(netfd, I_STR, &rloginmod) < 0)
		fatal(netfd, "ioctl RL_IOC_ENABLE of netfd failed\n");

	/*
	 * User level daemon now pauses till the shell exits.
	 */
	pause();
}
#endif

static void
cleanup()
{
	rmut();
#ifndef SYSV
	vhangup();
#endif /* SYSV */

	exit(1);
	/*NOTREACHED*/
}

/*
 * TPI style replacement for socket send() primitive, so we don't require
 * sockmod to be on the stream.
 */
static int
send_oob(int fd, char *ptr, int count)
{
	struct T_exdata_req exd_req;
	struct strbuf hdr, dat;
	int ret;

	exd_req.PRIM_type = T_EXDATA_REQ;
	exd_req.MORE_flag = 0;

	hdr.buf = (char *)&exd_req;
	hdr.len = sizeof (exd_req);

	dat.buf = ptr;
	dat.len = count;

	ret = putmsg(fd, &hdr, &dat, 0);
	if (ret == 0) {
		ret = count;
	}
	return (ret);
}


static void
fatal(f, msg)
	int f;
	char *msg;
{
	char buf[BUFSIZ];

	buf[0] = '\01';		/* error indicator */
	(void) sprintf(buf + 1, "rlogind: %s.\r\n", msg);
	(void) write(f, buf, strlen(buf));
	exit(1);
	/*NOTREACHED*/
}

static void
fatalperror(f, msg, errno)
	int f;
	char *msg;
	int errno;
{
	char buf[BUFSIZ];
	char *errstr;

	if ((errstr = strerror(errno)) != (char *)NULL)
		(void) sprintf(buf, "%s: %s", msg, errstr);
	else
		(void) sprintf(buf, "%s: Error %d", msg, errno);
	fatal(f, buf);
	/*NOTREACHED*/
}


#ifdef SYSV
static void
rmut()
{
	pam_handle_t *pamh;
	struct utmpx *up;
	char user[sizeof (up->ut_user) + 1];
	char ttyn[sizeof (up->ut_line) + 1];
	char rhost[sizeof (up->ut_host) + 1];

	signal(SIGCHLD, SIG_IGN); /* while cleaning up dont allow disruption */

	setutxent();
	while (up = getutxent()) {
		if (up->ut_pid == pid) {
			if (up->ut_type == DEAD_PROCESS) {
				/*
				 * Cleaned up elsewhere.
				 */
				break;
			}

			strncpy(user, up->ut_user, sizeof (up->ut_user));
			user[sizeof (up->ut_user)] = '\0';
			strncpy(ttyn, up->ut_line, sizeof (up->ut_line));
			ttyn[sizeof (up->ut_line)] = '\0';
			strncpy(rhost, up->ut_host, sizeof (up->ut_host));
			rhost[sizeof (up->ut_host)] = '\0';

			if ((pam_start("rlogin", user,  NULL, &pamh))
							== PAM_SUCCESS) {
				(void) pam_set_item(pamh, PAM_TTY, ttyn);
				(void) pam_set_item(pamh, PAM_RHOST, rhost);
				(void) pam_close_session(pamh, 0);
				pam_end(pamh, PAM_SUCCESS);
			}

			up->ut_type = DEAD_PROCESS;
			up->ut_exit.e_termination = WTERMSIG(0);
			up->ut_exit.e_exit = WEXITSTATUS(0);
			(void) time(&up->ut_tv.tv_sec);

			if (modutx(up) == NULL) {
				/*
				 * Since modutx failed we'll
				 * write out the new entry
				 * ourselves.
				 */
				(void) pututxline(up);
				updwtmpx("wtmpx", up);
			}
			break;
		}
	}

	endutxent();

	signal(SIGCHLD, (void (*)())cleanup);
}

#else /* !SYSV */

#define	SCPYN(a, b)	strncpy(a, b, sizeof (a))
#define	SCMPN(a, b)	strncmp(a, b, sizeof (a))

static void
rmut()
{
	register f;
	int found = 0;
	struct utmp *u, *utmp;
	int nutmp;
	struct stat statbf;
	struct	utmp wtmp;
	char	wtmpf[]	= WTMP_FILE;
	char	utmpf[] = UTMP_FILE;

	signal(SIGCHLD, SIG_IGN); /* while cleaning up don't allow disruption */
	f = open(utmpf, O_RDWR);
	if (f >= 0) {
		fstat(f, &statbf);
		utmp = (struct utmp *)malloc(statbf.st_size);
		if (!utmp)
			syslog(LOG_ERR, "utmp malloc failed");
		if (statbf.st_size && utmp) {
			nutmp = read(f, utmp, statbf.st_size);
			nutmp /= sizeof (struct utmp);

			for (u = utmp; u < &utmp[nutmp]; u++) {
				if (SCMPN(u->ut_line, line+5) ||
				    u->ut_name[0] == 0)
					continue;
				lseek(f, ((long)u)-((long)utmp), L_SET);
				SCPYN(u->ut_name, "");
				SCPYN(u->ut_host, "");
				time(&u->ut_time);
				write(f, (char *)u, sizeof (wtmp));
				found++;
			}
		}
		close(f);
	}
	if (found) {
		f = open(wtmpf, O_WRONLY|O_APPEND);
		if (f >= 0) {
			SCPYN(wtmp.ut_line, line+5);
			SCPYN(wtmp.ut_name, "");
			SCPYN(wtmp.ut_host, "");
			time(&wtmp.ut_time);
			write(f, (char *)&wtmp, sizeof (wtmp));
			close(f);
		}
	}
	chmod(line, 0666);
	chown(line, 0, 0);
	line[strlen("/dev/")] = 'p';
	chmod(line, 0666);
	chown(line, 0, 0);
	signal(SIGCHLD, (void (*)())cleanup);
}
#endif /* SYSV */

readstream(fd, buf, size)
	int	fd;
	char	*buf;
	int	size;
{
	struct strbuf ctlbuf, datbuf;
	union T_primitives tpi;
	int	nbytes = 0;
	int	ret = 0;
	int	flags = 0;
	int	bufsize = size;
	int	nread;

	bzero((char *)&ctlbuf, sizeof (ctlbuf));
	bzero((char *)&datbuf, sizeof (datbuf));

	ctlbuf.buf = (char *)&tpi;
	ctlbuf.maxlen = sizeof (tpi);
	datbuf.buf = buf;
	datbuf.maxlen = size;

	for (;;) {
		if (ioctl(fd, I_NREAD, &nread) < 0) {
			syslog(LOG_ERR, "I_NREAD returned error %m");
			return (-1);
		}
		if (nread + nbytes > bufsize) {
			buf = (char *)realloc(buf, (unsigned)(bufsize + nread));
			if (buf == NULL) {
				syslog(LOG_WARNING,
				    "buffer allocation failed\n");
				return (-1);
			}
			bufsize += nread;
			rlbuf = buf;
			datbuf.buf = buf + nbytes;
		}
		datbuf.maxlen = bufsize - nbytes;
		ret = getmsg(fd, &ctlbuf, &datbuf, &flags);
		if (ret < 0) {
			syslog(LOG_ERR, "getmsg failed error %m");
			return (-1);
		}
		if (ctlbuf.len <= 0) {
			nbytes += datbuf.len;
			datbuf.buf += datbuf.len;
			continue;
		}
		if (tpi.type == T_DATA_REQ) {
			return (nbytes);
		}
		if ((tpi.type == T_ORDREL_IND) || (tpi.type == T_DISCON_IND))
			cleanup();
	}
}

/*
 * Verify that the named module is at the top of the stream
 * and then pop it off.
 */
static int
removemod(int f, char *modname)
{
	char topmodname[BUFSIZ];

	if (ioctl(f, I_LOOK, topmodname) < 0)
		return (-1);
	if (strcmp(modname, topmodname) != 0) {
		errno = ENXIO;
		return (-1);
	}
	if (ioctl(f, I_POP, 0) < 0)
		return (-1);
	return (0);
}
