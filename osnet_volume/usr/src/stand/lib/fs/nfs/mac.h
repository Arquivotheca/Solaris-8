/*
 * Copyright (c) 1990-1999, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * mac.h contains MAC layer independent definttions
 */

#ifndef _MAC_H
#define	_MAC_H

#pragma ident	"@(#)mac.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int	initialized;	/* TRUE if network device initialized */
extern int	arp_index;	/* current arp table index */

struct mac_type {
	int		mac_type;	/* if_types.h */
	int		mac_dev;
	int		mac_mtu;
	caddr_t		mac_buf;	/* MTU sized buffer */
	caddr_t		mac_addr_buf;
	uint32_t	mac_arp_timeout;
	uint32_t	mac_in_timeout;
	uint32_t	mac_in_timeo_incr;
	int		mac_addr_len;
	int		(*mac_arp)(struct in_addr *, void *, uint32_t);
	void		(*mac_rarp)(void);
	int		(*mac_header_len)(void);
	int		(*mac_input)(int);
	int		(*mac_output)(int, struct inetgram *);
};

#define	ARP_TABLE_SIZE		(3)	/* size of ARP table */
#define	HW_ADDR_SIZE		(128)	/* max size of hardware address */
#define	MAC_IN_TIMEOUT		(10)	/* collect IP grams for X mseconds. */
#define	MAC_IN_TIMEO_MULT	(8)	/* Multiplier to arrive at maximum */

/* format of an arp table entry */
struct	arptable {
	struct in_addr	ia;
	u_char		ha[HW_ADDR_SIZE];
	int		hl;
};

extern void	mac_init(char *);	/* initialize MAC layer */
extern void	mac_fini(void);		/* tear down MAC layer */
extern void	mac_socket_init(struct inetboot_socket *);
extern void	mac_set_arp(struct in_addr *, void *, int);
extern int	mac_get_arp(struct in_addr *, void *, int, uint32_t);

extern struct mac_type mac_state;	/* in mac.c */

#ifdef	__cplusplus
}
#endif

#endif /* _MAC_H */
