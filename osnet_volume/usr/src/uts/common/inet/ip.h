/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_H
#define	_INET_IP_H

#pragma ident	"@(#)ip.h	1.88	99/11/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/isa_defs.h>
#include <sys/strick.h>
#include <sys/types.h>
#include <inet/mib2.h>
#include <sys/atomic.h>

#define	IP_DEBUG

/* Minor numbers */
#define	IPV4_MINOR	0
#define	IPV6_MINOR	1

#ifndef	_IPADDR_T
#define	_IPADDR_T
typedef uint32_t ipaddr_t;
#endif

/* Number of bits in an address */
#define	IP_ABITS		32
#define	IPV6_ABITS		128

#define	IP_HOST_MASK		(ipaddr_t)0xffffffffU

#define	IP_CSUM(mp, off, sum)		(~ip_cksum(mp, off, sum) & 0xFFFF)
#define	IP_CSUM_PARTIAL(mp, off, sum)	ip_cksum(mp, off, sum)
#define	IP_BCSUM_PARTIAL(bp, len, sum)	bcksum(bp, len, sum)

/*
 * Flag to IP write side to not compute the TCP/UDP checksums.
 * Stored in ipha_ident (which is otherwise zero).
 */
#define	NO_IP_TP_CKSUM			0xFFFF

#define	ILL_FRAG_HASH_TBL_COUNT	((unsigned int)64)
#define	ILL_FRAG_HASH_TBL_SIZE	(ILL_FRAG_HASH_TBL_COUNT * sizeof (ipfb_t))

#define	IP_DEV_NAME			"/dev/ip"
#define	IP_MOD_NAME			"ip"
#define	IPV4_ADDR_LEN			4
#define	IP_ADDR_LEN			IPV4_ADDR_LEN
#define	IP_ARP_PROTO_TYPE		0x0800

#define	IPV4_VERSION			4
#define	IP_VERSION			IPV4_VERSION
#define	IP_SIMPLE_HDR_LENGTH_IN_WORDS	5
#define	IP_SIMPLE_HDR_LENGTH		20
#define	IP_MAX_HDR_LENGTH		60

#define	IP_MIN_MTU			(IP_MAX_HDR_LENGTH + 8)	/* 68 bytes */

/*
 * XXX IP_MAXPACKET is defined in <netinet/ip.h> as well. At some point the
 * 2 files should be cleaned up to remove all redundant definitions.
 */
#define	IP_MAXPACKET			65535
#define	IP_SIMPLE_HDR_VERSION \
	((IP_VERSION << 4) | IP_SIMPLE_HDR_LENGTH_IN_WORDS)

/*
 * Constants and type definitions to support IP IOCTL commands
 */
#define	IP_IOCTL			(('i'<<8)|'p')
#define	IP_IOC_IRE_DELETE		4
#define	IP_IOC_IRE_DELETE_NO_REPLY	5
#define	IP_IOC_IRE_ADVISE_NO_REPLY	6
#define	IP_IOC_RTS_REQUEST		7

/* Common definitions used by IP IOCTL data structures */
typedef struct ipllcmd_s {
	uint_t	ipllc_cmd;
	uint_t	ipllc_name_offset;
	uint_t	ipllc_name_length;
} ipllc_t;

/* IP IRE Change Command Structure. */
typedef struct ipic_s {
	ipllc_t	ipic_ipllc;
	uint_t	ipic_ire_type;
	uint_t	ipic_max_frag;
	uint_t	ipic_addr_offset;
	uint_t	ipic_addr_length;
	uint_t	ipic_mask_offset;
	uint_t	ipic_mask_length;
	uint_t	ipic_src_addr_offset;
	uint_t	ipic_src_addr_length;
	uint_t	ipic_ll_hdr_offset;
	uint_t	ipic_ll_hdr_length;
	uint_t	ipic_gateway_addr_offset;
	uint_t	ipic_gateway_addr_length;
	clock_t	ipic_rtt;
	uint32_t ipic_ssthresh;
	clock_t	ipic_rtt_sd;
} ipic_t;

#define	ipic_cmd		ipic_ipllc.ipllc_cmd
#define	ipic_ll_name_length	ipic_ipllc.ipllc_name_length
#define	ipic_ll_name_offset	ipic_ipllc.ipllc_name_offset

/* IP IRE Delete Command Structure. */
typedef struct ipid_s {
	ipllc_t	ipid_ipllc;
	uint_t	ipid_ire_type;
	uint_t	ipid_addr_offset;
	uint_t	ipid_addr_length;
	uint_t	ipid_mask_offset;
	uint_t	ipid_mask_length;
} ipid_t;

#define	ipid_cmd		ipid_ipllc.ipllc_cmd


/* IP Options */
#ifndef IPOPT_EOL
#define	IPOPT_EOL		0x00
#define	IPOPT_NOP		0x01
#define	IPOPT_RR		0x07
#define	IPOPT_IT		0x44
#define	IPOPT_SEC		0x82
#define	IPOPT_LSRR		0x83
#define	IPOPT_EXTSEC		0x85
#define	IPOPT_COMSEC		0x86
#define	IPOPT_SID		0x88
#define	IPOPT_SSRR		0x89
#define	IPOPT_RALERT		0x94
#define	IPOPT_SDMDD		0x95

/* Bits in the option value */
#define	IPOPT_COPY		0x80
#endif /* IPOPT_EOL */

/* IP option header indexes */
#define	IPOPT_POS_VAL		0
#define	IPOPT_POS_LEN		1
#define	IPOPT_POS_OFF		2
#define	IPOPT_POS_OV_FLG	3

/* Minimum for src and record route options */
#define	IPOPT_MINOFF_SR		4

/* Minimum for timestamp option */
#define	IPOPT_MINOFF_IT		5
#define	IPOPT_MINLEN_IT		5

/* Timestamp option flag bits */
#define	IPOPT_IT_TIME		0	/* Only timestamp */
#define	IPOPT_IT_TIME_ADDR	1	/* Timestamp + IP address */
#define	IPOPT_IT_SPEC		3	/* Only listed routers */
#define	IPOPT_IT_SPEC_BSD	2	/* Defined fopr BSD compat */

#define	IPOPT_IT_TIMELEN	4	/* Timestamp size */

/* Controls forwarding of IP packets, set via ndd */
#define	IP_FORWARD_NEVER	0
#define	IP_FORWARD_ALWAYS	1

#define	WE_ARE_FORWARDING	(ip_g_forward == IP_FORWARD_ALWAYS)

#define	IPH_HDR_LENGTH(ipha)						\
	((int)(((ipha_t *)ipha)->ipha_version_and_hdr_length & 0xF) << 2)

#define	IPH_HDR_VERSION(ipha)						\
	((int)(((ipha_t *)ipha)->ipha_version_and_hdr_length) >> 4)

#ifdef _KERNEL
/*
 * IP reassembly macros.  We hide starting and ending offsets in b_next and
 * b_prev of messages on the reassembly queue.	The messages are chained using
 * b_cont.  These macros are used in ip_reassemble() so we don't have to see
 * the ugly casts and assignments.
 * Note that the offsets are <= 64k i.e. a uint_t is sufficient to represent
 * them.
 */
#define	IP_REASS_START(mp)		((uint_t)((mp)->b_next))
#define	IP_REASS_SET_START(mp, u)	((mp)->b_next = (mblk_t *)(u))
#define	IP_REASS_END(mp)		((uint_t)((mp)->b_prev))
#define	IP_REASS_SET_END(mp, u)		((mp)->b_prev = (mblk_t *)(u))

/* Privilege check for an IP instance of unknown type.  wq is write side. */
#define	IS_PRIVILEGED_QUEUE(wq)						\
	((wq)->q_next ? (((ill_t *)(wq)->q_ptr)->ill_priv_stream) \
	    : (((ipc_t *)(wq)->q_ptr)->ipc_priv_stream))

/*
 * Flags for the various ip_fanout_* routines.
 */
#define	IP_FF_SEND_ICMP		0x01	/* Send an ICMP error */
#define	IP_FF_HDR_COMPLETE	0x02	/* Call ip_hdr_complete if error */
#define	IP_FF_CKSUM		0x04	/* Recompute ipha_cksum if error */
#define	IP_FF_RAWIP		0x08	/* Use rawip mib variable */
#define	IP_FF_SRC_QUENCH	0x10	/* OK to send ICMP_SOURCE_QUENCH */
#define	IP_FF_SYN_ADDIRE	0x20	/* Add IRE if TCP syn packet */
#define	IP_FF_PROXY_ONLY	0x40	/* Only match proxy listeners */
#define	IP_FF_IP6INFO		0x80	/* Add ip6i_t if needed */

#ifdef _BIG_ENDIAN
#define	IP_UDP_HASH(port)		((port) & 0xFF)
#else	/* _BIG_ENDIAN */
#define	IP_UDP_HASH(port)		(((uint16_t)(port)) >> 8)
#endif	/* _BIG_ENDIAN */

#ifndef	IRE_DB_TYPE
#define	IRE_DB_TYPE	M_SIG
#endif

#ifndef	IRE_DB_REQ_TYPE
#define	IRE_DB_REQ_TYPE	M_PCSIG
#endif

/*
 * This is part of the interface between Transport provider and
 * IP which can be used to set policy information. This is usually
 * accompanied with O_T_BIND_REQ/T_BIND_REQ.ip_bind assumes that
 * only IPSEC_POLICY_SET is there when it is found in the chain.
 * The information contained is an struct ipsec_req_t. On success
 * or failure, either the T_BIND_ACK or the T_ERROR_ACK is returned.
 * IPSEC_POLICY_SET is never returned.
 */
#define	IPSEC_POLICY_SET	M_SETOPTS

#define	IRE_IS_LOCAL(ire)	((ire != NULL) && \
				((ire)->ire_type & (IRE_LOCAL | IRE_LOOPBACK)))

#define	IRE_IS_TARGET(ire)	((ire != NULL) && \
				((ire)->ire_type != IRE_BROADCAST))

/* IP Fragmentation Reassembly Header */
typedef struct ipf_s {
	struct ipf_s	*ipf_hash_next;
	struct ipf_s	**ipf_ptphn;	/* Pointer to previous hash next. */
	uint32_t	ipf_ident;	/* Ident to match. */
	uint8_t		ipf_protocol;	/* Protocol to match. */
	uchar_t		ipf_last_frag_seen : 1;	/* Last fragment seen ? */
	time_t		ipf_timestamp;	/* Reassembly start time. */
	mblk_t		*ipf_mp;	/* mblk we live in. */
	mblk_t		*ipf_tail_mp;	/* Frag queue tail pointer. */
	int		ipf_hole_cnt;	/* Number of holes (hard-case). */
	int		ipf_end;	/* Tail end offset (0 -> hard-case). */
	int		ipf_stripped_hdr_len;	/* What we've stripped from */
						/* the lead mblk. */
	uint16_t	ipf_checksum;	/* Partial checksum of fragment data */
	uint16_t	ipf_checksum_valid;
	uint_t		ipf_gen;	/* Frag queue generation */
	size_t		ipf_count;	/* Count of bytes used by frag */
	uint_t		ipf_nf_hdr_len; /* Length of nonfragmented header */
	in6_addr_t	ipf_v6src;	/* IPv6 source address */
	in6_addr_t	ipf_v6dst;	/* IPv6 dest address */
	uint_t		ipf_prev_nexthdr_offset; /* Offset for nexthdr value */
} ipf_t;

#define	ipf_src	V4_PART_OF_V6(ipf_v6src)
#define	ipf_dst	V4_PART_OF_V6(ipf_v6dst)

/* IP packet count structure */
typedef struct ippc_s {
	in6_addr_t ippc_v6addr;
	uint_t	ippc_ib_pkt_count;
	uint_t	ippc_ob_pkt_count;
	uint_t	ippc_fo_pkt_count;
} ippc_t;

#define	ippc_addr	V4_PART_OF_V6(ippc_v6addr)

#endif /* _KERNEL */

/* ICMP types */
#define	ICMP_ECHO_REPLY			0
#define	ICMP_DEST_UNREACHABLE		3
#define	ICMP_SOURCE_QUENCH		4
#define	ICMP_REDIRECT			5
#define	ICMP_ECHO_REQUEST		8
#define	ICMP_ROUTER_ADVERTISEMENT	9
#define	ICMP_ROUTER_SOLICITATION	10
#define	ICMP_TIME_EXCEEDED		11
#define	ICMP_PARAM_PROBLEM		12
#define	ICMP_TIME_STAMP_REQUEST		13
#define	ICMP_TIME_STAMP_REPLY		14
#define	ICMP_INFO_REQUEST		15
#define	ICMP_INFO_REPLY			16
#define	ICMP_ADDRESS_MASK_REQUEST	17
#define	ICMP_ADDRESS_MASK_REPLY		18

/* ICMP_TIME_EXCEEDED codes */
#define	ICMP_TTL_EXCEEDED		0
#define	ICMP_REASSEMBLY_TIME_EXCEEDED	1

/* ICMP_DEST_UNREACHABLE codes */
#define	ICMP_NET_UNREACHABLE		0
#define	ICMP_HOST_UNREACHABLE		1
#define	ICMP_PROTOCOL_UNREACHABLE	2
#define	ICMP_PORT_UNREACHABLE		3
#define	ICMP_FRAGMENTATION_NEEDED	4
#define	ICMP_SOURCE_ROUTE_FAILED	5
#define	ICMP_DEST_NET_UNKNOWN		6
#define	ICMP_DEST_HOST_UNKNOWN		7
#define	ICMP_SRC_HOST_ISOLATED		8
#define	ICMP_DEST_NET_UNREACH_ADMIN	9
#define	ICMP_DEST_HOST_UNREACH_ADMIN	10
#define	ICMP_DEST_NET_UNREACH_TOS	11
#define	ICMP_DEST_HOST_UNREACH_TOS	12

/* ICMP Header Structure */
typedef struct icmph_s {
	uint8_t		icmph_type;
	uint8_t		icmph_code;
	uint16_t	icmph_checksum;
	union {
		struct { /* ECHO request/response structure */
			uint16_t	u_echo_ident;
			uint16_t	u_echo_seqnum;
		} u_echo;
		struct { /* Destination unreachable structure */
			uint16_t	u_du_zero;
			uint16_t	u_du_mtu;
		} u_du;
		struct { /* Parameter problem structure */
			uint8_t		u_pp_ptr;
			uint8_t		u_pp_rsvd[3];
		} u_pp;
		struct { /* Redirect structure */
			ipaddr_t	u_rd_gateway;
		} u_rd;
	} icmph_u;
} icmph_t;

#define	icmph_echo_ident	icmph_u.u_echo.u_echo_ident
#define	icmph_echo_seqnum	icmph_u.u_echo.u_echo_seqnum
#define	icmph_du_zero		icmph_u.u_du.u_du_zero
#define	icmph_du_mtu		icmph_u.u_du.u_du_mtu
#define	icmph_pp_ptr		icmph_u.u_pp.u_pp_ptr
#define	icmph_rd_gateway	icmph_u.u_rd.u_rd_gateway

#define	ICMPH_SIZE	8

/* Aligned IP header */
typedef struct ipha_s {
	uint8_t		ipha_version_and_hdr_length;
	uint8_t		ipha_type_of_service;
	uint16_t	ipha_length;
	uint16_t	ipha_ident;
	uint16_t	ipha_fragment_offset_and_flags;
	uint8_t		ipha_ttl;
	uint8_t		ipha_protocol;
	uint16_t	ipha_hdr_checksum;
	ipaddr_t	ipha_src;
	ipaddr_t	ipha_dst;
} ipha_t;

#define	IPH_DF		0x4000	/* Don't fragment */
#define	IPH_MF		0x2000	/* More fragments to come */
#define	IPH_OFFSET	0x1FFF	/* Where the offset lives */
#define	IPH_FRAG_HDR	0x8000	/* IPv6 don't fragment bit */

/* IP Mac info structure */
typedef struct ip_m_s {
	t_uscalar_t	ip_m_mac_type;	/* From <sys/dlpi.h> */
	int		ip_m_type;	/* From <net/if_types.h> */
	t_uscalar_t	ip_m_sap;
	t_scalar_t	ip_m_sap_length;	/* From <sys/dlpi.h> */
	t_scalar_t	ip_m_brdcst_addr_length;
	uchar_t		*ip_m_brdcst_addr;
} ip_m_t;

/* Router entry types */
#define	IRE_BROADCAST		0x0001	/* Route entry for broadcast address */
#define	IRE_DEFAULT		0x0002	/* Route entry for default gateway */
#define	IRE_LOCAL		0x0004	/* Route entry for local address */
#define	IRE_LOOPBACK		0x0008	/* Route entry for loopback address */
#define	IRE_PREFIX		0x0010	/* Route entry for prefix routes */
#define	IRE_CACHE		0x0020	/* Cached Route entry */
#define	IRE_IF_NORESOLVER	0x0040	/* Route entry for local interface */
					/* net without any address mapping. */
#define	IRE_IF_RESOLVER		0x0080	/* Route entry for local interface */
					/* net with resolver. */
#define	IRE_HOST		0x0100	/* Host route entry */
#define	IRE_HOST_REDIRECT	0x0200	/* Host route entry from redirects */

#define	IRE_INTERFACE		(IRE_IF_NORESOLVER | IRE_IF_RESOLVER)
#define	IRE_OFFSUBNET		(IRE_DEFAULT | IRE_PREFIX | IRE_HOST | \
				IRE_HOST_REDIRECT)
#define	IRE_CACHETABLE		(IRE_CACHE | IRE_BROADCAST | IRE_LOCAL | \
				IRE_LOOPBACK)
#define	IRE_FORWARDTABLE	(IRE_INTERFACE | IRE_OFFSUBNET)

/*
 * If an IRE is marked with IRE_MARK_CONDEMNED, the last walker of
 * the bucket should delete this IRE from this bucket.
 */
#define	IRE_MARK_CONDEMNED	0x0001

/* Flags with ire_expire routine */
#define	FLUSH_ARP_TIME		0x0001	/* ARP info potentially stale timer */
#define	FLUSH_REDIRECT_TIME	0x0002	/* Redirects potentially stale */
#define	FLUSH_MTU_TIME		0x0004	/* Include path MTU per RFC 1191 */

/* Arguments to ire_flush_cache() */
#define	IRE_FLUSH_DELETE	0
#define	IRE_FLUSH_ADD		1

/*
 * Open/close synchronization flags.
 * These are kept in a separate field in the ipc/ill and the synchronization
 * depends on the atomic 32 bit access to that field.
 */
#define	IPCF_CLOSING		0x01	/* ip_close waiting for ip_wsrv */
#define	IPCF_CLOSE_DONE		0x02	/* ip_wsrv signalling ip_close */
#define	IPCF_OPENING		0x04	/* ip_open waiting for ip_wsrv */
#define	IPCF_OPEN_DONE		0x08	/* ip_wsrv signalling ip_open */

/* Used by proxy listeners to pick up packets in the forwarding path. */
typedef struct proxy_addr {
	ipaddr_t	pa_addr;
	ipaddr_t	pa_mask;
	struct proxy_addr	*pa_next;
} proxy_addr_t;

#ifdef _KERNEL
/* Group membership list per upper ipc */
/*
 * XXX add ilg info for ifaddr/ifindex.
 * XXX can we make ilg survive an ifconfig unplumb + plumb
 * by setting the ipif/ill to NULL and recover that later?
 */
typedef struct ilg_s {
	in6_addr_t	ilg_v6group;
	struct ipif_s	*ilg_ipif;	/* Logical interface we are member on */
	struct ill_s	*ilg_ill;	/* Physical interface " */
} ilg_t;

/* Multicast address list entry for lower ill */
typedef struct ilm_s {
	in6_addr_t	ilm_v6addr;
	int		ilm_refcnt;
	uint_t		ilm_timer;	/* IGMP */
	struct ipif_s	*ilm_ipif;	/* Back pointer to ipif */
	struct ilm_s	*ilm_next;	/* Linked list for each ill */
	uint_t		ilm_state;	/* state of the membership */
} ilm_t;

#define	ilm_addr	V4_PART_OF_V6(ilm_v6addr)

/* IP client structure, allocated when ip_open indicates a STREAM device */
typedef struct ipc_s {
	struct ipc_s	*ipc_hash_next; /* Hash chain must be first */
	struct ipc_s	**ipc_ptphn;	/* Pointer to previous hash next. */
	kmutex_t	ipc_irc_lock;	/* Lock to protect ipc_ire_cache */
	struct ire_s	*ipc_ire_cache;	/* Cached IRE_CACHE */
	queue_t	*ipc_rq;
	queue_t	*ipc_wq;
	kmutex_t *ipc_fanout_lock;	/* hash bucket lock when ptphn set */
	struct ill_s	*ipc_pending_ill; /* Waiting for ioctl on ill */
					/* Should match ill_pending_q */
	struct {
		in6_addr_t ipcua_laddr;	/* Local address */
		in6_addr_t ipcua_faddr;	/* Remote address. 0 => */
					/* not connected */
	} ipcua_v6addr;
#define	ipc_laddr	V4_PART_OF_V6(ipcua_v6addr.ipcua_laddr)
#define	ipc_faddr	V4_PART_OF_V6(ipcua_v6addr.ipcua_faddr)
#define	ipc_v6laddr	ipcua_v6addr.ipcua_laddr
#define	ipc_v6faddr	ipcua_v6addr.ipcua_faddr

	union {
		struct {
			uint16_t	ipcu_fport;	/* Remote port */
			uint16_t	ipcu_lport;	/* Local port */
		} ipcu_ports1;
		uint32_t	ipcu_ports2;	/* Rem port, local port */
					/* Used for TCP_MATCH performance */
	} ipc_ipcu;
#define	ipc_lport	ipc_ipcu.ipcu_ports1.ipcu_lport
#define	ipc_fport	ipc_ipcu.ipcu_ports1.ipcu_fport
#define	ipc_ports	ipc_ipcu.ipcu_ports2

	kmutex_t	ipc_reflock;	/* Protects ipc_refcnt */
	ushort_t	ipc_refcnt;	/* Number of pending upstream msg */
	kcondvar_t	ipc_refcv;	/* Wait for refcnt decrease */

	ilg_t	*ipc_ilg;		/* Group memberships */
	int	ipc_ilg_allocated;	/* Number allocated */
	int	ipc_ilg_inuse;		/* Number currently used */
	struct ipif_s	*ipc_multicast_ipif;	/* IP_MULTICAST_IF (IPv4) */
	struct ill_s	*ipc_multicast_ill;	/* IP_MULTICAST_IF (IPv6) */
	struct ill_s	*ipc_incoming_ill;	/* IP{,V6}_BOUND_IF */
	struct ill_s	*ipc_outgoing_ill;	/* IP{,V6}_BOUND_IF */
	uint_t	ipc_close_flags;	/* IPCF_* flags for close synch */
	uint_t	ipc_proto;		/* SO_PROTOTYPE state */
	unsigned int
		ipc_dontroute : 1,		/* SO_DONTROUTE state */
		ipc_loopback : 1,		/* SO_LOOPBACK state */
		ipc_broadcast : 1,		/* SO_BROADCAST state */
		ipc_reuseaddr : 1,		/* SO_REUSEADDR state */

		ipc_multicast_loop : 1,		/* IP_MULTICAST_LOOP */
		ipc_multi_router : 1,		/* Wants all multicast pkts */
		ipc_priv_stream : 1,		/* Privileged client? */
		ipc_draining : 1,		/* ip_wsrv running */

		ipc_did_putbq : 1,		/* ip_wput did a putbq */
		ipc_unspec_src : 1,		/* IP_UNSPEC_SRC */
		ipc_proxy_listen : 1,		/* proxy listener is active */
		ipc_policy_cached : 1,		/* Is policy cached/latched ? */

		ipc_in_enforce_policy : 1,	/* Enforce Policy on inbound */
		ipc_out_enforce_policy : 1,	/* Enforce Policy on outbound */
		ipc_af_isv6 : 1,		/* ip address family ver 6 */
		ipc_pkt_isv6 : 1,		/* ip packet format ver 6 */

		ipc_ipv6_recvpktinfo : 1,	/* IPV6_RECVPKTINFO option */
		ipc_ipv6_recvhoplimit : 1,	/* IPV6_RECVHOPLIMIT option */
		ipc_ipv6_recvhopopts : 1,	/* IPV6_RECVHOPOPTS option */
		ipc_ipv6_recvdstopts : 1,	/* IPV6_RECVDSTOPTS option */

		ipc_ipv6_recvrthdr : 1,		/* IPV6_RECVRTHDR option */
		ipc_ipv6_recvrtdstopts : 1,	/* IPV6_RECVRTHDRDSTOPTS */
		ipc_fully_bound : 1,		/* Fully bound connection */

		ipc_pad_to_bit_31 : 9;

	uint_t	ipc_proxy_ib_pkt_count;	/* # of pkts this proxy has picked up */
	proxy_addr_t	*ipc_palist;	/* list of proxy addr/mask */
	uchar_t		ipc_ulp;	/* Upper layer protocol */
	ipsec_req_t	*ipc_outbound_policy; /* Outbound Policy information */
	ipsec_req_t	*ipc_inbound_policy; /* Inbound Policy information */
	mblk_t		*ipc_ipsec_out;	/* Sent up with unbind ACK */
	mblk_t		*ipc_ipsec_req_in; /* for TCP connections */
} ipc_t;

#define	ipc_out_ah_req		ipc_outbound_policy->ipsr_ah_req
#define	ipc_out_auth_alg	ipc_outbound_policy->ipsr_auth_alg
#define	ipc_out_esp_req		ipc_outbound_policy->ipsr_esp_req
#define	ipc_out_esp_alg		ipc_outbound_policy->ipsr_esp_alg
#define	ipc_out_esp_auth_alg	ipc_outbound_policy->ipsr_esp_auth_alg
#define	ipc_out_self_encap_req	ipc_outbound_policy->ipsr_self_encap_req

#define	ipc_in_ah_req		ipc_inbound_policy->ipsr_ah_req
#define	ipc_in_auth_alg		ipc_inbound_policy->ipsr_auth_alg
#define	ipc_in_esp_req		ipc_inbound_policy->ipsr_esp_req
#define	ipc_in_esp_alg		ipc_inbound_policy->ipsr_esp_alg
#define	ipc_in_esp_auth_alg	ipc_inbound_policy->ipsr_esp_auth_alg
#define	ipc_in_self_encap_req	ipc_inbound_policy->ipsr_self_encap_req

/*
 * This is used to match an inbound/outbound datagram with
 * policy.
 */

typedef	struct ipsec_selector {
	ipaddr_t	src_addr;
	ipaddr_t	src_mask;
	ipaddr_t	dst_addr;
	ipaddr_t	dst_mask;
	uint16_t	src_port;
	uint16_t	dst_port;
	uint8_t		protocol;
	boolean_t	outbound;
} ipsec_selector_t;

/*
 * Macros used when sending data upstream using the fanout lists.
 * Needed to prevent the ipc stream from closing while there
 * is a reference to its queue.
 *
 * The ipc_refcnt does not capture all threads accessing an ipc.
 * Those that are running in ip_open, ip_close, put, or srv in the
 * queues corresponding to the ipc do not hold a refcnt. The refcnt only
 * captures other threads (e.g. the fanout of inbound packets) that
 * need to access the ipc.
 *
 * Note: In order to guard against the hash table changing
 * the caller of IPC_REFHOLD must hold the lock on the hash bucket.
 */
#define	IPC_REFHOLD(ipc) {			\
	ASSERT(ipc->ipc_fanout_lock != NULL);	\
	ASSERT(MUTEX_HELD(ipc->ipc_fanout_lock)); \
	mutex_enter(&(ipc)->ipc_reflock);	\
	(ipc)->ipc_refcnt++;			\
	ASSERT((ipc)->ipc_refcnt != 0);		\
	mutex_exit(&(ipc)->ipc_reflock);	\
}

#define	IPC_REFRELE(ipc) {			\
	mutex_enter(&(ipc)->ipc_reflock);	\
	ASSERT((ipc)->ipc_refcnt != 0);		\
	(ipc)->ipc_refcnt--;			\
	cv_broadcast(&(ipc)->ipc_refcv);	\
	mutex_exit(&(ipc)->ipc_reflock);	\
}

/* Values used in IP by IPSEC Code */
#define		IPSEC_OUTBOUND		B_TRUE
#define		IPSEC_INBOUND		B_FALSE

/*
 * There are two variants in policy failures. The packet may come in
 * secure when not needed (IPSEC_POLICY_???_NOT_NEEDED) or it may not
 * have the desired level of protection (IPSEC_POLICY_MISMATCH).
 */
#define	IPSEC_POLICY_NOT_NEEDED		0
#define	IPSEC_POLICY_MISMATCH		1
#define	IPSEC_POLICY_AUTH_NOT_NEEDED	2
#define	IPSEC_POLICY_ENCR_NOT_NEEDED	3
#define	IPSEC_POLICY_SE_NOT_NEEDED	4
#define	IPSEC_POLICY_MAX		5	/* Always max + 1. */

/*
 * Information cached in IRE for upper layer protocol (ULP).
 *
 * Notice that ire_max_frag is not included in the iulp_t structure, which
 * it may seem that it should.  But ire_max_frag cannot really be cached.  It
 * is fixed for each interface.  For MTU found by PMTUd, we may want to cache
 * it.  But currently, we do not do that.
 */
typedef struct iulp_s {
	boolean_t	iulp_set;	/* Is any metric set? */
	uint32_t	iulp_ssthresh;	/* Slow start threshold (TCP). */
	clock_t		iulp_rtt;	/* Guestimate in millisecs. */
	clock_t		iulp_rtt_sd;	/* Cached value of RTT variance. */
	uint32_t	iulp_spipe;	/* Send pipe size. */
	uint32_t	iulp_rpipe;	/* Receive pipe size. */
	uint32_t	iulp_rtomax;	/* Max round trip timeout. */
	uint32_t	iulp_sack;	/* Use SACK option (TCP)? */
	uint32_t
		iulp_tstamp_ok : 1,	/* Use timestamp option (TCP)? */
		iulp_wscale_ok : 1,	/* Use window scale option (TCP)? */
		iulp_ecn_ok : 1,	/* Enable ECN (for TCP)? */
		iulp_pmtud_ok : 1,	/* Enable PMTUd? */

		iulp_not_used : 28;
} iulp_t;

/* Zero iulp_t. */
extern const iulp_t ire_uinfo_null;

/*
 * The IP Client Fanout structure.
 * The hash tables and their linkage (ipc_hash_next, ipc_hash_ptpn) are
 * protected by the per-bucket icf_lock. Each ipc_t inserted in
 * the list points back at this lock using ipc_fanout_lock.
 */
typedef struct icf_s {
	ipc_t		*icf_ipc;
	kmutex_t	icf_lock;
} icf_t;

/*
 * An interface group structure.  See ip_if.c for details on
 * interface groups.
 */
typedef struct ifgrp_s {
	struct ipif_s *ifgrp_schednext;	/* Next member of ifgrp to get */
					/* assigned to a new cache entry. */
	struct ifgrp_s *ifgrp_next;	/* Next ifgrp in list. */
} ifgrp_t;

/*
 * Interface route structure which holds the necessary information to recreate
 * routes that are tied to an interface (namely where ire_ipif != NULL).
 * These routes which were initially created via a routing socket or via the
 * SIOCADDRT ioctl may be gateway routes (RTF_GATEWAY being set) or may be
 * traditional interface routes.  When an interface comes back up after being
 * marked down, this information will be used to recreate the routes.  These
 * are part of an mblk_t chain that hangs off of the IPIF (ipif_saved_ire_mp).
 */
typedef struct ifrt_s {
	ushort_t	ifrt_type;		/* Type of IRE */
	in6_addr_t	ifrt_v6addr;		/* Address IRE represents. */
	in6_addr_t	ifrt_v6gateway_addr;	/* Gateway if IRE_OFFSUBNET */
	in6_addr_t	ifrt_v6mask;		/* Mask for matching IRE. */
	uint32_t	ifrt_flags;		/* flags related to route */
	uint_t		ifrt_max_frag;		/* MTU (next hop or path). */
	iulp_t		ifrt_iulp_info;		/* Cached IRE ULP info. */
} ifrt_t;

#define	ifrt_addr		V4_PART_OF_V6(ifrt_v6addr)
#define	ifrt_gateway_addr	V4_PART_OF_V6(ifrt_v6gateway_addr)
#define	ifrt_mask		V4_PART_OF_V6(ifrt_v6mask)

/* IP interface structure, one per local address */
typedef struct ipif_s {
	struct	ipif_s	*ipif_next;
	struct	ill_s	*ipif_ill;	/* Back pointer to our ill */
	int	ipif_id;		/* Logical unit number */
	uint_t	ipif_mtu;		/* Starts at ipif_ill->ill_max_frag */
	in6_addr_t ipif_v6lcl_addr;	/* Local IP address for this if. */
	in6_addr_t ipif_v6src_addr;	/* Source IP address for this if. */
	in6_addr_t ipif_v6subnet;	/* Subnet prefix for this if. */
	in6_addr_t ipif_v6net_mask;	/* Net mask for this interface. */
	in6_addr_t ipif_v6brd_addr;	/* Broadcast addr for this interface. */
	in6_addr_t ipif_v6pp_dst_addr;	/* Point-to-point dest address. */
	uint_t	ipif_flags;		/* Interface flags. */
	uint_t	ipif_metric;		/* BSD if metric, for compatibility. */
	uint_t	ipif_ire_type;		/* IRE_LOCAL or IRE_LOOPBACK */
	mblk_t	*ipif_down_mp;		/* Allocated at time arp and ndp */
					/* comes up to */
					/* prevent awkward out of mem */
					/* condition later */
	mblk_t	*ipif_arp_on_mp;	/* Allocated at time arp comes up to */
					/* prevent awkward out of mem */
					/* condition later */

	mblk_t	*ipif_saved_ire_mp;	/* Allocated for each extra */
					/* IRE_IF_NORESOLVER/IRE_IF_RESOLVER */
					/* on this interface so that they */
					/* can survive ifconfig down. */
	kmutex_t ipif_saved_ire_lock;	/* Protects ipif_saved_ire_mp */

	/*
	 * If this union is a NULL pointer, than this ipif is not part of
	 * any interface group.
	 */
	union {
		struct ipif_s **ifgrpu_schednext; /* Pointer to the pointer */
						/* of the next ipif to be */
						/* scheduled in the ipif's */
						/* interface group.  See */
						/* ip_if.c for more on */
						/* interface groups (ifgrps). */
		ifgrp_t *ifgrpu_ifgrp;		/* Back-pointer to my ifgrp. */
	} ipif_ifgrpu;
#define	ipif_ifgrpschednext ipif_ifgrpu.ifgrpu_schednext
#define	ipif_ifgrp ipif_ifgrpu.ifgrpu_ifgrp

	struct ipif_s	*ipif_ifgrpnext;	/* Next ipif that has a */
						/* different ill entry in my */
						/* interface group set. */
						/* If I'm NULL, then I'm not */
						/* in the current ifgrp set. */

	/*
	 * The packet counts in the ipif contain the sum of the
	 * packet counts in dead IREs that were affiliated with
	 * this ipif.
	 */
	uint_t	ipif_fo_pkt_count;	/* Forwarded thru our dead IREs */
	uint_t	ipif_ib_pkt_count;	/* Inbound packets for our dead IREs */
	uint_t	ipif_ob_pkt_count;	/* Outbound packets to our dead IREs */
	unsigned int
		ipif_multicast_up : 1,	/* We have joined the allhosts group */
		ipif_solmcast_up : 1,	/* We joined solicited node mcast */
		ipif_pad_to_31 : 29;
} ipif_t;

/* IPv4 compatability macros */
#define	ipif_lcl_addr		V4_PART_OF_V6(ipif_v6lcl_addr)
#define	ipif_src_addr		V4_PART_OF_V6(ipif_v6src_addr)
#define	ipif_subnet		V4_PART_OF_V6(ipif_v6subnet)
#define	ipif_net_mask		V4_PART_OF_V6(ipif_v6net_mask)
#define	ipif_brd_addr		V4_PART_OF_V6(ipif_v6brd_addr)
#define	ipif_pp_dst_addr	V4_PART_OF_V6(ipif_v6pp_dst_addr)

/* Macros for easy backreferences to the ill. */
#define	ipif_wq			ipif_ill->ill_wq
#define	ipif_rq			ipif_ill->ill_rq
#define	ipif_net_type		ipif_ill->ill_net_type
#define	ipif_resolver_mp	ipif_ill->ill_resolver_mp
#define	ipif_ipif_up_count	ipif_ill->ill_ipif_up_count
#define	ipif_bcast_mp		ipif_ill->ill_bcast_mp
#define	ipif_index		ipif_ill->ill_index
#define	ipif_type		ipif_ill->ill_type
#define	ipif_isv6		ipif_ill->ill_isv6

/*
 * Fragmentation hash bucket
 */
typedef struct ipfb_s {
	struct ipf_s	*ipfb_ipf;	/* List of ... */
	size_t		ipfb_count;	/* Count of bytes used by frag(s) */
	kmutex_t	ipfb_lock;	/* Protect all ipf in list */
} ipfb_t;

/*
 * IP Lower level Structure.
 * Instance data structure in ip_open when there is a device below us.
 */
typedef struct ill_s {
	struct	ill_s	*ill_next;	/* Chained in at ill_g_head. */
	struct ill_s	**ill_ptpn;	/* Pointer to previous next. */
	queue_t	*ill_rq;		/* Read queue. */
	queue_t	*ill_wq;		/* Write queue. */

	int	ill_error;		/* Error value sent up by device. */

	ipif_t	*ill_ipif;		/* Interface chain for this ILL. */
	uint_t	ill_ipif_up_count;	/* Number of IPIFs currently up. */
	uint_t	ill_max_frag;		/* Max IDU from DLPI. */
	char	*ill_name;		/* Our name. */
	uint_t	ill_name_length;	/* Name length, incl. terminator. */
	char	*ill_ndd_name;		/* Name + ":ip_forwarding" for NDD. */
	uint_t	ill_net_type;		/* IRE_IF_RESOLVER/IRE_IF_NORESOLVER. */
	uint_t	ill_ppa;		/* Physical Point of Attachment num. */
	t_uscalar_t	ill_sap;
	t_scalar_t	ill_sap_length;	/* Including sign (for position) */
	uint_t	ill_phys_addr_length;	/* Excluding the sap. */
	uint_t	ill_bcast_addr_length;	/* Only set when the DL provider */
					/* supports broadcast. */
	t_uscalar_t	ill_mactype;
	uint8_t	*ill_frag_ptr;		/* Reassembly state. */
	timeout_id_t ill_frag_timer_id; /* qtimeout id for the frag timer */
	ipfb_t	*ill_frag_hash_tbl;	/* Fragment hash list head. */

	queue_t	*ill_pending_q;		/* Queue waiting for DL operation */
	ipif_t	*ill_pending_ipif;	/* IPIF waiting for DL operation. */

	ilm_t	*ill_ilm;		/* Multicast mebership for lower ill */
	int	ill_multicast_type;	/* type of router which is querier */
					/* on this interface */

	int	ill_multicast_time;	/* # of slow timeouts since last */
					/* old query */

	/*
	 * All non-NULL cells between 'ill_first_mp_to_free' and
	 * 'ill_last_mp_to_free' are freed in ill_delete.
	 */
#define	ill_first_mp_to_free	ill_bcast_mp
	mblk_t	*ill_bcast_mp;		/* DLPI header for broadcasts. */
	mblk_t	*ill_pending_mp;	/* DL awaiting completion. */
	mblk_t	*ill_resolver_mp;	/* Resolver template. */
	mblk_t	*ill_down_mp;		/* b_next chain prealloced at IFF_UP */
	mblk_t	*ill_dlunit_req;	/* DL_UNITDATA_REQ template, used to */
					/* build nce_resmp for IPv6 */

	mblk_t	*ill_dlpi_deferred;	/* b_next chain of control messages */
	mblk_t	*ill_hw_mp;		/* mblk which holds ill_hw_addr */
#define	ill_last_mp_to_free	ill_hw_mp

	uint8_t	*ill_hw_addr;		/* ill_hw_mp->b_rptr + off */
	inetcksum_t ill_ick;		/* Contains returned ick state */

	uint_t	ill_close_flags;	/* IPCF_* flags for open/close synch */
	uint_t
		ill_needs_attach : 1,
		ill_priv_stream : 1,
		ill_dlpi_pending : 1,	/* Wait for ack for DLPI control msg */
		ill_isv6 : 1,

		ill_name_set : 1,
		ill_dlpi_style_set : 1,
		ill_forwarding : 1,
		ill_ifname_pending : 1,	/* ipif_set_values wait for M_IOCACK */

		ill_pad_to_bit_31 : 24;

	/*
	 * Used in SIOCSIFMUXID and SIOCGIFMUXID for 'ifconfig unplumb'.
	 */
	int	ill_arp_muxid;		/* muxid returned from plink for arp */
	int	ill_ip_muxid;		/* muxid returned from plink for ip */

	/*
	 * Used for IP frag reassembly throttling on a per ILL basis.
	 *
	 * Note: frag_count is approximate, its added to and subtracted from
	 *	 without any locking, so simultaneous load/modify/stores can
	 *	 collide, also ill_frag_purge() recalculates its value by
	 *	 summing all the ipfb_count's without locking out updates
	 *	 to the ipfb's.
	 */
	uint_t	ill_ipf_gen;		/* Generation of next fragment queue */
	size_t	ill_frag_count;		/* Approx count of all mblk bytes */
	int	ill_index;		/* a unique value for each device */
	int	ill_type;		/* From <net/if_types.h> */
	uint_t	ill_dlpi_multicast_state;	/* See below IDMS_* */
	uint_t	ill_dlpi_fastpath_state;	/* See below IDMS_* */

	/*
	 * New fields for IPv6
	 */
	uint8_t	ill_max_hops;	/* Maximum hops for any logical interface */
	uint_t	ill_max_mtu;	/* Maximum MTU for any logical interface */
	uint32_t ill_reachable_time;	/* Value for ND algorithm in msec */
	uint32_t ill_reachable_retrans_time; /* Value for ND algorithm msec */
	uint_t	ill_max_buf;		/* Max # of req to buffer for ND */
	in6_addr_t	ill_token;
	uint_t		ill_token_length;
	uint32_t	ill_xmit_count;		/* ndp max multicast xmits */
	mib2_ipv6IfStatsEntry_t	*ill_ip6_mib;	/* Per interface mib */
	mib2_ipv6IfIcmpEntry_t	*ill_icmp6_mib;	/* Per interface mib */
} ill_t;

/*
 * State for detecting if a driver supports certain features.
 * Support for DL_ENABMULTI_REQ uses ill_dlpi_multicast_state.
 * Support for DLPI M_DATA fastpath uses ill_dlpi_fastpath_state.
 */
#define	IDMS_UNKNOWN	0	/* No DL_ENABMULTI_REQ sent */
#define	IDMS_INPROGRESS	1	/* Sent DL_ENABMULTI_REQ */
#define	IDMS_OK		2	/* DL_ENABMULTI_REQ ok */
#define	IDMS_FAILED	3	/* DL_ENABMULTI_REQ failed */

/* Named Dispatch Parameter Management Structure */
typedef struct ipparam_s {
	uint_t	ip_param_min;
	uint_t	ip_param_max;
	uint_t	ip_param_value;
	char	*ip_param_name;
} ipparam_t;

/*
 * IRE bucket structure. Usually there is an array of such structures,
 * each pointing to a linked list of ires. irb_refcnt counts the number
 * of walkers of a given hash bucket. Usually the reference count is
 * bumped up if the walker wants no IRES to be DELETED while walking the
 * list. Bumping up does not PREVENT ADDITION. This allows walking a given
 * hash bucket without stumbling up on a free pointer.
 */
typedef struct irb {
	struct ire_s	*irb_ire;	/* First ire in this bucket */
					/* Should be first in this struct */
	krwlock_t	irb_lock;	/* Protect this bucket */
	uint_t		irb_refcnt;	/* Protected by irb_lock */
	uchar_t		irb_marks;	/* CONDEMNED ires in this bucket ? */
} irb_t;

/*
 * Following are the macros to increment/decrement the reference
 * count of the IREs and IRBs (ire bucket).
 *
 * 1) We bump up the reference count of an IRE to make sure that
 *    it does not get deleted and freed while we are using it.
 *    Typically all the lookup functions hold the bucket lock,
 *    and look for the IRE. If it finds an IRE, it bumps up the
 *    reference count before dropping the lock. Sometimes we *may* want
 *    to bump up the reference count after we *looked* up i.e without
 *    holding the bucket lock. So, the IRE_REFHOLD macro does not assert
 *    on the bucket lock being held. Any thread trying to delete from
 *    the hash bucket can still do so but cannot free the IRE if
 *    ire_refcnt is not 0.
 *
 * 2) We bump up the reference count on the bucket where the IRE resides
 *    (IRB), when we want to prevent the IREs getting deleted from a given
 *    hash bucket. This makes life easier for ire_walk type functions which
 *    wants to walk the IRE list, call a function, but needs to drop
 *    the bucket lock to prevent recursive rw_enters. While the
 *    lock is dropped, the list could be changed by other threads or
 *    the same thread could end up deleting the ire or the ire pointed by
 *    ire_next. IRE_REFHOLDing the ire or ire_next is not sufficient as
 *    a delete will still remove the ire from the bucket while we have
 *    dropped the lock and hence the ire_next would be NULL. Thus, we
 *    need a mechanism to prevent deletions from a given bucket.
 *
 *    To prevent deletions, we bump up the reference count on the
 *    bucket. If the bucket is held, ire_delete just marks IRE_MARK_CONDEMNED
 *    both on the ire's ire_marks and the bucket's irb_marks. When the
 *    reference count on the bucket drops to zero, all the CONDEMNED ires
 *    are deleted. We don't have to bump up the reference count on the
 *    bucket if we are walking the bucket and never have to drop the bucket
 *    lock. Note that IRB_REFHOLD does not prevent addition of new ires
 *    in the list. It is okay because addition of new ires will not cause
 *    ire_next to point to freed memory. We do IRB_REFHOLD only when
 *    all of the 3 conditions are true :
 *
 *    1) The code needs to walk the IRE bucket from start to end.
 *    2) It may have to drop the bucket lock sometimes while doing (1)
 *    3) It does not want any ires to be deleted meanwhile.
 */

/*
 * Bump up the reference count on the IRE. We cannot assert that the
 * bucket lock is being held as it is legal to bump up the reference
 * count after the first lookup has returned the IRE without
 * holding the lock. Currently ip_wput does this for caching IRE_CACHEs.
 */
#define	IRE_REFHOLD(ire) {				\
	atomic_add_32(&(ire)->ire_refcnt, 1);		\
	ASSERT((ire)->ire_refcnt != 0);			\
}

/*
 * Decrement the reference count on the IRE.
 * In architectures e.g sun4u, where atomic_add_32_nv is just
 * a cas, we need to maintain the right memory barrier semantics
 * as that of mutex_exit i.e all the loads and stores should complete
 * before the cas is executed. membar_exit() does that here.
 *
 * NOTE : This macro is used only in places where we want performance.
 *	  To avoid bloating the code, we use the function "ire_refrele"
 *	  which essentially calls the macro.
 */
#define	IRE_REFRELE(ire) {					\
	ASSERT((ire)->ire_refcnt != 0);				\
	membar_exit();						\
	if (atomic_add_32_nv(&(ire)->ire_refcnt, -1) == 0)	\
		ire_inactive(ire);				\
}

/*
 * Bump up the reference count on the hash bucket - IRB to
 * prevent ires from being deleted in this bucket.
 */
#define	IRB_REFHOLD(irb) {				\
	rw_enter(&(irb)->irb_lock, RW_WRITER);		\
	(irb)->irb_refcnt++;				\
	ASSERT((irb)->irb_refcnt != 0);			\
	rw_exit(&(irb)->irb_lock);			\
}

#define	IRB_REFRELE(irb) {				\
	rw_enter(&(irb)->irb_lock, RW_WRITER);		\
	ASSERT((irb)->irb_refcnt != 0);			\
	if (--(irb)->irb_refcnt	== 0 &&			\
	    ((irb)->irb_marks & IRE_MARK_CONDEMNED)) {	\
		ire_t *ire_list;			\
							\
		ire_list = ire_unlink(irb);		\
		rw_exit(&(irb)->irb_lock);		\
		ASSERT(ire_list != NULL);		\
		ire_cleanup(ire_list);			\
	} else {					\
		rw_exit(&(irb)->irb_lock);		\
	}						\
}

typedef struct ire4 {
	ipaddr_t ire4_src_addr;		/* Source address to use. */
	ipaddr_t ire4_mask;		/* Mask for matching this IRE. */
	ipaddr_t ire4_addr;		/* Address this IRE represents. */
	ipaddr_t ire4_gateway_addr;	/* Gateway if IRE_CACHE/IRE_OFFSUBNET */
	ipaddr_t ire4_cmask;		/* Mask from parent prefix route */
} ire4_t;

typedef struct ire6 {
	in6_addr_t ire6_src_addr;	/* Source address to use. */
	in6_addr_t ire6_mask;		/* Mask for matching this IRE. */
	in6_addr_t ire6_addr;		/* Address this IRE represents. */
	in6_addr_t ire6_gateway_addr;	/* Gateway if IRE_CACHE/IRE_OFFSUBNET */
	in6_addr_t ire6_cmask;		/* Mask from parent prefix route */
} ire6_t;

typedef union ire_addr {
	ire6_t	ire6_u;
	ire4_t	ire4_u;
} ire_addr_u_t;

/* Internet Routing Entry */
typedef struct ire_s {
	struct	ire_s	*ire_next;	/* The hash chain must be first. */
	struct	ire_s	**ire_ptpn;	/* Pointer to previous next. */
	uint32_t	ire_refcnt;	/* Number of references */
	mblk_t		*ire_mp;	/* mblk we are in. */
	mblk_t		*ire_fp_mp;	/* Fast path header */
	queue_t		*ire_rfq;	/* recv from this queue */
	queue_t		*ire_stq;	/* send to this queue */
	uint_t		ire_max_frag;	/* MTU (next hop or path). */
	uint32_t	ire_frag_flag;	/* IPH_DF or zero. */
	uint32_t	ire_ident;	/* Per IRE IP ident. */
	uint32_t	ire_tire_mark;	/* Used for reclaim of unused. */
	uchar_t		ire_ipversion;	/* IPv4/IPv6 version */
	uchar_t		ire_marks;	/* IRE_MARK_CONDEMNED */
	ushort_t	ire_type;	/* Type of IRE */
	uint_t	ire_ib_pkt_count;	/* Inbound packets for ire_addr */
	uint_t	ire_ob_pkt_count;	/* Outbound packets to ire_addr */
	uint_t	ire_ll_hdr_length;	/* Non-zero if we do M_DATA prepends */
	time_t	ire_create_time;	/* Time (in secs) IRE was created. */
	mblk_t		*ire_dlureq_mp;	/* DL_UNIT_DATA_REQ/RESOLVER mp */
	uint32_t	ire_phandle;	/* Associate prefix IREs to cache */
	uint32_t	ire_ihandle;	/* Associate interface IREs to cache */
	ipif_t		*ire_ipif;	/* the interface that this ire uses */
	uint32_t	ire_flags;	/* flags related to route (RTF_*) */
	uint_t	ire_ipsec_options_size;	/* IPSEC options size */
	struct	nce_s	*ire_nce;	/* Neighbor Cache Entry for IPv6 */
	uint_t		ire_masklen;	/* # bits in ire_mask{,_v6} */
	ire_addr_u_t	ire_u;		/* IPv4/IPv6 address info. */

	irb_t		*ire_bucket;	/* Hash bucket when ire_ptphn is set */
	iulp_t		ire_uinfo;	/* Upper layer protocol info. */
	/*
	 * Protects ire_uinfo, ire_max_frag, and ire_frag_flag.
	 */
	kmutex_t	ire_lock;
} ire_t;

/* IPv4 compatiblity macros */
#define	ire_src_addr		ire_u.ire4_u.ire4_src_addr
#define	ire_mask		ire_u.ire4_u.ire4_mask
#define	ire_addr		ire_u.ire4_u.ire4_addr
#define	ire_gateway_addr	ire_u.ire4_u.ire4_gateway_addr
#define	ire_cmask		ire_u.ire4_u.ire4_cmask

#define	ire_src_addr_v6		ire_u.ire6_u.ire6_src_addr
#define	ire_mask_v6		ire_u.ire6_u.ire6_mask
#define	ire_addr_v6		ire_u.ire6_u.ire6_addr
#define	ire_gateway_addr_v6	ire_u.ire6_u.ire6_gateway_addr
#define	ire_cmask_v6		ire_u.ire6_u.ire6_cmask

/* Convenient typedefs for sockaddrs */
typedef	struct sockaddr_in	sin_t;
typedef	struct sockaddr_in6	sin6_t;

/* Address structure used for internal bind with IP */
typedef struct ipa_conn_s {
	ipaddr_t	ac_laddr;
	ipaddr_t	ac_faddr;
	uint16_t	ac_fport;
	uint16_t	ac_lport;
} ipa_conn_t;

typedef struct ipa6_conn_s {
	in6_addr_t	ac6_laddr;
	in6_addr_t	ac6_faddr;
	uint16_t	ac6_fport;
	uint16_t	ac6_lport;
} ipa6_conn_t;

/* Name/Value Descriptor. */
typedef struct nv_s {
	int	nv_value;
	char	*nv_name;
} nv_t;

/* IP Forwarding Ticket */
typedef	struct ipftk_s {
	queue_t	*ipftk_queue;
	ipaddr_t ipftk_dst;
} ipftk_t;

typedef struct ipt_s {
	pfv_t	func;		/* Routine to call */
	uchar_t	*arg;		/* ire or nce passed in */
} ipt_t;

#define	ILL_FRAG_HASH(s, i) \
	((ntohl(s) ^ ((i) ^ ((i) >> 8))) % ILL_FRAG_HASH_TBL_COUNT)

/*
 * Per-packet information for received packets and transmitted.
 * Used by the transport protocols when converting between the packet
 * and ancillary data and socket options.
 *
 * Note: This private data structure and related IPPF_* constant
 * definitions are exposed to enable compilation of some debugging tools
 * like lsof which use struct tcp_t in <inet/tcp.h>. This is intended to be
 * a temporary hack and long term alternate interfaces should be defined
 * to support the needs of such tools and private definitions moved to
 * private headers.
 */
struct ip6_pkt_s {
	uint_t		ipp_fields;		/* Which fields are valid */
	uint_t		ipp_ifindex;		/* pktinfo ifindex */
	in6_addr_t	ipp_addr;		/* pktinfo src/dst addr */
	uint_t		ipp_hoplimit;
	uint_t		ipp_hopoptslen;
	uint_t		ipp_rtdstoptslen;
	uint_t		ipp_rthdrlen;
	uint_t		ipp_dstoptslen;
	ip6_hbh_t	*ipp_hopopts;
	ip6_dest_t	*ipp_rtdstopts;
	ip6_rthdr_t	*ipp_rthdr;
	ip6_dest_t	*ipp_dstopts;
	in6_addr_t	ipp_nexthop;		/* Transmit only */
};
typedef struct ip6_pkt_s ip6_pkt_t;

/* ipp_fields values */
#define	IPPF_IFINDEX	0x0001	/* Part of in6_pktinfo: ifindex */
#define	IPPF_ADDR	0x0002	/* Part of in6_pktinfo: src/dst addr */
#define	IPPF_SCOPE_ID	0x0004	/* Add xmit ip6i_t for sin6_scope_id */
#define	IPPF_NO_CKSUM	0x0008	/* Add xmit ip6i_t for IP6I_NO_*_CKSUM */

#define	IPPF_RAW_CKSUM	0x0010	/* Add xmit ip6i_t for IP6I_RAW_CHECKSUM */
#define	IPPF_HOPLIMIT	0x0020
#define	IPPF_HOPOPTS	0x0040
#define	IPPF_RTHDR	0x0080

#define	IPPF_RTDSTOPTS	0x0100
#define	IPPF_DSTOPTS	0x0200
#define	IPPF_NEXTHOP	0x0400

#define	IPPF_HAS_IP6I \
	(IPPF_IFINDEX|IPPF_ADDR|IPPF_NEXTHOP|IPPF_SCOPE_ID| \
	IPPF_NO_CKSUM|IPPF_RAW_CKSUM)

#define	TCP_PORTS_OFFSET	0
#define	UDP_PORTS_OFFSET	0

/*
 * Note: TCP, UDP and "proto" have separate fanout tables for IPv6.
 * IP_TCP_CONN_HASH_SIZE must be a multiple of 2.
 */
#define	IP_TCP_CONN_HASH_SIZE 512
extern uint_t ipc_tcp_conn_hash_size;
#define	IP_TCP_CONN_HASH(ip_src, ports) \
	((unsigned)(ntohl(ip_src) ^ (ports >> 24) ^ (ports >> 16) \
	^ (ports >> 8) ^ ports) & (ipc_tcp_conn_hash_size - 1))
#define	IP_TCP_LISTEN_HASH(lport)	\
	((unsigned)(((lport) >> 8) ^ (lport)) % A_CNT(ipc_tcp_listen_fanout))

/*
 * Assumes that the caller passes in <fport, lport> as the uint32_t
 * parameter "ports".
 */
#define	IP_TCP_CONN_MATCH(ipc, ipha, ports)			\
	((ipc)->ipc_ports == (ports) &&				\
	    (ipc)->ipc_faddr == (ipha)->ipha_src &&		\
	    (ipc)->ipc_laddr == (ipha)->ipha_dst)

#define	IP_TCP_LISTEN_MATCH(ipc, lport, laddr)			\
	(((ipc)->ipc_lport == (lport)) &&			\
	    (((ipc)->ipc_laddr == 0) ||				\
	    ((ipc)->ipc_laddr == (laddr))))

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
#define	IP_UDP_MATCH(ipc, lport, laddr, fport, faddr)		\
	(((ipc)->ipc_lport == (lport)) &&			\
	    (((ipc)->ipc_laddr == 0) ||				\
	    (((ipc)->ipc_laddr == (laddr)) &&			\
	    (((ipc)->ipc_faddr == 0) ||				\
		((ipc)->ipc_faddr == (faddr) && (ipc)->ipc_fport == (fport))))))

/*
 * Checks ipc_wantpacket for multicast.
 *
 * ip_fanout_proto() needs to check just the IPv4 fanout.
 * This depends on the order of insertion in ip_bind() to ensure
 * that the most specific matches are first.  Thus the insertion order
 * in the fanout buckets must be:
 *	1) Fully specified ICMP connection (source and dest)
 *	2) Bound to a local IP address
 *	3) Bound to INADDR_ANY
 */
#define	IP_PROTO_MATCH(ipc, protocol, laddr, faddr)			\
	((((ipc)->ipc_laddr == 0) ||					\
	(((ipc)->ipc_laddr == (laddr)) &&				\
	(((ipc)->ipc_faddr == 0) ||					\
	((ipc)->ipc_faddr == (faddr))))) &&				\
	(ipc_wantpacket(ipc, ill, laddr) ||				\
	(protocol == IPPROTO_PIM) || (protocol == IPPROTO_RSVP)))

extern struct ill_s *ill_g_head;	/* ILL List Head */

extern ill_t	*ip_timer_ill;		/* ILL for IRE expiration timer. */
extern timeout_id_t ip_ire_expire_id;	/* IRE expiration timeout id. */
extern timeout_id_t ip_ire_reclaim_id;	/* IRE recalaim timeout id. */
extern ill_t	*ill_ire_gc;		/* ILL used for ire memory reclaim */
extern time_t	ip_ire_time_elapsed;	/* Time since IRE cache last flushed */

extern kmutex_t	ip_mi_lock;

/*
 * The following are the variables for IGMP and MLD timers
 * Both timers work in a similar fashion. At system
 * initialization a single igmp_timer_ill is created for the
 * very first IPv4 ill. If the ill is then removed, the mp
 * is simply repointed to another IPv4 ill. IP uses mi_timer
 * function to send a "times up" notification (using
 * igmp_timer_mp) in igmp_timer_interval milliseconds to
 * ip_wsrv function. The calculation of the
 * igmp_timer_interval variable is done by the macro
 * IGMP_TIMEOUT_INTERVAL. Note that this macro is needed
 * for igmp to convert the Max Response time units of a
 * tenth of a second (as specified by the IGMPv2 spec) to
 * milliseconds. This interval variable is not required for
 * MLD since the spec specifies Max Response Delay time in
 * milliseconds. The ip_wsrv then calls igmp_timeout function
 * to process the notification, by calling igmp_timeout_handler
 * routine
*/
extern ill_t	*igmp_timer_ill;	/* ILL for IGMP timer. */
extern mblk_t	*igmp_timer_mp;		/* IGMP timer */
extern int	igmp_timer_interval;
extern ill_t	*mld_timer_ill;		/* ILL for MLD timer. */
extern mblk_t	*mld_timer_mp;		/* MLD timer */

extern struct kmem_cache *ire_cache;

extern uint_t	ip_ire_default_count;	/* Number of IPv4 IRE_DEFAULT entries */
extern uint_t	ip_ire_default_index;	/* Walking index used to mod in */

extern ill_t	*proxy_frag_ill;	/* ILL for frags to proxies. */

extern ipaddr_t	ip_g_all_ones;
extern caddr_t	ip_g_nd;		/* Named Dispatch List Head */

extern int	ip_max_mtu;		/* Used by udp/icmp */

extern ipparam_t	*ip_param_arr;

extern int ip_g_forward;

#define	ip_respond_to_address_mask_broadcast ip_param_arr[0].ip_param_value
#define	ip_g_resp_to_address_mask	ip_param_arr[0].ip_param_value
#define	ip_g_send_redirects		ip_param_arr[4].ip_param_value
#define	ip_debug			ip_param_arr[6].ip_param_value
#define	ip_mrtdebug			ip_param_arr[7].ip_param_value
#define	ip_timer_interval		ip_param_arr[8].ip_param_value
#define	ip_ire_arp_interval		ip_param_arr[9].ip_param_value
#define	ip_def_ttl			ip_param_arr[11].ip_param_value
#define	ip_wroff_extra			ip_param_arr[13].ip_param_value
#define	ip_path_mtu_discovery		ip_param_arr[16].ip_param_value
#define	ip_ignore_delete_time		ip_param_arr[17].ip_param_value
#define	ip_output_queue			ip_param_arr[19].ip_param_value
#define	ip_broadcast_ttl		ip_param_arr[20].ip_param_value
#define	ip_icmp_err_interval		ip_param_arr[21].ip_param_value
#define	ip_icmp_err_burst		ip_param_arr[22].ip_param_value
#define	ip_reass_queue_bytes		ip_param_arr[23].ip_param_value
#define	ip_addrs_per_if			ip_param_arr[25].ip_param_value

#define	delay_first_probe_time		ip_param_arr[29].ip_param_value
#define	max_unicast_solicit		ip_param_arr[30].ip_param_value
#define	ipv6_def_hops			ip_param_arr[31].ip_param_value
#define	ipv6_icmp_return		ip_param_arr[32].ip_param_value
#define	ipv6_forward			ip_param_arr[33].ip_param_value
#define	ipv6_forward_src_routed		ip_param_arr[34].ip_param_value
#define	ipv6_resp_echo_mcast		ip_param_arr[35].ip_param_value
#define	ipv6_send_redirects		ip_param_arr[36].ip_param_value
#define	ipv6_ignore_redirect		ip_param_arr[37].ip_param_value
#define	ipv6_strict_dst_multihoming	ip_param_arr[38].ip_param_value
#define	ip_ire_reclaim_fraction		ip_param_arr[39].ip_param_value
#define	ipsec_policy_log_interval	ip_param_arr[40].ip_param_value
#ifdef DEBUG
#define	ipv6_drop_inbound_icmpv6	ip_param_arr[42].ip_param_value
#else
#define	ipv6_drop_inbound_icmpv6	0
#endif

extern int	ip_enable_group_ifs;
extern hrtime_t	ipsec_policy_failure_last;

extern int	dohwcksum;	/* use h/w cksum if supported by the h/w */
#ifdef ZC_TEST
extern int	noswcksum;
#endif

extern uint_t	ipif_g_count;			/* Count of IPIFs "up". */
extern char	ipif_loopback_name[];

extern nv_t	*ire_nv_tbl;

extern time_t	ip_g_frag_timeout;
extern clock_t	ip_g_frag_timo_ms;

extern mib2_ip_t	ip_mib;	/* For tcpInErrs and udpNoPorts */

extern struct module_info ip_mod_info;

extern timeout_id_t	igmp_slowtimeout_id;

extern icf_t rts_clients;

extern uint_t	loopback_packets;

/*
 * Network byte order macros
 */
#ifdef	_BIG_ENDIAN
#define	N_IN_CLASSD_NET		IN_CLASSD_NET
#define	N_INADDR_UNSPEC_GROUP	INADDR_UNSPEC_GROUP
#else /* _BIG_ENDIAN */
#define	N_IN_CLASSD_NET		(ipaddr_t)0x000000f0U
#define	N_INADDR_UNSPEC_GROUP	(ipaddr_t)0x000000e0U
#endif /* _BIG_ENDIAN */
#define	CLASSD(addr)	(((addr) & N_IN_CLASSD_NET) == N_INADDR_UNSPEC_GROUP)

#ifdef IP_DEBUG
#include <sys/debug.h>
#include <sys/promif.h>

#define	ip0dbg(a)	printf a
#define	ip1dbg(a)	if (ip_debug > 2) printf a
#define	ip2dbg(a)	if (ip_debug > 3) printf a
#define	ip3dbg(a)	if (ip_debug > 4) printf a
#define	ipcsumdbg(a, b) \
	if (ip_debug == 1) \
		prom_printf(a); \
	else if (ip_debug > 1) \
		{ prom_printf("mp=%p\n", (void *)b); debug_enter(a); }
#else
#define	ip0dbg(a)	/* */
#define	ip1dbg(a)	/* */
#define	ip2dbg(a)	/* */
#define	ip3dbg(a)	/* */
#define	ipcsumdbg(a, b)	/* */
#endif	/* IP_DEBUG */

extern char	*dlpi_prim_str(int);
extern void	ill_frag_timer(void *);
extern mblk_t	*ip_carve_mp(mblk_t **, ssize_t);
extern mblk_t	*ip_dlpi_alloc(size_t, t_uscalar_t);
extern char	*ip_dot_addr(ipaddr_t, char *);
extern mblk_t	*ip_timer_alloc(pfv_t, uchar_t *, int);
extern boolean_t icmp_err_rate_limit(void);
extern void	icmp_time_exceeded(queue_t *, mblk_t *, uint8_t);
extern void	icmp_unreachable(queue_t *, mblk_t *, uint8_t);
extern int	ip_bind_connected(ipc_t *, mblk_t *, ipaddr_t, uint16_t,
		    ipaddr_t, uint16_t, boolean_t, boolean_t, boolean_t);
extern int	ip_bind_laddr(ipc_t *, mblk_t *, ipaddr_t, uint16_t,
		    boolean_t, boolean_t, boolean_t);
extern uint_t	ip_cksum(mblk_t *, int, uint32_t);
extern int	ip_close(queue_t *);
extern uint16_t	ip_csum_hdr(ipha_t *);
extern void	ip_fanout_tcp_defq(queue_t *, mblk_t *, uint_t, boolean_t);
extern int	ip_hdr_complete(ipha_t *);
extern int	ip_ipc_report(queue_t *, mblk_t *, void *);
extern void	ip_ire_fini(void);
extern void	ip_ire_init(void);
extern int	ip_open(queue_t *, dev_t *, int, int, cred_t *);
extern boolean_t	ip_reassemble(mblk_t *, ipf_t *, uint_t, boolean_t,
			    uint_t, ill_t *);
extern void	ip_rput(queue_t *, mblk_t *);
extern void	ip_rput(queue_t *, mblk_t *);
extern void	ip_rput_dlpi(queue_t *, mblk_t *);
extern void	ip_rput_forward(ire_t *, ipha_t *, mblk_t *);
extern void	ip_rput_forward(ire_t *, ipha_t *, mblk_t *);
extern void	ip_rput_forward_multicast(ipaddr_t, mblk_t *, ipif_t *);
extern void	ip_rput_local(queue_t *, mblk_t *, ipha_t *, ire_t *, uint_t);
extern void	ip_rput_other(queue_t *, mblk_t *);
extern void	ip_rsrv(queue_t *);
extern void	ip_setqinfo(queue_t *, boolean_t, boolean_t);
extern void	ip_trash_ire_reclaim(void *);
extern void	ip_trash_timer_expire(void *);
extern void	ip_wput(queue_t *, mblk_t *);
extern void	ip_wput_ire(queue_t *, mblk_t *, ire_t *, ipc_t *);
extern void	ip_wput_local(queue_t *, ill_t *, ipha_t *, mblk_t *, int);
extern void	ip_wput_multicast(queue_t *, mblk_t *, ipif_t *);
extern void	ip_wput_nondata(queue_t *, mblk_t *);
extern void	ip_wsrv(queue_t *);
extern void	ipc_hash_insert_bound(icf_t *, ipc_t *);
extern void	ipc_hash_insert_connected(icf_t *, ipc_t *);
extern void	ipc_hash_insert_wildcard(icf_t *, ipc_t *);
extern void	ipc_walk(pfv_t, void *);
extern char	*ip_nv_lookup(nv_t *, int);
extern boolean_t ip_local_addr_ok_v6(const in6_addr_t *, const in6_addr_t *);
extern boolean_t ip_remote_addr_ok_v6(const in6_addr_t *, const in6_addr_t *);
extern ipaddr_t ip_massage_options(ipha_t *);
extern ipaddr_t ip_net_mask(ipaddr_t);

extern struct qinit rinit_ipv6;
extern struct qinit winit_ipv6;

extern	int	ipc_ipsec_length(ipc_t *ipc);
extern void	ip_wput_ipsec_out(queue_t *, mblk_t *);
extern ipaddr_t	ip_get_dst(ipha_t *);
extern boolean_t ipsec_check_global_policy(mblk_t *, ipc_t *);
extern mblk_t	*ipsec_in_alloc();
extern mblk_t	*ipsec_in_to_out(mblk_t *);
extern boolean_t ipsec_inbound_accept_clear(mblk_t *);
extern boolean_t ipsec_in_is_secure(mblk_t *);
extern boolean_t ipsec_check_ipsecin_policy(char *, queue_t *, mblk_t *,
    ipsec_req_t *);
extern void ipsec_log_policy_failure(queue_t *, int, char *, ipha_t *,
    boolean_t);

extern void	ire_cleanup(ire_t *);
extern void	ire_inactive(ire_t *);
extern ire_t	*ire_unlink(irb_t *);

extern int	ip_srcid_insert(const in6_addr_t *);
extern int	ip_srcid_remove(const in6_addr_t *);
extern void	ip_srcid_find_id(uint_t, in6_addr_t *);
extern uint_t	ip_srcid_find_addr(const in6_addr_t *);
extern int	ip_srcid_report(queue_t *, mblk_t *, void *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_H */
