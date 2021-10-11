/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *
 * Copyright (c) 1983, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)route.c	8.6 (Berkeley) 4/28/95
 *	@(#)linkaddr.c	8.1 (Berkeley) 6/4/93
 */

#pragma ident	"@(#)route.c	1.41	99/11/07 SMI"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stream.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/timod.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inet/common.h>
#include <inet/mib2.h>
#include <netinet/ip6.h>
#include <inet/ip.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <fcntl.h>

static struct keytab {
	char	*kt_cp;
	int	kt_i;
} keywords[] = {
#define	K_ADD		1
	{"add",		K_ADD},
#define	K_BLACKHOLE	2
	{"blackhole",	K_BLACKHOLE},
#define	K_CHANGE	3
	{"change",	K_CHANGE},
#define	K_CLONING	4
	{"cloning",	K_CLONING},
#define	K_DELETE	5
	{"delete",	K_DELETE},
#define	K_DST		6
	{"dst",		K_DST},
#define	K_EXPIRE	7
	{"expire",	K_EXPIRE},
#define	K_FLUSH		8
	{"flush",	K_FLUSH},
#define	K_GATEWAY	9
	{"gateway",	K_GATEWAY},
#define	K_GENMASK	10
	{"genmask",	K_GENMASK},
#define	K_GET		11
	{"get",		K_GET},
#define	K_HOPCOUNT	12
	{"hopcount",	K_HOPCOUNT},
#define	K_HOST		13
	{"host",	K_HOST},
#define	K_IFA		14
	{"ifa",		K_IFA},
#define	K_IFACE		15
	{"iface",	K_IFACE},
#define	K_IFP		16
	{"ifp",		K_IFP},
#define	K_INET		17
	{"inet",	K_INET},
#define	K_INET6		18
	{"inet6",	K_INET6},
#define	K_INTERFACE	19
	{"interface",	K_INTERFACE},
#define	K_LINK		20
	{"link",	K_LINK},
#define	K_LOCK		21
	{"lock",	K_LOCK},
#define	K_LOCKREST	22
	{"lockrest",	K_LOCKREST},
#define	K_MASK		23
	{"mask",	K_MASK},
#define	K_MONITOR	24
	{"monitor",	K_MONITOR},
#define	K_MTU		25
	{"mtu",		K_MTU},
#define	K_NET		26
	{"net",		K_NET},
#define	K_NETMASK	27
	{"netmask",	K_NETMASK},
#define	K_NOSTATIC	28
	{"nostatic",	K_NOSTATIC},
#define	K_PRIVATE	29
	{"private",	K_PRIVATE},
#define	K_PROTO1	30
	{"proto1",	K_PROTO1},
#define	K_PROTO2	31
	{"proto2",	K_PROTO2},
#define	K_RECVPIPE	32
	{"recvpipe",	K_RECVPIPE},
#define	K_REJECT	33
	{"reject",	K_REJECT},
#define	K_RTT		34
	{"rtt",		K_RTT},
#define	K_RTTVAR	35
	{"rttvar",	K_RTTVAR},
#define	K_SA		36
	{"sa",		K_SA},
#define	K_SENDPIPE	37
	{"sendpipe",	K_SENDPIPE},
#define	K_SSTHRESH	38
	{"ssthresh",	K_SSTHRESH},
#define	K_STATIC	39
	{"static",	K_STATIC},
#define	K_XRESOLVE	40
	{"xresolve",	K_XRESOLVE},
	{0, 0}
};

static union	sockunion {
	struct	sockaddr sa;
	struct	sockaddr_in sin;
	struct	sockaddr_dl sdl;
	struct	sockaddr_in6 sin6;
} so_dst, so_gate, so_mask, so_genmask, so_ifa, so_ifp;

typedef struct	mib_item_s {
	struct mib_item_s	*next_item;
	long			group;
	long			mib_id;
	long			length;
	char			*valp;
} mib_item_t;

typedef union sockunion *sup;

static void		bprintf(FILE *fp, int b, char *s);
static void		delRouteEntry(void *valp, int seqno);
static void		flushroutes(int argc, char *argv[]);
static boolean_t	getaddr(int which, char *s, struct hostent **hpp);
static boolean_t	in6_getaddr(char *s, struct sockaddr *saddr,
    int *plenp, struct hostent **hpp);
static boolean_t	in_getaddr(char *s, struct sockaddr *saddr, int *plenp,
    int which, struct hostent **hpp);
static int		in_getprefixlen(char *addr, boolean_t slash,
    int max_plen);
static boolean_t	in_prefixlentomask(int prefixlen, int maxlen,
    uchar_t *mask);
static void		inet_makenetandmask(ulong_t net,
    struct sockaddr_in *sin);
static ulong_t		inet_makesubnetmask(ulong_t addr, ulong_t mask);
static int		keyword(char *cp);
static void		link_addr(const char *addr, struct sockaddr_dl *sdl);
static char		*link_ntoa(const struct sockaddr_dl *sdl);
static mib_item_t	*mibget(int sd);
static char		*netname(struct sockaddr *sa);
static void		newroute(int argc, char **argv);
static void		pmsg_addrs(char *cp, int addrs);
static void		pmsg_common(struct rt_msghdr *rtm);
static void		print_getmsg(struct rt_msghdr *rtm, int msglen);
static void		print_rtmsg(struct rt_msghdr *rtm, int msglen);
static void		quit(char *s);
static char		*routename(struct sockaddr *sa);
static void		rtmonitor(int argc, char *argv[]);
static int		rtmsg(int cmd, int flags);
static int		salen(struct sockaddr *sa);
static void		set_metric(char *value, int key);
static void		sockaddr(char *addr, struct sockaddr *sa);
static void		sodump(sup su, char *which);
static void		usage(char *cp);

static int		pid, rtm_addrs, uid;
static int		s;
static boolean_t	forcehost, forcenet, nflag;
static int		af = AF_INET;
static boolean_t	qflag, tflag;
static boolean_t	iflag, verbose;
static boolean_t	locking, lockrest, debugonly;
static boolean_t	fflag;
static struct		rt_metrics rt_metrics;
static ulong_t		rtm_inits;
static int		masklen;

static struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;

/*
 * Sizes of data structures extracted from the base mib.
 * This allows the size of the tables entries to grow while preserving
 * binary compatibility.
 */
static int ipRouteEntrySize;
static int ipv6RouteEntrySize;

#define	ROUNDUP_LONG(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof (long) - 1))) : sizeof (long))
#define	ADVANCE(x, n) ((x) += ROUNDUP_LONG(salen(n)))
#define	C(x)	((x) & 0xff)

/*
 * return values from in_getprefixlen()
 */
#define	BAD_ADDR	-1	/* prefix is invalid */
#define	NO_PREFIX	-2	/* no prefix was found */

void
usage(char *cp)
{
	if (cp != NULL)
		(void) fprintf(stderr, "route: botched keyword: %s\n", cp);
	(void) fprintf(stderr,
	    "usage: route [ -fnqv ] cmd [[ -<qualifers> ] args ]\n");
	exit(1);
	/* NOTREACHED */
}

void
quit(char *s)
{
	int sverrno = errno;

	(void) fprintf(stderr, "route: ");
	if (s != NULL)
		(void) fprintf(stderr, "%s: ", s);
	(void) fprintf(stderr, "%s\n", strerror(sverrno));
	exit(1);
	/* NOTREACHED */
}

int
main(int argc, char **argv)
{
	extern int optind;
	int ch;
	int key;

	if (argc < 2)
		usage((char *)NULL);

	while ((ch = getopt(argc, argv, "nqdtvf")) != EOF) {
		switch (ch) {
		case 'n':
			nflag = B_TRUE;
			break;
		case 'q':
			qflag = B_TRUE;
			break;
		case 'v':
			verbose = B_TRUE;
			break;
		case 't':
			tflag = B_TRUE;
			break;
		case 'd':
			debugonly = B_TRUE;
			break;
		case 'f':
			fflag = B_TRUE;
			break;
		case '?':
		default:
			usage((char *)NULL);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	pid = getpid();
	uid = getuid();
	if (tflag)
		s = open("/dev/null", O_WRONLY, 0);
	else
		s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		quit("socket");
	if (fflag) {
		/*
		 * Accept an address family keyword after the -f.  Since the
		 * default address family is AF_INET, reassign af only for the
		 * other valid address families.
		 */
		if (*argv != NULL) {
			switch (key = keyword(*argv)) {
			case K_INET:
			case K_INET6:
				if (key == K_INET6)
					af = AF_INET6;
				/* Skip over the address family parameter. */
				argc--;
				argv++;
				break;
			}
		}
		flushroutes(0, NULL);
	}
	if (*argv != NULL) {
		switch (keyword(*argv)) {
		case K_GET:
		case K_CHANGE:
		case K_ADD:
		case K_DELETE:
			newroute(argc, argv);
			exit(0);
			/* NOTREACHED */

		case K_MONITOR:
			rtmonitor(argc, argv);
			/* NOTREACHED */

		case K_FLUSH:
			flushroutes(argc, argv);
			exit(0);
			/* NOTREACHED */
		}
	}
	if (!fflag)
		usage(*argv);
	return (0);
}

/*
 * Purge all entries in the routing tables not
 * associated with network interfaces.
 */
void
flushroutes(int argc, char *argv[])
{
	int seqno;
	int sd;	/* mib stream */
	mib_item_t	*item;
	mib2_ipRouteEntry_t *rp;
	mib2_ipv6RouteEntry_t *rp6;
	int off = 0;
	int on = 1;

	if (uid != 0) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
	if (setsockopt(s, SOL_SOCKET, SO_USELOOPBACK, (char *)&off,
	    sizeof (off)) < 0)
		quit("setsockopt");
	if (argc > 1) {
		argv++;
		if (argc == 2 && **argv == '-') {
			/*
			 * The address family (preceded by a dash) may be used
			 * to flush the routes of that particular family.
			 */
			switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
			case K_LINK:
				af = AF_LINK;
				break;
			case K_INET6:
				af = AF_INET6;
				break;
			default:
				usage(*argv);
				/* NOTREACHED */
			}
		} else {
			usage(*argv);
		}
	}
	sd = open("/dev/ip", O_RDWR);
	if (sd < 0)
		quit("can't open mib stream");
	if ((item = mibget(sd)) == NULL)
		quit("mibget");
	if (verbose) {
		(void) printf("Examining routing table from "
		    "T_SVR4_OPTMGMT_REQ\n");
	}
	seqno = 0;		/* ??? */
	switch (af) {
	case AF_INET:
		/* Extract ipRouteEntrySize */
		for (; item != NULL; item = item->next_item) {
			if (item->mib_id != 0)
				continue;
			if (item->group == MIB2_IP) {
				ipRouteEntrySize =
				    ((mib2_ip_t *)item->valp)->ipRouteEntrySize;
				break;
			}
		}
		if (ipRouteEntrySize == 0) {
			(void) fprintf(stderr,
			    "ipRouteEntrySize cannot be determined.\n");
			exit(1);
		}
		for (; item != NULL; item = item->next_item) {
			/*
			 * skip all the other trash that comes up the mib stream
			 */
			if (item->group != MIB2_IP ||
			    item->mib_id != MIB2_IP_ROUTE)
				continue;
			for (rp = (mib2_ipRouteEntry_t *)item->valp;
			    (char *)rp < item->valp + item->length;
			    rp = (mib2_ipRouteEntry_t *)
				((char *)rp + ipRouteEntrySize)) {
				delRouteEntry(rp, seqno);
				seqno++;
			}
			break;
		}
		break;
	case AF_INET6:
		/* Extract ipv6RouteEntrySize */
		for (; item != NULL; item = item->next_item) {
			if (item->mib_id != 0)
				continue;
			if (item->group == MIB2_IP6) {
				ipv6RouteEntrySize =
				    ((mib2_ipv6IfStatsEntry_t *)item->valp)->
					ipv6RouteEntrySize;
				break;
			}
		}
		if (ipv6RouteEntrySize == 0) {
			(void) fprintf(stderr,
			    "ipv6RouteEntrySize cannot be determined.\n");
			exit(1);
		}
		for (; item != NULL; item = item->next_item) {
			/*
			 * skip all the other trash that comes up the mib stream
			 */
			if (item->group != MIB2_IP6 ||
			    item->mib_id != MIB2_IP6_ROUTE)
				continue;
			for (rp6 = (mib2_ipv6RouteEntry_t *)item->valp;
			    (char *)rp6 < item->valp + item->length;
			    rp6 = (mib2_ipv6RouteEntry_t *)
				((char *)rp6 + ipv6RouteEntrySize)) {
				delRouteEntry(rp6, seqno);
				seqno++;
			}
			break;
		}
		break;
	}

	if (setsockopt(s, SOL_SOCKET, SO_USELOOPBACK, (char *)&on,
	    sizeof (on)) < 0)
		quit("setsockopt");
}

/*
 * Given the contents of a mib_item_t of id type MIB2_IP_ROUTE or
 * MIB2_IP6_ROUTE, construct and send an RTM_DELETE routing socket message in
 * order to facilitate the flushing of RTF_GATEWAY routes.
 */
void
delRouteEntry(void *valp, int seqno)
{
	char *cp;
	int ire_type;
	int rlen;
	mib2_ipRouteEntry_t *rp;
	mib2_ipv6RouteEntry_t *rp6;
	struct rt_msghdr *rtm;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	int slen;

	switch (af) {
	case AF_INET:
		rp = (mib2_ipRouteEntry_t *)valp;
		ire_type = rp->ipRouteInfo.re_ire_type;
		break;
	case AF_INET6:
		rp6 = (mib2_ipv6RouteEntry_t *)valp;
		ire_type = rp6->ipv6RouteInfo.re_ire_type;
		break;
	}
	if (ire_type != IRE_DEFAULT &&
	    ire_type != IRE_PREFIX &&
	    ire_type != IRE_HOST &&
	    ire_type != IRE_HOST_REDIRECT)
		return;

	rtm = &m_rtmsg.m_rtm;
	(void) memset(rtm, 0, sizeof (m_rtmsg));
	rtm->rtm_type = RTM_DELETE;
	rtm->rtm_seq = seqno;
	rtm->rtm_flags |= RTF_GATEWAY;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	cp = m_rtmsg.m_space;
	switch (af) {
	case AF_INET:
		slen = sizeof (struct sockaddr_in);
		if (rp->ipRouteMask == IP_HOST_MASK)
			rtm->rtm_flags |= RTF_HOST;
		(void) memset(&sin, 0, slen);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = rp->ipRouteDest;
		(void) memmove((void *) cp, (void *) &sin, slen);
		cp += slen;
		sin.sin_addr.s_addr = rp->ipRouteNextHop;
		(void) memmove((void *) cp, (void *) &sin, slen);
		cp += slen;
		sin.sin_addr.s_addr = rp->ipRouteMask;
		(void) memmove((void *) cp, (void *) &sin, slen);
		cp += slen;
		break;
	case AF_INET6:
		slen = sizeof (struct sockaddr_in6);
		if (rp6->ipv6RoutePfxLength == IPV6_ABITS)
			rtm->rtm_flags |= RTF_HOST;
		(void) memset(&sin6, 0, slen);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = rp6->ipv6RouteDest;
		(void) memmove((void *) cp, (void *) &sin6, slen);
		cp += slen;
		sin6.sin6_addr = rp6->ipv6RouteNextHop;
		(void) memmove((void *) cp, (void *) &sin6, slen);
		cp += slen;
		(void) memset(&sin6.sin6_addr, 0, sizeof (sin6.sin6_addr));
		(void) in_prefixlentomask(rp6->ipv6RoutePfxLength, IPV6_ABITS,
		    (uchar_t *)&sin6.sin6_addr.s6_addr);
		(void) memmove((void *) cp, (void *) &sin6, slen);
		cp += slen;
		break;
	}
	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
	if (debugonly) {
		/*
		 * In debugonly mode, the routing socket message to delete the
		 * current entry is not actually sent.  However if verbose is
		 * also set, the routing socket message that would have been
		 * is printed.
		 */
		if (verbose)
			print_rtmsg(rtm, rtm->rtm_msglen);
		return;
	}

	rlen = write(s, (char *)&m_rtmsg, rtm->rtm_msglen);
	if (rlen < (int)rtm->rtm_msglen) {
		if (rlen < 0) {
			(void) fprintf(stderr,
			    "route: write to routing socket: %s\n",
			    strerror(errno));
		} else {
			(void) fprintf(stderr,
			    "route: write to routing socket "
			    "got only %d for rlen\n", rlen);
		}
		return;
	}
	if (qflag) {
		/*
		 * In quiet mode, nothing is printed at all (unless the write()
		 * itself failed.
		 */
		return;
	}
	if (verbose) {
		print_rtmsg(rtm, rlen);
	} else {
		struct sockaddr *sa = (struct sockaddr *)(rtm + 1);

		(void) printf("%-20.20s ",
		    rtm->rtm_flags & RTF_HOST ? routename(sa) :
			netname(sa));
		sa = (struct sockaddr *)(salen(sa) + (char *)sa);
		(void) printf("%-20.20s ", routename(sa));
		(void) printf("done\n");
	}
}

/*
 * Return the name of the host whose address is given.
 */
char *
routename(struct sockaddr *sa)
{
	char *cp;
	static char line[MAXHOSTNAMELEN + 1];
	struct hostent *hp = NULL;
	static char domain[MAXHOSTNAMELEN + 1];
	static boolean_t first = B_TRUE;
	struct in_addr in;
	struct in6_addr in6;
	int error_num;
	ushort_t *s;
	ushort_t *slim;

	if (first) {
		first = B_FALSE;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}

	if (salen(sa) == 0) {
		(void) strcpy(line, "default");
		return (line);
	}
	switch (sa->sa_family) {

	case AF_INET:
		in = ((struct sockaddr_in *)sa)->sin_addr;

		cp = NULL;
		if (in.s_addr == INADDR_ANY)
			cp = "default";
		if (cp == NULL && !nflag) {
			hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
				AF_INET);
			if (hp != NULL) {
				if (((cp = strchr(hp->h_name, '.')) != NULL) &&
				    (strcmp(cp + 1, domain) == 0))
					*cp = 0;
				cp = hp->h_name;
			}
		}
		if (cp != NULL) {
			(void) strncpy(line, cp, MAXHOSTNAMELEN);
			line[MAXHOSTNAMELEN] = '\0';
		} else {
			in.s_addr = ntohl(in.s_addr);
			(void) sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8),
			    C(in.s_addr));
		}
		break;

	case AF_LINK:
		return (link_ntoa((struct sockaddr_dl *)sa));

	case AF_INET6:
		in6 = ((struct sockaddr_in6 *)sa)->sin6_addr;

		cp = NULL;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6))
			cp = "default";
		if (cp == NULL && !nflag) {
			hp = getipnodebyaddr((char *)&in6,
				sizeof (struct in6_addr), AF_INET6, &error_num);
			if (hp != NULL) {
				if (((cp = strchr(hp->h_name, '.')) != NULL) &&
				    (strcmp(cp + 1, domain) == 0))
					*cp = 0;
				cp = hp->h_name;
			}
		}
		if (cp != NULL) {
			(void) strncpy(line, cp, MAXHOSTNAMELEN);
			line[MAXHOSTNAMELEN] = '\0';
		} else {
			(void) inet_ntop(AF_INET6, (void *)&in6, line,
			    INET6_ADDRSTRLEN);
		}
		if (hp != NULL)
			freehostent(hp);

		break;

	default:
		s = (ushort_t *)sa;

		slim = s + ((salen(sa) + 1) >> 1);
		cp = line + sprintf(line, "(%d)", sa->sa_family);

		while (++s < slim) /* start with sa->sa_data */
			cp += sprintf(cp, " %x", *s);
		break;
	}
	return (line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(struct sockaddr *sa)
{
	char *cp = NULL;
	static char line[MAXHOSTNAMELEN + 1];
	struct netent *np = NULL;
	ulong_t net, mask;
	ulong_t i;
	int subnetshift;
	struct in_addr in;
	ushort_t *s;
	ushort_t *slim;

	switch (sa->sa_family) {

	case AF_INET:
		in = ((struct sockaddr_in *)sa)->sin_addr;

		i = in.s_addr = ntohl(in.s_addr);
		if (in.s_addr == INADDR_ANY) {
			cp = "default";
		} else if (!nflag) {
			if (IN_CLASSA(i)) {
				mask = IN_CLASSA_NET;
				subnetshift = 8;
			} else if (IN_CLASSB(i)) {
				mask = IN_CLASSB_NET;
				subnetshift = 8;
			} else {
				mask = IN_CLASSC_NET;
				subnetshift = 4;
			}
			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 */
			while (in.s_addr &~ mask)
				mask = (long)mask >> subnetshift;
			net = in.s_addr & mask;
			while ((mask & 1) == 0)
				mask >>= 1, net >>= 1;
			np = getnetbyaddr(net, AF_INET);
			if (np != NULL)
				cp = np->n_name;
		}
		if (cp != NULL) {
			(void) strncpy(line, cp, MAXHOSTNAMELEN);
			line[MAXHOSTNAMELEN] = '\0';
		} else if ((in.s_addr & 0xffffff) == 0) {
			(void) sprintf(line, "%u", C(in.s_addr >> 24));
		} else if ((in.s_addr & 0xffff) == 0) {
			(void) sprintf(line, "%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16));
		} else if ((in.s_addr & 0xff) == 0) {
			(void) sprintf(line, "%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8));
		} else {
			(void) sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8),
			    C(in.s_addr));
		}
		break;

	case AF_LINK:
		return (link_ntoa((struct sockaddr_dl *)sa));

	case AF_INET6:
		return (routename(sa));

	default:
		s = (ushort_t *)sa->sa_data;

		slim = s + ((salen(sa) + 1) >> 1);
		cp = line + sprintf(line, "af %d:", sa->sa_family);

		while (s < slim)
			cp += sprintf(cp, " %x", *s++);
		break;
	}
	return (line);
}

void
set_metric(char *value, int key)
{
	int flag = 0;
	uint_t noval, *valp = &noval;

	switch (key) {
#define	caseof(x, y, z)	case (x): valp = &rt_metrics.z; flag = (y); break
	caseof(K_MTU, RTV_MTU, rmx_mtu);
	caseof(K_HOPCOUNT, RTV_HOPCOUNT, rmx_hopcount);
	caseof(K_EXPIRE, RTV_EXPIRE, rmx_expire);
	caseof(K_RECVPIPE, RTV_RPIPE, rmx_recvpipe);
	caseof(K_SENDPIPE, RTV_SPIPE, rmx_sendpipe);
	caseof(K_SSTHRESH, RTV_SSTHRESH, rmx_ssthresh);
	caseof(K_RTT, RTV_RTT, rmx_rtt);
	caseof(K_RTTVAR, RTV_RTTVAR, rmx_rttvar);
#undef	caseof
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = B_FALSE;
	*valp = atoi(value);
}

void
newroute(int argc, char **argv)
{
	char *cmd, *dest = "", *gateway = "", *err;
	boolean_t ishost = B_FALSE;
	int ret, attempts, oerrno, flags = RTF_STATIC;
	int key;
	struct hostent *hp = NULL;
	static char obuf[INET6_ADDRSTRLEN];

	cmd = argv[0];
	if (*cmd != 'g') {
		if (uid != 0) {
			errno = EACCES;
			quit("must be root to alter routing table");
		}
		/* Don't want to read back our messages */
		(void) shutdown(s, 0);
	}
	while (--argc > 0) {
		key = keyword(*(++argv));
		if (key == K_HOST) {
			forcehost = B_TRUE;
		} else if (key == K_NET) {
			forcenet = B_TRUE;
		} else if (**(argv) == '-') {
			switch (key = keyword(1 + *argv)) {
			case K_LINK:
				af = AF_LINK;
				break;
			case K_INET:
				af = AF_INET;
				break;
			case K_SA:
				af = PF_ROUTE;
				break;
			case K_INET6:
				af = AF_INET6;
				break;
			case K_IFACE:
			case K_INTERFACE:
				iflag = B_TRUE;
				/* FALLTHROUGH */
			case K_NOSTATIC:
				flags &= ~RTF_STATIC;
				break;
			case K_LOCK:
				locking = B_TRUE;
				break;
			case K_LOCKREST:
				lockrest = B_TRUE;
				break;
			case K_HOST:
				forcehost = B_TRUE;
				break;
			case K_REJECT:
				flags |= RTF_REJECT;
				break;
			case K_BLACKHOLE:
				flags |= RTF_BLACKHOLE;
				break;
			case K_PROTO1:
				flags |= RTF_PROTO1;
				break;
			case K_PROTO2:
				flags |= RTF_PROTO2;
				break;
			case K_CLONING:
				flags |= RTF_CLONING;
				break;
			case K_XRESOLVE:
				flags |= RTF_XRESOLVE;
				break;
			case K_STATIC:
				flags |= RTF_STATIC;
				break;
			case K_IFA:
				argc--;
				(void) getaddr(RTA_IFA, *++argv, NULL);
				break;
			case K_IFP:
				argc--;
				(void) getaddr(RTA_IFP, *++argv, NULL);
				break;
			case K_GENMASK:
				argc--;
				(void) getaddr(RTA_GENMASK, *++argv, NULL);
				break;
			case K_GATEWAY:
				/*
				 * For the gateway parameter, retrieve the
				 * pointer to the struct hostent so that all
				 * possible addresses can be tried until one
				 * is successful.
				 */
				argc--;
				(void) getaddr(RTA_GATEWAY, *++argv, &hp);
				break;
			case K_DST:
				argc--;
				ishost = getaddr(RTA_DST, *++argv, NULL);
				dest = *argv;
				break;
			case K_NETMASK:
				argc--;
				(void) getaddr(RTA_NETMASK, *++argv, NULL);
				/* FALLTHROUGH */
			case K_NET:
				forcenet = B_TRUE;
				break;
			case K_MTU:
			case K_HOPCOUNT:
			case K_EXPIRE:
			case K_RECVPIPE:
			case K_SENDPIPE:
			case K_SSTHRESH:
			case K_RTT:
			case K_RTTVAR:
				argc--;
				set_metric(*++argv, key);
				break;
			case K_PRIVATE:
				flags |= RTF_PRIVATE;
				break;
			default:
				usage(*argv + 1);
				/* NOTREACHED */
			}
		} else {
			if ((rtm_addrs & RTA_DST) == 0) {
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, NULL);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				/*
				 * For the gateway parameter, retrieve the
				 * pointer to the struct hostent so that all
				 * possible addresses can be tried until one
				 * is successful.
				 */
				gateway = *argv;
				(void) getaddr(RTA_GATEWAY, *argv, &hp);
			} else {
				int ret = atoi(*argv);

				/*
				 * Assume that small numbers are metric
				 * Needed for compatibility with old route
				 * command syntax.
				 */
				if (ret == 0) {
					if (strcmp(*argv, "0") != 0)
						usage((char *)NULL);
					if (verbose) {
						(void) printf("old usage of "
						    "trailing 0, assuming "
						    "route to if\n");
					}
					iflag = B_TRUE;
					continue;
				} else if (ret > 0 && ret < 10) {
					if (verbose) {
						(void) printf("old usage of "
						    "trailing digit, assuming "
						    "route via gateway\n");
					}
					iflag = B_FALSE;
					continue;
				}
				(void) getaddr(RTA_NETMASK, *argv, NULL);
			}
		}
	}
	if ((rtm_addrs & RTA_DST) == 0) {
		(void) fprintf(stderr,
		    "route: destination required following command\n");
		usage((char *)NULL);
	} else if ((*cmd == 'a' || *cmd == 'd') &&
	    (rtm_addrs & RTA_GATEWAY) == 0) {
		(void) fprintf(stderr,
		    "route: gateway required for add or delete command\n");
		usage((char *)NULL);
	}

	/*
	 * If the netmask has been specified use it to determine RTF_HOST.
	 * Otherwise rely on the "-net" and "-host" specifiers.
	 * Final fallback is whether ot not any bits were set in the address
	 * past the classful network component.
	 */
	if (rtm_addrs & RTA_NETMASK) {
		if ((af == AF_INET &&
			so_mask.sin.sin_addr.s_addr == IP_HOST_MASK) ||
		    (af == AF_INET6 && masklen == IPV6_ABITS))
			forcehost = B_TRUE;
		else
			forcenet = B_TRUE;
	}
	if (forcehost)
		ishost = B_TRUE;
	if (forcenet)
		ishost = B_FALSE;
	flags |= RTF_UP;
	if (ishost)
		flags |= RTF_HOST;
	if (!iflag)
		flags |= RTF_GATEWAY;
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags)) == 0)
			break;
		if (errno != ENETUNREACH && errno != ESRCH)
			break;
		if (*gateway != '\0' && hp != NULL &&
		    hp->h_addr_list[attempts] != NULL) {
			switch (af) {
			case AF_INET:
				(void) memmove(&so_gate.sin.sin_addr,
				    hp->h_addr_list[attempts], hp->h_length);
				continue;
			case AF_INET6:
				(void) memmove(&so_gate.sin6.sin6_addr,
				    hp->h_addr_list[attempts], hp->h_length);
				continue;
			}
		}
		break;
	}
	oerrno = errno;
	if (*cmd != 'g') {
		(void) printf("%s %s %s", cmd, ishost ? "host" : "net", dest);
		if (*gateway != '\0') {
			switch (af) {
			case AF_INET:
				if (nflag) {
					(void) printf(": gateway %s",
					    inet_ntoa(so_gate.sin.sin_addr));
				} else if (attempts > 1 && ret == 0) {
					(void) printf(": gateway %s (%s)",
					    gateway,
					    inet_ntoa(so_gate.sin.sin_addr));
				} else {
					(void) printf(": gateway %s", gateway);
				}
				break;
			case AF_INET6:
				if (inet_ntop(AF_INET6,
				    (void *)&so_gate.sin6.sin6_addr, obuf,
				    INET6_ADDRSTRLEN) != NULL) {
					if (nflag) {
						(void) printf(": gateway %s",
						    obuf);
					} else if (attempts > 1 && ret == 0) {
						(void) printf(": gateway %s "
						    "(%s)",
						    gateway, obuf);
					}
					break;
				}
				/* FALLTHROUGH */
			default:
				(void) printf(": gateway %s", gateway);
				break;
			}
		}
		if (ret == 0)
			(void) printf("\n");
	}
	if (ret != 0) {
		if (*cmd == 'g') {
			if (nflag) {
				switch (af) {
				case AF_INET:
					(void) printf(" %s",
					    inet_ntoa(so_dst.sin.sin_addr));
					break;
				case AF_INET6:
					if (inet_ntop(AF_INET6,
					    (void *)&so_dst.sin6.sin6_addr,
					    obuf, INET6_ADDRSTRLEN) != NULL) {
						(void) printf(" %s", obuf);
						break;
					}
					/* FALLTHROUGH */
				default:
					(void) printf("%s", dest);
					break;
				}
			} else {
				(void) printf("%s", dest);
			}
		}
		switch (oerrno) {
		case ESRCH:
			err = "not in table";
			break;
		case EBUSY:
			err = "entry in use";
			break;
		case ENOBUFS:
			err = "routing table overflow";
			break;
		case EEXIST:
			err = "entry exists";
			break;
		default:
			err = strerror(oerrno);
			break;
		}
		(void) printf(": %s\n", err);
	}
	/*
	 * In the case of AF_INET6, one of the getipnodebyX() functions was used
	 * so free the allocated hostent.
	 */
	if (af == AF_INET6 && hp != NULL)
		freehostent(hp);
}


/*
 * Convert a network number to the corresponding IP address.
 * If the RTA_NETMASK hasn't been specified yet set it based
 * on the class of address.
 */
void
inet_makenetandmask(ulong_t net, struct sockaddr_in *sin)
{
	ulong_t addr, mask = 0;
	char *cp;

	if (net == 0) {
		mask = addr = 0;
	} else if (net < 128) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSA_NET;
	} else if (net < 65536) {
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 16777216L) {
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSC_NET;
	} else {
		addr = net;
		if ((addr & IN_CLASSA_HOST) == 0)
			mask =  IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask =  IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask =  IN_CLASSC_NET;
		else {
			if (IN_CLASSA(addr))
				mask =  IN_CLASSA_NET;
			else if (IN_CLASSB(addr))
				mask =  IN_CLASSB_NET;
			else if (IN_CLASSC(addr))
				mask =  IN_CLASSC_NET;
			else
				mask = IP_HOST_MASK;
			mask = inet_makesubnetmask(addr, mask);
		}
	}
	sin->sin_addr.s_addr = htonl(addr);

	if (!(rtm_addrs & RTA_NETMASK)) {
		rtm_addrs |= RTA_NETMASK;
		sin = &so_mask.sin;
		sin->sin_addr.s_addr = htonl(mask);
		sin->sin_family = AF_INET;
		cp = (char *)(&sin->sin_addr + 1);
		while (*--cp == 0 && cp > (char *)sin)
			;
	}
}

ulong_t
inet_makesubnetmask(ulong_t addr, ulong_t mask)
{
	int n;
	struct ifconf ifc;
	struct ifreq ifreq;
	struct ifreq *ifr;
	struct sockaddr_in *sin;
	char *buf;
	int numifs;
	size_t bufsize;
	int iosoc;
	ulong_t if_addr, if_mask;
	ulong_t if_subnetmask = 0;
	short if_flags;

	if (mask == 0)
		return (0);
	if ((iosoc = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		quit("socket");
	if (ioctl(iosoc, SIOCGIFNUM, (char *)&numifs) < 0)
		quit("ioctl");
	bufsize = numifs * sizeof (struct ifreq);
	buf = malloc(bufsize);
	if (buf == NULL)
		quit("malloc");
	(void) memset((char *)&ifc, 0, sizeof (ifc));
	ifc.ifc_len = bufsize;
	ifc.ifc_buf = buf;
	if (ioctl(iosoc, SIOCGIFCONF, (char *)&ifc) < 0)
		quit("ioctl (get interface configuration)");
	/* Let's check to see if this is maybe a local subnet route. */
	ifr = ifc.ifc_req;
	for (n = ifc.ifc_len / sizeof (struct ifreq); n > 0; n--, ifr++) {
		ifreq = *ifr;
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		if_addr = ntohl(sin->sin_addr.s_addr);

		if (ioctl(iosoc, SIOCGIFFLAGS, (char *)&ifreq) < 0)
			quit("ioctl (get interface flags)");
		if ((ifreq.ifr_flags & IFF_UP) == 0)
			continue;
		if_flags = ifreq.ifr_flags;

		if (ioctl(iosoc, SIOCGIFNETMASK, (char *)&ifreq) < 0)
			quit("ioctl (get netmask)");
		sin = (struct sockaddr_in *)&ifreq.ifr_addr;
		if_mask = ntohl(sin->sin_addr.s_addr);
		if ((if_addr & mask) == (addr & mask)) {
			/*
			 * Don't trust pt-pt interfaces if there are
			 * other interfaces.
			 */
			if (if_flags & IFF_POINTOPOINT) {
				if_subnetmask = if_mask;
				continue;
			}
			/*
			 * Fine.  Just assume the same net mask as the
			 * directly attached subnet interface is using.
			 */
			return (if_mask);
		}
	}
	if (if_subnetmask != 0)
		return (if_subnetmask);
	return (mask);
}

/*
 * Interpret an argument as a network address of some kind,
 * returning B_TRUE if a host address, B_FALSE if a network address.
 *
 * If the address family is one looked up in getaddr() using one of the
 * getipnodebyX() functions (currently only AF_INET6), then callers should
 * freehostent() the returned "struct hostent" pointer if one was passed in.
 */
boolean_t
getaddr(int which, char *s, struct hostent **hpp)
{
	sup su;
	struct hostent *hp;
	boolean_t ret;

	if (s == NULL) {
		(void) fprintf(stderr,
		    "route: argument required following keyword\n");
		usage((char *)NULL);
	}
	if (hpp == NULL)
		hpp = &hp;
	*hpp = NULL;
	rtm_addrs |= which;
	switch (which) {
	case RTA_DST:
		su = &so_dst;
		su->sa.sa_family = af;
		break;
	case RTA_GATEWAY:
		su = &so_gate;
		su->sa.sa_family = af;
		break;
	case RTA_NETMASK:
		su = &so_mask;
		su->sa.sa_family = af;
		break;
	case RTA_GENMASK:
		su = &so_genmask;
		su->sa.sa_family = af;
		break;
	case RTA_IFP:
		so_ifp.sdl.sdl_index = if_nametoindex(s);
		if (so_ifp.sdl.sdl_index == 0) {
			if (errno == ENXIO) {
				(void) fprintf(stderr,
				    "route: %s: no such interface\n", s);
				exit(1);
			} else {
				quit("if_nametoindex");
			}
		}
		so_ifp.sdl.sdl_family = AF_LINK;
		return (B_FALSE);
	case RTA_IFA:
		su = &so_ifa;
		su->sa.sa_family = af;
		break;
	default:
		quit("Internal Error");
		/* NOTREACHED */
	}
	if (strcmp(s, "default") == 0) {
		if (which == RTA_DST) {
			forcenet = B_TRUE;
			(void) getaddr(RTA_NETMASK, s, NULL);
		}
		return (B_FALSE);
	}
	switch (af) {
	case AF_LINK:
		link_addr(s, &su->sdl);
		return (B_TRUE);
	case PF_ROUTE:
		sockaddr(s, &su->sa);
		return (B_TRUE);
	case AF_INET6:
		switch (which) {
		case RTA_DST:
			if (s[0] == '/') {
				(void) fprintf(stderr,
				    "route: %s: unexpected '/'\n", s);
				exit(1);
			}
			masklen = 0;
			ret = in6_getaddr(s, (struct sockaddr *)&su->sin6,
			    &masklen, hpp);
			switch (masklen) {
			case NO_PREFIX:
				/* Nothing there - ok */
				return (ret);
			case BAD_ADDR:
				(void) fprintf(stderr,
				    "route: bad prefix length in %s\n", s);
				exit(1);
				/* NOTREACHED */
			default:
				(void) memset((char *)&so_mask.sin6.sin6_addr,
				    0, sizeof (so_mask.sin6.sin6_addr));
				if (!in_prefixlentomask(masklen, IPV6_ABITS,
				    (uchar_t *)&so_mask.sin6.sin6_addr)) {
					(void) fprintf(stderr,
					    "route: bad prefix length: %d\n",
					    masklen);
					exit(1);
				}
				break;
			}
			so_mask.sin6.sin6_family = af;
			rtm_addrs |= RTA_NETMASK;
			return (ret);
		case RTA_GATEWAY:
		case RTA_IFA:
			ret = in6_getaddr(s, (struct sockaddr *)&su->sin6,
			    NULL, hpp);
			return (ret);
		case RTA_NETMASK:
		case RTA_GENMASK:
			(void) fprintf(stderr,
			    "route: -netmask not supported for IPv6: "
			    "use <prefix>/<prefix-length> instead\n");
			exit(1);
			/* NOTREACHED */
		default:
			quit("Internal Error");
			/* NOTREACHED */
		}
	case AF_INET:
		switch (which) {
		case RTA_DST:
			if (s[0] == '/') {
				(void) fprintf(stderr,
				    "route: %s: unexpected '/'\n", s);
				exit(1);
			}
			masklen = 0;
			ret = in_getaddr(s, (struct sockaddr *)&su->sin,
			    &masklen, which, hpp);
			switch (masklen) {
			case NO_PREFIX:
				/* Nothing there - ok */
				return (ret);
			case BAD_ADDR:
				(void) fprintf(stderr,
				    "route: bad prefix length in %s\n", s);
				exit(1);
				/* NOTREACHED */
			default:
				(void) memset((char *)&so_mask.sin.sin_addr, 0,
				    sizeof (so_mask.sin.sin_addr));
				if (!in_prefixlentomask(masklen, IP_ABITS,
				    (uchar_t *)&so_mask.sin.sin_addr)) {
					(void) fprintf(stderr,
					    "route: bad prefix length: %d\n",
					    masklen);
					exit(1);
				}
				break;
			}
			so_mask.sin.sin_family = af;
			rtm_addrs |= RTA_NETMASK;
			return (ret);
		case RTA_GATEWAY:
		case RTA_IFA:
		case RTA_NETMASK:
		case RTA_GENMASK:
			ret = in_getaddr(s, (struct sockaddr *)&su->sin, NULL,
			    which, hpp);
			return (ret);
		default:
			quit("Internal Error");
			/* NOTREACHED */
		}
	default:
		quit("Internal Error");
		/* NOTREACHED */
	}

}

/*
 * Interpret an argument as an IPv4 network address of some kind,
 * returning B_TRUE if a host address, B_FALSE if a network address.
 *
 * If the last argument is non-NULL allow a <addr>/<n> syntax and
 * pass out <n> in *plenp.
 * If <n> doesn't parse return BAD_ADDR as *plenp.
 * If no /<n> is present return NO_PREFIX as *plenp.
 */
boolean_t
in_getaddr(char *s, struct sockaddr *saddr, int *plenp, int which,
    struct hostent **hpp)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)saddr;
	struct hostent *hp;
	struct netent *np;
	ulong_t val;
	char str[BUFSIZ];

	(void) strncpy(str, s, sizeof (str));

	/*
	 * Look for '/'<n> is plenp
	 */
	if (plenp != NULL) {
		char *cp;

		*plenp = in_getprefixlen(str, B_TRUE, IP_ABITS);
		if (*plenp == BAD_ADDR)
			return (B_FALSE);
		cp = strchr(str, '/');
		if (cp != NULL)
			*cp = '\0';
	} else if (strchr(str, '/') != NULL) {
		(void) fprintf(stderr, "route: %s: unexpected '/'\n", str);
		exit(1);
	}

	(void) memset((char *)saddr, 0, sizeof (*saddr));
	sin->sin_family = AF_INET;

	if (((int)(val = inet_addr(str)) != -1) &&
	    (which != RTA_DST || !forcenet)) {
		sin->sin_addr.s_addr = val;
		if (inet_lnaof(sin->sin_addr) != INADDR_ANY ||
		    forcehost)
			return (B_TRUE);
		val = ntohl(val);
		if (which == RTA_DST)
			inet_makenetandmask(val, sin);
		return (B_FALSE);
	}
	if ((int)(val = inet_network(str)) != -1 ||
	    ((np = getnetbyname(str)) != NULL &&
		(val = np->n_net) != 0)) {
		if (which == RTA_DST)
			inet_makenetandmask(val, sin);
		return (B_FALSE);
	}
	hp = gethostbyname(str);
	if (hp != NULL) {
		*hpp = hp;
		(void) memmove(&sin->sin_addr, hp->h_addr,
		    hp->h_length);
		return (B_TRUE);
	}
	(void) fprintf(stderr, "%s: bad value\n", s);
	exit(1);
	/* NOTREACHED */
}

/*
 * Interpret an argument as an IPv6 network address of some kind,
 * returning B_TRUE if a host address, B_FALSE if a network address.
 *
 * If the last argument is non-NULL allow a <addr>/<n> syntax and
 * pass out <n> in *plenp.
 * If <n> doesn't parse return BAD_ADDR as *plenp.
 * If no /<n> is present return NO_PREFIX as *plenp.
 */
boolean_t
in6_getaddr(char *s, struct sockaddr *saddr, int *plenp, struct hostent **hpp)
{
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)saddr;
	struct hostent *hp;
	char str[BUFSIZ];
	int error_num;

	(void) strncpy(str, s, sizeof (str));

	/*
	 * Look for '/'<n> is plenp
	 */
	if (plenp != NULL) {
		char *cp;

		*plenp = in_getprefixlen(str, B_TRUE, IPV6_ABITS);
		if (*plenp == BAD_ADDR)
			return (B_FALSE);
		cp = strchr(str, '/');
		if (cp != NULL)
			*cp = '\0';
	} else if (strchr(str, '/') != NULL) {
		(void) fprintf(stderr, "route: %s: unexpected '/'\n", str);
		exit(1);
	}

	(void) memset(sin6, 0, sizeof (struct sockaddr_in6));
	sin6->sin6_family = AF_INET6;

	hp = getipnodebyname(str, AF_INET6, 0, &error_num);
	if (hp != NULL) {
		*hpp = hp;
		(void) memmove((char *)&sin6->sin6_addr, hp->h_addr,
		    hp->h_length);
		return (B_FALSE);
	}
	if (error_num == TRY_AGAIN) {
		(void) fprintf(stderr, "route: %s: bad address "
		    "(try again later)\n", s);
	} else {
		(void) fprintf(stderr, "route: %s: bad address\n", s);
	}
	exit(1);
	/* NOTREACHED */
}

/*
 * If "slash" is zero this parses the whole string as
 * an integer. With "slash" non zero it parses the tail part as an integer.
 *
 * If it is not a valid integer this returns BAD_ADDR.
 * If there is /<n> present this returns NO_PREFIX.
 */
int
in_getprefixlen(char *addr, boolean_t slash, int max_plen)
{
	int prefixlen;
	char *str, *end;

	if (slash) {
		str = strchr(addr, '/');
		if (str == NULL)
			return (NO_PREFIX);
		str++;
	} else {
		str = addr;
	}

	prefixlen = strtol(str, &end, 10);
	if (prefixlen < 0)
		return (BAD_ADDR);
	if (str == end)
		return (BAD_ADDR);
	if (max_plen != 0 && max_plen < prefixlen)
		return (BAD_ADDR);
	else
		return (prefixlen);
}

/*
 * Convert a prefix length to a mask.
 * Returns B_TRUE if ok. B_FALSE otherwise.
 * Assumes the mask array is zeroed by the caller.
 */
boolean_t
in_prefixlentomask(int prefixlen, int maxlen, uchar_t *mask)
{
	if (prefixlen < 0 || prefixlen > maxlen)
		return (B_FALSE);

	while (prefixlen > 0) {
		if (prefixlen >= 8) {
			*mask++ = 0xFF;
			prefixlen -= 8;
			continue;
		}
		*mask |= 1 << (8 - prefixlen);
		prefixlen--;
	}
	return (B_TRUE);
}

void
rtmonitor(int argc, char *argv[])
{
	int n;
	char msg[2048];

	if (tflag)
		exit(0);
	verbose = B_TRUE;
	if (argc > 1) {
		argv++;
		if (argc == 2 && **argv == '-') {
			switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
			case K_LINK:
				af = AF_LINK;
				break;
			case K_INET6:
				af = AF_INET6;
				break;
			default:
				usage(*argv);
				/* NOTREACHED */
			}
		} else {
			usage(*argv);
		}
		(void) close(s);
		if (tflag)
			s = open("/dev/null", O_WRONLY, af);
		else
			s = socket(PF_ROUTE, SOCK_RAW, af);
		if (s < 0)
			quit("socket");
	}
	for (;;) {
		n = read(s, msg, sizeof (msg));
		if (n <= 0)
			quit("read");
		if (verbose)
			(void) printf("got message of size %d\n", n);
		print_rtmsg((struct rt_msghdr *)msg, n);
	}
}

int
rtmsg(int cmd, int flags)
{
	static int seq;
	int rlen;
	char *cp = m_rtmsg.m_space;
	int l;

	errno = 0;
	(void) memset(&m_rtmsg, 0, sizeof (m_rtmsg));
	if (cmd == 'a') {
		cmd = RTM_ADD;
	} else if (cmd == 'c') {
		cmd = RTM_CHANGE;
	} else if (cmd == 'g') {
		cmd = RTM_GET;
		if (so_ifp.sa.sa_family == 0) {
			so_ifp.sa.sa_family = AF_LINK;
			rtm_addrs |= RTA_IFP;
		}
	} else {
		cmd = RTM_DELETE;
	}
#define	rtm m_rtmsg.m_rtm
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;

#define	NEXTADDR(w, u) \
	if (rtm_addrs & (w)) { \
		l = ROUNDUP_LONG(salen(&u.sa)); \
		(void) memmove(cp, &(u), l); \
		cp += l; \
		if (verbose) \
			sodump(&(u), #u); \
	}
	NEXTADDR(RTA_DST, so_dst);
	NEXTADDR(RTA_GATEWAY, so_gate);
	NEXTADDR(RTA_NETMASK, so_mask);
	NEXTADDR(RTA_GENMASK, so_genmask);
	NEXTADDR(RTA_IFP, so_ifp);
	NEXTADDR(RTA_IFA, so_ifa);
#undef	NEXTADDR
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose)
		print_rtmsg(&rtm, l);
	if (debugonly)
		return (0);
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		switch (errno) {
		case ESRCH:
		case EBUSY:
		case ENOBUFS:
		case EEXIST:
		case ENETUNREACH:
		case EHOSTUNREACH:
			break;
		default:
			perror("writing to routing socket");
			break;
		}
		return (-1);
	} else if (rlen < (int)rtm.rtm_msglen) {
		(void) fprintf(stderr,
		    "route: write to routing socket got only %d for rlen\n",
		    rlen);
		return (-1);
	}
	if (cmd == RTM_GET) {
		do {
			l = read(s, (char *)&m_rtmsg, sizeof (m_rtmsg));
		} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l < 0) {
			(void) fprintf(stderr,
			    "route: read from routing socket: %s\n",
			    strerror(errno));
		} else {
			print_getmsg(&rtm, l);
		}
	}
#undef rtm
	return (0);
}

static char *msgtypes[] = {
	"",
	"RTM_ADD: Add Route",
	"RTM_DELETE: Delete Route",
	"RTM_CHANGE: Change Metrics or flags",
	"RTM_GET: Report Metrics",
	"RTM_LOSING: Kernel Suspects Partitioning",
	"RTM_REDIRECT: Told to use different route",
	"RTM_MISS: Lookup failed on this address",
	"RTM_LOCK: fix specified metrics",
	"RTM_OLDADD: caused by SIOCADDRT",
	"RTM_OLDDEL: caused by SIOCDELRT",
	"RTM_RESOLVE: Route created by cloning",
	"RTM_NEWADDR: address being added to iface",
	"RTM_DELADDR: address being removed from iface",
	"RTM_IFINFO: iface status change",
	0,
};

#define	NMSGTYPES (sizeof (msgtypes) / sizeof (msgtypes[0]))

static char metricnames[] =
"\011pksent\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount"
	"\1mtu";
static char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT"
	"\011CLONING\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE"
	"\016PRIVATE\017PROTO2\020PROTO1";
static char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6NOTRAILERS\7RUNNING\010NOARP"
	"\011PPROMISC\012ALLMULTI\013INTELLIGENT\014MULTICAST"
	"\015MULTI_BCAST\016UNNUMBERED\017DHCP\020PRIVATE"
	"\021NOXMIT\022NOLOCAL\023DEPRECATED\024ADDRCONF"
	"\025ROUTER\026NONUD\027ANYCAST\030NORTEXCH\031IPv4\032IPv6"
	"\033MIP";
static char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD";

void
print_rtmsg(struct rt_msghdr *rtm, int msglen)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;

	if (!verbose)
		return;
	if (rtm->rtm_version != RTM_VERSION) {
		(void) printf("routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	if (rtm->rtm_msglen > (ushort_t)msglen) {
		(void) printf("message length mismatch, in packet %d, "
		    "returned %d\n",
		    rtm->rtm_msglen, msglen);
	}
	/*
	 * Since rtm->rtm_type is unsigned, we'll just check the case of zero
	 * and the upper-bound of (NMSGTYPES - 1).
	 */
	if (rtm->rtm_type == 0 || rtm->rtm_type >= (NMSGTYPES - 1)) {
		(void) printf("routing message type %d not understood\n",
		    rtm->rtm_type);
		return;
	}
	(void) printf("%s: len %d, ", msgtypes[rtm->rtm_type], rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		(void) printf("if# %d, flags:", ifm->ifm_index);
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)(ifm + 1), ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		(void) printf("metric %d, flags:", ifam->ifam_metric);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)(ifam + 1), ifam->ifam_addrs);
		break;
	default:
		(void) printf("pid: %ld, seq %d, errno %d, flags:",
			rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		pmsg_common(rtm);
	}
}

void
print_getmsg(struct rt_msghdr *rtm, int msglen)
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL;
	struct sockaddr_dl *ifp = NULL;
	struct sockaddr *sa;
	char *cp;
	int i;

	(void) printf("   route to: %s\n", routename(&so_dst.sa));
	if (rtm->rtm_version != RTM_VERSION) {
		(void) fprintf(stderr,
		    "routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	if (rtm->rtm_msglen > (ushort_t)msglen) {
		(void) fprintf(stderr,
		    "message length mismatch, in packet %d, returned %d\n",
		    rtm->rtm_msglen, msglen);
	}
	if (rtm->rtm_errno)  {
		(void) fprintf(stderr, "RTM_GET: %s (errno %d)\n",
		    strerror(rtm->rtm_errno), rtm->rtm_errno);
		return;
	}
	cp = ((char *)(rtm + 1));
	if (rtm->rtm_addrs != 0) {
		for (i = 1; i != 0; i <<= 1) {
			if (i & rtm->rtm_addrs) {
				sa = (struct sockaddr *)cp;
				switch (i) {
				case RTA_DST:
					dst = sa;
					break;
				case RTA_GATEWAY:
					gate = sa;
					break;
				case RTA_NETMASK:
					mask = sa;
					break;
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					    ((struct sockaddr_dl *)sa)->
						sdl_nlen != 0)
						ifp = (struct sockaddr_dl *)sa;
					break;
				}
				ADVANCE(cp, sa);
			}
		}
	}
	if (dst != NULL && mask != NULL)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst != NULL)
		(void) printf("destination: %s\n", routename(dst));
	if (mask != NULL) {
		boolean_t savenflag = nflag;

		nflag = B_TRUE;
		(void) printf("       mask: %s\n", routename(mask));
		nflag = savenflag;
	}
	if (gate != NULL && rtm->rtm_flags & RTF_GATEWAY)
		(void) printf("    gateway: %s\n", routename(gate));
	if (ifp != NULL) {
		if (verbose) {
			int i;

			(void) printf("  interface: %.*s index %d address ",
			    ifp->sdl_nlen, ifp->sdl_data, ifp->sdl_index);
			for (i = ifp->sdl_nlen;
			    i < ifp->sdl_nlen + ifp->sdl_alen;
			    i++) {
				(void) printf("%02x ",
				    ifp->sdl_data[i] & 0xFF);
			}
			(void) printf("\n");
		} else {
			(void) printf("  interface: %.*s\n",
			    ifp->sdl_nlen, ifp->sdl_data);
		}
	}
	(void) printf("      flags: ");
	bprintf(stdout, rtm->rtm_flags, routeflags);

#define	lock(f)	((rtm->rtm_rmx.rmx_locks & RTV_ ## f) ? 'L' : ' ')
#define	msec(u)	(((u) + 500) / 1000)		/* usec to msec */

	(void) printf("\n%s\n", " recvpipe  sendpipe  ssthresh    rtt,ms "
	    "rttvar,ms  hopcount      mtu     expire");
	(void) printf("%8d%c ", rtm->rtm_rmx.rmx_recvpipe, lock(RPIPE));
	(void) printf("%8d%c ", rtm->rtm_rmx.rmx_sendpipe, lock(SPIPE));
	(void) printf("%8d%c ", rtm->rtm_rmx.rmx_ssthresh, lock(SSTHRESH));
	(void) printf("%8d%c ", msec(rtm->rtm_rmx.rmx_rtt), lock(RTT));
	(void) printf("%8d%c ", msec(rtm->rtm_rmx.rmx_rttvar), lock(RTTVAR));
	(void) printf("%8d%c ", rtm->rtm_rmx.rmx_hopcount, lock(HOPCOUNT));
	(void) printf("%8d%c ", rtm->rtm_rmx.rmx_mtu, lock(MTU));
	if (rtm->rtm_rmx.rmx_expire)
		rtm->rtm_rmx.rmx_expire -= time(0);
	(void) printf("%8d%c\n", rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
#undef msec
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_IFP|RTA_IFA|RTA_BRD)
	if (verbose) {
		pmsg_common(rtm);
	} else if (rtm->rtm_addrs &~ RTA_IGN) {
		(void) printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		(void) putchar('\n');
	}
#undef	RTA_IGN
}

void
pmsg_common(struct rt_msghdr *rtm)
{
	(void) printf("\nlocks: ");
	bprintf(stdout, (int)rtm->rtm_rmx.rmx_locks, metricnames);
	(void) printf(" inits: ");
	bprintf(stdout, (int)rtm->rtm_inits, metricnames);
	pmsg_addrs(((char *)(rtm + 1)), rtm->rtm_addrs);
}

void
pmsg_addrs(char *cp, int addrs)
{
	struct sockaddr *sa;
	int i;

	if (addrs == 0)
		return;
	(void) printf("\nsockaddrs: ");
	bprintf(stdout, addrs, addrnames);
	(void) putchar('\n');
	for (i = 1; i != 0; i <<= 1) {
		if (i & addrs) {
			sa = (struct sockaddr *)cp;
			(void) printf(" %s", routename(sa));
			ADVANCE(cp, sa);
		}
	}
	(void) putchar('\n');
	(void) fflush(stdout);
}

void
bprintf(FILE *fp, int b, char *s)
{
	int i;
	boolean_t gotsome = B_FALSE;

	if (b == 0)
		return;
	while ((i = *s++) != 0) {
		if (b & (1 << (i - 1))) {
			if (!gotsome)
				i = '<';
			else
				i = ',';
			(void) putc(i, fp);
			gotsome = B_TRUE;
			for (; (i = *s) > ' '; s++)
				(void) putc(i, fp);
		} else {
			while (*s > ' ')
				s++;
		}
	}
	if (gotsome)
		(void) putc('>', fp);
}

int
keyword(char *cp)
{
	struct keytab *kt = keywords;

	while (kt->kt_cp && strcmp(kt->kt_cp, cp))
		kt++;
	return (kt->kt_i);
}

void
sodump(sup su, char *which)
{
	static char obuf[INET6_ADDRSTRLEN];

	switch (su->sa.sa_family) {
	case AF_LINK:
		(void) printf("%s: link %s; ",
		    which, link_ntoa(&su->sdl));
		break;
	case AF_INET:
		(void) printf("%s: inet %s; ",
		    which, inet_ntoa(su->sin.sin_addr));
		break;
	case AF_INET6:
		if (inet_ntop(AF_INET6, (void *)&su->sin6.sin6_addr, obuf,
		    INET6_ADDRSTRLEN) != NULL) {
			(void) printf("%s: inet6 %s; ", which, obuf);
			break;
		}
		/* FALLTHROUGH */
	default:
		quit("Internal Error");
		/* NOTREACHED */
	}
	(void) fflush(stdout);
}

/* States */
#define	VIRGIN	0
#define	GOTONE	1
#define	GOTTWO	2
#define	RESET	3
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define	DELIM	(4*2)
#define	LETTER	(4*3)

void
sockaddr(char *addr, struct sockaddr *sa)
{
	char *cp = (char *)sa;
	int size = salen(sa);
	char *cplim = cp + size;
	int byte = 0, state = VIRGIN, new;

	(void) memset(cp, 0, size);
	cp++;
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0) {
			state |= END;
		} else {
			state |= DELIM;
		}
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte;
			/* FALLTHROUGH */
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte;
			/* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim);
}

int
salen(struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET:
		return (sizeof (struct sockaddr_in));
	case AF_LINK:
		return (sizeof (struct sockaddr_dl));
	case AF_INET6:
		return (sizeof (struct sockaddr_in6));
	default:
		return (sizeof (struct sockaddr));
	}
}

void
link_addr(const char *addr, struct sockaddr_dl *sdl)
{
	char *cp = sdl->sdl_data;
	char *cplim = sizeof (struct sockaddr_dl) + (char *)sdl;
	int byte = 0, state = VIRGIN, new;

	(void) memset((char *)sdl, 0, sizeof (struct sockaddr_dl));
	sdl->sdl_family = AF_LINK;
	do {
		state &= ~LETTER;
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0) {
			state |= END;
		} else if (state == VIRGIN &&
		    (((*addr >= 'A') && (*addr <= 'Z')) ||
		    ((*addr >= 'a') && (*addr <= 'z')))) {
			state |= LETTER;
		} else {
			state |= DELIM;
		}
		addr++;
		switch (state /* | INPUT */) {
		case VIRGIN | DIGIT:
		case VIRGIN | LETTER:
			*cp++ = addr[-1];
			continue;
		case VIRGIN | DELIM:
			state = RESET;
			sdl->sdl_nlen = cp - sdl->sdl_data;
			continue;
		case GOTTWO | DIGIT:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | DIGIT:
			state = GOTONE;
			byte = new;
			continue;
		case GOTONE | DIGIT:
			state = GOTTWO;
			byte = new + (byte << 4);
			continue;
		default: /* | DELIM */
			state = RESET;
			*cp++ = byte;
			byte = 0;
			continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | END:
			break;
		}
		break;
	} while (cp < cplim);
	sdl->sdl_alen = cp - LLADDR(sdl);
}

static char hexlist[] = "0123456789abcdef";

char *
link_ntoa(const struct sockaddr_dl *sdl)
{
	static char obuf[64];
	char *out = obuf;
	int i;
	uchar_t *in = (uchar_t *)LLADDR(sdl);
	uchar_t *inlim = in + sdl->sdl_alen;
	boolean_t firsttime = B_TRUE;

	if (sdl->sdl_nlen) {
		(void) memcpy(obuf, sdl->sdl_data, sdl->sdl_nlen);
		out += sdl->sdl_nlen;
		if (sdl->sdl_alen)
			*out++ = ':';
	}
	while (in < inlim) {
		if (firsttime)
			firsttime = B_FALSE;
		else
			*out++ = '.';
		i = *in++;
		if (i > 0xf) {
			out[1] = hexlist[i & 0xf];
			i >>= 4;
			out[0] = hexlist[i];
			out += 2;
		} else {
			*out++ = hexlist[i];
		}
	}
	*out = 0;
	return (obuf);
}

static mib_item_t *
mibget(int sd)
{
	char			buf[512];
	int			flags;
	int			i, j, getcode;
	struct strbuf		ctlbuf, databuf;
	struct T_optmgmt_req	*tor = (struct T_optmgmt_req *)buf;
	struct T_optmgmt_ack	*toa = (struct T_optmgmt_ack *)buf;
	struct T_error_ack	*tea = (struct T_error_ack *)buf;
	struct opthdr		*req;
	mib_item_t		*first_item = NULL;
	mib_item_t		*last_item  = NULL;
	mib_item_t		*temp;

	tor->PRIM_type = T_SVR4_OPTMGMT_REQ;
	tor->OPT_offset = sizeof (struct T_optmgmt_req);
	tor->OPT_length = sizeof (struct opthdr);
	tor->MGMT_flags = T_CURRENT;
	req = (struct opthdr *)&tor[1];
	req->level = MIB2_IP;		/* any MIB2_xxx value ok here */
	req->name  = 0;
	req->len   = 0;

	ctlbuf.buf = buf;
	ctlbuf.len = tor->OPT_length + tor->OPT_offset;
	flags = 0;
	if (putmsg(sd, &ctlbuf, NULL, flags) < 0) {
		perror("mibget: putmsg (ctl)");
		return (NULL);
	}
	/*
	 * each reply consists of a ctl part for one fixed structure
	 * or table, as defined in mib2.h.  The format is a T_OPTMGMT_ACK,
	 * containing an opthdr structure.  level/name identify the entry,
	 * len is the size of the data part of the message.
	 */
	req = (struct opthdr *)&toa[1];
	ctlbuf.maxlen = sizeof (buf);
	for (j = 1; ; j++) {
		flags = 0;
		getcode = getmsg(sd, &ctlbuf, NULL, &flags);
		if (getcode < 0) {
			perror("mibget: getmsg (ctl)");
			if (verbose) {
				(void) fprintf(stderr,
				    "#   level   name    len\n");
				i = 0;
				for (last_item = first_item; last_item != NULL;
				    last_item = last_item->next_item) {
					(void) printf("%d  %4ld   %5ld   %ld\n",
					    ++i, last_item->group,
					    last_item->mib_id,
					    last_item->length);
				}
			}
			break;
		}
		if (getcode == 0 &&
		    ctlbuf.len >= sizeof (struct T_optmgmt_ack) &&
		    toa->PRIM_type == T_OPTMGMT_ACK &&
		    toa->MGMT_flags == T_SUCCESS &&
		    req->len == 0) {
			if (verbose) {
				(void) printf("mibget getmsg() %d returned EOD "
				    "(level %lu, name %lu)\n", j, req->level,
				    req->name);
			}
			return (first_item);		/* this is EOD msg */
		}

		if (ctlbuf.len >= sizeof (struct T_error_ack) &&
		    tea->PRIM_type == T_ERROR_ACK) {
			(void) fprintf(stderr, "mibget %d gives T_ERROR_ACK: "
			    "TLI_error = 0x%lx, UNIX_error = 0x%lx\n",
			    j, tea->TLI_error, tea->UNIX_error);
			errno = (tea->TLI_error == TSYSERR)
				? tea->UNIX_error : EPROTO;
			break;
		}

		if (getcode != MOREDATA ||
		    ctlbuf.len < sizeof (struct T_optmgmt_ack) ||
		    toa->PRIM_type != T_OPTMGMT_ACK ||
		    toa->MGMT_flags != T_SUCCESS) {
			(void) printf("mibget getmsg(ctl) %d returned %d, "
			    "ctlbuf.len = %d, PRIM_type = %ld\n",
			    j, getcode, ctlbuf.len, toa->PRIM_type);
			if (toa->PRIM_type == T_OPTMGMT_ACK) {
				(void) printf("T_OPTMGMT_ACK: "
				    "MGMT_flags = 0x%lx, req->len = %ld\n",
				    toa->MGMT_flags, req->len);
			}
			errno = ENOMSG;
			break;
		}

		temp = (mib_item_t *)malloc(sizeof (mib_item_t));
		if (temp == NULL) {
			perror("mibget: malloc");
			break;
		}
		if (last_item != NULL)
			last_item->next_item = temp;
		else
			first_item = temp;
		last_item = temp;
		last_item->next_item = NULL;
		last_item->group = req->level;
		last_item->mib_id = req->name;
		last_item->length = req->len;
		last_item->valp = (char *)malloc(req->len);
		if (verbose) {
			(void) printf("msg %d:  group = %4ld   mib_id = %5ld   "
			    "length = %ld\n",
			    j, last_item->group, last_item->mib_id,
			    last_item->length);
		}

		databuf.maxlen = last_item->length;
		databuf.buf    = last_item->valp;
		databuf.len    = 0;
		flags = 0;
		getcode = getmsg(sd, NULL, &databuf, &flags);
		if (getcode < 0) {
			perror("mibget: getmsg (data)");
			break;
		} else if (getcode != 0) {
			(void) printf("mibget getmsg(data) returned %d, "
			    "databuf.maxlen = %d, databuf.len = %d\n",
			    getcode, databuf.maxlen, databuf.len);
			break;
		}
	}

	/*
	 * On error, free all the allocated mib_item_t objects.
	 */
	while (first_item != NULL) {
		last_item = first_item;
		first_item = first_item->next_item;
		free(last_item);
	}
	return (NULL);
}
