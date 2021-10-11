/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)igmp.c	1.39	99/09/26 SMI"

/*
 * Internet Group Management Protocol (IGMP) routines.
 * Multicast Listener Discovery Protocol (MLD) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb. 1995.
 *
 * MULTICAST 3.5.1.1
 */


#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/param.h>
#include <sys/socket.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/igmp_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/ip_multi.h>

#include <netinet/igmp.h>
#include <inet/ip_if.h>

#define		INFINITY	0xffffffffU
kmutex_t	igmp_ilm_lock;	/* Protect ilm_state and ilm_timer */
timeout_id_t	igmp_slowtimeout_id = 0;

static void	igmp_sendpkt(ilm_t *ilm, uchar_t type, ipaddr_t addr);
static void	mld_sendpkt(ilm_t *ilm, uchar_t type, const in6_addr_t *v6addr);


/*
 * NOTE: BSD timer is based on fastimo, which is 200ms. Solaris
 * is based on IGMP_TIMEOUT_INTERVAL which is 100ms.
 * Therefore, scalin factor is different, and Solaris timer value
 * is twice that of BSD's
 */
static int	igmp_timers_are_running;
static int 	igmp_time_since_last;	/* Time since last timeout */

boolean_t	mld_timers_are_running;
static int 	mld_time_since_last;	/* Time since last timeout */

/*
 * igmp_start_timers:
 * The unit for next is milliseconds.
 */
void
igmp_start_timers(unsigned next)
{
	if (next != (unsigned)-1 && !igmp_timers_are_running) {
		igmp_timers_are_running = 1;
		igmp_time_since_last = next;
		igmp_timeout_start(next);
	}

}

/*
 * mld_start_timers:
 * The unit for next is milliseconds.
 */
void
mld_start_timers(unsigned next)
{
	if (next != (unsigned)-1 && !mld_timers_are_running) {
		mld_timers_are_running = B_TRUE;
		mld_time_since_last = next;
		mld_timeout_start(next);
	}

}

/*
 * igmp_input:
 * Return 0 if the message is OK and should be handed to "raw" receivers.
 * Callers of igmp_input() may need to reinitialize variables that were copied
 * from the mblk as this calls pullupmsg().
 */
/* ARGSUSED */
int
igmp_input(queue_t *q, mblk_t *mp, ill_t *ill)
{
	igmpa_t 	*igmpa;
	ipha_t		*ipha = (ipha_t *)(mp->b_rptr);
	int		iphlen;
	int 		igmplen;
	ilm_t 		*ilm;
	uint32_t	src, dst;
	uint32_t 	group;
	unsigned	next;
	int		timer;	/* timer value in the igmp query header */
	ipif_t 		*ipif;

	ASSERT(ill != NULL);
	ASSERT(!ill->ill_isv6);
	++igmpstat.igps_rcv_total;

	iphlen = IPH_HDR_LENGTH(ipha);
	if ((mp->b_wptr - mp->b_rptr) < (iphlen + IGMP_MINLEN)) {
		if (!pullupmsg(mp, iphlen + IGMP_MINLEN)) {
			++igmpstat.igps_rcv_tooshort;
			freemsg(mp);
			return (-1);
		}
		ipha = (ipha_t *)(mp->b_rptr);
	}
	igmplen = ntohs(ipha->ipha_length) - iphlen;

	/*
	 * Validate lengths
	 */
	if (igmplen < IGMP_MINLEN) {
		++igmpstat.igps_rcv_tooshort;
		freemsg(mp);
		return (-1);
	}
	/*
	 * Validate checksum
	 */
	if (IP_CSUM(mp, iphlen, 0)) {
		++igmpstat.igps_rcv_badsum;
		freemsg(mp);
		return (-1);
	}
	igmpa = (igmpa_t *)(&mp->b_rptr[iphlen]);
	src = ipha->ipha_src;
	dst = ipha->ipha_dst;
	if (ip_debug > 1)
		(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
		    "igmp_input: src 0x%x, dst 0x%x on %s\n",
		    (int)ntohl(src), (int)ntohl(dst),
		    ill->ill_name);

	/*
	 * In the IGMPv2 specification, there are 3 states and a flag.
	 *
	 * In Non-Member state, we simply don't have a membership record.
	 * In Delaying Member state, our timer is running(ilm->ilm_timer)
	 * In Idle Member state, our timer is not running(ilm->ilm_timer == 0)
	 *
	 * The flag is ilm->ilm_state, it is set to IGMP_OTHERMEMBER if
	 * we have heard a report from another member, or IGMP_IREPORTEDLAST
	 * if I sent the last report.
	 */
	timer = (int)igmpa->igmpa_code *
	    IGMP_TIMEOUT_FREQUENCY/IGMP_TIMER_SCALE;

	switch (igmpa->igmpa_type) {

	case IGMP_MEMBERSHIP_QUERY:
		++igmpstat.igps_rcv_queries;

		if (igmpa->igmpa_code == 0) {
			/*
			 * Query from a old router.
			 * Remember that the querier on this
			 * interface is old, and set the timer to the
			 * value in RFC 1112.
			 */
			ill->ill_multicast_type = IGMP_V1_ROUTER;
			ill->ill_multicast_time = 0;


			/*
			 * BSD uses PR_FASTHZ, which is 5. See NOTE on
			 * top of the file.
			 */
			timer = IGMP_MAX_HOST_REPORT_DELAY *
			    IGMP_TIMEOUT_FREQUENCY;

			if (dst != htonl(INADDR_ALLHOSTS_GROUP) ||
			    igmpa->igmpa_group != 0) {
				++igmpstat.igps_rcv_badqueries;
				freemsg(mp);
				return (-1);
			}

		} else {
			/*
			 * Query from a new router
			 * Simply do a validity check
			 */
			group = igmpa->igmpa_group;
			if (group != 0 &&
			    (!CLASSD(group))) {
				++igmpstat.igps_rcv_badqueries;
				freemsg(mp);
				return (-1);
			}
		}

		if (ip_debug > 1)
			(void) mi_strlog(ill->ill_rq,
			    1, SL_TRACE,
			    "igmp_input: TIMER = igmp_code %d "
			    "igmp_type 0x%x",
			    (int)ntohs(igmpa->igmpa_code),
			    (int)ntohs(igmpa->igmpa_type));

		/*
		 * -Start the timers in all of our membership records for
		 * the physical interface on which the query arrived,
		 * excl. those that belong to the "all hosts" group(224.0.0.1).
		 * -Restart any timer that is already running but has a value
		 * longer that the requested timeout.
		 * -Use the value specified in the query message as the
		 * maximum timeout.
		 */
		mutex_enter(&igmp_ilm_lock);
		next = (unsigned)-1;
		for (ilm = ill->ill_ilm; ilm;
		    ilm = ilm->ilm_next) {
			ipaddr_t group;

			/*
			 * A multicast router joins INADDR_ANY address
			 * to enable promiscuous reception of all
			 * mcasts from the interface. This INADDR_ANY
			 *  is stored in the ilm_v6addr as V6 unspec addr
			 */
			if (!IN6_IS_ADDR_V4MAPPED(&ilm->ilm_v6addr))
				continue;
			IN6_V4MAPPED_TO_IPADDR(&ilm->ilm_v6addr, group);
			if (group == htonl(INADDR_ANY))
				continue;
			if (group != htonl(INADDR_ALLHOSTS_GROUP) &&
			    (igmpa->igmpa_group == 0) ||
			    (igmpa->igmpa_group == ilm->ilm_addr)) {
				if (ilm->ilm_timer == 0 ||
				    ilm->ilm_timer > timer) {
					ilm->ilm_timer =
					    IGMP_RANDOM_DELAY(ilm->ilm_addr,
					    ilm->ilm_ipif, timer);
					if (ilm->ilm_timer < next)
						next = ilm->ilm_timer;
				}
			}
		}
		igmp_start_timers(next);
		mutex_exit(&igmp_ilm_lock);
		break;

	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * can potentially get looped back if we are a multicast
		 * router, so discard reports sourced by me.
		 */
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			if (ipif->ipif_lcl_addr == src) {
				if (ip_debug > 1)
					(void) mi_strlog(ill->ill_rq,
					    1,
					    SL_TRACE,
					    "igmp_input: we are only "
					    "member src 0x%x ipif_local 0x%x",
					    (int)ntohl(src),
					    (int)
					    ntohl(ipif->ipif_lcl_addr));
				break;
			}
		}

		++igmpstat.igps_rcv_reports;
		group = igmpa->igmpa_group;
		if (!CLASSD(group)) {
			++igmpstat.igps_rcv_badreports;
			freemsg(mp);
			return (-1);
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 *
		 * Requires that a copy of *this* message it passed up
		 * to the raw interface which is done by our caller.
		 */
		if ((src & htonl(0xFF000000U)) == 0) {	/* Minimum net mask */
			/* Pick the first ipif on this ill */
			src = ill->ill_ipif->ipif_subnet;
			ip1dbg(("igmp_input: changed src to 0x%x\n",
			    (int)ntohl(src)));
			ipha->ipha_src = src;
		}

		/*
		 * If we belong to the group being reported,
		 * stop our timer for that group. Do this for all
		 * logical interfaces on the given physical interface.
		 */
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			ilm = ilm_lookup_ipif(ipif, group);
			if (ilm != NULL) {
				++igmpstat.igps_rcv_ourreports;

				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_timer = 0;
				ilm->ilm_state = IGMP_OTHERMEMBER;
				mutex_exit(&igmp_ilm_lock);
			}
		} /* for */
		break;
	}
	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket. Do not free the packet.
	 */
	return (0);
}

void
igmp_joingroup(ilm_t *ilm)
{
	ASSERT(!ilm->ilm_ipif->ipif_isv6);

	if (ilm->ilm_addr == htonl(INADDR_ALLHOSTS_GROUP)) {
		mutex_enter(&igmp_ilm_lock);
		ilm->ilm_timer = 0;
		ilm->ilm_state = IGMP_OTHERMEMBER;
		mutex_exit(&igmp_ilm_lock);
	} else {
		if (ilm->ilm_ipif->ipif_ill->ill_multicast_type ==
		    IGMP_V1_ROUTER)
			igmp_sendpkt(ilm, IGMP_V1_MEMBERSHIP_REPORT, 0);
		else
			igmp_sendpkt(ilm, IGMP_V2_MEMBERSHIP_REPORT, 0);

		/* Set the ilm timer value */
		mutex_enter(&igmp_ilm_lock);
		ilm->ilm_timer = IGMP_RANDOM_DELAY(ilm->ilm_addr,
		    ilm->ilm_ipif,
		    IGMP_MAX_HOST_REPORT_DELAY *
		    IGMP_TIMEOUT_FREQUENCY);
		igmp_start_timers(ilm->ilm_timer);

		ilm->ilm_state = IGMP_IREPORTEDLAST;
		mutex_exit(&igmp_ilm_lock);
	}

	if (ip_debug > 1) {
		(void) mi_strlog(ilm->ilm_ipif->ipif_ill->ill_rq, 1, SL_TRACE,
		    "igmp_joingroup: multicast_type %d timer %d",
		    (ilm->ilm_ipif->ipif_ill->ill_multicast_type),
		    (int)ntohl(ilm->ilm_timer));
	}

}

void
mld_joingroup(ilm_t *ilm)
{

	ASSERT(ilm->ilm_ipif->ipif_isv6);

	if (IN6_ARE_ADDR_EQUAL(&ipv6_all_hosts_mcast, &ilm->ilm_v6addr)) {
		mutex_enter(&igmp_ilm_lock);
		ilm->ilm_timer = 0;
		ilm->ilm_state = IGMP_OTHERMEMBER;
		mutex_exit(&igmp_ilm_lock);
	} else {
		mld_sendpkt(ilm, ICMP6_MEMBERSHIP_REPORT,
		    NULL);
		/* Set the ilm timer value */
		mutex_enter(&igmp_ilm_lock);
		ilm->ilm_timer = MLD_RANDOM_DELAY(ilm->ilm_v6addr,
		    ilm->ilm_ipif,
		    ICMP6_MAX_HOST_REPORT_DELAY *1000);
		mld_start_timers(ilm->ilm_timer);

		ilm->ilm_state = IGMP_IREPORTEDLAST;
		mutex_exit(&igmp_ilm_lock);
	}

	if (ip_debug > 1) {
		(void) mi_strlog(ilm->ilm_ipif->ipif_ill->ill_rq, 1, SL_TRACE,
		    "mld_joingroup: multicast_type %d timer %d",
		    (ilm->ilm_ipif->ipif_ill->ill_multicast_type),
		    (int)ntohl(ilm->ilm_timer));
	}

}

void
igmp_leavegroup(ilm_t *ilm)
{
	ill_t *ill = ilm->ilm_ipif->ipif_ill;

	ASSERT(!ill->ill_isv6);
	if (ilm->ilm_state == IGMP_IREPORTEDLAST &&
	    ill->ill_multicast_type != IGMP_V1_ROUTER &&
	    (ilm->ilm_addr != htonl(INADDR_ALLHOSTS_GROUP))) {
			igmp_sendpkt(ilm, IGMP_V2_LEAVE_GROUP,
			    (htonl(INADDR_ALLRTRS_GROUP)));
	}
}


void
mld_leavegroup(ilm_t *ilm)
{
	ill_t *ill = ilm->ilm_ipif->ipif_ill;

	ASSERT(ill->ill_isv6);
	if (ilm->ilm_state == IGMP_IREPORTEDLAST &&
	    (!IN6_ARE_ADDR_EQUAL(&ipv6_all_hosts_mcast,
		    &ilm->ilm_v6addr))) {
			mld_sendpkt(ilm, ICMP6_MEMBERSHIP_REDUCTION,
			    &ipv6_all_rtrs_mcast);
	}
}

/*
 * igmp_timeout_handler:
 * Called when there are timeout events, every next * TMEOUT_INTERVAL (tick).
 * Returns number of ticks to next event (or 0 if none).
 */
int
igmp_timeout_handler(void)
{
	ill_t	*ill;
	ilm_t 	*ilm;
	uint_t	next = INFINITY;
	int	elapsed;	/* Since last call */

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!igmp_timers_are_running)
		return (0);

	elapsed = igmp_time_since_last;
	if (elapsed == 0)
		elapsed = 1;

	igmp_timers_are_running = 0;
	for (ill = ill_g_head; ill != NULL; ill = ill->ill_next) {
		if (ill->ill_isv6) {
			continue;
		}
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {

			if (ilm->ilm_timer == 0)
				continue;
			if (ilm->ilm_timer <= elapsed) {
				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_timer = 0;
				mutex_exit(&igmp_ilm_lock);
				if (ill->ill_multicast_type ==
				    IGMP_V1_ROUTER) {
					igmp_sendpkt(ilm,
					    IGMP_V1_MEMBERSHIP_REPORT,
					    0);
				} else {
					igmp_sendpkt(ilm,
					    IGMP_V2_MEMBERSHIP_REPORT,
					    0);
				}
				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_state = IGMP_IREPORTEDLAST;
				mutex_exit(&igmp_ilm_lock);

				if (ip_debug > 1) {
					(void) mi_strlog(ill->ill_rq, 1,
					    SL_TRACE,
					    "igmp_timo_hlr 1: ilm_"
					    "timr %d elap %d typ %d"
					    " nxt %d",
					    (int)ntohl(ilm->ilm_timer),
					    elapsed,
					    (ill->ill_multicast_type),
					    next);
				}
			} else {
				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_timer -= elapsed;
				mutex_exit(&igmp_ilm_lock);
				igmp_timers_are_running = 1;
				if (ilm->ilm_timer < next)
					next = ilm->ilm_timer;

				if (ip_debug > 1) {
					(void) mi_strlog(ill->ill_rq, 1,
					    SL_TRACE,
					    "igmp_timo_hlr 2: ilm_timr"
					    " %d elap %d typ %d nxt"
					    " %d",
					    (int)ntohl(ilm->ilm_timer),
					    elapsed,
					    (ill-> ill_multicast_type),
					    next);
				}
			}
		}
	}
	if (next == (unsigned)-1)
		next = 0;
	igmp_time_since_last = next;
	return (next);
}

/*
 * mld_timeout_handler:
 * Called when there are timeout events, every next (tick).
 * Returns number of ticks to next event (or 0 if none).
 */
uint_t
mld_timeout_handler(void)
{
	ill_t	*ill;
	ilm_t 	*ilm;
	uint_t	next = INFINITY;
	int	elapsed;	/* Since last call */

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!mld_timers_are_running)
		return (0);

	elapsed = mld_time_since_last;
	if (elapsed == 0)
		elapsed = 1;

	mld_timers_are_running = B_FALSE;
	for (ill = ill_g_head; ill != NULL; ill = ill->ill_next) {
		if (!ill->ill_isv6) {
			continue;
		}
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {

			if (ilm->ilm_timer == 0)
				continue;
			if (ilm->ilm_timer <= elapsed) {
				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_timer = 0;
				mutex_exit(&igmp_ilm_lock);
				mld_sendpkt(ilm,
				    ICMP6_MEMBERSHIP_REPORT,
				    NULL);
				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_state = IGMP_IREPORTEDLAST;
				mutex_exit(&igmp_ilm_lock);

				if (ip_debug > 1) {
					(void) mi_strlog(ill->ill_rq, 1,
					    SL_TRACE,
					    "igmp_timo_hlr 1: ilm_"
					    "timr %d elap %d typ %d"
					    " nxt %d",
					    (int)ntohl(ilm->ilm_timer),
					    elapsed,
					    (ill-> ill_multicast_type), next);
				}
			} else {
				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_timer -= elapsed;
				mutex_exit(&igmp_ilm_lock);
				mld_timers_are_running = B_TRUE;
				if (ilm->ilm_timer < next)
					next = ilm->ilm_timer;

				if (ip_debug > 1) {
					(void) mi_strlog(ill->ill_rq, 1,
					    SL_TRACE,
					    "igmp_timo_hlr 2: ilm_timr"
					    " %d elap %d typ %d nxt"
					    " %d",
					    (int)ntohl(ilm->ilm_timer),
					    elapsed,
					    (ill-> ill_multicast_type), next);
				}
			}
		}
	}
	if (next == INFINITY)
		next = 0;
	mld_time_since_last = next;
	return (next);
}

/*
 * igmp_slowtimo:
 * - Resets to new router if we didnt we hear from the router
 *   in IGMP_AGE_THRESHOLD seconds.
 * - Resets slowtimeout.
 */
void
igmp_slowtimo(void *arg)
{
	queue_t *q = arg;	/* the ill_rq */
	ill_t	*ill = ill_g_head;

	for (ill = ill_g_head; ill != NULL; ill = ill->ill_next) {
		if (ill->ill_multicast_type == IGMP_V1_ROUTER) {
			ill->ill_multicast_time++;
			if (ill->ill_multicast_time >= (1000 *
				IGMP_AGE_THRESHOLD)/IGMP_SLOWTIMO_INTERVAL) {
				ill->ill_multicast_type = IGMP_V2_ROUTER;
			}
		}
	}
	igmp_slowtimeout_id = qtimeout(q, igmp_slowtimo, q,
		MSEC_TO_TICK(IGMP_SLOWTIMO_INTERVAL));

}


/*
 * igmp_sendpkt:
 * This will send to ip_wput like icmp_inbound.
 * Note that the lower ill (on which the membership is kept) is used
 * as an upper ill to pass in the multicast parameters.
 */
static void
igmp_sendpkt(ilm_t *ilm, uchar_t type, ipaddr_t addr)
{
	mblk_t	*mp;
	igmpa_t	*igmpa;
	ipha_t	*ipha;
	size_t	size  = sizeof (ipha_t) + sizeof (igmpa_t);
	ipif_t 	*ipif = ilm->ilm_ipif;
	ill_t 	*ill  = ipif->ipif_ill;	/* Will be the "lower" ill */

	ip1dbg(("igmp_sendpkt: for 0x%x on %s\n", (int)ntohl(ilm->ilm_addr),
	    ill->ill_name));
	mp = allocb(size, BPRI_HI);
	if (mp == NULL)
		return;
	bzero((char *)mp->b_rptr, size);
	mp->b_wptr = mp->b_rptr + size;

	ipha = (ipha_t *)(mp->b_rptr);
	igmpa = (igmpa_t *)(mp->b_rptr + sizeof (ipha_t));
	igmpa->igmpa_type   = type;
	igmpa->igmpa_code   = 0;
	igmpa->igmpa_group  = ilm->ilm_addr;
	igmpa->igmpa_cksum  = 0;
	igmpa->igmpa_cksum  = IP_CSUM(mp, sizeof (ipha_t), 0);

	ipha->ipha_version_and_hdr_length = (IP_VERSION << 4)
	    | IP_SIMPLE_HDR_LENGTH_IN_WORDS;
	ipha->ipha_type_of_service 	= 0;
	ipha->ipha_length 	= htons(IGMP_MINLEN + IP_SIMPLE_HDR_LENGTH);
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ttl 		= 1;
	ipha->ipha_protocol 	= IPPROTO_IGMP;
	ipha->ipha_hdr_checksum 	= 0;
	ipha->ipha_dst 		= addr ? addr : igmpa->igmpa_group;
	ipha->ipha_src 		= ipif->ipif_src_addr;
	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing demon can hear it.
	 */
	/*
	 * This will run multiple times for the same group if there are members
	 * on the same group for multiple ipif's on the same ill. The
	 * igmp_input code will suppress this due to the loopback thus we
	 * always loopback membership report.
	 */
	ASSERT(ill->ill_rq != NULL);
	ip_multicast_loopback(ill->ill_rq, ill, mp);

	ip_wput_multicast(ill->ill_wq, mp, ipif);

	++igmpstat.igps_snd_reports;
}

/*
 * mld_input:
 */
/* ARGSUSED */
void
mld_input(queue_t *q, mblk_t *mp, ill_t *ill)
{
	ip6_t		*ip6h = (ip6_t *)(mp->b_rptr);
	icmp6_mld_t	*mld;
	ilm_t		*ilm;
	uint16_t	hdr_length;
	uint16_t	exthdr_length;
	in6_addr_t	*v6group_ptr;
	unsigned	next;
	int		timer;	/* timer value in the igmp query header */
	ipif_t		*ipif;
	in6_addr_t	*lcladdr_ptr;

	BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInGroupMembTotal);

	/* Make sure the src address of the packet is link-local */
	if (!(IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src))) {
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInErrors);
		freemsg(mp);
		return;
	}

	/* XXX Ignore packet if hoplimit != 255 */

	/* XXX should we ignore packet if it contains routing headers? */
	/* Get to the icmp header part */
	if (ip6h->ip6_nxt != IPPROTO_ICMPV6) {
		hdr_length = ip_hdr_length_v6(mp, ip6h);
		exthdr_length = hdr_length;
	} else {
		hdr_length = IPV6_HDR_LEN;
		exthdr_length = 0;
	}

	/* An MLD packet must at least be 24 octets to be valid */
	if ((ip6h->ip6_plen - exthdr_length)  < sizeof (icmp6_mld_t)) {
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInErrors);
		freemsg(mp);
		return;
	}

	mld = (icmp6_mld_t *)(&mp->b_rptr[hdr_length]);

	/*
	 * In the MLD specification, there are 3 states and a flag.
	 *
	 * In Non-Listener state, we simply don't have a membership record.
	 * In Delaying state, our timer is running(ilm->ilm_timer)
	 * In Idle Member state, our timer is not running(ilm->ilm_timer == 0)
	 *
	 * The flag is ilm->ilm_state, it is set to IGMP_OTHERMEMBER if
	 * we have heard a report from another member, or IGMP_IREPORTEDLAST
	 * if I sent the last report.
	 */
	switch (mld->mld_type) {
	case ICMP6_MEMBERSHIP_QUERY:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInGroupMembQueries);

		/*
		 * Query from a router
		 * Simply do a validity check
		 */
		v6group_ptr = &mld->mld_group_addr;
		if (!(IN6_IS_ADDR_UNSPECIFIED(v6group_ptr)) &&
		    ((!IN6_IS_ADDR_MULTICAST(v6group_ptr)))) {
			BUMP_MIB(ill->
			    ill_icmp6_mib->ipv6IfIcmpInGroupMembBadQueries);
			freemsg(mp);
			return;
		}

		timer = (int)ntohs(mld->mld_maxdelay);
		if (ip_debug > 1) {
			(void) mi_strlog(ill->ill_rq,
			    1, SL_TRACE,
			    "mld_input: TIMER = mld_maxdelay %d "
			    "mld_type 0x%x",
			    timer, (int)mld->mld_type);
		}

		/*
		 * -Start the timers in all of our membership records for
		 * the physical interface on which the query arrived,
		 * excl:
		 *	1.  those that belong to the "all hosts" group,
		 *	2.  those with 0 scope, or 1 node-local scope.
		 *
		 * -Restart any timer that is already running but has a value
		 * longer that the requested timeout.
		 * -Use the value specified in the query message as the
		 * maximum timeout.
		 */
		mutex_enter(&igmp_ilm_lock);
		next = INFINITY;
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
			ASSERT(!IN6_IS_ADDR_V4MAPPED(&ilm->ilm_v6addr));
			if (IN6_IS_ADDR_UNSPECIFIED(&ilm->ilm_v6addr) ||
			    IN6_IS_ADDR_MC_NODELOCAL(&ilm->ilm_v6addr) ||
			    IN6_IS_ADDR_MC_RESERVED(&ilm->ilm_v6addr))
				continue;
			if ((!IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr,
			    &ipv6_all_hosts_mcast)) &&
			    (IN6_IS_ADDR_UNSPECIFIED(v6group_ptr)) ||
			    (IN6_ARE_ADDR_EQUAL(v6group_ptr,
			    &ilm->ilm_v6addr))) {
				if (timer == 0) {
					/* Respond immediately */
					mutex_enter(&igmp_ilm_lock);
					ilm->ilm_timer = 0;
					mutex_exit(&igmp_ilm_lock);
					mld_sendpkt(ilm,
					    ICMP6_MEMBERSHIP_REPORT,
					    NULL);
					mutex_enter(&igmp_ilm_lock);
					ilm->ilm_state = IGMP_IREPORTEDLAST;
					mutex_exit(&igmp_ilm_lock);
					continue;
				}
				if (ilm->ilm_timer == 0 ||
				    ilm->ilm_timer > timer) {
					ilm->ilm_timer =
					    MLD_RANDOM_DELAY
					    (ilm->ilm_v6addr,
					    ilm->ilm_ipif, timer);
					if (ilm->ilm_timer < next)
						next = ilm->ilm_timer;
				}
			}
		}
		mld_start_timers(next);
		mutex_exit(&igmp_ilm_lock);
		break;

	case ICMP6_MEMBERSHIP_REPORT: {

		ASSERT(ill->ill_ipif != NULL);
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * can potentially get looped back if we are a multicast
		 * router, so discard reports sourced by me.
		 */
		lcladdr_ptr = &(ill->ill_ipif->ipif_v6subnet);
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6lcl_addr,
			    lcladdr_ptr)) {
				if (ip_debug > 1) {
					char    buf1[INET6_ADDRSTRLEN];
					char	buf2[INET6_ADDRSTRLEN];

					(void) mi_strlog(ill->ill_rq,
					    1,
					    SL_TRACE,
					    "mld_input: we are only "
					    "member src %s ipif_local %s",
					    inet_ntop(AF_INET6, lcladdr_ptr,
					    buf1, sizeof (buf1)),
					    inet_ntop(AF_INET6,
					    &ipif->ipif_v6lcl_addr,
					    buf2, sizeof (buf2)));
				}
				break;
			}
		}
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInGroupMembResponses);

		v6group_ptr = &mld->mld_group_addr;
		if (!IN6_IS_ADDR_MULTICAST(v6group_ptr)) {
			BUMP_MIB(ill->ill_icmp6_mib->
			    ipv6IfIcmpInGroupMembBadReports);
			freemsg(mp);
			return;
		}


		/*
		 * If we belong to the group being reported,
		 * stop our timer for that group.
		 */
		ilm = ilm_lookup_ill_v6(ill, v6group_ptr);
		if (ilm != NULL) {
			BUMP_MIB(ill-> ill_icmp6_mib->
			    ipv6IfIcmpInGroupMembOurReports);

			mutex_enter(&igmp_ilm_lock);
			ilm->ilm_timer = 0;
			ilm->ilm_state = IGMP_OTHERMEMBER;
			mutex_exit(&igmp_ilm_lock);
		}
		break;
	}
	case ICMP6_MEMBERSHIP_REDUCTION:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInGroupMembReductions);
		break;
	}
	/*
	 * All MLD packets have already been passed up to any
	 * process(es) listening on a ICMP6 raw socket. This
	 * has been accomplished in ip_deliver_local_v6 prior to
	 * this function call. It is assumed that the multicast daemon
	 * will have a SOCK_RAW IPPROTO_ICMPV6 (and presumbly use the
	 * ICMP6_FILTER socket option to only receive the MLD messages)
	 * Thus we can free the MLD message block here
	 */
	freemsg(mp);
}

/*
 * Send packet with hoplimit 255
 */
static void
mld_sendpkt(ilm_t *ilm, uchar_t type, const in6_addr_t *v6addr)
{
	mblk_t		*mp;
	icmp6_mld_t	*mld;
	ip6_t 		*ip6h;
	size_t		size = IPV6_HDR_LEN + sizeof (icmp6_mld_t);
	ipif_t		*ipif = ilm->ilm_ipif;
	ill_t		*ill = ipif->ipif_ill;   /* Will be the "lower" ill */

	ASSERT(ill->ill_isv6);

	mp = allocb(size, BPRI_HI);
	if (mp == NULL)
		return;
	bzero(mp->b_rptr, size);
	mp->b_wptr = mp->b_rptr + size;

	ip6h = (ip6_t *)mp->b_rptr;
	mld = (icmp6_mld_t *)&ip6h[1];
	mld->mld_type = type;
	mld->mld_group_addr = ilm->ilm_v6addr;

	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6h->ip6_plen = htons(sizeof (icmp6_mld_t));
	ip6h->ip6_nxt = IPPROTO_ICMPV6;
	ip6h->ip6_hops = IPV6_MAX_HOPS;
	if (v6addr == NULL)
		ip6h->ip6_dst =  ilm->ilm_v6addr;
	else
		ip6h->ip6_dst = *v6addr;

	/* Ensure source is link-local */
	ip6h->ip6_src = ill->ill_ipif->ipif_v6src_addr;

	/*
	 * Prepare for checksum by putting icmp length in the icmp
	 * checksum field. The checksum is calculated in ip_wput_v6.
	 */
	mld->mld_cksum = ip6h->ip6_plen;

	/*
	 * ip_wput will automatically loopback the multicast packet to
	 * the ipc if multicast loopback is enabled.
	 * The MIB stats corresponding to this outgoing MLD packet
	 * will be accounted for in ip_wput->ip_wput_v6->ip_wput_ire_v6
	 * ->icmp_update_out_mib_v6 function call.
	 */
	ip_wput_v6(ill->ill_wq, mp);
}
