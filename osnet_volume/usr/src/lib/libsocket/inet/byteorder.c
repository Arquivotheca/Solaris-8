/*
 * Copyright (c) 2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * [dep, 17May2000]
 * This was written with no known prior exposure to the existing code.
 */

#pragma ident	"@(#)byteorder.c	1.8	00/09/14 SMI"

#include <sys/types.h>

#ifndef	_LITTLE_ENDIAN

uint32_t
htonl(uint32_t hostlong)
{
	return (hostlong);
}

uint16_t
htons(uint16_t hostshort)
{
	return (hostshort);
}

uint32_t
ntohl(uint32_t netlong)
{
	return (netlong);
}

uint16_t
ntohs(uint16_t netshort)
{
	return (netshort);
}

#else

uint32_t
htonl(uint32_t hostlong)
{
	return ((hostlong << 24) |
	    ((hostlong & 0xff00) << 8) |
	    ((hostlong >> 8) & 0xff00) |
	    (hostlong >> 24));
}

uint16_t
htons(uint16_t hostshort)
{
	return ((hostshort << 8) | (hostshort >> 8));
}

uint32_t
ntohl(uint32_t netlong)
{
	return ((netlong << 24) |
	    ((netlong & 0xff00) << 8) |
	    ((netlong >> 8) & 0xff00) |
	    (netlong >> 24));
}

uint16_t
ntohs(uint16_t netshort)
{
	return ((netshort << 8) | (netshort >> 8));
}

#endif

