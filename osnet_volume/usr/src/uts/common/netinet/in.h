/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

/*
 * Constants and structures defined by the internet system,
 * according to following documents
 *
 * Internet ASSIGNED NUMBERS (RFC1700) and its successors
 *	and other assignments at ftp://ftp.isi.edu/in-notes/iana/assignments
 * Basic Socket Interface Extensions for IPv6 (RFC2133 and its successors)
 *
 */

#ifndef _NETINET_IN_H
#define	_NETINET_IN_H

#pragma ident	"@(#)in.h	1.26	99/10/25 SMI"

#include <sys/feature_tests.h>

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/stream.h>
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
/*
 * Symbols such as htonl() are required to be exposed through this file,
 * per XNS Issue 5. This is achieved by inclusion of <sys/byteorder.h>
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__) || defined(_XPG5)
#include <sys/byteorder.h>
#endif

#ifndef _IN_PORT_T
#define	_IN_PORT_T
typedef	uint16_t	in_port_t;
#endif

/*
 * Note: IPv4 address data structures usage conventions.
 * The "in_addr_t" type below (required by Unix standards)
 * is NOT a typedef of "struct in_addr" and violates the usual
 * conventions where "struct <name>" and <name>_t are corresponding
 * typedefs.
 * To minimize confusion, kernel data structures/usage prefers use
 * of "ipaddr_t" as atomic uint32_t type and avoid using "in_addr_t"
 * The user level APIs continue to follow the historic popular
 * practice of using "struct in_addr".
 */
#ifndef _IN_ADDR_T
#define	_IN_ADDR_T
typedef	uint32_t	in_addr_t;
#endif

#ifndef _IPADDR_T
#define	_IPADDR_T
typedef uint32_t ipaddr_t;
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

struct in6_addr {
	union {
		/*
		 * Note: Static initalizers of "union" type assume
		 * the constant on the RHS is the type of the first member
		 * of union.
		 * To make static initializers (and efficient usage) work,
		 * the order of members exposed to user and kernel view of
		 * this data structure is different.
		 * User environment sees specified uint8_t type as first
		 * member whereas kernel sees most efficient type as
		 * first member.
		 */
#ifdef _KERNEL
		uint32_t	_S6_u32[4];	/* IPv6 address */
		uint8_t		_S6_u8[16];	/* IPv6 address */
#else
		uint8_t		_S6_u8[16];	/* IPv6 address */
		uint32_t	_S6_u32[4];	/* IPv6 address */
#endif
		uint32_t	__S6_align;	/* Align on 32 bit boundary */
	} _S6_un;
};
#define	s6_addr		_S6_un._S6_u8

#ifdef _KERNEL
#define	s6_addr8	_S6_un._S6_u8
#define	s6_addr32	_S6_un._S6_u32
#endif

typedef struct in6_addr in6_addr_t;

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifndef _SA_FAMILY_T
#define	_SA_FAMILY_T
typedef	unsigned short	sa_family_t;
#endif

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define	IPPROTO_HOPOPTS		0		/* Hop by hop header for IPv6 */
#define	IPPROTO_ICMP		1		/* control message protocol */
#define	IPPROTO_IGMP		2		/* group control protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define	IPPROTO_ENCAP		4		/* IP in IP encapsulation */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define	IPPROTO_PUP		12		/* pup */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define	IPPROTO_IDP		22		/* xns idp */
#define	IPPROTO_IPV6		41		/* IPv6 encapsulated in IP */
#define	IPPROTO_ROUTING		43		/* Routing header for IPv6 */
#define	IPPROTO_FRAGMENT	44		/* Fragment header for IPv6 */
#define	IPPROTO_RSVP		46		/* rsvp */
#define	IPPROTO_ESP		50		/* IPsec Encap. Sec. Payload */
#define	IPPROTO_AH		51		/* IPsec Authentication Hdr. */
#define	IPPROTO_ICMPV6		58		/* ICMP for IPv6 */
#define	IPPROTO_NONE		59		/* No next header for IPv6 */
#define	IPPROTO_DSTOPTS		60		/* Destination options */
#define	IPPROTO_HELLO		63		/* "hello" routing protocol */
#define	IPPROTO_ND		77		/* UNOFFICIAL net disk proto */
#define	IPPROTO_EON		80		/* ISO clnp */
#define	IPPROTO_PIM		103		/* PIM routing protocol */

#define	IPPROTO_RAW		255		/* raw IP packet */
#define	IPPROTO_MAX		256

/*
 * Port/socket numbers: network standard functions
 */
#define	IPPORT_ECHO		7
#define	IPPORT_DISCARD		9
#define	IPPORT_SYSTAT		11
#define	IPPORT_DAYTIME		13
#define	IPPORT_NETSTAT		15
#define	IPPORT_FTP		21
#define	IPPORT_TELNET		23
#define	IPPORT_SMTP		25
#define	IPPORT_TIMESERVER	37
#define	IPPORT_NAMESERVER	42
#define	IPPORT_WHOIS		43
#define	IPPORT_MTP		57

/*
 * Port/socket numbers: host specific functions
 */
#define	IPPORT_BOOTPS		67
#define	IPPORT_BOOTPC		68
#define	IPPORT_TFTP		69
#define	IPPORT_RJE		77
#define	IPPORT_FINGER		79
#define	IPPORT_TTYLINK		87
#define	IPPORT_SUPDUP		95

/*
 * UNIX TCP sockets
 */
#define	IPPORT_EXECSERVER	512
#define	IPPORT_LOGINSERVER	513
#define	IPPORT_CMDSERVER	514
#define	IPPORT_EFSSERVER	520

/*
 * UNIX UDP sockets
 */
#define	IPPORT_BIFFUDP		512
#define	IPPORT_WHOSERVER	513
#define	IPPORT_ROUTESERVER	520	/* 520+1 also used */

/*
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */
#define	IPPORT_RESERVED		1024
#define	IPPORT_USERRESERVED	5000

/*
 * Link numbers
 */
#define	IMPLINK_IP		155
#define	IMPLINK_LOWEXPER	156
#define	IMPLINK_HIGHEXPER	158

/*
 * IPv4 Internet address
 *	This definition contains obsolete fields for compatibility
 *	with SunOS 3.x and 4.2bsd.  The presence of subnets renders
 *	divisions into fixed fields misleading at best.  New code
 *	should use only the s_addr field.
 */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	_S_un_b	S_un_b
#define	_S_un_w	S_un_w
#define	_S_addr	S_addr
#define	_S_un	S_un
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

struct in_addr {
	union {
		struct { uint8_t s_b1, s_b2, s_b3, s_b4; } _S_un_b;
		struct { uint16_t s_w1, s_w2; } _S_un_w;
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
		uint32_t _S_addr;
#else
		in_addr_t _S_addr;
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
	} _S_un;
#define	s_addr	_S_un._S_addr		/* should be used for all code */
#define	s_host	_S_un._S_un_b.s_b2	/* OBSOLETE: host on imp */
#define	s_net	_S_un._S_un_b.s_b1	/* OBSOLETE: network */
#define	s_imp	_S_un._S_un_w.s_w2	/* OBSOLETE: imp */
#define	s_impno	_S_un._S_un_b.s_b4	/* OBSOLETE: imp # */
#define	s_lh	_S_un._S_un_b.s_b3	/* OBSOLETE: logical host */
};

/*
 * Definitions of bits in internet address integers.
 * On subnets, the decomposition of addresses to host and net parts
 * is done according to subnet mask, not the masks here.
 */
#define	IN_CLASSA(i)		(((i) & 0x80000000U) == 0)
#define	IN_CLASSA_NET		0xff000000U
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		0x00ffffffU
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(i)		(((i) & 0xc0000000U) == 0x80000000U)
#define	IN_CLASSB_NET		0xffff0000U
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		0x0000ffffU
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(i)		(((i) & 0xe0000000U) == 0xc0000000U)
#define	IN_CLASSC_NET		0xffffff00U
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		0x000000ffU

#define	IN_CLASSD(i)		(((i) & 0xf0000000U) == 0xe0000000U)
#define	IN_CLASSD_NET		0xf0000000U	/* These aren't really  */
#define	IN_CLASSD_NSHIFT	28		/* net and host fields, but */
#define	IN_CLASSD_HOST		0x0fffffffU	/* routing needn't know */
#define	IN_MULTICAST(i)		IN_CLASSD(i)

#define	IN_EXPERIMENTAL(i)	(((i) & 0xe0000000U) == 0xe0000000U)
#define	IN_BADCLASS(i)		(((i) & 0xf0000000U) == 0xf0000000U)

#define	INADDR_ANY		0x00000000U
#define	INADDR_LOOPBACK		0x7F000001U
#define	INADDR_BROADCAST	0xffffffffU	/* must be masked */

#define	INADDR_UNSPEC_GROUP	0xe0000000U	/* 224.0.0.0   */
#define	INADDR_ALLHOSTS_GROUP	0xe0000001U	/* 224.0.0.1   */
#define	INADDR_ALLRTRS_GROUP	0xe0000002U	/* 224.0.0.2   */
#define	INADDR_MAX_LOCAL_GROUP	0xe00000ffU	/* 224.0.0.255 */

#define	IN_LOOPBACKNET		127			/* official! */

/*
 * Define a macro to stuff the loopback address into an Internet address
 */
#if !defined(_XPG4_2) || !defined(__EXTENSIONS__)
#define	IN_SET_LOOPBACK_ADDR(a) \
	{ (a)->sin_addr.s_addr  = htonl(INADDR_LOOPBACK); \
	(a)->sin_family = AF_INET; }
#endif /* !defined(_XPG4_2) || !defined(__EXTENSIONS__) */

/*
 * IPv4 Socket address.
 */
struct sockaddr_in {
	sa_family_t	sin_family;
	in_port_t	sin_port;
	struct	in_addr sin_addr;
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
	char		sin_zero[8];
#else
	unsigned char	sin_zero[8];
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
};

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * IPv6 socket address.
 */
struct sockaddr_in6 {
	sa_family_t	sin6_family;
	in_port_t	sin6_port;
	uint32_t	sin6_flowinfo;
	struct in6_addr	sin6_addr;
	uint32_t	sin6_scope_id;  /* Depends on scope of sin6_addr */
	uint32_t	__sin6_src_id;	/* Impl. specific - UDP replies */
};

/*
 * Macros for accessing the traffic class and flow label fields from
 * sin6_flowinfo.
 * These are designed to be applied to a 32-bit value.
 */
#ifdef _BIG_ENDIAN

/* masks */
#define	IPV6_FLOWINFO_FLOWLABEL			0x000fffffU
#define	IPV6_FLOWINFO_TCLASS			0x0ff00000U

#else /* _BIG_ENDIAN */

/* masks */
#define	IPV6_FLOWINFO_FLOWLABEL			0xffff0f00U
#define	IPV6_FLOWINFO_TCLASS			0x0000f00fU

#endif	/* _BIG_ENDIAN */

/*
 * Note: Macros IN6ADDR_ANY_INIT and IN6ADDR_LOOPBACK_INIT are for
 * use as RHS of Static initializers of "struct in6_addr" (or in6_addr_t)
 * only. They need to be different for User/Kernel versions because union
 * component data structure is defined differently (it is identical at
 * binary representation level).
 *
 * const struct in6_addr IN6ADDR_ANY_INIT;
 * const struct in6_addr IN6ADDR_LOOPBACK_INIT;
 */

#ifdef _KERNEL
#define	IN6ADDR_ANY_INIT		{ 0, 0, 0, 0 }

#ifdef _BIG_ENDIAN
#define	IN6ADDR_LOOPBACK_INIT		{ 0, 0, 0, 0x00000001U }
#else /* _BIG_ENDIAN */
#define	IN6ADDR_LOOPBACK_INIT		{ 0, 0, 0, 0x01000000U }
#endif /* _BIG_ENDIAN */

#else

#define	IN6ADDR_ANY_INIT	    {	0, 0, 0, 0,	\
					0, 0, 0, 0,	\
					0, 0, 0, 0, 	\
					0, 0, 0, 0 }

#define	IN6ADDR_LOOPBACK_INIT	    {	0, 0, 0, 0,	\
					0, 0, 0, 0,	\
					0, 0, 0, 0,	\
					0, 0, 0, 0x1U }
#endif /* _KERNEL */

/*
 * RFC 2553 specifies the following macros. Their type is defined
 * as "int" in the RFC but they only have boolean significance
 * (zero or non-zero). For the purposes of our comment notation,
 * we assume a hypothetical type "bool" defined as follows to
 * write the prototypes assumed for macros in our comments better.
 *
 * typedef int bool;
 */

/*
 * IN6 macros used to test for special IPv6 addresses
 * (Mostly from spec)
 *
 * bool  IN6_IS_ADDR_UNSPECIFIED (const struct in6_addr *);
 * bool  IN6_IS_ADDR_LOOPBACK    (const struct in6_addr *);
 * bool  IN6_IS_ADDR_MULTICAST   (const struct in6_addr *);
 * bool  IN6_IS_ADDR_LINKLOCAL   (const struct in6_addr *);
 * bool  IN6_IS_ADDR_SITELOCAL   (const struct in6_addr *);
 * bool  IN6_IS_ADDR_V4MAPPED    (const struct in6_addr *);
 * bool  IN6_IS_ADDR_V4MAPPED_ANY(const struct in6_addr *); -- Not from RFC2553
 * bool  IN6_IS_ADDR_V4COMPAT    (const struct in6_addr *);
 * bool  IN6_IS_ADDR_MC_RESERVED (const struct in6_addr *); -- Not from RFC2553
 * bool  IN6_IS_ADDR_MC_NODELOCAL(const struct in6_addr *);
 * bool  IN6_IS_ADDR_MC_LINKLOCAL(const struct in6_addr *);
 * bool  IN6_IS_ADDR_MC_SITELOCAL(const struct in6_addr *);
 * bool  IN6_IS_ADDR_MC_ORGLOCAL (const struct in6_addr *);
 * bool  IN6_IS_ADDR_MC_GLOBAL   (const struct in6_addr *);
 */

#define	IN6_IS_ADDR_UNSPECIFIED(addr) \
	(((addr)->_S6_un._S6_u32[3] == 0) && \
	((addr)->_S6_un._S6_u32[2] == 0) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_LOOPBACK(addr) \
	(((addr)->_S6_un._S6_u32[3] == 0x00000001) && \
	((addr)->_S6_un._S6_u32[2] == 0) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))
#else /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_LOOPBACK(addr) \
	(((addr)->_S6_un._S6_u32[3] == 0x01000000) && \
	((addr)->_S6_un._S6_u32[2] == 0) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MULTICAST(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff000000) == 0xff000000)
#else /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MULTICAST(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x000000ff) == 0x000000ff)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_LINKLOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xffc00000) == 0xfe800000)
#else /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_LINKLOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x0000c0ff) == 0x000080fe)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_SITELOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xffc00000) == 0xfec00000)
#else /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_SITELOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x0000c0ff) == 0x0000c0fe)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_V4MAPPED(addr) \
	(((addr)->_S6_un._S6_u32[2] == 0x0000ffff) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))
#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_V4MAPPED(addr) \
	(((addr)->_S6_un._S6_u32[2] == 0xffff0000U) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))
#endif /* _BIG_ENDIAN */

/*
 * IN6_IS_ADDR_V4MAPPED - A IPv4 mapped INADDR_ANY
 * Note: This macro is currently NOT defined in RFC2553 specification
 * and not a standard macro that portable applications should use.
 */
#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_V4MAPPED_ANY(addr) \
	(((addr)->_S6_un._S6_u32[3] == 0) && \
	((addr)->_S6_un._S6_u32[2] == 0x0000ffff) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))
#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_V4MAPPED_ANY(addr) \
	(((addr)->_S6_un._S6_u32[3] == 0) && \
	((addr)->_S6_un._S6_u32[2] == 0xffff0000U) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0))
#endif /* _BIG_ENDIAN */

/* Exclude loopback and unspecified address */
#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_V4COMPAT(addr) \
	(((addr)->_S6_un._S6_u32[2] == 0) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0) && \
	!((addr)->_S6_un._S6_u32[3] == 0) && \
	!((addr)->_S6_un._S6_u32[3] == 0x00000001))

#else /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_V4COMPAT(addr) \
	(((addr)->_S6_un._S6_u32[2] == 0) && \
	((addr)->_S6_un._S6_u32[1] == 0) && \
	((addr)->_S6_un._S6_u32[0] == 0) && \
	!((addr)->_S6_un._S6_u32[3] == 0) && \
	!((addr)->_S6_un._S6_u32[3] == 0x01000000))
#endif /* _BIG_ENDIAN */

/*
 * Note:
 * IN6_IS_ADDR_MC_RESERVED macro is currently NOT defined in RFC2553
 * specification and not a standard macro that portable applications
 * should use.
 */
#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_RESERVED(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff0f0000) == 0xff000000)

#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_RESERVED(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x00000fff) == 0x000000ff)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_NODELOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff0f0000) == 0xff010000)
#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_NODELOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x00000fff) == 0x000001ff)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_LINKLOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff0f0000) == 0xff020000)
#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_LINKLOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x00000fff) == 0x000002ff)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_SITELOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff0f0000) == 0xff050000)
#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_SITELOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x00000fff) == 0x000005ff)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_ORGLOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff0f0000) == 0xff080000)
#else  /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_ORGLOCAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x00000fff) == 0x000008ff)
#endif /* _BIG_ENDIAN */

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_GLOBAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0xff0f0000) == 0xff0e0000)
#else /* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_GLOBAL(addr) \
	(((addr)->_S6_un._S6_u32[0] & 0x00000fff) == 0x00000eff)
#endif /* _BIG_ENDIAN */

/*
 * Useful utility macros for operations with IPv6 addresses
 * Note: These macros are NOT defined in the RFC2553 or any other
 * standard specification and are not standard macros that portable
 * applications should use.
 */

/*
 * IN6_V4MAPPED_TO_INADDR
 * IN6_V4MAPPED_TO_IPADDR
 *	Assign a IPv4-Mapped IPv6 address to an IPv4 address.
 *	Note: These macros are NOT defined in RFC2553 or any other standard
 *	specification and are not macros that portable applications should
 *	use.
 *
 * void IN6_V4MAPPED_TO_INADDR(const in6_addr_t *v6, struct in_addr *v4);
 * void IN6_V4MAPPED_TO_IPADDR(const in6_addr_t *v6, ipaddr_t v4);
 *
 */
#define	IN6_V4MAPPED_TO_INADDR(v6, v4) \
	((v4)->s_addr = (v6)->_S6_un._S6_u32[3])
#define	IN6_V4MAPPED_TO_IPADDR(v6, v4) \
	((v4) = (v6)->_S6_un._S6_u32[3])

/*
 * IN6_INADDR_TO_V4MAPPED
 * IN6_IPADDR_TO_V4MAPPED
 *	Assign a IPv4 address address to an IPv6 address as a IPv4-mapped
 *	address.
 *	Note: These macros are NOT defined in RFC2553 or any other standard
 *	specification and are not macros that portable applications should
 *	use.
 *
 * void IN6_INADDR_TO_V4MAPPED(const struct in_addr *v4, in6_addr_t *v6);
 * void IN6_IPADDR_TO_V4MAPPED(const ipaddr_t v4, in6_addr_t *v6);
 *
 */
#ifdef _BIG_ENDIAN
#define	IN6_INADDR_TO_V4MAPPED(v4, v6) \
	((v6)->_S6_un._S6_u32[3] = (v4)->s_addr, \
	(v6)->_S6_un._S6_u32[2] = 0x0000ffff, \
	(v6)->_S6_un._S6_u32[1] = 0, \
	(v6)->_S6_un._S6_u32[0] = 0)
#define	IN6_IPADDR_TO_V4MAPPED(v4, v6) \
	((v6)->_S6_un._S6_u32[3] = (v4), \
	(v6)->_S6_un._S6_u32[2] = 0x0000ffff, \
	(v6)->_S6_un._S6_u32[1] = 0, \
	(v6)->_S6_un._S6_u32[0] = 0)
#else /* _BIG_ENDIAN */
#define	IN6_INADDR_TO_V4MAPPED(v4, v6) \
	((v6)->_S6_un._S6_u32[3] = (v4)->s_addr, \
	(v6)->_S6_un._S6_u32[2] = 0xffff0000U, \
	(v6)->_S6_un._S6_u32[1] = 0, \
	(v6)->_S6_un._S6_u32[0] = 0)
#define	IN6_IPADDR_TO_V4MAPPED(v4, v6) \
	((v6)->_S6_un._S6_u32[3] = (v4), \
	(v6)->_S6_un._S6_u32[2] = 0xffff0000U, \
	(v6)->_S6_un._S6_u32[1] = 0, \
	(v6)->_S6_un._S6_u32[0] = 0)
#endif /* _BIG_ENDIAN */

/*
 * IN6_ARE_ADDR_EQUAL (defined in RFC2292)
 *	 Compares if IPv6 addresses are equal.
 * Note: Compares in order of high likelyhood of a miss so we minimize
 * compares. (Current heuristic order, compare in reverse order of
 * uint32_t units)
 *
 * bool  IN6_ARE_ADDR_EQUAL(const struct in6_addr *,
 *			    const struct in6_addr *);
 */
#define	IN6_ARE_ADDR_EQUAL(addr1, addr2) \
	(((addr1)->_S6_un._S6_u32[3] == (addr2)->_S6_un._S6_u32[3]) && \
	((addr1)->_S6_un._S6_u32[2] == (addr2)->_S6_un._S6_u32[2]) && \
	((addr1)->_S6_un._S6_u32[1] == (addr2)->_S6_un._S6_u32[1]) && \
	((addr1)->_S6_un._S6_u32[0] == (addr2)->_S6_un._S6_u32[0]))

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */


/*
 * Options for use with [gs]etsockopt at the IP level.
 *
 * Note: Some of the IP_ namespace has conflict with and
 * and is exposed through <xti.h>. (It also requires exposing
 * options not implemented). The options with potential
 * for conflicts use #ifndef guards.
 */
#ifndef IP_OPTIONS
#define	IP_OPTIONS	1	/* set/get IP per-packet options   */
#endif

#define	IP_HDRINCL	2	/* int; header is included with data (raw) */

#ifndef IP_TOS
#define	IP_TOS		3	/* int; IP type of service and precedence */
#endif

#ifndef IP_TTL
#define	IP_TTL		4	/* int; IP time to live */
#endif

#define	IP_RECVOPTS	5	/* int; receive all IP options w/datagram */
#define	IP_RECVRETOPTS	6	/* int; receive IP options for response */
#define	IP_RECVDSTADDR	7	/* int; receive IP dst addr w/datagram */
#define	IP_RETOPTS	8	/* ip_opts; set/get IP per-packet options */
#define	IP_MULTICAST_IF		0x10	/* set/get IP multicast interface  */
#define	IP_MULTICAST_TTL	0x11	/* set/get IP multicast timetolive */
#define	IP_MULTICAST_LOOP	0x12	/* set/get IP multicast loopback   */
#define	IP_ADD_MEMBERSHIP	0x13	/* add	an IP group membership	   */
#define	IP_DROP_MEMBERSHIP	0x14	/* drop an IP group membership	   */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * Different preferences that can be requested from IPSEC protocols.
 */
#define	IP_SEC_OPT		0x22	/* Used to set IPSEC options */
#define	IPSEC_PREF_NEVER	0x01
#define	IPSEC_PREF_REQUIRED	0x02
#define	IPSEC_PREF_UNIQUE	0x04
/*
 * This can be used with the setsockopt() call to set per socket security
 * options. When the application uses per-socket API, we will reflect
 * the request on both outbound and inbound packets.
 */

typedef struct ipsec_req {
	uint_t 		ipsr_ah_req;		/* AH request */
	uint_t 		ipsr_esp_req;		/* ESP request */
	uint_t		ipsr_self_encap_req;	/* Self-Encap request */
	uint8_t		ipsr_auth_alg;		/* Auth algs for AH */
	uint8_t		ipsr_esp_alg;		/* Encr algs for ESP */
	uint8_t		ipsr_esp_auth_alg;	/* Auth algs for ESP */
} ipsec_req_t;
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * SunOS private (potentially not portable) IP_ option names
 */
#define	IP_ADD_PROXY_ADDR	0x40	/* take in_prefix_t; add proxy addr. */
#define	IP_BOUND_IF		0x41	/* bind socket to an ifindex	   */
#define	IP_UNSPEC_SRC		0x42	/* use unspecified source address   */

/*
 * Option values and names (when !_XPG5) shared with <xti_inet.h>
 */
#ifndef IP_REUSEADDR
#define	IP_REUSEADDR		0x104
#endif

#ifndef IP_DONTROUTE
#define	IP_DONTROUTE		0x105
#endif

#ifndef IP_BROADCAST
#define	IP_BROADCAST		0x106
#endif

/*
 * The following option values are reserved by <xti_inet.h>
 *
 * T_IP_OPTIONS	0x107	 -  IP per-packet options
 * T_IP_TOS	0x108	 -  IP per packet type of service
 */

/*
 * Default value constants for multicast attributes controlled by
 * IP*_MULTICAST_LOOP and IP*_MULTICAST_{TTL,HOPS} options.
 */
#define	IP_DEFAULT_MULTICAST_TTL  1	/* normally limit m'casts to 1 hop */
#define	IP_DEFAULT_MULTICAST_LOOP 1	/* normally hear sends if a member */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
	struct in_addr	imr_multiaddr;	/* IP multicast address of group */
	struct in_addr	imr_interface;	/* local IP address of interface */
};

/*
 * Argument structure for IPV6_JOIN_GROUP and IPV6_LEAVE_GROUP on
 * IPv6 addresses.
 */
struct ipv6_mreq {
	struct in6_addr	ipv6mr_multiaddr;	/* IPv6 multicast addr */
	unsigned int	ipv6mr_interface;	/* interface index */
};

/*
 * Argument struct for IPV6_PKTINFO option
 */
struct in6_pktinfo {
	struct in6_addr		ipi6_addr;	/* src/dst IPv6 address */
	unsigned int		ipi6_ifindex;	/* send/recv interface index */
};

/*
 * IPv6 routing header types
 */
#define	IPV6_RTHDR_TYPE_0	0

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * Argument structure for IP_ADD_PROXY_ADDR.
 * Note that this is an unstable, experimental interface. It may change
 * later. Don't use it unless you know what it is.
 */
typedef struct {
	struct in_addr	in_prefix_addr;
	unsigned int	in_prefix_len;
} in_prefix_t;

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
/*
 * IPv6 options
 */
#define	IPV6_UNICAST_HOPS	0x5	/* hop limit value for unicast */
					/* packets. */
					/* argument type: uint_t */
#define	IPV6_MULTICAST_IF	0x6	/* outgoing interface for */
					/* multicast packets. */
					/* argument type: struct in6_addr */
#define	IPV6_MULTICAST_HOPS	0x7	/* hop limit value to use for */
					/* multicast packets. */
					/* argument type: uint_t */
#define	IPV6_MULTICAST_LOOP	0x8	/* enable/disable delivery of */
					/* multicast packets on same socket. */
					/* argument type: uint_t */
#define	IPV6_JOIN_GROUP		0x9	/* join an IPv6 multicast group. */
					/* argument type: struct ipv6_mreq */
#define	IPV6_LEAVE_GROUP	0xa	/* leave an IPv6 multicast group */
					/* argument type: struct ipv6_mreq */
/*
 * IPV6_ADD_MEMBERSHIP and IPV6_DROP_MEMBERSHIP are being kept
 * for backward compatibility. They have the same meaning as IPV6_JOIN_GROUP
 * and IPV6_LEAVE_GROUP respectively.
 */
#define	IPV6_ADD_MEMBERSHIP	0x9	/* join an IPv6 multicast group. */
					/* argument type: struct ipv6_mreq */
#define	IPV6_DROP_MEMBERSHIP	0xa	/* leave an IPv6 multicast group */
					/* argument type: struct ipv6_mreq */

#define	IPV6_PKTINFO		0xb	/* addr plus interface index */
					/* arg type: "struct in6_pktingo" - */
#define	IPV6_HOPLIMIT		0xc	/* hoplimit for datagram */
#define	IPV6_NEXTHOP		0xd	/* next hop address  */
#define	IPV6_HOPOPTS		0xe	/* hop by hop options */
#define	IPV6_DSTOPTS		0xf	/* destination options - after */
					/* the routing header */
#define	IPV6_RTHDR		0x10	/* routing header  */
#define	IPV6_RTHDRDSTOPTS	0x11	/* destination options - before */
					/* the routing header */
#define	IPV6_RECVPKTINFO	0x12	/* enable/disable IPV6_PKTINFO */
#define	IPV6_RECVHOPLIMIT	0x13	/* enable/disable IPV6_HOPLIMIT */
#define	IPV6_RECVHOPOPTS	0x14	/* enable/disable IPV6_HOPOPTS */
#define	IPV6_RECVDSTOPTS	0x15	/* enable/disable IPV6_DSTOPTS */
#define	IPV6_RECVRTHDR		0x16	/* enable/disable IPV6_RTHDR */
#define	IPV6_RECVRTHDRDSTOPTS	0x17	/* enable/disable IPV6_RTHDRDSTOPTS */

#define	IPV6_CHECKSUM		0x18	/* Control checksum on raw sockets */

/*
 * SunOS private (potentially not portable) IPV6_ option names
 */
#define	IPV6_BOUND_IF		0x41	/* bind to an ifindex */
#define	IPV6_UNSPEC_SRC		0x42	/* source of packets set to */
					/* unspecified (all zeros) */

/*
 * Miscellaneous IPv6 constants.
 */
#define	INET_ADDRSTRLEN		16	/* max len IPv4 addr in ascii dotted */
					/* decimal notation. */
#define	INET6_ADDRSTRLEN	46	/* max len of IPv6 addr in ascii */
					/* standard colon-hex notation. */
#define	IPV6_PAD1_OPT		0	/* pad byte in IPv6 extension hdrs */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * Extern declarations for pre-defined global const variables
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#ifndef _KERNEL
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#endif
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _NETINET_IN_H */
