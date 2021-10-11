/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)af.c	1.7	98/06/16 SMI"	/* SVr4.0 1.1	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1991  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


#include "defs.h"

/*
 * Address family support routines
 */
static int inet_hash(struct sockaddr_in *sin, struct afhash *hp);
static int inet_netmatch(struct sockaddr_in *sin1, struct sockaddr_in *sin2);
static int inet_portmatch(struct sockaddr_in *sin);
static int inet_portcheck(struct sockaddr_in *sin);
static int inet_output(int s, int flags, struct sockaddr_in *sin, int size);
static int inet_checkhost(struct sockaddr_in *sin);
static int inet_canon(struct sockaddr_in *sin);
static char *inet_format(struct sockaddr_in *sin);

#define	NIL	{ 0 }
/* BEGIN CSTYLED */
#define	INET \
	{ inet_hash,		inet_netmatch,		inet_output, \
	  inet_portmatch,	inet_portcheck,		inet_checkhost, \
	  inet_rtflags,		inet_sendsubnet,	inet_canon, \
	  inet_format \
	}
/* END CSTYLED */

struct afswitch afswitch[AF_MAX] = {
	NIL,		/* 0- unused */
	NIL,		/* 1- Unix domain, unused */
	INET,		/* Internet */
};

int af_max = sizeof (afswitch) / sizeof (afswitch[0]);

struct sockaddr_in inet_default = { AF_INET, INADDR_ANY };

static int
inet_hash(struct sockaddr_in *sin, struct afhash *hp)
{
	register ulong_t n;

	n = inet_netof(sin->sin_addr);
	if (n)
	    while ((n & 0xff) == 0)
		n >>= 8;
	hp->afh_nethash = n;
	hp->afh_hosthash = ntohl(sin->sin_addr.s_addr);
	hp->afh_hosthash &= 0x7fffffff;
	return (0);
}

static int
inet_netmatch(struct sockaddr_in *sin1, struct sockaddr_in *sin2)
{

	return (inet_netof(sin1->sin_addr) == inet_netof(sin2->sin_addr));
}

/*
 * Verify the message is from the right port.
 */
static int
inet_portmatch(struct sockaddr_in *sin)
{

	return (sin->sin_port == htons(IPPORT_ROUTESERVER));
}

/*
 * Verify the message is from a "trusted" port.
 */
static int
inet_portcheck(struct sockaddr_in *sin)
{

	return (ntohs(sin->sin_port) <= IPPORT_RESERVED);
}

/*
 * Internet output routine.
 */
static int
inet_output(int s, int flags, struct sockaddr_in *sin, int size)
{
	struct sockaddr_in dst;

	dst = *sin;
	sin = &dst;
	if (sin->sin_port == 0)
		sin->sin_port = htons(IPPORT_ROUTESERVER);
	if (sendto(s, packet, size, flags, (struct sockaddr *)sin,
	    sizeof (*sin)) < 0)
		perror("sendto");
	return (0);
}

/*
 * Return 1 if the address is believed
 * for an Internet host -- THIS IS A KLUDGE.
 */
static int
inet_checkhost(struct sockaddr_in *sin)
{
	register ulong_t i = ntohl(sin->sin_addr.s_addr);

	if (IN_BADCLASS(i) || sin->sin_port != 0)
		return (0);
	if (i != 0 && (i & 0xff000000) == 0)
		return (0);
	for (i = 0; i < sizeof (sin->sin_zero)/sizeof (sin->sin_zero[0]); i++)
		if (sin->sin_zero[i])
			return (0);
	return (1);
}

static int
inet_canon(struct sockaddr_in *sin)
{
	register int i;

	sin->sin_port = 0;
	for (i = 0; i < sizeof (sin->sin_zero); i++)
		sin->sin_zero[i] = 0;
	return (0);
}

static char *
inet_format(struct sockaddr_in *sin)
{
	return (inet_ntoa(sin->sin_addr));
}
