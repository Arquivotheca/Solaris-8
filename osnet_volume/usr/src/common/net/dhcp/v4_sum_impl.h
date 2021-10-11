/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_V4_SUM_IMPL_H
#define	_V4_SUM_IMPL_H

#pragma ident	"@(#)v4_sum_impl.h	1.1	99/02/22 SMI"

/*
 * Common definitions used for various non ip/udp module checksum
 * implementations.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	TRUE
#define	TRUE	(1)
#endif /* TRUE */
#ifndef	FALSE
#define	FALSE	(0)
#endif	/* FALSE */

#define	BIT_WRAP		(uint_t)0x10000	/* checksum wrap */

extern uint16_t ipv4cksum(uint16_t *, uint16_t);
extern uint16_t udp_chksum(struct udphdr *, const struct in_addr *,
    const struct in_addr *, const uint8_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _V4_SUM_IMPL_H */
