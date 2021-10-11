/*
 * Copyright (c) 2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * [dep, 17May2000]
 * This was written with no known prior exposure to the existing code.
 */

#ifndef	_BYTEORDER_H
#define	_BYTEORDER_H

#pragma ident	"@(#)byteorder.h	1.1	00/09/14 SMI"

#include <sys/isa_defs.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);

#ifndef	_LITTLE_ENDIAN

#define	htonl(x)	((uint32_t)(x))
#define	htons(x)	((uint16_t)(x))
#define	ntohl(x)	((uint32_t)(x))
#define	ntohs(x)	((uint16_t)(x))

#endif	/* !_LITTLE_ENDIAN */

#ifdef	__cplusplus
}
#endif

#endif	/* _BYTEORDER_H */
