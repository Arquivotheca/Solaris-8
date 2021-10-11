/*
 * Copyright (c) 1999, Sun Microsystems, Inc.
 *
 * ATM implementation-specific definitions
 */

#ifndef _ATM_INET_H
#define	_ATM_INET_H

#pragma ident	"@(#)atm_inet.h	1.2	99/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ATMSIZE			(4500)	/* Default ATM MTU size */
#define	ATM_ARP_TIMEOUT		(300000)	/* in milliseconds */
#define	ATM_IN_TIMEOUT		(4)	/* milliseconds wait for IP input */

#ifdef	__cplusplus
}
#endif

#endif /* _ATM_INET_H */
