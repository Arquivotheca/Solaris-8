/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)if.c	1.10	99/07/29 SMI"	/* SVr4.0 1.1	*/

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
 * 	(c) 1986,1987,1988,1989,1991,1992  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing Table Management Daemon
 */
#include "defs.h"

/*
 * Find the interface with address addr.
 */
struct interface *
if_ifwithaddr(struct sockaddr *addr)
{
	register struct interface *ifp;

#define	same(a1, a2) \
	(bcmp((caddr_t)((a1)->sa_data), (caddr_t)((a2)->sa_data), 14) == 0)
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if ((ifp->int_flags & IFF_UP) == 0)
			continue;
		if (ifp->int_flags & IFF_REMOTE)
			continue;
		if (ifp->int_addr.sa_family != addr->sa_family)
			continue;
		if (same(&ifp->int_addr, addr))
			break;
		if ((ifp->int_flags & IFF_BROADCAST) &&
		    same(&ifp->int_broadaddr, addr))
			break;
	}
	return (ifp);
}

/*
 * Find the point-to-point interface with destination address addr.
 */
struct interface *
if_ifwithdstaddr(struct sockaddr *addr)
{
	register struct interface *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if ((ifp->int_flags & IFF_UP) == 0)
			continue;
		if ((ifp->int_flags & IFF_POINTOPOINT) == 0)
			continue;
		if (same(&ifp->int_dstaddr, addr))
			break;
	}
	return (ifp);
}

/*
 * Find the interface with given name.
 */
struct interface *
if_ifwithname(char *name)
{
	register struct interface *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_name != NULL &&
		    strcmp(ifp->int_name, name) == 0)
			break;
	}
	return (ifp);
}

/*
 * Find the interface which reaches a specified destination
 */
struct interface *
if_ifwithdst(struct sockaddr *addr)
{
	register struct interface *ifp;
	register int af = addr->sa_family;
	register int (*netmatch)();

	if (af >= af_max)
		return (0);
	netmatch = afswitch[af].af_netmatch;
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if ((ifp->int_flags & IFF_UP) == 0)
			continue;
		if (ifp->int_flags & IFF_REMOTE)
			continue;
		if (af != ifp->int_addr.sa_family)
			continue;
		if (ifp->int_flags & IFF_POINTOPOINT) {
			if (same(&ifp->int_dstaddr, addr))
				break;
		} else if ((*netmatch)(addr, &ifp->int_addr)) {
			break;
		}
	}
	return (ifp);
}

/*
 * Find an interface from which the specified address
 * should have come from.  Used for figuring out which
 * interface a packet came in on -- for tracing.
 * The lookup is on the source address of the packet.
 */
struct interface *
if_iflookup(struct sockaddr *addr)
{
	register struct interface *ifp, *maybe;
	register int af = addr->sa_family;
	register int (*netmatch)();

	if (af >= af_max)
		return (0);
	maybe = 0;
	netmatch = afswitch[af].af_netmatch;
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if ((ifp->int_flags & IFF_UP) == 0)
			continue;
		if (ifp->int_addr.sa_family != af)
			continue;
		if (ifp->int_flags & IFF_POINTOPOINT) {
			if (same(&ifp->int_dstaddr, addr))
				break;
			continue;
		}
		if (same(&ifp->int_addr, addr))
			/* Loopback */
			break;
		if (maybe == 0 && (*netmatch)(addr, &ifp->int_addr))
			maybe = ifp;
	}
	if (ifp == 0)
		ifp = maybe;
	return (ifp);
}

/*
 * An interface has declared itself down - remove it completely
 * from our routing tables but keep the interface structure around.
 */
void
if_purge(struct interface *pifp)
{
	rtpurgeif(pifp);
	pifp->int_flags &= ~IFF_UP;
}

/*
 * Add an interface
 */
void
if_add(char *name, int flags)
{
	struct interface *ifp;

	ifp = (struct interface *)malloc(sizeof (struct interface));
	if (ifp == 0) {
		(void) printf("routed: out of memory\n");
		return;
	}
	bzero((char *)ifp, sizeof (struct interface));
	ifp->int_name = malloc((unsigned)strlen(name) + 1);
	if (ifp->int_name == 0) {
		(void) fprintf(stderr, "routed: if_add: out of memory\n");
		free((char *)ifp);
		return;
	}
	(void) strcpy(ifp->int_name, name);
	ifp->int_flags = flags;
	ifp->int_next = ifnet;
	ifnet = ifp;
	traceinit(ifp);
}

#ifdef DEBUG
extern char *inet_ntoa();
char *
long2str(mask)
	ulong_t mask;
{
	struct in_addr ina;

	ina.s_addr = mask;
	return (inet_ntoa(ina));
}

void
if_dump2(fd)
	FILE *fd;
{
	register struct interface *ifp;
	struct sockaddr_in *sin;
	static struct bits {
		ulong_t	t_bits;
		char	*t_name;
	} flagbits[] = {
		{ IFF_UP,		"UP" },
		{ IFF_BROADCAST,	"BROADCAST" },
		{ IFF_LOOPBACK,		"LOOPBACK" },
		{ IFF_POINTOPOINT,	"POINTOPOINT" },
		{ IFF_NORIPOUT,		"NORIPOUT" },
		{ IFF_NORIPIN,		"NORIPIN" },
		{ IFF_SUBNET,		"SUBNET" },
		{ IFF_PASSIVE,		"PASSIVE" },
		{ IFF_INTERFACE,	"INTERFACE" },
		{ IFF_PRIVATE,		"PRIVATE" },
		{ IFF_REMOTE,		"REMOTE" },
		{ IFF_NORTEXCH,		"NORTEXCH" },
		{ 0 }
	};
	register struct bits *p;
	char *cp;
	int first;

	if (fd == NULL)
		return;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		(void) fprintf(fd, "interface %s:\n",
		    (ifp->int_name != NULL) ? ifp->int_name : "(noname)");

		(void) fprintf(fd, "\tflags ");
		cp = " %s";
		for (first = 1, p = flagbits; p->t_bits > 0; p++) {
			if ((ifp->int_flags & p->t_bits) == 0)
				continue;
			(void) fprintf(fd, cp, p->t_name);
			if (first) {
				cp = "|%s";
				first = 0;
			}
		}
		if (first)
			(void) fprintf(fd, " 0");

		(void) fprintf(fd, "\ttransitions %d\n", ifp->int_transitions);
		if ((ifp->int_flags & IFF_UP) == 0)
			continue;
		sin = (struct sockaddr_in *)&ifp->int_addr;
		(void) fprintf(fd, "\taddress %s\n", inet_ntoa(sin->sin_addr));
		sin = (struct sockaddr_in *)&ifp->int_broadaddr;
		if (ifp->int_flags & IFF_BROADCAST) {
			(void) fprintf(fd, "\tbroadcast address %s\n",
			    inet_ntoa(sin->sin_addr));
		}
		sin = (struct sockaddr_in *)&ifp->int_dstaddr;
		if (ifp->int_flags & IFF_POINTOPOINT) {
			(void) fprintf(fd, "\tremote address %s\n",
			    inet_ntoa(sin->sin_addr));
		}
		(void) fprintf(fd, "\tmetric %d\n", ifp->int_metric);
		(void) fprintf(fd, "\tnetwork %s, ",
		    long2str(ifp->int_net));
		(void) fprintf(fd, "netmask %s\n",
		    long2str(ifp->int_netmask));
		(void) fprintf(fd, "\tsubnetwork %s, ",
		    long2str(ifp->int_subnet));
		(void) fprintf(fd, "subnetmask %s\n",
		    long2str(ifp->int_subnetmask));
	}
	(void) fflush(fd);
}

void
if_dump()
{
	if (ftrace)
		if_dump2(ftrace);
	else
		if_dump2(stderr);
}
#endif /* DEBUG */
