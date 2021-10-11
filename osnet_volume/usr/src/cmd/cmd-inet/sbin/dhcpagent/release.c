/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * DECLINE/RELEASE configuration functionality for the DHCP client.
 */

#pragma ident	"@(#)release.c	1.2	99/09/27 SMI"

#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/dhcp.h>
#include <dhcpmsg.h>
#include <dhcp_hostconf.h>
#include <unistd.h>

#include "packet.h"
#include "interface.h"
#include "states.h"

/*
 * send_decline(): sends a DECLINE message (broadcasted)
 *
 *   input: struct ifslist *: the interface to send the DECLINE on
 *	    char *: an optional text explanation to send with the message
 *	    struct in_addr *: the IP address being declined
 *  output: void
 */

void
send_decline(struct ifslist *ifsp, char *msg, struct in_addr *declined_ip)
{
	dhcp_pkt_t	*dpkt;

	dpkt = init_pkt(ifsp, DECLINE);
	add_pkt_opt32(dpkt, CD_SERVER_ID, ifsp->if_server.s_addr);

	if (msg != NULL)
		add_pkt_opt(dpkt, CD_MESSAGE, msg, strlen(msg) + 1);

	add_pkt_opt32(dpkt, CD_REQUESTED_IP_ADDR, declined_ip->s_addr);
	add_pkt_opt(dpkt, CD_END, NULL, 0);

	(void) send_pkt(ifsp, dpkt, htonl(INADDR_BROADCAST), NULL);
}

/*
 * dhcp_release(): sends a RELEASE message to a DHCP server and removes
 *		   the interface from DHCP control
 *
 *   input: struct ifslist *: the interface to send the RELEASE on and remove
 *	    char *: an optional text explanation to send with the message
 *  output: int: 1 on success, 0 on failure
 */

int
dhcp_release(struct ifslist *ifsp, char *msg)
{
	dhcp_pkt_t	*dpkt;

	if (ifsp->if_dflags & DHCP_IF_BOOTP)
		return (0);

	if (ifsp->if_state != BOUND && ifsp->if_state != RENEWING &&
	    ifsp->if_state != REBINDING)
		return (0);

	dhcpmsg(MSG_INFO, "releasing interface %s", ifsp->if_name);

	dpkt = init_pkt(ifsp, RELEASE);
	dpkt->pkt->ciaddr.s_addr = ifsp->if_addr.s_addr;

	if (msg != NULL)
		add_pkt_opt(dpkt, CD_MESSAGE, msg, strlen(msg) + 1);

	add_pkt_opt32(dpkt, CD_SERVER_ID, ifsp->if_server.s_addr);
	add_pkt_opt(dpkt, CD_END, NULL, 0);

	(void) send_pkt(ifsp, dpkt, ifsp->if_server.s_addr, NULL);

	/*
	 * XXX this totally sucks, but since udp is best-effort,
	 * without this delay, there's a good chance that the packet
	 * that we just enqueued for sending will get pitched
	 * when we canonize the interface below.
	 */

	(void) usleep(500);
	(void) canonize_ifs(ifsp);

	remove_ifs(ifsp);
	return (1);
}

/*
 * dhcp_drop(): drops the interface from DHCP control
 *
 *   input: struct ifslist *: the interface to drop
 *  output: void
 */

void
dhcp_drop(struct ifslist *ifsp)
{
	dhcpmsg(MSG_INFO, "dropping interface %s", ifsp->if_name);

	if (ifsp->if_state == BOUND || ifsp->if_state == RENEWING ||
	    ifsp->if_state == REBINDING) {

		if ((ifsp->if_dflags & DHCP_IF_BOOTP) == 0) {
			if (write_hostconf(ifsp->if_name, ifsp->if_ack,
			    monosec_to_time(ifsp->if_curstart_monosec)) == -1)
				dhcpmsg(MSG_ERR, "cannot write %s (reboot will "
				    "not use cached configuration)",
				    ifname_to_hostconf(ifsp->if_name));
		}
		(void) canonize_ifs(ifsp);
	}
	remove_ifs(ifsp);
}
