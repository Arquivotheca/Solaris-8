/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ipv4_sum.c	1.2	99/08/18 SMI"

/* LINTLIBRARY */

#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "v4_sum_impl.h"

/*
 * Compute one's complement checksum for IP packet headers.
 */
uint16_t
ipv4cksum(uint16_t *cp, uint16_t count)
{
	uint_t		sum = 0;
	uint_t		oneword = BIT_WRAP;

	if (count == 0)
		return (0);

	count >>= 1;
	while (count--) {
		sum += (uint_t)*cp++;
		if (sum >= oneword) {		/* Wrap carries into low bit */
			sum -= oneword;
			sum++;
		}
	}
	return ((uint16_t)~sum);
}
