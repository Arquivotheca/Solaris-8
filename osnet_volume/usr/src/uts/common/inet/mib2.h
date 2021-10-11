/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_MIB2_H
#define	_INET_MIB2_H

#pragma ident	"@(#)mib2.h	1.20	99/08/19 SMI"

#include <netinet/in.h>	/* For in6_addr_t */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The IPv6 parts of this are derived from:
 *	RFC 2465
 *	RFC 2466
 *	RFC 2452
 *	RFC 2454
 */

/*
 * SNMP set/get via M_PROTO T_OPTMGMT_REQ.  Structure is that used
 * for [gs]etsockopt() calls.  get uses T_CURRENT, set uses T_NEOGTIATE
 * MGMT_flags value.  The following definition of opthdr is taken from
 * socket.h:
 *
 * An option specification consists of an opthdr, followed by the value of
 * the option.  An options buffer contains one or more options.  The len
 * field of opthdr specifies the length of the option value in bytes.  This
 * length must be a multiple of sizeof(long) (use OPTLEN macro).
 *
 * struct opthdr {
 *	long	level;	protocol level affected
 *	long	name;	option to modify
 *	long	len;	length of option value
 * };
 *
 * #define OPTLEN(x) ((((x) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))
 * #define OPTVAL(opt) ((char *)(opt + 1))
 *
 * For get requests (T_NEGOTIATE), any MIB2_xxx value can be used (only
 * "get all" is supported, so all modules get a copy of the request to
 * return everything it knows.   Recommend: Use MIB2_IP
 *
 * IMPORTANT:  some fields are grouped in a different structure than
 * suggested by MIB-II, e.g., checksum error counts.  The original MIB-2
 * field name has been retained.  Field names beginning with "mi" are not
 * defined in the MIB but contain important & useful information maintained
 * by the corresponding module.
 */
#ifndef IPPROTO_MAX
#define	IPPROTO_MAX	256
#endif


#define	MIB2_SYSTEM		(IPPROTO_MAX+1)
#define	MIB2_INTERFACES		(IPPROTO_MAX+2)
#define	MIB2_AT			(IPPROTO_MAX+3)
#define	MIB2_IP			(IPPROTO_MAX+4)
#define	MIB2_ICMP		(IPPROTO_MAX+5)
#define	MIB2_TCP		(IPPROTO_MAX+6)
#define	MIB2_UDP		(IPPROTO_MAX+7)
#define	MIB2_EGP		(IPPROTO_MAX+8)
#define	MIB2_CMOT		(IPPROTO_MAX+9)
#define	MIB2_TRANSMISSION	(IPPROTO_MAX+10)
#define	MIB2_SNMP		(IPPROTO_MAX+11)
#define	MIB2_IP6		(IPPROTO_MAX+12)
#define	MIB2_ICMP6		(IPPROTO_MAX+13)
#define	MIB2_TCP6		(IPPROTO_MAX+14)
#define	MIB2_UDP6		(IPPROTO_MAX+15)
/*
 * Define range of levels for use with MIB2_*
 */
#define	MIB2_RANGE_START	(IPPROTO_MAX+1)
#define	MIB2_RANGE_END		(IPPROTO_MAX+15)


#define	EXPER			1024	/* experimental - not part of mib */
#define	EXPER_IGMP		(EXPER+1)
#define	EXPER_DVMRP		(EXPER+2)
#define	EXPER_RAWIP		(EXPER+3)

/*
 * Define range of levels for experimental use
 */
#define	EXPER_RANGE_START	(EXPER+1)
#define	EXPER_RANGE_END		(EXPER+3)


#define	BUMP_MIB(x)		(x)++
#define	SET_MIB(x, y)		x = y
#define	UPDATE_MIB(x, y)	x += y
#define	BUMP_LOCAL(x)		(x)++

#define	OCTET_LENGTH	32	/* Must be at least LIFNAMSIZ */
typedef struct Octet_s {
	int	o_length;
	char	o_bytes[OCTET_LENGTH];
} Octet_t;

typedef uint32_t	Counter;
typedef uint32_t	Counter32;
typedef uint32_t	Gauge;
typedef uint32_t	IpAddress;
typedef	struct in6_addr	Ip6Address;
typedef Octet_t		DeviceName;
typedef Octet_t		PhysAddress;
typedef uint32_t	DeviceIndex;	/* Interface index */

/*
 *  IP group
 */
#define	MIB2_IP_ADDR	20	/* ipAddrEntry */
#define	MIB2_IP_ROUTE	21	/* ipRouteEntry */
#define	MIB2_IP_MEDIA	22	/* ipNetToMediaEntry */
#define	MIB2_IP6_ROUTE	23	/* ipv6RouteEntry */
#define	MIB2_IP6_MEDIA	24	/* ipv6NetToMediaEntry */
#define	MIB2_IP6_ADDR	25	/* ipv6AddrEntry */
#define	EXPER_IP_GROUP_MEMBERSHIP	100
#define	EXPER_IP6_GROUP_MEMBERSHIP	101

/* Old names retained for compatibility */
#define	MIB2_IP_20	MIB2_IP_ADDR
#define	MIB2_IP_21	MIB2_IP_ROUTE
#define	MIB2_IP_22	MIB2_IP_MEDIA

typedef struct mib2_ip {
		/* forwarder?  1 gateway, 2 NOT gateway	{ip 1} RW */
	int	ipForwarding;
		/* default Time-to-Live for iph		{ip 2} RW */
	int	ipDefaultTTL;
		/* # of input datagrams			{ip 3} */
	Counter	ipInReceives;
		/* # of dg discards for iph error	{ip 4} */
	Counter	ipInHdrErrors;
		/* # of dg discards for bad addr	{ip 5} */
	Counter	ipInAddrErrors;
		/* # of dg being forwarded		{ip 6} */
	Counter	ipForwDatagrams;
		/* # of dg discards for unk protocol	{ip 7} */
	Counter	ipInUnknownProtos;
		/* # of dg discards of good dg's	{ip 8} */
	Counter	ipInDiscards;
		/* # of dg sent upstream		{ip 9} */
	Counter ipInDelivers;
		/* # of outdgs recv'd from upstream	{ip 10} */
	Counter	ipOutRequests;
		/* # of good outdgs discarded		{ip 11} */
	Counter ipOutDiscards;
		/* # of outdg discards: no route found	{ip 12} */
	Counter	ipOutNoRoutes;
		/* sec's recv'd frags held for reass.	{ip 13}	*/
	int	ipReasmTimeout;
		/* # of ip frags needing reassembly	{ip 14} */
	Counter	ipReasmReqds;
		/* # of dg's reassembled		{ip 15} */
	Counter	ipReasmOKs;
		/* # of reassembly failures (not dg cnt){ip 16} */
	Counter	ipReasmFails;
		/* # of dg's fragged			{ip 17} */
	Counter	ipFragOKs;
		/* # of dg discards for no frag set	{ip 18} */
	Counter ipFragFails;
		/* # of dg frags from fragmentation	{ip 19} */
	Counter	ipFragCreates;
		/* {ip 20} */
	int	ipAddrEntrySize;
		/* {ip 21} */
	int	ipRouteEntrySize;
		/* {ip 22} */
	int	ipNetToMediaEntrySize;
		/* # of valid route entries discarded 	{ip 23} */
	Counter	ipRoutingDiscards;
/*
 * following defined in MIB-II as part of TCP & UDP groups:
 */
		/* total # of segments recv'd with error	{ tcp 14 } */
	Counter	tcpInErrs;
		/* # of recv'd dg's not deliverable (no appl.)	{ udp 2 } */
	Counter	udpNoPorts;
/*
 * In addition to MIB-II
 */
		/* # of bad IP header checksums */
	Counter	ipInCksumErrs;
		/* # of complete duplicates in reassembly */
	Counter	ipReasmDuplicates;
		/* # of partial duplicates in reassembly */
	Counter	ipReasmPartDups;
		/* # of packets not forwarded due to adminstrative reasons */
	Counter	ipForwProhibits;
		/* # of UDP packets with bad UDP checksums */
	Counter udpInCksumErrs;
		/* # of UDP packets droped due to queue overflow */
	Counter udpInOverflows;
		/*
		 * # of RAW IP packets (all IP protocols except UDP, TCP
		 * and ICMP) droped due to queue overflow
		 */
	Counter rawipInOverflows;

	/*
	 * Folowing are private IPSEC MIB.
	 */
	/* # of incoming packets that succeeded policy checks */
	Counter ipsecInSucceeded;
	/* # of incoming packets that failed policy checks */
	Counter ipsecInFailed;
/* Compatible extensions added here */
	int	ipMemberEntrySize;	/* Size of ip_member_t */

		/* # of IPv6 packets received by IPv4 and dropped */
	Counter ipInIPv6;
		/* # of IPv6 packets transmitted by ip_wput */
	Counter ipOutIPv6;
		/* # of times ip_wput has switched to become ip_wput_v6 */
	Counter ipOutSwitchIPv6;
} mib2_ip_t;

/*
 *	ipv6IfStatsEntry OBJECT-TYPE
 *		SYNTAX     Ipv6IfStatsEntry
 *		MAX-ACCESS not-accessible
 *		STATUS     current
 *		DESCRIPTION
 *			"An interface statistics entry containing objects
 *			at a particular IPv6 interface."
 *		AUGMENTS { ipv6IfEntry }
 *		::= { ipv6IfStatsTable 1 }
 *
 * Per-interface IPv6 statistics table
 */

typedef struct mib2_ipv6IfStatsEntry {
	/* Local ifindex to identify the interface */
	DeviceIndex	ipv6IfIndex;

		/* forwarder?  1 gateway, 2 NOT gateway	{ipv6MIBObjects 1} RW */
	int	ipv6Forwarding;
		/* default Hoplimit for IPv6		{ipv6MIBObjects 2} RW */
	int	ipv6DefaultHopLimit;

	int	ipv6IfStatsEntrySize;
	int	ipv6AddrEntrySize;
	int	ipv6RouteEntrySize;
	int	ipv6NetToMediaEntrySize;
	int	ipv6MemberEntrySize;		/* Size of ipv6_member_t */

	/* # input datagrams (incl errors)	{ ipv6IfStatsEntry 1 } */
	Counter	ipv6InReceives;
	/* # errors in IPv6 headers and options	{ ipv6IfStatsEntry 2 } */
	Counter	ipv6InHdrErrors;
	/* # exceeds outgoing link MTU		{ ipv6IfStatsEntry 3 } */
	Counter	ipv6InTooBigErrors;
	/* # discarded due to no route to dest 	{ ipv6IfStatsEntry 4 } */
	Counter	ipv6InNoRoutes;
	/* # invalid or unsupported addresses	{ ipv6IfStatsEntry 5 } */
	Counter	ipv6InAddrErrors;
	/* # unknown next header 		{ ipv6IfStatsEntry 6 } */
	Counter	ipv6InUnknownProtos;
	/* # too short packets			{ ipv6IfStatsEntry 7 } */
	Counter	ipv6InTruncatedPkts;
	/* # discarded e.g. due to no buffers	{ ipv6IfStatsEntry 8 } */
	Counter	ipv6InDiscards;
	/* # delivered to upper layer protocols	{ ipv6IfStatsEntry 9 } */
	Counter	ipv6InDelivers;
	/* # forwarded out interface		{ ipv6IfStatsEntry 10 } */
	Counter	ipv6OutForwDatagrams;
	/* # originated out interface		{ ipv6IfStatsEntry 11 } */
	Counter	ipv6OutRequests;
	/* # discarded e.g. due to no buffers	{ ipv6IfStatsEntry 12 } */
	Counter	ipv6OutDiscards;
	/* # sucessfully fragmented packets	{ ipv6IfStatsEntry 13 } */
	Counter	ipv6OutFragOKs;
	/* # fragmentation failed		{ ipv6IfStatsEntry 14 } */
	Counter	ipv6OutFragFails;
	/* # fragments created			{ ipv6IfStatsEntry 15 } */
	Counter	ipv6OutFragCreates;
	/* # fragments to reassemble		{ ipv6IfStatsEntry 16 } */
	Counter	ipv6ReasmReqds;
	/* # packets after reassembly		{ ipv6IfStatsEntry 17 } */
	Counter	ipv6ReasmOKs;
	/* # reassembly failed			{ ipv6IfStatsEntry 18 } */
	Counter	ipv6ReasmFails;
	/* # received multicast packets		{ ipv6IfStatsEntry 19 } */
	Counter	ipv6InMcastPkts;
	/* # transmitted multicast packets	{ ipv6IfStatsEntry 20 } */
	Counter	ipv6OutMcastPkts;
/*
 * In addition to defined MIBs
 */
		/* # discarded due to no route to dest */
	Counter	ipv6OutNoRoutes;
		/* # of complete duplicates in reassembly */
	Counter	ipv6ReasmDuplicates;
		/* # of partial duplicates in reassembly */
	Counter	ipv6ReasmPartDups;
		/* # of packets not forwarded due to adminstrative reasons */
	Counter	ipv6ForwProhibits;
		/* # of UDP packets with bad UDP checksums */
	Counter udpInCksumErrs;
		/* # of UDP packets droped due to queue overflow */
	Counter udpInOverflows;
		/*
		 * # of RAW IPv6 packets (all IPv6 protocols except UDP, TCP
		 * and ICMPv6) droped due to queue overflow
		 */
	Counter rawipInOverflows;

		/* # of IPv4 packets received by IPv6 and dropped */
	Counter ipv6InIPv4;
		/* # of IPv4 packets transmitted by ip_wput_wput */
	Counter ipv6OutIPv4;
		/* # of times ip_wput_v6 has switched to become ip_wput */
	Counter ipv6OutSwitchIPv4;
} mib2_ipv6IfStatsEntry_t;

/*
 * The IP address table contains this entity's IP addressing information.
 *
 *	ipAddrTable OBJECT-TYPE
 *		SYNTAX  SEQUENCE OF IpAddrEntry
 *		ACCESS  not-accessible
 *		STATUS  mandatory
 *		DESCRIPTION
 *			"The table of addressing information relevant to
 *			this entity's IP addresses."
 *		::= { ip 20 }
 */

typedef struct mib2_ipAddrEntry {
			/* IP address of this entry	{ipAddrEntry 1} */
	IpAddress	ipAdEntAddr;
			/* Unique interface index	{ipAddrEntry 2} */
	DeviceName	ipAdEntIfIndex;
			/* Subnet mask for this IP addr	{ipAddrEntry 3} */
	IpAddress	ipAdEntNetMask;
			/* 2^lsb of IP broadcast addr	{ipAddrEntry 4} */
	int		ipAdEntBcastAddr;
			/* max size for dg reassembly	{ipAddrEntry 5} */
	int		ipAdEntReasmMaxSize;
			/* additional ipif_t fields */
	struct ipAdEntInfo_s {
		Gauge		ae_mtu;
				/* BSD if metric */
		int		ae_metric;
				/* ipif broadcast addr.  relation to above?? */
		IpAddress	ae_broadcast_addr;
				/* point-point dest addr */
		IpAddress	ae_pp_dst_addr;
		int		ae_flags;	/* IFF_* flags in if.h */
		Counter		ae_ibcnt;	/* Inbound packets */
		Counter		ae_obcnt;	/* Outbound packets */
		Counter		ae_focnt;	/* Forwarded packets */
		IpAddress	ae_subnet;	/* Subnet prefix */
		int		ae_subnet_len;	/* Subnet prefix length */
		IpAddress	ae_src_addr;	/* Source address */
	}		ipAdEntInfo;
} mib2_ipAddrEntry_t;

/*
 *	ipv6AddrTable OBJECT-TYPE
 *		SYNTAX      SEQUENCE OF Ipv6AddrEntry
 *		MAX-ACCESS  not-accessible
 *		STATUS      current
 *		DESCRIPTION
 *			"The table of addressing information relevant to
 *			this node's interface addresses."
 *		::= { ipv6MIBObjects 8 }
 */

typedef struct mib2_ipv6AddrEntry {
	/* Unique interface index			{ Part of INDEX } */
	DeviceName	ipv6AddrIfIndex;

	/* IPv6 address of this entry			{ ipv6AddrEntry 1 } */
	Ip6Address	ipv6AddrAddress;
	/* Prefix length				{ ipv6AddrEntry 2 } */
	uint_t		ipv6AddrPfxLength;
	/* Type: stateless(1), stateful(2), unknown(3)	{ ipv6AddrEntry 3 } */
	uint_t		ipv6AddrType;
	/* Anycast: true(1), false(2)			{ ipv6AddrEntry 4 } */
	uint_t		ipv6AddrAnycastFlag;
	/*
	 * Address status: preferred(1), deprecated(2), invalid(3),
	 * inaccessible(4), unknown(5)			{ ipv6AddrEntry 5 }
	 */
	uint_t		ipv6AddrStatus;
	struct ipv6AddrInfo_s {
		Gauge		ae_mtu;
				/* BSD if metric */
		int		ae_metric;
				/* point-point dest addr */
		Ip6Address	ae_pp_dst_addr;
		int		ae_flags;	/* IFF_* flags in if.h */
		Counter		ae_ibcnt;	/* Inbound packets */
		Counter		ae_obcnt;	/* Outbound packets */
		Counter		ae_focnt;	/* Forwarded packets */
		Ip6Address	ae_subnet;	/* Subnet prefix */
		int		ae_subnet_len;	/* Subnet prefix length */
		Ip6Address	ae_src_addr;	/* Source address */
	}		ipv6AddrInfo;
} mib2_ipv6AddrEntry_t;


/*
 * The IP routing table contains an entry for each route presently known to
 * this entity. (for IPv4 routes)
 *
 *	ipRouteTable OBJECT-TYPE
 *		SYNTAX  SEQUENCE OF IpRouteEntry
 *		ACCESS  not-accessible
 *		STATUS  mandatory
 *		DESCRIPTION
 *			"This entity's IP Routing table."
 *		::= { ip 21 }
 */

typedef struct mib2_ipRouteEntry {
		/* dest ip addr for this route		{ipRouteEntry 1 } RW */
	IpAddress	ipRouteDest;
		/* unique interface index for this hop	{ipRouteEntry 2 } RW */
	DeviceName	ipRouteIfIndex;
		/* primary route metric 		{ipRouteEntry 3 } RW */
	int		ipRouteMetric1;
		/* alternate route metric 		{ipRouteEntry 4 } RW */
	int		ipRouteMetric2;
		/* alternate route metric 		{ipRouteEntry 5 } RW */
	int		ipRouteMetric3;
		/* alternate route metric 		{ipRouteEntry 6 } RW */
	int		ipRouteMetric4;
		/* ip addr of next hop on this route	{ipRouteEntry 7 } RW */
	IpAddress	ipRouteNextHop;
		/* other(1), inval(2), dir(3), indir(4)	{ipRouteEntry 8 } RW */
	int		ipRouteType;
		/* mechanism by which route was learned	{ipRouteEntry 9 } */
	int		ipRouteProto;
		/* sec's since last update of route	{ipRouteEntry 10} RW */
	int		ipRouteAge;
		/* 					{ipRouteEntry 11} RW */
	IpAddress	ipRouteMask;
		/* alternate route metric 		{ipRouteEntry 12} RW */
	int		ipRouteMetric5;
		/* additional info from ire's		{ipRouteEntry 13 } */
	struct ipRouteInfo_s {
		Gauge		re_max_frag;
		Gauge		re_rtt;
		Counter		re_ref;
		int		re_frag_flag;
		IpAddress	re_src_addr;
		int		re_ire_type;
		Counter		re_obpkt;
		Counter		re_ibpkt;
		int		re_flags;
	} 		ipRouteInfo;
} mib2_ipRouteEntry_t;

/*
 * The IPv6 routing table contains an entry for each route presently known to
 * this entity.
 *
 *	ipv6RouteTable OBJECT-TYPE
 *		SYNTAX  SEQUENCE OF IpRouteEntry
 *		ACCESS  not-accessible
 *		STATUS  current
 *		DESCRIPTION
 *			"IPv6 Routing table. This table contains
 *			an entry for each valid IPv6 unicast route
 *			that can be used for packet forwarding
 *			determination."
 *		::= { ipv6MIBObjects 11 }
 */

typedef struct mib2_ipv6RouteEntry {
		/* dest ip addr for this route		{ ipv6RouteEntry 1 } */
	Ip6Address	ipv6RouteDest;
		/* prefix length 			{ ipv6RouteEntry 2 } */
	int		ipv6RoutePfxLength;
		/* unique route index 			{ ipv6RouteEntry 3 } */
	unsigned	ipv6RouteIndex;
		/* unique interface index for this hop	{ ipv6RouteEntry 4 } */
	DeviceName	ipv6RouteIfIndex;
		/* IPv6 addr of next hop on this route	{ ipv6RouteEntry 5 } */
	Ip6Address	ipv6RouteNextHop;
		/* other(1), discard(2), local(3), remote(4) */
		/* 					{ ipv6RouteEntry 6 } */
	int		ipv6RouteType;
		/* mechanism by which route was learned	{ ipv6RouteEntry 7 } */
		/*
		 * other(1), local(2), netmgmt(3), ndisc(4), rip(5), ospf(6),
		 * bgp(7), idrp(8), igrp(9)
		 */
	int		ipv6RouteProtocol;
		/* policy hook or traffic class		{ ipv6RouteEntry 8 } */
	unsigned	ipv6RoutePolicy;
		/* sec's since last update of route	{ ipv6RouteEntry 9} */
	int		ipv6RouteAge;
		/* Routing domain ID of the next hop	{ ipv6RouteEntry 10 } */
	unsigned	ipv6RouteNextHopRDI;
		/* route metric				{ ipv6RouteEntry 11 } */
	unsigned	ipv6RouteMetric;
		/* preference (impl specific)		{ ipv6RouteEntry 12 } */
	unsigned	ipv6RouteWeight;
		/* additional info from ire's		{ } */
	struct ipv6RouteInfo_s {
		Gauge		re_max_frag;
		Gauge		re_rtt;
		Counter		re_ref;
		int		re_frag_flag;
		Ip6Address	re_src_addr;
		int		re_ire_type;
		Counter		re_obpkt;
		Counter		re_ibpkt;
		int		re_flags;
	} 		ipv6RouteInfo;
} mib2_ipv6RouteEntry_t;


/*
 * The IP address translation table contain the IpAddress to
 * `physical' address equivalences.  Some interfaces do not
 * use translation tables for determining address
 * equivalences (e.g., DDN-X.25 has an algorithmic method);
 * if all interfaces are of this type, then the Address
 * Translation table is empty, i.e., has zero entries.
 *
 *	ipNetToMediaTable OBJECT-TYPE
 *		SYNTAX  SEQUENCE OF IpNetToMediaEntry
 *		ACCESS  not-accessible
 *		STATUS  mandatory
 *		DESCRIPTION
 *			"The IP Address Translation table used for mapping
 *			from IP addresses to physical addresses."
 *		::= { ip 22 }
 */

typedef struct mib2_ipNetToMediaEntry {
	/* Unique interface index		{ ipNetToMediaEntry 1 } RW */
	DeviceName	ipNetToMediaIfIndex;
	/* Media dependent physical addr	{ ipNetToMediaEntry 2 } RW */
	PhysAddress	ipNetToMediaPhysAddress;
	/* ip addr for this physical addr	{ ipNetToMediaEntry 3 } RW */
	IpAddress	ipNetToMediaNetAddress;
	/* other(1), inval(2), dyn(3), stat(4)	{ ipNetToMediaEntry 4 } RW */
	int		ipNetToMediaType;
	struct ipNetToMediaInfo_s {
		PhysAddress	ntm_mask;	/* subnet mask for entry */
		int		ntm_flags;	/* ACE_F_* flags in arp.h */
	}		ipNetToMediaInfo;
} mib2_ipNetToMediaEntry_t;

/*
 *	ipv6NetToMediaTable OBJECT-TYPE
 *		 SYNTAX      SEQUENCE OF Ipv6NetToMediaEntry
 *		 MAX-ACCESS  not-accessible
 *		 STATUS      current
 *		 DESCRIPTION
 *			"The IPv6 Address Translation table used for
 *			mapping from IPv6 addresses to physical addresses.
 *
 *			The IPv6 address translation table contain the
 *			Ipv6Address to `physical' address equivalencies.
 *			Some interfaces do not use translation tables
 *			for determining address equivalencies; if all
 *			interfaces are of this type, then the Address
 *			Translation table is empty, i.e., has zero
 *			entries."
 *		::= { ipv6MIBObjects 12 }
 */

typedef struct mib2_ipv6NetToMediaEntry {
	/* Unique interface index		{ Part of INDEX } */
	DeviceIndex	ipv6NetToMediaIfIndex;

	/* ip addr for this physical addr	{ ipv6NetToMediaEntry 1 } */
	Ip6Address	ipv6NetToMediaNetAddress;
	/* Media dependent physical addr	{ ipv6NetToMediaEntry 2 } */
	PhysAddress	ipv6NetToMediaPhysAddress;
	/*
	 * Type of mapping
	 * other(1), dynamic(2), static(3), local(4)
	 *					{ ipv6NetToMediaEntry 3 }
	 */
	int		ipv6NetToMediaType;
	/*
	 * NUD state
	 * reachable(1), stale(2), delay(3), probe(4), invalid(5), unknown(6)
	 * Note: The kernel returns ND_* states.
	 *					{ ipv6NetToMediaEntry 4 }
	 */
	int		ipv6NetToMediaState;
	/* sysUpTime last time entry was updated { ipv6NetToMediaEntry 5 } */
	int		ipv6NetToMediaLastUpdated;
} mib2_ipv6NetToMediaEntry_t;


/*
 * List of group members per interface
 */
typedef struct ip_member {
	/* Interface index */
	DeviceName	ipGroupMemberIfIndex;
	/* IP Multicast address */
	IpAddress	ipGroupMemberAddress;
	/* Number of member sockets */
	Counter		ipGroupMemberRefCnt;
} ip_member_t;


/*
 * List of IPv6 group members per interface
 */
typedef struct ipv6_member {
	/* Interface index */
	DeviceIndex	ipv6GroupMemberIfIndex;
	/* IP Multicast address */
	Ip6Address	ipv6GroupMemberAddress;
	/* Number of member sockets */
	Counter		ipv6GroupMemberRefCnt;
} ipv6_member_t;


/*
 * ICMP Group
 */
typedef struct mib2_icmp {
	/* total # of recv'd ICMP msgs			{ icmp 1 } */
	Counter	icmpInMsgs;
	/* recv'd ICMP msgs with errors			{ icmp 2 } */
	Counter	icmpInErrors;
	/* recv'd "dest unreachable" msg's		{ icmp 3 } */
	Counter	icmpInDestUnreachs;
	/* recv'd "time exceeded" msg's			{ icmp 4 } */
	Counter	icmpInTimeExcds;
	/* recv'd "parameter problem" msg's		{ icmp 5 } */
	Counter	icmpInParmProbs;
	/* recv'd "source quench" msg's			{ icmp 6 } */
	Counter	icmpInSrcQuenchs;
	/* recv'd "ICMP redirect" msg's			{ icmp 7 } */
	Counter	icmpInRedirects;
	/* recv'd "echo request" msg's			{ icmp 8 } */
	Counter	icmpInEchos;
	/* recv'd "echo reply" msg's			{ icmp 9 } */
	Counter	icmpInEchoReps;
	/* recv'd "timestamp" msg's			{ icmp 10 } */
	Counter	icmpInTimestamps;
	/* recv'd "timestamp reply" msg's		{ icmp 11 } */
	Counter	icmpInTimestampReps;
	/* recv'd "address mask request" msg's		{ icmp 12 } */
	Counter	icmpInAddrMasks;
	/* recv'd "address mask reply" msg's		{ icmp 13 } */
	Counter	icmpInAddrMaskReps;
	/* total # of sent ICMP msg's			{ icmp 14 } */
	Counter	icmpOutMsgs;
	/* # of msg's not sent for internal icmp errors	{ icmp 15 } */
	Counter	icmpOutErrors;
	/* # of "dest unreachable" msg's sent		{ icmp 16 } */
	Counter	icmpOutDestUnreachs;
	/* # of "time exceeded" msg's sent		{ icmp 17 } */
	Counter	icmpOutTimeExcds;
	/* # of "parameter problme" msg's sent		{ icmp 18 } */
	Counter	icmpOutParmProbs;
	/* # of "source quench" msg's sent		{ icmp 19 } */
	Counter	icmpOutSrcQuenchs;
	/* # of "ICMP redirect" msg's sent		{ icmp 20 } */
	Counter	icmpOutRedirects;
	/* # of "Echo request" msg's sent		{ icmp 21 } */
	Counter	icmpOutEchos;
	/* # of "Echo reply" msg's sent			{ icmp 22 } */
	Counter	icmpOutEchoReps;
	/* # of "timestamp request" msg's sent		{ icmp 23 } */
	Counter	icmpOutTimestamps;
	/* # of "timestamp reply" msg's sent		{ icmp 24 } */
	Counter	icmpOutTimestampReps;
	/* # of "address mask request" msg's sent	{ icmp 25 } */
	Counter	icmpOutAddrMasks;
	/* # of "address mask reply" msg's sent		{ icmp 26 } */
	Counter	icmpOutAddrMaskReps;
/*
 * In addition to MIB-II
 */
	/* # of received packets with checksum errors */
	Counter	icmpInCksumErrs;
	/* # of received packets with unknow codes */
	Counter	icmpInUnknowns;
	/* # of received unreachables with "fragmentation needed" */
	Counter	icmpInFragNeeded;
	/* # of sent unreachables with "fragmentation needed" */
	Counter	icmpOutFragNeeded;
	/*
	 * # of msg's not sent since original packet was broadcast/multicast
	 * or an ICMP error packet
	 */
	Counter	icmpOutDrops;
	/* # of ICMP packets droped due to queue overflow */
	Counter icmpInOverflows;
	/* recv'd "ICMP redirect" msg's	that are bad thus ignored */
	Counter	icmpInBadRedirects;
} mib2_icmp_t;


/*
 *	ipv6IfIcmpEntry OBJECT-TYPE
 *		SYNTAX      Ipv6IfIcmpEntry
 *		MAX-ACCESS  not-accessible
 *		STATUS      current
 *		DESCRIPTION
 *			"An ICMPv6 statistics entry containing
 *			objects at a particular IPv6 interface.
 *
 *			Note that a receiving interface is
 *			the interface to which a given ICMPv6 message
 *			is addressed which may not be necessarily
 *			the input interface for the message.
 *
 *			Similarly, the sending interface is
 *			the interface that sources a given
 *			ICMP message which is usually but not
 *			necessarily the output interface for the message."
 *		AUGMENTS { ipv6IfEntry }
 *		::= { ipv6IfIcmpTable 1 }
 *
 * Per-interface ICMPv6 statistics table
 */

typedef struct mib2_ipv6IfIcmpEntry {
	/* Local ifindex to identify the interface */
	DeviceIndex	ipv6IfIcmpIfIndex;

	int		ipv6IfIcmpEntrySize;	/* Size of ipv6IfIcmpEntry */

	/* The total # ICMP msgs rcvd includes ipv6IfIcmpInErrors */
	Counter32	ipv6IfIcmpInMsgs;
	/* # ICMP with ICMP-specific errors (bad checkum, length, etc) */
	Counter32	ipv6IfIcmpInErrors;
	/* # ICMP Destination Unreachable */
	Counter32	ipv6IfIcmpInDestUnreachs;
	/* # ICMP destination unreachable/communication admin prohibited */
	Counter32	ipv6IfIcmpInAdminProhibs;
	Counter32	ipv6IfIcmpInTimeExcds;
	Counter32	ipv6IfIcmpInParmProblems;
	Counter32	ipv6IfIcmpInPktTooBigs;
	Counter32	ipv6IfIcmpInEchos;
	Counter32	ipv6IfIcmpInEchoReplies;
	Counter32	ipv6IfIcmpInRouterSolicits;
	Counter32	ipv6IfIcmpInRouterAdvertisements;
	Counter32	ipv6IfIcmpInNeighborSolicits;
	Counter32	ipv6IfIcmpInNeighborAdvertisements;
	Counter32	ipv6IfIcmpInRedirects;
	Counter32	ipv6IfIcmpInGroupMembQueries;
	Counter32	ipv6IfIcmpInGroupMembResponses;
	Counter32	ipv6IfIcmpInGroupMembReductions;
	/* Total # ICMP messages attempted to send (includes OutErrors) */
	Counter32	ipv6IfIcmpOutMsgs;
	/* # ICMP messages not sent due to ICMP problems (e.g. no buffers) */
	Counter32	ipv6IfIcmpOutErrors;
	Counter32	ipv6IfIcmpOutDestUnreachs;
	Counter32	ipv6IfIcmpOutAdminProhibs;
	Counter32	ipv6IfIcmpOutTimeExcds;
	Counter32	ipv6IfIcmpOutParmProblems;
	Counter32	ipv6IfIcmpOutPktTooBigs;
	Counter32	ipv6IfIcmpOutEchos;
	Counter32	ipv6IfIcmpOutEchoReplies;
	Counter32	ipv6IfIcmpOutRouterSolicits;
	Counter32	ipv6IfIcmpOutRouterAdvertisements;
	Counter32	ipv6IfIcmpOutNeighborSolicits;
	Counter32	ipv6IfIcmpOutNeighborAdvertisements;
	Counter32	ipv6IfIcmpOutRedirects;
	Counter32	ipv6IfIcmpOutGroupMembQueries;
	Counter32	ipv6IfIcmpOutGroupMembResponses;
	Counter32	ipv6IfIcmpOutGroupMembReductions;
/* Additions beyond the MIB */
	Counter32	ipv6IfIcmpInOverflows;
	/* recv'd "ICMPv6 redirect" msg's that are bad thus ignored */
	Counter32	ipv6IfIcmpBadHoplimit;
	Counter32	ipv6IfIcmpInBadNeighborAdvertisements;
	Counter32	ipv6IfIcmpInBadNeighborSolicitations;
	Counter32	ipv6IfIcmpInBadRedirects;
	Counter32	ipv6IfIcmpInGroupMembTotal;
	Counter32	ipv6IfIcmpInGroupMembBadQueries;
	Counter32	ipv6IfIcmpInGroupMembBadReports;
	Counter32	ipv6IfIcmpInGroupMembOurReports;
} mib2_ipv6IfIcmpEntry_t;

/*
 * the TCP group
 *
 * Note that instances of object types that represent
 * information about a particular TCP connection are
 * transient; they persist only as long as the connection
 * in question.
 */
#define	MIB2_TCP_CONN	13	/* tcpConnEntry */
#define	MIB2_TCP6_CONN	14	/* tcp6ConnEntry */

/* Old name retained for compatibility */
#define	MIB2_TCP_13	MIB2_TCP_CONN

typedef struct mib2_tcp {
		/* algorithm used for transmit timeout value	{ tcp 1 } */
	int	tcpRtoAlgorithm;
		/* minimum retransmit timeout (ms)		{ tcp 2 } */
	int	tcpRtoMin;
		/* maximum retransmit timeout (ms)		{ tcp 3 } */
	int	tcpRtoMax;
		/* maximum # of connections supported		{ tcp 4 } */
	int	tcpMaxConn;
		/* # of direct transitions CLOSED -> SYN-SENT	{ tcp 5 } */
	Counter	tcpActiveOpens;
		/* # of direct transitions LISTEN -> SYN-RCVD	{ tcp 6 } */
	Counter	tcpPassiveOpens;
		/* # of direct SIN-SENT/RCVD -> CLOSED/LISTEN	{ tcp 7 } */
	Counter	tcpAttemptFails;
		/* # of direct ESTABLISHED/CLOSE-WAIT -> CLOSED	{ tcp 8 } */
	Counter	tcpEstabResets;
		/* # of connections ESTABLISHED or CLOSE-WAIT	{ tcp 9 } */
	Gauge	tcpCurrEstab;
		/* total # of segments recv'd			{ tcp 10 } */
	Counter	tcpInSegs;
		/* total # of segments sent			{ tcp 11 } */
	Counter	tcpOutSegs;
		/* total # of segments retransmitted		{ tcp 12 } */
	Counter	tcpRetransSegs;
		/* {tcp 13} */
	int	tcpConnTableSize;	/* Size of tcpConnEntry_t */
	/* in ip				{tcp 14} */
		/* # of segments sent with RST flag		{ tcp 15 } */
	Counter	tcpOutRsts;
/* In addition to MIB-II */
/* Sender */
	/* total # of data segments sent */
	Counter tcpOutDataSegs;
	/* total # of bytes in data segments sent */
	Counter tcpOutDataBytes;
	/* total # of bytes in segments retransmitted */
	Counter tcpRetransBytes;
	/* total # of acks sent */
	Counter tcpOutAck;
	/* total # of delayed acks sent */
	Counter tcpOutAckDelayed;
	/* total # of segments sent with the urg flag on */
	Counter tcpOutUrg;
	/* total # of window updates sent */
	Counter tcpOutWinUpdate;
	/* total # of zero window probes sent */
	Counter tcpOutWinProbe;
	/* total # of control segments sent (syn, fin, rst) */
	Counter tcpOutControl;
	/* total # of segments sent due to "fast retransmit" */
	Counter tcpOutFastRetrans;
/* Receiver */
	/* total # of ack segments received */
	Counter tcpInAckSegs;
	/* total # of bytes acked */
	Counter tcpInAckBytes;
	/* total # of duplicate acks */
	Counter tcpInDupAck;
	/* total # of acks acking unsent data */
	Counter tcpInAckUnsent;
	/* total # of data segments received in order */
	Counter tcpInDataInorderSegs;
	/* total # of data bytes received in order */
	Counter tcpInDataInorderBytes;
	/* total # of data segments received out of order */
	Counter tcpInDataUnorderSegs;
	/* total # of data bytes received out of order */
	Counter tcpInDataUnorderBytes;
	/* total # of complete duplicate data segments received */
	Counter tcpInDataDupSegs;
	/* total # of bytes in the complete duplicate data segments received */
	Counter tcpInDataDupBytes;
	/* total # of partial duplicate data segments received */
	Counter tcpInDataPartDupSegs;
	/* total # of bytes in the partial duplicate data segments received */
	Counter tcpInDataPartDupBytes;
	/* total # of data segments received past the window */
	Counter tcpInDataPastWinSegs;
	/* total # of data bytes received part the window */
	Counter tcpInDataPastWinBytes;
	/* total # of zero window probes received */
	Counter tcpInWinProbe;
	/* total # of window updates received */
	Counter tcpInWinUpdate;
	/* total # of data segments received after the connection has closed */
	Counter tcpInClosed;
/* Others */
	/* total # of failed attempts to update the rtt estimate */
	Counter tcpRttNoUpdate;
	/* total # of successful attempts to update the rtt estimate */
	Counter tcpRttUpdate;
	/* total # of retransmit timeouts */
	Counter tcpTimRetrans;
	/* total # of retransmit timeouts dropping the connection */
	Counter tcpTimRetransDrop;
	/* total # of keepalive timeouts */
	Counter tcpTimKeepalive;
	/* total # of keepalive timeouts sending a probe */
	Counter tcpTimKeepaliveProbe;
	/* total # of keepalive timeouts dropping the connection */
	Counter tcpTimKeepaliveDrop;
	/* total # of connections refused due to backlog full on listen */
	Counter tcpListenDrop;
	/* total # of connections refused due to half-open queue (q0) full */
	Counter tcpListenDropQ0;
	/* total # of connections dropped from a full half-open queue (q0) */
	Counter tcpHalfOpenDrop;
	/* total # of retransmitted segments by SACK retransmission */
	Counter	tcpOutSackRetransSegs;

	int	tcp6ConnTableSize;	/* Size of tcp6ConnEntry_t */
} mib2_tcp_t;

/*
 * The TCP/IPv4 connection table {tcp 13} contains information about this
 * entity's existing TCP connections over IPv4.
 */
/* For tcpConnState and tcp6ConnState */
#define	MIB2_TCP_closed		1
#define	MIB2_TCP_listen		2
#define	MIB2_TCP_synSent	3
#define	MIB2_TCP_synReceived	4
#define	MIB2_TCP_established	5
#define	MIB2_TCP_finWait1	6
#define	MIB2_TCP_finWait2	7
#define	MIB2_TCP_closeWait	8
#define	MIB2_TCP_lastAck	9
#define	MIB2_TCP_closing	10
#define	MIB2_TCP_timeWait	11
#define	MIB2_TCP_deleteTCB	12		/* only writeable value */

typedef struct mib2_tcpConnEntry {
		/* state of tcp connection		{ tcpConnEntry 1} RW */
	int		tcpConnState;
		/* local ip addr for this connection	{ tcpConnEntry 2 } */
	IpAddress	tcpConnLocalAddress;
		/* local port for this connection	{ tcpConnEntry 3 } */
	int		tcpConnLocalPort;	/* In host byte order */
		/* remote ip addr for this connection	{ tcpConnEntry 4 } */
	IpAddress	tcpConnRemAddress;
		/* remote port for this connection	{ tcpConnEntry 5 } */
	int		tcpConnRemPort;		/* In host byte order */
	struct tcpConnEntryInfo_s {
			/* seq # of next segment to send */
		Gauge		ce_snxt;
				/* seq # of of last segment unacknowledged */
		Gauge		ce_suna;
				/* currect send window size */
		Gauge		ce_swnd;
				/* seq # of next expected segment */
		Gauge		ce_rnxt;
				/* seq # of last ack'd segment */
		Gauge		ce_rack;
				/* currenct receive window size */
		Gauge		ce_rwnd;
					/* current rto (retransmit timeout) */
		Gauge		ce_rto;
					/* current max segment size */
		Gauge		ce_mss;
				/* actual internal state */
		int		ce_state;
	} 		tcpConnEntryInfo;
} mib2_tcpConnEntry_t;


/*
 * The TCP/IPv6 connection table {tcp 14} contains information about this
 * entity's existing TCP connections over IPv6.
 */

typedef struct mib2_tcp6ConnEntry {
	/* local ip addr for this connection	{ ipv6TcpConnEntry 1 } */
	Ip6Address	tcp6ConnLocalAddress;
	/* local port for this connection	{ ipv6TcpConnEntry 2 } */
	int		tcp6ConnLocalPort;
	/* remote ip addr for this connection	{ ipv6TcpConnEntry 3 } */
	Ip6Address	tcp6ConnRemAddress;
	/* remote port for this connection	{ ipv6TcpConnEntry 4 } */
	int		tcp6ConnRemPort;
	/* interface index or zero		{ ipv6TcpConnEntry 5 } */
	DeviceIndex	tcp6ConnIfIndex;
	/* state of tcp6 connection		{ ipv6TcpConnEntry 6 } RW */
	int		tcp6ConnState;
	struct tcp6ConnEntryInfo_s {
			/* seq # of next segment to send */
		Gauge		ce_snxt;
				/* seq # of of last segment unacknowledged */
		Gauge		ce_suna;
				/* currect send window size */
		Gauge		ce_swnd;
				/* seq # of next expected segment */
		Gauge		ce_rnxt;
				/* seq # of last ack'd segment */
		Gauge		ce_rack;
				/* currenct receive window size */
		Gauge		ce_rwnd;
					/* current rto (retransmit timeout) */
		Gauge		ce_rto;
					/* current max segment size */
		Gauge		ce_mss;
				/* actual internal state */
		int		ce_state;
	} 		tcp6ConnEntryInfo;
} mib2_tcp6ConnEntry_t;

/*
 * the UDP group
 */
#define	MIB2_UDP_ENTRY	5	/* udpEntry */
#define	MIB2_UDP6_ENTRY	6	/* udp6Entry */

/* Old name retained for compatibility */
#define	MIB2_UDP_5	MIB2_UDP_ENTRY

typedef struct mib2_udp {
		/* total # of UDP datagrams sent upstream	{ udp 1 } */
	Counter	udpInDatagrams;
	/* in ip				{ udp 2 } */
		/* # of recv'd dg's not deliverable (other)	{ udp 3 }  */
	Counter	udpInErrors;
		/* total # of dg's sent				{ udp 4 } */
	Counter	udpOutDatagrams;
		/* { udp 5 } */
	int	udpEntrySize;			/* Size of udpEntry_t */
	int	udp6EntrySize;			/* Size of udp6Entry_t */
	Counter	udpOutErrors;
} mib2_udp_t;

/*
 * The UDP listener table contains information about this entity's UDP
 * end-points on which a local application is currently accepting datagrams.
 */

/* For both IPv4 and IPv6 ue_state: */
#define	MIB2_UDP_unbound	1
#define	MIB2_UDP_idle		2
#define	MIB2_UDP_connected	3
#define	MIB2_UDP_unknown	4

typedef struct mib2_udpEntry {
		/* local ip addr of listener		{ udpEntry 1 } */
	IpAddress	udpLocalAddress;
		/* local port of listener		{ udpEntry 2 } */
	int		udpLocalPort;		/* In host byte order */
	struct udpEntryInfo_s {
		int		ue_state;
		IpAddress	ue_RemoteAddress;
		int		ue_RemotePort;	/* In host byte order */
	}		udpEntryInfo;
} mib2_udpEntry_t;

/*
 * The UDP (for IPv6) listener table contains information about this
 * entity's UDP end-points on which a local application is
 * currently accepting datagrams.
 */

typedef	struct mib2_udp6Entry {
		/* local ip addr of listener		{ ipv6UdpEntry 1 } */
	Ip6Address	udp6LocalAddress;
		/* local port of listener		{ ipv6UdpEntry 2 } */
	int		udp6LocalPort;		/* In host byte order */
		/* interface index or zero 		{ ipv6UdpEntry 3 } */
	DeviceIndex	udp6IfIndex;
	struct udp6EntryInfo_s {
		int	ue_state;
		Ip6Address	ue_RemoteAddress;
		int		ue_RemotePort;	/* In host byte order */
	}		udp6EntryInfo;
} mib2_udp6Entry_t;

/*
 * the RAWIP group
 */
typedef struct mib2_rawip {
		/* total # of RAWIP datagrams sent upstream */
	Counter	rawipInDatagrams;
		/* # of RAWIP packets with bad IPV6_CHECKSUM checksums */
	Counter rawipInCksumErrs;
		/* # of recv'd dg's not deliverable (other) */
	Counter	rawipInErrors;
		/* total # of dg's sent */
	Counter	rawipOutDatagrams;
		/* total # of dg's not sent (e.g. no memory) */
	Counter	rawipOutErrors;
} mib2_rawip_t;

/* DVMRP group */
#define	EXPER_DVMRP_VIF		1
#define	EXPER_DVMRP_MRT		2


#ifdef	__cplusplus
}
#endif

#endif	/* _INET_MIB2_H */
