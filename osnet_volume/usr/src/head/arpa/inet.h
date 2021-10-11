/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _ARPA_INET_H
#define	_ARPA_INET_H

#pragma ident	"@(#)inet.h	1.17	99/03/21 SMI"

#include <sys/feature_tests.h>

#include <netinet/in.h>
#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
#include <sys/byteorder.h>
#endif /* defined(_XPG4_2) && !defined(__EXTENSIONS__) */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * External definitions for
 * functions in inet(3N)
 */
#ifdef __STDC__
extern	int	inet_aton(const char *, struct in_addr *);
extern	int	inet_net_pton(int, const char *, void *, size_t);
extern	int	inet_pton(int, const char *, void *);
extern  const	char *inet_ntop(int, const void *, char *, size_t);
extern in_addr_t inet_addr(const char *);
extern in_addr_t inet_lnaof(struct in_addr);
extern struct in_addr inet_makeaddr(in_addr_t, in_addr_t);
extern in_addr_t inet_netof(struct in_addr);
extern in_addr_t inet_network(const char *);
extern char *inet_ntoa(struct in_addr);
/*
 * Private functions for IPv6 routing header manipulation.
 * Public versions will be made available once the IPv6
 * Advanced Socket API specification has been finalized.
 */
extern size_t		__inet6_rthdr_space(int, int);
extern void		*__inet6_rthdr_init(void *, int, int, int);
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int		__inet6_rthdr_add(void *, const struct in6_addr *);
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
extern int		__inet6_rthdr_reverse(const void *, void *);
extern int		__inet6_rthdr_segments(const void *);
extern struct in6_addr	*__inet6_rthdr_getaddr(void *, int);
#else
unsigned long inet_addr();
char	*inet_ntoa();
struct	in_addr inet_makeaddr();
unsigned long inet_network();
extern unsigned long inet_lnaof();
extern unsigned long inet_netof();
extern int inet_pton();
extern const char *inet_ntop();
extern size_t		__inet6_rthdr_space();
extern void		*__inet6_rthdr_init();
extern int		__inet6_rthdr_add();
extern int		__inet6_rthdr_reverse();
extern int		__inet6_rthdr_segments();
extern struct in6_addr	*__inet6_rthdr_getaddr();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _ARPA_INET_H */
