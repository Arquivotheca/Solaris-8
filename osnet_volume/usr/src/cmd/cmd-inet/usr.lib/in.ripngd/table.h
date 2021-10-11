/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)table.h 1.2	99/07/29 SMI"	/* SVr4.0 1.1   */

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
 * 	(c) 1986-1989,1991,1992,1997-1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                 All rights reserved.
 *
 */

/*
 * Routing table management daemon.
 */

/*
 * Routing table structure; differs a bit from kernel tables.
 */
struct rthash {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
};

struct rt_entry {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
	uint_t	rt_hash;		/* for net or host */
	struct	in6_addr rt_dst;	/* match value */
	struct	in6_addr rt_router;	/* who to forward to */
	int	rt_prefix_length;	/* bits in prefix */
	struct	interface *rt_ifp;	/* interface to take */
	uint_t	rt_flags;		/* kernel flags */
	uint_t	rt_state;		/* see below */
	int	rt_timer;		/* for invalidation */
	int	rt_metric;		/* cost of route including the if */
	int	rt_tag;			/* route tag attribute */
};

#define	ROUTEHASHSIZ	32		/* must be a power of 2 */
#define	ROUTEHASHMASK	(ROUTEHASHSIZ - 1)

/*
 * "State" of routing table entry.
 */
#define	RTS_CHANGED	0x1		/* route has been altered recently */
#define	RTS_INTERFACE	0x2		/* route is for network interface */
#define	RTS_PRIVATE	0x4		/* route is private, do not advertise */

/*
 * XXX This is defined in <inet/ip.h> (but should be defined in <netinet/ip6.h>
 * for completeness).
 */
#define	IPV6_ABITS	128		/* Number of bits in an IPv6 address */

extern struct	rthash	*net_hashes[IPV6_ABITS + 1];

extern void	rtadd(struct in6_addr *, struct in6_addr *, int, int, int,
    boolean_t, struct interface *);
extern void	rtchange(struct rt_entry *, struct in6_addr *, short,
    struct interface *);
extern void	rtchangeall(void);
extern void	rtcreate_prefix(struct in6_addr *, struct in6_addr *, int);
extern void	rtdelete(struct rt_entry *);
extern void	rtdown(struct rt_entry *);
extern void	rtdump(void);
extern struct	rt_entry *rtlookup(struct in6_addr *, int);
extern void	rtpurgeif(struct interface *);
