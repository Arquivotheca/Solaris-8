/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)output.c	1.3	99/09/21 SMI"	/* SVr4.0 1.1	*/

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


/*
 * Routing Table Management Daemon
 */
#include "defs.h"

/*
 * Apply the function "supply" to all active
 * interfaces with a link-local address.
 */
void
supplyall(struct sockaddr_in6 *sin6, int rtstate, struct interface *skipif,
    boolean_t splith)
{
	struct interface *ifp;

	for (ifp = ifnet; ifp != NULL; ifp = ifp->int_next) {
		if ((ifp->int_flags & RIP6_IFF_UP) == 0)
			continue;
		if (ifp->int_flags & RIP6_IFF_NORTEXCH) {
			if (tracing & OUTPUT_BIT) {
				(void) fprintf(ftrace,
				    "Suppress sending RIPng response packet "
				    "on %s (no route exchange on interface)\n",
				    ifp->int_name);
				(void) fflush(ftrace);
			}
			continue;
		}
		if (ifp->int_sock == -1)
			continue;
		if (ifp == skipif)
			continue;
		if (!IN6_IS_ADDR_LINKLOCAL(&ifp->int_addr))
			continue;
		supply(sin6, ifp, rtstate, splith);
	}
}

static void
solicit(struct sockaddr_in6 *sin6, struct interface *ifp)
{
	msg->rip6_cmd = RIPCMD6_REQUEST;
	msg->rip6_vers = RIPVERSION6;
	msg->rip6_nets[0].rip6_prefix = in6addr_any;
	msg->rip6_nets[0].rip6_prefix_length = 0;
	msg->rip6_nets[0].rip6_metric = HOPCNT_INFINITY;
	sendpacket(sin6, ifp, sizeof (struct rip6), 0);
}

void
solicitall(struct sockaddr_in6 *sin6)
{
	struct interface *ifp;

	for (ifp = ifnet; ifp != NULL; ifp = ifp->int_next) {
		if ((ifp->int_flags & RIP6_IFF_UP) == 0)
			continue;
		if (ifp->int_flags & RIP6_IFF_NORTEXCH) {
			if (tracing & OUTPUT_BIT) {
				(void) fprintf(ftrace,
				    "Suppress sending RIPng request packet "
				    "on %s (no route exchange on interface)\n",
				    ifp->int_name);
				(void) fflush(ftrace);
			}
			continue;
		}
		if (ifp->int_sock == -1)
			continue;
		solicit(sin6, ifp);
	}
}


/*
 * Output a preformed packet.
 */
/*ARGSUSED*/
void
sendpacket(struct sockaddr_in6 *sin6, struct interface *ifp, int size,
    int flags)
{
	if (sendto(ifp->int_sock, packet, size, flags,
	    (struct sockaddr *)sin6, sizeof (*sin6)) < 0) {
		syslog(LOG_ERR, "sendpacket: sendto: %m");
		return;
	}
	TRACE_OUTPUT(ifp, sin6, sizeof (struct rip6));
	ifp->int_opackets++;
}

/*
 * Supply dst with the contents of the routing tables.
 * If this won't fit in one packet, chop it up into several.
 */
void
supply(struct sockaddr_in6 *sin6, struct interface *ifp, int rtstate,
    boolean_t splith)
{
	struct rt_entry *rt;
	struct netinfo6 *n = msg->rip6_nets;
	struct rthash *rh;
	int size, i, maxsize;
	uint8_t rtmetric;

	msg->rip6_cmd = RIPCMD6_RESPONSE;
	msg->rip6_vers = RIPVERSION6;

	/*
	 * Initialize maxsize to the size of the largest RIPng packet supported
	 * on the outgoing interface.
	 */
	maxsize = ifp->int_mtu - sizeof (ip6_t) - sizeof (struct udphdr);

	for (i = IPV6_ABITS; i >= 0; i--) {
		if (net_hashes[i] == NULL)
			continue;

		for (rh = net_hashes[i]; rh < &net_hashes[i][ROUTEHASHSIZ];
		    rh++) {
			for (rt = rh->rt_forw; rt != (struct rt_entry *)rh;
			    rt = rt->rt_forw) {

				if (IN6_IS_ADDR_LINKLOCAL(&rt->rt_dst))
					continue;
				if (IN6_IS_ADDR_UNSPECIFIED(&rt->rt_dst))
					continue;

				/* do not send if private */
				if (rt->rt_state & RTS_PRIVATE)
					continue;

				/*
				 * Don't resend the information
				 * on the network from which it was received.
				 */
				if (splith && rt->rt_ifp != NULL &&
				    strcmp(ifp->int_ifbase,
					rt->rt_ifp->int_ifbase) == 0) {
					if (dopoison)
						rtmetric = HOPCNT_INFINITY;
					else
						continue;
				} else {
					rtmetric = rt->rt_metric;
				}

				/*
				 * For dynamic updates, limit update to routes
				 * with the specified state.
				 */
				if (rtstate != 0 &&
				    (rt->rt_state & rtstate) == 0)
					continue;

				/*
				 * Check if there is space for another RTE.  If
				 * not, send the packet built up and reset n for
				 * the remaining RTEs.
				 */
				size = (char *)n - packet;
				if (size > maxsize - sizeof (struct netinfo6)) {
					sendpacket(sin6, ifp, size, 0);
					TRACE_OUTPUT(ifp, sin6, size);
					n = msg->rip6_nets;
				}
				n->rip6_prefix = rt->rt_dst;
				n->rip6_route_tag = rt->rt_tag;
				n->rip6_prefix_length = rt->rt_prefix_length;
				n->rip6_metric = min(rtmetric, HOPCNT_INFINITY);
				n++;
			} /* end of hash chain */
		} /* end of particular prefix length */
	} /* end of all prefix lengths */
	if (n != msg->rip6_nets) {
		size = (char *)n - packet;
		sendpacket(sin6, ifp, size, 0);
		TRACE_OUTPUT(ifp, sin6, size);
	}
}
