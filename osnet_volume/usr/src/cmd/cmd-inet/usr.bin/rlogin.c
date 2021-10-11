/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rlogin.c	1.25	99/03/21 SMI"	/* SVr4.0 1.10	*/

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
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


/*
 * rlogin - remote login
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/wait.h>
#ifdef SYSV
#include <sys/stropts.h>
#include <sys/termios.h>
#include <sys/ttold.h>
#else
#include <sys/ioctl.h>
#endif SYSV

#include <netinet/in.h>

#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>
#include <fcntl.h>
#include <locale.h>
#include <sys/sockio.h>

#ifndef TIOCPKT_WINDOW
#define	TIOCPKT_WINDOW 0x80
#endif TIOCPKT_WINDOW

#ifdef SYSV

#define	RAW	O_RAW
#define	CBREAK	O_CBREAK
#define	TBDELAY	O_TBDELAY
#define	CRMOD	O_CRMOD

/*
 * XXX - SysV ptys don't have BSD packet mode, but these should still
 * be defined in some header file.
 */
#define		TIOCPKT_DATA		0x00	/* data packet */
#define		TIOCPKT_FLUSHREAD	0x01	/* flush data not yet */
						/* written to controller */
#define		TIOCPKT_FLUSHWRITE	0x02	/* flush data read from */
						/* controller but not  */
						/* yet processed */
#define		TIOCPKT_STOP		0x04	/* stop output */
#define		TIOCPKT_START		0x08	/* start output */
#define		TIOCPKT_NOSTOP		0x10	/* no more ^S, ^Q */
#define		TIOCPKT_DOSTOP		0x20	/* now do ^S, ^Q */
#define		TIOCPKT_IOCTL		0x40	/* "ioctl" packet */

#define	rindex	strrchr
#define	index	strchr
#define	bcopy(a, b, c)	memcpy(b, a, c)
#define	bcmp(a, b, c)	memcmp(b, a, c)
#define	bzero(s, n)	memset((s), 0, (n))
#define	signal(s, f)	sigset((s), (f))

#ifndef sigmask
#define	sigmask(m)	(1 << ((m)-1))
#endif

#define	set2mask(setp) ((setp)->__sigbits[0])
#define	mask2set(mask, setp) \
	((mask) == -1 ? sigfillset(setp) : (((setp)->__sigbits[0]) = (mask)))


static sigsetmask(mask)
	int mask;
{
	sigset_t oset;
	sigset_t nset;

	(void) sigprocmask(0, (sigset_t *)0, &nset);
	mask2set(mask, &nset);
	(void) sigprocmask(SIG_SETMASK, &nset, &oset);
	return (set2mask(&oset));
}

static sigblock(mask)
	int mask;
{
	sigset_t oset;
	sigset_t nset;

	(void) sigprocmask(0, (sigset_t *)0, &nset);
	mask2set(mask, &nset);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);
	return (set2mask(&oset));
}

int		ttcompat;
struct termios	savetty;
#endif /* SYSV */


char	*index(), *rindex(), *malloc(), *getenv();
char	*errmsg();
char	*name;
int	port_number = IPPORT_LOGINSERVER;
int	rem;
char	cmdchar = '~';
int	nocmdchar = 0;
int	eight;
int	litout;
/*
 * Note that this list of speeds is shorter than the list of speeds
 * supported by termios.  This is because we can't be sure other rlogind's
 * in the world will correctly cope with values other than what 4.2/4.3BSD
 * supported.
 */
char	*speeds[] =
	{ "0", "50", "75", "110", "134", "150", "200", "300",
	    "600", "1200", "1800", "2400", "4800", "9600", "19200",
	    "38400" };
char	term[256] = "network";
extern	int errno;
int	lostpeer();
int	dosigwinch = 0;
#ifndef sigmask
#define	sigmask(m)	(1 << ((m)-1))
#endif
struct	winsize winsize;
int	sigwinch(), oob();

main(argc, argv)
	int argc;
	char **argv;
{
	char *host, *cp;
	struct sgttyb ttyb;
	struct passwd *pwd;
	struct passwd *getpwuid();
	uid_t uid;
	int options = 0, oldmask;
	int on = 1;
#ifdef	SYSV
	speed_t speed = 0;
	int getattr_ret;
#else
	int speed = 0;
#endif /* SYSV */

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

#ifdef SYSV
	{
		int it;

		if ((getattr_ret = tcgetattr(0, &savetty)) < 0)
			perror("tcgetattr");
		it = ioctl(0, I_FIND, "ttcompat");
		if (it < 0) {
			perror("ioctl I_FIND ttcompat");
			exit(1);
		}
		if (it == 0) {
			if (ioctl(0, I_PUSH, "ttcompat") < 0) {
				perror("ioctl I_PUSH ttcompat");
				exit(1);
			}
			ttcompat = 1;
		}
	}
#endif SYSV

	host = rindex(argv[0], '/');
	if (host)
		host++;
	else
		host = argv[0];
	argv++, --argc;
	if (strcmp(host, "rlogin") == 0) {
		if (argc == 0)
			goto usage;
		if (*argv[0] != '-')
			host = *argv++, --argc;
		else
			host = 0;
	}
another:
	if (argc > 0 && strcmp(*argv, "-d") == 0) {
		argv++, argc--;
		options |= SO_DEBUG;
		goto another;
	}
	if (argc > 0 && strcmp(*argv, "-l") == 0) {
		argv++, argc--;
		if (argc == 0)
			goto usage;
		name = *argv++; argc--;
		goto another;
	}
	if (argc > 0 && strncmp(*argv, "-e", 2) == 0) {
		int c;

		cp = &argv[0][2];
		argv++, argc--;

		if (*cp == 0)
			if (argc > 0) {
				cp = *argv++;
				argc--;
			} else {
				fprintf(stderr,
	gettext("rlogin: no escape character specified for -e option\n"));
				goto usage;
			}
		if ((c = *cp) != '\\')
			cmdchar = c;
		else {
			c = cp[1];
			if (c == 0 || c == '\\') {
				cmdchar = '\\';
			} else if (c >= '0' && c <= '7') {
				long strtol(), lc;

				lc = strtol(&cp[1], (char **)0, 8);
				if (lc < 0 || lc > 255) {
					fprintf(stderr,
	gettext("rlogin: octal escape character %s too large\n"),
						cp);
					goto usage;
				}
				cmdchar = (char)lc;
			} else {
				fprintf(stderr,
	gettext("rlogin: unrecognized escape character option %s\n"),
					cp);
				goto usage;
			}
		}
		goto another;
	}
	if (argc > 0 && strcmp(*argv, "-E") == 0) {
		nocmdchar = 1;
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && strcmp(*argv, "-8") == 0) {
		eight = 1;
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && strcmp(*argv, "-L") == 0) {
		litout = 1;
		argv++, argc--;
		goto another;
	}
	if (host == 0) {
		if (argc == 0)
			goto usage;
		host = *argv++, --argc;
	}
	if (argc > 0)
		goto usage;
	pwd = getpwuid(uid = getuid());
	if (pwd == 0) {
		fprintf(stderr, gettext(
	"getpwuid(): can not find password entry for user id %d\n"),
			    uid);
		exit(1);
	}
	cp = getenv("TERM");
	if (cp) {
		(void) strncpy(term, cp, sizeof (term));
		term[sizeof (term) - 1] = '\0';
	}
#ifdef	SYSV
	if (getattr_ret == 0) {
		speed = cfgetospeed(&savetty);
		/*
		 * "Be conservative in what we send" -- Only send baud rates
		 * which at least all 4.x BSD derivatives are known to handle
		 * correctly.
		 * NOTE:  This code assumes new termios speed values will
		 * be "higher" speeds.
		 */
		if (speed > B38400)
			speed = B38400;
	}
#else
	if (ioctl(0, TIOCGETP, &ttyb) == 0) {
		speed = ttyb.sg_ospeed;
	} else {
		perror("ioctl TIOCGETP");
	}
#endif	/* SYSV */

	/*
	 * Only put the terminal speed info in if we have room
	 * so we don't overflow the buffer, and only if we have
	 * a speed we recognize.
	 */
	if (speed > 0 && speed < sizeof (speeds)/sizeof (char *) &&
	    strlen(term) + strlen("/") + strlen(speeds[speed]) + 1 <
	    sizeof (term)) {
		(void) strcat(term, "/");
		(void) strcat(term, speeds[speed]);
	}
	(void) signal(SIGPIPE, (void (*)())lostpeer);
	/* will use SIGUSR1 for window size hack, so hold it off */
	oldmask = sigblock(sigmask(SIGURG) | sigmask(SIGUSR1));

	rem = rcmd_af(&host, htons(port_number), pwd->pw_name,
	    name ? name : pwd->pw_name, term, 0, AF_INET6);
	if (rem < 0)
		goto pop;
	if (options & SO_DEBUG &&
	    setsockopt(rem, SOL_SOCKET, SO_DEBUG, (char *)&on,
			    sizeof (on)) < 0)
		perror("rlogin: setsockopt (SO_DEBUG)");
	{
		int bufsize = 8192;
		setsockopt(rem, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize,
		    sizeof (int));
	}
	uid = getuid();
	if (setuid(uid) < 0) {
		perror("rlogin: setuid");
		goto pop;
	}
	doit(oldmask);
	/*NOTREACHED*/
usage:
	fprintf(stderr,
gettext("usage: rlogin [ -E | -ex ] [ -l username ] [ -8 ] [ -L ] host\n"));
pop:
#ifdef SYSV
	if (ttcompat) {
		/*
		 * Pop ttcompat module
		 */
		(void) ioctl(0, I_POP, 0);
	}
	(void) tcsetattr(0, TCSANOW, &savetty);
#endif /* SYSV */
	exit(1);
}

#define	CRLF "\r\n"

pid_t	child;
int	catchild();
int	copytochild(), writeroob();

int	defflags, tabflag;
int	deflflags;
char	deferase, defkill;
struct	tchars deftc;
struct	ltchars defltc;
struct	tchars notc =	{ (char)-1, (char)-1, (char)-1,
			    (char)-1, (char)-1, (char)-1 };
struct	ltchars noltc =	{ (char)-1, (char)-1, (char)-1,
			    (char)-1, (char)-1, (char)-1 };

doit(oldmask)
{
	int exit();
	struct sgttyb sb;
	int atmark;

	if (ioctl(0, TIOCGETP, (char *)&sb) == -1)
		perror("ioctl TIOCGETP");
	defflags = sb.sg_flags;
	tabflag = defflags & TBDELAY;
	defflags &= ECHO | CRMOD;
	deferase = sb.sg_erase;
	defkill = sb.sg_kill;
	if (ioctl(0, TIOCLGET, (char *)&deflflags) == -1)
		perror("ioctl TIOCLGET");
	if (ioctl(0, TIOCGETC, (char *)&deftc) == -1)
		perror("ioctl TIOCGETC");
	notc.t_startc = deftc.t_startc;
	notc.t_stopc = deftc.t_stopc;
	if (ioctl(0, TIOCGLTC, (char *)&defltc) == -1)
		perror("ioctl TIOCGLTC");
	(void) signal(SIGINT, SIG_IGN);
	setsignal(SIGHUP, exit);
	setsignal(SIGQUIT, exit);
	child = fork();
	if (child == (pid_t)-1) {
		perror("rlogin: fork");
		done(1);
	}
	if (child == 0) {
		mode(1);
		if (reader(oldmask) == 0) {
			prf(gettext("Connection closed."));
			exit(0);
		}
		sleep(1);
		prf(gettext("\007Connection closed."));
		exit(3);
	}

	/*
	 * We may still own the socket, and may have a pending SIGURG (or might
	 * receive one soon) that we really want to send to the reader.  Set a
	 * trap that simply copies such signals to the child.
	 */
#ifdef F_SETOWN_BUG_FIXED
	(void) signal(SIGURG, (void (*)())copytochild);
#else
	(void) signal(SIGURG, SIG_IGN);
#endif /* F_SETOWN_BUG_FIXED */
	(void) signal(SIGUSR1, (void (*)())writeroob);
	/*
	 * Of course, if the urgent byte already arrived, allowing SIGURG
	 * won't get us notification.  So, we check to see if we've got
	 * an urgent byte.  If so, force a call to writeroob() to pretend
	 * we got SIGURG.
	 */
	if (ioctl(rem, SIOCATMARK, &atmark) >= 0) {
		if (atmark) {
			writeroob();
		}
	}
	(void) sigsetmask(oldmask);
	(void) signal(SIGCHLD, (void (*)())catchild);
	writer();
	prf(gettext("Closed connection."));
	done(0);
}

/*
 * Trap a signal, unless it is being ignored.
 */
setsignal(sig, act)
	int sig, (*act)();
{
	int omask = sigblock(sigmask(sig));

	if (signal(sig, (void (*)())act) == SIG_IGN)
		(void) signal(sig, SIG_IGN);
	(void) sigsetmask(omask);
}

done(status)
	int status;
{
	pid_t w;

	mode(0);
	if (child > 0) {
		/* make sure catchild does not snap it up */
		(void) signal(SIGCHLD, SIG_DFL);
		if (kill(child, SIGKILL) >= 0)
#ifdef SYSV
			while ((w = wait(0)) > (pid_t)0 && w != child)
#else
			while ((w = wait((union wait *)0)) > 0 && w != child)
#endif SYSV
				/* void */;
	}
#ifdef SYSV
	if (ttcompat) {
		/*
		 * Pop ttcompat module
		 */
		(void) ioctl(0, I_POP, 0);
	}
	(void) tcsetattr(0, TCSANOW, &savetty);
#endif /* SYSV */
	exit(status);
}

/*
 * Copy SIGURGs to the child process.
 */
copytochild()
{

	(void) kill(child, SIGURG);
}

/*
 * This is called when the reader process gets the out-of-band (urgent)
 * request to turn on the window-changing protocol.
 */
writeroob()
{
	int mask;

	if (dosigwinch == 0) {
		/*
		 * Start tracking window size.  It doesn't matter which
		 * order the next two are in, because we'll be unconditionally
		 * sending a size notification in a moment.
		 */
		(void) signal(SIGWINCH, (void (*)())sigwinch);
		dosigwinch = 1;

		/*
		 * It would be bad if a SIGWINCH came in between the ioctl
		 * and sending the data.  It could result in the SIGWINCH
		 * handler sending a good message, and then us sending an
		 * outdated or inconsistent message.
		 *
		 * Instead, if the change is made before the
		 * ioctl, the sigwinch handler will send a size message
		 * and we'll send another, identical, one.  If the change
		 * is made after the ioctl, we'll send a message with the
		 * old value, and then the sigwinch handler will send
		 * a revised, correct one.
		 */
		mask = sigblock(sigmask(SIGWINCH));
		if (ioctl(0, TIOCGWINSZ, &winsize) == 0)
			sendwindow();
		sigsetmask(mask);
	}
}

catchild()
{
#ifdef SYSV
#include <sys/siginfo.h>
	int status;
	int pid;
	int options;
	siginfo_t	info;
	int error;

	while (1) {
		options = WNOHANG | WEXITED;
		error = waitid(P_ALL, 0, &info, options);
		if (error != 0)
			return (error);
		if (info.si_pid == 0)
			return (0);
		if (info.si_code == CLD_TRAPPED)
			continue;
		if (info.si_code == CLD_STOPPED)
			continue;
		done(info.si_status);
	}
#else
	union wait status;
	pid_t pid;

again:
	pid = wait3(&status, WNOHANG|WUNTRACED, (struct rusage *)0);
	if (pid == 0)
		return;
	/*
	 * if the child (reader) dies, just quit
	 */
	if (pid < 0 || pid == child && !WIFSTOPPED(status.w_status))
		done((int)(status.w_termsig | status.w_retcode));
	goto again;
#endif /* SYSV */
}

/*
 * writer: write to remote: 0 -> line.
 * ~.	terminate
 * ~^Z	suspend rlogin process.
 * ~^Y  suspend rlogin process, but leave reader alone.
 */
writer()
{
	char c;
	register n;
	register bol = 1;		/* beginning of line */
	register local = 0;

	for (;;) {
		n = read(0, &c, 1);
		if (n <= 0) {
			if (n == 0)
				break;
			if (errno == EINTR)
				continue;
			else {
				prf(gettext("Read error from terminal: %s"),
				    errmsg(errno));
				break;
			}
		}
		/*
		 * If we're at the beginning of the line
		 * and recognize a command character, then
		 * we echo locally.  Otherwise, characters
		 * are echo'd remotely.  If the command
		 * character is doubled, this acts as a
		 * force and local echo is suppressed.
		 */
		if (bol && !nocmdchar) {
			bol = 0;
			if (c == cmdchar) {
				bol = 0;
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (c == '.' || c == deftc.t_eofc) {
				echo(c);
				break;
			}
			if (c == defltc.t_suspc || c == defltc.t_dsuspc) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
			if (c != cmdchar) {
				if (write(rem, &cmdchar, 1) < 0) {
					prf(gettext(
"Write error to network: %s"),
					    errmsg(errno));
					break;
				}
			}
		}
		if ((n = write(rem, &c, 1)) <= 0) {
			if (n == 0)
				prf(gettext("line gone"));
			else
				prf(gettext("Write error to network: %s"),
				    errmsg(errno));
			break;
		}
		bol = c == defkill || c == deftc.t_eofc ||
		    c == deftc.t_intrc || c == defltc.t_suspc ||
		    c == '\r' || c == '\n';
	}
}

echo(c)
register char c;
{
	char buf[8];
	register char *p = buf;

	c &= 0177;
	*p++ = cmdchar;
	if (c < ' ') {
		*p++ = '^';
		*p++ = c + '@';
	} else if (c == 0177) {
		*p++ = '^';
		*p++ = '?';
	} else
		*p++ = c;
	*p++ = '\r';
	*p++ = '\n';
	if (write(1, buf, p - buf) < 0)
		prf(gettext("Write error to terminal: %s"), errmsg(errno));
}

stop(cmdc)
	char cmdc;
{
	mode(0);
	(void) signal(SIGCHLD, SIG_IGN);
	(void) kill(cmdc == defltc.t_suspc ? 0 : getpid(), SIGTSTP);
	(void) signal(SIGCHLD, (void (*)())catchild);
	mode(1);
	sigwinch();			/* check for size changes */
}

sigwinch()
{
	struct winsize ws;

	if (dosigwinch && ioctl(0, TIOCGWINSZ, &ws) == 0 &&
	    bcmp(&ws, &winsize, sizeof (ws))) {
		winsize = ws;
		sendwindow();
	}
}

/*
 * Send the window size to the server via the magic escape.
 * Note:  SIGWINCH should be blocked when this is called, lest
 * winsize change underneath us and chaos result.
 */
sendwindow()
{
	char obuf[4 + sizeof (struct winsize)];
	struct winsize *wp = (struct winsize *)(obuf+4);

	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	wp->ws_row = htons(winsize.ws_row);
	wp->ws_col = htons(winsize.ws_col);
	wp->ws_xpixel = htons(winsize.ws_xpixel);
	wp->ws_ypixel = htons(winsize.ws_ypixel);
	if (write(rem, obuf, sizeof (obuf)) < 0)
		prf(gettext("Write error to network: %s"), errmsg(errno));
}


/*
 * reader: read from remote: remote -> stdout
 */
#define	READING	1
#define	WRITING	2

char	rcvbuf[8 * 1024];
int	rcvcnt;
int	rcvstate;
pid_t	ppid;
jmp_buf	rcvtop;

oob()
{
	int out = FWRITE, atmark, n;
	int rcvd = 0;
	char waste[4*BUFSIZ], mark;
	struct sgttyb sb;
	fd_set exceptfds;
	struct timeval tv;
	int ret;

	FD_ZERO(&exceptfds);
	FD_SET(rem, &exceptfds);
	timerclear(&tv);
	ret = select(rem+1, NULL, NULL, &exceptfds, &tv);
	/*
	 * We may get an extra signal at start up time since we are trying
	 * to take all precautions not to miss the urgent byte. This
	 * means we may get here without any urgent data to process, in which
	 * case we do nothing and just return.
	 */
	if (ret <= 0)
		return;

	do {
		if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
			break;
		}
		if (!atmark) {
			/*
			 * Urgent data not here yet.
			 * It may not be possible to send it yet
			 * if we are blocked for output
			 * and our input buffer is full.
			 */
			if (rcvcnt < sizeof (rcvbuf)) {
				n = read(rem, rcvbuf + rcvcnt,
					sizeof (rcvbuf) - rcvcnt);
				if (n <= 0)
					return;
				rcvd += n;
				rcvcnt += n;
			} else {
				/*
				 * We still haven't gotten to the urgent mark
				 * and we're out of buffer space.  Since we
				 * must clear our receive window to allow it
				 * to arrive, we will have to throw away
				 * these bytes.
				 */
				n = read(rem, waste, sizeof (waste));
				if (n <= 0)
					return;
			}
		}
	} while (atmark == 0);
	while (recv(rem, &mark, 1, MSG_OOB) < 0) {
		switch (errno) {

		case EWOULDBLOCK:
			/*
			 * We've reached the urgent mark, so the next
			 * data to arrive will be the urgent, but it must
			 * not have arrived yet.
			 */
			sleep(1);
			continue;

		default:
			return;
		}
	}
	if (mark & TIOCPKT_WINDOW) {
		/*
		 * Let server know about window size changes
		 */
		(void) kill(ppid, SIGUSR1);
	}
	if (!eight && (mark & TIOCPKT_NOSTOP)) {
		if (ioctl(0, TIOCGETP, (char *)&sb) == -1)
			perror("ioctl TIOCGETP");
		sb.sg_flags &= ~CBREAK;
		sb.sg_flags |= RAW;
		if (ioctl(0, TIOCSETN, (char *)&sb) == -1)
			perror("ioctl TIOCSETN 1");
		notc.t_stopc = -1;
		notc.t_startc = -1;
		if (ioctl(0, TIOCSETC, (char *)&notc) == -1)
			perror("ioctl TIOCSETC");
	}
	if (!eight && (mark & TIOCPKT_DOSTOP)) {
		if (ioctl(0, TIOCGETP, (char *)&sb) == -1)
			perror("ioctl TIOCGETP");
		sb.sg_flags &= ~RAW;
		sb.sg_flags |= CBREAK;
		if (ioctl(0, TIOCSETN, (char *)&sb) == -1)
			perror("ioctl TIOCSETN 2");
		notc.t_stopc = deftc.t_stopc;
		notc.t_startc = deftc.t_startc;
		if (ioctl(0, TIOCSETC, (char *)&notc) == -1)
			perror("ioctl TIOCSETC");
	}
	if (mark & TIOCPKT_FLUSHWRITE) {
		if (ioctl(1, TIOCFLUSH, (char *)&out) == -1)
			perror("ioctl TIOCFLUSH");
		for (;;) {
			if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
				perror("ioctl SIOCATMARK");
				break;
			}
			if (atmark)
				break;
			n = read(rem, waste, sizeof (waste));
			if (n <= 0) {
				if (n < 0)
					prf(gettext(
"Read error from network: %s"),
					    errmsg(errno));
				break;
			}
		}
		/*
		 * Don't want any pending data to be output,
		 * so clear the recv buffer.
		 * If we were hanging on a write when interrupted,
		 * don't want it to restart.  If we were reading,
		 * restart anyway.
		 */
		rcvcnt = 0;
		longjmp(rcvtop, 1);
	}
	/*
	 * If we filled the receive buffer while a read was pending,
	 * longjmp to the top to restart appropriately.  Don't abort
	 * a pending write, however, or we won't know how much was written.
	 */
	if (rcvd && rcvstate == READING)
		longjmp(rcvtop, 1);
}

/*
 * reader: read from remote: line -> 1
 */
reader(oldmask)
	int oldmask;
{
	/*
	 * 4.3bsd or later and SunOS 4.0 or later use the posiitive
	 * pid; otherwise use the negative.
	 */
	pid_t pid = getpid();
	int n, remaining;
	char *bufp = rcvbuf;

	(void) signal(SIGTTOU, SIG_IGN);
	(void) signal(SIGURG, (void (*)())oob);
	ppid = getppid();
	if (fcntl(rem, F_SETOWN, pid) == -1)
		perror("fcntl F_SETOWN");
	/*
	 * A SIGURG may have been posted before we were completely forked,
	 * which means we may not have received it. To insure we do not miss
	 * any urgent data, we force the signal. The signal hander will be
	 * able to determine if in fact there is urgent data or not.
	 */
	kill(pid, SIGURG);
	(void) setjmp(rcvtop);
	(void) sigsetmask(oldmask);
	for (;;) {
		while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0) {
			rcvstate = WRITING;
			n = write(1, bufp, remaining);
			if (n < 0) {
				if (errno != EINTR) {
			prf(gettext("Write error to terminal: %s"),
errmsg(errno));
					return (-1);
				}
				continue;
			}
			bufp += n;
		}
		bufp = rcvbuf;
		rcvcnt = 0;
		rcvstate = READING;
		rcvcnt = read(rem, rcvbuf, sizeof (rcvbuf));
		if (rcvcnt == 0)
			return (0);
		if (rcvcnt < 0) {
			if (errno == EINTR)
				continue;
			prf(gettext("Read error from network: %s"),
errmsg(errno));
			return (-1);
		}
	}
}

mode(f)
{
	struct tchars *tc;
	struct ltchars *ltc;
	struct sgttyb sb;
	int	lflags;

	if (ioctl(0, TIOCGETP, (char *)&sb) == -1)
		perror("ioctl TIOCGETP");
	if (ioctl(0, TIOCLGET, (char *)&lflags) == -1)
		perror("ioctl TIOCLGET");
	switch (f) {

	case 0:
		sb.sg_flags &= ~(CBREAK|RAW|TBDELAY);
		sb.sg_flags |= defflags|tabflag;
		tc = &deftc;
		ltc = &defltc;
		sb.sg_kill = defkill;
		sb.sg_erase = deferase;
		lflags = deflflags;
		break;

	case 1:
		sb.sg_flags |= (eight ? RAW : CBREAK);
		sb.sg_flags &= ~defflags;
		/* preserve tab delays, but turn off XTABS */
		if ((sb.sg_flags & TBDELAY) == O_XTABS)
			sb.sg_flags &= ~TBDELAY;
		tc = &notc;
		ltc = &noltc;
		sb.sg_kill = sb.sg_erase = -1;
		if (litout)
			lflags |= LLITOUT;
		break;

	default:
		return;
	}
	if (ioctl(0, TIOCSLTC, (char *)ltc) == -1)
		perror("ioctl TIOCSLTC");
	if (ioctl(0, TIOCSETC, (char *)tc) == -1)
		perror("ioctl TIOCSETC");
	if (ioctl(0, TIOCSETN, (char *)&sb) == -1)
		perror("ioctl TIOCSETN 3");
	if (ioctl(0, TIOCLSET, (char *)&lflags) == -1)
		perror("ioctl TIOCLSET");
}

/*VARARGS*/
prf(f, a1, a2, a3, a4, a5)
	char *f;
{
	fprintf(stderr, f, a1, a2, a3, a4, a5);
	fprintf(stderr, CRLF);
}

lostpeer()
{
	(void) signal(SIGPIPE, SIG_IGN);
	prf("\007Connection closed.");
	done(1);
}

char *
errmsg(errcode)
	int errcode;
{
	extern int sys_nerr;
	extern char *strerror();

	if (errcode < 0 || errcode > sys_nerr)
		return (gettext("Unknown error"));
	else
		return (strerror(errcode));
}
