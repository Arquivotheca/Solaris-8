/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DHCPBOOT_H
#define	_DHCPBOOT_H

#pragma ident	"@(#)dhcpboot.h	1.2	99/03/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DHCP_NO_DATA		(1)		/* no data */
#define	DHCP_TIMEOUT		(60*1000)	/* try for 60 seconds */
#define	DHCP_CLIENT_SZ		(64)		/* max size of client id */
#define	DHCP_CLASS_SZ		(254)		/* max size of class id */
#define	DHCP_ARP_TIMEOUT	(1000)		/* Wait one sec for response */
#define	DHCP_RETRIES		(0xffffffff)	/* Forever */
#define	DHCP_SCRATCH		(128)
#define	DHCP_WAIT		(4)		/* first wait - 4 seconds */

enum DHCPSTATE { INIT, SELECTING, REQUESTING, BOUND, CONFIGURED };

extern int dhcp(struct in_addr *, char *, int, char *, int, char *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCPBOOT_H */
