/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SOCKET_SPEC_H
#define	_SOCKET_SPEC_H

#pragma ident	"@(#)socket_spec.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern	"C" {
#endif

#if defined(_BIG_ENDIAN)
#undef htonl
#undef htons
#undef ntohl
#undef ntohs

extern uint32_t htonl(uint32_t hostlong);
extern uint16_t htons(uint16_t hostshort);
extern uint32_t ntohl(uint32_t hostlong);
extern uint16_t ntohs(uint16_t hostshort);
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _SOCKET_SPEC_H */
