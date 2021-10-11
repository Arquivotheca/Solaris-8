
/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)spray.c 1.9	96/05/10 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include <rpcsvc/spray.h>
#include <sys/poll.h>

enum clnt_stat sprayproc_spray_1nd(/*argp, clnt*/);

#define	DEFBYTES	100000	/* default numbers of bytes to send */
#define	MAXPACKETLEN	1514
#define	SPRAYOVERHEAD	86	/* size of rpc packet when size=0 */

main(argc, argv)
	int argc;
	char **argv;
{
	int		c;
	extern char	*optarg;
	extern int	optind;
	register CLIENT *clnt;
	unsigned int	i;
	int		delay = 0;
	unsigned int	psec, bsec;
	int		buf[SPRAYMAX/4];
	char		msgbuf[256];
	unsigned int	lnth, cnt;
	sprayarr	arr;
	spraycumul	cumul;
	spraycumul	*co;
	char		*host = NULL;
	char		*type;

	if (argc < 2)
		usage();

	cnt = 0;
	lnth = SPRAYOVERHEAD;
	type = "netpath";
	while (optind < argc) {
		if (argv[optind][0] == '-') {
			if ((c = getopt(argc, argv, "d:c:t:l:")) == EOF) {
				break;
			}
			switch (c) {
				case 'd':
					delay = atoi(optarg);
					break;
				case 'c':
					cnt = (unsigned int) atoi(optarg);
					break;
				case 't':
					type = optarg;
					break;
				case 'l':
					lnth = (unsigned int) atoi(optarg);
					break;
				default:
					usage();
			}
		} else {
			host = argv[optind++];
		}
	}
	if (host == NULL)
		usage();
	clnt = clnt_create(host, SPRAYPROG, SPRAYVERS, type);
	if (clnt == (CLIENT *)NULL) {
		sprintf(msgbuf, "spray: cannot clnt_create %s:%s", host, type);
		clnt_pcreateerror(msgbuf);
		exit(1);
	}
	if (cnt == 0)
		cnt = DEFBYTES/lnth;
	if (lnth < SPRAYOVERHEAD)
		lnth = SPRAYOVERHEAD;
	else if (lnth >= SPRAYMAX)
		lnth = SPRAYMAX;
	if (lnth <= MAXPACKETLEN && lnth % 4 != 2)
		lnth = ((lnth + 5) / 4) * 4 - 2;
	arr.sprayarr_len = lnth - SPRAYOVERHEAD;
	arr.sprayarr_val = (char *) buf;
	printf("sending %u packets of length %u to %s ...", cnt, lnth, host);
	fflush(stdout);
	if (sprayproc_clear_1(NULL, clnt) == NULL) {
		clnt_perror(clnt, "SPRAYPROC_CLEAR ");
		exit(1);
	}
	for (i = 0; i < cnt; i++) {
		sprayproc_spray_1nd(&arr, clnt);
		if (delay > 0)
			slp(delay);
	}
	if ((co = sprayproc_get_1(NULL, clnt)) == NULL) {
		clnt_perror(clnt, "SPRAYPROC_GET ");
		exit(1);
	}
	cumul = *co;
	if (cumul.counter < cnt)
		printf("\n\t%d packets (%.3f%%) dropped by %s\n",
			cnt - cumul.counter,
			100.0 * (cnt - cumul.counter)/cnt, host);
	else
		printf("\n\tno packets dropped by %s\n", host);
	psec = (1000000.0 * cumul.counter)
		/ (1000000.0 * cumul.clock.sec + cumul.clock.usec);
	bsec = (lnth * 1000000.0 * cumul.counter)/
		(1000000.0 * cumul.clock.sec + cumul.clock.usec);
	printf("\t%u packets/sec, %u bytes/sec\n", psec, bsec);
	exit(0);
	/* NOTREACHED */
}

/*
 * A special call, where the TIMEOUT is 0. So, every call times-out.
 */
static struct timeval TIMEOUT = { 0, 0 };

enum clnt_stat
sprayproc_spray_1nd(argp, clnt)
	sprayarr *argp;
	CLIENT *clnt;
{
	return (clnt_call(clnt, SPRAYPROC_SPRAY, xdr_sprayarr, (caddr_t)argp,
			xdr_void, NULL, TIMEOUT));
}

/* A cheap milliseconds sleep call */
slp(usecs)
{
	static struct pollfd pfds[1] = {
		0, POLLIN, 0
	};
	pfds[0].fd = fileno(stdout);
	poll(pfds, 1, usecs/1000);
}

usage()
{
	printf("spray host [-t nettype] [-l lnth] [-c cnt] [-d delay]\n");
	exit(1);
}
