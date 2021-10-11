/*
 * Copyright (c) 1995-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IPV6_H
#define	_INET_IPV6_H

#pragma ident	"@(#)ip6.h	1.11	99/10/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/isa_defs.h>

/* version number for IPv6 - hard to get this one wrong! */
#define	IPV6_VERSION		6

#define	IPV6_HDR_LEN		40

#define	IPV6_ADDR_LEN		16

#ifdef	_KERNEL

/*
 * Private header used between the transports and IP to carry the content
 * of the options IPV6_PKTINFO/IPV6_RECVPKTINFO (the interface index only)
 * and IPV6_NEXTHOP.
 * Also used to specify that raw sockets do not want the UDP/TCP transport
 * checksums calculated in IP (akin to NO_IP_TP_CKSUM) and provide for
 * IPV6_CHECKSUM on the transmit side (using ip6i_checksum_off).
 *
 * When this header is used it must be the first header in the packet i.e.
 * before the real ip6 header. The use of a next header value of 255
 * (IPPROTO_RAW) in this header indicates its presence. Note that
 * ip6_nxt = IPPROTO_RAW indicates that "this" header is ip6_info - the
 * next header is always IPv6.
 *
 * Note that ip6i_nexthop is at the same offset as ip6_dst so that
 * this header can be kept in the packet while the it passes through
 * ip_newroute* and the ndp code. Those routines will use ip6_dst for
 * resolution.
 *
 * Implementation offset assumptions about ip6_info_t and ip6_t fields
 * and their alignments shown in figure below
 *
 * ip6_info (Private headers from transports to IP) header below
 * _______________________________________________________________ _ _ _ _ _
 * | .... | ip6i_nxt (255)| ......................|ip6i_nexthop| ...ip6_t.
 * --------------------------------------------------------------- - - - - -
 *        ^                                       ^
 * <---- >| same offset for {ip6i_nxt,ip6_nxt}    ^
 *        ^                                       ^
 * <------^-------------------------------------->| same offset for
 *        ^                                       ^ {ip6i_nxthop,ip6_dst}
 * _______________________________________________________________ _ _ _
 * | .... | ip6_nxt       | ......................|ip6_dst     | .other hdrs...
 * --------------------------------------------------------------- - - -
 * ip6_t (IPv6 protocol) header above
 *
 */
struct ip6_info {
	union {
		struct ip6_info_ctl {
			uint32_t	ip6i_un1_flow;
			uint16_t	ip6i_un1_plen;   /* payload length */
			uint8_t		ip6i_un1_nxt;    /* next header */
			uint8_t		ip6i_un1_hlim;   /* hop limit */
		} ip6i_un1;
	} ip6i_ctlun;
	int		ip6i_flags;	/* See below */
	int		ip6i_ifindex;
	int		ip6i_checksum_off;
	int		ip6i_pad;
	in6_addr_t	ip6i_nexthop;	/* Same offset as ip6_dst */
};
typedef struct ip6_info	ip6i_t;

#define	ip6i_flow	ip6i_ctlun.ip6i_un1.ip6i_un1_flow
#define	ip6i_vcf	ip6i_flow		/* Version, class, flow */
#define	ip6i_nxt	ip6i_ctlun.ip6i_un1.ip6i_un1_nxt
#define	ip6i_hops	ip6i_ctlun.ip6i_un1.ip6i_un1_hlim

/* ip6_info flags */
#define	IP6I_IFINDEX	0x1	/* ip6i_ifindex is set (to nonzero value) */
#define	IP6I_NEXTHOP	0x2	/* ip6i_nexthop is different than ip6_dst */
#define	IP6I_NO_TCP_UDP_CKSUM	0x4
			/*
			 * Do no generate TCP/UDP transport checksum.
			 * Used by raw sockets. Does not affect the
			 * generation of transport checksums for ICMPv6
			 * since such packets always arrive through
			 * a raw socket.
			 */
#define	IP6I_UNSPEC_SRC	0x8
			/* Used to carry ipc_unspec_src through ip_newroute* */
#define	IP6I_RAW_CHECKSUM	0x10
			/* Compute checksum and stuff in ip6i_checksum_off */
#define	IP6I_VERIFY_SRC	0x20	/* Verify ip6_src. Used when IPV6_PKTINFO */


/*
 * Different address scopes. Used internally for both unicast and multicast
 * Except for V4COMPAT and V4MAPPED a higher scope is better.
 */
#define	IP6_SCOPE_LINKLOCAL	1
#define	IP6_SCOPE_SITELOCAL	2
#define	IP6_SCOPE_GLOBAL	3
#define	IP6_SCOPE_V4COMPAT	4
#define	IP6_SCOPE_V4MAPPED	5
#define	IP6_SCOPE_MAX		IP6_SCOPE_V4MAPPED

/* Extract the scope from a multicast address */
#ifdef _BIG_ENDIAN
#define	IN6_ADDR_MC_SCOPE(addr) \
	(((addr)->s6_addr32[0] & 0x000f0000) >> 16)
#else
#define	IN6_ADDR_MC_SCOPE(addr) \
	(((addr)->s6_addr32[0] & 0x00000f00) >> 8)
#endif

/* Default IPv4 TTL for IPv6-in-IPv4 encapsulated packets */
#define	IPV6_DEFAULT_HOPS	60	/* XXX What should it be? */

/* Max IPv6 TTL */
#define	IPV6_MAX_HOPS	255

/* Minimum IPv6 MTU */
#define	IPV6_MIN_MTU		(1024+256)

/* EUI-64 based token length */
#define	IPV6_TOKEN_LEN		64

/*
 * Minimum and maximum extension header length for IPv6
 * The 8-bit len field indicates one less than the number of octets
 * in the header, which is always an integer number of octets in length.
 * Hence, 255 + 1 is the maximum number of octets in an extension header.
 */
#define	MIN_EHDR_LEN		8
#define	MAX_EHDR_LEN		(256 * 8)

/*
 * The high-order bit of the version field is used by the transports to
 * indicate a rechability confirmation to IP.
 */
#ifdef _BIG_ENDIAN
#define	IPV6_DEFAULT_VERS_AND_FLOW	0x60000000
#define	IPV6_VERS_AND_FLOW_MASK		0xF0000000
#define	IP_FORWARD_PROG			0x80000000

#define	V6_MCAST			0xFF000000
#define	V6_LINKLOCAL			0xFE800000

#else
#define	IPV6_DEFAULT_VERS_AND_FLOW	0x00000060
#define	IPV6_VERS_AND_FLOW_MASK		0x000000F0
#define	IP_FORWARD_PROG			0x00000080

#define	V6_MCAST			0x000000FF
#define	V6_LINKLOCAL			0x000080FE
#endif

/*
 * UTILITY MACROS FOR ADDRESSES.
 */

/*
 * Convert an IPv4 address mask to an IPv6 mask.   Pad with 1-bits.
 */
#define	V4MASK_TO_V6(v4, v6)	((v6).s6_addr32[0] = 0xffffffffUL,	\
				(v6).s6_addr32[1] = 0xffffffffUL,	\
				(v6).s6_addr32[2] = 0xffffffffUL,	\
				(v6).s6_addr32[3] = (v4))

/*
 * Convert aligned IPv4-mapped IPv6 address into an IPv4 address.
 * Note: We use "v6" here in definition of macro instead of "(v6)"
 * Not possible to use "(v6)" here since macro is used with struct
 * field names as arguments.
 */
#define	V4_PART_OF_V6(v6)	v6.s6_addr32[3]

#ifdef _BIG_ENDIAN
#define	V6_OR_V4_INADDR_ANY(a)	((a).s6_addr32[3] == 0 &&		\
				((a).s6_addr32[2] == 0xffffU ||	\
				(a).s6_addr32[2] == 0) &&		\
				(a).s6_addr32[1] == 0 &&		\
				(a).s6_addr32[0] == 0)

#else
#define	V6_OR_V4_INADDR_ANY(a)	((a).s6_addr32[3] == 0 && 		\
				((a).s6_addr32[2] == 0xffff0000U ||	\
				(a).s6_addr32[2] == 0) &&		\
				(a).s6_addr32[1] == 0 &&		\
				(a).s6_addr32[0] == 0)
#endif /* _BIG_ENDIAN */

/* Clear an IPv6 addr */
#define	V6_SET_ZERO(a)		((a).s6_addr32[0] = 0,			\
				(a).s6_addr32[1] = 0,			\
				(a).s6_addr32[2] = 0,			\
				(a).s6_addr32[3] = 0)

/* Mask comparison: is IPv6 addr a, and'ed with mask m, equal to addr b? */
#define	V6_MASK_EQ(a, m, b)						\
	((((a).s6_addr32[0] & (m).s6_addr32[0]) == (b).s6_addr32[0]) &&	\
	(((a).s6_addr32[1] & (m).s6_addr32[1]) == (b).s6_addr32[1]) &&	\
	(((a).s6_addr32[2] & (m).s6_addr32[2]) == (b).s6_addr32[2]) &&	\
	(((a).s6_addr32[3] & (m).s6_addr32[3]) == (b).s6_addr32[3]))

#define	V6_MASK_EQ_2(a, m, b)						\
	((((a).s6_addr32[0] & (m).s6_addr32[0]) ==			\
	    ((b).s6_addr32[0]  & (m).s6_addr32[0])) &&			\
	(((a).s6_addr32[1] & (m).s6_addr32[1]) ==			\
	    ((b).s6_addr32[1]  & (m).s6_addr32[1])) &&			\
	(((a).s6_addr32[2] & (m).s6_addr32[2]) ==			\
	    ((b).s6_addr32[2]  & (m).s6_addr32[2])) &&			\
	(((a).s6_addr32[3] & (m).s6_addr32[3]) ==			\
	    ((b).s6_addr32[3]  & (m).s6_addr32[3])))

/* Copy IPv6 address (s), logically and'ed with mask (m), into (d) */
#define	V6_MASK_COPY(s, m, d)						\
	((d).s6_addr32[0] = (s).s6_addr32[0] & (m).s6_addr32[0],	\
	(d).s6_addr32[1] = (s).s6_addr32[1] & (m).s6_addr32[1],		\
	(d).s6_addr32[2] = (s).s6_addr32[2] & (m).s6_addr32[2],		\
	(d).s6_addr32[3] = (s).s6_addr32[3] & (m).s6_addr32[3])

#define	ILL_FRAG_HASH_V6(v6addr, i)					\
	((ntohl((v6addr).s6_addr32[3]) ^ (i ^ (i >> 8))) % 		\
						ILL_FRAG_HASH_TBL_COUNT)

/*
 * Assumes that the caller passes in <fport, lport> as the uint32_t
 * parameter "ports".
 *
 * This is optimized a bit by checking the likely mismatches first.
 * in different order (See comments in IN6_ARE_ADDR_EQUAL for details)
 * Comparis of local/foreign checks are interleaved to minimize compares
 * in case of failure.
 *
 * Logically it is
 *	IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6faddr, &(ip6h)->ip6_src) &&
 *	IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6laddr, &(ip6h)->ip6_dst)
 */
#define	IP_TCP_CONN_MATCH_V6(ipc, ip6h, ports)				\
	(((ipc)->ipc_ports == (ports)) &&				\
		((ipc)->ipc_v6faddr.s6_addr32[3] ==			\
				(ip6h)->ip6_src.s6_addr32[3]) &&	\
	((ipc)->ipc_v6laddr.s6_addr32[3] ==				\
				(ip6h)->ip6_dst.s6_addr32[3]) &&	\
	((ipc)->ipc_v6faddr.s6_addr32[0] ==				\
				(ip6h)->ip6_src.s6_addr32[0]) &&	\
	((ipc)->ipc_v6laddr.s6_addr32[0] ==				\
				(ip6h)->ip6_dst.s6_addr32[0]) &&	\
	((ipc)->ipc_v6faddr.s6_addr32[1] ==				\
				(ip6h)->ip6_src.s6_addr32[1]) &&	\
	((ipc)->ipc_v6laddr.s6_addr32[1] ==				\
				(ip6h)->ip6_dst.s6_addr32[1]) &&	\
	((ipc)->ipc_v6faddr.s6_addr32[2] ==				\
				(ip6h)->ip6_src.s6_addr32[2]) &&	\
	((ipc)->ipc_v6laddr.s6_addr32[2] ==				\
				(ip6h)->ip6_dst.s6_addr32[2]))

#define	IP_TCP_CONN_HASH_V6(ipv6_src, ports)				\
	((unsigned)(ntohl((ipv6_src).s6_addr32[3]) ^ (ports >> 24) ^	\
	(ports >> 16)							\
	^ (ports >> 8) ^ ports) & (ipc_tcp_conn_hash_size - 1))

#define	IP_TCP_LISTEN_HASH_V6(lport)	\
	((unsigned)(((lport) >> 8) ^ (lport)) % A_CNT(ipc_tcp_listen_fanout_v6))

#define	IP_TCP_LISTEN_MATCH_V6(ipc, lport, ipv6_laddr)			\
	(((ipc)->ipc_lport == (lport)) &&				\
	(IN6_IS_ADDR_UNSPECIFIED(&(ipc)->ipc_v6laddr) ||		\
	IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6laddr, &(ipv6_laddr))))

/*
 * Does not check ipc_wantpacket. Caller must do that separately for
 * multicast packets.
 *
 * If ipc_faddr is non-zero check for a connected UDP socket.
 * This depends on the order of insertion in ip_bind() to ensure that
 * the most specific matches are first. Thus the insertion order in
 * the fanout buckets must be:
 *	1) Fully connected UDP sockets
 *	2) Bound to a local IP address
 *	3) Bound to INADDR_ANY
 */
#define	IP_UDP_MATCH_V6(ipc, lport, laddr, fport, faddr)		\
	(((ipc)->ipc_lport == (lport)) &&				\
	(IN6_IS_ADDR_UNSPECIFIED(&(ipc)->ipc_v6laddr) ||		\
	(IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6laddr, &(laddr)) &&		\
	((IN6_IS_ADDR_UNSPECIFIED(&(ipc)->ipc_v6faddr) ||		\
	(IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6faddr, &(faddr))) &&		\
	(ipc)->ipc_fport == (fport))))))

/*
 * Checks ipc_wantpacket for multicast.
 *
 * ip_fanout_proto_v6() needs to check just the IPv6 fanout.
 * This depends on the order of insertion in ip_bind() to ensure
 * that the most specific matches are first.  Thus the insertion order
 * in the fanout buckets must be:
 *	1) Fully specified ICMP connection (source and dest)
 *	2) Bound to a local IP address
 *	3) Bound to INADDR_ANY
 */
#define	IP_PROTO_MATCH_V6(ipc, ill, protocol, laddr, faddr)		\
	((IN6_IS_ADDR_UNSPECIFIED(&(ipc)->ipc_v6laddr) ||		\
	(IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6laddr, &(laddr)) &&		\
	(IN6_IS_ADDR_UNSPECIFIED(&(ipc)->ipc_v6faddr) ||		\
	IN6_ARE_ADDR_EQUAL(&(ipc)->ipc_v6faddr, &(faddr))))) &&		\
	(ipc_wantpacket_v6(ipc, ill, &laddr) ||				\
	(protocol == IPPROTO_RSVP)))

/*
 * GLOBAL EXTERNALS
 */
extern uint_t ipv6_ire_default_count;	/* Number of IPv6 IRE_DEFAULT entries */
extern uint_t ipv6_ire_default_index;	/* Walking IPv6 index used to mod in */
extern int ipv6_ire_cache_cnt;	/* Number of IPv6 IRE_CACHE entries */

extern const in6_addr_t	ipv6_all_ones;
extern const in6_addr_t	ipv6_all_zeros;
extern const in6_addr_t	ipv6_loopback;
extern const in6_addr_t	ipv6_all_hosts_mcast;
extern const in6_addr_t	ipv6_all_rtrs_mcast;
extern const in6_addr_t	ipv6_solicited_node_mcast;
extern const in6_addr_t	ipv6_unspecified_group;

/*
 * IPv6 mibs when the interface (ill) is not known.
 * When the ill is known the per-interface mib in the ill is used.
 */
extern mib2_ipv6IfStatsEntry_t	ip6_mib;
extern mib2_ipv6IfIcmpEntry_t	icmp6_mib;

extern icf_t *ipc_tcp_conn_fanout_v6;	/* TCP fanout hash list. */
extern icf_t ipc_tcp_listen_fanout_v6[256];
extern icf_t ipc_udp_fanout_v6[256];		/* UDP fanout hash list. */
extern icf_t ipc_proto_fanout_v6[IPPROTO_MAX+1]; /* Misc fanout hash list. */

/*
 * FUNCTION PROTOTYPES
 */
extern void	convert2ascii(char *buf, const in6_addr_t *addr);
extern char	*inet_ntop(int, const void *, char *, int);
extern int	inet_pton(int, char *, void *);
extern void	icmp_param_problem_v6(queue_t *, mblk_t *, uint8_t,
		    uint32_t ptr, boolean_t, boolean_t);
extern void	icmp_pkt2big_v6(queue_t *, mblk_t *, uint32_t,
		    boolean_t, boolean_t);
extern void	icmp_time_exceeded_v6(queue_t *, mblk_t *, uint8_t,
		    boolean_t, boolean_t);
extern void	icmp_unreachable_v6(queue_t *, mblk_t *, uint8_t,
		    boolean_t, boolean_t);
extern uint_t	ip_address_scope_v6(const in6_addr_t *);
extern void	ip_bind_v6(queue_t *, mblk_t *);
extern void	ip_build_hdrs_v6(uchar_t *, uint_t, ip6_pkt_t *, uint8_t);
extern void	ip_deliver_local_v6(queue_t *, mblk_t *, ill_t *, uint8_t,
		    uint_t, uint_t);
extern void	ip_fanout_destroy_v6(void);
extern void	ip_fanout_init_v6(void);
extern int	ip_find_hdr_v6(mblk_t *, ip6_t *, ip6_pkt_t *, uint8_t *);
extern ip6_rthdr_t	*ip_find_rthdr_v6(ip6_t *, uint8_t *);
extern int	ip_hdr_complete_v6(ip6_t *);
extern boolean_t	ip_hdr_length_nexthdr_v6(mblk_t *, ip6_t *,
		    uint16_t *, uint8_t **);
extern int	ip_hdr_length_v6(mblk_t *, ip6_t *);
extern uint_t	ip_srcaddr_to_index_v6(in6_addr_t *);
extern boolean_t	ip_index_to_srcaddr_v6(uint_t, in6_addr_t *);
extern uint32_t	ip_massage_options_v6(ip6_t *, ip6_rthdr_t *);
extern ipif_t	*ip_newroute_get_src_ipif_v6(ipif_t *, boolean_t,
		    const in6_addr_t *);
extern void	ip_newroute_multi_v6(queue_t *, mblk_t *, ipif_t *,
		    const in6_addr_t *, int);
extern void	ip_newroute_v6(queue_t *, mblk_t *, const in6_addr_t *,
		    const in6_addr_t *, ill_t *);
extern void	ip_rput_v6(queue_t *, mblk_t *);
extern int	ip_total_hdrs_len_v6(ip6_pkt_t *);
extern void	ip_wput_v6(queue_t *, mblk_t *);
extern void	mld_input(queue_t *, mblk_t *, ill_t *);
extern void	mld_joingroup(ilm_t *);
extern void	mld_leavegroup(ilm_t *);
extern uint_t	mld_timeout_handler(void);
extern void	mld_timeout_start(int);
extern void	pr_addr_dbg(char *, int, const void *);
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IPV6_H */
