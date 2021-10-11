/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_INTERFACES_H
#define	_INTERFACES_H

#pragma ident	"@(#)interfaces.h	1.11	99/03/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct interfaces {
	char			nm[IFNAMSIZ];	/* Interface name */
	short			mtu;		/* MTU of interface */
	int			senddesc;	/* network send descriptor */
	int			recvdesc;	/* network receive descriptor */
	int			type;		/* descriptor flags */
	u_int			flags;		/* interface flags */
	struct in_addr		bcast;		/* interface broadcast */
	struct in_addr		mask;		/* interface netmask */
	struct in_addr		addr;		/* interface IP addr */
	PKT_LIST		*pkthead;	/* head of packet list */
	PKT_LIST		*pkttail;	/* tail of packet list */
	OFFLST			*of_head;	/* IF specific OFFERs */
	ENCODE			*ecp;		/* IF specific options */
	u_int			transmit;	/* # of transmitted pkts */
	u_int			received;	/* # of received pkts */
	u_int			duplicate;	/* # of duplicate pkts */
	u_int			dropped;	/* # of dropped pkts */
	u_int			errors;		/* # of protocol errors */
	u_int			processed;	/* # of processed pkts */
	thread_t		if_thread;	/* rcv service thread */
	int			thr_exit;	/* sent when time to exit */
	mutex_t			ifp_mtx;	/* mutex lock on this struct */
	mutex_t			pkt_mtx;	/* lock for PKT_LIST */
	struct interfaces	*next;
} IF;

#define	DHCP_SOCKET		0	/* Plain AF_INET socket */
#define	DHCP_DLPI		1	/* DLPI stream */
#define	MAXIFS			256	/* default max number of interfaces */
#define	DHCP_MON_SYSERRS	30	/* Max allowable interface errors */
#define	DHCP_MON_ERRINTVL	1	/* Time interval for IF errors (secs) */
#define	DHCP_MON_THRESHOLD	2000	/* Max allowable pending pkts per IF */
#define	DHCP_MON_DUMP		300	/* dump # pkts if THRESHOLD reached */

/*
 * Pause interval (mins) if IF error threshold reached.
 */
#define	DHCP_MON_SLEEP		5

extern IF	*if_head;	/* head of monitored interfaces */
extern mutex_t	if_head_mtx;	/* lock to protect interfaces list */
extern char	*interfaces;	/* list of user-requested interfaces. */
extern int	check_interfaces(void);
extern int	open_interfaces(void);
extern int	read_interfaces(int);
extern int	write_interface(IF *, PKT *, int, struct sockaddr_in *);
extern void	close_interfaces(void);
extern void	detach_plp(IF *, PKT_LIST *);
extern PKT_LIST	*refresh_pktlist(IF *, PKT_LIST *);
#ifdef	DEBUG_PKTLIST
extern void	display_pktlist(IF *);
#endif	/* DEBUG_PKTLIST */
extern int	set_arp(IF *, struct in_addr *, u_char *, int, u_char);
extern int	dhcp(IF *, PKT_LIST *);
extern int	bootp(IF *, PKT_LIST *);
extern int	relay_agent(IF *, PKT_LIST *);
extern int	determine_network(IF *, PKT_LIST *, struct in_addr *,
		    struct in_addr *);
extern int	get_netmask(struct in_addr *, struct in_addr **);
extern int	send_reply(IF *, PKT *, int, struct in_addr *);
extern int	check_offers(IF *, struct in_addr *);
extern void	free_offers(IF *);
extern void	disp_if_stats(IF *);
extern int	select_offer(PER_NET_DB *, PKT_LIST *, IF *, PN_REC *);

#ifdef	__cplusplus
}
#endif

#endif	/* _INTERFACES_H */
