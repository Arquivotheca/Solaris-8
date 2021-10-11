/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	PACKET_H
#define	PACKET_H

#pragma ident	"@(#)packet.h	1.2	99/08/18 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>		/* MIN, MAX, ... */
#include <netinet/in.h>
#include <netinet/dhcp.h>

#include "agent.h"

/*
 * packet.[ch] contain routines for manipulating, setting, and
 * transmitting DHCP/BOOTP packets.  see packet.c for descriptions on
 * how to use the exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

struct ifslist;				/* forward declaration */

/*
 * data type for recv_pkt().  needed because we may want to wait for
 * several kinds of packets at once, and the existing enumeration of
 * DHCP packet types does not provide a way to do that easily.  here,
 * we light a different bit in the enumeration for each type of packet
 * we want to receive.
 */

typedef enum {

	DHCP_PUNTYPED	= 0x001,	/* untyped (BOOTP) message */
	DHCP_PDISCOVER	= 0x002,
	DHCP_POFFER 	= 0x004,
	DHCP_PREQUEST	= 0x008,
	DHCP_PDECLINE	= 0x010,
	DHCP_PACK	= 0x020,
	DHCP_PNAK	= 0x040,
	DHCP_PRELEASE	= 0x080,
	DHCP_PINFORM	= 0x100

} dhcp_message_type_t;

/*
 * a dhcp_pkt_t is (right now) what is used by the packet manipulation
 * functions.  while the structure is not strictly necessary, it allows
 * a better separation of functionality since metadata about the packet
 * (such as its current length) is stored along with the packet.
 */

typedef struct dhcp_pkt {

	PKT		*pkt;		/* the real underlying packet */
	unsigned int	pkt_max_len; 	/* its maximum length */
	unsigned int	pkt_cur_len;	/* its current length */

} dhcp_pkt_t;

/*
 * a `stop_func_t' is used by parts of dhcpagent that use the
 * retransmission capability of send_pkt().  this makes it so the
 * callers of send_pkt() decide when to stop retransmitting, which
 * makes more sense than hardcoding their instance-specific cases into
 * packet.c
 */

typedef boolean_t stop_func_t(struct ifslist *, unsigned int);

dhcp_pkt_t	*init_pkt(struct ifslist *, uchar_t);
void		add_pkt_opt(dhcp_pkt_t *, uchar_t, void *, uchar_t);
void		add_pkt_opt16(dhcp_pkt_t *, uchar_t, uint16_t);
void		add_pkt_opt32(dhcp_pkt_t *, uchar_t, uint32_t);
void		free_pkt_list(PKT_LIST **);
void		remove_from_pkt_list(PKT_LIST **, PKT_LIST *);
void		stop_pkt_retransmission(struct ifslist *);
int		recv_pkt(struct ifslist *, int, dhcp_message_type_t, boolean_t);
int		send_pkt(struct ifslist *, dhcp_pkt_t *, in_addr_t,
		    stop_func_t *);
void		get_pkt_times(PKT_LIST *, uint32_t *, uint32_t *, uint32_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* PACKET_H */
