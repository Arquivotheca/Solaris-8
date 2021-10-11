/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SLP_NET_UTILS_H
#define	_SLP_NET_UTILS_H

#pragma ident	"@(#)slp_net_utils.h	1.1	99/04/02 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/* address manipulation */
extern SLPError slp_broadcast_addrs(slp_handle_impl_t *, struct in_addr *,
				int, struct sockaddr_in **, int *);
extern SLPBoolean slp_on_subnet(slp_handle_impl_t *, struct in_addr);
extern SLPBoolean slp_on_localhost(slp_handle_impl_t *, struct in_addr);
extern void slp_free_ifinfo(void *);
extern SLPError slp_surl2sin(SLPSrvURL *, struct sockaddr_in *);
extern char *slp_gethostbyaddr(const char *, int);

#define	SLP_NETDB_BUFSZ	NSS_BUFLEN_HOSTS
#define	INET6_ADDRSTRLEN	46	/* max len of IPv6 addr in ascii */

/* @@@ temporary backport hack */
#ifdef	OSVERS6
typedef int socklen_t;
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SLP_NET_UTILS_H */
