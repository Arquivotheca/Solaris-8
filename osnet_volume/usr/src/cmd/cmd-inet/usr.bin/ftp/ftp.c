/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ftp.c	1.44	99/11/03 SMI"	/* SVr4.0 1.6	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
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
 *	(c) 1986-1990,1992-1993,1995-1999  Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989	AT&T.
 *	All rights reserved.
 *
 */


#include "ftp_var.h"
#include <arpa/nameser.h>
#include <sys/types.h>
static struct	sockaddr_in6 data_addr;
int	data = -1;
static int	abrtflag = 0;
static int	ptflag = 0;
int		connected;
static int	socksize = 24 * 1024;
static struct	sockaddr_in6 myctladdr;
static jmp_buf	sendabort;
static jmp_buf	recvabort;
static jmp_buf 	ptabort;
static int ptabflg;
boolean_t	eport_supported = B_TRUE;
/*
 * For IPv6 addresses, EPSV will be the default (rather than EPRT/LPRT).
 * The EPSV/ERPT ftp protocols are specified in RFC 2428.
 *
 * Perform EPSV if epassivemode is set.
 */
boolean_t	epassivemode = B_FALSE;
int	use_eprt = 0;	/* Testing option that specifies EPRT by default */
FILE	*ctrl_in, *ctrl_out;

static void abortsend(int sig);
static void abortpt(int sig);
static void proxtrans(char *cmd, char *local, char *remote);
static void cmdabort(int sig);
static int empty(struct fd_set *mask, int sec, int nfds);
static void abortrecv(int sig);
static int initconn(void);
static FILE *dataconn(char *mode);
static void ptransfer(char *direction, off_t bytes, struct timeval *t0,
    struct timeval *t1, char *local, char *remote);
static void tvsub(struct timeval *tdiff, struct timeval *t1,
    struct timeval *t0);
static void psabort(int sig);
static char *gunique(char *local);
static const char *inet_ntop_native(int af, const void *src, char *dst,
    size_t size);

#define	MAX(a, b) ((a) > (b) ? (a) : (b))

static struct	sockaddr_in6	remctladdr;

char *
hookup(char *host, ushort_t port)
{
	register struct hostent *hp = 0;
	int s;
	socklen_t len;
	static char hostnamebuf[80];
	struct in6_addr ipv6addr;
	char abuf[INET6_ADDRSTRLEN];
	int error_num;
	int on = 1;

	bzero((char *)&remctladdr, sizeof (struct sockaddr_in6));
	hp = getipnodebyname(host, AF_INET6, AI_ALL | AI_ADDRCONFIG |
	    AI_V4MAPPED, &error_num);
	if (hp == NULL) {
		if (error_num == TRY_AGAIN) {
			printf("%s: unknown host or invalid literal address "
			    "(try again later)\n", host);
		} else {
			printf("%s: unknown host or invalid literal address\n",
			    host);
		}
		code = -1;
		return ((char *)0);
	}

	bcopy(hp->h_addr_list[0], (caddr_t)&remctladdr.sin6_addr, hp->h_length);
	/*
	 * If hp->h_name is a IPv4-mapped IPv6 literal, we'll convert it to
	 * IPv4 literal address.
	 */
	if ((inet_pton(AF_INET6, hp->h_name, &ipv6addr) > 0) &&
	    IN6_IS_ADDR_V4MAPPED(&ipv6addr)) {
		(void) inet_ntop_native(AF_INET6, &ipv6addr, hostnamebuf,
		    sizeof (hostnamebuf));

		/*
		 * It can even be the case that the "host" supplied by the user
		 * can be a IPv4-mapped IPv6 literal. So, let's fix that too.
		 */
		if ((inet_pton(AF_INET6, host, &ipv6addr) > 0) &&
		    IN6_IS_ADDR_V4MAPPED(&ipv6addr))
			(void) strcpy(host, hostnamebuf);
	} else {
		reset_timer();
		(void) strcpy(hostnamebuf, hp->h_name);
	}
	remctladdr.sin6_family = hp->h_addrtype;

	hostname = hostnamebuf;
	s = socket(AF_INET6, SOCK_STREAM, 0);
	if (s < 0) {
		perror("ftp: socket");
		code = -1;
		freehostent(hp);
		return (0);
	}
	if (timeout && setsockopt(s, IPPROTO_TCP, TCP_ABORT_THRESHOLD,
	    (char *)&timeoutms, sizeof (timeoutms)) < 0 && debug)
	    perror("ftp: setsockopt (TCP_ABORT_THRESHOLD)");
	reset_timer();

	remctladdr.sin6_port = port;

	while (connect(s, (struct sockaddr *)&remctladdr,
	    sizeof (remctladdr)) < 0) {
		if (hp && hp->h_addr_list[1]) {
			int oerrno = errno;

			fprintf(stderr, "ftp: connect to address %s: ",
			    inet_ntop_native(AF_INET6,
			    (void *)&remctladdr.sin6_addr, abuf,
			    sizeof (abuf)));
			errno = oerrno;
			perror((char *)0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0],
			    (caddr_t)&remctladdr.sin6_addr, hp->h_length);
			fprintf(stdout, "Trying %s...\n",
			    inet_ntop_native(AF_INET6,
			    (void *)&remctladdr.sin6_addr, abuf,
			    sizeof (abuf)));
			(void) close(s);
			s = socket(remctladdr.sin6_family, SOCK_STREAM, 0);
			if (s < 0) {
				perror("ftp: socket");
				code = -1;
				freehostent(hp);
				return (0);
			}
			if (timeout && setsockopt(s, IPPROTO_TCP,
			    TCP_ABORT_THRESHOLD, (char *)&timeoutms,
			    sizeof (timeoutms)) < 0 && debug)
				perror("ftp: setsockopt "
				    "(TCP_ABORT_THRESHOLD)");
			continue;
		}

		perror("ftp: connect");
		code = -1;
		freehostent(hp);
		goto bad;
	}
	freehostent(hp);
	hp = NULL;

	/*
	 * Set EPSV mode flag on by default only if a native IPv6 address
	 * is being used -and- the use_eprt is not set.
	 */
	if (use_eprt == 0) {
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)
		    &remctladdr.sin6_addr))
			epassivemode = B_FALSE;
		else
			epassivemode = B_TRUE;
	}
	len = sizeof (myctladdr);
	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		perror("ftp: getsockname");
		code = -1;
		goto bad;
	}
	ctrl_in = fdopen(s, "r");
	ctrl_out = fdopen(s, "w");
	if (ctrl_in == NULL || ctrl_out == NULL) {
		fprintf(stderr, "ftp: fdopen failed.\n");
		if (ctrl_in)
			(void) fclose(ctrl_in);
		if (ctrl_out)
			(void) fclose(ctrl_out);
		code = -1;
		goto bad;
	}
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) {	/* read startup message from server */
		if (ctrl_in)
			(void) fclose(ctrl_in);
		if (ctrl_out)
			(void) fclose(ctrl_out);
		ctrl_in = ctrl_out = NULL;
		ctrl_in = ctrl_out = NULL;
		code = -1;
		goto bad;
	}
	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on,
	    sizeof (on)) < 0 && debug)
		perror("ftp: setsockopt (SO_OOBINLINE)");

	return (hostname);
bad:
	(void) close(s);
	return ((char *)0);
}

int
login(char *host)
{
	char tmp[80];
	char *user, *pass, *acct;
	int n, aflag = 0;

	user = pass = acct = 0;
	if (ruserpass(host, &user, &pass, &acct) < 0) {
		disconnect(0, NULL);
		code = -1;
		return (0);
	}
	if (user == NULL) {
		char *myname = getlogin();

		if (myname == NULL) {
			struct passwd *pp = getpwuid(getuid());

			if (pp != NULL)
				myname = pp->pw_name;
		}
		stop_timer();
		printf("Name (%s:%s): ", host, (myname == NULL) ? "" : myname);
		(void) fgets(tmp, sizeof (tmp) - 1, stdin);
		tmp[strlen(tmp) - 1] = '\0';
		if (*tmp == '\0' && myname != NULL)
			user = myname;
		else
			user = tmp;
	}
	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL)
			pass = mygetpass("Password:");
		n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		if (acct == NULL)
			acct = mygetpass("Account:");
		n = command("ACCT %s", acct);
	}
	if (n != COMPLETE) {
		fprintf(stderr, "Login failed.\n");
		return (0);
	}
	if (!aflag && acct != NULL)
		(void) command("ACCT %s", acct);
	if (proxy)
		return (1);
	for (n = 0; n < macnum; ++n) {
		if (strcmp("init", macros[n].mac_name) == 0) {
			(void) strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	return (1);
}

/*ARGSUSED*/
static void
cmdabort(int sig)
{
	printf("\n");
	(void) fflush(stdout);
	abrtflag++;
	if (ptflag)
		longjmp(ptabort, 1);
}

int
command(char *fmt, ...)
{
	int r;
	void (*oldintr)();
	va_list ap;

	va_start(ap, fmt);
	abrtflag = 0;
	if (debug) {
		printf("---> ");
		vfprintf(stdout, fmt, ap);
		printf("\n");
		(void) fflush(stdout);
	}
	if (ctrl_out == NULL) {
		perror("No control connection for command");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
	vfprintf(ctrl_out, fmt, ap);
	fprintf(ctrl_out, "\r\n");
	(void) fflush(ctrl_out);
	va_end(ap);
	cpend = 1;
	r = getreply(strcmp(fmt, "QUIT") == 0);
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)();
	(void) signal(SIGINT, oldintr);
	return (r);
}

/* Need to save reply reponse from server for use in EPSV mode */
char reply_string[BUFSIZ];

int
getreply(int expecteof)
{
	register int c, n;
	register int dig;
	int originalcode = 0, continuation = 0;
	void (*oldintr)();
	int pflag = 0;
	char *pt = pasv;
	int	len;
	char *cp;

	if (!ctrl_in)
		return (0);
	oldintr = signal(SIGINT, cmdabort);
	for (;;) {
		dig = n = code = 0;
		cp = reply_string;
		reset_timer();	/* once per line */
		while ((c = fgetwc(ctrl_in)) != '\n') {
			if (c == IAC) {	/* handle telnet commands */
				switch (c = fgetwc(ctrl_in)) {
				case WILL:
				case WONT:
					c = fgetwc(ctrl_in);
					fprintf(ctrl_out, "%c%c%wc", IAC,
					    WONT, c);
					(void) fflush(ctrl_out);
					break;
				case DO:
				case DONT:
					c = fgetwc(ctrl_in);
					fprintf(ctrl_out, "%c%c%wc", IAC,
					    DONT, c);
					(void) fflush(ctrl_out);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void) signal(SIGINT, oldintr);
					code = 221;
					return (0);
				}
				lostpeer(0);
				if (verbose) {
					printf(
					    "421 Service not available, remote"
					    " server has closed connection\n");
				}
				else
					printf("Lost connection\n");
				(void) fflush(stdout);
				code = 421;
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == '5' && dig > 4))) {
				if (proxflag &&
				    (dig == 1 || dig == 5 && verbose == 0))
					printf("%s:", hostname);
				(void) putwchar(c);
			}
			if (dig < 4 && isascii(c) && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && code == 227)
				pflag = 1;
			if (dig > 4 && pflag == 1 && isascii(c) && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')') {
					char mb[MB_LEN_MAX];
					int avail;

					/*
					 * space available in pasv[], accounting
					 * for trailing NULL
					 */
					avail = &pasv[sizeof (pasv)] - pt - 1;

					len = wctomb(mb, c);
					if (len <= 0 && avail > 0) {
						*pt++ = (unsigned char)c;
					} else if (len > 0 && avail >= len) {
						bcopy(mb, pt, (size_t)len);
						pt += len;
					} else {
						/*
						 * no room in pasv[];
						 * close connection
						 */
						printf("\nReply too long - "
						    "closing connection\n");
						lostpeer(0);
						(void) fflush(stdout);
						(void) signal(SIGINT, oldintr);
						return (4);
					}
				} else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &reply_string[sizeof (reply_string) - 1])
				*cp++ = c;

		}
		if (verbose > 0 || verbose > -1 && n == '5') {
			(void) putwchar(c);
			(void) fflush(stdout);
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if (n != '1')
			cpend = 0;
		(void) signal(SIGINT, oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer(0);
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)();
		return (n - '0');
	}
}

static int
empty(struct fd_set *mask, int sec, int nfds)
{
	struct timeval t;

	reset_timer();
	t.tv_sec = (long)sec;
	t.tv_usec = 0;
	return (select(nfds, mask, NULL, NULL, &t));
}

/*ARGSUSED*/
static void
abortsend(int sig)
{
	mflag = 0;
	abrtflag = 0;
	printf("\nsend aborted\n");
	(void) fflush(stdout);
	longjmp(sendabort, 1);
}

void
sendrequest(char *cmd, char *local, char *remote, int allowpipe)
{
	FILE *fin, *dout = 0;
	int (*closefunc)();
	void (*oldintr)(), (*oldintp)();
	off_t bytes = 0, hashbytes = FTPBUFSIZ;
	register int c, d;
	struct stat st;
	struct timeval start, stop;

	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	if (setjmp(sendabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortsend);
	if (strcmp(local, "-") == 0)
		fin = stdin;
	else if (allowpipe && *local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fin = mypopen(local + 1, "r");
		if (fin == NULL) {
			perror(local + 1);
			(void) signal(SIGINT, oldintr);
			(void) signal(SIGPIPE, oldintp);
			code = -1;
			return;
		}
		closefunc = mypclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			perror(local);
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode&S_IFMT) != S_IFREG) {
			fprintf(stdout, "%s: not a plain file.\n", local);
			(void) signal(SIGINT, oldintr);
			code = -1;
			fclose(fin);
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		return;
	}
	if (setjmp(sendabort))
		goto abort;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			return;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			return;
		}
	dout = dataconn("w");
	if (dout == NULL)
		goto abort;
	stop_timer();
	(void) gettimeofday(&start, (struct timezone *)0);
	switch (type) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(fin), buf, FTPBUFSIZ)) > 0) {
			if ((d = write(fileno(dout), buf, c)) < 0)
				break;
			bytes += c;
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += FTPBUFSIZ;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0)
			perror(local);
		if (d < 0)
			perror("netout");
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += FTPBUFSIZ;
				}
				if (ferror(dout))
					break;
				(void) putc('\r', dout);
				bytes++;
			}
			(void) putc(c, dout);
			bytes++;
#ifdef notdef
			if (c == '\r') {
				(void) putc('\0', dout); /* this violates rfc */
				bytes++;
			}
#endif
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(fin))
			perror(local);
		if (ferror(dout))
			perror("netout");
		break;
	}
	reset_timer();
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void) fclose(dout); data = -1;
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);

	/*
	 * Only print the transfer successful message if the code returned
	 * from remote is 226 or 250. All other codes are error codes.
	 */
	if ((bytes > 0) && verbose && ((code == 226) || (code == 250)))
		ptransfer("sent", bytes, &start, &stop, local, remote);
	if (!ctrl_in)
		printf("Lost connection\n");
	return;
abort:
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (dout) {
		(void) fclose(dout);
		data = -1;
	}
	(void) getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	(void) gettimeofday(&stop, (struct timezone *)0);
	/*
	 * Only print the transfer successful message if the code returned
	 * from remote is 226 or 250. All other codes are error codes.
	 */
	if ((bytes > 0) && verbose && ((code == 226) || (code == 250)))
		ptransfer("sent", bytes, &start, &stop, local, remote);
	if (!ctrl_in)
		printf("Lost connection\n");
}

/*ARGSUSED*/
static void
abortrecv(int sig)
{
	mflag = 0;
	abrtflag = 0;
	printf("\n");
	(void) fflush(stdout);
	longjmp(recvabort, 1);
}

void
recvrequest(char *cmd, char *local, char *remote, char *mode, int allowpipe)
{
	FILE *fout, *din = 0;
	int (*closefunc)();
	void (*oldintr)(), (*oldintp)();
	int oldverbose, oldtype = 0, tcrflag, nfnd;
	char msg;
	off_t bytes = 0, hashbytes = FTPBUFSIZ;
	struct fd_set mask;
	register int c, d;
	struct timeval start, stop;
	int errflg = 0;
	int nfds;

	if (proxy && strcmp(cmd, "RETR") == 0) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && (strcmp(cmd, "RETR") == 0);
	if (setjmp(recvabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	if (strcmp(local, "-") != 0 && (*local != '|' || !allowpipe)) {
		if (access(local, 2) < 0) {
			char *dir = rindex(local, '/');
			int file_errno = errno;

			if (file_errno != ENOENT && file_errno != EACCES) {
				perror(local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if ((dir != NULL) && (dir != local))
				*dir = 0;
			if (dir == local)
				d = access("/", 2);
			else
				d = access(dir ? local : ".", 2);
			if ((dir != NULL) && (dir != local))
				*dir = '/';
			if (d < 0) {
				perror(local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (!runique && file_errno == EACCES) {
				errno = file_errno;
				perror(local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (runique && file_errno == EACCES &&
			    (local = gunique(local)) == NULL) {
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
		} else if (runique && (local = gunique(local)) == NULL) {
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (strcmp(cmd, "RETR") && type != TYPE_A) {
		oldtype = type;
		oldverbose = verbose;
		if (!debug)
			verbose = 0;
		setascii(0, NULL);
		verbose = oldverbose;
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldtype) {
				if (!debug)
					verbose = 0;
				switch (oldtype) {
					case TYPE_I:
						setbinary(0, NULL);
						break;
					case TYPE_E:
						setebcdic(0, NULL);
						break;
					case TYPE_L:
						settenex(0, NULL);
						break;
				}
				verbose = oldverbose;
			}
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldtype) {
				if (!debug)
					verbose = 0;
				switch (oldtype) {
					case TYPE_I:
						setbinary(0, NULL);
						break;
					case TYPE_E:
						setebcdic(0, NULL);
						break;
					case TYPE_L:
						settenex(0, NULL);
						break;
				}
				verbose = oldverbose;
			}
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (strcmp(local, "-") == 0)
		fout = stdout;
	else if (allowpipe && *local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = mypopen(local + 1, "w");
		if (fout == NULL) {
			perror(local+1);
			goto abort;
		}
		closefunc = mypclose;
	} else {
		fout = fopen(local, mode);
		if (fout == NULL) {
			perror(local);
			goto abort;
		}
		closefunc = fclose;
	}
	(void) gettimeofday(&start, (struct timezone *)0);
	stop_timer();
	switch (type) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(din), buf, FTPBUFSIZ)) > 0) {
			if ((d = write(fileno(fout), buf, c)) != c)
				goto writeerr;
			bytes += c;
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += FTPBUFSIZ;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0) {
			errflg = 1;
			perror("netin");
		}
		if ((d < 0) || ((c == 0) && (fsync(fileno(fout)) == -1))) {
writeerr:
			errflg = 1;
			perror(local);
		}
		break;

	case TYPE_A:
		while ((c = getc(din)) != EOF) {
			while (c == '\r') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += FTPBUFSIZ;
				}
				bytes++;
				if ((c = getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						break;
					if (putc('\r', fout) == EOF)
						goto writer_ascii_err;
				}
#ifdef notdef
				if (c == '\0') {
					bytes++;
					continue;
				}
#endif
			}
			if (putc(c, fout) == EOF)
				goto writer_ascii_err;
			bytes++;
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(din)) {
			errflg = 1;
			perror("netin");
		}
		if ((fflush(fout) == EOF) || ferror(fout) ||
			(fsync(fileno(fout)) == -1)) {
writer_ascii_err:
			errflg = 1;
			perror(local);
		}
		break;
	}
	reset_timer();
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	(void) fclose(din); data = -1;
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) getreply(0);
	if (bytes > 0 && verbose && !errflg)
		ptransfer("received", bytes, &start, &stop, local, remote);
	if (!ctrl_in)
		printf("Lost connection\n");
	if (oldtype) {
		if (!debug)
			verbose = 0;
		switch (oldtype) {
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		verbose = oldverbose;
	}
	return;
abort:

/* abort using RFC959 recommended IP, SYNC sequence  */

	(void) gettimeofday(&stop, (struct timezone *)0);
	if (oldintp)
		(void) signal(SIGPIPE, oldintr);
	(void) signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}

	fprintf(ctrl_out, "%c%c", IAC, IP);
	(void) fflush(ctrl_out);
	msg = (char)IAC;
	/*
	 * send IAC in urgent mode instead of DM because UNIX places oob
	 * mark after urgent byte rather than before as now is protocol
	 */
	if (send(fileno(ctrl_out), &msg, 1, MSG_OOB) != 1) {
		perror("abort");
	}
	fprintf(ctrl_out, "%cABOR\r\n", DM);
	(void) fflush(ctrl_out);
	nfds = fileno(ctrl_in) + 1;
	FD_ZERO(&mask);
	FD_SET(fileno(ctrl_in), &mask);
	if (din) {
		FD_SET(fileno(din), &mask);
		nfds = MAX(fileno(din) + 1, nfds);
	}
	if ((nfnd = empty(&mask, 10, nfds)) <= 0) {
		if (nfnd < 0) {
			perror("abort");
		}
		code = -1;
		lostpeer(0);
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		do {
			reset_timer();
		} while ((c = read(fileno(din), buf, FTPBUFSIZ)) > 0);
	}
	if ((c = getreply(0)) == ERROR && code == 552) {
		/* needed for nic style abort */
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		(void) getreply(0);
	}
	if (oldtype) {
		if (!debug)
			verbose = 0;
		switch (oldtype) {
		case TYPE_I:
			setbinary(0, NULL);
			break;
		case TYPE_E:
			setebcdic(0, NULL);
			break;
		case TYPE_L:
			settenex(0, NULL);
			break;
		}
		verbose = oldverbose;
	}
	(void) getreply(0);
	code = -1;
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din) {
		(void) fclose(din);
		data = -1;
	}
	if (bytes > 0 && verbose)
		ptransfer("received", bytes, &start, &stop, local, remote);
	if (!ctrl_in)
		printf("Lost connection\n");
	(void) signal(SIGINT, oldintr);
}

/*
 * Need to start a listen on the data channel
 * before we send the command, otherwise the
 * server's connect may fail.
 */

static int
initconn(void)
{
	unsigned char *p, *a;
	int result, tmpno = 0;
	int on = 1;
	socklen_t len;
	int bufsize = 0;
	int v4_addr;
	char *c, *c2, dilm;
	in_port_t ports;

	if (epassivemode == B_TRUE) {
		data = socket(AF_INET6, SOCK_STREAM, 0);
		if (data < 0) {
			perror("socket");
			return (1);
		}
		if ((options & SO_DEBUG) &&
		    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			    sizeof (on)) < 0)
			perror("setsockopt (ignored)");

		if (command("EPSV") != COMPLETE) {
			fprintf(stderr, "Passive mode refused. Try EPRT");
			goto noport;
		}

		/*
		 * Get the data port from reply string from the
		 * server.  The format of the reply string is:
		 * 229 Entering Extended Passive Mode (|||port|)
		 * where | is the dilimiter being used.
		 */
		data_addr = remctladdr;

		c = strchr(reply_string, '(');
		c2 = strchr(reply_string, ')');
		if (c == NULL || c2 == NULL) {
			fprintf(stderr, "Extended passive mode"
			    "parsing failure..\n");
			goto bad;
		}
		*(c2 - 1) = NULL;
		/* Delimiter is the next char in the reply string */
		dilm = *(++c);
		while (*c == dilm) {
			if (!*(c++)) {
				fprintf(stderr, "Extended passive mode"
				    "parsing failure..\n");
				goto bad;
			}
		}
		/* assign the port for data connection */
		ports = (in_port_t)atoi(c);
		data_addr.sin6_port =  htons(ports);

		if (connect(data, (struct sockaddr *)&data_addr,
		    sizeof (data_addr)) < 0) {
			perror("connect");
			goto bad;
		}
		return (0);
	}

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.sin6_port = 0;	/* let system pick one */

	if (data != -1)
		(void) close(data);
	data = socket(AF_INET6, SOCK_STREAM, 0);
	if (data < 0) {
		perror("ftp: socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof (on)) < 0) {
			perror("ftp: setsockopt (SO_REUSEADDR)");
			goto bad;
		}
	if (bind(data,
	    (struct sockaddr *)&data_addr, sizeof (data_addr)) < 0) {
		perror("ftp: bind");
		goto bad;
	}
	if (timeout && setsockopt(data, IPPROTO_TCP, TCP_ABORT_THRESHOLD,
	    (char *)&timeoutms, sizeof (timeoutms)) < 0 && debug)
		perror("ftp: setsockopt (TCP_ABORT_THRESHOLD)");
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG,
	    (char *)&on, sizeof (on)) < 0)
		perror("ftp: setsockopt (SO_DEBUG - ignored)");
	/*
	 * Only set the send and receive buffer size if the default size
	 * is smaller than socksize.
	 */
	len = sizeof (bufsize);
	if (getsockopt(data, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize,
	    &len) < 0) {
		perror("ftp: getsockopt (SO_SNDBUF - ignored)");
	}
	if (bufsize < socksize) {
		if (setsockopt(data, SOL_SOCKET, SO_SNDBUF, (char *)&socksize,
		    sizeof (socksize)) < 0) {
			perror("ftp: setsockopt (SO_SNDBUF - ignored)");
		}
	}
	len = sizeof (bufsize);
	bufsize = 0;
	if (getsockopt(data, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize,
	    &len) < 0) {
		perror("ftp: getsockopt (SO_RCVBUF - ignored)");
	}
	if (bufsize < socksize) {
		if (setsockopt(data, SOL_SOCKET, SO_RCVBUF, (char *)&socksize,
		    sizeof (socksize)) < 0) {
			perror("ftp: setsockopt (SO_RCVBUF - ignored)");
		}
	}
	len = sizeof (data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		perror("ftp: getsockname");
		goto bad;
	}

	v4_addr = IN6_IS_ADDR_V4MAPPED(&data_addr.sin6_addr);
	if (listen(data, 1) < 0)
		perror("ftp: listen");

	if (sendport) {
		a = (unsigned char *)&data_addr.sin6_addr;
		p = (unsigned char *)&data_addr.sin6_port;
#define	UC(b)	((b)&0xff)
		if (v4_addr) {
			result =
			    command("PORT %d,%d,%d,%d,%d,%d",
			    UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
			    UC(p[0]), UC(p[1]));
		} else {
			char hname[INET6_ADDRSTRLEN];

			result = COMPLETE + 1;
			/*
			 * if on previous try to server, it was
			 * determined that the server doesn't support
			 * EPRT, don't bother trying again.  Just try
			 * LPRT.
			 */
			if (eport_supported == B_TRUE) {
				if (inet_ntop(AF_INET6, &data_addr.sin6_addr,
				    hname, sizeof (hname)) != NULL) {
					result = command("EPRT |%d|%s|%d|", 2,
					    hname, htons(data_addr.sin6_port));
					if (result != COMPLETE)
						eport_supported = B_FALSE;
				    }
			}
			/* Try LPRT */
			if (result != COMPLETE) {
				result = command(
"LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
6, 16,
UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
UC(a[4]), UC(a[5]), UC(a[6]), UC(a[7]),
UC(a[8]), UC(a[9]), UC(a[10]), UC(a[11]),
UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
2, UC(p[0]), UC(p[1]));
			}
		}

		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
	return (0);
bad:
	(void) close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

static FILE *
dataconn(char *mode)
{
	struct sockaddr_in6 from;
	int s;
	socklen_t fromlen = sizeof (from);

	reset_timer();
	if (epassivemode == B_TRUE)
		return (fdopen(data, mode));

	s = accept(data, (struct sockaddr *)&from, &fromlen);
	if (s < 0) {
		perror("ftp: accept");
		(void) close(data), data = -1;
		return (NULL);
	}
	(void) close(data);
	data = s;
	return (fdopen(data, mode));
}

static void
ptransfer(char *direction, off_t bytes, struct timeval *t0,
    struct timeval *t1, char *local, char *remote)
{
	struct timeval td;
	float s, bs;

	tvsub(&td, t1, t0);
	s = td.tv_sec + (td.tv_usec / 1000000.);
#define	nz(x)	((x) == 0 ? 1 : (x))
	bs = bytes / nz(s);
	if (local && *local != '-')
		printf("local: %s ", local);
	if (remote)
		printf("remote: %s\n", remote);
	printf("%lld bytes %s in %.2g seconds (%.2f Kbytes/s)\n",
		(longlong_t)bytes, direction, s, bs / 1024.);
}

static void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}


/*ARGSUSED*/
static void
psabort(int sig)
{
	abrtflag++;
}

void
pswitch(int flag)
{
	void (*oldintr)();
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
		struct sockaddr_in6 mctl;
		struct sockaddr_in6 hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
		} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void) strncpy(ip->name, hostname, sizeof (ip->name) - 1);
		ip->name[strlen(ip->name)] = '\0';
	} else
		ip->name[0] = 0;
	hostname = op->name;
	ip->hctl = remctladdr;
	remctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = ctrl_in;
	ctrl_in = op->in;
	ip->out = ctrl_out;
	ctrl_out = op->out;
	ip->tpe = type;
	type = op->tpe;
	if (!type)
		type = 1;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void) strncpy(ip->nti, ntin, 16);
	(ip->nti)[strlen(ip->nti)] = '\0';
	(void) strcpy(ntin, op->nti);
	(void) strncpy(ip->nto, ntout, 16);
	(ip->nto)[strlen(ip->nto)] = '\0';
	(void) strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void) strncpy(ip->mi, mapin, MAXPATHLEN - 1);
	(ip->mi)[strlen(ip->mi)] = '\0';
	(void) strcpy(mapin, op->mi);
	(void) strncpy(ip->mo, mapout, MAXPATHLEN - 1);
	(ip->mo)[strlen(ip->mo)] = '\0';
	(void) strcpy(mapout, op->mo);
	(void) signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)();
	}
}

/*ARGSUSED*/
static void
abortpt(int sig)
{
	printf("\n");
	(void) fflush(stdout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

static void
proxtrans(char *cmd, char *local, char *remote)
{
	void (*oldintr)();
	int tmptype, oldtype = 0, secndflag = 0, nfnd;
	extern jmp_buf ptabort;
	char *cmd2;
	struct fd_set mask;
	int ipv4_addr = IN6_IS_ADDR_V4MAPPED(&remctladdr.sin6_addr);

	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if (command(ipv4_addr ? "PASV" : "EPSV") != COMPLETE) {
		printf("proxy server does not support third part transfers.\n");
		return;
	}
	tmptype = type;
	pswitch(0);
	if (!connected) {
		printf("No primary connection\n");
		pswitch(1);
		code = -1;
		return;
	}
	if (type != tmptype) {
		oldtype = type;
		switch (tmptype) {
			case TYPE_A:
				setascii(0, NULL);
				break;
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
	}
	if (command(ipv4_addr ? "PORT %s" : "EPRT %s", pasv) != COMPLETE) {
		switch (oldtype) {
			case 0:
				break;
			case TYPE_A:
				setascii(0, NULL);
				break;
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, (void (*)())abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void) signal(SIGINT, oldintr);
		switch (oldtype) {
			case 0:
				break;
			case TYPE_A:
				setascii(0, NULL);
				break;
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void) getreply(0);
	pswitch(0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	switch (oldtype) {
		case 0:
			break;
		case TYPE_A:
			setascii(0, NULL);
			break;
		case TYPE_I:
			setbinary(0, NULL);
			break;
		case TYPE_E:
			setebcdic(0, NULL);
			break;
		case TYPE_L:
			settenex(0, NULL);
			break;
	}
	pswitch(1);
	ptflag = 0;
	printf("local: %s remote: %s\n", local, remote);
	return;
abort:
	(void) signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if ((strcmp(cmd, "RETR") == 0) && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			switch (oldtype) {
				case 0:
					break;
				case TYPE_A:
					setascii(0, NULL);
					break;
				case TYPE_I:
					setbinary(0, NULL);
					break;
				case TYPE_E:
					setebcdic(0, NULL);
					break;
				case TYPE_L:
					settenex(0, NULL);
					break;
			}
			if (cpend) {
				char msg[2];

				fprintf(ctrl_out, "%c%c", IAC, IP);
				(void) fflush(ctrl_out);
				*msg = (char)IAC;
				*(msg+1) = (char)DM;
				if (send(fileno(ctrl_out), msg, 2, MSG_OOB)
				    != 2)
					perror("abort");
				fprintf(ctrl_out, "ABOR\r\n");
				(void) fflush(ctrl_out);
				FD_ZERO(&mask);
				FD_SET(fileno(ctrl_in), &mask);
				if ((nfnd = empty(&mask, 10,
				    fileno(ctrl_in) + 1)) <= 0) {
					if (nfnd < 0) {
						perror("abort");
					}
					if (ptabflg)
						code = -1;
					lostpeer(0);
				}
				(void) getreply(0);
				(void) getreply(0);
			}
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}
	if (cpend) {
		char msg[2];

		fprintf(ctrl_out, "%c%c", IAC, IP);
		(void) fflush(ctrl_out);
		*msg = (char)IAC;
		*(msg+1) = (char)DM;
		if (send(fileno(ctrl_out), msg, 2, MSG_OOB) != 2)
			perror("abort");
		fprintf(ctrl_out, "ABOR\r\n");
		(void) fflush(ctrl_out);
		FD_ZERO(&mask);
		FD_SET(fileno(ctrl_in), &mask);
		if ((nfnd = empty(&mask, 10, fileno(ctrl_in) + 1)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			switch (oldtype) {
				case 0:
					break;
				case TYPE_A:
					setascii(0, NULL);
					break;
				case TYPE_I:
					setbinary(0, NULL);
					break;
				case TYPE_E:
					setebcdic(0, NULL);
					break;
				case TYPE_L:
					settenex(0, NULL);
					break;
			}
			if (cpend) {
				char msg[2];

				fprintf(ctrl_out, "%c%c", IAC, IP);
				(void) fflush(ctrl_out);
				*msg = (char)IAC;
				*(msg+1) = (char)DM;
				if (send(fileno(ctrl_out), msg, 2, MSG_OOB)
				    != 2)
					perror("abort");
				fprintf(ctrl_out, "ABOR\r\n");
				(void) fflush(ctrl_out);
				FD_ZERO(&mask);
				FD_SET(fileno(ctrl_in), &mask);
				if ((nfnd = empty(&mask, 10,
				    fileno(ctrl_in) + 1)) <= 0) {
					if (nfnd < 0) {
						perror("abort");
					}
					if (ptabflg)
						code = -1;
					lostpeer(0);
				}
				(void) getreply(0);
				(void) getreply(0);
			}
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend) {
		char msg[2];

		fprintf(ctrl_out, "%c%c", IAC, IP);
		(void) fflush(ctrl_out);
		*msg = (char)IAC;
		*(msg+1) = (char)DM;
		if (send(fileno(ctrl_out), msg, 2, MSG_OOB) != 2)
			perror("abort");
		fprintf(ctrl_out, "ABOR\r\n");
		(void) fflush(ctrl_out);
		FD_ZERO(&mask);
		FD_SET(fileno(ctrl_in), &mask);
		if ((nfnd = empty(&mask, 10, fileno(ctrl_in) + 1)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	pswitch(!proxy);
	if (cpend) {
		FD_ZERO(&mask);
		FD_SET(fileno(ctrl_in), &mask);
		if ((nfnd = empty(&mask, 10, fileno(ctrl_in) + 1)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	if (proxy)
		pswitch(0);
	switch (oldtype) {
		case 0:
			break;
		case TYPE_A:
			setascii(0, NULL);
			break;
		case TYPE_I:
			setbinary(0, NULL);
			break;
		case TYPE_E:
			setebcdic(0, NULL);
			break;
		case TYPE_L:
			settenex(0, NULL);
			break;
	}
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void) signal(SIGINT, oldintr);
}

/*ARGSUSED*/
void
reset(int argc, char *argv[])
{
	struct fd_set mask;
	int nfnd = 1;

	FD_ZERO(&mask);
	while (nfnd) {
		FD_SET(fileno(ctrl_in), &mask);
		if ((nfnd = empty(&mask, 0, fileno(ctrl_in) + 1)) < 0) {
			perror("reset");
			code = -1;
			lostpeer(0);
		} else if (nfnd) {
			(void) getreply(0);
		}
	}
}

static char *
gunique(char *local)
{
	static char new[MAXPATHLEN];
	char *cp = rindex(local, '/');
	int d, count = 0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp ? local : ".", 2);
	if (cp)
		*cp = '/';
	if (d < 0) {
		perror(local);
		return ((char *)0);
	}
	(void) strncpy(new, local, sizeof (new));
	if (strlen(local) >= sizeof (new)) {
		printf("gunique: too long: local %s, %d, new %d\n",
		    local, strlen(local), sizeof (new));
		new[MAXPATHLEN - 1] = '\0';
	}

	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			printf("runique: can't find unique file name.\n");
			return ((char *)0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, 0)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
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
