/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tables.c 1.19	98/06/23 SMI"	/* SVr4.0 1.2	*/

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
 * 	(c) 1986,1987,1988,1989,1991,1992,1993,1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <syslog.h>

#ifndef DEBUG
#define	DEBUG	0
#endif

/* simulate vax insque and remque instructions. */

typedef struct vq {
	caddr_t		fwd, back;
} vq_t;

#define	insque(e, p)	((vq_t *)e)->back = (caddr_t)(p); \
			((vq_t *)e)->fwd = \
				(caddr_t)((vq_t *)((vq_t *)p)->fwd); \
			((vq_t *)((vq_t *)p)->fwd)->back = (caddr_t)(e); \
			((vq_t *)p)->fwd = (caddr_t)(e);

#define	remque(e)	((vq_t *)((vq_t *)e)->back)->fwd =  \
					(caddr_t)((vq_t *)e)->fwd; \
			((vq_t *)((vq_t *)e)->fwd)->back = \
					(caddr_t)((vq_t *)e)->back; \
			((vq_t *)e)->fwd = (caddr_t)0; \
			((vq_t *)e)->back = (caddr_t)0;
int	install = !DEBUG;		/* if 1 call kernel */

extern int iosoc;

static void rtpurgegate(struct sockaddr *gate);

static void
log_change(int level, struct rt_entry *orig, struct rt_entry *new)
{
	char buf1[64], buf2[64], buf3[64];

	strcpy(buf1,
	    (*afswitch[new->rt_dst.sa_family].af_format)(&new->rt_dst));
	strcpy(buf2,
	    (*afswitch[orig->rt_router.sa_family].af_format)(&orig->rt_router));
	strcpy(buf3,
	    (*afswitch[new->rt_router.sa_family].af_format)(&new->rt_router));
	syslog(level, "\tdst %s from gw %s to gw %s metric %d",
	    buf1, buf2, buf3, new->rt_metric);
}

static void
log_single(int level, struct rt_entry *rt)
{
	char buf1[64], buf2[64];

	strcpy(buf1,
	    (*afswitch[rt->rt_dst.sa_family].af_format)(&rt->rt_dst));
	strcpy(buf2,
	    (*afswitch[rt->rt_router.sa_family].af_format)(&rt->rt_router));
	syslog(level, "\tdst %s gw %s metric %d",
	    buf1, buf2, rt->rt_metric);
}

/*
 * Lookup dst in the tables for an exact match.
 */
struct rt_entry *
rtlookup(struct sockaddr *dst)
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register uint_t hash;
	struct afhash h;
	int doinghost = 1;

	if (dst->sa_family >= (unsigned)af_max)
		return (0);
	(*afswitch[dst->sa_family].af_hash)(dst, &h);
	hash = h.afh_hosthash;
	rh = &hosthash[hash & ROUTEHASHMASK];
again:
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (equal(&rt->rt_dst, dst))
			return (rt);
	}
	if (doinghost) {
		doinghost = 0;
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
		goto again;
	}
	return (0);
}

/*
 * Lookup dst in the tables for an exact match of both dst and router.
 */
struct rt_entry *
rtlookup2(struct sockaddr *dst, struct sockaddr *router)
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register uint_t hash;
	struct afhash h;
	int doinghost = 1;

	if (dst->sa_family >= (unsigned)af_max)
		return (0);
	(*afswitch[dst->sa_family].af_hash)(dst, &h);
	hash = h.afh_hosthash;
	rh = &hosthash[hash & ROUTEHASHMASK];
again:
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (equal(&rt->rt_dst, dst) &&
		    equal(&rt->rt_router, router))
			return (rt);
	}
	if (doinghost) {
		doinghost = 0;
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
		goto again;
	}
	return (0);
}

/*
 * Find a route to dst as the kernel would.
 */
struct rt_entry *
rtfind(struct sockaddr *dst)
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register uint_t hash;
	struct afhash h;
	int af = dst->sa_family;
	int doinghost = 1, (*match)();

	if (af >= af_max)
		return (0);
	(*afswitch[af].af_hash)(dst, &h);
	hash = h.afh_hosthash;
	rh = &hosthash[hash & ROUTEHASHMASK];
	match = NULL;

again:
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (doinghost) {
			if (equal(&rt->rt_dst, dst))
				return (rt);
		} else {
			if (rt->rt_dst.sa_family == af &&
			    (*match)(&rt->rt_dst, dst))
				return (rt);
		}
	}
	if (doinghost) {
		doinghost = 0;
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
		match = afswitch[af].af_netmatch;
		goto again;
	}
	return (0);
}

void
rtadd(struct sockaddr *dst, struct sockaddr *gate, int metric,
    int state, struct interface *ifp)
{
	struct afhash h;
	register struct rt_entry *rt;
	struct rthash *rh;
	int af = dst->sa_family, flags;
	uint_t hash;

	if (af >= af_max || af == 0)
		return;
	if (metric >= HOPCNT_INFINITY)
		return;
	(*afswitch[af].af_hash)(dst, &h);
	flags = (*afswitch[af].af_rtflags)(dst);
	/*
	 * Subnet flag isn't visible to kernel, move to state.	XXX
	 */
	if (flags & RTF_SUBNET) {
		state |= RTS_SUBNET;
		flags &= ~RTF_SUBNET;
	}
	if (flags & RTF_HOST) {
		hash = h.afh_hosthash;
		rh = &hosthash[hash & ROUTEHASHMASK];
	} else {
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
	}
	rt = (struct rt_entry *)malloc(sizeof (*rt));
	if (rt == 0)
		return;
	rt->rt_hash = hash;
	rt->rt_dst = *dst;
	rt->rt_router = *gate;
	rt->rt_metric = metric;
	rt->rt_timer = 0;
	rt->rt_flags = RTF_UP | flags;
	rt->rt_state = state | RTS_CHANGED;
	rt->rt_ifp = ifp;
	if (rt->rt_ifp == NULL) {
		if (state & RTS_POINTOPOINT)
			rt->rt_ifp = if_ifwithdstaddr(&rt->rt_router);
		else
			rt->rt_ifp = if_ifwithdst(&rt->rt_router);
	}
	if (rt->rt_ifp == NULL)
		rt->rt_ifp = if_ifwithaddr(&rt->rt_router);
	if (!(rt->rt_state & RTS_INTERFACE))
		rt->rt_flags |= RTF_GATEWAY;
	insque(rt, rh);
	TRACE_ACTION("ADD", rt);
	/*
	 * If the ioctl fails because the gateway is unreachable
	 * from this host, discard the entry.  This should only
	 * occur because of an incorrect entry in /etc/gateways.
	 */
	if (install && (rt->rt_state & (RTS_INTERNAL | RTS_EXTERNAL)) == 0 &&
	    ioctl(iosoc, SIOCADDRT, (char *)&rt->rt_rt) < 0) {
		if (errno != EEXIST) {
			syslog(LOG_ERR, "rtadd SIOCADDRT: %m");
			log_single(LOG_ERR, rt);
		}
		if (errno == ENETUNREACH) {
			TRACE_ACTION("DELETE", rt);
			remque(rt);
			free((char *)rt);
		}
	}
}

/*
 * Handle the case when the metric changes but the gateway is the same
 * and when both gateway and metric changes. Note that routes with
 * metric >= INFITIY are not in the kernel.
 */
void
rtchange(struct rt_entry *rt, struct sockaddr *gate, short metric)
{
	int doioctl = 0, doioctldelete, metricchanged = 0;
	int oldmetric;
	struct rt_entry oldroute;
	int af = rt->rt_dst.sa_family, flags;

	if (gate->sa_family != af)
		return;
	flags = (*afswitch[af].af_rtflags)(&rt->rt_dst);
	if (!(rt->rt_state & RTS_INTERFACE))
		flags |= RTF_GATEWAY;
	if ((rt->rt_flags ^ flags) & (RTF_HOST|RTF_GATEWAY)) {
		/*
		 * Could be caused e.g. by a host route being changed to a
		 * net route due to the netmask having changed.
		 */
		int state;
		struct sockaddr dst;
		struct interface *ifp;

		TRACE_ACTION("CHANGE - flags", rt);
		state = rt->rt_state;
		dst = rt->rt_dst;
		ifp = rt->rt_ifp;
		rtdelete(rt);
		rtadd(&dst, gate, metric, state, ifp);
		return;
	}
	if (metric >= HOPCNT_INFINITY) {
		rtdown(rt);
		return;
	}
	if (!equal(&rt->rt_router, gate) && (rt->rt_state & RTS_INTERNAL) == 0)
		doioctl++;
	oldmetric = rt->rt_metric;
	if (oldmetric >= HOPCNT_INFINITY)
		doioctldelete = 0;
	else
		doioctldelete = doioctl;
	if (metric != rt->rt_metric)
		metricchanged++;
	rt->rt_timer = 0;
	if (doioctl || metricchanged) {
		TRACE_ACTION("CHANGE FROM", rt);
		if ((rt->rt_state & RTS_INTERFACE) && metric) {
			rt->rt_state &= ~RTS_INTERFACE;
			if (rt->rt_ifp == NULL) {
				syslog(LOG_ERR,
				    "route to %s had interface flag but no "
				    "pointer",
				    (*afswitch[gate->sa_family].
					af_format)(&rt->rt_dst));
			} else {
				syslog(LOG_ERR,
				    "changing route from interface %s "
				    "(timed out)",
				    (rt->rt_ifp->int_name != NULL) ?
					rt->rt_ifp->int_name : "(noname)");
			}
		}
		if (doioctl) {
			oldroute = *rt;
			rt->rt_router = *gate;
			rt->rt_ifp = if_ifwithdst(&rt->rt_router);
			if (rt->rt_ifp == 0)
				rt->rt_ifp = if_ifwithaddr(&rt->rt_router);
		}
		rt->rt_metric = metric;
		if (!(rt->rt_state & RTS_INTERFACE))
			rt->rt_flags |= RTF_GATEWAY;
		else
			rt->rt_flags &= ~RTF_GATEWAY;
		rt->rt_state |= RTS_CHANGED;
		TRACE_ACTION("CHANGE TO", rt);
	}
	if (install && rt->rt_state != RTS_INTERNAL) {
		if (doioctldelete) {
			if (ioctl(iosoc, SIOCADDRT, (char *)&rt->rt_rt) < 0)
				if (errno != EEXIST) {
					syslog(LOG_ERR,
					    "rtchange SIOCADDRT: %m");
					log_change(LOG_ERR, rt, &oldroute);
				}
			if (ioctl(iosoc, SIOCDELRT,
			    (char *)&oldroute.rt_rt) < 0) {
				syslog(LOG_ERR, "rtchange SIOCDELRT: %m");
				log_change(LOG_ERR, rt, &oldroute);
			}
		} else if (doioctl || oldmetric >= HOPCNT_INFINITY) {
			if (ioctl(iosoc, SIOCADDRT, (char *)&rt->rt_rt) < 0 &&
			    errno != EEXIST) {
				syslog(LOG_ERR, "rtchange2 SIOCADDRT: %m");
				log_change(LOG_ERR, rt, &oldroute);
			}
		}
	}
}

void
rtdown(struct rt_entry *rt)
{

	if (rt->rt_metric != HOPCNT_INFINITY) {
		TRACE_ACTION("DELETE", rt);
		if (install &&
		    (rt->rt_state & (RTS_INTERNAL | RTS_EXTERNAL)) == 0 &&
		    ioctl(iosoc, SIOCDELRT, (char *)&rt->rt_rt)) {
			if ((errno == ESRCH) &&
			    (rt->rt_state & RTS_INTERFACE)) {
				/*
				 * We get these errors when an interface changes
				 * netmask since the kernel has already changed
				 * the routing table.
				 */
				perror("SIOCDELRT");
			} else {
				syslog(LOG_ERR, "SIOCDELRT: %m");
				log_single(LOG_ERR, rt);
			}
		}
		rt->rt_metric = HOPCNT_INFINITY;
		rt->rt_state |= RTS_CHANGED;
	}
	if (rt->rt_timer < EXPIRE_TIME)
		rt->rt_timer = EXPIRE_TIME;
	if (rt->rt_state & RTS_DEFAULT) {
		/* Remove all routes through the router */
		rtpurgegate(&rt->rt_router);
	}
}

void
rtdelete(struct rt_entry *rt)
{

	if (rt->rt_state & RTS_INTERFACE)
		if (rt->rt_ifp) {
			syslog(LOG_ERR,
			    "deleting route to interface %s (timed out)",
			    (rt->rt_ifp->int_name != NULL) ?
				rt->rt_ifp->int_name : "(noname)");
			log_single(LOG_ERR, rt);
		}
	rtdown(rt);
	remque(rt);
	free((char *)rt);
}

/*
 * If we have an interface to the wide, wide world,
 * add an entry for an Internet default route (wildcard) to the internal
 * tables and advertise it.  This route is not added to the kernel routes,
 * but this entry prevents us from listening to other people's defaults
 * and installing them in the kernel here.
 */
void
rtdefault(void)
{
	extern struct sockaddr inet_default;

	rtadd(&inet_default, &inet_default, 0,
		RTS_CHANGED | RTS_PASSIVE | RTS_INTERNAL, NULL);
}

void
rtinit(void)
{
	register struct rthash *rh;

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		rh->rt_forw = rh->rt_back = (struct rt_entry *)rh;
	for (rh = hosthash; rh < &hosthash[ROUTEHASHSIZ]; rh++)
		rh->rt_forw = rh->rt_back = (struct rt_entry *)rh;
}

void
rtpurgeif(struct interface *ifp)
{
	register struct rthash *rh;
	register struct rt_entry *rt;

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; ) {
			if (rt->rt_ifp == ifp) {
				rtdown(rt);
				rt->rt_ifp = 0;
				rt->rt_state &= ~RTS_INTERFACE;
			}
			rt = rt->rt_forw;
		}
	for (rh = hosthash; rh < &hosthash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; ) {
			if (rt->rt_ifp == ifp) {
				rtdown(rt);
				rt->rt_ifp = 0;
				rt->rt_state &= ~RTS_INTERFACE;
			}
			rt = rt->rt_forw;
		}
}

/* Used only if save_space is set when a RTS_DEFAULT entry goes away */
static void
rtpurgegate(struct sockaddr *gate)
{
	register struct rthash *rh;
	register struct rt_entry *rt;

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; ) {
			if (equal(&rt->rt_router, gate) &&
			    (rt->rt_state & RTS_DEFAULT) == 0) {
				rtdown(rt);
			}
			rt = rt->rt_forw;
		}
	for (rh = hosthash; rh < &hosthash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; ) {
			if (equal(&rt->rt_router, gate) &&
			    (rt->rt_state & RTS_DEFAULT) == 0) {
				rtdown(rt);
			}
			rt = rt->rt_forw;
		}
}

/*
 * Called when the subnetmask has changed on one or more interfaces.
 * Re-evaluates all non-interface routes by doing a rtchange so that
 * routes that were believed to be host routes before the netmask change
 * can be converted to network routes and vice versa.
 */
void
rtchangeall(void)
{
	register struct rthash *rh;
	register struct rt_entry *rt, *next_rt;

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh;
		    rt = next_rt) {
			next_rt = rt->rt_forw;
			if ((rt->rt_state & RTS_INTERFACE) == 0) {
				rtchange(rt, &rt->rt_router, rt->rt_metric);
			}
		}
	for (rh = hosthash; rh < &hosthash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh;
		    rt = next_rt) {
			next_rt = rt->rt_forw;
			if ((rt->rt_state & RTS_INTERFACE) == 0) {
				rtchange(rt, &rt->rt_router, rt->rt_metric);
			}
		}
}

#if DEBUG > 0
static void
rtdumpentry(fd, rt)
	FILE *fd;
	register struct rt_entry *rt;
{
	struct sockaddr_in *dst, *gate;
	static struct bits {
		ulong_t	t_bits;
		char	*t_name;
	} flagbits[] = {
		{ RTF_UP,		"UP" },
		{ RTF_GATEWAY,		"GATEWAY" },
		{ RTF_HOST,		"HOST" },
		{ RTF_DYNAMIC,		"DYNAMIC" },
		{ RTF_MODIFIED,		"MODIFIED" },
		{ RTF_SUBNET,		"SUBNET" },
		{ 0 }
	}, statebits[] = {
		{ RTS_PASSIVE,		"PASSIVE" },
		{ RTS_REMOTE,		"REMOTE" },
		{ RTS_INTERFACE,	"INTERFACE" },
		{ RTS_CHANGED,		"CHANGED" },
		{ RTS_INTERNAL,		"INTERNAL" },
		{ RTS_EXTERNAL,		"EXTERNAL" },
		{ RTS_SUBNET,		"SUBNET" },
		{ RTS_DEFAULT,		"DEFAULT" },
		{ RTS_POINTOPOINT,	"POINTOPOINT" },
		{ RTS_PRIVATE,		"PRIVATE" },
		{ 0 }
	};
	register struct bits *p;
	register int first;
	char *cp;

	if (fd == NULL)
		return;
	dst = (struct sockaddr_in *)&rt->rt_dst;
	gate = (struct sockaddr_in *)&rt->rt_router;
	(void) fprintf(fd, "dst %s ", inet_ntoa(dst->sin_addr));
	(void) fprintf(fd, "via %s metric %d timer %d",
		inet_ntoa(gate->sin_addr), rt->rt_metric, rt->rt_timer);
	if (rt->rt_ifp) {
		(void) fprintf(fd, " if %s",
		    (rt->rt_ifp->int_name != NULL) ?
			rt->rt_ifp->int_name : "(noname)");
	}
	(void) fprintf(fd, " state");
	cp = " %s";
	for (first = 1, p = statebits; p->t_bits > 0; p++) {
		if ((rt->rt_state & p->t_bits) == 0)
			continue;
		(void) fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	if (first)
		(void) fprintf(fd, " 0");
	if (rt->rt_flags != (RTF_UP | RTF_GATEWAY)) {
		cp = " %s";
		for (first = 1, p = flagbits; p->t_bits > 0; p++) {
			if ((rt->rt_flags & p->t_bits) == 0)
				continue;
			(void) fprintf(fd, cp, p->t_name);
			if (first) {
				cp = "|%s";
				first = 0;
			}
		}
	}
	(void) putc('\n', fd);
	(void) fflush(fd);
}

void
rtdump2(fd)
	FILE *fd;
{
	register struct rthash *rh;
	register struct rt_entry *rt;

	if (fd == NULL)
		return;
	(void) fprintf(fd, "NETWORKS\n");
	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; ) {
			rtdumpentry(fd, rt);
			rt = rt->rt_forw;
		}
	(void) fprintf(fd, "HOSTS\n");
	for (rh = hosthash; rh < &hosthash[ROUTEHASHSIZ]; rh++)
		for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; ) {
			rtdumpentry(fd, rt);
			rt = rt->rt_forw;
		}
}

void
rtdump()
{
	if (ftrace)
		rtdump2(ftrace);
	else
		rtdump2(stderr);
}
#endif /* DEBUG */
