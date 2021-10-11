/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)main.c	1.19	98/06/16 SMI"	/* SVr4.0 1.5	*/

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
 * 	(c) 1986,1987,1988,1989,1991,1992,1997,1998  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing Table Management Daemon
 */
#define	EXTERN
#include "defs.h"
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/sockio.h>
#include <sys/stropts.h>
#include <net/if.h>

#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/file.h>

#ifdef DEBUG
void if_dump(), rtdump();
#endif
int	maysupply = -1;		/* process may supply updates */
int	supplier = -1;		/* process should supply updates */
static int gateway = 0;		/* 1 if we are a gateway to parts beyond */
int	save_space = 0;		/* If one and not a supplier we install */
				/* default route only */
static int deamon = 1;		/* Fork off a detached deamon */

struct	rip *msg = (struct rip *)packet;
int	iosoc;			/* descriptor to do ioctls on */

static void process(int fd);
static int getsocket(int domain, int type, struct sockaddr_in *sin);

#define	set2mask(setp) ((setp)->__sigbits[0])
#define	mask2set(mask, setp) \
	((mask) == -1 ? sigfillset(setp) : (((setp)->__sigbits[0]) = (mask)))

int
sigsetmask(int mask)
{
	sigset_t oset;
	sigset_t nset;

	(void) sigprocmask(0, (sigset_t *)0, &nset);
	mask2set(mask, &nset);
	(void) sigprocmask(SIG_SETMASK, &nset, &oset);
	return (set2mask(&oset));
}

int
sigblock(int mask)
{
	sigset_t oset;
	sigset_t nset;

	(void) sigprocmask(0, (sigset_t *)0, &nset);
	mask2set(mask, &nset);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);
	return (set2mask(&oset));
}

int
main(int argc, char *argv[])
{
	int omask;
	struct timeval *tvp, waittime;

	argv0 = argv;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(IPPORT_ROUTESERVER);
	s = getsocket(AF_INET, SOCK_DGRAM, &addr);
	if (s < 0)
		return (1);
	if ((iosoc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		return (1);
	}
	argv++, argc--;
	while (argc > 0 && **argv == '-') {
		if (strcmp(*argv, "-S") == 0) {
			save_space = 1;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-n") == 0) {
			install = 0;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-s") == 0) {
			maysupply = 1;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-q") == 0) {
			maysupply = 0;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-v") == 0) {
			argv++, argc--;
			tracing |= ACTION_BIT;
			continue;
		}
		if (strcmp(*argv, "-T") == 0) {
			deamon = 0;
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-t") == 0) {
			tracepackets++;
			deamon = 0;
			argv++, argc--;
			tracing |= INPUT_BIT;
			tracing |= OUTPUT_BIT;
			continue;
		}
		if (strcmp(*argv, "-d") == 0) {
			argv++, argc--;
			continue;
		}
		if (strcmp(*argv, "-g") == 0) {
			gateway = 1;
			argv++, argc--;
			continue;
		}
	usage:
		fprintf(stderr,
		    "usage: %s [ -s ] [ -q ] [ -t ] [-g] [-S] [-v] "
		    "[<logfile>]\n",
		    argv0[0]);
		return (1);
	}
	/*
	 * Any extra argument is considered
	 * a tracing log file.
	 */
	if (argc > 0) {
		traceon(*argv);
	} else if (tracing && !deamon) {
		traceonfp(stdout);
	} else if (tracing) {
		fprintf(stderr, "Need logfile with -v\n");
		goto usage;
	}

#ifndef DEBUG
	if (deamon) {
		int t;

		if (fork())
			return (0);
		for (t = 0; t < 20; t++)
			if ((t != s) && (t != iosoc) &&
			    (!tracing || (t != fileno(ftrace))))
				(void) close(t);
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		setsid();
	}
#endif
	openlog("in.routed", LOG_PID | LOG_CONS, LOG_DAEMON);

	(void) gettimeofday(&now, (struct timezone *)NULL);
	/*
	 * Collect an initial view of the world by
	 * checking the interface configuration and the gateway kludge
	 * file.  Then, send a request packet on all
	 * directly connected networks to find out what
	 * everyone else thinks.
	 */
	rtinit();
	gwkludge();
	ifinit();
	if (gateway > 0)
		rtdefault();
	msg->rip_cmd = RIPCMD_REQUEST;
	msg->rip_vers = RIPVERSION;
	msg->rip_nets[0].rip_dst.sa_family = AF_UNSPEC;
	msg->rip_nets[0].rip_metric = HOPCNT_INFINITY;
	msg->rip_nets[0].rip_dst.sa_family = htons(AF_UNSPEC);
	msg->rip_nets[0].rip_metric = htonl((unsigned)HOPCNT_INFINITY);
	toall(sendpacket, 0, (struct interface *)NULL);
	(void) sigset(SIGALRM, (void (*)(int))timer);
	(void) sigset(SIGHUP, (void (*)(int))ifinit);
	(void) sigset(SIGTERM, (void (*)(int))hup);
#ifdef DEBUG
	(void) sigset(SIGUSR1, if_dump);
	(void) sigset(SIGUSR2, rtdump);
#endif
	srand((uint_t)getpid());
	timer();

	for (;;) {
		fd_set ibits;
		register int n;

		FD_ZERO(&ibits);
		FD_SET(s, &ibits);
		/*
		 * If we need a dynamic update that was held off,
		 * needupdate will be set, and nextbcast is the time
		 * by which we want select to return.  Compute time
		 * until dynamic update should be sent, and select only
		 * until then.  If we have already passed nextbcast,
		 * just poll.
		 */
		if (needupdate) {
			waittime = nextbcast;
			timevalsub(&waittime, &now);
			if (waittime.tv_sec < 0) {
				waittime.tv_sec = 0;
				waittime.tv_usec = 0;
			}
			if (tracing & ACTION_BIT) {
				fprintf(ftrace,
				    "select until dynamic update %d/%d "
				    "sec/usec\n",
				    (int)waittime.tv_sec,
				    (int)waittime.tv_usec);
				fflush(ftrace);
			}
			tvp = &waittime;
		} else {
			tvp = (struct timeval *)NULL;
		}
		n = select(s + 1, &ibits, (fd_set *)0, (fd_set *)0,
		    tvp);
		if (n <= 0) {
			/*
			 * Need delayed dynamic update if select returned
			 * nothing and we timed out.  Otherwise, ignore
			 * errors (e.g. EINTR).
			 */
			if (n < 0) {
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "select: %m");
			}
			omask = sigblock(sigmask(SIGALRM));
			(void) sigblock(sigmask(SIGHUP));
			if (n == 0 && needupdate) {
				TRACE_ACTION("send delayed dynamic "
				    "update",
				    (struct rt_entry *)NULL);
				(void) gettimeofday(&now,
				    (struct timezone *)NULL);
				toall(supply, RTS_CHANGED,
				    (struct interface *)NULL);
				lastbcast = now;
				needupdate = 0;
				nextbcast.tv_sec = 0;
			}
			sigsetmask(omask);
			continue;
		}
		(void) gettimeofday(&now, (struct timezone *)NULL);
		omask = sigblock(sigmask(SIGALRM));
		(void) sigblock(sigmask(SIGHUP));
		if (FD_ISSET(s, &ibits))
			process(s);
		/* handle ICMP redirects */
		sigsetmask(omask);
	}
}

void
timevaladd(struct timeval *t1, struct timeval *t2)
{

	t1->tv_sec += t2->tv_sec;
	if ((t1->tv_usec += t2->tv_usec) > 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

void
timevalsub(struct timeval *t1, struct timeval *t2)
{

	t1->tv_sec -= t2->tv_sec;
	if ((t1->tv_usec -= t2->tv_usec) < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
}

static void
process(int fd)
{
	struct sockaddr from;
	socklen_t fromlen = (socklen_t)sizeof (from);
	int cc;


	for (;;) {
		cc = recvfrom(fd, packet, sizeof (packet), 0, &from, &fromlen);
		if (cc <= 0) {
			if (cc < 0 && errno != EWOULDBLOCK)
				perror("recvfrom");
			break;
		}
		if (fromlen != (socklen_t)sizeof (struct sockaddr_in))
			continue;
		rip_input(&from, cc);
	}
}

static int
getsocket(int domain, int type, struct sockaddr_in *sin)
{
	int s, on = 1;
	int recvsize = RCVBUFSIZ;

	if ((s = socket(domain, type, 0)) < 0) {
		perror("socket");
		return (-1);
	}
	/* In SunOS, you don't have to ask to use broadcast. */
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&on,
	    sizeof (int)) < 0) {
		perror("setsockopt SO_BROADCAST");
		close(s);
		return (-1);
	}
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&recvsize,
	    sizeof (int)) < 0) {
		perror("setsockopt SO_RCVBUF");
		close(s);
		return (-1);
	}
	if (bind(s, (struct sockaddr *)sin, sizeof (struct sockaddr_in)) < 0) {
		perror("bind");
		close(s);
		return (-1);
	}
	if (fcntl(s, F_SETFL, FNDELAY) == -1)
		syslog(LOG_ERR, "fcntl FNDELAY: %m\n");
	return (s);
}
