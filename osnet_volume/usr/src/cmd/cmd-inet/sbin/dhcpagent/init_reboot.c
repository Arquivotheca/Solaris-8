/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * INIT_REBOOT state of the DHCP client state machine.
 */

#pragma ident	"@(#)init_reboot.c	1.2	99/08/23 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netinet/dhcp.h>
#include <dhcpmsg.h>

#include "agent.h"
#include "packet.h"
#include "states.h"
#include "util.h"
#include "dlpi_io.h"
#include "interface.h"

static stop_func_t	stop_init_reboot;

/*
 * dhcp_init_reboot(): attempts to reuse a cached configuration on an interface
 *
 *   input: struct ifslist *: the interface to reuse the configuration on
 *  output: void
 */

void
dhcp_init_reboot(struct ifslist *ifsp)
{
	dhcp_pkt_t		*dpkt;

	dhcpmsg(MSG_VERBOSE,  "%s has cached configuration - entering "
	    "INIT_REBOOT", ifsp->if_name);

	ifsp->if_state = INIT_REBOOT;

	if (register_acknak(ifsp) == 0) {

		ifsp->if_state   = INIT;
		ifsp->if_dflags |= DHCP_IF_FAILED;
		ipc_action_finish(ifsp, DHCP_IPC_E_MEMORY);
		async_finish(ifsp);

		dhcpmsg(MSG_ERROR, "dhcp_init_reboot: cannot register to "
		    "collect ACK/NAK packets, reverting to INIT on %s",
		    ifsp->if_name);
		return;
	}

	/*
	 * assemble DHCPREQUEST message
	 */

	dpkt = init_pkt(ifsp, REQUEST);
	add_pkt_opt32(dpkt, CD_REQUESTED_IP_ADDR,
	    ifsp->if_ack->pkt->yiaddr.s_addr);

	add_pkt_opt32(dpkt, CD_LEASE_TIME, htonl(DHCP_PERM));
	add_pkt_opt16(dpkt, CD_MAX_DHCP_SIZE, htons(ifsp->if_max));

	add_pkt_opt(dpkt, CD_CLASS_ID, class_id, class_id_len);
	add_pkt_opt(dpkt, CD_REQUEST_LIST, ifsp->if_prl, ifsp->if_prllen);
	add_pkt_opt(dpkt, CD_END, NULL, 0);

	(void) send_pkt(ifsp, dpkt, htonl(INADDR_BROADCAST), stop_init_reboot);
}

/*
 * stop_init_reboot(): decides when to stop retransmitting REQUESTs
 *
 *   input: struct ifslist *: the interface REQUESTs are being sent on
 *	    unsigned int: the number of REQUESTs sent so far
 *  output: boolean_t: B_TRUE if retransmissions should stop
 */

static boolean_t
stop_init_reboot(struct ifslist *ifsp, unsigned int n_requests)
{
	if (n_requests >= DHCP_MAX_REQUESTS) {

		(void) unregister_acknak(ifsp);

		dhcpmsg(MSG_INFO, "no ACK/NAK to INIT_REBOOT REQUEST, "
		    "using remainder of existing lease on %s", ifsp->if_name);

		/*
		 * we already stuck our old ack in ifsp->if_ack and
		 * relativized the packet times, so we can just
		 * pretend that the server sent it to us and move to
		 * bound.  if that fails, fall back to selecting.
		 */

		if (dhcp_bound(ifsp, NULL) == 0)
			dhcp_selecting(ifsp);
		else {
			ipc_action_finish(ifsp, DHCP_IPC_SUCCESS);
			async_finish(ifsp);
		}

		return (B_TRUE);
	}

	return (B_FALSE);
}
