/*
 * Copyright (c) 1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * ICMPv4 implementation-specific definitions
 */

#ifndef _ICMP4_H
#define	_ICMP4_H

#pragma ident	"@(#)icmp4.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern void icmp4(struct inetgram *, struct ip *, uint16_t, struct in_addr);

#ifdef	__cplusplus
}
#endif

#endif /* _ICMP4_H */
