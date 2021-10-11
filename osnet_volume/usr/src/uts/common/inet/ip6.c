/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip6.c	1.26	99/11/17 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/sysmacros.h>
#include <sys/strlog.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/vtrace.h>
#include <sys/isa_defs.h>
#include <sys/atomic.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/snmpcom.h>
#include <sys/strick.h>

#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/tcp.h>

#include <inet/ip_multi.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>
#include <inet/optcom.h>
#include <inet/ip_ndp.h>

/*
 * Naming conventions:
 *      These rules should be judiciously applied
 *	if there is a need to identify something as IPv6 versus IPv4
 *	IPv6 funcions will end with _v6 in the ip module.
 *	IPv6 funcions will end with _ipv6 in the transport modules.
 *	IPv6 macros:
 *		Some macros end with _V6; e.g. ILL_FRAG_HASH_V6
 *		Some macros start with V6_; e.g. V6_OR_V4_INADDR_ANY
 *		And then there are ..V4_PART_OF_V6.
 *		The intent is that macros in the ip module end with _V6.
 *	IPv6 global variables will start with ipv6_
 *	IPv6 structures will start with ipv6
 *	IPv6 defined constants should start with IPV6_
 *		(but then there are NDP_DEFAULT_VERS_PRI_AND_FLOW, etc)
 */

/*
 * IPv6 mibs when the interface (ill) is not known.
 * When the ill is known the per-interface mib in the ill is used.
 */
mib2_ipv6IfStatsEntry_t	ip6_mib;
mib2_ipv6IfIcmpEntry_t	icmp6_mib;

uint_t ipv6_ire_default_count;	/* Number of IPv6 IRE_DEFAULT entries */
uint_t ipv6_ire_default_index;	/* Walking IPv6 index used to mod in */

const in6_addr_t ipv6_all_ones =
	{ 0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU };
const in6_addr_t ipv6_all_zeros = { 0, 0, 0, 0 };

#ifdef	_BIG_ENDIAN
const in6_addr_t ipv6_unspecified_group = { 0xff000000U, 0, 0, 0 };
#else	/* _BIG_ENDIAN */
const in6_addr_t ipv6_unspecified_group = { 0x000000ffU, 0, 0, 0 };
#endif	/* _BIG_ENDIAN */

#ifdef	_BIG_ENDIAN
const in6_addr_t ipv6_loopback = { 0, 0, 0, 0x00000001U };
#else  /* _BIG_ENDIAN */
const in6_addr_t ipv6_loopback = { 0, 0, 0, 0x01000000U };
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
const in6_addr_t ipv6_all_hosts_mcast = { 0xff020000U, 0, 0, 0x00000001U };
#else  /* _BIG_ENDIAN */
const in6_addr_t ipv6_all_hosts_mcast = { 0x000002ffU, 0, 0, 0x01000000U };
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
const in6_addr_t ipv6_all_rtrs_mcast = { 0xff020000U, 0, 0, 0x00000002U };
#else  /* _BIG_ENDIAN */
const in6_addr_t ipv6_all_rtrs_mcast = { 0x000002ffU, 0, 0, 0x02000000U };
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
const in6_addr_t ipv6_solicited_node_mcast =
			{ 0xff020000U, 0, 0x00000001U, 0xff000000U };
#else  /* _BIG_ENDIAN */
const in6_addr_t ipv6_solicited_node_mcast =
			{ 0x000002ffU, 0, 0x01000000U, 0x000000ffU };
#endif /* _BIG_ENDIAN */

icf_t	*ipc_tcp_conn_fanout_v6;	/* IPv6 tcp fanout hash list. */
icf_t	ipc_tcp_listen_fanout_v6[256];
icf_t	ipc_udp_fanout_v6[256];		/* IPv6 udp fanout hash list. */
icf_t	ipc_proto_fanout_v6[IPPROTO_MAX + 1];	/* misc. fanout hash list. */

/* Leave room for ip_newroute to tack on the src and target addresses */
#define	OK_RESOLVER_MP_V6(mp)						\
		((mp) && ((mp)->b_wptr - (mp)->b_rptr) >= (2 * IPV6_ADDR_LEN))

/* Flags for ip_xmit_v6() */
#define	IPV6_REACHABILITY_CONFIRMATION	0x01


static void	icmp_inbound_error_fanout_v6(queue_t *, mblk_t *, ip6_t *,
		    icmp6_t *, ill_t *);
static void	icmp_inbound_too_big_v6(queue_t *, mblk_t *, ill_t *ill);
static void	icmp_pkt_v6(queue_t *, mblk_t *, void *, size_t,
		    const in6_addr_t *);
static void	icmp_redirect_v6(queue_t *, mblk_t *, ill_t *ill);
static boolean_t	icmp_redirect_ok_v6(ill_t *ill, mblk_t *mp);
static int	ip_bind_connected_v6(ipc_t *, mblk_t *, const in6_addr_t *,
		    uint16_t, const in6_addr_t *, uint16_t,
		    boolean_t, boolean_t);
static int	ip_bind_fanout_insert_v6(ipc_t *, int, int);
static boolean_t ip_bind_insert_ire_v6(mblk_t *, ire_t *, const in6_addr_t *,
		    iulp_t *);
static int	ip_bind_laddr_v6(ipc_t *, mblk_t *, const in6_addr_t *,
		    uint16_t, boolean_t, boolean_t);
static mblk_t	*ip_add_info_v6(mblk_t *, ill_t *, const in6_addr_t *);
static void	ip_fanout_proto_v6(queue_t *, mblk_t *, ip6_t *, ill_t *,
		    uint8_t, uint_t, uint_t);
static void	ip_fanout_tcp_listen_v6(queue_t *, mblk_t *, ip6_t *,
		    uint32_t, ill_t *, uint_t, uint_t);
static void	ip_fanout_tcp_v6(queue_t *, mblk_t *, ip6_t *,
		    uint32_t, ill_t *, uint_t, uint_t);
static void	ip_fanout_udp_v6(queue_t *, mblk_t *, ip6_t *,
		    uint32_t, ill_t *, uint_t);
static void	ip_fanout_send_icmp_v6(queue_t *, mblk_t *, uint_t,
		    Counter *, uint_t, uint8_t, uint_t);
static int	ip_process_options_v6(queue_t *, mblk_t *, ip6_t *,
		    uint8_t *, uint_t, uint8_t);
static void	ip_rput_data_v6(queue_t *, ill_t *, mblk_t *, ip6_t *,
		    boolean_t, boolean_t);
static mblk_t	*ip_rput_frag_v6(queue_t *, mblk_t *, ip6_t *,
		    ip6_frag_t *, uint_t, uint_t *);
static boolean_t	ip_source_routed_v6(ip6_t *, mblk_t *);
static void	ip_wput_frag_v6(mblk_t *, ire_t *, int, ipc_t *);
static void	ip_wput_ire_v6(queue_t *, mblk_t *, ire_t *, int, int,
		    ipc_t *);
static void	ip_xmit_v6(mblk_t *, ire_t *, uint_t, ipc_t *);
static boolean_t ipc_wantpacket_v6(ipc_t *, ill_t *, const in6_addr_t *);

struct qinit rinit_ipv6 = {
	(pfi_t)ip_rput_v6,
	(pfi_t)ip_rsrv,
	ip_open,
	ip_close,
	NULL,
	&ip_mod_info
};


struct qinit winit_ipv6 = {
	(pfi_t)ip_wput_v6,
	(pfi_t)ip_wsrv,
	ip_open,
	ip_close,
	NULL,
	&ip_mod_info
};

/*
 * Handle IPv6 ICMP packets sent to us.  Consume the mblk passed in.
 * The message has already been checksummed and if needed,
 * a copy has been made to be sent any interested ICMP client (ipc)
 * Note that this is different than icmp_inbound() which does the fanout
 * to ipc's as well as local processing of the ICMP packets.
 *
 * All error messages are passed to the matching transport stream.
 */
static void
icmp_inbound_v6(queue_t *q, mblk_t *mp, ill_t *ill, uint_t hdr_length)
{
	icmp6_t		*icmp6;
	ip6_t		*ip6h = (ip6_t *)mp->b_rptr;
	boolean_t	interested;

	ASSERT(ill != NULL);

	BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInMsgs);

	if ((mp->b_wptr - mp->b_rptr) < (hdr_length + ICMP6_MINLEN)) {
		if (!pullupmsg(mp, hdr_length + ICMP6_MINLEN)) {
			ip1dbg(("icmp_inbound_v6: pullupmsg failed\n"));
			BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInErrors);
			freemsg(mp);
			return;
		}
		ip6h = (ip6_t *)mp->b_rptr;
	}

	icmp6 = (icmp6_t *)(&mp->b_rptr[hdr_length]);
	interested = !(icmp6->icmp6_type & ICMP6_INFOMSG_MASK);

	switch (icmp6->icmp6_type) {
	case ICMP6_DST_UNREACH:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInDestUnreachs);
		if (icmp6->icmp6_code == ICMP6_DST_UNREACH_ADMIN)
			BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInAdminProhibs);
		break;

	case ICMP6_TIME_EXCEEDED:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInTimeExcds);
		break;

	case ICMP6_PARAM_PROB:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInParmProblems);
		break;

	case ICMP6_PACKET_TOO_BIG:
		icmp_inbound_too_big_v6(q, mp, ill);
		return;
	case ICMP6_ECHO_REQUEST:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInEchos);
		if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst) &&
		    !ipv6_resp_echo_mcast)
			break;

		/*
		 * We must have exclusive use of the mblk to convert it to
		 * a response.
		 * If not, we copy it.
		 */
		if (mp->b_datap->db_ref > 1) {
			mblk_t	*mp1;

			mp1 = copymsg(mp);
			freemsg(mp);
			if (mp1 == NULL) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
				return;
			}
			mp = mp1;
			ip6h = (ip6_t *)mp->b_rptr;
			icmp6 = (icmp6_t *)(&mp->b_rptr[hdr_length]);
		}
		/*
		 * Turn the echo into an echo reply.
		 * Remove any extension headers (do not reverse a source route)
		 * and clear the flow id (keep traffic class for now).
		 */
		if (hdr_length != IPV6_HDR_LEN) {
			int	i;

			for (i = 0; i < IPV6_HDR_LEN; i++)
				mp->b_rptr[hdr_length - i - 1] =
				    mp->b_rptr[IPV6_HDR_LEN - i - 1];
			mp->b_rptr += (hdr_length - IPV6_HDR_LEN);
			ip6h = (ip6_t *)mp->b_rptr;
			ip6h->ip6_nxt = IPPROTO_ICMPV6;
			hdr_length = IPV6_HDR_LEN;
		}
		ip6h->ip6_vcf &= ~IPV6_FLOWINFO_FLOWLABEL;
		icmp6->icmp6_type = ICMP6_ECHO_REPLY;

		ip6h->ip6_plen =
		    htons((uint16_t)(msgdsize(mp) - IPV6_HDR_LEN));

		/*
		 * Reverse the source and destination addresses.
		 * If the return address is a multicast, zero out the source
		 * (ip_wput_v6 will set an address).
		 */
		if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst)) {
			ip6h->ip6_dst = ip6h->ip6_src;
			ip6h->ip6_src = ipv6_all_zeros;
		} else {
			in6_addr_t	ipv6addr;

			ipv6addr = ip6h->ip6_src;
			ip6h->ip6_src = ip6h->ip6_dst;
			ip6h->ip6_dst = ipv6addr;
		}

		/* set the hop limit */
		ip6h->ip6_hops = ipv6_def_hops;

		/*
		 * Prepare for checksum by putting icmp length in the icmp
		 * checksum field. The checksum is calculated in ip_wput_v6.
		 */
		icmp6->icmp6_cksum = ip6h->ip6_plen;
		put(WR(q), mp);
		return;

	case ICMP6_ECHO_REPLY:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInEchoReplies);
		break;

	case ND_ROUTER_SOLICIT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInRouterSolicits);
		break;

	case ND_ROUTER_ADVERT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInRouterAdvertisements);
		break;

	case ND_NEIGHBOR_SOLICIT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInNeighborSolicits);
		ndp_input(ill, mp);
		return;

	case ND_NEIGHBOR_ADVERT:
		BUMP_MIB(ill->ill_icmp6_mib->
		    ipv6IfIcmpInNeighborAdvertisements);
		ndp_input(ill, mp);
		return;

	case ND_REDIRECT: {
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInRedirects);

		if (ipv6_ignore_redirect)
			break;

		if (!pullupmsg(mp, -1) ||
		    !icmp_redirect_ok_v6(ill, mp)) {
			BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInBadRedirects);
			break;
		}
		icmp_redirect_v6(q, mp, ill);
		return;
	}

	/*
	* The next three icmp messages will be handled by MLD.
	* Pass all valid MLD packets up to any process(es)
	* listening on a raw ICMP socket. MLD messages are
	* freed by mld_input function.
	*/
	case ICMP6_MEMBERSHIP_QUERY:
	case ICMP6_MEMBERSHIP_REPORT:
	case ICMP6_MEMBERSHIP_REDUCTION:
		mld_input(q, mp, ill);
		return;
	default:
		break;
	}
	if (interested)
		icmp_inbound_error_fanout_v6(q, mp, ip6h, icmp6, ill);
	else
		freemsg(mp);
}

/*
 * Process received IPv6 ICMP Packet too big.
 * After updating any IRE it does the fanout to any matching transport streams.
 * Assumes the IPv6 plus ICMPv6 headers have been pulled up but nothing else.
 */
/* ARGSUSED */
static void
icmp_inbound_too_big_v6(queue_t *q, mblk_t *mp, ill_t *ill)
{
	ip6_t		*ip6h;
	ip6_t		*inner_ip6h;
	icmp6_t		*icmp6;
	uint16_t	hdr_length;
	uint32_t	mtu;
	ire_t		*ire;

	/*
	 * We must have exclusive use of the mblk to update the MTU
	 * in the packet.
	 * If not, we copy it.
	 */
	if (mp->b_datap->db_ref > 1) {
		mblk_t	*mp1;

		mp1 = copymsg(mp);
		freemsg(mp);
		if (mp1 == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			return;
		}
		mp = mp1;
	}
	ip6h = (ip6_t *)mp->b_rptr;
	if (ip6h->ip6_nxt != IPPROTO_ICMPV6)
		hdr_length = ip_hdr_length_v6(mp, ip6h);
	else
		hdr_length = IPV6_HDR_LEN;

	icmp6 = (icmp6_t *)(&mp->b_rptr[hdr_length]);
	ASSERT((size_t)(mp->b_wptr - mp->b_rptr) >= hdr_length + ICMP6_MINLEN);
	inner_ip6h = (ip6_t *)&icmp6[1];	/* Packet in error */
	if ((uchar_t *)&inner_ip6h[1] > mp->b_wptr) {
		if (!pullupmsg(mp, (uchar_t *)&inner_ip6h[1] - mp->b_rptr)) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			freemsg(mp);
			return;
		}
		ip6h = (ip6_t *)mp->b_rptr;
		icmp6 = (icmp6_t *)&mp->b_rptr[hdr_length];
		inner_ip6h = (ip6_t *)&icmp6[1];
	}

	/*
	 * For link local destinations matching simply on IRE type is not
	 * sufficient. Same link local addresses for different ILL's is
	 * possible.
	 */

	if (IN6_IS_ADDR_LINKLOCAL(&inner_ip6h->ip6_dst)) {
		ire = ire_ctable_lookup_v6(&inner_ip6h->ip6_dst, NULL,
			IRE_CACHE, ill->ill_ipif, NULL,
			MATCH_IRE_TYPE | MATCH_IRE_ILL);

		if (ire == NULL) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("icmp_inbound_too_big_v6:"
					"no ire for dst %s\n", AF_INET6,
					&inner_ip6h->ip6_dst);
			}
			freemsg(mp);
			return;
		}

		mtu = ntohl(icmp6->icmp6_mtu);
		mutex_enter(&ire->ire_lock);
		if (mtu < IPV6_MIN_MTU) {
			ip1dbg(("Received mtu less than IPv6 min mtu %d: %d\n",
				IPV6_MIN_MTU, mtu));
			mtu = IPV6_MIN_MTU;
			/*
			 * If an mtu less than IPv6 min mtu is received,
			 * we must include a fragment header in subsequent
			 * packets.
			 */
			ire->ire_frag_flag |= IPH_FRAG_HDR;
		}
		ip1dbg(("Received mtu from router: %d\n", mtu));
		ire->ire_max_frag = MIN(ire->ire_max_frag, mtu);
		/* Record the new max frag size for the ULP. */
		icmp6->icmp6_mtu = htonl(ire->ire_max_frag);
		mutex_exit(&ire->ire_lock);
		ire_refrele(ire);
	} else {
		irb_t	*irb = NULL;
		/*
		 * for non-link local destinations we match only on the IRE type
		 */
		ire = ire_ctable_lookup_v6(&inner_ip6h->ip6_dst, NULL,
			IRE_CACHE, ill->ill_ipif, NULL, MATCH_IRE_TYPE);
		if (ire == NULL) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("icmp_inbound_too_big_v6:"
					"no ire for dst %s\n",
					AF_INET6, &inner_ip6h->ip6_dst);
			}
			freemsg(mp);
			return;
		}
		irb = ire->ire_bucket;
		ire_refrele(ire);
		rw_enter(&irb->irb_lock, RW_READER);
		for (ire = irb->irb_ire; ire != NULL; ire = ire->ire_next) {
			if (IN6_ARE_ADDR_EQUAL(&ire->ire_addr_v6,
				&inner_ip6h->ip6_dst)) {
				mtu = ntohl(icmp6->icmp6_mtu);
				mutex_enter(&ire->ire_lock);
				if (mtu < IPV6_MIN_MTU) {
					ip1dbg(("Received mtu less than IPv6"
						"min mtu %d: %d\n",
						IPV6_MIN_MTU, mtu));
					mtu = IPV6_MIN_MTU;
					/*
					 * If an mtu less than IPv6 min mtu is
					 * received, we must include a fragment
					 * header in subsequent packets.
					 */
					ire->ire_frag_flag |= IPH_FRAG_HDR;
				}

				ip1dbg(("Received mtu from router: %d\n", mtu));
				ire->ire_max_frag = MIN(ire->ire_max_frag, mtu);
				/* Record the new max frag size for the ULP. */
				icmp6->icmp6_mtu = htonl(ire->ire_max_frag);
				mutex_exit(&ire->ire_lock);
			}
		}
		rw_exit(&irb->irb_lock);
	}
	icmp_inbound_error_fanout_v6(q, mp, ip6h, icmp6, ill);
}


/*
 * Fanout received ICMPv6 error packets to the transports.
 * Assumes the IPv6 plus ICMPv6 headers have been pulled up but nothing else.
 */
static void
icmp_inbound_error_fanout_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h,
    icmp6_t *icmp6, ill_t *ill)
{
	uint16_t *up;	/* Pointer to ports in ULP header */
	uint32_t ports;	/* reversed ports for fanout */
	ip6_t rip6h;	/* With reversed addresses */
	uint16_t	hdr_length;
	uint8_t		*nexthdrp;
	uint8_t		nexthdr;

	hdr_length = (uint16_t)((uchar_t *)icmp6 - (uchar_t *)ip6h);
	ASSERT((size_t)(mp->b_wptr - (uchar_t *)icmp6) >= ICMP6_MINLEN);

	/*
	 * Need to pullup everything in order to use
	 * ip_hdr_length_nexthdr_v6()
	 */
	if (mp->b_cont != NULL) {
		if (!pullupmsg(mp, -1)) {
			ip1dbg(("icmp_inbound_error_fanout_v6: "
			    "pullupmsg failed\n"));
			goto drop_pkt;
		}
		ip6h = (ip6_t *)mp->b_rptr;
		icmp6 = (icmp6_t *)(&mp->b_rptr[hdr_length]);
	}

	ip6h = (ip6_t *)&icmp6[1];	/* Packet in error */
	if ((uchar_t *)&ip6h[1] > mp->b_wptr)
		goto drop_pkt;

	if (!ip_hdr_length_nexthdr_v6(mp, ip6h, &hdr_length, &nexthdrp))
		goto drop_pkt;
	nexthdr = *nexthdrp;

	/* Set message type, must be done after pullups */
	mp->b_datap->db_type = M_CTL;

	/* Try to pass the ICMP message to clients who need it */
	switch (nexthdr) {
	case IPPROTO_UDP: {
		/* Verify we have at least eight bytes of header */
		if ((uchar_t *)ip6h + hdr_length + 8 > mp->b_wptr) {
			goto drop_pkt;
		}
		/*
		 * Attempt to find a client stream based on port.
		 * Note that we do a reverse lookup since the header is
		 * in the form we sent it out.
		 * The rip6h header is only used for the IP_UDP_MATCH_V6 and we
		 * only set the src and dst addresses and nexthdr.
		 */
		up = (uint16_t *)((uchar_t *)ip6h + hdr_length);
		rip6h.ip6_src = ip6h->ip6_dst;
		rip6h.ip6_dst = ip6h->ip6_src;
		rip6h.ip6_nxt = nexthdr;
		((uint16_t *)&ports)[0] = up[1];
		((uint16_t *)&ports)[1] = up[0];

		ip_fanout_udp_v6(q, mp, &rip6h, ports, ill, 0);
		return;
	}
	case IPPROTO_TCP: {
		/* Verify we have at least eight bytes of header */
		if ((uchar_t *)ip6h + hdr_length + 8 > mp->b_wptr) {
			goto drop_pkt;
		}
		/*
		 * Attempt to find a client stream based on port.
		 * Note that we do a reverse lookup since the header is
		 * in the form we sent it out.
		 * The rip6h header is only used for the IP_TCP_*MATCH_V6 and
		 * we only set the src and dst addresses and nexthdr.
		 */
		up = (uint16_t *)((uchar_t *)ip6h + hdr_length);
		rip6h.ip6_src = ip6h->ip6_dst;
		rip6h.ip6_dst = ip6h->ip6_src;
		rip6h.ip6_nxt = nexthdr;
		((uint16_t *)&ports)[0] = up[1];
		((uint16_t *)&ports)[1] = up[0];

		ip_fanout_tcp_v6(q, mp, &rip6h, ports, ill, 0, 0);
		return;

	}
	default:
		/*
		 * The rip6h header is only used for the lookup and we
		 * only set the src and dst addresses and nexthdr.
		 */
		rip6h.ip6_src = ip6h->ip6_dst;
		rip6h.ip6_dst = ip6h->ip6_src;
		rip6h.ip6_nxt = nexthdr;

		ip_fanout_proto_v6(q, mp, &rip6h, ill, nexthdr, 0, 0);
		return;
	}
	/* NOTREACHED */
drop_pkt:
	BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInErrors);
	ip1dbg(("icmp_inbound_error_fanout_v6: drop pkt\n"));
	freemsg(mp);
}

/*
 * Validate the incoming redirect message,  if valid redirect
 * processing is done later.  This is separated from the actual
 * redirect processing to avoid becoming single threaded when not
 * necessary. (i.e invalid packet)
 * Assumes that any AH or ESP headers have already been removed.
 * The mp has already been pulled up.
 */
boolean_t
icmp_redirect_ok_v6(ill_t *ill, mblk_t *mp)
{
	ip6_t		*ip6h = (ip6_t *)mp->b_rptr;
	nd_redirect_t	*rd;
	ire_t		*ire;
	uint16_t	len;
	uint16_t	hdr_length;

	ASSERT(mp->b_cont == NULL);
	if (ip6h->ip6_nxt != IPPROTO_ICMPV6)
		hdr_length = ip_hdr_length_v6(mp, ip6h);
	else
		hdr_length = IPV6_HDR_LEN;
	rd = (nd_redirect_t *)&mp->b_rptr[hdr_length];
	len = mp->b_wptr - mp->b_rptr -  hdr_length;
	if (!IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src) ||
	    (ip6h->ip6_hops != IPV6_MAX_HOPS) ||
	    (rd->nd_rd_code != 0) ||
	    (len < sizeof (nd_redirect_t)) ||
	    (IN6_IS_ADDR_MULTICAST(&rd->nd_rd_dst))) {
		return (B_FALSE);
	}
	if (!(IN6_IS_ADDR_LINKLOCAL(&rd->nd_rd_target) ||
	    IN6_ARE_ADDR_EQUAL(&rd->nd_rd_target, &rd->nd_rd_dst))) {
		return (B_FALSE);
	}

	/*
	 * Verify that the IP source address of the redirect is
	 * the same as the current first-hop router for the specified
	 * ICMP destination address.  Just to be cautious, this test
	 * will be done again after we are exclusive, in case the
	 * router goes away between now and then.
	 */
	ire = ire_route_lookup_v6(&rd->nd_rd_dst, 0,
	    &ip6h->ip6_src, 0, ill->ill_ipif, NULL, NULL,
	    MATCH_IRE_GW | MATCH_IRE_ILL);
	if (ire == NULL)
		return (B_FALSE);
	ire_refrele(ire);
	if (len > sizeof (nd_redirect_t)) {
		if (!ndp_verify_optlen((nd_opt_hdr_t *)&rd[1],
		    len - sizeof (nd_redirect_t)))
			return (B_FALSE);
	}
	return (B_TRUE);
}

/*
 * Process received IPv6 ICMP Redirect messages.
 * Assumes that the icmp packet has already been verfied to be
 * valid, aligned and in a single mblk all done in icmp_redirect_ok_v6().
 */
/* ARGSUSED */
static void
icmp_redirect_v6(queue_t *q, mblk_t *mp, ill_t *ill)
{
	ip6_t		*ip6h;
	uint16_t	hdr_length;
	nd_redirect_t	*rd;
	ire_t		*ire;
	ire_t		*prev_ire;
	ire_t		*redir_ire;
	in6_addr_t	*src, *dst, *gateway;
	nd_opt_hdr_t	*opt;
	nce_t		*nce;
	int		nce_flags = 0;
	int		err = 0;
	boolean_t	redirect_to_router = B_FALSE;
	int		len;
	iulp_t		ulp_info = { 0 };

	ip6h = (ip6_t *)mp->b_rptr;
	if (ip6h->ip6_nxt != IPPROTO_ICMPV6)
		hdr_length = ip_hdr_length_v6(mp, ip6h);
	else
		hdr_length = IPV6_HDR_LEN;

	rd = (nd_redirect_t *)&mp->b_rptr[hdr_length];
	src = &ip6h->ip6_src;
	dst = &rd->nd_rd_dst;
	gateway = &rd->nd_rd_target;
	if (!IN6_ARE_ADDR_EQUAL(gateway, dst)) {
		redirect_to_router = B_TRUE;
		nce_flags |= NCE_F_ISROUTER;
	}
	/*
	 * Make sure we had a route for the dest in question and that
	 * route was pointing to the old gateway (the source of the
	 * redirect packet.)
	 */
	prev_ire = ire_route_lookup_v6(dst, 0, src, 0, ill->ill_ipif, NULL,
	    NULL, MATCH_IRE_GW | MATCH_IRE_ILL);
	/*
	 * Check that
	 *	the redirect was not from ourselves
	 *	old gateway is still directly reachable
	 */
	if (prev_ire == NULL ||
	    prev_ire->ire_type == IRE_LOCAL) {
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInBadRedirects);
		goto fail_redirect;
	}
	if (prev_ire->ire_ipif->ipif_flags & IFF_NONUD)
		nce_flags |= NCE_F_NONUD;

	/*
	 * Should we use the old ULP info to create the new gateway?  From
	 * a user's perspective, we should inherit the info so that it
	 * is a "smooth" transition.  If we do not do that, then new
	 * connections going thru the new gateway will have no route metrics,
	 * which is counter-intuitive to user.  From a network point of
	 * view, this may or may not make sense even though the new gateway
	 * is still directly connected to us so the route metrics should not
	 * change much.
	 *
	 * But if the old ire_uinfo is not initialized, we do another
	 * recursive lookup on the dest using the new gateway.  There may
	 * be a route to that.  If so, use it to initialize the redirect
	 * route.
	 */
	if (prev_ire->ire_uinfo.iulp_set) {
		bcopy(&prev_ire->ire_uinfo, &ulp_info, sizeof (iulp_t));
	} else if (redirect_to_router) {
		/*
		 * Only do the following if the redirection is really to
		 * a router.
		 */
		ire_t *tmp_ire;
		ire_t *sire;

		tmp_ire = ire_ftable_lookup_v6(dst, 0, gateway, 0, NULL, &sire,
		    NULL, 0,
		    (MATCH_IRE_RECURSIVE | MATCH_IRE_GW | MATCH_IRE_DEFAULT));
		if (sire != NULL) {
			bcopy(&sire->ire_uinfo, &ulp_info, sizeof (iulp_t));
			ASSERT(tmp_ire != NULL);
			ire_refrele(tmp_ire);
			ire_refrele(sire);
		} else if (tmp_ire != NULL) {
			bcopy(&tmp_ire->ire_uinfo, &ulp_info,
			    sizeof (iulp_t));
			ire_refrele(tmp_ire);
		}
	}

	len = mp->b_wptr - mp->b_rptr -  hdr_length - sizeof (nd_redirect_t);
	opt = (nd_opt_hdr_t *)&rd[1];
	opt = ndp_get_option(opt, len, ND_OPT_TARGET_LINKADDR);
	if (opt != NULL) {
		err = ndp_lookup_then_add(ill,
		    (uchar_t *)&opt[1],		/* Link layer address */
		    gateway,
		    &ipv6_all_ones,		/* prefix mask */
		    &ipv6_all_zeros,		/* Mapping mask */
		    0,
		    nce_flags,
		    ND_STALE,
		    &nce);
		switch (err) {
		case 0:
			NCE_REFRELE(nce);
			break;
		case EEXIST:
			/*
			 * Check to see if link layer address has changed and
			 * process the nce_state accordingly.
			 */
			ndp_process(nce, (uchar_t *)&opt[1], 0, B_FALSE);
			NCE_REFRELE(nce);
			break;
		default:
			ip1dbg(("icmp_redirect_v6: NCE create failed %d\n",
			    err));
			goto fail_redirect;
		}
	}
	if (redirect_to_router) {
		/* icmp_redirect_ok_v6() must  have already verified this  */
		ASSERT(IN6_IS_ADDR_LINKLOCAL(gateway));

		/*
		 * Create a Route Association.  This will allow us to remember
		 * a router told us to use the particular gateway.
		 */
		ire = ire_create_v6(
		    dst,
		    &ipv6_all_ones,		/* mask */
		    &prev_ire->ire_src_addr_v6,	/* source addr */
		    gateway,			/* gateway addr */
		    prev_ire->ire_max_frag,	/* max frag */
		    NULL,			/* Fast Path header */
		    NULL, 			/* no rfq */
		    NULL,			/* no stq */
		    IRE_HOST_REDIRECT,
		    NULL,
		    prev_ire->ire_ipif,
		    NULL,
		    0,
		    0,
		    (RTF_DYNAMIC | RTF_GATEWAY | RTF_HOST),
		    &ulp_info);
	} else {
		/*
		 * Just create an on link entry, may or may not be a router
		 * If there is no link layer address option ire_add() won't
		 * add this.
		 */
		ire = ire_create_v6(
		    dst,				/* gateway == dst */
		    &ipv6_all_ones,			/* mask */
		    &prev_ire->ire_src_addr_v6,		/* source addr */
		    &ipv6_all_zeros,			/* gateway addr */
		    prev_ire->ire_max_frag,		/* max frag */
		    NULL,				/* Fast Path header */
		    prev_ire->ire_rfq,			/* ire rfq */
		    prev_ire->ire_stq,			/* ire stq */
		    IRE_CACHE,
		    NULL,
		    prev_ire->ire_ipif,
		    &ipv6_all_ones,
		    0,
		    0,
		    0,
		    &ulp_info);
	}
	if (ire == NULL)
		goto fail_redirect;
	if (prev_ire->ire_type == IRE_CACHE)
		ire_delete(prev_ire);
	ire_refrele(prev_ire);
	prev_ire = NULL;

	/*
	 * XXX If there is no nce i.e there is no target link layer address
	 * option with the redirect message, ire_add will fail. In that
	 * case we never add the IRE_CACHE/IRE_HOST_REDIRECT. We need
	 * to fix this.
	 */
	if ((ire = ire_add(ire)) != NULL) {

		/* tell routing sockets that we received a redirect */
		ip_rts_change_v6(RTM_REDIRECT,
		    &rd->nd_rd_dst,
		    &rd->nd_rd_target,
		    &ipv6_all_ones, 0, &ire->ire_src_addr_v6,
		    (RTF_DYNAMIC | RTF_GATEWAY | RTF_HOST), 0,
		    (RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_AUTHOR));

		/*
		 * Delete any existing IRE_HOST_REDIRECT for this destination.
		 * This together with the added IRE has the effect of
		 * modifying an existing redirect.
		 */
		redir_ire = ire_ftable_lookup_v6(dst, 0, src, IRE_HOST_REDIRECT,
		    ire->ire_ipif, NULL, NULL, 0,
		    (MATCH_IRE_GW | MATCH_IRE_TYPE | MATCH_IRE_ILL));

		ire_refrele(ire);		/* Held in ire_add_v6 */

		if (redir_ire != NULL) {
			ire_delete(redir_ire);
			ire_refrele(redir_ire);
		}
	}
fail_redirect:
	if (prev_ire != NULL)
		ire_refrele(prev_ire);
	freemsg(mp);
}

static ill_t *
ip_queue_to_ill_v6(queue_t *q)
{
	ill_t *ill;

	ASSERT(WR(q) == q);

	if (q->q_next != NULL)
		ill = (ill_t *)q->q_ptr;
	else
		ill = ill_lookup_on_name(ipif_loopback_name,
		    mi_strlen(ipif_loopback_name) + 1, B_FALSE, B_TRUE);
	if (ill == NULL) {
		ip0dbg(("ip_queue_to_ill_v6: no ill\n"));
	}
	return (ill);
}

/*
 * Assigns an appropriate source address to the packet.
 * If origdst is one of our IP addresses that use it as the source.
 * If the queue is an ill queue then select a source from that ill.
 * Otherwise pick a source based on a route lookup back to the origsrc.
 *
 * src is the return parameter. Returns a pointer to src or NULL if failure.
 */
static in6_addr_t *
icmp_pick_source_v6(queue_t *wq, in6_addr_t *origsrc, in6_addr_t *origdst,
    in6_addr_t *src)
{
	ill_t	*ill;
	ire_t	*ire;
	ipif_t	*ipif;

	ASSERT(!(wq->q_flag & QREADR));
	if (wq->q_next != NULL)
		ill = (ill_t *)wq->q_ptr;
	else
		ill = NULL;

	ire = ire_route_lookup_v6(origdst, 0, 0, (IRE_LOCAL|IRE_LOOPBACK),
	    NULL, NULL, NULL, MATCH_IRE_TYPE);
	if (ire != NULL) {
		/* Destined to one of our addresses */
		*src = *origdst;
		ire_refrele(ire);
		return (src);

	}
	if (ill == NULL) {
		/* What is the route back to the original source? */
		ire = ire_route_lookup_v6(origsrc, 0, 0, 0,
		    NULL, NULL, NULL, MATCH_IRE_DEFAULT|MATCH_IRE_RECURSIVE);
		if (ire == NULL) {
			BUMP_MIB(ip6_mib.ipv6OutNoRoutes);
			return (NULL);
		}
		ASSERT(ire->ire_ipif != NULL);
		ill = ire->ire_ipif->ipif_ill;
		ire_refrele(ire);
	}
	ipif = ipif_select_source_v6(ill, origsrc);
	if (ipif != NULL) {
		*src = ipif->ipif_v6src_addr;
		return (src);
	}
	/*
	 * Unusual case - can't find a usable source address to reach the
	 * original source. Use what in the route to the source.
	 */
	if (ill != NULL) {
		ire = ire_route_lookup_v6(origsrc, 0, 0, 0,
		    NULL, NULL, NULL, MATCH_IRE_DEFAULT|MATCH_IRE_RECURSIVE);
		if (ire == NULL) {
			BUMP_MIB(ip6_mib.ipv6OutNoRoutes);
			return (NULL);
		}
	}
	ASSERT(ire != NULL);
	*src = ire->ire_src_addr_v6;
	ire_refrele(ire);
	return (src);
}

/*
 * Build and ship an IPv6 ICMP message using the packet data in mp,
 * and the ICMP header pointed to by "stuff".  (May be called as
 * writer.)
 * Note: assumes that icmp_pkt_err_ok_v6 has been called to
 * verify that an icmp error packet can be sent.
 *
 * If q is an ill write side queue (which is the case when packets
 * arrive from ip_rput) then ip_wput code will ensure that packets to
 * link-local destinations are sent out that ill.
 *
 * If v6src_ptr is set use it as a source. Otherwise select a reasonable
 * source address (see above function).
 */
static void
icmp_pkt_v6(queue_t *q, mblk_t *mp, void *stuff, size_t len,
    const in6_addr_t *v6src_ptr)
{
	ip6_t		*ip6h;
	in6_addr_t	v6dst;
	size_t		len_needed;
	size_t		msg_len;
	mblk_t		*mp1;
	icmp6_t		*icmp6;
	ill_t		*ill;
	in6_addr_t	v6src;

	ill = ip_queue_to_ill_v6(q);
	if (ill == NULL) {
		freemsg(mp);
		return;
	}

	ip6h = (ip6_t *)mp->b_rptr;
	if (v6src_ptr != NULL) {
		v6src = *v6src_ptr;
	} else {
		if (icmp_pick_source_v6(q, &ip6h->ip6_src, &ip6h->ip6_dst,
		    &v6src) == NULL) {
			freemsg(mp);
			return;
		}
	}
	v6dst = ip6h->ip6_src;
	len_needed = ipv6_icmp_return - IPV6_HDR_LEN - len;
	msg_len = msgdsize(mp);
	if (msg_len > len_needed) {
		if (!adjmsg(mp, len_needed - msg_len)) {
			BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutErrors);
			freemsg(mp);
			return;
		}
		msg_len = len_needed;
	}
	mp1 = allocb(IPV6_HDR_LEN + len, BPRI_HI);
	if (mp1 == NULL) {
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutErrors);
		freemsg(mp);
		return;
	}
	mp1->b_cont = mp;
	mp = mp1;
	ip6h = (ip6_t *)mp->b_rptr;
	mp1->b_wptr = (uchar_t *)ip6h + (IPV6_HDR_LEN + len);

	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6h->ip6_nxt = IPPROTO_ICMPV6;
	ip6h->ip6_hops = ipv6_def_hops;
	ip6h->ip6_dst = v6dst;
	ip6h->ip6_src = v6src;
	msg_len += IPV6_HDR_LEN + len;
	if (msg_len > IP_MAXPACKET + IPV6_HDR_LEN) {
		(void) adjmsg(mp, IP_MAXPACKET + IPV6_HDR_LEN - msg_len);
		msg_len = IP_MAXPACKET + IPV6_HDR_LEN;
	}
	ip6h->ip6_plen = htons((uint16_t)(msgdsize(mp) - IPV6_HDR_LEN));
	icmp6 = (icmp6_t *)&ip6h[1];
	bcopy(stuff, (char *)icmp6, len);
	/*
	 * Prepare for checksum by putting icmp length in the icmp
	 * checksum field. The checksum is calculated in ip_wput_v6.
	 */
	icmp6->icmp6_cksum = ip6h->ip6_plen;
	if (icmp6->icmp6_type == ND_REDIRECT) {
		ip6h->ip6_hops = IPV6_MAX_HOPS;
	}
	/* Send to V6 writeside put routine */
	put(q, mp);
}

/*
 * Update the output mib when ICMPv6 packets are sent.
 */
static void
icmp_update_out_mib_v6(ill_t *ill, icmp6_t *icmp6)
{
	BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutMsgs);

	switch (icmp6->icmp6_type) {
	case ICMP6_DST_UNREACH:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutDestUnreachs);
		if (icmp6->icmp6_code == ICMP6_DST_UNREACH_ADMIN)
			BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutAdminProhibs);
		break;

	case ICMP6_TIME_EXCEEDED:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutTimeExcds);
		break;

	case ICMP6_PARAM_PROB:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutParmProblems);
		break;

	case ICMP6_PACKET_TOO_BIG:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutPktTooBigs);
		break;

	case ICMP6_ECHO_REQUEST:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutEchos);
		break;

	case ICMP6_ECHO_REPLY:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutEchoReplies);
		break;

	case ND_ROUTER_SOLICIT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutRouterSolicits);
		break;

	case ND_ROUTER_ADVERT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutRouterAdvertisements);
		break;

	case ND_NEIGHBOR_SOLICIT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutNeighborSolicits);
		break;

	case ND_NEIGHBOR_ADVERT:
		BUMP_MIB(ill->ill_icmp6_mib->
		    ipv6IfIcmpOutNeighborAdvertisements);
		break;

	case ND_REDIRECT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutRedirects);
		break;

	case ICMP6_MEMBERSHIP_QUERY:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutGroupMembQueries);
		break;

	case ICMP6_MEMBERSHIP_REPORT:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutGroupMembResponses);
		break;

	case ICMP6_MEMBERSHIP_REDUCTION:
		BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpOutGroupMembReductions);
		break;
	}
}

/*
 * Check if it is ok to send an ICMPv6 error packet in
 * response to the IP packet in mp.
 * Free the message and return null if no
 * ICMP error packet should be sent.
 */
mblk_t *
icmp_pkt_err_ok_v6(queue_t *q, mblk_t *mp,
    boolean_t llbcast, boolean_t mcast_ok)
{
	ip6_t	*ip6h;

	if (!mp)
		return (NULL);

	ip6h = (ip6_t *)mp->b_rptr;

	/* Check if source address uniquely identifies the host */

	if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_src)||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6h->ip6_src)) {
		freemsg(mp);
		return (NULL);
	}

	if (ip6h->ip6_nxt == IPPROTO_ICMPV6) {
		size_t	len_needed = IPV6_HDR_LEN + ICMP6_MINLEN;
		icmp6_t		*icmp6;

		if (mp->b_wptr - mp->b_rptr < len_needed) {
			if (!pullupmsg(mp, len_needed)) {
				ill_t	*ill;

				ill = ip_queue_to_ill_v6(q);
				if (ill == NULL) {
					BUMP_MIB(icmp6_mib.ipv6IfIcmpInErrors);
				} else {
					BUMP_MIB(ill->ill_icmp6_mib->
					    ipv6IfIcmpInErrors);
				}
				freemsg(mp);
				return (NULL);
			}
			ip6h = (ip6_t *)mp->b_rptr;
		}
		icmp6 = (icmp6_t *)&ip6h[1];
		/* Explicitly do not generate errors in response to redirects */
		if (ICMP6_IS_ERROR(icmp6->icmp6_type) ||
		    icmp6->icmp6_type == ND_REDIRECT) {
			freemsg(mp);
			return (NULL);
		}
	}
	/*
	 * Check that the destination is not multicast and that the packet
	 * was not sent on link layer broadcast or multicast.  (Exception
	 * is Packet too big message as per the draft - when mcast_ok is set.)
	 */
	if (!mcast_ok &&
	    (llbcast || IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst))) {
		freemsg(mp);
		return (NULL);
	}
	if (icmp_err_rate_limit()) {
		/*
		 * Only send ICMP error packets every so often.
		 * This should be done on a per port/source basis,
		 * but for now this will suffice.
		 */
		freemsg(mp);
		return (NULL);
	}
	return (mp);
}

/*
 * Generate an ICMPv6 redirect message.
 * Include target link layer address option if it exits.
 * Always include redirect header.
 */
void
icmp_send_redirect_v6(queue_t *q, mblk_t *mp,
    in6_addr_t *targetp, in6_addr_t *dest,
    ill_t *ill, boolean_t llbcast)
{
	nd_redirect_t	*rd;
	nd_opt_rd_hdr_t	*rdh;
	uchar_t		*buf;
	nce_t		*nce = NULL;
	nd_opt_hdr_t	*opt;
	int		len;
	int		ll_opt_len = 0;
	int		max_redir_hdr_data_len;
	int		pkt_len;

	mp = icmp_pkt_err_ok_v6(q, mp, llbcast, B_FALSE);
	if (mp == NULL)
		return;
	nce = ndp_lookup(ill, targetp);
	if (nce != NULL && nce->nce_state != ND_INCOMPLETE) {
		ll_opt_len = (sizeof (nd_opt_hdr_t) +
		    ill->ill_phys_addr_length + 7)/8 * 8;
	}
	len = sizeof (nd_redirect_t) + sizeof (nd_opt_rd_hdr_t) + ll_opt_len;
	ASSERT(len % 4 == 0);
	buf = kmem_alloc(len, KM_NOSLEEP);
	if (buf == NULL) {
		if (nce != NULL)
			NCE_REFRELE(nce);
		freemsg(mp);
		return;
	}

	rd = (nd_redirect_t *)buf;
	rd->nd_rd_type = (uint8_t)ND_REDIRECT;
	rd->nd_rd_code = 0;
	rd->nd_rd_reserved = 0;
	rd->nd_rd_target = *targetp;
	rd->nd_rd_dst = *dest;

	opt = (nd_opt_hdr_t *)(buf + sizeof (nd_redirect_t));
	if (nce != NULL && ll_opt_len != 0) {
		opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
		opt->nd_opt_len = ll_opt_len/8;
		bcopy((char *)nce->nce_res_mp->b_rptr +
		    NCE_LL_ADDR_OFFSET(ill), &opt[1],
		    ill->ill_phys_addr_length);
	}
	if (nce != NULL)
		NCE_REFRELE(nce);
	rdh = (nd_opt_rd_hdr_t *)(buf + sizeof (nd_redirect_t) + ll_opt_len);
	rdh->nd_opt_rh_type = (uint8_t)ND_OPT_REDIRECTED_HEADER;
	/* max_redir_hdr_data_len and nd_opt_rh_len must be multiple of 8 */
	max_redir_hdr_data_len = (ipv6_icmp_return - IPV6_HDR_LEN - len)/8*8;
	pkt_len = msgdsize(mp);
	/* Make sure mp is 8 byte aligned */
	if (pkt_len > max_redir_hdr_data_len) {
		rdh->nd_opt_rh_len = (max_redir_hdr_data_len +
		    sizeof (nd_opt_rd_hdr_t))/8;
		(void) adjmsg(mp, max_redir_hdr_data_len - pkt_len);
	} else {
		rdh->nd_opt_rh_len = (pkt_len + sizeof (nd_opt_rd_hdr_t))/8;
		(void) adjmsg(mp, -(pkt_len % 8));
	}
	rdh->nd_opt_rh_reserved1 = 0;
	rdh->nd_opt_rh_reserved2 = 0;
	/* ipif_v6src_addr contains the link-local source address */
	icmp_pkt_v6(q, mp, buf, len, &ill->ill_ipif->ipif_v6src_addr);
	kmem_free(buf, len);
}


/* Generate an ICMP time exceeded message.  (May be called as writer.) */
void
icmp_time_exceeded_v6(queue_t *q, mblk_t *mp, uint8_t code,
    boolean_t llbcast, boolean_t mcast_ok)
{
	icmp6_t	icmp6;

	mp = icmp_pkt_err_ok_v6(q, mp, llbcast, mcast_ok);
	if (mp == NULL)
		return;
	bzero(&icmp6, sizeof (icmp6_t));
	icmp6.icmp6_type = ICMP6_TIME_EXCEEDED;
	icmp6.icmp6_code = code;
	icmp_pkt_v6(q, mp, &icmp6, sizeof (icmp6_t), NULL);
}

/*
 * Generate an ICMP unreachable message.
 */
void
icmp_unreachable_v6(queue_t *q, mblk_t *mp, uint8_t code,
    boolean_t llbcast, boolean_t mcast_ok)
{
	icmp6_t	icmp6;

	mp = icmp_pkt_err_ok_v6(q, mp, llbcast, mcast_ok);
	if (mp == NULL)
		return;
	bzero(&icmp6, sizeof (icmp6_t));
	icmp6.icmp6_type = ICMP6_DST_UNREACH;
	icmp6.icmp6_code = code;
	icmp_pkt_v6(q, mp, &icmp6, sizeof (icmp6_t), NULL);
}

/*
 * Generate an ICMP pkt too big message.
 */
void
icmp_pkt2big_v6(queue_t *q, mblk_t *mp, uint32_t mtu,
    boolean_t llbcast, boolean_t mcast_ok)
{
	icmp6_t	icmp6;

	mp = icmp_pkt_err_ok_v6(q, mp, llbcast, mcast_ok);
	if (mp == NULL)
		return;
	bzero(&icmp6, sizeof (icmp6_t));
	icmp6.icmp6_type = ICMP6_PACKET_TOO_BIG;
	icmp6.icmp6_code = 0;
	icmp6.icmp6_mtu = htonl(mtu);
	icmp_pkt_v6(q, mp, &icmp6, sizeof (icmp6_t), NULL);
}

/*
 * Generate an ICMP parameter problem message. (May be called as writer.)
 * 'offset' is the offset from the beginning of the packet in error.
 */
void
icmp_param_problem_v6(queue_t *q, mblk_t *mp, uint8_t code,
    uint32_t offset, boolean_t llbcast, boolean_t mcast_ok)
{
	icmp6_t	icmp6;

	mp = icmp_pkt_err_ok_v6(q, mp, llbcast, mcast_ok);
	if (mp == NULL)
		return;
	bzero((char *)&icmp6, sizeof (icmp6_t));
	icmp6.icmp6_type = ICMP6_PARAM_PROB;
	icmp6.icmp6_code = code;
	icmp6.icmp6_pptr = htonl(offset);
	icmp_pkt_v6(q, mp, &icmp6, sizeof (icmp6_t), NULL);
}

/*
 * This code will need to take into account the possibility of binding
 * to a link local address on a multi-homed host, in which case the
 * outgoing interface (from the ipc) will need to be used when getting
 * an ire for the dst.
 */
void
ip_bind_v6(queue_t *q, mblk_t *mp)
{
	ipc_t		*ipc = (ipc_t *)q->q_ptr;
	icf_t		*icf;
	ssize_t		len;
	int		protocol;
	struct T_bind_req	*tbr;
	sin6_t		*sin6;
	ipa6_conn_t	*ac6;
	in6_addr_t	v6src;
	in6_addr_t	v6dst;
	uint16_t	lport;
	uint16_t	fport;
	uchar_t		*ucp;
	mblk_t		*mp1;
	boolean_t	ire_requested;
	int		error = 0;
	boolean_t	local_bind;
	boolean_t	orig_pkt_isv6 = ipc->ipc_pkt_isv6;

	ASSERT(ipc->ipc_af_isv6);
	len = mp->b_wptr - mp->b_rptr;
	if (len < (sizeof (*tbr) + 1)) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "ip_bind_v6: bogus msg, len %ld", len);
		freemsg(mp);
		return;
	}
	/* Back up and extract the protocol identifier. */
	mp->b_wptr--;
	tbr = (struct T_bind_req *)mp->b_rptr;
	/* Reset the message type in preparation for shipping it back. */
	mp->b_datap->db_type = M_PCPROTO;

	protocol = *mp->b_wptr & 0xFF;
	ipc->ipc_ulp = (uint8_t)protocol;

	/*
	 * Check for a zero length address.  This is from a protocol that
	 * wants to register to receive all packets of its type.
	 */
	if (tbr->ADDR_length == 0) {
		if ((protocol == IPPROTO_TCP ||
		    protocol == IPPROTO_ESP || protocol == IPPROTO_AH) &&
		    ipc_proto_fanout_v6[protocol].icf_ipc != NULL) {
			/*
			 * TCP, AH, and ESP have single protocol fanouts.
			 * Do not allow others to bind to these.
			 */
			goto bad_addr;
		}

		/* No hash here really.  The table is big enough. */
		ipc->ipc_v6laddr = ipv6_all_zeros;
		icf = &ipc_proto_fanout_v6[protocol];
		ipc_hash_insert_wildcard(icf, ipc);
		tbr->PRIM_type = T_BIND_ACK;
		qreply(q, mp);
		return;
	}

	/* Extract the address pointer from the message. */
	ucp = (uchar_t *)mi_offset_param(mp, tbr->ADDR_offset,
	    tbr->ADDR_length);
	if (ucp == NULL) {
		ip1dbg(("ip_bind_v6: no address\n"));
		goto bad_addr;
	}
	if (!OK_32PTR(ucp)) {
		ip1dbg(("ip_bind_v6: unaligned address\n"));
		goto bad_addr;
	}

	mp1 = mp->b_cont;	/* trailing mp if any */
	ire_requested = (mp1 && mp1->b_datap->db_type == IRE_DB_REQ_TYPE);

	switch (tbr->ADDR_length) {
	default:
		ip1dbg(("ip_bind_v6: bad address length %d\n",
		    (int)tbr->ADDR_length));
		goto bad_addr;

	case IPV6_ADDR_LEN:
		/* Verification of local address only */
		v6src = *(in6_addr_t *)ucp;
		lport = 0;
		local_bind = B_TRUE;
		break;

	case sizeof (sin6_t):
		sin6 = (sin6_t *)ucp;
		v6src = sin6->sin6_addr;
		lport = sin6->sin6_port;
		local_bind = B_TRUE;
		break;

	case sizeof (ipa6_conn_t):
		/*
		 * Verify that both the source and destination addresses
		 * are valid.
		 * Note that we allow connect to broadcast and multicast
		 * addresses when ire_requested is set. Thus the ULP
		 * has to check for IRE_BROADCAST and multicast.
		 */
		ac6 = (ipa6_conn_t *)ucp;
		v6src = ac6->ac6_laddr;
		v6dst = ac6->ac6_faddr;
		fport = ac6->ac6_fport;
		lport = ac6->ac6_lport;
		local_bind = B_FALSE;
		break;
	}
	if (local_bind) {
		if (IN6_IS_ADDR_V4MAPPED(&v6src)) {
			/* Bind to IPv4 address */
			ipaddr_t v4src;

			IN6_V4MAPPED_TO_IPADDR(&v6src, v4src);
			if (CLASSD(v4src)) {
				error = EADDRNOTAVAIL;
				goto bad_addr;
			}
			/*
			 * XXX Fix needed. Need to pass ipsec_policy_set
			 * instead of B_FALSE.
			 */
			error = ip_bind_laddr(ipc, mp, v4src, lport,
			    ire_requested, B_FALSE,
			    tbr->ADDR_length != IPV6_ADDR_LEN);
			if (error != 0)
				goto bad_addr;
			ipc->ipc_pkt_isv6 = B_FALSE;
		} else {
			error = ip_bind_laddr_v6(ipc, mp, &v6src, lport,
			    ire_requested, (tbr->ADDR_length != IPV6_ADDR_LEN));
			if (error != 0)
				goto bad_addr;
			ipc->ipc_pkt_isv6 = B_TRUE;
		}
	} else {
		/*
		 * Bind to local and remorte address. Local might be
		 * unspecified in which case it will be extracted from
		 * ire_src_addr_v6
		 */
		if (IN6_IS_ADDR_V4MAPPED(&v6dst)) {
			/* Connect to IPv4 address */
			ipaddr_t v4src;
			ipaddr_t v4dst;

			/* Is the source unspecified or mapped? */
			if (!IN6_IS_ADDR_V4MAPPED(&v6src) &&
			    !IN6_IS_ADDR_UNSPECIFIED(&v6src)) {
				ip1dbg(("ip_bind_connected_v6: dest mapped but"
				    "not src\n"));
				goto bad_addr;
			}
			IN6_V4MAPPED_TO_IPADDR(&v6src, v4src);
			IN6_V4MAPPED_TO_IPADDR(&v6dst, v4dst);

			if (CLASSD(v4dst)) {
				error = EADDRNOTAVAIL;
				goto bad_addr;
			}
			/*
			 * XXX Fix needed. Need to pass ipsec_policy_set
			 * instead of B_FALSE.
			 */
			error = ip_bind_connected(ipc, mp, v4src, lport,
				v4dst, fport, ire_requested, B_FALSE, B_TRUE);
			if (error != 0)
				goto bad_addr;
			ipc->ipc_pkt_isv6 = B_FALSE;
		} else if (IN6_IS_ADDR_V4MAPPED(&v6src)) {
			ip1dbg(("ip_bind_connected_v6: src "
			    "mapped but not dst\n"));
			goto bad_addr;
		} else {
			error = ip_bind_connected_v6(ipc, mp, &v6src,
			    lport, &v6dst, fport, ire_requested, B_TRUE);
			if (error != 0)
				goto bad_addr;
			ipc->ipc_pkt_isv6 = B_TRUE;
		}
	}
	/* Update qinfo if v4/v6 changed */
	if (orig_pkt_isv6 != ipc->ipc_pkt_isv6)
		ip_setqinfo(RD(q), ipc->ipc_pkt_isv6, B_TRUE);

	/*
	 * Pass the IPSEC headers size in ire_ipsec_options_size.
	 * We can't do this in ip_bind_insert_ire because the policy
	 * may not have been inherited at that point in time and hence
	 * ipc_out_enforce_policy may not be set.
	 */
	if (ire_requested && ipc->ipc_out_enforce_policy) {
		ire_t *ire = (ire_t *)mp->b_cont->b_rptr;
		ire->ire_ipsec_options_size = (ipc_ipsec_length(ipc));
	}

	/* Send it home. */
	mp->b_datap->db_type = M_PCPROTO;
	tbr->PRIM_type = T_BIND_ACK;
	qreply(q, mp);
	return;

bad_addr:
	if (error > 0)
		mp = mi_tpi_err_ack_alloc(mp, TSYSERR, error);
	else
		mp = mi_tpi_err_ack_alloc(mp, TBADADDR, 0);
	if (mp)
		qreply(q, mp);
}

/*
 * Here address is verified to be a valid local address.
 * If the IRE_DB_REQ_TYPE mp is present, a multicast
 * address is also considered a valid local address.
 * In the case of a multicast address, however, the
 * upper protocol is expected to reset the src address
 * to 0 if it sees an ire with IN6_IS_ADDR_MULTICAST returned so that
 * no packets are emitted with multicast address as
 * source address.
 * The addresses valid for bind are:
 *	(1) - in6addr_any
 *	(2) - IP address of an UP interface
 *	(3) - IP address of a DOWN interface
 *	(4) - a multicast address. In this case
 *	the ipc will only receive packets destined to
 *	the specified multicast address. Note: the
 *	application still has to issue an
 *	IPV6_JOIN_GROUP socket option.
 *
 */
static int
ip_bind_laddr_v6(ipc_t *ipc, mblk_t *mp, const in6_addr_t *v6src,
    uint16_t lport, boolean_t ire_requested, boolean_t fanout_insert)
{
	int		error = 0;
	ire_t		*src_ire;
	struct T_bind_req	*tbr;
	ire_t		*save_ire;

	tbr = (struct T_bind_req *)mp->b_rptr;
	src_ire = NULL;

	/*
	 * If it was previously connected, ipc_fully_bound would have
	 * been set.
	 */
	ipc->ipc_fully_bound = B_FALSE;

	if (!IN6_IS_ADDR_UNSPECIFIED(v6src)) {
		src_ire = ire_route_lookup_v6(v6src, 0, 0,
		    0, NULL, NULL, NULL, MATCH_IRE_DSTONLY);
		/*
		 * If an address other than in6addr_any is requested,
		 * we verify that it is a valid address for bind
		 * Note: Following code is in if-else-if form for
		 * readability compared to a condition check.
		 */
		ASSERT(src_ire == NULL || !(src_ire->ire_type & IRE_BROADCAST));
		/* LINTED - statement has no consequent */
		if (IRE_IS_LOCAL(src_ire)) {
			/*
			 * (2) Bind to address of local UP interface
			 */
		} else if (ipif_lookup_addr_v6(v6src, NULL) != NULL) {
			/*
			 * (3) Bind to address of local DOWN interface
			 * (ipif_lookup_addr_v6() looks up all interfaces
			 * but we do not get here for UP interfaces
			 * - case (2) above)
			 */
			/*EMPTY*/;
		} else if (IN6_IS_ADDR_MULTICAST(v6src)) {
			ipif_t *ipif;

			/*
			 * (4) bind to multicast address.
			 * Fake out the IRE returned to upper layer to
			 * be a broadcast IRE in ip_bind_insert_ire_v6().
			 * Pass other information that matches
			 * the ipif (e.g. the source address).
			 *
			 * ipc_multicast_{ipif,ill} are shared between
			 * IPv4 and IPv6 and AF_INET6 sockets can
			 * send both IPv4 and IPv6 packets. Hence
			 * we have to check that "isv6" matches.
			 */
			ipif = ipc->ipc_multicast_ipif;
			if (ipif == NULL || !ipif->ipif_isv6) {
				/* Look for default like ip_wput_v6 */
				ipif = ipif_lookup_group_v6(
				    &ipv6_unspecified_group);
			}
			save_ire = src_ire;
			src_ire = NULL;
			if (ipif == NULL || !ire_requested ||
			    (src_ire = ipif_to_ire_v6(ipif)) == NULL) {
				src_ire = save_ire;
				error = EADDRNOTAVAIL;
			} else {
				ASSERT(src_ire != NULL);
				if (save_ire != NULL)
					ire_refrele(save_ire);
			}
		} else {
			/*
			 * Not a valid address for bind
			 */
			error = EADDRNOTAVAIL;
		}

		if (error != 0) {
			/* Red Alert!  Attempting to be a bogon! */
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_bind_laddr_v6: bad src"
				    " address %s\n", AF_INET6, v6src);
			}
			goto bad_addr;
		}
	}

	/* If not fanout_insert this was just an address verification */
	if (fanout_insert) {
		/*
		 * The addresses have been verified. Time to insert in
		 * the correct fanout list.
		 */
		ipc->ipc_v6laddr = *v6src;
		ipc->ipc_v6faddr = ipv6_all_zeros;
		ipc->ipc_lport = lport;
		ipc->ipc_fport = 0;
		error = ip_bind_fanout_insert_v6(ipc, *mp->b_wptr & 0xFF,
		    tbr->ADDR_length);
	}
	if (error == 0 && ire_requested) {
		if (!ip_bind_insert_ire_v6(mp, src_ire, v6src, NULL)) {
			error = -1;
		}
	}
bad_addr:
	if (src_ire != NULL)
		ire_refrele(src_ire);
	return (error);
}

/*
 * Verify that both the source and destination addresses
 * are valid.
 */
static int
ip_bind_connected_v6(ipc_t *ipc, mblk_t *mp, const in6_addr_t *v6src,
    uint16_t lport, const in6_addr_t *v6dst, uint16_t fport,
    boolean_t ire_requested, boolean_t fanout_insert)
{
	ire_t		*src_ire;
	ire_t		*dst_ire;
	int		error = 0;
	struct T_bind_req	*tbr;
	ire_t		*sire = NULL;

	tbr = (struct T_bind_req *)mp->b_rptr;
	src_ire = dst_ire = NULL;

	/*
	 * If we never got a disconnect before, clear it now.
	 */
	ipc->ipc_fully_bound = B_FALSE;

	if (IN6_IS_ADDR_MULTICAST(v6dst)) {
		ipif_t *ipif;

		/*
		 * Use an "emulated" IRE_BROADCAST to tell the transport it
		 * is a multicast.
		 * Pass other information that matches
		 * the ipif (e.g. the source address).
		 *
		 * ipc_multicast_{ipif,ill} are shared between
		 * IPv4 and IPv6 and AF_INET6 sockets can
		 * send both IPv4 and IPv6 packets. Hence
		 * we have to check that "isv6" matches.
		 */
		ipif = ipc->ipc_multicast_ipif;
		if (ipif == NULL || !ipif->ipif_isv6) {
			/* Look for default like ip_wput_v6 */
			ipif = ipif_lookup_group_v6(&ipv6_unspecified_group);
		}
		if (ipif == NULL || !ire_requested ||
		    (dst_ire = ipif_to_ire_v6(ipif)) == NULL) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_bind_connected_v6: bad "
				    "connected multicast %s\n", AF_INET6,
				    v6dst);
			}
			return (ENETUNREACH);
		}
	} else {
		dst_ire = ire_route_lookup_v6(v6dst, NULL, NULL, 0,
		    NULL, &sire, NULL,
		    (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT));
		/*
		 * We also prevent ire's with src address INADDR_ANY to
		 * be used, which are created temporarily for
		 * sending out packets from endpoints that have
		 * IPC_UNSPEC_SRC set.
		 */
		if (dst_ire == NULL ||
		    IN6_IS_ADDR_UNSPECIFIED(&dst_ire->ire_src_addr_v6)) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_bind_connected_v6: bad "
				    "connected dst %s\n", AF_INET6, v6dst);
			}
			if (dst_ire != NULL)
				ire_refrele(dst_ire);
			if (sire != NULL)
				ire_refrele(sire);
			return (ENETUNREACH);
		}
	}

	ASSERT(dst_ire->ire_ipversion == IPV6_VERSION);

	/*
	 * Supply a local source address such that
	 * interface group balancing happens.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(v6src)) {
		/*
		 * Do the moral equivalent of parts of
		 * ip_newroute_v6(), including the possible
		 * reassignment of dst_ire.  Reassignment
		 * should happen if it is enabled, and the
		 * logical interface in question isn't in
		 * a singleton group.
		 *
		 * Note: While we pick a src_ipif we are really only interested
		 * in the ill for load balancing. The source ipif is determined
		 * by source address selection below.
		 */
		ipif_t *dst_ipif = dst_ire->ire_ipif;
		ipif_t *sched_ipif;
		ire_t *sched_ire;

		if (ip_enable_group_ifs &&
		    dst_ipif->ipif_ifgrpnext != dst_ipif) {

			/* Reassign dst_ire based on ifgrp. */

			sched_ipif = ifgrp_scheduler(dst_ipif);
			if (sched_ipif != NULL) {
				sched_ire = ipif_to_ire_v6(sched_ipif);
				/*
				 * Reassign dst_ire to
				 * correspond to the results
				 * of ifgrp scheduling.
				 */
				if (sched_ire != NULL) {
					IRE_REFRELE(dst_ire);
					dst_ire = sched_ire;
				}
			}
		}
		/*
		 * Determine the best source address on this ill for
		 * the destination.
		 */
		sched_ipif = ipif_select_source_v6(dst_ire->ire_ipif->ipif_ill,
		    v6dst);
		if (sched_ipif != NULL) {
			sched_ire = ipif_to_ire_v6(sched_ipif);
			if (sched_ire != NULL) {
				IRE_REFRELE(dst_ire);
				dst_ire = sched_ire;
			}
		}
		v6src = &dst_ire->ire_src_addr_v6;
	}

	/*
	 * We do ire_route_lookup() here (and not
	 * interface lookup as we assert that
	 * src_addr should only come from an
	 * UP interface for hard binding.
	 */
	src_ire = ire_route_lookup_v6(v6src, 0, 0, 0, NULL,
	    NULL, NULL, MATCH_IRE_DSTONLY);

	/* src_ire must be a local|loopback */
	if (!IRE_IS_LOCAL(src_ire)) {
		if (ip_debug > 2) {
			/* ip1dbg */
			pr_addr_dbg("ip_bind_connected_v6: bad "
			    "connected src %s\n", AF_INET6, v6src);
		}
		error = EADDRNOTAVAIL;
		goto bad_addr;
	}

	/*
	 * If the source address is a loopback address, the
	 * destination had best be local or multicast.
	 * The transports that can't handle multicast will reject
	 * those addresses.
	 */
	if (src_ire->ire_type == IRE_LOOPBACK &&
	    !(IRE_IS_LOCAL(dst_ire) || IN6_IS_ADDR_MULTICAST(v6dst))) {
		ip1dbg(("ip_bind_connected_v6: bad connected loopback\n"));
		error = -1;
		goto bad_addr;
	}
	/* If not fanout_insert this was just an address verification */
	if (fanout_insert) {
		/*
		 * The addresses have been verified. Time to insert in
		 * the correct fanout list.
		 */
		ipc->ipc_v6laddr = *v6src;
		ipc->ipc_v6faddr = *v6dst;
		ipc->ipc_lport = lport;
		ipc->ipc_fport = fport;
		error = ip_bind_fanout_insert_v6(ipc, *mp->b_wptr & 0xFF,
		    tbr->ADDR_length);
	}
	if (error == 0 && ire_requested) {
		iulp_t *ulp_info = NULL;

		/*
		 * Note that sire will not be NULL if this is an off-link
		 * connection and there is not cache for that dest yet.
		 *
		 * XXX Because of an existing bug, if there are multiple
		 * default routes, the IRE returned now may not be the actual
		 * default route used (default routes are chosen in a
		 * round robin fashion).  So if the metrics for different
		 * default routes are different, we may return the wrong
		 * metrics.  This will not be a problem if the existing
		 * bug is fixed.
		 */
		if (sire != NULL)
			ulp_info = &(sire->ire_uinfo);
		if (!ip_bind_insert_ire_v6(mp, dst_ire, v6dst, ulp_info)) {
			error = -1;
		}
	}
	if (error == 0)
		ipc->ipc_fully_bound = B_TRUE;
bad_addr:
	if (src_ire != NULL)
		IRE_REFRELE(src_ire);
	if (dst_ire != NULL)
		IRE_REFRELE(dst_ire);
	if (sire != NULL)
		IRE_REFRELE(sire);
	return (error);
}

/*
 * Insert an ipc in the correct fanout table.
 */
static int
ip_bind_fanout_insert_v6(ipc_t *ipc, int protocol, int addr_len)
{
	icf_t		*icf;

	switch (protocol) {
	case IPPROTO_UDP:
	default:
		/*
		 * Note the requested port number and IP address for use
		 * in the inbound fanout.  Validation (and uniqueness) of
		 * the port/address request is UDPs business.
		 */
		if (protocol == IPPROTO_UDP) {
			icf = &ipc_udp_fanout_v6[IP_UDP_HASH(ipc->ipc_lport)];
		} else {
			/* No hash here really.  The table is big enough. */
			icf = &ipc_proto_fanout_v6[protocol];
		}
		/*
		 * Insert entries with a specified remote address first,
		 * followed by those with a specified local address and
		 * ending with those bound to INADDR_ANY. This ensures
		 * that the search from the beginning of a hash bucket
		 * will find the most specific match.
		 * IP_UDP_MATCH_V6 assumes this insertion order.
		 */
		if (!IN6_IS_ADDR_UNSPECIFIED(&ipc->ipc_v6faddr))
			ipc_hash_insert_connected(icf, ipc);
		else if (!IN6_IS_ADDR_UNSPECIFIED(&ipc->ipc_v6laddr))
			ipc_hash_insert_bound(icf, ipc);
		else
			ipc_hash_insert_wildcard(icf, ipc);
		break;

	case IPPROTO_TCP:
		switch (addr_len) {
		case sizeof (ipa6_conn_t):
			ASSERT(!IN6_IS_ADDR_UNSPECIFIED(&ipc->ipc_v6faddr));
			/* Insert the IPC in the TCP fanout hash table. */
			icf = &ipc_tcp_conn_fanout_v6[
			    IP_TCP_CONN_HASH_V6(ipc->ipc_v6faddr,
			    ipc->ipc_ports)];
			ipc_hash_insert_connected(icf, ipc);
			break;

		case sizeof (sin6_t):
			/* Insert the IPC in the TCP listen hash table. */
			icf = &ipc_tcp_listen_fanout_v6[
			    IP_TCP_LISTEN_HASH_V6(ipc->ipc_lport)];
			if (!IN6_IS_ADDR_UNSPECIFIED(&ipc->ipc_v6laddr))
				ipc_hash_insert_bound(icf, ipc);
			else
				ipc_hash_insert_wildcard(icf, ipc);
			break;
		}
		break;
	}
	return (0);
}

/*
 * Insert the ire in b_cont. Returns false if it fails (due to lack of space).
 * Makes the IRE be IRE_BROADCAST if addr is a multicast address.
 */
static boolean_t
ip_bind_insert_ire_v6(mblk_t *mp, ire_t *ire, const in6_addr_t *addr,
    iulp_t *ulp_info)
{
	mblk_t	*mp1;
	ire_t	*ret_ire;

	mp1 = mp->b_cont;
	ASSERT(mp1 != NULL);

	if (ire != NULL) {
		/*
		 * mp1 initialized above to IRE_DB_REQ_TYPE
		 * appended mblk. Its <upper protocol>'s
		 * job to make sure there is room.
		 */
		if ((mp1->b_datap->db_lim - mp1->b_rptr) < sizeof (ire_t))
			return (0);

		mp1->b_datap->db_type = IRE_DB_TYPE;
		mp1->b_wptr = mp1->b_rptr + sizeof (ire_t);
		bcopy(ire, mp1->b_rptr, sizeof (ire_t));
		if (ire->ire_ipif != NULL) {
			ill_t   *ill = ire->ire_ipif->ipif_ill;

			if ((ill != NULL) &&
			    (ill->ill_ick.ick_magic == ICK_M_CTL_MAGIC))
				mp1->b_ick_flag = ICK_VALID;
		}
		ret_ire = (ire_t *)mp1->b_rptr;
		if (IN6_IS_ADDR_MULTICAST(addr)) {
			ret_ire->ire_type = IRE_BROADCAST;
			ret_ire->ire_addr_v6 = *addr;
		}
		if (ulp_info != NULL) {
			bcopy(ulp_info, &(ret_ire->ire_uinfo),
			    sizeof (iulp_t));
		}
	} else {
		/*
		 * No IRE was found. Remove IRE mblk.
		 */
		mp->b_cont = mp1->b_cont;
		freeb(mp1);
	}
	return (1);
}

/*
 * Add an ip6i_t header to the front of the mblk.
 * Inline if possible else allocate a separate mblk containing only the ip6i_t.
 * Returns NULL if allocation fails (and frees original message).
 * Used in outgoing path when going through ip_newroute_*v6().
 * Used in incoming path to pass ifindex to transports.
 */
static mblk_t *
ip_add_info_v6(mblk_t *mp, ill_t *ill, const in6_addr_t *dst)
{
	mblk_t *mp1;
	ip6i_t *ip6i;
	ip6_t *ip6h;

	ip6h = (ip6_t *)mp->b_rptr;
	ip6i = (ip6i_t *)(mp->b_rptr - sizeof (ip6i_t));
	if ((uchar_t *)ip6i < mp->b_datap->db_base ||
	    mp->b_datap->db_ref > 1) {
		mp1 = allocb(sizeof (ip6i_t), BPRI_MED);
		if (mp1 == NULL) {
			freemsg(mp);
			return (NULL);
		}
		mp1->b_wptr = mp1->b_rptr = mp1->b_datap->db_lim;
		mp1->b_cont = mp;
		mp = mp1;
		ip6i = (ip6i_t *)(mp->b_rptr - sizeof (ip6i_t));
	}
	mp->b_rptr = (uchar_t *)ip6i;
	ip6i->ip6i_vcf = ip6h->ip6_vcf;
	ip6i->ip6i_nxt = IPPROTO_RAW;
	if (ill != NULL) {
		ip6i->ip6i_flags = IP6I_IFINDEX;
		ip6i->ip6i_ifindex = ill->ill_index;
	} else {
		ip6i->ip6i_flags = 0;
	}
	ip6i->ip6i_nexthop = *dst;
	return (mp);
}

/*
 * Handle protocols with which IP is less intimate.  There
 * can be more than one stream bound to a particular
 * protocol.  When this is the case, normally each one gets a copy
 * of any incoming packets.
 * However, if the packet was tunneled and not multicast we only send to it
 * the first match.
 */
static void
ip_fanout_proto_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h, ill_t *ill,
    uint8_t nexthdr, uint_t nexthdr_offset, uint_t flags)
{
	icf_t	*icf;
	ipc_t	*ipc, *first_ipc, *next_ipc;
	queue_t	*rq;
	mblk_t	*mp1;
	in6_addr_t dst = ip6h->ip6_dst;
	in6_addr_t src = ip6h->ip6_src;
	boolean_t one_only;

	/*
	 * If the packet was tunneled and not multicast we only send to it
	 * the first match.
	 */
	one_only = ((nexthdr == IPPROTO_ENCAP || nexthdr == IPPROTO_IPV6) &&
	    !IN6_IS_ADDR_MULTICAST(&dst));

	icf = &ipc_proto_fanout_v6[nexthdr];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No one bound to this port.  Is
			 * there a client that wants all
			 * unclaimed datagrams?
			 */
			mutex_exit(&icf->icf_lock);
			ip_fanout_send_icmp_v6(q, mp, flags,
			    &ill->ill_ip6_mib->ipv6InUnknownProtos,
			    ICMP6_PARAM_PROB, ICMP6_PARAMPROB_NEXTHEADER,
			    nexthdr_offset);
			return;
		}
		/* IP_PROTO_MATCH_V6 compares ipc_incomming_ill and multicast */
	} while (!(IP_PROTO_MATCH_V6(ipc, ill, nexthdr, dst, src)));

	IPC_REFHOLD(ipc);
	first_ipc = ipc;
	if (one_only) {
		/*
		 * Only send message to one tunnel driver by immediately
		 * terminating the loop.
		 */
		ipc = NULL;
	} else {
		ipc = ipc->ipc_hash_next;
	}
	for (;;) {
		while (ipc != NULL) {
			if ((IP_PROTO_MATCH_V6(ipc, ill, nexthdr, dst, src)))
				break;
			ipc = ipc->ipc_hash_next;
		}
		if (ipc == NULL || (((mp1 = dupmsg(mp)) == NULL) &&
		    ((mp1 = copymsg(mp)) == NULL))) {
			/*
			 * No more intested clients or memory
			 * allocation failed
			 */
			ipc = first_ipc;
			break;
		}
		IPC_REFHOLD(ipc);
		mutex_exit(&icf->icf_lock);
		rq = ipc->ipc_rq;
		/*
		 * For link-local always add ifindex so that transport can set
		 * sin6_scope_id. Avoid it for ICMP error fanout.
		 */
		if ((ipc->ipc_ipv6_recvpktinfo ||
		    IN6_IS_ADDR_LINKLOCAL(&src)) &&
		    (flags & IP_FF_IP6INFO)) {
			/* Add header */
			mp1 = ip_add_info_v6(mp1, ill, &dst);
		}
		if (mp1 == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
		} else if (!canputnext(rq)) {
			Counter *mibincr;

			if (flags & IP_FF_RAWIP)
				mibincr = &ill->ill_ip6_mib->rawipInOverflows;
			else
				mibincr =
				    &ill->ill_icmp6_mib->ipv6IfIcmpInOverflows;

			BUMP_MIB(*mibincr);
			freemsg(mp1);
		} else {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
			putnext(rq, mp1);
		}
		mutex_enter(&icf->icf_lock);
		/* Follow the next pointer before releasing the ipc. */
		next_ipc = ipc->ipc_hash_next;
		IPC_REFRELE(ipc);
		ipc = next_ipc;
	}

	/* Last one.  Send it upstream. */
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	/*
	 * For link-local always add ifindex so that transport can set
	 * sin6_scope_id. Avoid it for ICMP error fanout.
	 */
	if ((ipc->ipc_ipv6_recvpktinfo || IN6_IS_ADDR_LINKLOCAL(&src)) &&
	    (flags & IP_FF_IP6INFO)) {
		/* Add header */
		mp = ip_add_info_v6(mp, ill, &dst);
		if (mp == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			IPC_REFRELE(ipc);
			return;
		}
	}
	if (!canputnext(rq)) {
		Counter *mibincr;

		if (flags & IP_FF_RAWIP)
			mibincr = &ill->ill_ip6_mib->rawipInOverflows;
		else
			mibincr = &ill->ill_icmp6_mib->ipv6IfIcmpInOverflows;

		BUMP_MIB(*mibincr);
		freemsg(mp);
	} else {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
		putnext(rq, mp);
	}
	IPC_REFRELE(ipc);
}

/*
 * Send an ICMP error after patching up the packet appropriately.
 */
static void
ip_fanout_send_icmp_v6(queue_t *q, mblk_t *mp, uint_t flags, Counter *mibincr,
    uint_t icmp_type, uint8_t icmp_code, uint_t nexthdr_offset)
{
	ip6_t *ip6h = (ip6_t *)mp->b_rptr;

	if (flags & IP_FF_SEND_ICMP) {
		BUMP_MIB(*mibincr);
		if (flags & IP_FF_HDR_COMPLETE) {
			if (ip_hdr_complete_v6(ip6h)) {
				freemsg(mp);
				return;
			}
		}
		switch (icmp_type) {
		case ICMP6_DST_UNREACH:
			icmp_unreachable_v6(WR(q), mp, icmp_code,
			    B_FALSE, B_FALSE);
			break;
		case ICMP6_PARAM_PROB:
			icmp_param_problem_v6(WR(q), mp, icmp_code,
			    nexthdr_offset, B_FALSE, B_FALSE);
			break;
		default:
#ifdef DEBUG
			cmn_err(CE_PANIC, "ip_fanout_send_icmp_v6: wrong type");
#endif
			freemsg(mp);
			break;
		}
	} else {
		freemsg(mp);
	}
}

/*
 * Fanout for TCP packets
 * The caller puts <fport, lport> in the ports parameter.
 */
static void
ip_fanout_tcp_listen_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h,
    uint32_t ports, ill_t *ill, uint_t flags, uint_t hdr_len)
{
	icf_t	*icf;
	ipc_t	*ipc;
	queue_t	*rq;
	uint16_t	dstport;
	tcph_t	*tcph;
	ill_t	*ipc_ill;

	/* If this is a SYN packet attempt to add an IRE_DB_TYPE */
	tcph = (tcph_t *)&mp->b_rptr[hdr_len];
	if ((flags & IP_FF_SYN_ADDIRE) &&
	    (tcph->th_flags[0] & (TH_SYN|TH_ACK)) == TH_SYN) {
		/* SYN without the ACK - add an IRE for the source */
		ip_ire_append_v6(mp, &ip6h->ip6_src);
	}

	/* Extract port in net byte order */
	dstport = htons(ntohl(ports) & 0xFFFF);

	icf = &ipc_tcp_listen_fanout_v6[IP_TCP_LISTEN_HASH_V6(dstport)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No match on local port. Look for a client
			 * that wants all unclaimed.  Note
			 * that TCP must normally make sure that
			 * there is such a stream, otherwise it
			 * will be tough to get inbound connections
			 * going.
			 */
			mutex_exit(&icf->icf_lock);
			/*
			 * XXX Fix needed. Need to pass the right value
			 * instead of B_FALSE.
			 */
			ip_fanout_tcp_defq(q, mp, flags, B_FALSE);
			return;
		}
		ipc_ill = ipc->ipc_incoming_ill;
	} while (!IP_TCP_LISTEN_MATCH_V6(ipc, dstport, ip6h->ip6_dst) ||
		(ipc_ill != NULL && ipc_ill != ill));
	/* Got a client, up it goes. */
	IPC_REFHOLD(ipc);
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	/*
	 * For link-local always add ifindex so that TCP can bind to that
	 * interface. Avoid it for ICMP error fanout.
	 */
	if ((ipc->ipc_ipv6_recvpktinfo ||
	    IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src)) &&
	    (flags & IP_FF_IP6INFO)) {
		/* Add header */
		mp = ip_add_info_v6(mp, ill, &ip6h->ip6_dst);
		if (mp == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			IPC_REFRELE(ipc);
			return;
		}
	}
	BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
	putnext(rq, mp);
	IPC_REFRELE(ipc);
}

/*
 * Fanout for TCP packets
 * The caller puts <fport, lport> in the ports parameter.
 */
static void
ip_fanout_tcp_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h,
    uint32_t ports, ill_t *ill, uint_t flags, uint_t hdr_len)
{
	icf_t	*icf;
	ipc_t	*ipc;
	queue_t	*rq;
	ill_t	*ipc_ill;

	/* Find a TCP client stream for this packet. */
	icf = &ipc_tcp_conn_fanout_v6
	    [IP_TCP_CONN_HASH_V6(ip6h->ip6_src, ports)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No hard-bound match.  Look for a
			 * stream bound to the local port only.
			 */
			dblk_t *dp = mp->b_datap;

			mutex_exit(&icf->icf_lock);

			if (dp->db_struioflag & STRUIO_IP) {
				/*
				 * Do the postponed checksum now.
				 */
				mblk_t *mp1;
				ssize_t off = dp->db_struioptr - mp->b_rptr;

				if (IP_CSUM(mp, (uint32_t)off, 0)) {
					ipcsumdbg("swcksumerr1\n", mp);
					BUMP_MIB(ip_mib.tcpInErrs);
					freemsg(mp);
					return;
				}
				mp1 = mp;
				do {
					mp1->b_datap->db_struioflag &=
						~STRUIO_IP;
				} while ((mp1 = mp1->b_cont) != NULL);
			}
			ip_fanout_tcp_listen_v6(q, mp, ip6h, ports, ill,
			    flags, hdr_len);
			return;
		}
		ipc_ill = ipc->ipc_incoming_ill;
	} while (!IP_TCP_CONN_MATCH_V6(ipc, ip6h, ports) ||
		(ipc_ill != NULL && ipc_ill != ill));
	/* Got a client, up it goes. */
	IPC_REFHOLD(ipc);
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	/*
	 * For link-local always add ifindex so that TCP can bind to that
	 * interface. Avoid it for ICMP error fanout.
	 */
	if ((ipc->ipc_ipv6_recvpktinfo ||
	    IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src)) &&
	    (flags & IP_FF_IP6INFO)) {
		/* Add header */
		mp = ip_add_info_v6(mp, ill, &ip6h->ip6_dst);
		if (mp == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			IPC_REFRELE(ipc);
			return;
		}
	}
	BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
	putnext(rq, mp);
	IPC_REFRELE(ipc);
}

/*
 * Fanout for UDP packets.
 * The caller puts <fport, lport> in the ports parameter.
 * ire_type must be IRE_BROADCAST for multicast and broadcast packets.
 *
 * If SO_REUSEADDR is set all multicast and broadcast packets
 * will be delivered to all streams bound to the same port.
 */
static void
ip_fanout_udp_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h,
    uint32_t ports, ill_t *ill, uint_t flags)
{
	icf_t	*icf;
	ipc_t	*ipc;
	queue_t	*rq;
	uint32_t	dstport, srcport;
	in6_addr_t dst;
	ill_t	*ipc_ill;

	/* Extract ports in net byte order */
	dstport = htons(ntohl(ports) & 0xFFFF);
	srcport = htons(ntohl(ports) >> 16);
	dst = ip6h->ip6_dst;

	/* Attempt to find a client stream based on destination port. */
	icf = &ipc_udp_fanout_v6[IP_UDP_HASH(dstport)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	if (!IN6_IS_ADDR_MULTICAST(&dst)) {
		/*
		 * Not multicast. Send to the one (first)
		 * client we find. No need to check ipc_wantpacket_v6().
		 */
		do {
			ipc = ipc->ipc_hash_next;
			if (ipc == NULL) {
				mutex_exit(&icf->icf_lock);
				goto notfound;
			}
			ipc_ill = ipc->ipc_incoming_ill;
		} while (!IP_UDP_MATCH_V6(ipc, dstport, dst,
		    srcport, ip6h->ip6_src) ||
		    (ipc_ill != NULL && ipc_ill != ill));

		/* Found a client */
		IPC_REFHOLD(ipc);
		mutex_exit(&icf->icf_lock);
		rq = ipc->ipc_rq;
		/*
		 * For link-local always add ifindex so that transport can set
		 * sin6_scope_id. Avoid it for ICMP error fanout.
		 */
		if ((ipc->ipc_ipv6_recvpktinfo ||
		    IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src)) &&
		    (flags & IP_FF_IP6INFO)) {
			/* Add header */
			mp = ip_add_info_v6(mp, ill, &dst);
			if (mp == NULL) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
				IPC_REFRELE(ipc);
				return;
			}
		}
		if (!canputnext(rq)) {
			freemsg(mp);
			BUMP_MIB(ill->ill_ip6_mib->udpInOverflows);
		} else {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
			putnext(rq, mp);
		}
		IPC_REFRELE(ipc);
		return;
	}

	/*
	 * Multicast case
	 * If SO_REUSEADDR has been set on the first we send the
	 * packet to all clients that have joined the group and
	 * match the port.
	 */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			mutex_exit(&icf->icf_lock);
			goto notfound;
		}
	} while (!(IP_UDP_MATCH_V6(ipc, dstport, dst,
	    srcport, ip6h->ip6_src) && ipc_wantpacket_v6(ipc, ill, &dst)));

	if (ipc->ipc_reuseaddr) {
		ipc_t		*first_ipc = ipc;
		ipc_t		*next_ipc;
		mblk_t		*mp1;
		in6_addr_t	src = ip6h->ip6_src;

		IPC_REFHOLD(ipc);
		ipc = ipc->ipc_hash_next;
		for (;;) {
			while (ipc != NULL) {
				if (IP_UDP_MATCH_V6(ipc, dstport, dst,
				    srcport, src) &&
				    ipc_wantpacket_v6(ipc, ill, &dst))
					break;
				ipc = ipc->ipc_hash_next;
			}
			if (ipc == NULL || (((mp1 = dupmsg(mp)) == NULL) &&
			    ((mp1 = copymsg(mp)) == NULL))) {
				/*
				 * No more intested clients or memory
				 * allocation failed
				 */
				ipc = first_ipc;
				break;
			}
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			rq = ipc->ipc_rq;
			/*
			 * For link-local always add ifindex so that transport
			 * can set sin6_scope_id. Avoid it for ICMP error
			 * fanout.
			 */
			if ((ipc->ipc_ipv6_recvpktinfo ||
			    IN6_IS_ADDR_LINKLOCAL(&src)) &&
			    (flags & IP_FF_IP6INFO)) {
				/* Add header */
				mp1 = ip_add_info_v6(mp1, ill, &dst);
			}
			if (mp1 == NULL) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			} else if (!canputnext(rq)) {
				BUMP_MIB(ill->ill_ip6_mib->udpInOverflows);
				freemsg(mp1);
			} else {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
				putnext(rq, mp1);
			}
			mutex_enter(&icf->icf_lock);
			/* Follow the next pointer before releasing the ipc. */
			next_ipc = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
			ipc = next_ipc;
		}
	} else {
		IPC_REFHOLD(ipc);
	}

	/* Last one.  Send it upstream. */
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	/*
	 * For link-local always add ifindex so that transport can set
	 * sin6_scope_id. Avoid it for ICMP error fanout.
	 */
	if ((ipc->ipc_ipv6_recvpktinfo ||
	    IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src)) &&
	    (flags & IP_FF_IP6INFO)) {
		/* Add header */
		mp = ip_add_info_v6(mp, ill, &dst);
		if (mp == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			IPC_REFRELE(ipc);
			return;
		}
	}
	if (!canputnext(rq)) {
		BUMP_MIB(ill->ill_ip6_mib->udpInOverflows);
		freemsg(mp);
	} else {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
		putnext(rq, mp);
	}
	IPC_REFRELE(ipc);
	return;

notfound:
	/*
	 * No one bound to this port.  Is
	 * there a client that wants all
	 * unclaimed datagrams?
	 */
	if (ipc_proto_fanout_v6[IPPROTO_UDP].icf_ipc != NULL) {
		ip_fanout_proto_v6(q, mp, ip6h, ill, IPPROTO_UDP, 0,
		    flags | IP_FF_RAWIP | IP_FF_IP6INFO);
	} else {
		ip_fanout_send_icmp_v6(q, mp, flags, &ip_mib.udpNoPorts,
		    ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOPORT, 0);
	}
}

void
ip_fanout_destroy_v6(void)
{
	int i;

	for (i = 0; i < A_CNT(ipc_udp_fanout_v6); i++) {
		mutex_destroy(&ipc_udp_fanout_v6[i].icf_lock);
	}
	for (i = 0; i < ipc_tcp_conn_hash_size; i++) {
		mutex_destroy(&ipc_tcp_conn_fanout_v6[i].icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_tcp_listen_fanout_v6); i++) {
		mutex_destroy(&ipc_tcp_listen_fanout_v6[i].icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_proto_fanout_v6); i++) {
		mutex_destroy(&ipc_proto_fanout_v6[i].icf_lock);
	}
}

void
ip_fanout_init_v6(void)
{
	int i;

	for (i = 0; i < A_CNT(ipc_udp_fanout_v6); i++) {
		mutex_init(&ipc_udp_fanout_v6[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	/*
	 * ipc_tcp_conn_hash_size should have been already calculated
	 * in ip_fanout_init().  We can just use it here.
	 */
	ipc_tcp_conn_fanout_v6 =
	    (icf_t *)kmem_zalloc(ipc_tcp_conn_hash_size * sizeof (icf_t),
	    KM_SLEEP);
	for (i = 0; i < ipc_tcp_conn_hash_size; i++) {
		mutex_init(&ipc_tcp_conn_fanout_v6[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < A_CNT(ipc_tcp_listen_fanout_v6); i++) {
		mutex_init(&ipc_tcp_listen_fanout_v6[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}
	for (i = 0; i < A_CNT(ipc_proto_fanout_v6); i++) {
		mutex_init(&ipc_proto_fanout_v6[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}
}

/*
 * int ip_find_hdr_v6()
 *
 *  - Sets extension header pointers to appropriate location
 *
 *  - Determines IPv6 header length and returns it
 *
 *  - Returns a pointer to the last nexthdr value.
 *
 * The caller must initialize ipp_fields.
 *
 * NOTE: If multiple extension headers of the same type
 * are present, ip_find_hdr_v6() will set the respective
 * extension header pointers to the first one that it
 * encounters in the IPv6 header.
 * This routine is used by the upper layer protocol.
 * This routine deals with malformed packets of various sorts in which case
 * the returned length is up to the malformed part.
 */
int
ip_find_hdr_v6(mblk_t *mp, ip6_t *ip6h, ip6_pkt_t *ipp, uint8_t *nexthdrp)
{
	uint_t	length, ehdrlen;
	uint8_t nexthdr;
	uint8_t *whereptr, *endptr;
	ip6_dest_t *tmpdstopts;
	ip6_rthdr_t *tmprthdr;
	ip6_hbh_t *tmphopopts;

	length = IPV6_HDR_LEN;
	whereptr = ((uint8_t *)&ip6h[1]); /* point to next hdr */
	endptr = mp->b_wptr;

	nexthdr = ip6h->ip6_nxt;
	while (whereptr < endptr) {
		/* Is there enough left for len + nexthdr? */
		if (whereptr + MIN_EHDR_LEN > endptr)
			goto done;

		ASSERT(nexthdr != IPPROTO_FRAGMENT);
		switch (nexthdr) {
		case IPPROTO_HOPOPTS:
			tmphopopts = (ip6_hbh_t *)whereptr;
			ehdrlen = 8 * (tmphopopts->ip6h_len + 1);
			if ((uchar_t *)tmphopopts +  ehdrlen > endptr)
				goto done;
			nexthdr = tmphopopts->ip6h_nxt;
			/* return only 1st hbh */
			if (!(ipp->ipp_fields & IPPF_HOPOPTS)) {
				ipp->ipp_fields |= IPPF_HOPOPTS;
				ipp->ipp_hopopts = tmphopopts;
				ipp->ipp_hopoptslen = ehdrlen;
			}
			break;
		case IPPROTO_DSTOPTS:
			tmpdstopts = (ip6_dest_t *)whereptr;
			ehdrlen = 8 * (tmpdstopts->ip6d_len + 1);
			if ((uchar_t *)tmpdstopts +  ehdrlen > endptr)
				goto done;
			nexthdr = tmpdstopts->ip6d_nxt;
			/*
			 * ipp_dstopts is set to the destination header after a
			 * routing header.
			 * Assume it is a post-rthdr destination header
			 * and adjust when we find an rthdr.
			 */
			if (!(ipp->ipp_fields & IPPF_DSTOPTS)) {
				ipp->ipp_fields |= IPPF_DSTOPTS;
				ipp->ipp_dstopts = tmpdstopts;
				ipp->ipp_dstoptslen = ehdrlen;
			}
			break;
		case IPPROTO_ROUTING:
			tmprthdr = (ip6_rthdr_t *)whereptr;
			ehdrlen = 8 * (tmprthdr->ip6r_len + 1);
			if ((uchar_t *)tmprthdr +  ehdrlen > endptr)
				goto done;
			nexthdr = tmprthdr->ip6r_nxt;
			/* return only 1st rthdr */
			if (!(ipp->ipp_fields & IPPF_RTHDR)) {
				ipp->ipp_fields |= IPPF_RTHDR;
				ipp->ipp_rthdr = tmprthdr;
				ipp->ipp_rthdrlen = ehdrlen;
			}
			/*
			 * Make any destination header we've seen be a
			 * pre-rthdr destination header.
			 */
			if (ipp->ipp_fields & IPPF_DSTOPTS) {
				ipp->ipp_fields &= ~IPPF_DSTOPTS;
				ipp->ipp_fields |= IPPF_RTDSTOPTS;
				ipp->ipp_rtdstopts = ipp->ipp_dstopts;
				ipp->ipp_dstopts = NULL;
				ipp->ipp_rtdstoptslen = ipp->ipp_dstoptslen;
				ipp->ipp_dstoptslen = 0;
			}
			break;
		case IPPROTO_NONE:
		default:
			goto done;
		}
		length += ehdrlen;
		whereptr += ehdrlen;
	}
done:
	if (nexthdrp != NULL)
		*nexthdrp = nexthdr;
	return (length);
}

int
ip_hdr_complete_v6(ip6_t *ip6h)
{
	ire_t *ire;

	if (IN6_IS_ADDR_UNSPECIFIED(&ip6h->ip6_src)) {
		ire = ire_lookup_local_v6();
		if (ire == NULL) {
			ip1dbg(("ip_hdr_complete_v6: no source IRE\n"));
			return (1);
		}
		ip6h->ip6_src = ire->ire_addr_v6;
		ire_refrele(ire);
	}
	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6h->ip6_hops = ipv6_def_hops;
	return (0);
}

/*
 * Try to determine where and what are the IPv6 header length and
 * pointer to nexthdr value for the upper layer protocol (or an
 * unknown next hdr).
 *
 * Parameters returns a pointer to the nexthdr value;
 * Must handle malformed packets of various sorts.
 * Function returns failure for malformed cases.
 */
boolean_t
ip_hdr_length_nexthdr_v6(mblk_t *mp, ip6_t *ip6h, uint16_t *hdr_length_ptr,
    uint8_t **nexthdrpp)
{
	uint_t	length;
	uint_t	ehdrlen;
	uint8_t	*nexthdrp;
	uint8_t *whereptr;
	uint8_t *endptr;
	ip6_dest_t *desthdr;
	ip6_rthdr_t *rthdr;
	ip6_frag_t *fraghdr;

	length = IPV6_HDR_LEN;
	whereptr = ((uint8_t *)&ip6h[1]); /* point to next hdr */
	endptr = mp->b_wptr;

	nexthdrp = &ip6h->ip6_nxt;
	while (whereptr < endptr) {
		/* Is there enough left for len + nexthdr? */
		if (whereptr + MIN_EHDR_LEN > endptr)
			break;

		switch (*nexthdrp) {
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
			/* Assumes the headers are identical for hbh and dst */
			desthdr = (ip6_dest_t *)whereptr;
			ehdrlen = 8 * (desthdr->ip6d_len + 1);
			if ((uchar_t *)desthdr +  ehdrlen > endptr)
				return (B_FALSE);
			nexthdrp = &desthdr->ip6d_nxt;
			break;
		case IPPROTO_ROUTING:
			rthdr = (ip6_rthdr_t *)whereptr;
			ehdrlen =  8 * (rthdr->ip6r_len + 1);
			if ((uchar_t *)rthdr +  ehdrlen > endptr)
				return (B_FALSE);
			nexthdrp = &rthdr->ip6r_nxt;
			break;
		case IPPROTO_FRAGMENT:
			fraghdr = (ip6_frag_t *)whereptr;
			ehdrlen = sizeof (ip6_frag_t);
			if ((uchar_t *)&fraghdr[1] > endptr)
				return (B_FALSE);
			nexthdrp = &fraghdr->ip6f_nxt;
			break;
		case IPPROTO_NONE:
			/* No next header means we're finished */
		default:
			*hdr_length_ptr = length;
			*nexthdrpp = nexthdrp;
			return (B_TRUE);
		}
		length += ehdrlen;
		whereptr += ehdrlen;
		*hdr_length_ptr = length;
		*nexthdrpp = nexthdrp;
	}
	switch (*nexthdrp) {
	case IPPROTO_HOPOPTS:
	case IPPROTO_DSTOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_FRAGMENT:
		/*
		 * If any know extension headers are still to be processed,
		 * the packet's malformed (or at least all the IP header(s) are
		 * not in the same mblk - and that should never happen.
		 */
		return (B_FALSE);

	default:
		/*
		 * If we get here, we know that all of the IP headers were in
		 * the same mblk, even if the ULP header is in the next mblk.
		 */
		*hdr_length_ptr = length;
		*nexthdrpp = nexthdrp;
		return (B_TRUE);
	}
}

/*
 * Return the length of the IPv6 related headers (including extension headers)
 * Returns a length even if the packet is malformed.
 */
int
ip_hdr_length_v6(mblk_t *mp, ip6_t *ip6h)
{
	uint16_t hdr_len;
	uint8_t	*nexthdrp;

	(void) ip_hdr_length_nexthdr_v6(mp, ip6h, &hdr_len, &nexthdrp);
	return (hdr_len);
}

/* Select a source ipif subject to interface group load balancing */
ipif_t *
ip_newroute_get_src_ipif_v6(
    ipif_t *dst_ipif,
    boolean_t islocal,
    const in6_addr_t *src_addrp)
{
	ipif_t *src_ipif;

	if (ip_enable_group_ifs == 0 || dst_ipif->ipif_ifgrpnext == dst_ipif)
		return (dst_ipif);
	if (!IN6_IS_ADDR_UNSPECIFIED(src_addrp) && islocal) {
		/*
		 * We already have a source address, and the packet
		 * originated here.
		 *
		 * Perform the following sets of reality checks:
		 *	- Find an ipif that is up for this source
		 *	  address.
		 *	- If it is the same ipif as for the route,
		 *	  cool, return dst_ipif.
		 *	  (Except when instructed to do otherwise.)
		 *	- If the ipif is not in the same ifgrp as
		 *	  for the route, return dst_ipif
		 *	  because the request source
		 *	  address doesn't seem to even come CLOSE
		 *	  to what routing says.
		 *	- If the ipif is in the same ifgrp but not
		 *	  the same ipif as the ire, set src_ipif to
		 *	  this ipif.  Most likely, this source
		 *	  address was set by bind() in user space or
		 *	  by a call to ifgrp_scheduler() in
		 *	  ip_bind() or ip_ire_req() because of TCP
		 *	  source address selection.
		 *
		 * REMEMBER, if there is no ipif for the source
		 * address, then the packet is bogus.  The islocal
		 * ensures that this is not a forwarded packet.
		 */
		ire_t *src_ire;

		src_ire = ire_ctable_lookup_v6(src_addrp, 0,
		    IRE_LOCAL, NULL, NULL, MATCH_IRE_TYPE);
		if (src_ire == NULL) {
			/*
			 * Locally-originated packet with source
			 * address that's not attached to an up
			 * interface.  Possibly a deliberately forged
			 * IP datagram.
			 */
			ip1dbg(("ip_newroute_get_src_ipif_v6: "
			    "Packet from me with non-up src!\n"));
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_newroute_get_src_ipif_v6: "
				    "Address is %s.\n", AF_INET6, src_addrp);
			}
			src_ipif = ifgrp_scheduler(dst_ipif);
		} else {
			src_ipif = src_ire->ire_ipif;
			ASSERT(src_ipif != NULL);
			ire_refrele(src_ire);
		}

		if (src_ipif == dst_ipif ||
		    !IN6_ARE_ADDR_EQUAL(&src_ipif->ipif_v6net_mask,
		    &dst_ipif->ipif_v6net_mask) ||
		    !IN6_ARE_ADDR_EQUAL(&src_ipif->ipif_v6subnet,
		    &dst_ipif->ipif_v6subnet)) {
			/*
			 * Returning dst_ipif means to just
			 * use the ire obtained by the initial
			 * ire_ftable_lookup_v6.  We do this if the source
			 * address matches the ire's source address,
			 * or if the ire's outbound interface is not
			 * in the same ifgrp as the source address.
			 * (There is a possibility of multiple
			 * prefixes on the same interface, or
			 * interface group, but we punt on that for
			 * now.)
			 *
			 * We perform that last reality check by
			 * checking prefixes.
			 */
			return (dst_ipif);
		}

		/*
		 * If I reach here without explicitly scheduling
		 * src_ipif or returning dst_ipif, then the
		 * source address ipif is in the same interface
		 * group as the ire's ipif, but it is not the
		 * same actual ipif.  So basically fallthrough
		 * with src_ipif set to what the source address
		 * says.  This means the new route will go out
		 * the interface assigned to the (probably
		 * user-specified) source address.  This may
		 * upset the balance.
		 */
	} else {
		/* No specified source address or forwarded packet. */
		src_ipif = ifgrp_scheduler(dst_ipif);
	}

	/*
	 * If the new source ipif isn't the same type as the dest, I can't
	 * send packets out the other interface in the group, because of
	 * potential link-level header differences, and a bunch of other
	 * cruft.
	 */
	if (dst_ipif->ipif_type != src_ipif->ipif_type)
		return (dst_ipif);

	return (src_ipif);
}

/*
 * IPv6 -
 * ip_newroute_v6 is called by ip_rput_data_v6 or ip_wput_v6 whenever we need
 * to send out a packet to a destination address for which we do not have
 * specific routing information.
 *
 * Handle non-multicast packets. If ill is non-NULL the match is done
 * for that ill.
 *
 * When a specific ill is specified (using IPV6_PKTINFO,
 * IPV6_MULTICAST_IF, or IPV6_BOUND_IF) we will only match
 * on routing entries (ftable and ctable) that have a matching
 * ire->ire_ipif->ipif_ill. Thus this can only be used
 * for destinations that are on-link for the specific ill
 * and that can appear on multiple links. Thus it is useful
 * for multicast destinations, link-local destinations, and
 * at some point perhaps for site-local destinations (if the
 * node sits at a site boundary).
 * We create the cache entries in the regular ctable since
 * it can not "confuse" things for other destinations.
 * table.
 *
 * NOTE : These are the scopes of some of the variables that point at IRE,
 *	  which needs to be followed while making any future modifications
 *	  to avoid memory leaks.
 *
 *	- ire and sire are the entries looked up initially by
 *	  ire_ftable_lookup_v6.
 *	- ipif_ire is used to hold the interface ire associated with
 *	  the new cache ire. But it's scope is limited, so we always REFRELE
 *	  it before branching out to error paths.
 *	- save_ire is initialized before ire_create, so that ire returned
 *	  by ire_create will not over-write the ire. We REFRELE save_ire
 *	  before breaking out of the switch.
 *
 *	Thus on failures, we have to REFRELE only ire and sire, if they
 *	are not NULL.
 */
void
ip_newroute_v6(queue_t *q, mblk_t *mp, const in6_addr_t *v6dstp,
    const in6_addr_t *v6srcp, ill_t *ill)
{
	in6_addr_t	v6gw;
	in6_addr_t	dst;
	ire_t		*ire;
	queue_t		*stq;
	ipif_t		*src_ipif;
	ire_t		*sire = NULL;
	ire_t		*save_ire;
	mblk_t		*dlureq_mp;
	ip6_t		*ip6h;
	int		err = 0;
	boolean_t	natural_if_route = B_FALSE;

	ASSERT(!IN6_IS_ADDR_MULTICAST(v6dstp));

	/*
	 * Get what we can from ire_ftable_lookup_v6 which will follow an IRE
	 * chain until it gets the most specific information available.
	 * For example, we know that there is no IRE_CACHE for this dest,
	 * but there may be an IRE_OFFSUBNET which specifies a gateway.
	 * ire_ftable_lookup_v6 will look up the gateway, etc.
	 */
	if (ill == NULL) {
		ire = ire_ftable_lookup_v6(v6dstp, 0, 0, 0, NULL, &sire, NULL,
		    0, (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT |
		    MATCH_IRE_RJ_BHOLE));
	} else {
		ire = ire_ftable_lookup_v6(v6dstp, 0, 0, 0, ill->ill_ipif,
		    &sire, NULL, 0,
		    (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT |
		    MATCH_IRE_RJ_BHOLE | MATCH_IRE_ILL));
	}
	if (ire == NULL) {
		ip_rts_change_v6(RTM_MISS, v6dstp, 0, 0, 0, 0, 0, 0, RTA_DST);
		goto icmp_err_ret;
	}

	ASSERT(ire->ire_ipversion == IPV6_VERSION);

	/*
	 * Verify that the returned IRE does not have either the RTF_REJECT or
	 * RTF_BLACKHOLE flags set and that the IRE is either an IRE_CACHE,
	 * IRE_IF_NORESOLVER or IRE_IF_RESOLVER.
	 */
	if ((ire->ire_flags & (RTF_REJECT | RTF_BLACKHOLE)) ||
	    (ire->ire_type & (IRE_CACHE | IRE_INTERFACE)) == 0)
		goto icmp_err_ret;

	/*
	 * Increment the ire_ob_pkt_count field for ire if it is an INTERFACE
	 * (IF_RESOLVER or IF_NORESOLVER) IRE type, and increment the same for
	 * the parent IRE, sire, if it is some sort of prefix IRE (which
	 * includes DEFAULT, PREFIX, HOST and HOST_REDIRECT).
	 */
	if ((ire->ire_type & IRE_INTERFACE) != 0)
		ire->ire_ob_pkt_count++;

	if (sire != NULL) {
		mutex_enter(&sire->ire_lock);
		v6gw = sire->ire_gateway_addr_v6;
		mutex_exit(&sire->ire_lock);
		ASSERT((sire->ire_type & (IRE_CACHETABLE |
		    IRE_INTERFACE)) == 0);
		sire->ire_ob_pkt_count++;
	} else {
		v6gw = ipv6_all_zeros;
	}
	if (ire->ire_type & IRE_INTERFACE) {
		ire_t 	*ipif_ire;

		ipif_ire = ipif_to_ire_v6(ire->ire_ipif);
		if (ipif_ire != NULL) {
			if (ipif_ire == ire) {
				/*
				 * 'ire' is the natural inteface route
				 * associated with ire->ire_ipif. Record it.
				 */
				natural_if_route = B_TRUE;
			}
			ire_refrele(ipif_ire);
		}
	}
	/*
	 * If the application specified the ill (ifindex) we don't
	 * try to use a different ill for load balancing!
	 */
	if (ill == NULL &&
	    (ire->ire_type == IRE_CACHE || natural_if_route)) {
		/*
		 * If the interface belongs to an interface group, make sure
		 * the next possible interface in the group is used.  This
		 * encourages load-balancing among peers in an interface group.
		 * Furthermore if a user has previously bound to a source
		 * address, try and use that interface if it makes good
		 * routing sense.
		 *
		 * Interface scheduling is only done in the IRE_CACHE case
		 * (where the IRE_CACHE is for the next-hop of the destination
		 * being looked up) or in the IRE_INTERFACE case when the IRE
		 * represents the interface route for ire->ire_ipif itself.  In
		 * general for manually added interface routes, the latter case
		 * does not hold true so interface scheduling is not done.
		 * This means that manually added interface routes can
		 * be used to selectively disable ifgrp scheduling.
		 *
		 * Note: While we pick a src_ipif we are really only interested
		 * in the ill for load balancing. The source ipif is
		 * determined by source address selection below.
		 *
		 * Remember kids, mp->b_prev is the indicator of local
		 * origination!
		 */
		src_ipif = ip_newroute_get_src_ipif_v6(ire->ire_ipif,
		    (mp->b_prev == NULL), v6srcp);
	} else {
		src_ipif = ire->ire_ipif;
		if (ill != NULL && src_ipif->ipif_ill != ill) {
			/* Can't honor IPV6_BOUND_IF - drop */
			ip1dbg(("ip_newroute_v6: BOUND_IF failed ill %s "
			    "ire->ipif->ill %s\n",
			    ill->ill_name,
			    ire->ire_ipif->ipif_ill->ill_name));
			goto icmp_err_ret;
		}
	}
	/*
	 * Pick a source address which matches the scope
	 * of the destination address.
	 */
	src_ipif = ipif_select_source_v6(src_ipif->ipif_ill, v6dstp);
	if (src_ipif == NULL) {
		if (ip_debug > 2) {
			/* ip1dbg */
			pr_addr_dbg("ip_newroute_v6: no src for dst %s\n, ",
			    AF_INET6, v6dstp);
			printf("ip_newroute_v6: interface name %s\n",
			    ire->ire_ipif->ipif_ill->ill_name);
		}
		goto icmp_err_ret;
	}
	if (natural_if_route && (src_ipif != ire->ire_ipif)) {
		/*
		 * If 'ire' is a natural interface ire, and if we have
		 * chosen a new src_ipif, replace 'ire' with the natural
		 * interface ire associated with the new src_ipif, if it
		 * exists.
		 */
		ire_t	*ipif_ire;

		ipif_ire = ipif_to_ire_v6(src_ipif);
		if (ipif_ire != NULL) {
			ire_refrele(ire);
			ire = ipif_ire;
		}
	}
	/*
	 * If ire_ftable_lookup_v6() returned an interface ire, then at this
	 * point, 'ire' holds the right interface ire that will be tied
	 * to the new cache ire via the ihandle
	 */
	if (ip_debug > 3) {
		/* ip2dbg */
		pr_addr_dbg("ip_newroute_v6: first hop %s\n",
		    AF_INET6, &v6gw);
	}
	ip2dbg(("\tire type %s (%d)\n",
	    ip_nv_lookup(ire_nv_tbl, ire->ire_type), ire->ire_type));

	stq = ire->ire_stq;

	/*
	 * At this point in ip_newroute_v6(), ire is either the IRE_CACHE of the
	 * next-hop gateway for an off-subnet destination or an IRE_INTERFACE
	 * type that should be used to resolve an on-subnet destination or
	 * an on-subnet next-hop gateway.
	 *
	 * In the IRE_CACHE case, src_ipif will point to the outgoing ipif to be
	 * used for this destination (as returned by ire->ire_ipif and possibly
	 * modified by interface group scheduling).  The IRE sire will point to
	 * the prefix that is the longest matching route for the destination.
	 * These prefix types include IRE_DEFAULT, IRE_PREFIX, IRE_HOST, and
	 * IRE_HOST_REDIRECT.  The newly created IRE_CACHE entry for the
	 * off-subnet destination is tied to both the prefix route and the
	 * interface route used to resolve the next-hop gateway via the
	 * ire_phandle and ire_ihandle fields, respectively.
	 *
	 * In the IRE_INTERFACE case, sire may or may not be NULL but the
	 * IRE_CACHE that is to be created will only be tied to the
	 * IRE_INTERFACE it was derived from via the ire_ihandle field.
	 * The IRE_INTERFACE used may vary if interface groups are enabled and
	 * if this IRE represents the interface itself (namely, if
	 * ipif_to_ire(ire->ipif) is the same as ire).
	 */
	save_ire = ire;
	switch (ire->ire_type) {
	case IRE_CACHE: {
		ire_t	*ipif_ire;

		ASSERT(sire != NULL);
		if (IN6_IS_ADDR_UNSPECIFIED(&v6gw)) {
			mutex_enter(&ire->ire_lock);
			v6gw = ire->ire_gateway_addr_v6;
			mutex_exit(&ire->ire_lock);
		}
		/*
		 * We need 3 ire's to create a new cache ire for an off-link
		 * destn. from the cache ire of the gateway.
		 *	1. The prefix ire 'sire'
		 *	2. The interface ire 'ipif_ire'
		 *	3. The cache ire of the gateway 'ire'
		 * The cache ire that is to be created needs be tied to
		 * an appropriate interface ire via the ihandle. The algorithm
		 * employed below is as follows.
		 * 1. Use the natural interface ire associated with the new
		 *    src_ipif, if it exists.
		 * 2. Otherwise use the natural interface ire associated with
		 *    ire->ire_ipif if it exists.
		 * 3. Otherwise use the interface ire, corresponding to
		 *    ire->ire_ihandle, if it exists. We can hit this case if
		 *    someone has manually deleted the natural interface route,
		 *    and added a host-specific interface route to the
		 *    gateway.
		 * 4. Otherwise, there is no interface route to the gateway.
		 *    This is a race condition, where we found the cache
		 *    but the inteface route has been deleted.
		 */
		ipif_ire = ipif_to_ire_v6(src_ipif);
		if (ipif_ire == NULL) {
			ipif_ire = ipif_to_ire_v6(ire->ire_ipif);
			if (ipif_ire == NULL) {
				ipif_ire =
				    ire_ihandle_lookup_offlink_v6(ire, sire);
				if (ipif_ire == NULL) {
					ip0dbg(("ip_newroute_v6: "
					    "ire_ihandle_lookup_offlink_v6 "
					    "failed\n"));
					goto icmp_err_ret;
				}
			}
		}
		/*
		 * Assume DL_UNITDATA_REQ is same for all physical interfaces
		 * in the ifgrp.  If it isn't, this code will
		 * have to be seriously rewhacked to allow the
		 * fastpath probing (such that I cache the link
		 * header in the IRE_CACHE) to work over ifgrps.
		 * We have what we need to build an IRE_CACHE.
		 */
		ire = ire_create_v6(
			v6dstp,				/* dest address */
			&ipv6_all_ones,			/* mask */
			&src_ipif->ipif_v6src_addr,	/* source address */
			&v6gw,				/* gateway address */
			save_ire->ire_max_frag,
			NULL,				/* Fast Path header */
			src_ipif->ipif_rq,		/* recv-from queue */
			src_ipif->ipif_wq,		/* send-to queue */
			IRE_CACHE,
			NULL,
			src_ipif,
			&sire->ire_mask_v6,		/* Parent mask */
			sire->ire_phandle,		/* Parent handle */
			ipif_ire->ire_ihandle,		/* Interface handle */
			0,				/* flags if any */
			&(sire->ire_uinfo));

		ire_refrele(save_ire);
		if (ire == NULL) {
			ire_refrele(ipif_ire);
			break;
		}
		/*
		 * Prevent sire and ipif_ire from getting deleted. The
		 * newly created ire is tied to both of them via the phandle
		 * and ihandle respectively.
		 */
		IRB_REFHOLD(sire->ire_bucket);
		/* Has it been removed already ? */
		if (sire->ire_marks & IRE_MARK_CONDEMNED) {
			IRB_REFRELE(sire->ire_bucket);
			ire_refrele(ipif_ire);
			break;
		}

		IRB_REFHOLD(ipif_ire->ire_bucket);
		/* Has it been removed already ? */
		if (ipif_ire->ire_marks & IRE_MARK_CONDEMNED) {
			IRB_REFRELE(ipif_ire->ire_bucket);
			IRB_REFRELE(sire->ire_bucket);
			ire_refrele(ipif_ire);
			break;
		}

		/* Remember the packet we want to xmit */
		ire->ire_mp->b_cont = mp;
		ire_add_then_send(q, ire->ire_mp);

		/* Assert that sire is not deleted yet. */
		ASSERT(sire->ire_ptpn != NULL);
		IRB_REFRELE(sire->ire_bucket);
		ire_refrele(sire);

		/* Assert that ipif_ire is not deleted yet. */
		ASSERT(ipif_ire->ire_ptpn != NULL);
		IRB_REFRELE(ipif_ire->ire_bucket);
		ire_refrele(ipif_ire);
		return;
	}
	case IRE_IF_NORESOLVER:
		/* We have what we need to build an IRE_CACHE. */

		/*
		 * Create a new dlureq_mp with the
		 * IPv6 gateway address in destination address in the DLPI hdr
		 * if the physical length is exactly 16 bytes.
		 */
		ill = ire_to_ill(ire);
		if (!ill) {
			ip0dbg(("ip_newroute_v6: ire_to_ill failed\n"));
			break;
		}
		if (ill->ill_phys_addr_length == IPV6_ADDR_LEN) {
			const in6_addr_t *addr;

			if (!IN6_IS_ADDR_UNSPECIFIED(&v6gw))
				addr = &v6gw;
			else
				addr = v6dstp;

			dlureq_mp = ill_dlur_gen((uchar_t *)addr,
			    ill->ill_phys_addr_length, ill->ill_sap,
			    ill->ill_sap_length);
		} else {
			dlureq_mp = ire->ire_dlureq_mp;
		}
		if (dlureq_mp == NULL)
			break;

		ire = ire_create_v6(
			v6dstp,				/* dest address */
			&ipv6_all_ones,			/* mask */
			&src_ipif->ipif_v6src_addr,	/* source address */
			&v6gw,				/* gateway address */
			save_ire->ire_max_frag,
			NULL,				/* Fast Path header */
			save_ire->ire_rfq,		/* recv-from queue */
			stq,				/* send-to queue */
			IRE_CACHE,
			dlureq_mp,
			save_ire->ire_ipif,
			&save_ire->ire_mask_v6,		/* Parent mask */
			0,
			save_ire->ire_ihandle,		/* Interface handle */
			0,				/* flags if any */
			&(save_ire->ire_uinfo));

		if (ill->ill_phys_addr_length == IPV6_ADDR_LEN)
			freeb(dlureq_mp);

		if (ire == NULL) {
			ire_refrele(save_ire);
			break;
		}

		if (!IN6_IS_ADDR_UNSPECIFIED(&v6gw))
			dst = v6gw;
		else
			dst = *v6dstp;
		err = ndp_noresolver(ire->ire_ipif, &dst);
		if (err != 0) {
			ire_refrele(save_ire);
			break;
		}

		/* Prevent save_ire from getting deleted */
		IRB_REFHOLD(save_ire->ire_bucket);
		/* Has it been removed already ? */
		if (save_ire->ire_marks & IRE_MARK_CONDEMNED) {
			IRB_REFRELE(save_ire->ire_bucket);
			ire_refrele(save_ire);
			break;
		}

		/* Don't need sire anymore */
		if (sire != NULL)
			ire_refrele(sire);

		/* Remember the packet we want to xmit */
		ire->ire_mp->b_cont = mp;
		ire_add_then_send(ire->ire_stq, ire->ire_mp);

		/* Assert that it is not deleted yet. */
		ASSERT(save_ire->ire_ptpn != NULL);
		IRB_REFRELE(save_ire->ire_bucket);
		ire_refrele(save_ire);
		return;

	case IRE_IF_RESOLVER:
		/*
		 * We can't build an IRE_CACHE yet, but at least we found a
		 * resolver that can help.
		 */
		dst = *v6dstp;
		if (!stq)
			break;
		/*
		 * To be at this point in the code with a non-zero gw means
		 * that dst is reachable through a gateway that we have never
		 * resolved.  By changing dst to the gw addr we resolve the
		 * gateway first.  When ire_add_then_send() tries to put the IP
		 * dg to dst, it will reenter ip_newroute() at which time we
		 * will find the IRE_CACHE for the gw and create another
		 * IRE_CACHE above (for dst itself).
		 */
		if (!IN6_IS_ADDR_UNSPECIFIED(&v6gw)) {
			dst = v6gw;
			v6gw = ipv6_all_zeros;
		}
		ire = ire_create_v6(
			&dst,				/* dest address */
			&ipv6_all_ones,			/* mask */
			&src_ipif->ipif_v6src_addr,	/* source address */
			&v6gw,				/* gateway address */
			save_ire->ire_max_frag,
			NULL,				/* Fast Path header */
			stq,				/* recv-from queue */
			OTHERQ(stq),			/* send-to queue */
			IRE_CACHE,
			NULL,
			save_ire->ire_ipif,
			&save_ire->ire_mask_v6,		/* Parent mask */
			0,
			save_ire->ire_ihandle,		/* Interface handle */
			0,				/* flags if any */
			&(save_ire->ire_uinfo));

		if (ire == NULL) {
			ire_refrele(save_ire);
			break;
		}

		ill = ire_to_ill(ire);
		if (ill == NULL) {
			ip0dbg(("ip_newroute_v6: ire_to_ill failed\n"));
			ire_refrele(save_ire);
			break;
		}
		err = ndp_resolver(ire->ire_ipif, &dst, mp);
		switch (err) {
		case 0:
			/* Prevent save_ire from getting deleted */
			IRB_REFHOLD(save_ire->ire_bucket);
			/* Has it been removed already ? */
			if (save_ire->ire_marks & IRE_MARK_CONDEMNED) {
				IRB_REFRELE(save_ire->ire_bucket);
				ire_refrele(save_ire);
				break;
			}
			if (sire != NULL)
				ire_refrele(sire);
			/* Remember the packet we want to xmit */
			ire->ire_mp->b_cont = mp;
			/*
			 * We have a resolved cache entry, add in the IRE.
			 */
			ire_add_then_send(q, ire->ire_mp);

			/* Assert that it is not deleted yet. */
			ASSERT(save_ire->ire_ptpn != NULL);
			IRB_REFRELE(save_ire->ire_bucket);
			ire_refrele(save_ire);
			return;

		case EINPROGRESS:
			/*
			 * mp was consumed - presumably queued.
			 * No need for ire, presumably resolution is
			 * in progress, and ire will be added when the
			 * address is resolved.
			 */
			ASSERT(ire->ire_nce == NULL);
			ire_delete(ire);
			if (sire != NULL)
				ire_refrele(sire);
			ire_refrele(save_ire);
			return;
		default:
			/* Some transient error */
			ASSERT(ire->ire_nce == NULL);
			ire_refrele(save_ire);
			break;
		}
		break;
	default:
		break;
	}

err_ret:
	ip1dbg(("ip_newroute_v6: dropped\n"));
	if (ill != NULL) {
		if (mp->b_prev != NULL)
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
		else
			BUMP_MIB(ill->ill_ip6_mib->ipv6OutDiscards);
	} else {
		if (mp->b_prev != NULL)
			BUMP_MIB(ip6_mib.ipv6InDiscards);
		else
			BUMP_MIB(ip6_mib.ipv6OutDiscards);
	}
	/* Did this packet originate externally? */
	if (mp->b_prev) {
		mp->b_next = NULL;
		mp->b_prev = NULL;
	}
	freemsg(mp);
	if (ire != NULL)
		ire_refrele(ire);
	if (sire != NULL)
		ire_refrele(sire);
	return;

icmp_err_ret:
	ip1dbg(("ip_newroute_v6: no route\n"));
	if (sire != NULL)
		ire_refrele(sire);
	/*
	 * We need to set sire to NULL to avoid double freeing if we
	 * ever goto err_ret from below.
	 */
	sire = NULL;
	ip6h = (ip6_t *)mp->b_rptr;
	/* Skip ip6i_t header if present */
	if (ip6h->ip6_nxt == IPPROTO_RAW) {
		/* Make sure the IPv6 header is present */
		if ((mp->b_wptr - (uchar_t *)ip6h) <
		    sizeof (ip6i_t) + IPV6_HDR_LEN) {
			if (!pullupmsg(mp, sizeof (ip6i_t) + IPV6_HDR_LEN)) {
				ip1dbg(("ip_newroute_v6: pullupmsg failed\n"));
				goto err_ret;
			}
		}
		mp->b_rptr += sizeof (ip6i_t);
		ip6h = (ip6_t *)mp->b_rptr;
	}
	/* Did this packet originate externally? */
	if (mp->b_prev) {
		if (ill != NULL)
			BUMP_MIB(ill->ill_ip6_mib->ipv6InNoRoutes);
		else
			BUMP_MIB(ip6_mib.ipv6InNoRoutes);
		mp->b_next = NULL;
		mp->b_prev = NULL;
		q = WR(q);
	} else {
		if (ill != NULL)
			BUMP_MIB(ill->ill_ip6_mib->ipv6OutNoRoutes);
		else
			BUMP_MIB(ip6_mib.ipv6OutNoRoutes);
		if (ip_hdr_complete_v6(ip6h)) {
			/* Failed */
			freemsg(mp);
			if (ire != NULL)
				ire_refrele(ire);
			return;
		}
	}

	/*
	 * At this point we will have ire only if RTF_BLACKHOLE
	 * or RTF_REJECT flags are set on the IRE. It will not
	 * generate ICMP6_DST_UNREACH_NOROUTE if RTF_BLACKHOLE is set.
	 */
	if (ire != NULL) {
		if (ire->ire_flags & RTF_BLACKHOLE) {
			ire_refrele(ire);
			freemsg(mp);
			return;
		}
		ire_refrele(ire);
	}
	icmp_unreachable_v6(WR(q), mp, ICMP6_DST_UNREACH_NOROUTE,
	    B_FALSE, B_FALSE);
	if (ip_debug > 3) {
		/* ip2dbg */
		pr_addr_dbg("ip_newroute_v6: no route to %s\n",
		    AF_INET6, v6dstp);
	}
}

/*
 * ip_newroute_multi_v6 is called by ip_wput_v6 and
 * ip_rput_forward_multicast whenever we need to send
 * out a packet to a destination address for which we do not have specific
 * routing information. It is only used for multicast packets.
 *
 * If unspec_src we allow creating an IRE with source address zero.
 * ire_send_v6() will delete it after the packet is sent.
 */
void
ip_newroute_multi_v6(queue_t *q, mblk_t *mp, ipif_t *ipif,
    const in6_addr_t *v6dstp, int unspec_src)
{
	ire_t	*ire = NULL;
	queue_t	*stq;
	ipif_t	*src_ipif;
	ill_t	*ill;
	int	err = 0;
	ire_t	*save_ire;

	ASSERT(!IN6_IS_ADDR_V4MAPPED(v6dstp));
	if (ip_debug > 2) {
		/* ip1dbg */
		pr_addr_dbg("ip_newroute_multi_v6: v6dst %s\n, ",
		    AF_INET6, v6dstp);
		printf("ip_newroute_multi_v6: if %s, v6 %d\n",
		    ipif->ipif_ill->ill_name, ipif->ipif_isv6);
	}

	/*
	 * If the interface is a pt-pt interface we look for an IRE_IF_RESOLVER
	 * or IRE_IF_NORESOLVER that matches both the local_address and the
	 * pt-pt destination address. Otherwise we just match the
	 * local address.
	 */
	if (!(ipif->ipif_flags & IFF_MULTICAST)) {
		goto err_ret;
	}

	/*
	 * Pick a source address which matches the scope
	 * of the destination address.
	 */
	src_ipif = ipif_select_source_v6(ipif->ipif_ill, v6dstp);
	if (src_ipif == NULL) {
		if (!unspec_src) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_newroute_multi_v6: no src for"
				    " dst %s\n, ", AF_INET6, v6dstp);
				printf("ip_newroute_multi_v6: if %s\n",
				    ipif->ipif_ill->ill_name);
			}
			goto err_ret;
		}
		/* Use any ipif for source */
		for (src_ipif = ipif->ipif_ill->ill_ipif; src_ipif != NULL;
		    src_ipif = src_ipif->ipif_next) {
			if ((src_ipif->ipif_flags & IFF_UP) &&
			    IN6_IS_ADDR_UNSPECIFIED(&src_ipif->ipif_v6src_addr))
				break;
		}
		if (src_ipif == NULL) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_newroute_multi_v6: no src for"
				    " dst %s\n ", AF_INET6, v6dstp);
				printf("ip_newroute_multi_v6: if %s"
				    "(UNSPEC_SRC)\n",
				    ipif->ipif_ill->ill_name);
			}
			goto err_ret;
		}
		src_ipif = ipif;
	} else {
		ipif = src_ipif;
	}
	ire = ipif_to_ire_v6(ipif);
	if (ire == NULL) {
		if (ip_debug > 2) {
			/* ip1dbg */
			pr_addr_dbg("ip_newroute_multi_v6: v6src %s\n",
			    AF_INET6, &ipif->ipif_v6lcl_addr);
			printf("ip_newroute_multi_v6: "
			    "if %s\n", ipif->ipif_ill->ill_name);
		}
		goto err_ret;
	}
	if (ire->ire_flags & (RTF_REJECT | RTF_BLACKHOLE))
		goto err_ret;

	ASSERT(ire->ire_ipversion == IPV6_VERSION);

	ip1dbg(("ip_newroute_multi_v6: interface type %s (%d),",
	    ip_nv_lookup(ire_nv_tbl, ire->ire_type), ire->ire_type));
	if (ip_debug > 2) {
		/* ip1dbg */
		pr_addr_dbg(" address %s\n", AF_INET6, &ire->ire_src_addr_v6);
	}
	stq = ire->ire_stq;
	ill = ire_to_ill(ire);
	if (!ill) {
		ip0dbg(("ip_newroute_multi_v6: ire_to_ill failed\n"));
		goto err_ret;
	}
	save_ire = ire;
	switch (ire->ire_type) {
	case IRE_IF_NORESOLVER: {
		/* We have what we need to build an IRE_CACHE. */
		mblk_t	*dlureq_mp;

		/*
		 * Create a new dlureq_mp with the
		 * IPv6 gateway address in destination address in the DLPI hdr
		 * if the physical length is exactly 16 bytes.
		 */
		ASSERT(ill->ill_isv6);
		if (ill->ill_phys_addr_length == IPV6_ADDR_LEN) {
			dlureq_mp = ill_dlur_gen((uchar_t *)v6dstp,
			    ill->ill_phys_addr_length, ill->ill_sap,
			    ill->ill_sap_length);
		} else {
			dlureq_mp = ire->ire_dlureq_mp;
		}

		if (dlureq_mp == NULL)
			break;
		ire = ire_create_v6(
			v6dstp,				/* dest address */
			&ipv6_all_ones,			/* mask */
			&ipif->ipif_v6src_addr,		/* source address */
			NULL,				/* gateway address */
			save_ire->ire_max_frag,
			NULL,				/* Fast Path header */
			save_ire->ire_rfq,		/* recv-from queue */
			stq,				/* send-to queue */
			IRE_CACHE,
			dlureq_mp,
			save_ire->ire_ipif,
			NULL,
			0,
			save_ire->ire_ihandle,		/* Interface handle */
			0,				/* flags if any */
			&ire_uinfo_null);

		if (ill->ill_phys_addr_length == IPV6_ADDR_LEN)
			freeb(dlureq_mp);

		if (ire == NULL) {
			ire_refrele(save_ire);
			break;
		}

		err = ndp_noresolver(ire->ire_ipif, v6dstp);
		if (err != 0) {
			ire_refrele(save_ire);
			break;
		}

		/* Prevent save_ire from getting deleted */
		IRB_REFHOLD(save_ire->ire_bucket);
		/* Has it been removed already ? */
		if (save_ire->ire_marks & IRE_MARK_CONDEMNED) {
			IRB_REFRELE(save_ire->ire_bucket);
			ire_refrele(save_ire);
			break;
		}

		/* Remember the packet we want to xmit */
		ire->ire_mp->b_cont = mp;
		ire_add_then_send(ire->ire_stq, ire->ire_mp);

		/* Assert that it is not deleted yet. */
		ASSERT(save_ire->ire_ptpn != NULL);
		IRB_REFRELE(save_ire->ire_bucket);
		ire_refrele(save_ire);
		return;
	}
	case IRE_IF_RESOLVER: {

		ASSERT(ill->ill_isv6);

		/*
		 * NOTE: a resolvers rfq is NULL and its stq points upstream.
		 *
		 * We obtain a partial IRE_CACHE which we will pass along
		 * with the resolver query.  When the response comes back it
		 * will be there ready for us to add.
		 */
		ire = ire_create_v6(
			v6dstp,				/* dest address */
			&ipv6_all_ones,			/* mask */
			&ipif->ipif_v6src_addr,		/* source address */
			NULL,				/* gateway address */
			save_ire->ire_max_frag,
			NULL,				/* Fast Path header */
			stq,				/* recv-from queue */
			OTHERQ(stq),			/* send-to queue */
			IRE_CACHE,
			NULL,
			save_ire->ire_ipif,
			NULL,
			0,
			save_ire->ire_ihandle,		/* Interface handle */
			0,				/* flags if any */
			&ire_uinfo_null);

		if (ire == NULL) {
			ire_refrele(save_ire);
			break;
		}

		/* Resolve and add ire to the ctable */
		err = ndp_resolver(ire->ire_ipif, v6dstp, mp);
		switch (err) {
		case 0:
			/* Prevent save_ire from getting deleted */
			IRB_REFHOLD(save_ire->ire_bucket);
			/* Has it been removed already ? */
			if (save_ire->ire_marks & IRE_MARK_CONDEMNED) {
				IRB_REFRELE(save_ire->ire_bucket);
				ire_refrele(save_ire);
				break;
			}
			/* Remember the packet we want to xmit */
			ire->ire_mp->b_cont = mp;
			/*
			 * We have a resolved cache entry, add in the IRE.
			 */
			ire_add_then_send(q, ire->ire_mp);

			/* Assert that it is not deleted yet. */
			ASSERT(save_ire->ire_ptpn != NULL);
			IRB_REFRELE(save_ire->ire_bucket);
			ire_refrele(save_ire);
			return;

		case EINPROGRESS:
			/*
			 * mp was consumed - presumably queued.
			 * No need for ire, presumably resolution is
			 * in progress, and ire will be added when the
			 * address is resolved.
			 */
			ire_delete(ire);
			ire_refrele(save_ire);
			return;
		default:
			/* Some transient error */
			ire_refrele(save_ire);
			break;
		}
		break;
	}
	default:
		break;
	}

err_ret:
	if (ire != NULL)
		ire_refrele(ire);
	/* Multicast - no point in trying to generate ICMP error */
	ill = ipif->ipif_ill;
	if (mp->b_prev || mp->b_next)
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
	else
		BUMP_MIB(ill->ill_ip6_mib->ipv6OutDiscards);
	ip1dbg(("ip_newroute_multi_v6: dropped\n"));
	mp->b_next = NULL;
	mp->b_prev = NULL;
	freemsg(mp);
}

/*
 * Parse and process any hop-by-hop or destination options.
 *
 * Assumes that q is an ill read queue so that ICMP errors for link-local
 * destinations are sent out the correct interface.
 *
 * Returns -1 if there was an error and mp has been consumed.
 * Returns 0 if no special action is needed.
 * Returns 1 if the packet contained a router alert option for this node
 * which is verified to be "interesting/known" for our implementation.
 *
 * XXX Note: In future as more hbh or dest options are defined,
 * it may be better to have different routines for hbh and dest
 * options as opt_type fields other than IP6OPT_PAD1 and IP6OPT_PADN
 * may have same value in different namespaces. Or is it same namespace ??
 * Current code checks for each opt_type (other than pads) if it is in
 * the expected  nexthdr (hbh or dest)
 */
static int
ip_process_options_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h, uint8_t *optptr,
    uint_t optlen, uint8_t hdr_type)
{
	uint8_t opt_type;
	uint_t optused;
	int ret = 0;

	while (optlen != 0) {
		opt_type = *optptr;
		if (opt_type == IP6OPT_PAD1)
			optused = 1;
		else {
			if (optlen < 2)
				goto bad_opt;
			switch (opt_type) {
			case IP6OPT_PADN:
				/*
				 * Note:We don't verify that (N-2) pad octets
				 * are zero as required by spec. Adhere to
				 * "be liberal in what you accept..." part of
				 * implementation philosophy (RFC791,RFC1122)
				 */
				optused = 2 + optptr[1];
				if (optused > optlen)
					goto bad_opt;
				break;

			case IP6OPT_JUMBO:
				if (hdr_type != IPPROTO_HOPOPTS)
					goto opt_error;
				goto opt_error; /* XXX Not implemented! */

			case IP6OPT_ROUTER_ALERT: {
				struct ip6_opt_router *or;

				if (hdr_type != IPPROTO_HOPOPTS)
					goto opt_error;
				optused = 2 + optptr[1];
				if (optused > optlen)
					goto bad_opt;
				or = (struct ip6_opt_router *)optptr;
				/* Check total length and alignment */
				if (optused != sizeof (*or) ||
				    ((uintptr_t)or->ip6or_value & 0x1) != 0)
					goto opt_error;
				/* Check value */
				switch (*((uint16_t *)or->ip6or_value)) {
				case IP6_ALERT_MLD:
				case IP6_ALERT_RSVP:
					ret = 1;
				}
				break;
			case IP6OPT_HOME_ADDRESS: {
				/*
				 * Minimal support for the home address option
				 * (which is required by all IPv6 nodes).
				 * Implement by just swapping the home address
				 * and source address.
				 * XXX Note: this has IPsec implications since
				 * AH needs to take this into account.
				 * Also, when IPsec is used we need to ensure
				 * that this is only processed once
				 * in the received packet (to avoid swapping
				 * back and forth).
				 */
				struct ip6_opt_home_address *oh;
				in6_addr_t tmp;

				if (hdr_type != IPPROTO_DSTOPTS)
					goto opt_error;
				optused = 2 + optptr[1];
				if (optused > optlen)
					goto bad_opt;
				oh = (struct ip6_opt_home_address *)optptr;
				/* Check total length and alignment */
				if (optused < sizeof (*oh) ||
				    ((uintptr_t)oh->ip6oh_addr & 0x7) != 0)
					goto opt_error;
				/* Swap ip6_src and the home address */
				tmp = ip6h->ip6_src;
				/* XXX Note: only 8 byte alignment option */
				ip6h->ip6_src = *(in6_addr_t *)oh->ip6oh_addr;
				*(in6_addr_t *)oh->ip6oh_addr = tmp;
				break;
			}
			}
			default:
			opt_error:
				ip0dbg(("ip_option_process: bad opt 0x%x\n",
				    opt_type));
				switch (IP6OPT_TYPE(opt_type)) {
				case IP6OPT_TYPE_SKIP:
					optused = 2 + optptr[1];
					if (optused > optlen)
						goto bad_opt;
					break;
				case IP6OPT_TYPE_DISCARD:
					freemsg(mp);
					return (-1);
				case IP6OPT_TYPE_ICMP:
					icmp_param_problem_v6(WR(q), mp,
					    ICMP6_PARAMPROB_OPTION,
					    (uint32_t)(optptr -
					    (uint8_t *)ip6h),
					    B_FALSE, B_FALSE);
					return (-1);
				case IP6OPT_TYPE_FORCEICMP:
					icmp_param_problem_v6(WR(q), mp,
					    ICMP6_PARAMPROB_OPTION,
					    (uint32_t)(optptr -
					    (uint8_t *)ip6h),
					    B_FALSE, B_TRUE);
					return (-1);
				}
			}
		}
		optlen -= optused;
		optptr += optused;
	}
	return (ret);

bad_opt:
	icmp_param_problem_v6(WR(q), mp, ICMP6_PARAMPROB_OPTION,
	    (uint32_t)(optptr - (uint8_t *)ip6h),
	    B_FALSE, B_FALSE);
	return (-1);
}

/*
 * Process a routing header that is not yet empty.
 * Only handles type 0 routing headers.
 */
static void
ip_process_rthdr(queue_t *q, mblk_t *mp, ip6_t *ip6h, ip6_rthdr_t *rth,
    ill_t *ill)
{
	ip6_rthdr0_t *rthdr;
	uint_t ehdrlen;
	uint_t numaddr;
	in6_addr_t *addrptr;
	in6_addr_t tmp;

	ASSERT(rth->ip6r_segleft != 0);

	if (!ipv6_forward || !ipv6_forward_src_routed) {
		/* XXX Check for source routed out same interface? */
		BUMP_MIB(ill->ill_ip6_mib->ipv6ForwProhibits);
		BUMP_MIB(ill->ill_ip6_mib->ipv6InAddrErrors);
		freemsg(mp);
		return;
	}

	if (rth->ip6r_type != 0) {
		icmp_param_problem_v6(WR(q), mp,
		    ICMP6_PARAMPROB_HEADER,
		    (uint32_t)((uchar_t *)&rth->ip6r_type - (uchar_t *)ip6h),
		    B_FALSE, B_FALSE);
		return;
	}
	rthdr = (ip6_rthdr0_t *)rth;
	ehdrlen = 8 * (rthdr->ip6r0_len + 1);
	ASSERT(mp->b_rptr + ehdrlen <= mp->b_wptr);
	addrptr = (in6_addr_t *)&rthdr->ip6r0_addr;
	/* rthdr->ip6r0_len is twice the number of addresses in the header */
	if (rthdr->ip6r0_len & 0x1) {
		/* An odd length is impossible */
		icmp_param_problem_v6(WR(q), mp,
		    ICMP6_PARAMPROB_HEADER,
		    (uint32_t)((uchar_t *)&rthdr->ip6r0_len - (uchar_t *)ip6h),
		    B_FALSE, B_FALSE);
		return;
	}
	numaddr = rthdr->ip6r0_len / 2;
	if (rthdr->ip6r0_segleft > numaddr) {
		/* segleft exceeds number of addresses in routing header */
		icmp_param_problem_v6(WR(q), mp,
		    ICMP6_PARAMPROB_HEADER,
		    (uint32_t)((uchar_t *)&rthdr->ip6r0_segleft -
			(uchar_t *)ip6h),
		    B_FALSE, B_FALSE);
		return;
	}
	addrptr += (numaddr - rthdr->ip6r0_segleft);
	if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst) ||
	    IN6_IS_ADDR_MULTICAST(addrptr)) {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
		freemsg(mp);
		return;
	}
	/* Swap */
	tmp = *addrptr;
	*addrptr = ip6h->ip6_dst;
	ip6h->ip6_dst = tmp;
	rthdr->ip6r0_segleft--;
	/* Don't allow any mapped addresses - ip_wput_v6 can't handle them */
	if (IN6_IS_ADDR_V4MAPPED(&ip6h->ip6_dst)) {
		icmp_unreachable_v6(WR(q), mp, ICMP6_DST_UNREACH_NOROUTE,
		    B_FALSE, B_FALSE);
		return;
	}
	ip_rput_data_v6(q, ill, mp, ip6h, B_FALSE, B_FALSE);
}

/*
 * Read side put procedure for IPv6 module.
 */
void
ip_rput_v6(queue_t *q, mblk_t *mp)
{
	mblk_t		*mp1;
	ip6_t		*ip6h;
	boolean_t	ll_multicast = 0;
	ill_t		*ill;
	struct iocblk	*iocp;

	ill = (ill_t *)q->q_ptr;
	if (ill->ill_ipif == NULL) {
		/*
		 * Things are opening or closing - only accept DLPI
		 * ack messages
		 */
		if (mp->b_datap->db_type != M_PCPROTO) {
			freemsg(mp);
			return;
		}
	}

	switch (mp->b_datap->db_type) {
	case M_DATA:
		break;

	case M_PROTO:
	case M_PCPROTO:
		if (((dl_unitdata_ind_t *)mp->b_rptr)->dl_primitive !=
		    DL_UNITDATA_IND) {
			/* Go handle anything other than data elsewhere. */
			ip_rput_dlpi(q, mp);
			return;
		}
#define	dlur	((dl_unitdata_ind_t *)mp->b_rptr)
		ll_multicast = dlur->dl_group_address;
#undef	dlur
		/* Ditch the DLPI header. */
		mp1 = mp;
		mp = mp->b_cont;
		freeb(mp1);
		break;
	case M_BREAK:
		cmn_err(CE_PANIC, "ip_rput_v6: got an M_BREAK");
		break;
	case M_IOCACK:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case DL_IOC_HDR_INFO:
			ill = (ill_t *)q->q_ptr;
			ill_fastpath_ack(ill, mp);
			return;
		case SIOCSTUNPARAM:
		case SIOCGTUNPARAM:
			/* Go through become_exlusive */
			break;
		default:
			putnext(q, mp);
			return;
		}
		/* FALLTHRU */
	case M_ERROR:
	case M_HANGUP:
		become_exclusive(q, mp, ip_rput_other);
		return;
	case M_CTL: {
		inetcksum_t *ick = (inetcksum_t *)mp->b_rptr;

		if ((mp->b_wptr - mp->b_rptr) == sizeof (*ick) &&
		    ick->ick_magic == ICK_M_CTL_MAGIC) {
			ill = (ill_t *)q->q_ptr;
			ill->ill_ick = *ick;
			freemsg(mp);
			return;
		} else {
			putnext(q, mp);
			return;
		}
	}
	case M_IOCNAK:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case DL_IOC_HDR_INFO:
		case SIOCSTUNPARAM:
		case SIOCGTUNPARAM:
			become_exclusive(q, mp, ip_rput_other);
			return;
		default:
			break;
		}
		/* FALLTHRU */
	default:
		putnext(q, mp);
		return;
	}

	BUMP_MIB(ill->ill_ip6_mib->ipv6InReceives);
	/*
	 * if db_ref > 1 then copymsg and free original. Packet may be
	 * changed and do not want other entity who has a reference to this
	 * message to trip over the changes. This is a blind change because
	 * trying to catch all places that might change packet is too
	 * difficult (since it may be a module above this one).
	 */
	if (mp->b_datap->db_ref > 1) {
		mblk_t  *mp1;

		mp1 = copymsg(mp);
		freemsg(mp);
		if (mp1 == NULL) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			return;
		}
		mp = mp1;
	}
	ip6h = (ip6_t *)mp->b_rptr;

	/* check for alignment and full IPv6 header */
	if (!OK_32PTR((uchar_t *)ip6h) ||
	    (mp->b_wptr - (uchar_t *)ip6h) < IPV6_HDR_LEN) {
		if (!pullupmsg(mp, IPV6_HDR_LEN)) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			ip1dbg(("ip_rput_v6: pullupmsg failed\n"));
			freemsg(mp);
			return;
		}
		ip6h = (ip6_t *)mp->b_rptr;
	}
	if ((ip6h->ip6_vcf & IPV6_VERS_AND_FLOW_MASK) ==
	    IPV6_DEFAULT_VERS_AND_FLOW) {
		/*
		 * It may be a bit too expensive to do this mapped address
		 * check here, but in the interest of robustness, it seems
		 * like the correct place.
		 * TODO: Avoid this check for e.g. connected TCP sockets
		 */
		if (IN6_IS_ADDR_V4MAPPED(&ip6h->ip6_src)) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			ip1dbg(("ip_rput_v6: pkt with mapped src addr\n"));
			freemsg(mp);
			return;
		}
		ip_rput_data_v6(q, ill, mp, ip6h, B_FALSE, ll_multicast);
	} else {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InIPv4);
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
		freemsg(mp);
	}
}

/*
 * ip_rput_data_v6 -- received IPv6 packets in M_DATA messages show up here.
 * ip_rput_v6 has already verified alignment, the min length, the version,
 * and db_ref = 1.
 */
static void
ip_rput_data_v6(queue_t *q, ill_t *ill, mblk_t *mp, ip6_t *ip6h,
    boolean_t no_cksum, boolean_t ll_multicast)
{
	ire_t		*ire = NULL;
	queue_t		*rq;
	uint8_t		*whereptr;
	uint8_t		nexthdr;
	uint16_t	remlen;
	uint_t		prev_nexthdr_offset;
	uint_t		used;
	size_t		pkt_len;
	uint16_t	ip6_len;

	ASSERT(OK_32PTR((uchar_t *)ip6h) &&
	    (mp->b_wptr - (uchar_t *)ip6h) >= IPV6_HDR_LEN);

	if (mp->b_cont == NULL)
		pkt_len = mp->b_wptr - mp->b_rptr;
	else
		pkt_len = msgdsize(mp);
	ip6_len = ntohs(ip6h->ip6_plen) + IPV6_HDR_LEN;

	/*
	 * Check for bogus (too short packet) and packet which
	 * was padded by the link layer.
	 */
	if (ip6_len != pkt_len) {
		ssize_t diff;

		if (ip6_len > pkt_len) {
			ip1dbg(("ip_rput_data_v6: packet too short %d %lu\n",
			    ip6_len, pkt_len));
			BUMP_MIB(ill->ill_ip6_mib->ipv6InTruncatedPkts);
			freemsg(mp);
			return;
		}
		diff = (ssize_t)(pkt_len - ip6_len);

		if (!adjmsg(mp, -diff)) {
			ip1dbg(("ip_rput_data_v6: adjmsg failed\n"));
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			freemsg(mp);
			return;
		}
		pkt_len -= diff;
	}

	/*
	 * XXX When zero-copy support is added, this turning off of ICK_VALID
	 * will need to be done more selectively.
	 */
	mp->b_ick_flag &= ~ICK_VALID;

	nexthdr = ip6h->ip6_nxt;

	prev_nexthdr_offset = (uint_t)((uchar_t *)&ip6h->ip6_nxt -
	    (uchar_t *)ip6h);
	whereptr = (uint8_t *)&ip6h[1];
	remlen = pkt_len - IPV6_HDR_LEN;	/* Track how much is left */

	/* Process hop by hop header options */
	if (nexthdr == IPPROTO_HOPOPTS) {
		ip6_hbh_t *hbhhdr;
		uint_t ehdrlen;
		uint8_t *optptr;

		if (remlen < MIN_EHDR_LEN)
			goto pkt_too_short;
		if (mp->b_cont != NULL &&
		    whereptr + MIN_EHDR_LEN > mp->b_wptr) {
			if (!pullupmsg(mp, IPV6_HDR_LEN + MIN_EHDR_LEN)) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
				freemsg(mp);
				return;
			}
			ip6h = (ip6_t *)mp->b_rptr;
			whereptr = (uint8_t *)ip6h + pkt_len - remlen;
		}
		hbhhdr = (ip6_hbh_t *)whereptr;
		nexthdr = hbhhdr->ip6h_nxt;
		prev_nexthdr_offset = (uint_t)(whereptr - (uint8_t *)ip6h);
		ehdrlen = 8 * (hbhhdr->ip6h_len + 1);

		if (remlen < ehdrlen)
			goto pkt_too_short;
		if (mp->b_cont != NULL &&
		    whereptr + ehdrlen > mp->b_wptr) {
			if (!pullupmsg(mp, IPV6_HDR_LEN + ehdrlen)) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
				freemsg(mp);
				return;
			}
			ip6h = (ip6_t *)mp->b_rptr;
			whereptr = (uint8_t *)ip6h + pkt_len - remlen;
			hbhhdr = (ip6_hbh_t *)whereptr;
		}

		optptr = whereptr + 2;
		whereptr += ehdrlen;
		remlen -= ehdrlen;
		switch (ip_process_options_v6(q, mp, ip6h, optptr,
		    ehdrlen - 2, nexthdr)) {
		case -1:
			/*
			 * Packet has been consumed and any
			 * needed ICMP messages sent.
			 */
			BUMP_MIB(ill->ill_ip6_mib->ipv6InHdrErrors);
			return;
		case 0:
			/* no action needed */
			break;
		case 1:
			/* Known router alert */
			goto ipv6forus;
		}
	}

	/*
	 * On incoming v6 multicast packets we will bypass the ire table,
	 * and assume that the read queue corresponds to the targetted
	 * interface.
	 *
	 * The effect of this is the same as the IPv4 original code, but is
	 * much cleaner I think.  See ip_rput for how that was done.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst)) {

		BUMP_MIB(ill->ill_ip6_mib->ipv6InMcastPkts);
		/*
		 * XXX TODO Give to mrouted to for multicast forwarding.
		 */
		if (ilm_lookup_ill_v6(ill, &ip6h->ip6_dst) == NULL) {
			if (ip_debug > 3) {
				/* ip2dbg */
				pr_addr_dbg("ip_rput_data_v6: got mcast packet"
				    "  which is not for us: %s\n", AF_INET6,
				    &ip6h->ip6_dst);
			}
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			freemsg(mp);
			return;
		}
		if (ip_debug > 3) {
			/* ip2dbg */
			pr_addr_dbg("ip_rput_data_v6: multicast for us: %s\n",
			    AF_INET6, &ip6h->ip6_dst);
		}
		rq = ill->ill_rq;
		goto ipv6forus;
	}

	/*
	 * Find an ire that matches destination. For link-local addresses
	 * we have to match the ill.
	 * TBD for site local addresses.
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_dst)) {
		ire = ire_ctable_lookup_v6(&ip6h->ip6_dst, NULL,
		    IRE_CACHE|IRE_LOCAL, ill->ill_ipif, NULL,
		    MATCH_IRE_TYPE | MATCH_IRE_ILL);
	} else {
		ire = ire_cache_lookup_v6(&ip6h->ip6_dst);
	}
	if (ire == NULL) {
		/*
		 * No matching IRE found.  Mark this packet as having
		 * originated externally.
		 */
		if (!ipv6_forward || ll_multicast) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6ForwProhibits);
			if (!ipv6_forward)
				BUMP_MIB(ill->ill_ip6_mib->ipv6InAddrErrors);
			freemsg(mp);
			return;
		}
		if (ip6h->ip6_hops <= 1) {
			icmp_time_exceeded_v6(WR(q), mp,
			    ICMP6_TIME_EXCEED_TRANSIT, ll_multicast, B_FALSE);
			return;
		}
		mp->b_prev = (mblk_t *)q;
		if (IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_dst)) {
			ip_newroute_v6(q, mp, &ip6h->ip6_dst,
			    &ip6h->ip6_src, ill);
		} else {
			ip_newroute_v6(q, mp, &ip6h->ip6_dst,
			    &ip6h->ip6_src, NULL);
		}
		return;
	}

	/* we have a matching IRE */
	if (ire->ire_stq != NULL) {
		/* This ire has a send-to queue - forward the packet */
		if (!ipv6_forward || ll_multicast) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6ForwProhibits);
			if (!ipv6_forward)
				BUMP_MIB(ill->ill_ip6_mib->ipv6InAddrErrors);
			freemsg(mp);
			ire_refrele(ire);
			return;
		}
		if (ip6h->ip6_hops <= 1) {
			ip1dbg(("ip_rput_data_v6: hop limit expired.\n"));
			icmp_time_exceeded_v6(WR(q), mp,
			    ICMP6_TIME_EXCEED_TRANSIT, ll_multicast, B_FALSE);
			ire_refrele(ire);
			return;
		}
		if (pkt_len > ire->ire_max_frag) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InTooBigErrors);
			icmp_pkt2big_v6(WR(q), mp, ire->ire_max_frag,
			    ll_multicast, B_TRUE);
			ire_refrele(ire);
			return;
		}
		if (ire->ire_rfq == q && ipv6_send_redirects) {
			in6_addr_t	*v6targ;
			mblk_t		*mp1;
			in6_addr_t	gw_addr_v6;
			ire_t		*src_ire_v6 = NULL;

			/*
			 * The packet is going out the same interface
			 * it came in.  If the packet is not source routed
			 * through this interface then send a redirect
			 * Use  link-local source address (ill_ipif).
			 */
			if (!ip_source_routed_v6(ip6h, mp)) {
				mutex_enter(&ire->ire_lock);
				gw_addr_v6 = ire->ire_gateway_addr_v6;
				mutex_exit(&ire->ire_lock);
				if (!IN6_IS_ADDR_UNSPECIFIED(&gw_addr_v6)) {
					v6targ = &gw_addr_v6;
					/*
					 * We won't send redirects to a router
					 * that doesn't have a link local
					 * address, but will forward.
					 */
					if (!IN6_IS_ADDR_LINKLOCAL(v6targ)) {
						BUMP_MIB(ill->ill_ip6_mib->
						    ipv6InAddrErrors);
						goto forward;
					}
				} else {
					v6targ = &ip6h->ip6_dst;
				}

				src_ire_v6 = ire_ftable_lookup_v6(
						&ip6h->ip6_src, NULL, NULL,
						IRE_INTERFACE, ire->ire_ipif,
						NULL, NULL, 0, MATCH_IRE_IPIF |
						MATCH_IRE_TYPE);

				if (src_ire_v6 != NULL) {
					/*
					 * The source is directly connected.
					 */
					mp1 = copymsg(mp);
					if (mp1 != NULL) {
						icmp_send_redirect_v6(WR(q),
						    mp1, v6targ, &ip6h->ip6_dst,
						    ill, B_FALSE);
					}
					ire_refrele(src_ire_v6);
				}
			}
		} else {
			/* Don't forward link-locals off-link */
			if (IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_dst)) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6InAddrErrors);
				freemsg(mp);
				ire_refrele(ire);
				return;
			}
			/* TBD add site-local check at site boundary? */
		}

forward:
		/* Hoplimit verified above */
		ip6h->ip6_hops--;
		ire->ire_ib_pkt_count++;
		BUMP_MIB(ill->ill_ip6_mib->ipv6OutForwDatagrams);
		ip_xmit_v6(mp, ire, 0, NULL);
		IRE_REFRELE(ire);
		return;
	}
	rq = ire->ire_rfq;

	/*
	 * Need to put on correct queue for reassembly to find it.
	 * No need to use put() since reassembly has its own locks.
	 * Note: multicast will be reassembled on the
	 * arriving ill.
	 * Guard against packets destined to loopback (ire_rfq is NULL
	 * for loopback).
	 */
	if (rq != q) {
		ASSERT(!IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst));
		if ((ipv6_strict_dst_multihoming && !ipv6_forward) ||
		    rq == NULL) {
			/*
			 * This packet came in on an interface other than the
			 * one associated with the destination address
			 * and we are strict about matches.
			 */
			BUMP_MIB(ill->ill_ip6_mib->ipv6ForwProhibits);
			freemsg(mp);
			ire_refrele(ire);
			return;
		}

		q = rq;
		ill = (ill_t *)q->q_ptr;
		ASSERT(ill);
	}

	ire->ire_ib_pkt_count++;
	/* Don't use the ire after this point. */
	ire_refrele(ire);
ipv6forus:
	/*
	 * Looks like this packet is for us one way or another.
	 * This is where we'll process destination headers etc.
	 */
	for (; ; ) {
		switch (nexthdr) {
		case IPPROTO_TCP: {
			uint16_t	*up;
			uint32_t	sum;
			dblk_t		*dp;
			uint32_t	ports;
			uint_t		hdr_len = pkt_len - remlen;
			int		offset;

			/* TCP needs all of the TCP header */
			if (remlen < TCP_MIN_HEADER_LENGTH)
				goto pkt_too_short;
			if (mp->b_cont != NULL &&
			    whereptr + TCP_MIN_HEADER_LENGTH > mp->b_wptr) {
				if (!pullupmsg(mp,
				    hdr_len + TCP_MIN_HEADER_LENGTH)) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + hdr_len;
			}
			/*
			 * Extract the offset field from the TCP header.
			 */
			offset = ((uchar_t *)ip6h)[hdr_len + 12] >> 4;
			if (offset != 5) {
				if (offset < 5) {
					freemsg(mp);
					return;
				}
				/*
				 * There must be TCP options.
				 * Make sure we can grab them.
				 */
				offset <<= 2;
				if (remlen < offset)
					goto pkt_too_short;
				if (mp->b_cont != NULL &&
				    whereptr + offset > mp->b_wptr) {
					if (!pullupmsg(mp,
					    hdr_len + offset)) {
						BUMP_MIB(ill->ill_ip6_mib->
						    ipv6InDiscards);
						freemsg(mp);
						return;
					}
					ip6h = (ip6_t *)mp->b_rptr;
					whereptr = (uint8_t *)ip6h + hdr_len;
				}
			}

			/*
			 * If packet is being looped back locally checksums
			 * aren't used
			 */
			if (no_cksum) {
				if (mp->b_datap->db_type == M_DATA) {
					/*
					 * M_DATA mblk, so init mblk (chain)
					 * for no struio().
					 */
					mblk_t  *mp1 = mp;

					do {
						mp1->b_datap->db_struioflag = 0;
					} while ((mp1 = mp1->b_cont) != NULL);
				}
				goto tcp_fanout;
			}

			up = (uint16_t *)&ip6h->ip6_src;
			/*
			 * TCP checksum calculation.  First sum up the
			 * pseudo-header fields:
			 *  -	Source IPv6 address
			 *  -	Destination IPv6 address
			 *  -	TCP payload length
			 *  -	TCP protocol ID
			 * XXX need zero-copy support here
			 */
			sum = htons(IPPROTO_TCP + remlen) +
			    up[0] + up[1] + up[2] + up[3] +
			    up[4] + up[5] + up[6] + up[7] +
			    up[8] + up[9] + up[10] + up[11] +
			    up[12] + up[13] + up[14] + up[15];
			sum = (sum & 0xffff) + (sum >> 16);
			dp = mp->b_datap;
			if (dp->db_type != M_DATA || dp->db_ref > 1
			    /* BEGIN CSTYLED */
#ifdef ZC_TEST
			    || !syncstream
#endif
				) {
				/* END CSTYLED */
				/*
				 * Not M_DATA mblk or its a dup, so do the
				 * checksum now.
				 */
				sum = IP_CSUM(mp, hdr_len, sum);
				if (sum) {
					/* checksum failed */
					ip1dbg(("ip_rput_data_v6: TCP checksum"
					    " failed %x off %d\n",
					    sum, hdr_len));
					BUMP_MIB(ip_mib.tcpInErrs);
					freemsg(mp);
					return;
				}
			} else {
				/*
				 * M_DATA mblk and not a dup, so postpone the
				 * checksum.
				 */
				mblk_t	*mp1 = mp;
				dblk_t	*dp1;

				sum += (sum >> 16);
				*(uint16_t *)dp->db_struioun.data =
				    (uint16_t)sum;
				dp->db_struiobase = (uchar_t *)whereptr;
				dp->db_struioptr = (uchar_t *)whereptr;
				dp->db_struiolim = mp->b_wptr;
				dp->db_struioflag |= STRUIO_SPEC|STRUIO_IP;
				while ((mp1 = mp1->b_cont) != NULL) {
					dp1 = mp1->b_datap;
					*(uint16_t *)dp1->db_struioun.data = 0;
					dp1->db_struiobase = mp1->b_rptr;
					dp1->db_struioptr = mp1->b_rptr;
					dp1->db_struiolim = mp1->b_wptr;
					dp1->db_struioflag |=
					    STRUIO_SPEC|STRUIO_IP;
				}
			}
		tcp_fanout:
			ports = *(uint32_t *)(mp->b_rptr + hdr_len +
			    TCP_PORTS_OFFSET);
			ip_fanout_tcp_v6(q, mp, ip6h, ports, ill,
			    IP_FF_SEND_ICMP|IP_FF_SYN_ADDIRE|IP_FF_IP6INFO,
			    hdr_len);
			return;
		}
		case IPPROTO_UDP: {
			uint16_t	*up;
			uint32_t	sum;
			uint32_t	ports;
			uint_t		hdr_len = pkt_len - remlen;
#define	UDPH_SIZE 8

			/* Verify that at least the ports are present */
			if (remlen < UDPH_SIZE)
				goto pkt_too_short;
			if (mp->b_cont != NULL &&
			    whereptr + UDPH_SIZE > mp->b_wptr) {
				if (!pullupmsg(mp, hdr_len + UDPH_SIZE)) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + hdr_len;
			}
#undef UDPH_SIZE
			/*
			 * If packet is being looped back locally checksums
			 * aren't used
			 */
			if (no_cksum)
				goto udp_fanout;

			up = (uint16_t *)&ip6h->ip6_src;
			/*
			 * UDP checksum calculation.  First sum up the
			 * pseudo-header fields:
			 *  -	Source IPv6 address
			 *  -	Destination IPv6 address
			 *  -	UDP payload length
			 *  -	UDP protocol ID
			 */
			sum = htons(IPPROTO_UDP + remlen) +
			    up[0] + up[1] + up[2] + up[3] +
			    up[4] + up[5] + up[6] + up[7] +
			    up[8] + up[9] + up[10] + up[11] +
			    up[12] + up[13] + up[14] + up[15];

			sum = (sum & 0xffff) + (sum >> 16);
			/* Next sum in the UDP packet */
			sum = IP_CSUM(mp, hdr_len, sum);
			if (sum) {
				/* UDP checksum failed */
				ip1dbg(("ip_rput_data_v6: UDP checksum "
				    "failed %x\n",
				    sum));
				BUMP_MIB(ill->ill_ip6_mib->udpInCksumErrs);
				freemsg(mp);
				return;
			}
		udp_fanout:
			ports = *(uint32_t *)(mp->b_rptr + hdr_len +
			    UDP_PORTS_OFFSET);
			ip_fanout_udp_v6(q, mp, ip6h, ports, ill,
			    IP_FF_SEND_ICMP|IP_FF_IP6INFO);
			return;
		}
		case IPPROTO_ICMPV6: {
			uint16_t	*up;
			uint32_t	sum;
			uint_t		hdr_len = pkt_len - remlen;
			mblk_t		*mp1;

			/*
			 * If packet is being looped back locally checksums
			 * aren't used
			 */
			if (no_cksum)
				goto icmp_fanout;

			up = (uint16_t *)&ip6h->ip6_src;
			sum = htons(IPPROTO_ICMPV6 + remlen) +
			    up[0] + up[1] + up[2] + up[3] +
			    up[4] + up[5] + up[6] + up[7] +
			    up[8] + up[9] + up[10] + up[11] +
			    up[12] + up[13] + up[14] + up[15];
			sum = (sum & 0xffff) + (sum >> 16);
			sum = IP_CSUM(mp, hdr_len, sum);
			if (sum) {
				/* IPv6 ICMP checksum failed */
				ip1dbg(("ip_rput_data_v6: ICMPv6 checksum "
				    "failed %x\n",
				    sum));
				BUMP_MIB(ill->ill_icmp6_mib->ipv6IfIcmpInMsgs);
				BUMP_MIB(ill->ill_icmp6_mib->
				    ipv6IfIcmpInErrors);
				freemsg(mp);
				return;
			}

		icmp_fanout:
			/* Check variable for testing applications */
			if (ipv6_drop_inbound_icmpv6) {
				freemsg(mp);
				return;
			}
			/*
			 * Assume that there is always at least one ipc for
			 * ICMPv6 (in.ndpd) i.e. don't optimize the case
			 * where there is no ipc.
			 */
			mp1 = copymsg(mp);
			if (mp1)
				icmp_inbound_v6(q, mp1, ill, hdr_len);
		}
			/* FALLTHRU */
		default: {
			/*
			 * Handle protocols with which IPv6 is less intimate.
			 */
			uint_t flags = IP_FF_RAWIP|IP_FF_IP6INFO;

			/*
			 * Enable sending ICMP for "Unknown" nexthdr
			 * case. i.e. where we did not FALLTHRU from
			 * IPPROTO_ICMPV6 processing case above.
			 */
			if (nexthdr != IPPROTO_ICMPV6)
				flags |= IP_FF_SEND_ICMP;

			ip_fanout_proto_v6(q, mp, ip6h, ill, nexthdr,
			    prev_nexthdr_offset, flags);
			return;
		}

		case IPPROTO_DSTOPTS: {
			uint_t ehdrlen;
			uint8_t *optptr;
			ip6_dest_t *desthdr;

			if (remlen < MIN_EHDR_LEN)
				goto pkt_too_short;
			if (mp->b_cont != NULL &&
			    whereptr + MIN_EHDR_LEN > mp->b_wptr) {
				if (!pullupmsg(mp,
				    pkt_len - remlen + MIN_EHDR_LEN)) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + pkt_len - remlen;
			}
			desthdr = (ip6_dest_t *)whereptr;
			nexthdr = desthdr->ip6d_nxt;
			prev_nexthdr_offset = (uint_t)(whereptr -
			    (uint8_t *)ip6h);
			ehdrlen = 8 * (desthdr->ip6d_len + 1);
			if (remlen < ehdrlen)
				goto pkt_too_short;
			if (mp->b_cont != NULL &&
			    whereptr + ehdrlen > mp->b_wptr) {
				if (!pullupmsg(mp,
				    pkt_len - remlen + ehdrlen)) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + pkt_len - remlen;
			}
			optptr = whereptr + 2;
			/*
			 * Note: XXX This code does not seem to make
			 * distinction between Destination Options Header
			 * being before/after Routing Header which can
			 * happen if we are at the end of source route.
			 * This may become significant in future.
			 * (No real significant Destination Options are
			 * defined/implemented yet ).
			 */
			switch (ip_process_options_v6(q, mp, ip6h, optptr,
			    ehdrlen - 2, IPPROTO_DSTOPTS)) {
			case -1:
				/*
				 * Packet has been consumed and any needed
				 * ICMP errors sent.
				 */
				BUMP_MIB(ill->ill_ip6_mib->ipv6InHdrErrors);
				return;
			case 0:
				/* No action needed  continue */
				break;
			case 1:
				/*
				 * Unnexpected return value
				 * (Router alert is a Hop-by-Hop option)
				 */
#ifdef DEBUG
				cmn_err(CE_PANIC, "ip_rput_data_v6: router "
				    "alert hbh opt indication in dest opt");
#endif
				freemsg(mp);
				BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
				return;
			}
			used = ehdrlen;
			break;
		}
		case IPPROTO_FRAGMENT: {
			ip6_frag_t *fraghdr;
			size_t no_frag_hdr_len;

			if (remlen < sizeof (ip6_frag_t))
				goto pkt_too_short;

			if (mp->b_cont != NULL &&
			    whereptr + sizeof (ip6_frag_t) > mp->b_wptr) {
				if (!pullupmsg(mp,
				    pkt_len - remlen + sizeof (ip6_frag_t))) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + pkt_len - remlen;
			}

			fraghdr = (ip6_frag_t *)whereptr;
			used = (uint_t)sizeof (ip6_frag_t);
			BUMP_MIB(ill->ill_ip6_mib->ipv6ReasmReqds);
			mp = ip_rput_frag_v6(q, mp, ip6h, fraghdr,
			    remlen - used, &prev_nexthdr_offset);
			if (mp == NULL) {
				/* Reassembly is still pending */
				return;
			}
			/* The first mblk are the headers before the frag hdr */
			BUMP_MIB(ill->ill_ip6_mib->ipv6ReasmOKs);

			no_frag_hdr_len = mp->b_wptr - mp->b_rptr;
			ip6h = (ip6_t *)mp->b_rptr;
			nexthdr = ((char *)ip6h)[prev_nexthdr_offset];
			whereptr = mp->b_rptr + no_frag_hdr_len;
			remlen = ntohs(ip6h->ip6_plen)  +
			    (uint16_t)(IPV6_HDR_LEN - no_frag_hdr_len);
			pkt_len = msgdsize(mp);
			used = 0;
			break;
		}
		case IPPROTO_HOPOPTS:
			/*
			 * Illegal header sequence.
			 * (Hop-by-hop headers are processed above
			 *  and required to immediately follow IPv6 header)
			 */
			icmp_param_problem_v6(WR(q), mp,
			    ICMP6_PARAMPROB_NEXTHEADER,
			    prev_nexthdr_offset,
			    B_FALSE, B_FALSE);
			return;

		case IPPROTO_ROUTING: {
			uint_t ehdrlen;
			ip6_rthdr_t *rthdr;

			if (remlen < MIN_EHDR_LEN)
				goto pkt_too_short;
			if (mp->b_cont != NULL &&
			    whereptr + MIN_EHDR_LEN > mp->b_wptr) {
				if (!pullupmsg(mp,
				    pkt_len - remlen + MIN_EHDR_LEN)) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + pkt_len - remlen;
			}
			rthdr = (ip6_rthdr_t *)whereptr;
			nexthdr = rthdr->ip6r_nxt;
			prev_nexthdr_offset = (uint_t)(whereptr -
			    (uint8_t *)ip6h);
			ehdrlen = 8 * (rthdr->ip6r_len + 1);
			if (remlen < ehdrlen)
				goto pkt_too_short;
			if (mp->b_cont != NULL &&
			    whereptr + ehdrlen > mp->b_wptr) {
				if (!pullupmsg(mp,
				    pkt_len - remlen + ehdrlen)) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6InDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				whereptr = (uint8_t *)ip6h + pkt_len - remlen;
				rthdr = (ip6_rthdr_t *)whereptr;
			}
			if (rthdr->ip6r_segleft != 0) {
				/* Not end of source route */
				if (ll_multicast) {
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6ForwProhibits);
					freemsg(mp);
					return;
				}
				ip_process_rthdr(q, mp, ip6h, rthdr, ill);
				return;
			}
			used = ehdrlen;
			break;
		}
		case IPPROTO_NONE:
			/* All processing is done. Count as "delivered". */
			freemsg(mp);
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDelivers);
			return;
		}
		whereptr += used;
		ASSERT(remlen >= used);
		remlen -= used;
	}
	/* NOTREACHED */
pkt_too_short:
	ip1dbg(("ip_rput_data_v6: packet too short %d %lu %d\n",
	    ip6_len, pkt_len, remlen));
	BUMP_MIB(ill->ill_ip6_mib->ipv6InTruncatedPkts);
	freemsg(mp);
}

/*
 * Reassemble fragment.
 * When it returns a completed message the first mblk will only contain
 * the headers prior to the fragment header.
 *
 * prev_nexthdr_offset is an offset indication of where the nexthdr field is
 * of the preceding header.  This is needed to patch the previous header's
 * nexthdr field when reassembly completes.
 */
static mblk_t *
ip_rput_frag_v6(queue_t *q, mblk_t *mp, ip6_t *ip6h,
    ip6_frag_t *fraghdr, uint_t remlen, uint_t *prev_nexthdr_offset)
{
	ill_t		*ill = (ill_t *)q->q_ptr;
	uint32_t	ident = ntohl(fraghdr->ip6f_ident);
	uint16_t	offset;
	boolean_t	more_frags;
	uint8_t		nexthdr = fraghdr->ip6f_nxt;
	in6_addr_t	*v6dst_ptr;
	in6_addr_t	*v6src_ptr;
	uint_t		end;
	uint_t		hdr_length;
	size_t		count;
	ipf_t		*ipf;
	ipf_t		**ipfp;
	ipfb_t		*ipfb;
	mblk_t		*mp1;

	/*
	 * Note: Fragment offset in header is in 8-octet units.
	 * Clearing least significant 3 bits not only extracts
	 * it but also gets it in units of octets.
	 */
	offset = ntohs(fraghdr->ip6f_offlg) & ~7;
	more_frags = (fraghdr->ip6f_offlg & IP6F_MORE_FRAG);

	/*
	 * Is the more frags flag on and the payload length not a multiple
	 * of eight?
	 */
	if (more_frags && (ntohs(ip6h->ip6_plen) & 7)) {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InHdrErrors);
		icmp_param_problem_v6(WR(q), mp, ICMP6_PARAMPROB_HEADER,
		    (uint32_t)((char *)&ip6h->ip6_plen -
		    (char *)ip6h), B_FALSE, B_FALSE);
		return (NULL);
	}

	v6src_ptr = &ip6h->ip6_src;
	v6dst_ptr = &ip6h->ip6_dst;
	end = remlen;

	hdr_length = (uint_t)((char *)&fraghdr[1] - (char *)ip6h);
	end += offset;

	/*
	 * Would fragment cause reassembled packet to have a payload length
	 * greater than IP_MAXPACKET - the max payload size?
	 */
	if (end > IP_MAXPACKET) {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InHdrErrors);
		icmp_param_problem_v6(WR(q), mp, ICMP6_PARAMPROB_HEADER,
		    (uint32_t)((char *)&fraghdr->ip6f_offlg -
		    (char *)ip6h), B_FALSE, B_FALSE);
		return (NULL);
	}

	/*
	 * This packet just has one fragment. Reassembly not
	 * needed.
	 */
	if (!more_frags && offset == 0) {
		goto reass_done;
	}

	/*
	 * If this is not the first fragment, dump the unfragmentable
	 * portion of the packet.
	 */
	if (offset)
		mp->b_rptr = (uchar_t *)&fraghdr[1];

	/*
	 * Fragmentation reassembly.  Each ILL has a hash table for
	 * queueing packets undergoing reassembly for all IPIFs
	 * associated with the ILL.  The hash is based on the packet
	 * IP ident field.  The ILL frag hash table was allocated
	 * as a timer block at the time the ILL was created.  Whenever
	 * there is anything on the reassembly queue, the timer will
	 * be running.
	 */
	if (ill->ill_frag_count > ip_reass_queue_bytes)
		ill_frag_prune(ill, ip_reass_queue_bytes);
	ipfb = &ill->ill_frag_hash_tbl[ILL_FRAG_HASH_V6(*v6src_ptr, ident)];
	mutex_enter(&ipfb->ipfb_lock);
	ipfp = &ipfb->ipfb_ipf;
	/* Try to find an existing fragment queue for this packet. */
	for (;;) {
		ipf = ipfp[0];
		if (ipf) {
			/*
			 * It has to match on ident, source address, and
			 * dest address.
			 */
			if (ipf->ipf_ident == ident &&
			    IN6_ARE_ADDR_EQUAL(&ipf->ipf_v6src, v6src_ptr) &&
			    IN6_ARE_ADDR_EQUAL(&ipf->ipf_v6dst, v6dst_ptr))
				break;
			ipfp = &ipf->ipf_hash_next;
			continue;
		}
		/* New guy.  Allocate a frag message. */
		mp1 = allocb(sizeof (*ipf), BPRI_MED);
		if (!mp1) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			freemsg(mp);
	partial_reass_done:
			mutex_exit(&ipfb->ipfb_lock);
			return (NULL);
		}
		mp1->b_cont = mp;

		/* Initialize the fragment header. */
		ipf = (ipf_t *)mp1->b_rptr;
		ipf->ipf_mp = mp1;
		ipf->ipf_ptphn = ipfp;
		ipfp[0] = ipf;
		ipf->ipf_hash_next = NULL;
		ipf->ipf_ident = ident;
		ipf->ipf_v6src = *v6src_ptr;
		ipf->ipf_v6dst = *v6dst_ptr;
		/* Record reassembly start time. */
		ipf->ipf_timestamp = hrestime.tv_sec;
		/* Record ipf generation and account for frag header */
		ipf->ipf_gen = ill->ill_ipf_gen++;
		ipf->ipf_count = mp1->b_datap->db_lim - mp1->b_datap->db_base;
		ipf->ipf_protocol = nexthdr;
		ipf->ipf_nf_hdr_len = 0;
		ipf->ipf_prev_nexthdr_offset = 0;
		ipf->ipf_last_frag_seen = !more_frags;
		/*
		 * We handle reassembly two ways.  In the easy case,
		 * where all the fragments show up in order, we do
		 * minimal bookkeeping, and just clip new pieces on
		 * the end.  If we ever see a hole, then we go off
		 * to ip_reassemble which has to mark the pieces and
		 * keep track of the number of holes, etc.  Obviously,
		 * the point of having both mechanisms is so we can
		 * handle the easy case as efficiently as possible.
		 */
		if (offset == 0) {
			/* Easy case, in-order reassembly so far. */
			/* Update the byte count */
			ipf->ipf_count += mp->b_datap->db_lim -
			    mp->b_datap->db_base;
			while (mp->b_cont) {
				mp = mp->b_cont;
				ipf->ipf_count += mp->b_datap->db_lim -
				    mp->b_datap->db_base;
			}
			ipf->ipf_tail_mp = mp;
			/*
			 * Keep track of next expected offset in
			 * ipf_end.
			 */
			ipf->ipf_end = end;
			ipf->ipf_stripped_hdr_len = 0;
			ipf->ipf_nf_hdr_len = hdr_length;
			ipf->ipf_prev_nexthdr_offset = *prev_nexthdr_offset;
		} else {
			/* Hard case, hole at the beginning. */
			ipf->ipf_tail_mp = NULL;
			/*
			 * ipf_end == 0 means that we have given up
			 * on easy reassembly.
			 */
			ipf->ipf_end = 0;
			/*
			 * ipf_hole_cnt and ipf_stripped_hdr_len are
			 * set by ip_reassemble.
			 *
			 * ipf_count is updated by ip_reassemble.
			 */
			(void) ip_reassemble(mp, ipf, offset, more_frags,
			    hdr_length, ill);
		}
		/* Update per ipfb and ill byte counts */
		ipfb->ipfb_count += ipf->ipf_count;
		ASSERT(ipfb->ipfb_count > 0);	/* Wraparound */
		ill->ill_frag_count += ipf->ipf_count;
		ASSERT(ill->ill_frag_count > 0);	/* Wraparound */
		/* If the frag timer wasn't already going, start it. */
		if (ill->ill_frag_timer_id == 0) {
			ill->ill_frag_timer_id = qtimeout(ill->ill_rq,
			    ill_frag_timer, ill,
			    MSEC_TO_TICK(ip_g_frag_timo_ms));
		}
		goto partial_reass_done;
	}

	/*
	 * If we have seen the last fragment already and this
	 * fragment does not have more_frags set, ignore this
	 * packet. ip_reassemble will not calculate the hole_cnt
	 * properly if we send in two packets with more_frags = 0.
	 */
	if (ipf->ipf_last_frag_seen && !more_frags) {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
		freemsg(mp);
		goto partial_reass_done;
	} else {
		ipf->ipf_last_frag_seen = !more_frags;
	}

	/*
	 * We have a new piece of a datagram which is already being
	 * reassembled.
	 */

	if (offset && ipf->ipf_end == offset) {
		/* The new fragment fits at the end */
		ipf->ipf_tail_mp->b_cont = mp;
		/* Update the byte count */
		count = mp->b_datap->db_lim - mp->b_datap->db_base;
		while (mp->b_cont) {
			mp = mp->b_cont;
			count += mp->b_datap->db_lim - mp->b_datap->db_base;
		}
		ipf->ipf_count += count;
		/* Update per ipfb and ill byte counts */
		ipfb->ipfb_count += count;
		ASSERT(ipfb->ipfb_count > 0);	/* Wraparound */
		ill->ill_frag_count += count;
		ASSERT(ill->ill_frag_count > 0);	/* Wraparound */
		if (more_frags) {
			/* More to come. */
			ipf->ipf_end = end;
			ipf->ipf_tail_mp = mp;
			goto partial_reass_done;
		}
	} else {
		/*
		 * Go do the hard cases.
		 * Call ip_reassable().
		 */
		boolean_t ret;

		if (offset == 0) {
			if (ipf->ipf_prev_nexthdr_offset == 0) {
				ipf->ipf_nf_hdr_len = hdr_length;
				ipf->ipf_prev_nexthdr_offset =
				    *prev_nexthdr_offset;
			}
		}
		/* Save current byte count */
		count = ipf->ipf_count;
		ret = ip_reassemble(mp, ipf, offset, more_frags,
		    offset ? hdr_length : 0, ill);
		/* Count of bytes added and subtracted (freeb()ed) */
		count = ipf->ipf_count - count;
		if (count) {
			/* Update per ipfb and ill byte counts */
			ipfb->ipfb_count += count;
			ASSERT(ipfb->ipfb_count > 0);	/* Wraparound */
			ill->ill_frag_count += count;
			ASSERT(ill->ill_frag_count > 0); /* Wraparound */
		}
		if (! ret)
			goto partial_reass_done;
		/* Return value of 'true' means mp is complete. */
	}
	/*
	 * We have completed reassembly.  Unhook the frag header from
	 * the reassembly list.
	 *
	 * Grab the unfragmentable header length next header value out
	 * of the first fragment
	 */
	ASSERT(ipf->ipf_nf_hdr_len != 0);
	hdr_length = ipf->ipf_nf_hdr_len;

	/*
	 * Store the nextheader field in the header preceding the fragment
	 * header
	 */
	nexthdr = ipf->ipf_protocol;
	*prev_nexthdr_offset = ipf->ipf_prev_nexthdr_offset;
	ipfp = ipf->ipf_ptphn;
	mp1 = ipf->ipf_mp;
	count = ipf->ipf_count;
	ipf = ipf->ipf_hash_next;
	if (ipf)
		ipf->ipf_ptphn = ipfp;
	ipfp[0] = ipf;
	ill->ill_frag_count -= count;
	ASSERT(ipfb->ipfb_count >= count);
	ipfb->ipfb_count -= count;
	mutex_exit(&ipfb->ipfb_lock);
	/* Ditch the frag header. */
	mp = mp1->b_cont;
	freeb(mp1);

	/*
	 * Make sure the packet is good by doing some sanity
	 * check. If bad we can silentely drop the packet.
	 */
reass_done:
	if (hdr_length < sizeof (ip6_frag_t)) {
		BUMP_MIB(ill->ill_ip6_mib->ipv6InHdrErrors);
		ip1dbg(("ip_rput_frag_v6: bad packet\n"));
		freemsg(mp);
		return (NULL);
	}

	/*
	 * Remove the fragment header from the initial header by
	 * splitting the mblk into the non-fragmentable header and
	 * everthing after the fragment extension header.  This has the
	 * side effect of putting all the headers that need destination
	 * processing into the b_cont block-- on return this fact is
	 * used in order to avoid having to look at the extensions
	 * already processed.
	 */
	if (mp->b_rptr + hdr_length != mp->b_wptr) {
		mblk_t *nmp;

		if (!(nmp = dupb(mp))) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
			ip1dbg(("ip_rput_frag_v6: dupb failed\n"));
			freemsg(mp);
			return (NULL);
		}
		nmp->b_cont = mp->b_cont;
		mp->b_cont = nmp;
		nmp->b_rptr += hdr_length;
	}
	mp->b_wptr = mp->b_rptr + hdr_length - sizeof (ip6_frag_t);

	ip6h = (ip6_t *)mp->b_rptr;
	((char *)ip6h)[*prev_nexthdr_offset] = nexthdr;

	/* Restore original IP length in header. */
	ip6h->ip6_plen = htons((uint16_t)(msgdsize(mp) - IPV6_HDR_LEN));

	return (mp);
}

/*
 * ip_source_routed_v6:
 * This function is called by redirect code in ip_rput_data_v6 to
 * know whether this packet is source routed through this node i.e
 * whether this node (router) is part of the journey. This
 * function is called under two cases :
 *
 * case 1 : Routing header was processed by this node and
 *	    ip_process_rthdr replaced ip6_dst with the next hop
 *          and we are forwarding the packet to the next hop.
 *
 * case 2 : Routing header was not processed by this node and we
 *	    are just forwarding the packet.
 *
 * For case (1) we don't want to send redirects. For case(2) we
 * want to send redirects.
 */
/* ARGSUSED */
static boolean_t
ip_source_routed_v6(ip6_t *ip6h, mblk_t *mp)
{
	uint8_t		nexthdr;
	in6_addr_t	*addrptr;
	ip6_rthdr0_t	*rthdr;
	uint8_t		numaddr;
	ip6_hbh_t	*hbhhdr;
	uint_t		ehdrlen;
	uint8_t		*byteptr;

	ip2dbg(("ip_source_routed_v6\n"));
	nexthdr = ip6h->ip6_nxt;
	ehdrlen = IPV6_HDR_LEN;

	/* if a routing hdr is preceeded by HOPOPT or DSTOPT */
	while (nexthdr == IPPROTO_HOPOPTS ||
	    nexthdr == IPPROTO_DSTOPTS) {
		byteptr = (uint8_t *)ip6h + ehdrlen;
		/*
		 * Check if we have already processed
		 * packets or we are just a forwarding
		 * router which only pulled up msgs up
		 * to IPV6HDR and  one HBH ext header
		 */
		if (byteptr + MIN_EHDR_LEN > mp->b_wptr) {
			ip2dbg(("ip_source_routed_v6: Extension"
			    " headers not processed\n"));
			return (B_FALSE);
		}
		hbhhdr = (ip6_hbh_t *)byteptr;
		nexthdr = hbhhdr->ip6h_nxt;
		ehdrlen = ehdrlen + 8 * (hbhhdr->ip6h_len + 1);
	}
	switch (nexthdr) {
	case IPPROTO_ROUTING:
		byteptr = (uint8_t *)ip6h + ehdrlen;
		/*
		 * If for some reason, we haven't pulled up
		 * the routing hdr data mblk, then we must
		 * not have processed it at all. So for sure
		 * we are not part of the source routed journey.
		 */
		if (byteptr + MIN_EHDR_LEN > mp->b_wptr) {
			ip2dbg(("ip_source_routed_v6: Routing"
			    " header not processed\n"));
			return (B_FALSE);
		}
		rthdr = (ip6_rthdr0_t *)byteptr;
		/*
		 * Either we are an intermediate router or the
		 * last hop before destination and we have
		 * already processed the routing header.
		 * If segment_left is greater than or equal to zero,
		 * then we must be the (numaddr - segleft) entry
		 * of the routing header. Although ip6r0_segleft
		 * is a unit8_t variable, we still check for zero
		 * or greater value, if in case the data type
		 * is changed someday in future.
		 */
		if (rthdr->ip6r0_segleft > 0 ||
		    rthdr->ip6r0_segleft == 0) {
			ire_t 	*ire = NULL;

			numaddr = rthdr->ip6r0_len / 2;
			addrptr = (in6_addr_t *)&rthdr->ip6r0_addr;
			addrptr += (numaddr - (rthdr->ip6r0_segleft + 1));
			if (addrptr != NULL) {
				ire = ire_ctable_lookup_v6(addrptr, NULL,
				    IRE_LOCAL, NULL, NULL, MATCH_IRE_TYPE);
				if (ire != NULL) {
					ire_refrele(ire);
					return (B_TRUE);
				}
				ip1dbg(("ip_source_routed_v6: No ire found\n"));
			}
		}
	/* FALLTHRU */
	default:
		ip2dbg(("ip_source_routed_v6: Not source routed here\n"));
		return (B_FALSE);
	}
}

/*
 * ip_wput_v6 -- Packets sent down from transport modules show up here.
 * Assumes that the following set of headers appear in the first
 * mblk:
 *	ip6i_t (if present) CAN also appear as a separate mblk.
 *	ip6_t
 *	Any extension headers
 *	TCP/UDP header (if present)
 * The routine can handle an ICMPv6 header that is not in the first mblk.
 *
 * The order to determine the outgoing interface is as follows:
 * 1. If an ip6i_t with IP6I_IFINDEX set then use that ill.
 * 2. If q is an ill queue and (link local or multicast destination) then
 *    use that ill.
 * 3. If IPV6_BOUND_IF has been set use that ill.
 * 4. For multicast: if IPV6_MULTICAST_IF has been set use it. Otherwise
 *    look for the best IRE match for the unspecified group to determine
 *    the ill.
 * 4. For unicast: Just do an IRE lookup for the best match.
 */
void
ip_wput_v6(queue_t *q, mblk_t *mp)
{
	ire_t		*ire = NULL;
	ip6_t		*ip6h;
	ipc_t		*ipc;
	in6_addr_t	*v6dstp;
	ill_t		*ill;
	ipif_t		*ipif;
	ip6i_t		*ip6i;
	int		cksum_request;	/* -1 => normal. */
			/* 1 => Skip TCP/UDP checksum */
			/* Otherwise contains insert offset for checksum */
	int		unspec_src;
	int		do_outrequests;	/* Increment OutRequests? */
	mib2_ipv6IfStatsEntry_t	*mibptr;

	if (mp->b_datap->db_type != M_DATA) {
		if (mp->b_datap->db_type == M_CTL)
			ip_wput(q, mp);
		else
			ip_wput_nondata(q, mp);
		return;
	}

	ip6h = (ip6_t *)mp->b_rptr;

	/*
	 * Highest bit in version field is Reachability Confirmation bit
	 * used by NUD in ip_xmit_v6().
	 */
#ifdef	_BIG_ENDIAN
#define	IPVER(ip6h)	((((uint32_t *)ip6h)[0] >> 28) & 0x7)
#else
#define	IPVER(ip6h)	((((uint32_t *)ip6h)[0] >> 4) & 0x7)
#endif

	if (IPVER(ip6h) != IPV6_VERSION) {
		mibptr = &ip6_mib;
		goto notv6;
	}

	if (q->q_next != NULL) {
		ipc = NULL;
		ill = (ill_t *)q->q_ptr;
		mibptr = ill->ill_ip6_mib;
		unspec_src = 0;
		BUMP_MIB(mibptr->ipv6OutRequests);
		do_outrequests = B_FALSE;
	} else {
		ipc = (ipc_t *)q->q_ptr;
		ill = NULL;
		/* is queue flow controlled? */
		if (q->q_first && !ipc->ipc_draining) {
			(void) putq(q, mp);
			return;
		}
		mibptr = &ip6_mib;
		unspec_src = ipc->ipc_unspec_src;
		do_outrequests = B_TRUE;
		if (ipc->ipc_dontroute)
			ip6h->ip6_hops = 1;
	}

	/* check for alignment and full IPv6 header */
	if (!OK_32PTR((uchar_t *)ip6h) ||
	    (mp->b_wptr - (uchar_t *)ip6h) < IPV6_HDR_LEN) {
		ip0dbg(("ip_wput_v6: bad alignment or length\n"));
		if (do_outrequests)
			BUMP_MIB(mibptr->ipv6OutRequests);
		BUMP_MIB(mibptr->ipv6OutDiscards);
		freemsg(mp);
		return;
	}
	v6dstp = &ip6h->ip6_dst;
	cksum_request = -1;
	ip6i = NULL;

	if (ip6h->ip6_nxt == IPPROTO_RAW) {
		/*
		 * This is an ip6i_t header followed by an ip6_hdr.
		 * Check which fields are set.
		 *
		 * When the packet comes from a transport we should have
		 * all needed headers in the first mblk. However, when
		 * going through ip_newroute*_v6 the ip6i might be in
		 * a separate mblk when we return here. In that case
		 * we pullup everything to ensure that extension and transport
		 * headers "stay" in the first mblk.
		 */
		ip6i = (ip6i_t *)ip6h;

		ASSERT((mp->b_wptr - (uchar_t *)ip6i) == sizeof (ip6i_t) ||
		    ((mp->b_wptr - (uchar_t *)ip6i) >=
		    sizeof (ip6i_t) + IPV6_HDR_LEN));

		if ((mp->b_wptr - (uchar_t *)ip6i) == sizeof (ip6i_t)) {
			if (!pullupmsg(mp, -1)) {
				ip1dbg(("ip_wput_v6: pullupmsg failed\n"));
				if (do_outrequests)
					BUMP_MIB(mibptr->ipv6OutRequests);
				BUMP_MIB(mibptr->ipv6OutDiscards);
				freemsg(mp);
				return;
			}
			ip6h = (ip6_t *)mp->b_rptr;
			v6dstp = &ip6h->ip6_dst;
			ip6i = (ip6i_t *)ip6h;
		}
		ip6h = (ip6_t *)&ip6i[1];
		/*
		 * Advance rptr past the ip6i_t to get ready for
		 * transmitting the packet. However, if the packet gets
		 * passed to ip_newroute*_v6 then rptr is moved back so
		 * that the ip6i_t header can be inspected when the
		 * packet comes back here after passing through
		 * ire_add_then_send.
		 */
		mp->b_rptr = (uchar_t *)ip6h;

		if (ip6i->ip6i_flags & IP6I_IFINDEX) {
			ASSERT(ip6i->ip6i_ifindex != 0);
			ill = ill_lookup_on_ifindex(ip6i->ip6i_ifindex, 1);
			if (ill == NULL) {
				if (do_outrequests)
					BUMP_MIB(mibptr->ipv6OutRequests);
				BUMP_MIB(mibptr->ipv6OutDiscards);
				ip1dbg(("ip_wput_v6: bad ifindex %d\n",
				    ip6i->ip6i_ifindex));
				freemsg(mp);
				return;
			}
			mibptr = ill->ill_ip6_mib;
		}
		if (ip6i->ip6i_flags & IP6I_VERIFY_SRC) {
			ASSERT(!IN6_IS_ADDR_UNSPECIFIED(&ip6h->ip6_src));
			if (!IS_PRIVILEGED_QUEUE(q)) {
				ire = ire_route_lookup_v6(&ip6h->ip6_src,
				    0, 0, (IRE_LOCAL|IRE_LOOPBACK), NULL,
				    NULL, NULL, MATCH_IRE_TYPE);
				if (ire == NULL) {
					if (do_outrequests)
						BUMP_MIB(
						    mibptr->ipv6OutRequests);
					BUMP_MIB(mibptr->ipv6OutDiscards);
					ip1dbg(("ip_wput_v6: bad source "
					    "addr\n"));
					freemsg(mp);
					return;
				}
				ire_refrele(ire);
			}
			/* No need to verify again when using ip_newroute */
			ip6i->ip6i_flags &= ~IP6I_VERIFY_SRC;
		}
		if (!(ip6i->ip6i_flags & IP6I_NEXTHOP)) {
			/*
			 * Make sure they match since ip_newroute*_v6 etc might
			 * (unknown to them) inspect ip6i_nexthop when
			 * they think they access ip6_dst.
			 */
			ip6i->ip6i_nexthop = ip6h->ip6_dst;
		}
		if (ip6i->ip6i_flags & IP6I_NO_TCP_UDP_CKSUM)
			cksum_request = 1;
		if (ip6i->ip6i_flags & IP6I_RAW_CHECKSUM)
			cksum_request = ip6i->ip6i_checksum_off;
		if (ip6i->ip6i_flags & IP6I_UNSPEC_SRC)
			unspec_src = 1;

		if (do_outrequests && ill != NULL) {
			BUMP_MIB(mibptr->ipv6OutRequests);
			do_outrequests = B_FALSE;
		}
	}

	if (IN6_IS_ADDR_MULTICAST(v6dstp))
		goto ipv6multicast;

	/* 1. If an ip6i_t with IP6I_IFINDEX set then use that ill. */
	if (ip6i != NULL && (ip6i->ip6i_flags & IP6I_IFINDEX)) {
		ASSERT(ill != NULL);
		goto send_from_ill;
	}

	/*
	 * 2. If q is an ill queue and (link local or multicast destination)
	 *    then use that ill.
	 */
	if (ill != NULL && IN6_IS_ADDR_LINKLOCAL(v6dstp))
		goto send_from_ill;

	/* 3. If IPV6_BOUND_IF has been set use that ill. */
	if (ipc != NULL && ipc->ipc_outgoing_ill != NULL) {
		ill = ipc->ipc_outgoing_ill;
		mibptr = ill->ill_ip6_mib;
		goto send_from_ill;
	}

	/*
	 * 4. For unicast: Just do an IRE lookup for the best match.
	 * If we get here for a link-local address it is rather random
	 * what interface we pick on a multihomed host.
	 * *If* there is an IRE_CACHE (and the link-local address
	 * isn't duplicated on multi links) this will find the IRE_CACHE.
	 * Otherwise it will use one of the matching IRE_INTERFACE routes
	 * for the link-local prefix. Hence, applications
	 * *should* be encouraged to specify an outgoing interface when sending
	 * to a link local address.
	 */
	if (ipc == NULL || (ipc->ipc_ulp == IPPROTO_TCP &&
	    !ipc->ipc_fully_bound)) {
		/*
		 * We cache IRE_CACHEs to avoid lookups. We don't do
		 * this for the tcp global queue and listen end point
		 * as it does not really have a real destination to
		 * talk to.
		 */
		ire = ire_cache_lookup_v6(v6dstp);
	} else {
		/*
		 * IRE_MARK_CONDEMNED is marked in ire_delete. We don't
		 * grab a lock here to check for CONDEMNED as it is okay
		 * to send a packet or two with the IRE_CACHE that is going
		 * away.
		 */
		mutex_enter(&ipc->ipc_irc_lock);
		ire = ipc->ipc_ire_cache;
		if (ire != NULL &&
		    IN6_ARE_ADDR_EQUAL(&ire->ire_addr_v6, v6dstp) &&
		    !(ire->ire_marks & IRE_MARK_CONDEMNED)) {

			IRE_REFHOLD(ire);
			mutex_exit(&ipc->ipc_irc_lock);

		} else {
			ipc->ipc_ire_cache = NULL;
			mutex_exit(&ipc->ipc_irc_lock);
			if (ire != NULL)
				IRE_REFRELE(ire);
			ire = (ire_t *)ire_cache_lookup_v6(v6dstp);
			if (ire != NULL) {
				mutex_enter(&ipc->ipc_irc_lock);
				if (ipc->ipc_ire_cache == NULL) {
					ipc->ipc_ire_cache = ire;
					IRE_REFHOLD(ire);
				}
				mutex_exit(&ipc->ipc_irc_lock);
			}
		}
	}

	if (ire != NULL) {
		if (do_outrequests) {
			mibptr = ire->ire_ipif->ipif_ill->ill_ip6_mib;
			BUMP_MIB(mibptr->ipv6OutRequests);
		}
		ip_wput_ire_v6(q, mp, ire, unspec_src, cksum_request, ipc);
		IRE_REFRELE(ire);
		return;
	}

	/*
	 * No full IRE for this destination.  Send it to
	 * ip_newroute_v6 to see if anything else matches.
	 * Mark this packet as having originated on this
	 * machine.
	 * Update rptr if there was an ip6i_t header.
	 */
	mp->b_prev = NULL;
	mp->b_next = NULL;
	if (ip6i != NULL)
		mp->b_rptr -= sizeof (ip6i_t);

	if (unspec_src) {
		if (ip6i == NULL) {
			/*
			 * Add ip6i_t header to carry unspec_src
			 * until the packet comes back in ip_wput_v6.
			 */
			mp = ip_add_info_v6(mp, NULL, v6dstp);
			if (mp == NULL) {
				if (do_outrequests)
					BUMP_MIB(mibptr->ipv6OutRequests);
				BUMP_MIB(mibptr->ipv6OutDiscards);
				return;
			}
			ip6i = (ip6i_t *)mp->b_rptr;
			/* ndp_resolver code assumes that mp is pulled up */
			if ((mp->b_wptr - (uchar_t *)ip6i) ==
			    sizeof (ip6i_t)) {
				if (!pullupmsg(mp, -1)) {
					ip1dbg(("ip_wput_v6: pullupmsg"
					    " failed\n"));
					if (do_outrequests)
						BUMP_MIB(mibptr->
						    ipv6OutRequests);
					BUMP_MIB(mibptr->ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				v6dstp = &ip6h->ip6_dst;
				ip6i = (ip6i_t *)ip6h;
			}
			ip6h = (ip6_t *)&ip6i[1];
		}
		ip6i->ip6i_flags |= IP6I_UNSPEC_SRC;
	}
	if (do_outrequests)
		BUMP_MIB(mibptr->ipv6OutRequests);
	ip_newroute_v6(q, mp, v6dstp, &ip6h->ip6_src, NULL);
	return;


	/*
	 * Handle multicast packets with or without an ipc.
	 * Assumes that the transports set ip6_hops taking
	 * IPV6_MULTICAST_HOPS (and the other ways to set the hoplimit)
	 * into account.
	 */
ipv6multicast:
	ip2dbg(("ip_wput_v6: multicast\n"));

	/* 1. If an ip6i_t with IP6I_IFINDEX set then use that ill. */
	if (ip6i != NULL && (ip6i->ip6i_flags & IP6I_IFINDEX)) {
		ASSERT(ill != NULL);
		ipif = ill->ill_ipif;	/* Link local ipif */
	}
	/*
	 * 2. If q is an ill queue and (link local or multicast destination)
	 * then use that ill.
	 */
	else if (ill != NULL) {
		ipif = ill->ill_ipif;	/* Link local ipif */
	} else if (ipc != NULL) {
		/* 3. If IPV6_BOUND_IF has been set use that ill. */
		if (ipc->ipc_outgoing_ill != NULL) {
			ill = ipc->ipc_outgoing_ill;
			ipif = ill->ill_ipif;	/* Link local ipif */
		}
		/*
		 * 4. For multicast: if IPV6_MULTICAST_IF has been set use it.
		 * Otherwise look for the best IRE match for the unspecified
		 * group to determine the ill.
		 *
		 * ipc_multicast_{ipif,ill} are shared between
		 * IPv4 and IPv6 and AF_INET6 sockets can
		 * send both IPv4 and IPv6 packets. Hence
		 * we have to check that "isv6" matches.
		 */
		else if (ipc->ipc_multicast_ill != NULL &&
		    ipc->ipc_multicast_ill->ill_isv6) {
			ill = ipc->ipc_multicast_ill;
			ipif = ipc->ipc_multicast_ipif;
		} else {
			ipif = ipif_lookup_group_v6(&ipv6_unspecified_group);
			if (ipif == NULL) {
				ip1dbg(("ip_wput_v6: No ipif for multicast\n"));
				if (do_outrequests)
					BUMP_MIB(mibptr->ipv6OutRequests);
				BUMP_MIB(mibptr->ipv6OutDiscards);
				freemsg(mp);
				return;
			}
			ill = ipif->ipif_ill;

			/* Save binding until IPV6_MULTICAST_IF changes it */
			ipc->ipc_multicast_ipif = ipif;
			ipc->ipc_multicast_ill = ill;
		}
	}

	ASSERT(ill != NULL);
	mibptr = ill->ill_ip6_mib;
	if (do_outrequests) {
		BUMP_MIB(mibptr->ipv6OutRequests);
		do_outrequests = B_FALSE;
	}
	/*
	 * Do loopback multicast here to handle both the cases
	 * when we find an IRE_CACHE and when we go through
	 * ip_newroute_multi_v6. When we return from ip_newroute_multi_v6
	 * we arrive on an ill queue thuse there will be no loopback
	 * check then.
	 */
	if (ipc != NULL && ipc->ipc_multicast_loop &&
	    ilm_lookup_ill_v6(ill, &ip6h->ip6_dst) != NULL) {
		mblk_t *nmp;

		ip1dbg(("ip_wput_v6: Loopback multicast\n"));
		nmp = copymsg(mp);
		if (nmp) {
			ip6_t	*nip6h;

			nip6h = (ip6_t *)nmp->b_rptr;
			if (IN6_IS_ADDR_UNSPECIFIED(&nip6h->ip6_src) &&
			    !unspec_src)
				nip6h->ip6_src = ipif->ipif_v6src_addr;
			/*
			 * Deliver locally on the read queue of the interface
			 * this packet would be sent out on.
			 * Avoid any transport checksum checks since the
			 * checksums have not yet been computed.
			 */
			ip_rput_data_v6(q, ill, nmp, nip6h, B_TRUE, B_FALSE);
		} else {
			BUMP_MIB(mibptr->ipv6OutDiscards);
			ip1dbg(("ip_wput_v6: copymsg failed\n"));
		}
	}
	if (ip6h->ip6_hops == 0 ||
	    IN6_IS_ADDR_MC_NODELOCAL(v6dstp)) {
		BUMP_MIB(mibptr->ipv6OutMcastPkts);
		ip1dbg(("ip_wput_v6: local multicast only\n"));
		freemsg(mp);
		return;
	}

send_from_ill:
	ASSERT(ill != NULL);
	ASSERT(mibptr == ill->ill_ip6_mib);
	if (do_outrequests) {
		BUMP_MIB(mibptr->ipv6OutRequests);
		do_outrequests = B_FALSE;
	}
	if (IN6_IS_ADDR_MULTICAST(v6dstp))
		BUMP_MIB(mibptr->ipv6OutMcastPkts);

	/*
	 * When a specific ill is specified (using IPV6_PKTINFO,
	 * IPV6_MULTICAST_IF, or IPV6_BOUND_IF) we will only match
	 * on routing entries (ftable and ctable) that have a matching
	 * ire->ire_ipif->ipif_ill. Thus this can only be used
	 * for destinations that are on-link for the specific ill
	 * and that can appear on multiple links. Thus it is useful
	 * for multicast destinations, link-local destinations, and
	 * at some point perhaps for site-local destinations (if the
	 * node sits at a site boundary).
	 * We create the cache entries in the regular ctable since
	 * it can not "confuse" things for other destinations.
	 * table.
	 *
	 * NOTE : ipc_ire_cache is not used for caching ire_ctable_lookups.
	 *	  It is used only when ire_cache_lookup is used above.
	 */
	ire = ire_ctable_lookup_v6(v6dstp, 0, 0, ill->ill_ipif, NULL,
	    MATCH_IRE_ILL);
	if (ire != NULL) {
		ip1dbg(("ip_wput_v6: send on %s, ire = %p, ill index = %d\n",
		    ill->ill_name, (void *)ire, ill->ill_index));

		ip_wput_ire_v6(q, mp, ire, unspec_src, cksum_request, ipc);
		ire_refrele(ire);
		return;
	}

	/* Update rptr if there was an ip6i_t header. */
	if (ip6i != NULL)
		mp->b_rptr -= sizeof (ip6i_t);
	if (unspec_src) {
		if (ip6i == NULL) {
			/*
			 * Add ip6i_t header to carry unspec_src
			 * until the packet comes back in ip_wput_v6.
			 */
			mp = ip_add_info_v6(mp, NULL, v6dstp);
			if (mp == NULL) {
				BUMP_MIB(mibptr->ipv6OutDiscards);
				return;
			}
			ip6i = (ip6i_t *)mp->b_rptr;
			/* ndp_resolver code assumes that mp is pulled up */
			if ((mp->b_wptr - (uchar_t *)ip6i) ==
			    sizeof (ip6i_t)) {
				if (!pullupmsg(mp, -1)) {
					ip1dbg(("ip_wput_v6: pullupmsg"
					    " failed\n"));
					BUMP_MIB(mibptr->ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
				v6dstp = &ip6h->ip6_dst;
				ip6i = (ip6i_t *)ip6h;
			}
			ip6h = (ip6_t *)&ip6i[1];
		}
		ip6i->ip6i_flags |= IP6I_UNSPEC_SRC;
	}
	if (IN6_IS_ADDR_MULTICAST(v6dstp)) {
		ip_newroute_multi_v6(q, mp, ill->ill_ipif, v6dstp,
		    unspec_src);
	} else {
		ip_newroute_v6(q, mp, v6dstp, &ip6h->ip6_src, ill);
	}
	return;

notv6:
	/*
	 * XXX implement a IPv4 and IPv6 packet counter per ipc and
	 * switch when ratio exceeds e.g. 10:1
	 */
	if (q->q_next == NULL) /* Avoid ill queue */
		ip_setqinfo(RD(q), B_FALSE, B_TRUE);
	BUMP_MIB(mibptr->ipv6OutIPv4);
	ip_wput(q, mp);
}

/*
 * Send packet using IRE.
 * Checksumming is controlled by cksum_request:
 *	-1 => normal i.e. TCP, UDP, ICMPv6 are checksummed and nothing else.
 *	1 => Skip TCP/UDP checksum
 * 	Otherwise => checksum_request contains insert offset for checksum
 *
 * Assumes that the following set of headers appear in the first
 * mblk:
 *	ip6_t
 *	Any extension headers
 *	TCP/UDP header (if present)
 * The routine can handle an ICMPv6 header that is not in the first mblk.
 *
 * NOTE : This function does not ire_refrele the ire passed in as the
 *	  argument unlike ip_wput_ire where the REFRELE is done.
 *	  Refer to ip_wput_ire for more on this.
 */
static void
ip_wput_ire_v6(queue_t *q, mblk_t *mp, ire_t *ire, int unspec_src,
    int cksum_request, ipc_t *ipc)
{
	ip6_t		*ip6h;
	uint8_t		nexthdr;
	uint_t		nexthdr_offset;
	uint16_t	hdr_length;
	uint_t		reachable = 0x0;
	ill_t		*ill = ire->ire_ipif->ipif_ill;

	ip6h = (ip6_t *)mp->b_rptr;

	if (IN6_IS_ADDR_UNSPECIFIED(&ip6h->ip6_src) && !unspec_src) {
		/*
		 * The ire_src_addr always contains a useable source
		 * address for the destination (based on source address
		 * selection rules with respect to address scope as well
		 * as deprecated vs. preferred addresses).
		 */
		ip6h->ip6_src = ire->ire_src_addr_v6;
	}

	/* Fastpath */
	nexthdr = ip6h->ip6_nxt;
	switch (nexthdr) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
	case IPPROTO_ICMPV6:
		hdr_length = IPV6_HDR_LEN;
		nexthdr_offset = (uint_t)((uchar_t *)&ip6h->ip6_nxt -
		    (uchar_t *)ip6h);
		break;
	default: {
		uint8_t	*nexthdrp;

		if (!ip_hdr_length_nexthdr_v6(mp, ip6h,
		    &hdr_length, &nexthdrp)) {
			/* Malformed packet */
			BUMP_MIB(ill->ill_ip6_mib->ipv6OutDiscards);
			freemsg(mp);
			return;
		}
		nexthdr = *nexthdrp;
		nexthdr_offset = nexthdrp - (uint8_t *)ip6h;
		break;
	}
	}

	if (ire->ire_stq != NULL) {
		uint32_t	sum;

		/*
		 * non-NULL send-to queue - packet is to be sent
		 * out an interface.
		 */

		/*
		 * Look for reachability confirmations from the transport.
		 */
		if (ip6h->ip6_vcf & IP_FORWARD_PROG) {
			reachable |= IPV6_REACHABILITY_CONFIRMATION;
			ip6h->ip6_vcf &= ~IP_FORWARD_PROG;
		}
		if (cksum_request != -1 && nexthdr != IPPROTO_ICMPV6) {
			uint16_t	*up;
			uint16_t	*insp;

			if (cksum_request == 1) {
				/* Skip the transport checksum */
				goto cksum_done;
			}
			/*
			 * Do user-configured raw checksum.
			 * Compute checksum and insert at offset "cksum_request"
			 */

			/* check for enough headers for checksum */
			cksum_request += hdr_length;	/* offset from rptr */
			if ((mp->b_wptr - mp->b_rptr) <
			    (cksum_request + sizeof (int16_t))) {
				if (!pullupmsg(mp,
				    cksum_request + sizeof (int16_t))) {
					ip1dbg(("ip_wput_v6: ICMP hdr pullupmsg"
					    " failed\n"));
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
			}
			insp = (uint16_t *)((uchar_t *)ip6h + cksum_request);
			ASSERT(((uintptr_t)insp & 0x1) == 0);
			up = (uint16_t *)&ip6h->ip6_src;
			/*
			 * icmp has placed length and routing
			 * header adjustment in *insp.
			 */
			sum = htons(nexthdr) +
			    up[0] + up[1] + up[2] + up[3] +
			    up[4] + up[5] + up[6] + up[7] +
			    up[8] + up[9] + up[10] + up[11] +
			    up[12] + up[13] + up[14] + up[15];
			sum = (sum & 0xffff) + (sum >> 16);
			*insp = IP_CSUM(mp, hdr_length, sum);
		} else if (nexthdr == IPPROTO_TCP) {
			uint16_t	*up;

			/*
			 * Check for full IPv6 header + enough TCP header
			 * to get at the checksum field.
			 * XXX need hardware checksum support.
			 */
#define	TCP_CSUM_OFFSET	16
#define	TCP_CSUM_SIZE	2
			if ((mp->b_wptr - mp->b_rptr) <
			    (hdr_length + TCP_CSUM_OFFSET + TCP_CSUM_SIZE)) {
				if (!pullupmsg(mp, hdr_length +
				    TCP_CSUM_OFFSET + TCP_CSUM_SIZE)) {
					ip1dbg(("ip_wput_v6: TCP hdr pullupmsg"
					    " failed\n"));
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
			}

			up = (uint16_t *)&ip6h->ip6_src;
			/*
			 * Note: The TCP module has stored the length value
			 * into the tcp checksum field, so we don't
			 * need to explicitly sum it in here.
			 */
			if (hdr_length == IPV6_HDR_LEN) {
				/* src, dst, tcp consequtive */
				up = (uint16_t *)(((uchar_t *)ip6h) +
				    IPV6_HDR_LEN + TCP_CSUM_OFFSET);
				*up = IP_CSUM(mp,
				    IPV6_HDR_LEN - 2 * sizeof (in6_addr_t),
				    htons(IPPROTO_TCP));
			} else {
				sum = htons(IPPROTO_TCP) +
				    up[0] + up[1] + up[2] + up[3] +
				    up[4] + up[5] + up[6] + up[7] +
				    up[8] + up[9] + up[10] + up[11] +
				    up[12] + up[13] + up[14] + up[15];
				/*
				 * Fold the initial sum.
				 */
				sum = (sum & 0xffff) + (sum >> 16);
				up = (uint16_t *)(((uchar_t *)ip6h) +
				    hdr_length + TCP_CSUM_OFFSET);
				*up = IP_CSUM(mp, hdr_length, sum);
			}
#undef TCP_CSUM_OFFSET
#undef TCP_CSUM_SIZE

		} else if (nexthdr == IPPROTO_UDP) {
			uint16_t	*up;

			/*
			 * check for full IPv6 header + enough UDP header
			 * to get at the UDP checksum field
			 */
#define	UDP_CSUM_OFFSET	6
#define	UDP_CSUM_SIZE	2
			if ((mp->b_wptr - mp->b_rptr) < (hdr_length +
			    UDP_CSUM_OFFSET + UDP_CSUM_SIZE)) {
				if (!pullupmsg(mp, hdr_length +
				    UDP_CSUM_OFFSET + UDP_CSUM_SIZE)) {
					ip1dbg(("ip_wput_v6: UDP hdr pullupmsg"
					    " failed\n"));
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
			}
			up = (uint16_t *)&ip6h->ip6_src;
			/*
			 * Note: The UDP module has stored the length value
			 * into the udp checksum field, so we don't
			 * need to explicitly sum it in here.
			 */
			if (hdr_length == IPV6_HDR_LEN) {
				/* src, dst, udp consequtive */
				up = (uint16_t *)(((uchar_t *)ip6h) +
				    IPV6_HDR_LEN + UDP_CSUM_OFFSET);
				*up = IP_CSUM(mp,
				    IPV6_HDR_LEN - 2 * sizeof (in6_addr_t),
				    htons(IPPROTO_UDP));
			} else {
				sum = htons(IPPROTO_UDP) +
				    up[0] + up[1] + up[2] + up[3] +
				    up[4] + up[5] + up[6] + up[7] +
				    up[8] + up[9] + up[10] + up[11] +
				    up[12] + up[13] + up[14] + up[15];
				sum = (sum & 0xffff) + (sum >> 16);
				up = (uint16_t *)(((uchar_t *)ip6h) +
				    hdr_length + UDP_CSUM_OFFSET);
				*up = IP_CSUM(mp, hdr_length, sum);
			}
#undef UDP_CSUM_OFFSET
#undef UDP_CSUM_SIZE
		} else if (nexthdr == IPPROTO_ICMPV6) {
			uint16_t	*up;
			icmp6_t *icmp6;

			/* check for full IPv6+ICMPv6 header */
			if ((mp->b_wptr - mp->b_rptr) <
			    (hdr_length + ICMP6_MINLEN)) {
				if (!pullupmsg(mp, hdr_length + ICMP6_MINLEN)) {
					ip1dbg(("ip_wput_v6: ICMP hdr pullupmsg"
					    " failed\n"));
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
			}
			icmp6 = (icmp6_t *)((uchar_t *)ip6h + hdr_length);
			up = (uint16_t *)&ip6h->ip6_src;
			/*
			 * icmp has placed length and routing
			 * header adjustment in icmp6_cksum.
			 */
			sum = htons(IPPROTO_ICMPV6) +
			    up[0] + up[1] + up[2] + up[3] +
			    up[4] + up[5] + up[6] + up[7] +
			    up[8] + up[9] + up[10] + up[11] +
			    up[12] + up[13] + up[14] + up[15];
			sum = (sum & 0xffff) + (sum >> 16);
			icmp6->icmp6_cksum = IP_CSUM(mp, hdr_length, sum);
			/* Update output mib stats */
			icmp_update_out_mib_v6(ill, icmp6);
		}

	cksum_done:
		if (ntohs(ip6h->ip6_plen) + IPV6_HDR_LEN > ire->ire_max_frag ||
		    (ire->ire_frag_flag & IPH_FRAG_HDR)) {
			if (ntohs(ip6h->ip6_plen) + IPV6_HDR_LEN !=
			    (mp->b_cont ? msgdsize(mp) :
			    mp->b_wptr - (uchar_t *)ip6h)) {
				ip0dbg(("Packet length mismatch: %d, %ld\n",
				    ntohs(ip6h->ip6_plen) + IPV6_HDR_LEN,
				    msgdsize(mp)));
				freemsg(mp);
				return;
			}
			ASSERT((queue_t *)mp->b_prev == NULL);
			ip2dbg(("Fragmenting Size = %d, mtu = %d\n",
			    ntohs(ip6h->ip6_plen) +
			    IPV6_HDR_LEN, ire->ire_max_frag));
			ip_wput_frag_v6(mp, ire, reachable, ipc);
			return;
		}
		/*
		 * XXX multicast: add ip_mforward_v6() here.
		 * Check ipc_dontroute
		 */
		ire->ire_ob_pkt_count++;
		ip_xmit_v6(mp, ire, reachable, ipc);
	} else {
		/*
		 * NULL send-to queue - packet is to be delivered locally.
		 */
		uint32_t	ports;

		ire->ire_ob_pkt_count++;

		/*
		 * Remove reacability confirmation bit from version field
		 * before looping back the packet.
		 */
		if (ip6h->ip6_vcf & IP_FORWARD_PROG) {
			ip6h->ip6_vcf &= ~IP_FORWARD_PROG;
		}

		switch (nexthdr) {
		case IPPROTO_TCP:
			if (mp->b_datap->db_type == M_DATA) {
				/*
				 * M_DATA mblk, so init mblk (chain) for
				 * no struio().
				 */
				mblk_t  *mp1 = mp;

				do {
					mp1->b_datap->db_struioflag = 0;
				} while ((mp1 = mp1->b_cont) != NULL);
			}
			ports = *(uint32_t *)(mp->b_rptr + hdr_length +
			    TCP_PORTS_OFFSET);
			ip_fanout_tcp_v6(q, mp, ip6h, ports, ill,
			    IP_FF_SEND_ICMP|IP_FF_SYN_ADDIRE|IP_FF_IP6INFO,
			    hdr_length);
			return;

		case IPPROTO_UDP:
			ports = *(uint32_t *)(mp->b_rptr + hdr_length +
			    UDP_PORTS_OFFSET);
			ip_fanout_udp_v6(q, mp, ip6h, ports, ill,
			    IP_FF_SEND_ICMP|IP_FF_IP6INFO);
			return;

		case IPPROTO_ICMPV6: {
			icmp6_t *icmp6;
			mblk_t	*mp1;

			/* check for full IPv6+ICMPv6 header */
			if ((mp->b_wptr - mp->b_rptr) <
			    (hdr_length + ICMP6_MINLEN)) {
				if (!pullupmsg(mp, hdr_length + ICMP6_MINLEN)) {
					ip1dbg(("ip_wput_v6: ICMP hdr pullupmsg"
					    " failed\n"));
					BUMP_MIB(ill->ill_ip6_mib->
					    ipv6OutDiscards);
					freemsg(mp);
					return;
				}
				ip6h = (ip6_t *)mp->b_rptr;
			}
			icmp6 = (icmp6_t *)((uchar_t *)ip6h + hdr_length);

			/* Update output mib stats */
			icmp_update_out_mib_v6(ill, icmp6);

			/* Check variable for testing applications */
			if (ipv6_drop_inbound_icmpv6) {
				freemsg(mp);
				return;
			}
			/*
			 * Assume that there is always at least one ipc for
			 * ICMPv6 (in.ndpd) i.e. don't optimize the case
			 * where there is no ipc.
			 */
			mp1 = copymsg(mp);
			if (mp1)
				icmp_inbound_v6(q, mp1, ill, hdr_length);
		}
		/* FALLTHRU */
		default: {
			/*
			 * Handle protocols with which IPv6 is less intimate.
			 */
			uint_t flags = IP_FF_RAWIP|IP_FF_IP6INFO;

			/*
			 * Enable sending ICMP for "Unknown" nexthdr
			 * case. i.e. where we did not FALLTHRU from
			 * IPPROTO_ICMPV6 processing case above.
			 */
			if (nexthdr != IPPROTO_ICMPV6)
				flags |= IP_FF_SEND_ICMP;
			/*
			 * Note: There can be more than one stream bound
			 * to a particular protocol. When this is the case,
			 * each one gets a copy of any incoming packets.
			 */
			ip_fanout_proto_v6(q, mp, ip6h, ill, nexthdr,
			    nexthdr_offset, flags);
			return;
		}
		}
	}
}

/*
 * IPv6 fragmentation.  Essentially the same as IPv4 fragmentation.
 * We have not optimized this in terms of number of mblks
 * allocated. For instance, for each fragment sent we always allocate a
 * mblk to hold the IPv6 header and fragment header.
 *
 * Assumes that all the extension headers are contained in the first mblk.
 *
 * The fragment header is inserted after an hop-by-hop options header
 * and after [an optional destinations header followed by] a routing header.
 *
 * NOTE : This function does not ire_refrele the ire passed in as
 * the argument.
 */
static void
ip_wput_frag_v6(mblk_t *mp, ire_t *ire, int reachable, ipc_t *ipc)
{
	ip6_t		*ip6h = (ip6_t *)mp->b_rptr;
	ip6_t		*fip6h;
	mblk_t		*hmp;
	mblk_t		*hmp0;
	mblk_t		*dmp;
	ip6_frag_t	*fraghdr;
	size_t		ip_frag_hdr_len;
	size_t		len;
	size_t		mlen;
	size_t		max_chunk;
	uint32_t	ident;
	uint16_t	off_flags;
	uint16_t	offset = 0;
	ill_t		*ill = ire->ire_ipif->ipif_ill;
	uint8_t		nexthdr;
	uint_t		prev_nexthdr_offset;
	uint8_t		*ptr;

	/*
	 * Determine length of hop-by-hop, routing headers, and
	 * any hbh and routing hdrs only
	 */
	nexthdr = ip6h->ip6_nxt;
	prev_nexthdr_offset = (uint8_t *)&ip6h->ip6_nxt - (uint8_t *)ip6h;
	ptr = (uint8_t *)&ip6h[1];
	if (nexthdr == IPPROTO_HOPOPTS) {
		ip6_hbh_t	*hbh_hdr;
		uint_t		hdr_len;

		hbh_hdr = (ip6_hbh_t *)ptr;
		hdr_len = 8 * (hbh_hdr->ip6h_len + 1);
		nexthdr = hbh_hdr->ip6h_nxt;
		prev_nexthdr_offset = (uint8_t *)&hbh_hdr->ip6h_nxt
		    - (uint8_t *)ip6h;
		ptr += hdr_len;
	}
	if (nexthdr == IPPROTO_DSTOPTS) {
		ip6_dest_t	*dest_hdr;
		uint_t		hdr_len;

		dest_hdr = (ip6_dest_t *)ptr;
		if (dest_hdr->ip6d_nxt == IPPROTO_ROUTING) {
			hdr_len = 8 * (dest_hdr->ip6d_len + 1);
			nexthdr = dest_hdr->ip6d_nxt;
			prev_nexthdr_offset = (uint8_t *)&dest_hdr->ip6d_nxt
			    - (uint8_t *)ip6h;
			ptr += hdr_len;
		}
	}
	if (nexthdr == IPPROTO_ROUTING) {
		ip6_rthdr_t	*rthdr;
		uint_t		hdr_len;

		rthdr = (ip6_rthdr_t *)ptr;
		nexthdr = rthdr->ip6r_nxt;
		prev_nexthdr_offset = (uint8_t *)&rthdr->ip6r_nxt
		    - (uint8_t *)ip6h;
		hdr_len = 8 * (rthdr->ip6r_len + 1);
		ptr += hdr_len;
	}
	ip_frag_hdr_len = (uint_t)(ptr - (uint8_t *)ip6h);

	/*
	 * Allocate for the IP headers, pre-frag headers, the fragment header
	 * and room for a link-layer header.
	 */
	hmp = allocb(ip_frag_hdr_len + sizeof (ip6_frag_t) + ip_wroff_extra,
	    BPRI_HI);
	if (hmp == NULL) {
		BUMP_MIB(ill->ill_ip6_mib->ipv6OutFragFails);
		freemsg(mp);
		return;
	}
	hmp->b_rptr += ip_wroff_extra;
	hmp->b_wptr = hmp->b_rptr + ip_frag_hdr_len + sizeof (ip6_frag_t);

	fip6h = (ip6_t *)hmp->b_rptr;
	fraghdr = (ip6_frag_t *)(hmp->b_rptr + ip_frag_hdr_len);

	bcopy(ip6h, fip6h, ip_frag_hdr_len);
	hmp->b_rptr[prev_nexthdr_offset] = IPPROTO_FRAGMENT;

	ident = atomic_add_32_nv(&ire->ire_ident, 1);

	fraghdr->ip6f_nxt = nexthdr;
	fraghdr->ip6f_reserved = 0;
	fraghdr->ip6f_offlg = htons(0);
	fraghdr->ip6f_ident = htonl(ident);

	len = ntohs(ip6h->ip6_plen);

	max_chunk = (ire->ire_max_frag - ip_frag_hdr_len -
	    sizeof (ip6_frag_t)) & ~7;

	/*
	 * Move read ptr past unfragmentable portion, we don't want this part
	 * of the data in our fragments.
	 */
	mp->b_rptr += ip_frag_hdr_len;

	while (len != 0) {
		mlen = MIN(len, max_chunk);
		len -= mlen;
		if (len != 0) {
			/* Not last */
			hmp0 = copyb(hmp);
			if (hmp0 == NULL) {
				freeb(hmp);
				freemsg(mp);
				BUMP_MIB(ill->ill_ip6_mib->ipv6OutFragFails);
				ip1dbg(("ip_wput_frag_v6: copyb failed\n"));
				return;
			}
			off_flags = IP6F_MORE_FRAG;
		} else {
			/* Last fragment */
			hmp0 = hmp;
			hmp = NULL;
			off_flags = 0;
		}
		fip6h = (ip6_t *)(hmp0->b_rptr);
		fraghdr = (ip6_frag_t *)(hmp0->b_rptr + ip_frag_hdr_len);

		fip6h->ip6_plen = htons((uint16_t)(mlen +
		    ip_frag_hdr_len - IPV6_HDR_LEN + sizeof (ip6_frag_t)));
		/*
		 * Note: Optimization alert.
		 * In IPv6 (and IPv4) protocol header, Fragment Offset
		 * ("offset") is 13 bits wide and in 8-octet units.
		 * In IPv6 protocol header (unlike IPv4) in a 16 bit field,
		 * it occupies the most significant 13 bits.
		 * (least significant 13 bits in IPv4).
		 * We do not do any shifts here. Not shifting is same effect
		 * as taking offset value in octet units, dividing by 8 and
		 * then shifting 3 bits left to line it up in place in proper
		 * place protocol header.
		 */
		fraghdr->ip6f_offlg = htons(offset) | off_flags;

		if (!(dmp = ip_carve_mp(&mp, mlen))) {
			/* mp has already been freed by ip_carve_mp() */
			if (hmp != NULL)
				freeb(hmp);
			freeb(hmp0);
			ip1dbg(("ip_carve_mp: failed\n"));
			BUMP_MIB(ill->ill_ip6_mib->ipv6OutFragFails);
			return;
		}
		hmp0->b_cont = dmp;
		ire->ire_ob_pkt_count++;
		ip_xmit_v6(hmp0, ire, reachable, ipc);
		reachable = 0;	/* No need to redo state machine in loop */
		BUMP_MIB(ill->ill_ip6_mib->ipv6OutFragCreates);
		offset += mlen;
	}
	BUMP_MIB(ill->ill_ip6_mib->ipv6OutFragOKs);
}

/*
 * Determine if the ill and multicast aspects of that packets
 * "matches" the ipc.
 */
static boolean_t
ipc_wantpacket_v6(ipc_t *ipc, ill_t *ill, const in6_addr_t *v6dst_ptr)
{
	if (ipc->ipc_incoming_ill != NULL &&
	    ipc->ipc_incoming_ill != ill)
		return (B_FALSE);

	if (!IN6_IS_ADDR_MULTICAST(v6dst_ptr) || ipc->ipc_multi_router)
		return (B_TRUE);
	return (ilg_lookup_ill_v6(ipc, v6dst_ptr, ill) != NULL);
}


/*
 * Transmit a packet and update any NUD state based on the flags
 * XXX need to "recover" any ip6i_t when doing putq!
 *
 * NOTE : This function does not ire_refrele the ire passed in as the
 * argument.
 */
static void
ip_xmit_v6(mblk_t *mp, ire_t *ire, uint_t flags, ipc_t *ipc)
{
	mblk_t		*mp1;
	nce_t		*nce = ire->ire_nce;
	ill_t		*ill;
	uint64_t	delta;
	ip6_t		*ip6h = (ip6_t *)mp->b_rptr;
	queue_t		*stq = ire->ire_stq;

	ASSERT(!IN6_IS_ADDR_V4MAPPED(&ire->ire_addr_v6));
	ASSERT(ire->ire_ipversion == IPV6_VERSION);
	ASSERT(nce != NULL);
	ASSERT(mp->b_datap->db_type == M_DATA);

	ill = ire_to_ill(ire);
	if (!ill) {
		ip0dbg(("ip_xmit_v6: ire_to_ill failed\n"));
		freemsg(mp);
		return;
	}

	/*
	 * Check for fastpath, we need to hold nce_lock to prevent
	 * fastpath update from chaining nce_fp_mp.
	 */
	mutex_enter(&nce->nce_lock);
	if ((mp1 = nce->nce_fp_mp) != NULL) {
		uint32_t	hlen = mp1->b_wptr - mp1->b_rptr;
		uchar_t		*rptr = mp->b_rptr - hlen;

		/* make sure there is room for the fastpath datalink header */
		if (rptr < mp->b_datap->db_base) {
			mp1 = copyb(mp1);
			if (mp1 == NULL) {
				mutex_exit(&nce->nce_lock);
				BUMP_MIB(ill->ill_ip6_mib->ipv6OutDiscards);
				freemsg(mp);
				return;
			}
			mp1->b_cont = mp;
			mp = mp1;
		} else {
			mp->b_rptr = rptr;
			/* fastpath -  pre-pend datalink header */
			bcopy(mp1->b_rptr, rptr, hlen);
		}
		mutex_exit(&nce->nce_lock);
	} else {
		mutex_exit(&nce->nce_lock);
		mp1 = nce->nce_res_mp;
		if (mp1 == NULL) {
			ip1dbg(("ip_xmit_v6: No resolution block ire = %p\n",
			    (void *)ire));
			freemsg(mp);
			return;
		}
		/*
		 * Prepend the DL_UNITDATA_REQ.
		 */
		mp1 = copyb(mp1);
		if (!mp1) {
			BUMP_MIB(ill->ill_ip6_mib->ipv6OutDiscards);
			freemsg(mp);
			return;
		}
		mp1->b_cont = mp;
		mp = mp1;
	}

	/* Send it down */
	if (ip6h->ip6_nxt == IPPROTO_TCP) {
		putnext(stq, mp);
	} else if (canput(stq->q_next)) {
		putnext(stq, mp);
	} else {
		/*
		 * Queue packet if we have an ipc to give back pressure.
		 */
		if (ip_output_queue && ipc != NULL) {
			if (ipc->ipc_draining) {
				ipc->ipc_did_putbq = 1;
				(void) putbq(ipc->ipc_wq, mp);
			} else
				(void) putq(ipc->ipc_wq, mp);
			return;
		}
		BUMP_MIB(ill->ill_ip6_mib->ipv6OutDiscards);
		freemsg(mp);
		return;
	}

	if (nce->nce_flags & (NCE_F_NONUD|NCE_F_PERMANENT)) {
		return;
	}

	ASSERT(nce->nce_state != ND_INCOMPLETE);

	/*
	 * Check for upper layer advice
	 */
	if (flags & IPV6_REACHABILITY_CONFIRMATION) {
		/*
		 * It should be o.k. to check the state without
		 * a lock here, at most we lose an advice.
		 */
		nce->nce_last = TICK_TO_MSEC(lbolt64);
		if (nce->nce_state != ND_REACHABLE) {

			mutex_enter(&nce->nce_lock);
			nce->nce_state = ND_REACHABLE;
			nce->nce_pcnt = ND_MAX_UNICAST_SOLICIT;
			mi_timer(RD(ire->ire_stq), nce->nce_timer_mp, -1);
			mutex_exit(&nce->nce_lock);
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ip_xmit_v6: state for %s changed"
				    " to REACHABLE\n", AF_INET6,
				    &ire->ire_addr_v6);
			}
		}
		return;
	}

	delta =  TICK_TO_MSEC(lbolt64) - nce->nce_last;
	ip1dbg(("ip_xmit_v6: delta = %" PRId64 " ill_reachable_time = %d \n",
	    delta, ill->ill_reachable_time));
	if (delta > (uint64_t)ill->ill_reachable_time) {
		nce = ire->ire_nce;
		mutex_enter(&nce->nce_lock);
		switch (nce->nce_state) {
		case ND_REACHABLE:
			nce->nce_state = ND_STALE;
			mutex_exit(&nce->nce_lock);
			if (ip_debug > 3) {
				/* ip2dbg */
				pr_addr_dbg("ip_xmit_v6: state for %s changed"
				    " to STALE\n", AF_INET6, &ire->ire_addr_v6);
			}
			break;
		case ND_STALE:
			nce->nce_state = ND_DELAY;
			mi_timer(ill->ill_rq, nce->nce_timer_mp,
					delay_first_probe_time);
			mutex_exit(&nce->nce_lock);
			if (ip_debug > 3) {
				/* ip2dbg */
				pr_addr_dbg("ip_xmit_v6: state for %s changed"
				    " to DELAY\n", AF_INET6, &ire->ire_addr_v6);
			}
			break;
		case ND_DELAY:
		case ND_PROBE:
			mutex_exit(&nce->nce_lock);
			/* Timers have already started */
			break;
		default:
			ASSERT(0);
		}
	}
}

/*
 * inet_ntop -- Convert an IPv4 or IPv6 address in binary form into
 * printable form, and return a pointer to that string. Caller should
 * provide a buffer to store string into.
 * Note: this routine is kernel version of inet_ntop. It has similar
 * format as inet_ntop() defined in rfc2553. But it does not do
 * error handling operations exactly as rfc2553 defines. This function
 * is used by kernel inet directory routines only for debugging.
 * This inet_ntop() function, does not return NULL if third argument
 * is NULL. The reason is simple that we don't want kernel to panic
 * as the output of this function is directly fed to ip<n>dbg macro.
 * Instead it uses a local buffer for destination address for
 * those calls which purposely pass NULL ptr for the destination
 * buffer. This function is thread-safe when the caller passes a non-
 * null buffer with the third argument.
 */
/* ARGSUSED */
char *
inet_ntop(int af, const void *addr, char *buf, int addrlen)
{
	static char local_buf[INET6_ADDRSTRLEN];
	in6_addr_t	*v6addr;
	uchar_t		*v4addr;
	char		*caddr;

	/*
	 * We don't allow thread unsafe inet_ntop calls, they
	 * must pass a non-null buffer pointer. For DEBUG mode
	 * we use the ASSERT() and for non-debug kernel it will
	 * silently allow it for now. Someday we should remove
	 * the static buffer from this function.
	 */

	ASSERT(buf != NULL);
	if (buf == NULL)
		buf = local_buf;

	/* Let user know politely not to send NULL or unaligned addr */
	if (addr == NULL || !(IS_P2ALIGNED(addr, sizeof (uint32_t)))) {
		(void) sprintf(buf, "address is either <null> or unaligned");
		return (buf);
	}


#define	UC(b)	(((int)b) & 0xff)
	switch (af) {
	case AF_INET:
		v4addr = (uchar_t *)addr;
		(void) sprintf(buf, "%03d.%03d.%03d.%03d",
		    UC(v4addr[0]), UC(v4addr[1]), UC(v4addr[2]), UC(v4addr[3]));
		return (buf);

	case AF_INET6:
		v6addr = (in6_addr_t *)addr;
		if (IN6_IS_ADDR_V4MAPPED(v6addr)) {
			caddr = (char *)addr;
			(void) sprintf(buf, "::ffff:%d.%d.%d.%d",
			    UC(caddr[12]), UC(caddr[13]),
			    UC(caddr[14]), UC(caddr[15]));
		} else if (IN6_IS_ADDR_V4COMPAT(v6addr)) {
			caddr = (char *)addr;
			(void) sprintf(buf, "::%d.%d.%d.%d",
			    UC(caddr[12]), UC(caddr[13]), UC(caddr[14]),
			    UC(caddr[15]));
		} else if (IN6_IS_ADDR_UNSPECIFIED(v6addr)) {
			(void) sprintf(buf, "::");
		} else {
			convert2ascii(buf, v6addr);
		}
		return (buf);

	default:
		(void) sprintf(buf, "<unknown family %d>", af);
		return (buf);
	}
#undef UC
}

/*
 *
 * v6 formats supported
 * General format xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
 * The short hand notation :: is used for COMPAT addr
 * Other forms : fe80::xxxx:xxxx:xxxx:xxxx
 */
void
convert2ascii(char *buf, const in6_addr_t *addr)
{
	int		hexdigits;
	int		head_zero = 0;
	int		tail_zero = 0;
	/* tempbuf must be big enough to hold ffff:\0 */
	char		tempbuf[6];
	char		*ptr;
	uint16_t	*addr_component;
	size_t		len;
	boolean_t	first = B_FALSE;
	boolean_t	med_zero = B_FALSE;
	boolean_t	end_zero = B_FALSE;

	addr_component = (uint16_t *)addr;
	ptr = buf;

	/* First count if trailing zeroes higher in number */
	for (hexdigits = 0; hexdigits < 8; hexdigits++) {
		if (*addr_component == 0) {
			if (hexdigits < 4)
				head_zero++;
			else
				tail_zero++;
		}
		addr_component++;
	}
	addr_component = (uint16_t *)addr;
	if (tail_zero > head_zero && (head_zero + tail_zero) != 7)
		end_zero = B_TRUE;

	for (hexdigits = 0; hexdigits < 8; hexdigits++) {

		/* if entry is a 0 */

		if (*addr_component == 0) {
			if (!first && *(addr_component + 1) == 0) {
				if (end_zero && (hexdigits < 4)) {
					*ptr++ = '0';
					*ptr++ = ':';
				} else {
					/*
					 * address starts with 0s ..
					 * stick in leading ':' of pair
					 */
					if (hexdigits == 0)
						*ptr++ = ':';
					/* add another */
					*ptr++ = ':';
					first = B_TRUE;
					med_zero = B_TRUE;
				}
			} else if (first && med_zero) {
				if (hexdigits == 7)
					*ptr++ = ':';
				addr_component++;
				continue;
			} else {
				*ptr++ = '0';
				*ptr++ = ':';
			}
			addr_component++;
			continue;
		}
		if (med_zero)
			med_zero = B_FALSE;

		tempbuf[0] = '\0';
		(void) sprintf(tempbuf, "%x:", ntohs(*addr_component) & 0xffff);
		len = strlen(tempbuf);
		bcopy(tempbuf, ptr, len);
		ptr = ptr + len;
		addr_component++;
	}
	*--ptr = '\0';
}

/*
 * search for char c, terminate on trailing white space
 */
static char *
strchr_w(const char *sp, int c)
{
	/* skip leading white space */
	while (*sp && (*sp == ' ' || *sp == '\t')) {
		sp++;
	}

	do {
		if (*sp == (char)c)
			return ((char *)sp);
		if (*sp == ' ' || *sp == '\t')
			return (NULL);
	} while (*sp++);
	return (NULL);
}

static int
str2inet_addr(char *cp, ipaddr_t *addrp)
{
	char *end;
	int byte;
	int i;
	ipaddr_t addr = 0;

	for (i = 0; i < 4; i++) {
		byte = (int)mi_strtol(cp, &end, 10);
		if (byte < 0 || byte > 255) {
			return (0);
		}
		addr = (addr << 8) | byte;
		if (i < 3) {
			if (*end != '.') {
				return (0);
			} else {
				cp = end + 1;
			}
		} else {
			cp = end;
		}
	}
	*addrp = addr;
	return (1);
}

/*
 * inet_pton: This function takes string format IPv4 or IPv6 address and
 * converts it to binary form. The format of this function corresponds to
 * inet_pton() in the socket library.
 * It returns 0 for invalid IPV4 and IPv6 address
 *            1 when successfully converts ascii to binary
 *            -1 when af is not AF_INET or AF_INET6
 */
int
inet_pton(int af, char *inp, void *outp)
{
	int i;
	int byte;
	char *end;

	switch (af) {
	case AF_INET:
		return (str2inet_addr(inp, (ipaddr_t *)outp));
	case AF_INET6: {
		union v6buf_u {
			uint16_t v6words_u[8];
			in6_addr_t v6addr_u;
		} v6buf, *v6outp;
		uint16_t	*dbl_col = NULL;
		char lastbyte = NULL;

		v6outp = (union v6buf_u *)outp;

		if (strchr_w(inp, '.') != NULL) {
			/* v4 mapped or v4 compatable */
			if (strncmp(inp, "::ffff:", 7) == 0) {
				ipaddr_t ipv4_all_zeroes = 0;
				/* mapped - first init prefix and then fill */
				IN6_IPADDR_TO_V4MAPPED(ipv4_all_zeroes,
				    &v6outp->v6addr_u);
				return (str2inet_addr(inp + 7,
				    &V4_PART_OF_V6(v6outp->v6addr_u)));
			} else if (strncmp(inp, "::", 2) == 0) {
				/* v4 compatable - prefix all zeroes */
				V6_SET_ZERO(v6outp->v6addr_u);
				return (str2inet_addr(inp + 2,
				    &V4_PART_OF_V6(v6outp->v6addr_u)));
			}
			return (0);
		}
		for (i = 0; i < 8; i++) {
			byte = mi_strtol(inp, &end, 16);
			if (byte < 0 || byte > 0x0ffff) {
				return (0);
			}
			v6buf.v6words_u[i] = (uint16_t)byte;
			if (*end == NULL || i == 7) {
				inp = end;
				break;
			}
			if (inp == end) {	/* not a number must be */
				if (*inp == ':' &&
				    ((i == 0 && *(inp + 1) == ':') ||
				    lastbyte == ':')) {
					if (dbl_col) {
						return (0);
					}
					if (byte != 0)
						i++;
					dbl_col = &v6buf.v6words_u[i];
					if (i == 0)
						inp++;
				} else if (*inp == NULL || *inp == ' ' ||
				    *inp == '\t') {
					break;
				} else {
					return (0);
				}
			} else {
				inp = end;
			}
			if (*inp != ':') {
				return (0);
			}
			inp++;
			if (*inp == NULL || *inp == ' ' || *inp == '\t') {
				break;
			}
			lastbyte = *inp;
		}
		if (*inp != NULL && *inp != ' ' && *inp != '\t') {
			return (0);
		}
		/*
		 * v6words now contains the bytes we could translate
		 * dbl_col points to the word (should be 0) where
		 * a double colon was found
		 */
		if (i == 7) {
			v6outp->v6addr_u = v6buf.v6addr_u;
		} else {
			int rem;
			int word;
			int next;
			if (dbl_col == NULL) {
				return (0);
			}
			v6outp->v6addr_u = ipv6_all_zeros;
			rem = dbl_col - &v6buf.v6words_u[0];
			for (next = 0; next < rem; next++) {
				v6outp->v6words_u[next] = v6buf.v6words_u[next];
			}
			next++;	/* skip dbl_col 0 */
			rem = i - rem;
			word = 8 - rem;
			while (rem > 0) {
				v6outp->v6words_u[word] = v6buf.v6words_u[next];
				word++;
				rem--;
				next++;
			}
		}
		return (1);	/* Success */
	}
	}	/* switch */
	return (-1);	/* return -1 for default case */
}


/*
 * pr_addr_dbg function provides the needed buffer space to call
 * inet_ntop() function's 3rd argument. This function should be
 * used by any kernel routine which wants to save INET6_ADDRSTRLEN
 * stack buffer space in it's own stack frame. This function uses
 * a buffer from it's own stack and prints the information.
 * Example: pr_addr_dbg("func: no route for %s\n ", AF_INET, addr)
 *
 * Note:    This function can call inet_ntop() once.
 */
void
pr_addr_dbg(char *fmt1, int af, const void *addr)
{
	char	buf[INET6_ADDRSTRLEN];

	if (fmt1 == NULL) {
		ip0dbg(("pr_addr_dbg: Wrong arguments\n"));
		return;
	}

	/*
	 * This does not compare debug level and just prints
	 * out. Thus it is the responsibility of the caller
	 * to check the appropriate debug-level before calling
	 * this function.
	 */
	if (ip_debug > 0) {
		printf(fmt1, inet_ntop(af, addr, buf, sizeof (buf)));
	}


}


/*
 * Return the length in bytes of the IPv6 headers (base header, ip6i_t
 * if needed and extension headers) that will be needed based on the
 * ip6_pkt_t structure passed by the caller.
 *
 * The returned length does not include the length of the upper level
 * protocol (ULP) header.
 */
int
ip_total_hdrs_len_v6(ip6_pkt_t *ipp)
{
	int len;

	len = IPV6_HDR_LEN;
	if (ipp->ipp_fields & IPPF_HAS_IP6I)
		len += sizeof (ip6i_t);
	if (ipp->ipp_fields & IPPF_HOPOPTS) {
		ASSERT(ipp->ipp_hopoptslen != 0);
		len += ipp->ipp_hopoptslen;
	}
	if (ipp->ipp_fields & IPPF_RTHDR) {
		ASSERT(ipp->ipp_rthdrlen != 0);
		len += ipp->ipp_rthdrlen;
	}
	/*
	 * En-route destination options
	 * Only do them if there's a routing header as well
	 */
	if ((ipp->ipp_fields & (IPPF_RTDSTOPTS|IPPF_RTHDR)) ==
	    (IPPF_RTDSTOPTS|IPPF_RTHDR)) {
		ASSERT(ipp->ipp_rtdstoptslen != 0);
		len += ipp->ipp_rtdstoptslen;
	}
	if (ipp->ipp_fields & IPPF_DSTOPTS) {
		ASSERT(ipp->ipp_dstoptslen != 0);
		len += ipp->ipp_dstoptslen;
	}
	return (len);
}

/*
 * All-purpose routine to build a header chain of an IPv6 header
 * followed by any required extension headers and a proto header,
 * preceeded (where necessary) by an ip6i_t private header.
 *
 * The fields of the IPv6 header that are derived from the ip6_pkt_t
 * will be filled in appropriately.
 * Thus the caller must fill in the rest of the IPv6 header, such as
 * traffic class/flowid, source address (if not set here), hoplimit (if not
 * set here) and destination address.
 *
 * The extension headers and ip6i_t header will all be fully filled in.
 */
void
ip_build_hdrs_v6(uchar_t *ext_hdrs, uint_t ext_hdrs_len,
    ip6_pkt_t *ipp, uint8_t protocol)
{
	uint8_t *nxthdr_ptr;
	uint8_t *cp;
	ip6i_t	*ip6i;
	ip6_t	*ip6h = (ip6_t *)ext_hdrs;

	/*
	 * If sending private ip6i_t header down (checksum info, nexthop,
	 * or ifindex), adjust ip header pointer and set ip6i_t header pointer,
	 * then fill it in. (The checksum info will be filled in by icmp).
	 */
	if (ipp->ipp_fields & IPPF_HAS_IP6I) {
		ip6i = (ip6i_t *)ip6h;
		ip6h = (ip6_t *)&ip6i[1];

		ip6i->ip6i_flags = 0;
		ip6i->ip6i_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
		if (ipp->ipp_fields & IPPF_IFINDEX) {
			ASSERT(ipp->ipp_ifindex != 0);
			ip6i->ip6i_flags |= IP6I_IFINDEX;
			ip6i->ip6i_ifindex = ipp->ipp_ifindex;
		}
		if (ipp->ipp_fields & IPPF_ADDR) {
			/*
			 * Enable per-packet source address verification if
			 * IPV6_PKTINFO specified the source address.
			 * ip6_src is set in the transport's _wput function.
			 */
			ASSERT(!IN6_IS_ADDR_UNSPECIFIED(
			    &ipp->ipp_addr));
			ip6i->ip6i_flags |= IP6I_VERIFY_SRC;
		}
		if (ipp->ipp_fields & IPPF_NEXTHOP) {
			ASSERT(!IN6_IS_ADDR_UNSPECIFIED(
			    &ipp->ipp_nexthop));
			ip6i->ip6i_flags |= IP6I_NEXTHOP;
			ip6i->ip6i_nexthop = ipp->ipp_nexthop;
		}
		/*
		 * tell IP this is an ip6i_t private header
		 */
		ip6i->ip6i_nxt = IPPROTO_RAW;
	}
	/* Initialize IPv6 header */
	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	if (ipp->ipp_fields & IPPF_HOPLIMIT)
		ip6h->ip6_hops = ipp->ipp_hoplimit;

	if (ipp->ipp_fields & IPPF_ADDR)
		ip6h->ip6_src = ipp->ipp_addr;

	nxthdr_ptr = (uint8_t *)&ip6h->ip6_nxt;
	cp = (uint8_t *)&ip6h[1];
	/*
	 * Here's where we have to start stringing together
	 * any extension headers in the right order:
	 * Hop-by-hop, destination, routing, and final destination opts.
	 */
	if (ipp->ipp_fields & IPPF_HOPOPTS) {
		/* Hop-by-hop options */
		ip6_hbh_t *hbh = (ip6_hbh_t *)cp;

		*nxthdr_ptr = IPPROTO_HOPOPTS;
		nxthdr_ptr = &hbh->ip6h_nxt;

		bcopy(ipp->ipp_hopopts, cp, ipp->ipp_hopoptslen);
		cp += ipp->ipp_hopoptslen;
	}
	/*
	 * En-route destination options
	 * Only do them if there's a routing header as well
	 */
	if ((ipp->ipp_fields & (IPPF_RTDSTOPTS|IPPF_RTHDR)) ==
	    (IPPF_RTDSTOPTS|IPPF_RTHDR)) {
		ip6_dest_t *dst = (ip6_dest_t *)cp;

		*nxthdr_ptr = IPPROTO_DSTOPTS;
		nxthdr_ptr = &dst->ip6d_nxt;

		bcopy(ipp->ipp_rtdstopts, cp, ipp->ipp_rtdstoptslen);
		cp += ipp->ipp_rtdstoptslen;
	}
	/*
	 * Routing header next
	 */
	if (ipp->ipp_fields & IPPF_RTHDR) {
		ip6_rthdr_t *rt = (ip6_rthdr_t *)cp;

		*nxthdr_ptr = IPPROTO_ROUTING;
		nxthdr_ptr = &rt->ip6r_nxt;

		bcopy(ipp->ipp_rthdr, cp, ipp->ipp_rthdrlen);
		cp += ipp->ipp_rthdrlen;
	}
	/*
	 * Do ultimate destination options
	 */
	if (ipp->ipp_fields & IPPF_DSTOPTS) {
		ip6_dest_t *dest = (ip6_dest_t *)cp;

		*nxthdr_ptr = IPPROTO_DSTOPTS;
		nxthdr_ptr = &dest->ip6d_nxt;

		bcopy(ipp->ipp_dstopts, cp, ipp->ipp_dstoptslen);
		cp += ipp->ipp_dstoptslen;
	}
	/*
	 * Now set the last header pointer to the proto passed in
	 */
	*nxthdr_ptr = protocol;
	ASSERT((int)(cp - ext_hdrs) == ext_hdrs_len);
}


/*
 * Return a pointer to the routing header extension header
 * in the IPv6 header(s) chain passed in.
 * If none found, return NULL
 * Assumes that all extension headers are in same mblk as the v6 header
 */
ip6_rthdr_t *
ip_find_rthdr_v6(ip6_t *ip6h, uint8_t *endptr)
{
	ip6_dest_t	*desthdr;
	ip6_frag_t	*fraghdr;
	uint_t		hdrlen;
	uint8_t		nexthdr;
	uint8_t		*ptr = (uint8_t *)&ip6h[1];

	if (ip6h->ip6_nxt == IPPROTO_ROUTING)
		return ((ip6_rthdr_t *)ptr);

	/*
	 * The routing header will precede all extension headers
	 * other than the hop-by-hop and destination options
	 * extension headers, so if we see anything other than those,
	 * we're done and didn't find it.
	 * We could see a destination options header alone but no
	 * routing header, in which case we'll return NULL as soon as
	 * we see anything after that.
	 * Hop-by-hop and destination option headers are identical,
	 * so we can use either one we want as a template.
	 */
	nexthdr = ip6h->ip6_nxt;
	while (ptr < endptr) {
		/* Is there enough left for len + nexthdr? */
		if (ptr + MIN_EHDR_LEN > endptr)
			return (NULL);

		switch (nexthdr) {
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
			/* Assumes the headers are identical for hbh and dst */
			desthdr = (ip6_dest_t *)ptr;
			hdrlen = 8 * (desthdr->ip6d_len + 1);
			nexthdr = desthdr->ip6d_nxt;
			break;

		case IPPROTO_ROUTING:
			return ((ip6_rthdr_t *)ptr);

		case IPPROTO_FRAGMENT:
			fraghdr = (ip6_frag_t *)ptr;
			hdrlen = sizeof (ip6_frag_t);
			nexthdr = fraghdr->ip6f_nxt;
			break;

		default:
			return (NULL);
		}
		ptr += hdrlen;
	}
	return (NULL);
}

/*
 * Called for source-routed packets originating on this node.
 * Manipulates the original routing header by moving every entry up
 * one slot, placing the first entry in the v6 header's v6_dst field,
 * and placing the ultimate destination in the routing header's last
 * slot.
 *
 * Returns the checksum diference between the ultimate destination
 * (last hop in the routing header when the packet is sent) and
 * the first hop (ip6_dst when the packet is sent)
 */
uint32_t
ip_massage_options_v6(ip6_t *ip6h, ip6_rthdr_t *rth)
{
	uint_t		numaddr;
	uint_t		i;
	in6_addr_t	*addrptr;
	in6_addr_t	tmp;
	ip6_rthdr0_t	*rthdr = (ip6_rthdr0_t *)rth;
	uint32_t	cksm;
	uint32_t	addrsum = 0;
	uint16_t	*ptr;

	/*
	 * Perform any processing needed for source routing.
	 * We know that all extension headers will be in the same mblk
	 * as the IPv6 header.
	 */

	/*
	 * If no segments left in header, or the header length field is zero,
	 * don't move hop addresses around;
	 * Checksum difference is zero.
	 */
	if ((rthdr->ip6r0_segleft == 0) || (rthdr->ip6r0_len == 0))
		return (0);

	ptr = (uint16_t *)&ip6h->ip6_dst;
	cksm = 0;
	for (i = 0; i < (sizeof (in6_addr_t) / sizeof (uint16_t)); i++) {
		cksm += ptr[i];
	}
	cksm = (cksm & 0xFFFF) + (cksm >> 16);

	/*
	 * Here's where the fun begins - we have to
	 * move all addresses up one spot, take the
	 * first hop and make it our first ip6_dst,
	 * and place the ultimate destination in the
	 * newly-opened last slot.
	 */
	addrptr = (in6_addr_t *)&rthdr->ip6r0_addr;
	numaddr = rthdr->ip6r0_len / 2;
	tmp = *addrptr;
	for (i = 0; i < (numaddr - 1); addrptr++, i++) {
		*addrptr = addrptr[1];
	}
	*addrptr = ip6h->ip6_dst;
	ip6h->ip6_dst = tmp;

	/*
	 * From the checksummed ultimate destination subtract the checksummed
	 * current ip6_dst (the first hop address). Return that number.
	 * (In the v4 case, the second part of this is done in each routine
	 *  that calls ip_massage_options(). We do it all in this one place
	 *  for v6).
	 */
	ptr = (uint16_t *)&ip6h->ip6_dst;
	for (i = 0; i < (sizeof (in6_addr_t) / sizeof (uint16_t)); i++) {
		addrsum += ptr[i];
	}
	cksm -= ((addrsum >> 16) + (addrsum & 0xFFFF));
	if ((int)cksm < 0)
		cksm--;
	cksm = (cksm & 0xFFFF) + (cksm >> 16);

	return (cksm);
}
