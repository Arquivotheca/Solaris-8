/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1989,1996-1999 by Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Routing Information Protocol for IPv6 (RIPng)
 * as specfied by RFC 2080.
 */

#ifndef _PROTOCOLS_RIPNGD_H
#define	_PROTOCOLS_RIPNGD_H

#pragma ident	"@(#)ripngd.h	1.1	99/03/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct netinfo6 {
	struct in6_addr	rip6_prefix;		/* destination prefix */
	uint16_t	rip6_route_tag;		/* route tag */
	uint8_t		rip6_prefix_length;	/* destination prefix length */
	uint8_t		rip6_metric;		/* cost of route */
};

struct rip6 {
	uint8_t		rip6_cmd;		/* request/response */
	uint8_t		rip6_vers;		/* protocol version # */
	uint16_t	rip6_res1;		/* pad to 32-bit boundary */
	struct netinfo6	rip6_nets[1];		/* variable length... */
};

#define	RIPVERSION6		1

/*
 * Packet types.
 */
#define	RIPCMD6_REQUEST		1	/* want info - from suppliers */
#define	RIPCMD6_RESPONSE	2	/* responding to request */

#define	IPPORT_ROUTESERVER6	521

#ifdef	__cplusplus
}
#endif

#endif	/* _PROTOCOLS_RIPNGD_H */
