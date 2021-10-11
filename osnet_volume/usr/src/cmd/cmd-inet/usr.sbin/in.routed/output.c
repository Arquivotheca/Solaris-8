/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)output.c	1.11	99/07/29 SMI"	/* SVr4.0 1.1	*/

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
#include <net/if.h>

/*
 * Apply the function "f" to all non-passive
 * interfaces.  If the interface supports the
 * broadcasting or is point-to-point use it,
 * otherwise skip it, since we do not know who to send it to.
 */
void
toall(void (*f)(), int rtstate, struct interface *skipif)
{
	register struct interface *ifp;
	register struct sockaddr *dst;
	register int flags;
	extern struct interface *ifnet;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if ((ifp->int_flags & IFF_UP) == 0)
			continue;
		if (ifp->int_flags & IFF_NORIPOUT)
			continue;
		if (ifp->int_flags & IFF_NORTEXCH) {
			if (tracing & OUTPUT_BIT) {
				fprintf(ftrace,
				    "suppress sending RIP packet on %s "
				    "(no route exchange on interface)\n",
				    ifp->int_name);
				fflush(ftrace);
			}
			continue;
		}
		if ((ifp->int_flags & IFF_PASSIVE) || ifp == skipif)
			continue;
		if (ifp->int_flags & IFF_BROADCAST)
			dst = &ifp->int_broadaddr;
		else if (ifp->int_flags & IFF_POINTOPOINT)
			dst = &ifp->int_dstaddr;
		else
			continue;
		flags = ifp->int_flags & IFF_INTERFACE ? MSG_DONTROUTE : 0;
		(*f)(dst, flags, ifp, rtstate);
	}
}

/*
 * Output a preformed packet.
 */
/*ARGSUSED*/
void
sendpacket(struct sockaddr *dst, int flags, struct interface *ifp, int rtstate)
{

	(*afswitch[dst->sa_family].af_output)(s, flags,
		dst, sizeof (struct rip));
	TRACE_OUTPUT(ifp, dst, sizeof (struct rip));
}

/*
 * Supply dst with the contents of the routing tables.
 * If this won't fit in one packet, chop it up into several.
 */
void
supply(struct sockaddr *dst, int flags, struct interface *ifp, int rtstate)
{
	register struct rt_entry *rt;
	struct netinfo *n = msg->rip_nets;
	register struct rthash *rh;
	struct rthash *base = hosthash;
	int doinghost = 1, size;
	int (*output)() = afswitch[dst->sa_family].af_output;
	int (*sendsubnet)() = afswitch[dst->sa_family].af_sendsubnet;

	msg->rip_cmd = RIPCMD_RESPONSE;
	msg->rip_vers = RIPVERSION;
again:
	for (rh = base; rh < &base[ROUTEHASHSIZ]; rh++)
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		/*
		 * Don't resend the information
		 * on the network from which it was received.
		 * RTS_INTERNAL have an rt_ifp but we ignore that here
		 * to make sure that RTS_INTERNAL propagate everywhere
		 * they should.
		 */
		if (ifp && rt->rt_ifp == ifp && !(rt->rt_state & RTS_INTERNAL))
			continue;
		if (rt->rt_state & RTS_EXTERNAL)
			continue;
		/*
		 * Don't propagate information on "private" entries
		 */
		if (rt->rt_state & RTS_PRIVATE)
			continue;
		/*
		 * For dynamic updates, limit update to routes
		 * with the specified state.
		 */
		if (rtstate && (rt->rt_state & rtstate) == 0)
			continue;
		/*
		 * Limit the spread of subnet information
		 * to those who are interested.
		 */
		if (rt->rt_state & RTS_SUBNET) {
			if (rt->rt_dst.sa_family != dst->sa_family)
				continue;
			if ((*sendsubnet)(rt, dst) == 0)
				continue;
		}
		size = (char *)n - packet;
		if (size > MAXPACKETSIZE - sizeof (struct netinfo)) {
			(*output)(s, flags, dst, size);
			TRACE_OUTPUT(ifp, dst, size);
			n = msg->rip_nets;
		}
		n->rip_dst = rt->rt_dst;
		n->rip_dst.sa_family = htons(n->rip_dst.sa_family);
		n->rip_metric = htonl((ulong_t)min(rt->rt_metric,
		    HOPCNT_INFINITY));
		n++;
	}
	if (doinghost) {
		doinghost = 0;
		base = nethash;
		goto again;
	}
	if (n != msg->rip_nets) {
		size = (char *)n - packet;
		(*output)(s, flags, dst, size);
		TRACE_OUTPUT(ifp, dst, size);
	}
}
