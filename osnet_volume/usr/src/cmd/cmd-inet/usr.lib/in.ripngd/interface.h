/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)interface.h	1.2	99/07/29 SMI"

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
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986-1989,1991,1992,1997-1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

struct interface {
	struct	interface *int_next;
	struct	in6_addr int_addr;		/* address on this if */
	struct	in6_addr int_dstaddr;		/* other end of p-to-p link */
	int	int_metric;			/* init's routing entry */
	uint_t	int_flags;			/* see below */
	int	int_prefix_length;		/* prefix length on this if */
	char	*int_name;			/* from kernel if structure */
	char	*int_ifbase;			/* name of physical interface */
	int	int_sock;			/* socket on if to send/recv */
	int	int_ifindex;			/* interface index */
	uint_t	int_mtu;			/* maximum transmission unit */
	struct  ifdebug int_input, int_output;  /* packet tracing stuff */
	int	int_ipackets;			/* input packets received */
	int	int_opackets;			/* output packets sent */
	ushort_t int_transitions;		/* times gone up-down */
};

#define	RIP6_IFF_UP		0x1		/* interface is up */
#define	RIP6_IFF_POINTOPOINT	0x2		/* interface is p-to-p link */
#define	RIP6_IFF_MARKED		0x4		/* to determine removed ifs */
#define	RIP6_IFF_NORTEXCH	0x8		/* don't exchange route info */
#define	RIP6_IFF_PRIVATE	0x10		/* interface is private */

#define	IFMETRIC(ifp)	((ifp != NULL) ? (ifp)->int_metric : 1)

extern void	if_dump(void);
extern struct	interface *if_ifwithname(char *);
extern void	if_purge(struct interface *);
