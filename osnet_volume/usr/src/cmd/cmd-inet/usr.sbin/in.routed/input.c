/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)input.c	1.14	99/07/29 SMI"	/* SVr4.0 1.1	*/

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
 * 	(c) 1986,1987,1988,1989,1991,1992,1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <syslog.h>

extern struct sockaddr_in inet_default;
#define	DEFAULT_METRIC	1

/*
 * Process a newly received packet.
 */
void
rip_input(struct sockaddr *from, int size)
{
	register struct rt_entry *rt;
	register struct netinfo *n;
	register struct interface *ifp;
	int newsize;
	int changes = 0;
	register struct afswitch *afp;
	int answer = supplier;
	struct entryinfo	*e;

	ifp = 0;
	TRACE_INPUT(ifp, from, (int)size);
	if (from->sa_family >= (unsigned)af_max ||
	    (afp = &afswitch[from->sa_family])->af_hash == (int (*)())0) {
		return;
	}

	/*
	 * If the packet is recevied on an interface with IFF_NORTEXCH flag set,
	 * we ignore the packet.
	 * TRACE_INPUT might have already done interface lookup.
	 * TODO: this code can be re-written using one socket per interface
	 * to determine which interface the packet is recevied.
	 */
	if (((ifp != NULL) || ((ifp = if_iflookup(from)) != NULL)) &&
	    ifp->int_flags & IFF_NORTEXCH) {
		if (tracing & INPUT_BIT) {
			fprintf(ftrace,
			    "ignore received RIP packet on %s "
			    "(no route exchange on interface)\n",
			    ifp->int_name);
			fflush(ftrace);
		}
		return;
	}

	switch (msg->rip_cmd) {

	case RIPCMD_POLL:		/* request specifically for us */
		answer = 1;
		/* fall through */
		/*FALLTHRU*/
	case RIPCMD_REQUEST:		/* broadcasted request */
		newsize = 0;
		size -= 4 * sizeof (char);
		n = msg->rip_nets;
		while (size > 0) {
			if (size < sizeof (struct netinfo))
				break;
			size -= sizeof (struct netinfo);

			if (msg->rip_vers > 0) {
				n->rip_dst.sa_family =
					ntohs(n->rip_dst.sa_family);
				n->rip_metric = ntohl((ulong_t)n->rip_metric);
			}
			/*
			 * A single entry with sa_family == AF_UNSPEC and
			 * metric ``infinity'' means ``all routes''.
			 * We respond to routers only if we are acting
			 * as a supplier, or to anyone other than a router
			 * (eg, query).
			 */
			if (n->rip_dst.sa_family == AF_UNSPEC &&
			    n->rip_metric == HOPCNT_INFINITY && size == 0) {
				if (answer || (*afp->af_portmatch)(from) == 0)
					supply(from, 0, ifp, 0);
				return;
			}
			if (n->rip_dst.sa_family < (unsigned)af_max &&
			    afswitch[n->rip_dst.sa_family].af_hash)
				rt = rtlookup(&n->rip_dst);
			else
				rt = 0;
			n->rip_metric = rt == 0 ? HOPCNT_INFINITY :
				min(rt->rt_metric, HOPCNT_INFINITY);
			if (msg->rip_vers > 0) {
				n->rip_dst.sa_family =
					htons(n->rip_dst.sa_family);
				n->rip_metric = htonl((ulong_t)n->rip_metric);
			}
			n++, newsize += sizeof (struct netinfo);
		}
		if (answer && newsize > 0) {
			msg->rip_cmd = RIPCMD_RESPONSE;
			newsize += sizeof (int);
			(*afp->af_output)(s, 0, from, newsize);
		}
		return;

	case RIPCMD_TRACEON:
	case RIPCMD_TRACEOFF:
		syslog(LOG_ERR, "trace command from %s - ignored",
		    (*afswitch[from->sa_family].af_format)(from));
		return;

	case RIPCMD_RESPONSE:
		/* verify message came from a router */
		if ((*afp->af_portmatch)(from) == 0)
			return;
		(*afp->af_canon)(from);
		/* are we talking to ourselves? */
		ifp = if_ifwithaddr(from);
		if (ifp) {
			rt = rtfind(from);
			if (rt == 0 || (rt->rt_state & RTS_INTERFACE) == 0)
				addrouteforif(ifp);
			else
				rt->rt_timer = 0;
			return;
		}
		/*
		 * Update timer for interface on which the packet arrived.
		 * If from other end of a point-to-point link that isn't
		 * in the routing tables, (re-)add the route.
		 */
		if ((rt = rtfind(from)) != NULL &&
		    (rt->rt_state & (RTS_INTERFACE | RTS_REMOTE))) {
			rt->rt_timer = 0;
		} else if (ifp = if_ifwithdstaddr(from)) {
			addrouteforif(ifp);
		} else if (if_iflookup(from) == 0) {
			static struct sockaddr_in	badrouter;

			if (bcmp((char *)from, (char *)&badrouter,
			    sizeof (*from))) {
				bcopy((char *)from, (char *)&badrouter,
				    sizeof (*from));
				syslog(LOG_INFO,
				    "packet from unknown router, %s",
				    (*afswitch[from->sa_family].
					af_format)(from));
			}
			return;
		}
		if ((ifp = if_iflookup(from)) != NULL &&
		    ifp->int_flags & IFF_NORIPIN)
			return;
		size -= 4 * sizeof (char);
		n = msg->rip_nets;
		for (; !(!supplier && save_space) && size > 0;
		    size -= sizeof (struct netinfo), n++) {
			if (size < sizeof (struct netinfo))
				break;
			if (msg->rip_vers > 0) {
				n->rip_dst.sa_family =
					ntohs(n->rip_dst.sa_family);
				n->rip_metric = ntohl((ulong_t)n->rip_metric);
			}
			if ((unsigned)n->rip_metric > HOPCNT_INFINITY)
				continue;
			if (n->rip_dst.sa_family >= (unsigned)af_max ||
			    (afp = &afswitch[n->rip_dst.sa_family])->af_hash ==
			    (int (*)())0) {
				if (ftrace) {
					fprintf(ftrace,
					    "route in unsupported address "
					    "family (%d), from %s (af %d)\n",
					    n->rip_dst.sa_family,
					    (*afswitch[from->sa_family].
						af_format)(from),
					    from->sa_family);
				}
				continue;
			}
			if (((*afp->af_checkhost)(&n->rip_dst)) == 0) {
				if (ftrace) {
					fprintf(ftrace, "bad host %s ",
					    (*afswitch[from->sa_family].
						af_format)(&n->rip_dst));
					fprintf(ftrace,
					    "in route from %s, metric %d\n",
					    (*afswitch[from->sa_family].
						af_format)(from),
					    n->rip_metric);
					fflush(ftrace);
				}
				continue;
			}
			/* Include metric for incomming interface */
			n->rip_metric += IFMETRIC(ifp);

			rt = rtlookup(&n->rip_dst);
			/*
			 * 4.3 BSD for some reason also special-cased
			 * INTERNAL INTERFACE routes here. This causes an
			 * extra non-subnet route to appear in the routing
			 * tables of routers at the outside edges of a
			 * subnetted network.  It does not seem to cause
			 * problems, but why????
			 */
			if (rt == 0) {
				rt = rtfind(&n->rip_dst);
				if (rt && equal(from, &rt->rt_router) &&
				    rt->rt_metric == n->rip_metric)
					continue;
				if (n->rip_metric < HOPCNT_INFINITY) {
					rtadd(&n->rip_dst, from, n->rip_metric,
					    0, NULL);
				    changes++;
				}
				continue;
			}

			/*
			 * Update if from gateway and different,
			 * shorter, or getting stale and equivalent.
			 */
			if (equal(from, &rt->rt_router)) {
				if (n->rip_metric != rt->rt_metric) {
					rtchange(rt, from, n->rip_metric);
					changes++;
				} else if (n->rip_metric < HOPCNT_INFINITY) {
					rt->rt_timer = 0;
				}
			} else if ((unsigned)(n->rip_metric) < rt->rt_metric ||
			    (rt->rt_timer > (EXPIRE_TIME/2) &&
			    rt->rt_metric == n->rip_metric)) {
				rtchange(rt, from, n->rip_metric);
				changes++;
			}
		}
		/*
		 * If we are not grabbing all the routing entries add
		 * a default route to each sender of RIP response packets.
		 */
		if (!supplier && save_space) {
			rt = rtlookup2((struct sockaddr *)&inet_default, from);
			if (rt == 0) {
				rtadd((struct sockaddr *)&inet_default, from,
				    DEFAULT_METRIC, RTS_DEFAULT, NULL);
			} else if (rt->rt_metric != DEFAULT_METRIC) {
				rtchange(rt, from, DEFAULT_METRIC);
			} else {
				rt->rt_timer = 0;
			}
		}
		if (changes && supplier)
			dynamic_update(ifp);
		return;
	case RIPCMD_POLLENTRY:
		n = msg->rip_nets;
		if (n->rip_dst.sa_family < (unsigned)af_max &&
		    afswitch[n->rip_dst.sa_family].af_hash)
			rt = rtfind(&n->rip_dst);
		else
			rt = 0;
		newsize = sizeof (struct entryinfo);
		if (rt) {	/* don't bother to check rip_vers */
			e = (struct entryinfo *)n;
			e->rtu_dst = rt->rt_dst;
			e->rtu_dst.sa_family =
				ntohs(e->rtu_dst.sa_family);
			e->rtu_router = rt->rt_router;
			e->rtu_router.sa_family =
				ntohs(e->rtu_router.sa_family);
			e->rtu_flags = ntohs((ushort_t)rt->rt_flags);
			e->rtu_state = ntohs((ushort_t)rt->rt_state);
			e->rtu_timer = ntohl((ulong_t)rt->rt_timer);
			if (rt->rt_state & RTS_INTERFACE) {
				e->rtu_metric = ntohl((ulong_t)rt->rt_metric -
				    IFMETRIC(rt->rt_ifp));
			} else {
				e->rtu_metric = ntohl((ulong_t)rt->rt_metric);
			}
			ifp = rt->rt_ifp;
			if (ifp) {
				e->int_flags = ntohl((ulong_t)ifp->int_flags);
				(void) strncpy(e->int_name,
				    rt->rt_ifp->int_name, sizeof (e->int_name));
			} else {
				e->int_flags = 0;
				(void) strcpy(e->int_name, "(none)");
			}
		} else {
			bzero((char *)n, newsize);
		}
		(*afp->af_output)(s, 0, from, newsize);
		return;
	}
}

/*
 * If changes have occurred, and if we have not sent a broadcast
 * recently, send a dynamic update.  This update is sent only
 * on interfaces other than the one on which we received notice
 * of the change.  If we are within MIN_WAITTIME of a full update,
 * don't bother sending; if we just sent a dynamic update
 * and set a timer (nextbcast), delay until that time.
 * If we just sent a full update, delay the dynamic update.
 * Set a timer for a randomized value to suppress additional
 * dynamic updates until it expires; if we delayed sending
 * the current changes, set needupdate.
 */
void
dynamic_update(struct interface *ifp)
{
	if (now.tv_sec - lastfullupdate.tv_sec < SUPPLY_INTERVAL-MAX_WAITTIME) {
		ulong_t delay;

		if (now.tv_sec - lastbcast.tv_sec >= MIN_WAITTIME &&
		    /* BEGIN CSTYLED */
		    timercmp(&nextbcast, &now, <)) {
		    /* END CSTYLED */
			TRACE_ACTION("send dynamic update",
			    (struct rt_entry *)NULL);
			toall(supply, RTS_CHANGED, ifp);
			lastbcast = now;
			needupdate = 0;
			nextbcast.tv_sec = 0;
		} else {
			needupdate++;
			TRACE_ACTION("delay dynamic update",
			    (struct rt_entry *)NULL);
		}
#define	RANDOMDELAY()	(MIN_WAITTIME * 1000000 + \
	((ulong_t)rand() * 65536) % \
	((MAX_WAITTIME - MIN_WAITTIME) * 1000000))
		if (nextbcast.tv_sec == 0) {
			delay = RANDOMDELAY();
			if (tracing & ACTION_BIT) {
				fprintf(ftrace,
				    "inhibit dynamic update for %d usec\n",
				    (int)delay);
				fflush(ftrace);
			}
			nextbcast.tv_sec = delay / 1000000;
			nextbcast.tv_usec = delay % 1000000;
			timevaladd(&nextbcast, &now);
			/*
			 * If the next possibly dynamic update
			 * is within MIN_WAITTIME of the next full
			 * update, force the delay past the full
			 * update, or we might send a dynamic update
			 * just before the full update.
			 */
			if (nextbcast.tv_sec > lastfullupdate.tv_sec +
			    SUPPLY_INTERVAL - MIN_WAITTIME) {
				nextbcast.tv_sec = lastfullupdate.tv_sec +
					SUPPLY_INTERVAL + 1;
			}
		}
	}
}
