/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)netex.h	1.3	97/05/09 SMI"
 */

/*
 *	Header file for definitions specific to exercising network devices.
 */

#ifndef _NETEX_H_
#define	_NETEX_H_

#define	ETHERADDRL	6
#define	IPADDRL		4

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)

#define	ETHERTYPE_IP		0x0800		/* IP protocol */
#define	ETHERTYPE_ARP		0x0806		/* Addr. resolution protocol */
#define	ETHERTYPE_REVARP	0x8035		/* Reverse ARP */

#define	IPPROTO_ICMP		1		/* ICMP protocol */
#define	IPPROTO_UDP		17		/* user datagram protocol */

#define	ICMP_MINLEN	8			/* Minimum ICMP size */
#define	ICMP_ECHOREPLY	0			/* Reply to ICMP_ECHO */
#define	ICMP_ECHO	8			/* ICMP echo request */

#define	htons(x)	swap_two_bytes(x)
#define	ntohs(x)	swap_two_bytes(x)
#define	htonl(x)	swap_four_bytes(x)
#define	ntohl(x)	swap_four_bytes(x)

struct ether_addr {
	unchar ether_addr_octet[ETHERADDRL];
};

struct	in_addr {
	unchar in_addr_octet[IPADDRL];
};

struct ether_header {
	struct ether_addr ether_dhost;
	struct ether_addr ether_shost;
	ushort ether_type;
};

struct arphdr {
	ushort ar_hrd;		/* format of hardware address */
#define	ARPHRD_ETHER	1	/* ethernet hardware address */
	ushort ar_pro;		/* format of protocol address */
	unchar ar_hln;		/* length of hardware address */
	unchar ar_pln;		/* length of protocol address */
	ushort ar_op;		/* one of: */
#define	ARPOP_REQUEST	1	/* request to resolve address */
#define	ARPOP_REPLY	2	/* response to previous request */
#define	REVARP_REQUEST	3	/* Reverse ARP request */
#define	REVARP_REPLY	4	/* Reverse ARP reply */
};

struct ether_arp {
	struct arphdr ea_hdr;		/* fixed-size header */
	struct ether_addr arp_sha;	/* sender hardware address */
	unchar arp_spa[IPADDRL];	/* sender protocol address */
	struct ether_addr arp_tha;	/* target hardware address */
	unchar arp_tpa[IPADDRL];	/* target protocol address */
};
#define	arp_hrd ea_hdr.ar_hrd
#define	arp_pro ea_hdr.ar_pro
#define	arp_hln ea_hdr.ar_hln
#define	arp_pln ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op

struct arp_packet {
	struct ether_header	arp_eh;
	struct ether_arp	arp_ea;
#define	used_size (sizeof (struct ether_header) + sizeof (struct ether_arp))
	char	filler[ETHERMIN - sizeof (struct ether_arp)];
};

struct ip {
	unchar	ip_len_ver;		/* version and header length */
	unchar	ip_tos;			/* type of service */
	short	ip_len;			/* total length */
	ushort	ip_id;			/* identification */
	short	ip_off;			/* fragment offset field */
	unchar	ip_ttl;			/* time to live */
	unchar	ip_p;			/* protocol */
	ushort	ip_sum;			/* checksum */
	struct	in_addr ip_src, ip_dst; /* source and dest address */
};

struct udphdr {
	ushort	uh_sport;		/* source port */
	ushort	uh_dport;		/* destination port */
	short	uh_ulen;		/* udp length */
	ushort	uh_sum;			/* udp checksum */
};

/*
 * pseudo header needed for calculating UDP checksums.
 */
struct pseudo_udp {
	struct in_addr	src;
	struct in_addr	dst;
	unchar		notused;	/* always zero */
	unchar		proto;		/* protocol used */
	ushort		len;		/* UDP len */
	struct udphdr	hdr;		/* UDP header */
};

struct rpc_hr {
	ulong	transaction;
	ulong	call_type;
	ulong	rpc_version;
	ulong	program;
	ulong	version;
	ulong	procedure;
	ulong	cred_flavor;
	ulong	cred_len;
	ulong	verifier_flavor;
	ulong	verifier_len;
};

#define	IP_ADDR_TYPE	1

/*
 * The boot code randomizes this value within a specified range.
 * We just use a value observed during a specific test boot.
 */
#define	WHOAMI_PORT	1016

struct bp_address {
	ulong address_type;
	ulong ip_addr[IPADDRL];
};
typedef struct bp_address bp_address;


struct bp_whoami_arg {
	bp_address client_address;
};
typedef struct bp_whoami_arg bp_whoami_arg;

#define	BOOTPARAMPROG 100026L
#define	BOOTPARAMVERS 1
#define	BOOTPARAMPROC_WHOAMI 1

#define	PMAPPROG	100000L
#define	PMAPPORT	111

struct pmap {
	ulong program;
	ulong version;
	ulong proc;
	ulong len;
};

struct whoami_packet {
	unchar fill[2];
	unchar eh[14];
	struct ip iph;
	struct udphdr udph;
	struct rpc_hr rpch;
	struct pmap pmap;
	struct bp_whoami_arg bp;
};

struct icmp_packet {
	unchar fill[2];
	unchar eh[14];
	struct ip iph;
	struct icmp {
		unchar icmp_type;
		unchar icmp_code;
		ushort icmp_cksum;
		unchar icmp_rest[60];
	} icmp;
};

#endif /* _NETEX_H_ */
