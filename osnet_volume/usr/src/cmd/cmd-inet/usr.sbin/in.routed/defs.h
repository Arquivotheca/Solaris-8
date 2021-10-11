/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)defs.h	1.11	98/06/16 SMI"	/* SVr4.0 1.2	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1991,1992,1998  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


/*
 * Internal data structure definitions for
 * user routing process.  Based on Xerox NS
 * protocol specs with mods relevant to more
 * general addressing scheme.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stream.h>
#include <sys/time.h>

#include <net/route.h>
#include <netinet/in.h>
#include <protocols/routed.h>

#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>

#ifndef	EXTERN
#define	EXTERN	extern
#endif

#include "trace.h"
#include "interface.h"
#include "table.h"
#include "af.h"


/* Should be in <protocols/routed.h> */
#define	MIN_WAITTIME		2	/* min. interval to broadcast changes */
#define	MAX_WAITTIME		5	/* max. time to delay changes */

/*
 * When we find any interfaces marked down we rescan the
 * kernel every CHECK_INTERVAL seconds to see if they've
 * come up.
 */
#define	CHECK_INTERVAL	(1*60)

#define	LOOPBACKNET	0x7F000000
#define	equal(a1, a2) \
	(bcmp((caddr_t)(a1), (caddr_t)(a2), sizeof (struct sockaddr)) == 0)
#define	min(a, b)	((a) > (b) ? (b) : (a))

/*
 * The maximum receive buffer size is controlled via Solaris' NDD udp_max_buf
 * tunable.
 */
#define	RCVBUFSIZ	(65536)

EXTERN struct sockaddr_in addr;	/* address of daemon's socket */

EXTERN int	s;		/* source and sink of all data */
EXTERN int	supplier;	/* process should supply updates */
EXTERN int	maysupply;	/* process must, may, or may not supply */
EXTERN int	install;	/* if 1 call kernel */
EXTERN int	lookforinterfaces; /* if 1 probe kernel for new up interfaces */
EXTERN int	save_space;	/* if 1 and not supplier install only default */
				/* routes to the routers. */
EXTERN struct timeval now;	/* current idea of time */
EXTERN struct timeval lastbcast; /* last time all/changes broadcast */
EXTERN struct timeval lastfullupdate;	/* last time full table broadcast */
EXTERN struct timeval nextbcast; /* time to wait before changes broadcast */
EXTERN int	needupdate;	/* true if we need update at nextbcast */

EXTERN char	packet[MAXPACKETSIZE+1];
EXTERN struct	rip *msg;

EXTERN char	**argv0;
EXTERN struct interface *ifnet;

extern char *inet_ntoa(const struct in_addr in);
extern int sigsetmask(int mask);
extern int sigblock(int mask);
extern ulong_t inet_netmask(ulong_t addr);
extern ulong_t inet_network(const char *cp);
extern struct in_addr inet_makeaddr(ulong_t net, ulong_t host);
extern ulong_t inet_addr(const char *cp);
extern int inet_rtflags(struct sockaddr_in *sin);
extern int inet_sendsubnet(struct rt_entry *rt, struct sockaddr_in *dst);

extern void rip_input(struct sockaddr *from, int size);
extern void toall(void (*f)(), int rtstate, struct interface *skipif);
extern void sendpacket(struct sockaddr *dst, int flags, struct interface *ifp,
    int rtstate);
extern void supply(struct sockaddr *dst, int flags, struct interface *ifp,
    int rtstate);
extern void gwkludge(void);
extern void addrouteforif(struct interface *ifp);
extern void timer(void);
extern void hup(void);
extern void dynamic_update(struct interface *ifp);
extern void timevaladd(struct timeval *t1, struct timeval *t2);
extern void timevalsub(struct timeval *t1, struct timeval *t2);

#ifndef LIBNSL_BUG_FIXED
#define	inet_netof	routed_inet_netof
extern int routed_inet_netof(struct in_addr in);
#endif /* LIBNSL_BUG_FIXED */

#ifndef	sigmask
#define	sigmask(m)	(1 << ((m)-1))
#endif
