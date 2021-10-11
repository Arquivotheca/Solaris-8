/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_NETINET_ICMP6_H
#define	_NETINET_ICMP6_H

#pragma ident	"@(#)icmp6.h	1.2	99/08/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Type and code definitions for ICMPv6.
 * Based on RFC2292.
 */

#define	ICMP6_INFOMSG_MASK		0x80 /* all informational messages */

/* Minimum ICMPv6 header length. */
#define	ICMP6_MINLEN	8

typedef struct icmp6_hdr {
	uint8_t	 icmp6_type;	/* type field */
	uint8_t	 icmp6_code;	/* code field */
	uint16_t icmp6_cksum;	/* checksum field */
	union {
		uint32_t icmp6_un_data32[1];	/* type-specific field */
		uint16_t icmp6_un_data16[2];	/* type-specific field */
		uint8_t	 icmp6_un_data8[4];	/* type-specific field */
	} icmp6_dataun;
} icmp6_t;

#define	icmp6_data32	icmp6_dataun.icmp6_un_data32
#define	icmp6_data16	icmp6_dataun.icmp6_un_data16
#define	icmp6_data8	icmp6_dataun.icmp6_un_data8
#define	icmp6_pptr	icmp6_data32[0]	/* parameter prob */
#define	icmp6_mtu	icmp6_data32[0]	/* packet too big */
#define	icmp6_id	icmp6_data16[0]	/* echo request/reply */
#define	icmp6_seq	icmp6_data16[1]	/* echo request/reply */
#define	icmp6_maxdelay	icmp6_data16[0]	/* mcast group membership */

/* For the Multicast Listener Discovery messages */
typedef struct icmp6_mld {
	icmp6_t		icmp6m_hdr;
	struct in6_addr	icmp6m_group; /* group address */
} icmp6_mld_t;

#define	mld_type	icmp6m_hdr.icmp6_type
#define	mld_code	icmp6m_hdr.icmp6_code
#define	mld_cksum	icmp6m_hdr.icmp6_cksum
#define	mld_maxdelay	icmp6m_hdr.icmp6_data16[0]
#define	mld_reserved	icmp6m_hdr.icmp6_data16[1]
#define	mld_group_addr	icmp6m_group

/* ICMPv6 error types */
#define	ICMP6_DST_UNREACH		1
#define	ICMP6_PACKET_TOO_BIG		2
#define	ICMP6_TIME_EXCEEDED		3
#define	ICMP6_PARAM_PROB		4

/* ICMPv6 query types */
#define	ICMP6_ECHO_REQUEST		128
#define	ICMP6_ECHO_REPLY		129

/* ICMPv6 group membership types */
#define	ICMP6_MEMBERSHIP_QUERY		130
#define	ICMP6_MEMBERSHIP_REPORT		131
#define	ICMP6_MEMBERSHIP_REDUCTION	132

/* types for neighbor discovery */
#define	ND_ROUTER_SOLICIT		133
#define	ND_ROUTER_ADVERT		134
#define	ND_NEIGHBOR_SOLICIT		135
#define	ND_NEIGHBOR_ADVERT		136
#define	ND_REDIRECT			137
#define	ICMP6_MAX_INFO_TYPE		137

#define	ICMP6_IS_ERROR(x) ((x) < 128)

/* codes for ICMP6_DST_UNREACH */
#define	ICMP6_DST_UNREACH_NOROUTE	0 /* no route to destination */
#define	ICMP6_DST_UNREACH_ADMIN		1 /* communication with destination */
					/* administratively prohibited */
#define	ICMP6_DST_UNREACH_NOTNEIGHBOR	2 /* not a neighbor */
#define	ICMP6_DST_UNREACH_ADDR		3 /* address unreachable */
#define	ICMP6_DST_UNREACH_NOPORT	4 /* bad port */

/* codes for ICMP6_TIME_EXCEEDED */
#define	ICMP6_TIME_EXCEED_TRANSIT	0 /* Hop Limit == 0 in transit */
#define	ICMP6_TIME_EXCEED_REASSEMBLY	1 /* Reassembly time out */

/* codes for ICMP6_PARAM_PROB */
#define	ICMP6_PARAMPROB_HEADER		0 /* erroneous header field */
#define	ICMP6_PARAMPROB_NEXTHEADER	1 /* unrecognized Next Header */
#define	ICMP6_PARAMPROB_OPTION		2 /* unrecognized IPv6 option */

/* Default MLD max report delay value */
#define	ICMP6_MAX_HOST_REPORT_DELAY	10	/* max delay for response to */
						/* query (in seconds)   */

typedef struct nd_router_solicit {	/* router solicitation */
	icmp6_t		nd_rs_hdr;
	/* could be followed by options */
} nd_router_solicit_t;

#define	nd_rs_type	nd_rs_hdr.icmp6_type
#define	nd_rs_code	nd_rs_hdr.icmp6_code
#define	nd_rs_cksum	nd_rs_hdr.icmp6_cksum
#define	nd_rs_reserved	nd_rs_hdr.icmp6_data32[0]

typedef struct nd_router_advert {	/* router advertisement */
	icmp6_t		nd_ra_hdr;
	uint32_t	nd_ra_reachable;   /* reachable time */
	uint32_t	nd_ra_retransmit;  /* retransmit timer */
	/* could be followed by options */
} nd_router_advert_t;

#define	nd_ra_type		nd_ra_hdr.icmp6_type
#define	nd_ra_code		nd_ra_hdr.icmp6_code
#define	nd_ra_cksum		nd_ra_hdr.icmp6_cksum
#define	nd_ra_curhoplimit	nd_ra_hdr.icmp6_data8[0]
#define	nd_ra_flags_reserved	nd_ra_hdr.icmp6_data8[1]

#define	ND_RA_FLAG_OTHER	0x40
#define	ND_RA_FLAG_MANAGED	0x80

#define	nd_ra_router_lifetime    nd_ra_hdr.icmp6_data16[1]

typedef struct nd_neighbor_solicit {   /* neighbor solicitation */
	icmp6_t		nd_ns_hdr;
	struct in6_addr nd_ns_target; /* target address */
	/* could be followed by options */
} nd_neighbor_solicit_t;

#define	nd_ns_type		nd_ns_hdr.icmp6_type
#define	nd_ns_code		nd_ns_hdr.icmp6_code
#define	nd_ns_cksum		nd_ns_hdr.icmp6_cksum
#define	nd_ns_reserved		nd_ns_hdr.icmp6_data32[0]

typedef struct nd_neighbor_advert {	/* neighbor advertisement */
	icmp6_t		  nd_na_hdr;
	struct in6_addr   nd_na_target; /* target address */
	/* could be followed by options */
} nd_neighbor_advert_t;

#define	nd_na_type	nd_na_hdr.icmp6_type
#define	nd_na_code	nd_na_hdr.icmp6_code
#define	nd_na_cksum	nd_na_hdr.icmp6_cksum

#define	nd_na_flags_reserved	nd_na_hdr.icmp6_data32[0]

/*
 * The first three bits of the flgs_reserved field of the ND structure are
 * defined in this order:
 *	Router flag
 *	Solicited flag
 * 	Override flag
 */

/* Save valuable htonl() cycles on little-endian boxen. */

#ifdef _BIG_ENDIAN

#define	ND_NA_FLAG_ROUTER	0x80000000
#define	ND_NA_FLAG_SOLICITED	0x40000000
#define	ND_NA_FLAG_OVERRIDE	0x20000000

#else /* _BIG_ENDIAN */

#define	ND_NA_FLAG_ROUTER	0x80
#define	ND_NA_FLAG_SOLICITED	0x40
#define	ND_NA_FLAG_OVERRIDE	0x20

#endif /* _BIG_ENDIAN */

typedef struct nd_redirect {	/* redirect */
	icmp6_t		nd_rd_hdr;
	struct in6_addr	nd_rd_target; /* target address */
	struct in6_addr	nd_rd_dst;    /* destination address */
	/* could be followed by options */
} nd_redirect_t;

#define	nd_rd_type	nd_rd_hdr.icmp6_type
#define	nd_rd_code	nd_rd_hdr.icmp6_code
#define	nd_rd_cksum	nd_rd_hdr.icmp6_cksum
#define	nd_rd_reserved	nd_rd_hdr.icmp6_data32[0]

typedef struct nd_opt_hdr {	/* Neighbor discovery option header */
	uint8_t	nd_opt_type;
	uint8_t	nd_opt_len;	/* in units of 8 octets */
	/* followed by option specific data */
} nd_opt_hdr_t;

/* Neighbor discovery option types */
#define	ND_OPT_SOURCE_LINKADDR		1
#define	ND_OPT_TARGET_LINKADDR		2
#define	ND_OPT_PREFIX_INFORMATION	3
#define	ND_OPT_REDIRECTED_HEADER	4
#define	ND_OPT_MTU			5

typedef struct nd_opt_prefix_info {	/* prefix information */
	uint8_t   nd_opt_pi_type;
	uint8_t   nd_opt_pi_len;
	uint8_t   nd_opt_pi_prefix_len;
	uint8_t   nd_opt_pi_flags_reserved;
	uint32_t  nd_opt_pi_valid_time;
	uint32_t  nd_opt_pi_preferred_time;
	uint32_t  nd_opt_pi_reserved2;
	struct in6_addr  nd_opt_pi_prefix;
} nd_opt_prefix_info_t;

#define	ND_OPT_PI_FLAG_AUTO	0x40
#define	ND_OPT_PI_FLAG_ONLINK	0x80

typedef struct nd_opt_rd_hdr {	/* redirected header */
	uint8_t   nd_opt_rh_type;
	uint8_t   nd_opt_rh_len;
	uint16_t  nd_opt_rh_reserved1;
	uint32_t  nd_opt_rh_reserved2;
	/* followed by IP header and data */
} nd_opt_rd_hdr_t;

typedef struct nd_opt_mtu {	/* MTU option */
	uint8_t   nd_opt_mtu_type;
	uint8_t   nd_opt_mtu_len;
	uint16_t  nd_opt_mtu_reserved;
	uint32_t  nd_opt_mtu_mtu;
} nd_opt_mtu_t;

/* Note: the option is variable length (at least 8 bytes long) */
#ifndef ND_MAX_HDW_LEN
#define	ND_MAX_HDW_LEN	64
#endif
struct nd_opt_lla {
	uint8_t	nd_opt_lla_type;
	uint8_t	nd_opt_lla_len;	/* in units of 8 octets */
	uint8_t	nd_opt_lla_hdw_addr[ND_MAX_HDW_LEN];
};


/* Neighbor discovery protocol constants */

/* Router constants */
#define	ND_MAX_INITIAL_RTR_ADVERT_INTERVAL	16000	/* milliseconds */
#define	ND_MAX_INITIAL_RTR_ADVERTISEMENTS	3	/* transmissions */
#define	ND_MAX_FINAL_RTR_ADVERTISEMENTS		3	/* transmissions */
#define	ND_MIN_DELAY_BETWEEN_RAS		3000	/* milliseconds */
#define	ND_MAX_RA_DELAY_TIME			500	/* milliseconds */

/* Host constants */
#define	ND_MAX_RTR_SOLICITATION_DELAY		1000	/* milliseconds */
#define	ND_RTR_SOLICITATION_INTERVAL		4000	/* milliseconds */
#define	ND_MAX_RTR_SOLICITATIONS		3	/* transmissions */

/* Node constants */
#define	ND_MAX_MULTICAST_SOLICIT		3	/* transmissions */
#define	ND_MAX_UNICAST_SOLICIT			3	/* transmissions */
#define	ND_MAX_ANYCAST_DELAY_TIME		1000	/* milliseconds */
#define	ND_MAX_NEIGHBOR_ADVERTISEMENT		3	/* transmissions */
#define	ND_REACHABLE_TIME			30000	/* milliseconds */
#define	ND_RETRANS_TIMER			1000	/* milliseconds */
#define	ND_DELAY_FIRST_PROBE_TIME		5000	/* milliseconds */
#define	ND_MIN_RANDOM_FACTOR			.5
#define	ND_MAX_RANDOM_FACTOR			1.5

#define	ND_MAX_REACHTIME			3600000	/* milliseconds */
#define	ND_MAX_REACHRETRANSTIME			100000	/* milliseconds */

/*
 * ICMPv6 type filtering for IPPROTO_ICMPV6 ICMP6_FILTER socket option
 */
#define	ICMP6_FILTER	0x01	/* Set filter */

typedef struct icmp6_filter {
	uint32_t	__icmp6_filt[8];
} icmp6_filter_t;

/* Pass all ICMPv6 messages to the application */
#define	ICMP6_FILTER_SETPASSALL(filterp) ( \
	((filterp)->__icmp6_filt[0] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[1] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[2] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[3] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[4] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[5] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[6] = 0xFFFFFFFFU), \
	((filterp)->__icmp6_filt[7] = 0xFFFFFFFFU))

/* ICMPv6 messages are blocked from being passed to the application */
#define	ICMP6_FILTER_SETBLOCKALL(filterp) ( \
	((filterp)->__icmp6_filt[0] = 0x0), \
	((filterp)->__icmp6_filt[1] = 0x0), \
	((filterp)->__icmp6_filt[2] = 0x0), \
	((filterp)->__icmp6_filt[3] = 0x0), \
	((filterp)->__icmp6_filt[4] = 0x0), \
	((filterp)->__icmp6_filt[5] = 0x0), \
	((filterp)->__icmp6_filt[6] = 0x0), \
	((filterp)->__icmp6_filt[7] = 0x0))

/* Pass messages of a given type to the application */
#define	ICMP6_FILTER_SETPASS(type, filterp) \
	((((filterp)->__icmp6_filt[(type) >> 5]) |= (1 << ((type) & 31))))

/* Block messages of a given type from being passed to the application */
#define	ICMP6_FILTER_SETBLOCK(type, filterp) \
	((((filterp)->__icmp6_filt[(type) >> 5]) &= ~(1 << ((type) & 31))))

/* Test if message of a given type will be passed to an application */
#define	ICMP6_FILTER_WILLPASS(type, filterp) \
	((((filterp)->__icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) != 0)

/*
 * Test if message of a given type will blocked from
 * being passed to an application
 */
#define	ICMP6_FILTER_WILLBLOCK(type, filterp) \
	((((filterp)->__icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) == 0)


#ifdef	__cplusplus
}
#endif

#endif /* _NETINET_ICMP6_H */
