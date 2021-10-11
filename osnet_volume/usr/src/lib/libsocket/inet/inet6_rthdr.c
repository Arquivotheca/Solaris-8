/*
 * Copyright (c) 1998 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)inet6_rthdr.c	1.1	99/03/21 SMI"

/*
 * XXX Implements preliminary functions for inet6_rthdr*.
 * Current set of functions all have double leading underscores since
 * they are not blessed by an internet-draft or an RFC.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <errno.h>


/*
 * Return amount of space needed to hold N segments for the specified
 * routing type. Does NOT include space for cmsghdr.
 */
size_t
__inet6_rthdr_space(int type, int segments)
{
	if (type != IPV6_RTHDR_TYPE_0 || segments < 0)
		return (0);

	return (sizeof (struct ip6_rthdr0) +
	    (segments - 1) * sizeof (struct in6_addr));
}

/*
 * Initializes rthdr structure. Verifies the segments against the length of
 * the buffer.
 * Note that a routing header can only hold 127 segments since the length field
 * in the header is just a byte.
 */
void *
__inet6_rthdr_init(void *bp, int bp_len, int type, int segments)
{
	struct ip6_rthdr0 *rthdr;

	if (type != IPV6_RTHDR_TYPE_0 || segments < 0 || segments > 127)
		return (NULL);

	if (bp_len < sizeof (struct ip6_rthdr0) +
	    (segments - 1) * sizeof (struct in6_addr))
		return (NULL);

	rthdr = (struct ip6_rthdr0 *)bp;
	rthdr->ip6r0_nxt = 0;
	rthdr->ip6r0_len = (segments * 2);
	rthdr->ip6r0_type = type;
	rthdr->ip6r0_segleft = 0;	/* Incremented by rthdr_add */
	*(uint32_t *)&rthdr->ip6r0_reserved = 0;
	return (bp);
}

/*
 * Add one more address to the routing header. Fails when there is no more
 * room.
 */
int
__inet6_rthdr_add(void *bp, const struct in6_addr *addr)
{
	struct ip6_rthdr0 *rthdr;

	rthdr = (struct ip6_rthdr0 *)bp;
	if ((rthdr->ip6r0_segleft + 1) * 2 > rthdr->ip6r0_len) {
		/* Not room for one more */
		return (-1);
	}
	rthdr->ip6r0_addr[rthdr->ip6r0_segleft++] = *addr;
	return (0);
}

/*
 * Reverse a source route. Both arguments can point to the same buffer.
 */
int
__inet6_rthdr_reverse(const void *in, void *out)
{
	struct ip6_rthdr0 *rtin, *rtout;
	int i, segments;
	struct in6_addr tmp;

	rtin = (struct ip6_rthdr0 *)in;
	rtout = (struct ip6_rthdr0 *)out;

	if (rtout->ip6r0_len != rtin->ip6r0_len)
		return (-1);

	segments = rtin->ip6r0_len / 2;
	for (i = 0; i < (segments + 1)/2; i++) {
		tmp = rtin->ip6r0_addr[i];
		rtout->ip6r0_addr[i] = rtin->ip6r0_addr[segments - 1 - i];
		rtout->ip6r0_addr[segments - 1 - i] = tmp;
	}
	rtout->ip6r0_segleft = segments;
	return (0);
}

/*
 * Return the number of segments in the routing header.
 */
int
__inet6_rthdr_segments(const void *bp)
{
	struct ip6_rthdr0 *rthdr;

	rthdr = (struct ip6_rthdr0 *)bp;
	return (rthdr->ip6r0_len / 2);
}

/*
 * Return a pointer to an element in the source route
 * Unlike the current RFC 2292 this uses the C convention for
 * index [0, size-1].
 */
struct in6_addr *
__inet6_rthdr_getaddr(void *bp, int index)
{
	struct ip6_rthdr0 *rthdr;

	rthdr = (struct ip6_rthdr0 *)bp;
	if (index >= rthdr->ip6r0_len/2 || index < 0)
		return (NULL);

	return (&rthdr->ip6r0_addr[index]);
}
