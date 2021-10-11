/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rdate.c	1.10	99/03/21 SMI"	/* SVr4.0 1.2	*/

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
 *		  All rights reserved.
 */


/*
 *  rdate - get date from remote machine
 *
 *	sets time, obtaining value from host
 *	on the tcp/time socket.  Since timeserver returns
 *	with time of day in seconds since Jan 1, 1900,  must
 *	subtract 86400(365*70 + 17) to get time
 *	since Jan 1, 1970, which is what get/settimeofday
 *	uses.
 */

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>

#define	TOFFSET	((unsigned long)86400*(365*70 + 17))
#define	WRITTEN 440199955	/* tv_sec when program was written */
#define	SECONDS_TO_MS	1000

static void timeout();

void
main(argc, argv)
	int argc;
	char **argv;
{
	struct sockaddr_in6 server;
	int s, i;
	ulong_t time;
	struct servent *sp;
	struct protoent *pp;
	struct hostent *hp;
	struct timeval timestruct;
	ulong_t	connect_timeout;
	/* number of seconds to wait for something to happen.  */
	int rdate_timeout = 30;
	int error_num;


	if (argc != 2) {
		(void) fprintf(stderr, "usage: rdate host\n");
		exit(EXIT_FAILURE);
	}
	bzero((char *)&server, sizeof (server));

	if (hp = getipnodebyname(argv[1], AF_INET6, AI_ALL | AI_ADDRCONFIG |
	    AI_V4MAPPED, &error_num)) {
		bcopy(hp->h_addr_list[0], &server.sin6_addr,
		    hp->h_length);
		server.sin6_family = hp->h_addrtype;
	} else {
		if (error_num == TRY_AGAIN) {
			printf("Host name %s not found (try again later)\n",
			    argv[1]);
		} else {
			printf("Host name %s not found\n", argv[1]);
		}
		exit(EXIT_FAILURE);
	}

	if ((sp = getservbyname("time", "tcp")) == NULL) {
		(void) fprintf(stderr,
		    "Sorry, time service not in services database\n");
		freehostent(hp);
		exit(EXIT_FAILURE);
	}
	if ((pp = getprotobyname("tcp")) == NULL) {
		(void) fprintf(stderr,
		    "Sorry, TCP protocol not in protocols database\n");
		freehostent(hp);
		exit(EXIT_FAILURE);
	}
	if ((s = socket(AF_INET6, SOCK_STREAM, pp->p_proto)) < 0) {
		perror("rdate: socket");
		freehostent(hp);
		exit(EXIT_FAILURE);
	}

	server.sin6_port = sp->s_port;

	connect_timeout = rdate_timeout * SECONDS_TO_MS;
	if (setsockopt(s, IPPROTO_TCP, TCP_CONN_ABORT_THRESHOLD,
	    (char *)&connect_timeout, sizeof (unsigned long)) == -1) {
		perror("setsockopt TCP_CONN_ABORT_THRESHOLD");
	}

	while (connect(s, (struct sockaddr *)&server, sizeof (server)) < 0) {

		if (hp->h_addr_list[1]) {
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0],
			    (caddr_t)&server.sin6_addr, hp->h_length);
			(void) close(s);
			s = socket(server.sin6_family, SOCK_STREAM, 0);
			if (s < 0) {
				perror("rdate: socket");
				freehostent(hp);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		perror("rdate: connect");
		close(s);
		freehostent(hp);
		exit(EXIT_FAILURE);
	}
	freehostent(hp);
	hp = NULL;

	(void) alarm(rdate_timeout);
	(void) signal(SIGALRM, (void (*)())timeout);

	if (read(s, (char *)&time, sizeof (int)) != sizeof (int)) {
		perror("rdate: read");
		exit(EXIT_FAILURE);
	}
	time = ntohl(time) - TOFFSET;
	/* date must be later than when program was written */
	if (time < WRITTEN) {
		(void) fprintf(stderr, "didn't get plausible time from %s\n",
		    argv[1]);
		exit(EXIT_FAILURE);
	}
	timestruct.tv_usec = 0;
	timestruct.tv_sec = time;
	i = settimeofday(&timestruct, 0);
	if (i == -1) {
		perror("couldn't set time of day");
		exit(EXIT_FAILURE);
	} else {
		(void) printf("%s", ctime(&timestruct.tv_sec));
#if defined(i386)
		system("/usr/sbin/rtc -c > /dev/null 2>&1");
#endif
	}
	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

static void
timeout()
{
	(void) fprintf(stderr, "couldn't contact time server\n");
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
