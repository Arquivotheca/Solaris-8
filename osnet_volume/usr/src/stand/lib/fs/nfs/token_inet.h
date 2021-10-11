/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Token ring implementation-specific definitions
 */

#ifndef _TOKEN_INET_H
#define	_TOKEN_INET_H

#pragma ident	"@(#)token_inet.h	1.2	99/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	TOKENSIZE		(17800)	/* Default Token Ring MTU size */
#define	TOKEN_ARP_TIMEOUT	(300000)	/* in milliseconds */
#define	TOKEN_IN_TIMEOUT	(8)	/* millisecond wait for IP frames */

#ifdef	__cplusplus
}
#endif

#endif /* _TOKEN_INET_H */
