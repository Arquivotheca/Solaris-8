/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * IPv4 implementation-specific definitions
 */

#ifndef _IPV4_H
#define	_IPV4_H

#pragma ident	"@(#)ipv4.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	FRAG_MAX	(10)	/* max number of IP fragments per datagram */
#define	FRAG_SUCCESS	(0)	/* datagram reassembled ok */
#define	FRAG_DUP	(1)	/* duplicate ip fragment */
#define	FRAG_NOSLOTS	(2)	/* no more ip fragment slots */
#define	FRAG_ATTEMPTS	1	/* Try twice to get all the fragments */

/*
 * IP fragmentation data structure
 */
struct ip_frag {
	int16_t		more;	/* Fragment bit (TRUE == MF, FALSE == No more */
	int16_t		offset;	/* Offset within the encapsulated datagram */
	caddr_t		datap;	/* Fragment including IP header */
	int16_t		datal;	/* raw fragment length */
	uint16_t	ipid;	/* fragment ident */
	int16_t		iplen;	/* IP datagram's length */
	int16_t		iphlen;	/* Len of IP header */
};

#define	RT_UNUSED	1	/* Table entry is unused */
#define	RT_DEFAULT	2	/* Gateway is a default router */
#define	RT_HOST		4	/* Destination is a host */
#define	RT_NET		8	/* Destination is a network */
#define	RT_NG		10	/* Route is No Good */
#define	IPV4_ROUTE_TABLE_SIZE	(5)	/* Number of entries in the table */
#define	IPV4_ADD_ROUTE		0
#define	IPV4_DEL_ROUTE		1
#define	IPV4_BAD_ROUTE		2

/*
 * true offset is in 8 octet units. The high order 3 bits of the IP header
 * offset field are therefore used for fragmentation flags. Shift these
 * bits off to produce the true offset. The high order flag bit is unused
 * (what would be considered the sign bit). Still, we cast the callers
 * value as an unsigned quantity to ensure it is treated as positive.
 */
#define	IPV4_OFFSET(a)	((uint16_t)(a) << 3)

/*
 * IP routing table. IP addresses are in network-order.
 */
struct routing {
	struct in_addr	dest;
	struct in_addr	gateway;
	uint8_t		flag;
};

extern char		*inet_ntoa(struct in_addr);
extern void		ipv4_raw_socket(struct inetboot_socket *, uint8_t);
extern void		ipv4_socket_init(struct inetboot_socket *);
extern int		ipv4_header_len(void);
extern int		ipv4_setpromiscuous(int);
extern void		ipv4_setipaddr(struct in_addr *);
extern void		ipv4_getipaddr(struct in_addr *);
extern void		ipv4_setnetmask(struct in_addr *);
extern void		ipv4_getnetmask(struct in_addr *);
extern void		ipv4_setmaxttl(uint8_t);
extern int		gethostname(caddr_t, int);
extern int		sethostname(caddr_t, int);
extern int		ipv4_input(int);
extern int		ipv4_output(int, struct inetgram *);
extern int		ipv4_route(int, uint8_t, struct in_addr *,
			    struct in_addr *);
extern struct in_addr	*ipv4_get_route(uint8_t, struct in_addr *,
			    struct in_addr *);

#ifdef	__cplusplus
}
#endif

#endif /* _IPV4_H */
