/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 *
 * FDDI implementation-specific definitions
 */

#ifndef _FDDI_INET_H
#define	_FDDI_INET_H

#pragma ident	"@(#)fddi_inet.h	1.2	99/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	FDDISIZE		(4500)	/* Default FDDI MTU size */
#define	FDDI_ARP_TIMEOUT	(300000)	/* in milliseconds */
#define	FDDI_IN_TIMEOUT		(4)	/* milliseconds wait for IP input */

#ifdef	__cplusplus
}
#endif

#endif /* _FDDI_INET_H */
