/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)interface.h	1.11	99/07/29 SMI"	/* SVr4.0 1.1	*/

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
 * 	(c) 1986,1987,1988,1989,1991,1992  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing table management daemon.
 */

/*
 * An ``interface'' is similar to an ifnet structure,
 * except it doesn't contain q'ing info, and it also
 * handles ``logical'' interfaces (remote gateways
 * that we want to keep polling even if they go down).
 * The list of interfaces which we maintain is used
 * in supplying the gratuitous routing table updates.
 */
struct interface {
	struct	interface *int_next;
	struct	sockaddr int_addr;		/* address on this host */
	union {
		struct	sockaddr intu_broadaddr;
		struct	sockaddr intu_dstaddr;
	} int_intu;
#define	int_broadaddr	int_intu.intu_broadaddr	/* broadcast address */
#define	int_dstaddr	int_intu.intu_dstaddr	/* other end of p-to-p link */
	int	int_metric;			/* init's routing entry */
	uint_t	int_flags;			/* see below */
	/* START INTERNET SPECIFIC */
	ulong_t	int_net;			/* network # */
	ulong_t	int_netmask;			/* net mask for addr */
	ulong_t	int_subnet;			/* subnet # */
	ulong_t	int_subnetmask;			/* subnet mask for addr */
	/* END INTERNET SPECIFIC */
	struct	ifdebug int_input, int_output;	/* packet tracing stuff */
	int	int_ipackets;			/* input packets received */
	int	int_opackets;			/* output packets sent */
	char	*int_name;			/* from kernel if structure */
	ushort_t	int_transitions;		/* times gone up-down */
};

#define	IFMETRIC(ifp)	((ifp) ? (ifp)->int_metric : 1)

/*
 * 0x1 to 0x10 are reused from the kernel's ifnet definitions,
 * the others agree with the RTS_ flags defined elsewhere.
 *
 * XXX Ideally we shouldn't have to redefine these here. This
 *     sharing of kernel and routed private flags needs fixing
 */
#define	IFF_UP		0x00000001	/* interface is up */
#define	IFF_BROADCAST	0x00000002	/* broadcast address valid */
#define	IFF_DEBUG	0x00000004	/* turn on debugging */
#define	IFF_LOOPBACK	0x00000008	/* software loopback net */
#define	IFF_POINTOPOINT	0x00000010	/* interface is point-to-point link */

#define	IFF_NORIPOUT	0x00000100	/* Do not send RIP packets on if */
#define	IFF_NORIPIN	0x00000200	/* Do not accept RIP packets on if */
#define	IFF_SUBNET	0x00001000	/* interface on subnetted network */
#define	IFF_PASSIVE	0x00002000	/* can't tell if up/down */
#define	IFF_INTERFACE	0x00004000	/* hardware interface */
#define	IFF_PRIVATE	0x00008000	/* do not propagate route */
#define	IFF_REMOTE	0x00010000	/* interface isn't on this machine */
#define	IFF_NORTEXCH	0x00800000	/* Do not exchange routing info */
#define	IFF_FROMKERNEL	0x0080801F	/* only kernel flags allowed */
#define	IFF_SAVED	(IFF_NORIPIN | IFF_NORIPOUT)
					/* saved across up/down */

extern struct interface *if_ifwithaddr(struct sockaddr *addr);
extern struct interface *if_ifwithdst(struct sockaddr *addr);
extern struct interface *if_ifwithdstaddr(struct sockaddr *addr);
extern struct interface *if_ifwithname(char *name);
extern struct interface *if_iflookup(struct sockaddr *addr);
extern void ifinit(void);
extern void if_purge(struct interface *pifp);
extern void if_add(char *name, int flags);
