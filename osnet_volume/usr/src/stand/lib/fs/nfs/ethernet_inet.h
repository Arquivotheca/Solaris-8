/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 *
 * Ethernet implementation-specific definitions
 */

#ifndef _ETHERNET_INET_H
#define	_ETHERNET_INET_H

#pragma ident	"@(#)ethernet_inet.h	1.2	99/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ETHERSIZE	(ETHERMTU + 50)	/* Usually a little bigger than 1500 */
#define	ETHER_ARP_TIMEOUT	(300000)	/* in milliseconds */
#define	ETHER_IN_TIMEOUT	(5)	/* msecond wait for IP frames */
#define	ETHER_MAX_FRAMES	(200)	/* Maximum of consecutive frames */
#define	ETHER_INPUT_ATTEMPTS	(8)	/* Number of consecutive attempts */
#define	ETHER_WAITCNT		(2)	/* Activity interval */

extern int ether_header_len(void);
extern int ether_arp(struct in_addr *, void *, uint32_t);
extern void ether_revarp(void);
extern int ether_input(int);
extern int ether_output(int, struct inetgram *);

#ifdef	__cplusplus
}
#endif

#endif /* _ETHERNET_INET_H */
