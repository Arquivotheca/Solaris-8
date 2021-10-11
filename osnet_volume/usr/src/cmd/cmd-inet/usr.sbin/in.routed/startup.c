/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)startup.c	1.16	99/07/29 SMI"	/* SVr4.0 1.3	*/

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
 * 	(c) 1986,1987,1988,1989,1991,1992,1993  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/sockio.h>
#include <net/if.h>
#include <syslog.h>

struct	interface *ifnet;
int	lookforinterfaces = 1;
static int externalinterfaces = 0;	/* # of remote and local interfaces */

#if	!defined(SIOCGIFCONF_FIXED)
#define	MAXIFS	32	/* results in a bufsize of 1024 */
#else
#define	MAXIFS	256
#endif

extern int iosoc;

static int getnetorhostname(char *type, char *name, struct sockaddr_in *sin);
static int gethostnameornumber(char *name, struct sockaddr_in *sin);

/*
 * Find the network interfaces which have configured themselves.
 * Always keep lookforinterfaces set so that we periodically rerun this
 * adding new and removing down interfaces.
 */
void
ifinit(void)
{
	struct interface ifs, *ifp;
	int n;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	struct lifreq lifreq;
	struct sockaddr_in *sin;
	ulong_t i;
	char *buf;
	int numifs;
	unsigned bufsize;
	int netmaskchange = 0;
	int changes = 0;

#ifdef SIOCGIFNUM
	if (ioctl(iosoc, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
	}
#else
	numifs = MAXIFS;
#endif
	bufsize = numifs * sizeof (struct ifreq);
	buf = malloc(bufsize);
	if (buf == NULL) {
		syslog(LOG_ERR, "out of memory\n");
		return;
	}
	bzero((char *)&ifc, sizeof (ifc));
	ifc.ifc_len = bufsize;
	ifc.ifc_buf = buf;
	if (ioctl(iosoc, SIOCGIFCONF, (char *)&ifc) < 0) {
		perror("ioctl (get interface configuration)");
		(void) free(buf);
		return;
	}
	ifr = ifc.ifc_req;
	for (n = ifc.ifc_len / sizeof (struct ifreq); n > 0; n--, ifr++) {
		bzero((char *)&ifs, sizeof (ifs));
		ifs.int_addr = ifr->ifr_addr;
		ifreq = *ifr;
		/* We need to use new interface ioctls to get 64-bit flags */
		(void) strncpy(lifreq.lifr_name, ifr->ifr_name,
		    sizeof (ifr->ifr_name));
		if (ioctl(iosoc, SIOCGLIFFLAGS, (char *)&lifreq) < 0) {
			perror("ioctl (get interface flags)");
			continue;
		}
		/*
		 * Note that although lifr_flags is a 64-bit value, because
		 * of masking with IFF_FROMKERNEL, we are not getting
		 * everything out of it. Furthermore int_flags is only 32-bits.
		 */
		ifs.int_flags = (lifreq.lifr_flags & IFF_FROMKERNEL)
		    | IFF_INTERFACE;
		if ((ifs.int_flags & IFF_UP) == 0 ||
		    ifr->ifr_addr.sa_family == AF_UNSPEC) {
			if (ifp = if_ifwithname(ifr->ifr_name)) {
				if (ifp->int_flags & IFF_UP)
					changes++;
				if_purge(ifp);
			}
			continue;
		}
		/*
		 * do we already know about this interface? Save for later
		 * checks
		 */
		ifp = if_ifwithname(ifr->ifr_name);
		/* argh, this'll have to change sometime */
		if (ifs.int_addr.sa_family != AF_INET)
			continue;
		if (ifs.int_flags & IFF_POINTOPOINT) {
			if (ioctl(iosoc, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
				perror("ioctl (get dstaddr)");
				continue;
			}
			ifs.int_dstaddr = ifreq.ifr_dstaddr;
			if (ifs.int_dstaddr.sa_family != AF_INET)
				continue;
			sin = (struct sockaddr_in *)&ifs.int_dstaddr;
			i = ntohl(sin->sin_addr.s_addr);
			if (i == INADDR_ANY)
				continue;
		}
		if (ioctl(iosoc, SIOCGIFMETRIC, (char *)&ifreq) < 0 ||
		    ifreq.ifr_metric < 0)
			ifs.int_metric = 1;
		else
			ifs.int_metric = ifreq.ifr_metric + 1;

		if (ioctl(iosoc, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			/*
			 * we allow this to be run on a machine that does
			 * not have this ioctl.
			 */
			bzero((caddr_t)&ifreq.ifr_addr,
			    sizeof (struct sockaddr));
		}
		sin = (struct sockaddr_in *)&ifreq.ifr_addr;
		ifs.int_subnetmask = ntohl(sin->sin_addr.s_addr);
		sin = (struct sockaddr_in *)&ifs.int_addr;
		i = ntohl(sin->sin_addr.s_addr);
		if (i == INADDR_ANY)
			continue;
		ifs.int_netmask = inet_netmask(i);
		if (ifs.int_subnetmask == 0)
			ifs.int_subnetmask = ifs.int_netmask;
		ifs.int_net = i & ifs.int_netmask;
		ifs.int_subnet = i & ifs.int_subnetmask;
		if (ifs.int_subnetmask != ifs.int_netmask)
			ifs.int_flags |= IFF_SUBNET;

		/* no one cares about software loopback interfaces */
		if (ifs.int_net == LOOPBACKNET)
			continue;
		if (ifs.int_flags & IFF_BROADCAST) {
			if (ioctl(iosoc, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
				/*
				 * presume old-style if new ioctl not supported
				 */
				sin = (struct sockaddr_in *)&ifs.int_broadaddr;
				bzero((caddr_t)sin, sizeof (ifs.int_broadaddr));
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr = ifs.int_subnet;
			} else {
				ifs.int_broadaddr = ifreq.ifr_addr;
			}
		}
		if (ifp) {
			/*
			 * We already know about this interface. Has anything
			 * significant changed?
			 */
#define	same(a1, a2) \
	(bcmp((caddr_t)((a1)->sa_data), (caddr_t)((a2)->sa_data), 14) == 0)
			if (same(&ifp->int_addr, &ifs.int_addr) &&
			    same(&ifp->int_dstaddr, &ifs.int_dstaddr) &&
			    same(&ifp->int_broadaddr, &ifs.int_broadaddr) &&
			    ifp->int_netmask == ifs.int_netmask &&
			    ifp->int_subnetmask == ifs.int_subnetmask &&
			    (ifp->int_flags & ~IFF_SAVED) ==
			    (ifs.int_flags & ~IFF_SAVED) &&
			    ifp->int_metric == ifs.int_metric)
				continue;
			if_purge(ifp);
			if (ifp->int_subnetmask != ifs.int_subnetmask)
				netmaskchange = 1;
#undef same
			/* Preserve the value of some flags */
			ifp->int_addr = ifs.int_addr;
			ifp->int_broadaddr = ifs.int_broadaddr;
			ifp->int_dstaddr = ifs.int_dstaddr;
			ifp->int_metric = ifs.int_metric;
			ifp->int_flags &= IFF_SAVED;
			ifp->int_flags |= ifs.int_flags;
			ifp->int_net = ifs.int_net;
			ifp->int_netmask = ifs.int_netmask;
			ifp->int_subnet = ifs.int_subnet;
			ifp->int_subnetmask = ifs.int_subnetmask;
		} else {
			ifp = (struct interface *)
			    malloc(sizeof (struct interface));
			if (ifp == 0) {
				(void) printf("routed: out of memory\n");
				break;
			}
			*ifp = ifs;
			ifp->int_name =
			    malloc((unsigned)strlen(ifr->ifr_name) + 1);
			if (ifp->int_name == 0) {
				syslog(LOG_ERR, "ifinit: out of memory\n");
				goto bad;		/* ??? */
			}
			(void) strcpy(ifp->int_name, ifr->ifr_name);
			ifp->int_next = ifnet;
			ifnet = ifp;
			traceinit(ifp);
		}

		changes++;
		/*
		 * Count the # of directly connected networks
		 * and point to point links which aren't looped
		 * back to ourself.  This is used below to
		 * decide if we should be a routing ``supplier''.
		 */
		/*
		 * If we have a point-to-point link, we want to act
		 * as a supplier even if it's our only interface,
		 * as that's the only way our peer on the other end
		 * can tell that the link is up.
		 * We do this by counting the point to point links twice.
		 */
		if ((ifs.int_flags & IFF_POINTOPOINT) &&
		    if_ifwithaddr(&ifs.int_dstaddr) != 0)
			externalinterfaces += 2;
		else
			externalinterfaces++;

		addrouteforif(ifp);
	}
	supplier = maysupply;
	if (supplier < 0)
		supplier = externalinterfaces > 1;
	(void) free(buf);

	if (netmaskchange)
		rtchangeall();
	if (supplier & changes)
		dynamic_update((struct interface *)NULL);
	return;
bad:
	(void) free(buf);
	(void) sleep(60);
	(void) execv(argv0[0], argv0);
	_exit(0177);
}

/*
 * Add route for interface if not currently installed.
 * Create route to other end if a point-to-point link,
 * otherwise a route to this (sub)network.
 * INTERNET SPECIFIC.
 */
void
addrouteforif(struct interface *ifp)
{
	struct sockaddr_in net;
	struct sockaddr *dst;
	struct rt_entry *rt;

	if (ifp->int_flags & IFF_POINTOPOINT) {
		dst = &ifp->int_dstaddr;
	} else {
		bzero((char *)&net, sizeof (net));
		net.sin_family = AF_INET;
		net.sin_addr = inet_makeaddr(ifp->int_subnet, INADDR_ANY);
		dst = (struct sockaddr *)&net;
	}
	rt = rtlookup(dst);
	if (rt &&
	    (rt->rt_state & (RTS_INTERFACE | RTS_INTERNAL)) == RTS_INTERFACE)
		return;
	if (rt)
		rtdelete(rt);
	/*
	 * If interface on subnetted network,
	 * install route to network as well.
	 * This is meant for external viewers.
	 */
	if ((ifp->int_flags & IFF_SUBNET) == IFF_SUBNET) {
		struct sockaddr_in subnet_sa;

		bzero((char *)&subnet_sa, sizeof (subnet_sa));
		subnet_sa.sin_family = AF_INET;
		subnet_sa.sin_addr = inet_makeaddr(ifp->int_net, INADDR_ANY);
		/*
		 * TBD if we have multiple interfaces within the same subnetted
		 * network we might want to add an INTERNAL route for each
		 * interface. Currently this route is "attached" to the first
		 * interface i.e. if that interface goes down this route will
		 * be removed.
		 */
		rt = rtfind((struct sockaddr *)&subnet_sa);
		if (rt == 0) {
			rtadd((struct sockaddr *)&subnet_sa, &ifp->int_addr,
			    ifp->int_metric,
			    ((ifp->int_flags &
				(IFF_INTERFACE|IFF_REMOTE|IFF_PRIVATE)) |
			    RTS_PASSIVE | RTS_INTERNAL | RTS_SUBNET), ifp);
		}
	}
	if (ifp->int_transitions++ > 0) {
		(void) printf("re-installing interface %s\n",
		    (ifp->int_name != NULL) ? ifp->int_name : "(noname)");
		(void) fflush(stdout);
	}
	rtadd(dst, &ifp->int_addr, ifp->int_metric,
	    ifp->int_flags &
		(IFF_INTERFACE | IFF_PASSIVE | IFF_REMOTE | IFF_PRIVATE |
		    IFF_SUBNET | IFF_POINTOPOINT),
	    ifp);
}

/*
 * As a concession to the ARPANET we read a list of gateways
 * from /etc/gateways and add them to our tables.  This file
 * exists at each ARPANET gateway and indicates a set of ``remote''
 * gateways (i.e. a gateway which we can't immediately determine
 * if it's present or not as we can do for those directly connected
 * at the hardware level).  If a gateway is marked ``passive''
 * in the file, then we assume it doesn't have a routing process
 * of our design and simply assume it's always present.  Those
 * not marked passive are treated as if they were directly
 * connected -- they're added into the interface list so we'll
 * send them routing updates.
 *
 * It is also possible to specify "norip <interface>" in /etc/gateways.
 * This will stop routed from sending out any packets over that interface.
 */
void
gwkludge(void)
{
	struct sockaddr_in dst, gate;
	FILE *fp;
	char *type, *dname, *gname, *qual, buf[BUFSIZ];
	struct interface *ifp;
	int metric;
	struct rt_entry route;
	int line = 0;

	fp = fopen("/etc/gateways", "r");
	if (fp == NULL)
		return;
	qual = buf;
	dname = buf + 64;
	gname = buf + ((BUFSIZ - 64) / 3);
	type = buf + (((BUFSIZ - 64) * 2) / 3);
	bzero((char *)&dst, sizeof (dst));
	bzero((char *)&gate, sizeof (gate));
	bzero((char *)&route, sizeof (route));
	/*
	 * format: {net | host} XX gateway XX metric DD [passive]\n
	 * or:	   {norip | noripin | noripout} <interface>\n
	 */
#define	readentry(fp) \
	fscanf((fp), "%s %s gateway %s metric %d %s\n", \
		type, dname, gname, &metric, qual)
	for (;;) {
		line++;
		if (readentry(fp) == EOF)
			break;
		if (strcmp(type, "norip") == 0) {
			if (ifp = if_ifwithname(dname))
				ifp->int_flags |= IFF_NORIPIN | IFF_NORIPOUT;
			else
				if_add(dname, IFF_NORIPIN | IFF_NORIPOUT);
			continue;
		}
		if (strcmp(type, "noripin") == 0) {
			if (ifp = if_ifwithname(dname))
				ifp->int_flags |= IFF_NORIPIN;
			else
				if_add(dname, IFF_NORIPIN);
			continue;
		}
		if (strcmp(type, "noripout") == 0) {
			if (ifp = if_ifwithname(dname))
				ifp->int_flags |= IFF_NORIPOUT;
			else
				if_add(dname, IFF_NORIPOUT);
			continue;
		}
		if (!getnetorhostname(type, dname, &dst)) {
			syslog(LOG_ERR, "Error in line %d in /etc/gateways\n",
				line);
			continue;
		}
		if (!gethostnameornumber(gname, &gate)) {
			syslog(LOG_ERR, "Error in line %d in /etc/gateways\n",
				line);
			continue;
		}
		if (strcmp(qual, "passive") == 0) {
			/*
			 * Passive entries aren't placed in our tables,
			 * only the kernel's, so we don't copy all of the
			 * external routing information within a net.
			 * Internal machines should use the default
			 * route to a suitable gateway (like us).
			 */
			route.rt_dst = *(struct sockaddr *)&dst;
			route.rt_router = *(struct sockaddr *)&gate;
			route.rt_flags = RTF_UP;
			if (strcmp(type, "host") == 0)
				route.rt_flags |= RTF_HOST;
			if (metric)
				route.rt_flags |= RTF_GATEWAY;
			(void) ioctl(iosoc, SIOCADDRT, (char *)&route.rt_rt);
			continue;
		}
		if (strcmp(qual, "external") == 0) {
			/*
			 * Entries marked external are handled
			 * by other means, e.g. EGP,
			 * and are placed in our tables only
			 * to prevent overriding them
			 * with something else.
			 */
			rtadd((struct sockaddr *)&dst,
			    (struct sockaddr *)&gate,
			    metric, RTS_EXTERNAL|RTS_PASSIVE, NULL);
			continue;
		}
		/* assume no duplicate entries */
		externalinterfaces++;
		ifp = (struct interface *)malloc(sizeof (*ifp));
		bzero((char *)ifp, sizeof (*ifp));
		ifp->int_flags = IFF_REMOTE;
		/* can't identify broadcast capability */
		ifp->int_net = inet_netof(dst.sin_addr);
		ifp->int_subnet = ifp->int_net;	/* bug #1028094 */
		if (strcmp(type, "host") == 0) {
			ifp->int_flags |= IFF_POINTOPOINT;
			ifp->int_dstaddr = *((struct sockaddr *)&dst);
		}
		ifp->int_addr = *((struct sockaddr *)&gate);
		ifp->int_metric = metric;
		ifp->int_next = ifnet;
		ifnet = ifp;
		addrouteforif(ifp);
	}
	(void) fclose(fp);
}

static int
getnetorhostname(char *type, char *name, struct sockaddr_in *sin)
{

	if (strcmp(type, "net") == 0) {
		struct netent *np;
		int n;

		n = inet_network(name);
		if (n == -1) {
			np = getnetbyname(name);
			if (np == NULL || np->n_addrtype != AF_INET)
				return (0);
			n = np->n_net;
			/*
			 * getnetbyname returns right-adjusted value.
			 */
			if (n < 128)
				n <<= IN_CLASSA_NSHIFT;
			else if (n < 65536)
				n <<= IN_CLASSB_NSHIFT;
			else
				n <<= IN_CLASSC_NSHIFT;
		}
		sin->sin_family = AF_INET;
		sin->sin_addr = inet_makeaddr((ulong_t)n, INADDR_ANY);
		return (1);
	}
	if (strcmp(type, "host") == 0) {
		struct hostent *hp;

		sin->sin_addr.s_addr = inet_addr(name);
		if ((int)sin->sin_addr.s_addr == -1) {
			hp = gethostbyname(name);
			if (hp == NULL || hp->h_addrtype != AF_INET)
				return (0);
			bcopy(hp->h_addr, (char *)&sin->sin_addr,
			    hp->h_length);
		}
		sin->sin_family = AF_INET;
		return (1);
	}
	return (0);
}

int
gethostnameornumber(char *name, struct sockaddr_in *sin)
{
	struct hostent *hp;

	sin->sin_addr.s_addr = inet_addr(name);
	sin->sin_family = AF_INET;
	if ((int)sin->sin_addr.s_addr != -1)
		return (1);
	hp = gethostbyname(name);
	if (hp) {
		bcopy(hp->h_addr, (char *)&sin->sin_addr, hp->h_length);
		sin->sin_family = hp->h_addrtype;
		return (1);
	}
	return (0);
}
