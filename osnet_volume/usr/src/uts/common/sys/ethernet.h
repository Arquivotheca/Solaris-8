/*
 * Copyright (c) 1990-1993,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * ethernet.h header for common Ethernet declarations.
 */

#ifndef	_SYS_ETHERNET_H
#define	_SYS_ETHERNET_H

#pragma ident	"@(#)ethernet.h	1.13	99/10/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ETHERADDRL	(6)		/* ethernet address length in octets */
#define	ETHERFCSL	(4)		/* ethernet FCS length in octets */

/*
 * Ethernet address - 6 octets
 */
struct	ether_addr {
	uchar_t	ether_addr_octet[ETHERADDRL];
};

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct	ether_header {
	struct	ether_addr	ether_dhost;
	struct	ether_addr	ether_shost;
	ushort_t		ether_type;
};

#define	ETHERTYPE_PUP		(0x0200)	/* PUP protocol */
#define	ETHERTYPE_IP		(0x0800)	/* IP protocol */
#define	ETHERTYPE_ARP		(0x0806)	/* Addr. resolution protocol */
#define	ETHERTYPE_REVARP	(0x8035)	/* Reverse ARP */
#define	ETHERTYPE_IPV6		(0x86dd)	/* IPv6 */
#define	ETHERTYPE_MAX		(0xffff)	/* Max valid ethernet type */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		(0x1000)	/* Trailer packet */
#define	ETHERTYPE_NTRAILER	(16)

#define	ETHERMTU		(1500)	/* max frame w/o header or fcs */
#define	ETHERMIN		(60)	/* min frame w/header w/o fcs */
#define	ETHERMAX		(1514)	/* max frame w/header w/o fcs */

/*
 * Compare two Ethernet addresses - assumes that the two given
 * pointers can be referenced as shorts.  On architectures
 * where this is not the case, use bcmp instead.  Note that like
 * bcmp, we return zero if they are the SAME.
 */

#if defined(sparc) || defined(__sparc)
#define	ether_cmp(a, b) (((short *)b)[2] != ((short *)a)[2] || \
	((short *)b)[1] != ((short *)a)[1] || \
	((short *)b)[0] != ((short *)a)[0])
#else
#define	ether_cmp(a, b) (bcmp((caddr_t)a, (caddr_t)b, 6))
#endif

/*
 * Copy Ethernet addresses from a to b - assumes that the two given
 * pointers can be referenced as shorts.  On architectures
 * where this is not the case, use bcopy instead.
 */

#if defined(sparc) || defined(__sparc)
#define	ether_copy(a, b) { ((short *)b)[0] = ((short *)a)[0]; \
	((short *)b)[1] = ((short *)a)[1]; ((short *)b)[2] = ((short *)a)[2]; }
#else
#define	ether_copy(a, b) (bcopy((caddr_t)a, (caddr_t)b, 6))
#endif

#if defined(_KERNEL)
extern int localetheraddr(struct ether_addr *, struct ether_addr *);
extern char *ether_sprintf(struct ether_addr *);
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ETHERNET_H */
