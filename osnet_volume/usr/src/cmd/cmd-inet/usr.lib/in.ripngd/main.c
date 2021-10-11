/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)main.c	1.2	99/11/07 SMI"	/* SVr4.0 1.1	*/

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
 * 	(c) 1986-1989,1991,1992,1997-1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

#include "defs.h"

struct		sockaddr_in6 allrouters;
char		*control;
boolean_t	dopoison = _B_TRUE;	/* Do poison reverse */
int		iocsoc;
struct		timeval lastfullupdate;	/* last time full table multicast */
struct		timeval lastmcast;	/* last time all/changes multicast */
int		max_poll_ifs = START_POLL_SIZE;
struct		rip6 *msg;
boolean_t	needupdate;		/* true if need update at nextmcast */
struct		timeval nextmcast;	/* time to wait before changes mcast */
struct		timeval now;		/* current idea of time */
char		*packet;
struct		pollfd *poll_ifs = NULL;
int		poll_ifs_num = 0;
int		rip6_port;
boolean_t	supplier = _B_TRUE;	/* process should supply updates */

struct		in6_addr allrouters_in6 = {
/* BEGIN CSTYLED */
    { 0xff, 0x2, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x9 }
/* END CSTYLED */
};

static void	timevalsub(struct timeval *t1, struct timeval *t2);

static void
usage(char *fname)
{
	(void) fprintf(stderr,
	    "usage: "
	    "%s [ -P ] [ -p port ] [ -q ] [ -s ] [ -t ] [ -v ] [<logfile>]\n",
	    fname);
	exit(EXIT_FAILURE);
}

void
main(int argc, char *argv[])
{
	int i, n;
	struct interface *ifp;
	int c;
	struct timeval waittime;
	int timeout;
	boolean_t daemon = _B_TRUE;	/* Fork off a detached daemon */

	rip6_port = htons(IPPORT_ROUTESERVER6);
	allrouters.sin6_family = AF_INET6;
	allrouters.sin6_port = rip6_port;
	allrouters.sin6_addr = allrouters_in6;

	while ((c = getopt(argc, argv, "nsqvTtdgPp:")) != EOF) {
		switch (c) {
		case 'n':
			install = _B_FALSE;
			break;
		case 's':
			supplier = _B_TRUE;
			break;
		case 'q':
			supplier = _B_FALSE;
			break;
		case 'v':
			tracing |= ACTION_BIT;
			break;
		case 'T':
			daemon = _B_FALSE;
			break;
		case 't':
			tracepackets = _B_TRUE;
			daemon = _B_FALSE;
			tracing |= (INPUT_BIT | OUTPUT_BIT);
			break;
		case 'd':
			break;
		case 'P':
			dopoison = _B_FALSE;
			break;
		case 'p':
			rip6_port = htons(atoi(optarg));
			allrouters.sin6_port = rip6_port;
			break;
		default:
			usage(argv[0]);
			/* NOTREACHED */
		}
	}

	/*
	 * Any extra argument is considered
	 * a tracing log file.
	 */
	if (optind < argc) {
		traceon(argv[optind]);
	} else if (tracing && !daemon) {
		traceonfp(stdout);
	} else if (tracing) {
		(void) fprintf(stderr, "Need logfile with -v\n");
		usage(argv[0]);
		/* NOTREACHED */
	}

	if (daemon) {
		int t;

		if (fork())
			exit(EXIT_SUCCESS);
		for (t = 0; t < 20; t++) {
			if (!tracing || (t != fileno(ftrace)))
				(void) close(t);
		}
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		(void) setsid();
	}

	iocsoc = socket(AF_INET6, SOCK_DGRAM, 0);
	if (iocsoc < 0) {
		syslog(LOG_ERR, "main: socket: %m");
		exit(EXIT_FAILURE);
	}

	setup_rtsock();

	/*
	 * Allocate the buffer to hold the RIPng packet.  In reality, it will be
	 * smaller than IPV6_MAX_PACKET octets due to (at least) the IPv6 and
	 * UDP headers but IPV6_MAX_PACKET is a convenient size.
	 */
	packet = (char *)malloc(IPV6_MAX_PACKET);
	if (packet == NULL) {
		syslog(LOG_ERR, "main: malloc: %m");
		exit(EXIT_FAILURE);
	}
	msg = (struct rip6 *)packet;

	/*
	 * Allocate the buffer to hold the ancillary data.  This data is used to
	 * insure that the incoming hop count of a RIPCMD6_RESPONSE message is
	 * IPV6_MAX_HOPS which indicates that it came from a direct neighbor
	 * (namely, no intervening router decremented it).
	 */
	control = (char *)malloc(IPV6_MAX_PACKET);
	if (control == NULL) {
		syslog(LOG_ERR, "main: malloc: %m");
		exit(EXIT_FAILURE);
	}

	openlog("in.ripngd", LOG_PID | LOG_CONS, LOG_DAEMON);

	(void) gettimeofday(&now, (struct timezone *)NULL);

	initifs();
	solicitall(&allrouters);

	if (supplier)
		supplyall(&allrouters, 0, (struct interface *)NULL, _B_TRUE);

	(void) sigset(SIGALRM, (void (*)(int))timer);
	(void) sigset(SIGHUP, (void (*)(int))initifs);
	(void) sigset(SIGTERM, (void (*)(int))term);
	(void) sigset(SIGUSR1, (void (*)(int))if_dump);
	(void) sigset(SIGUSR2, (void (*)(int))rtdump);

	/*
	 * Seed the pseudo-random number generator for GET_RANDOM().
	 */
	srandom((uint_t)gethostid());

	timer();

	for (;;) {
		if (needupdate) {
			waittime = nextmcast;
			timevalsub(&waittime, &now);
			if (waittime.tv_sec < 0) {
				timeout = 0;
			} else {
				timeout = TIME_TO_MSECS(waittime);
			}
			if (tracing & ACTION_BIT) {
				(void) fprintf(ftrace,
				    "poll until dynamic update in %d msec\n",
				    timeout);
				(void) fflush(ftrace);
			}
		} else {
			timeout = INFTIM;
		}

		if ((n = poll(poll_ifs, poll_ifs_num, timeout)) < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "main: poll: %m");
			exit(EXIT_FAILURE);
		}
		(void) sighold(SIGALRM);
		(void) sighold(SIGHUP);
		/*
		 * Poll timed out.
		 */
		if (n == 0) {
			if (needupdate) {
				TRACE_ACTION("send delayed dynamic update",
				    (struct rt_entry *)NULL);
				(void) gettimeofday(&now,
				    (struct timezone *)NULL);
				supplyall(&allrouters, RTS_CHANGED,
				    (struct interface *)NULL, _B_TRUE);
				lastmcast = now;
				needupdate = _B_FALSE;
				nextmcast.tv_sec = 0;
			}
			(void) sigrelse(SIGHUP);
			(void) sigrelse(SIGALRM);
			continue;
		}
		(void) gettimeofday(&now, (struct timezone *)NULL);
		for (i = 0; i < poll_ifs_num; i++) {
			/*
			 * This case should never happen.
			 */
			if (poll_ifs[i].revents & POLLERR) {
				syslog(LOG_ERR,
				    "main: poll returned a POLLERR event");
				continue;
			}
			if (poll_ifs[i].revents & POLLIN) {
				for (ifp = ifnet; ifp != NULL;
				    ifp = ifp->int_next) {
					if (poll_ifs[i].fd == ifp->int_sock)
						in_data(ifp);
				}
			}
		}
		(void) sigrelse(SIGHUP);
		(void) sigrelse(SIGALRM);
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
