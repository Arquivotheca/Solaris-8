/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * REQUESTING state of the client state machine.
 */

#pragma ident	"@(#)request.c	1.1	99/04/09 SMI"

#include <sys/types.h>
#include <sys/stropts.h>	/* FLUSHR/FLUSHW */
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <dhcp_hostconf.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dhcpmsg.h>

#include "states.h"
#include "util.h"
#include "packet.h"
#include "interface.h"
#include "agent.h"
#include "defaults.h"

static PKT_LIST		*select_best(PKT_LIST **);
static void		restart_dhcp(struct ifslist *);
static stop_func_t	stop_requesting;

/*
 * dhcp_requesting(): checks if OFFER packets to come in from DHCP servers.
 *		      if so, chooses the best one, sends a REQUEST to the
 *		      server and registers an event handler to receive
 *		      the ACK/NAK
 *
 *   input: tq_t *: unused
 *	    void *: the interface receiving OFFER packets
 *  output: void
 */

/* ARGSUSED */
void
dhcp_requesting(tq_t *tqp, void *arg)
{
	struct ifslist		*ifsp = (struct ifslist *)arg;
	dhcp_pkt_t		*dpkt;
	PKT_LIST		*offer;
	lease_t			lease;

	/*
	 * if the interface is going away, it's our job to unregister
	 * the OFFER handler, since it may never receive a packet which
	 * would allow it to unregister itself.
	 */

	if (check_ifs(ifsp) == 0) {
		if (eh_unregister_event(eh, ifsp->if_offer_id, NULL) != 0)
			(void) release_ifs(ifsp);
		(void) release_ifs(ifsp);
		return;
	}

	/*
	 * select the best OFFER; all others pitched.
	 */

	offer = select_best(&ifsp->if_recv_pkt_list);
	if (offer == NULL) {

		dhcpmsg(MSG_VERBOSE, "no OFFERs on %s, waiting...",
		    ifsp->if_name);

		/*
		 * no acceptable OFFERs have come in.  reschedule
		 * ourselves for callback.
		 */

		if (tq_schedule_timer(tq, df_get_int(ifsp->if_name,
		    DF_OFFER_WAIT), dhcp_requesting, ifsp) == -1) {

			/*
			 * ugh.  the best we can do at this point is
			 * revert back to INIT and wait for a user to
			 * restart us.
			 */

			ifsp->if_state	 = INIT;
			ifsp->if_dflags |= DHCP_IF_FAILED;

			stop_pkt_retransmission(ifsp);
			ipc_action_finish(ifsp, DHCP_IPC_E_MEMORY);
			async_finish(ifsp);

			dhcpmsg(MSG_WARNING, "dhcp_requesting: cannot "
			    "reschedule callback, reverting to INIT state on "
			    "%s", ifsp->if_name);
		} else
			hold_ifs(ifsp);

		return;
	}

	stop_pkt_retransmission(ifsp);

	/*
	 * stop collecting packets.  check to see whether we got an
	 * OFFER or a BOOTP packet.  if we got a BOOTP packet, go to
	 * the BOUND state now.
	 */

	if (eh_unregister_event(eh, ifsp->if_offer_id, NULL) != 0)
		(void) release_ifs(ifsp);

	if (offer->opts[CD_DHCP_TYPE] == NULL) {

		ifsp->if_state = REQUESTING;

		if (dhcp_bound(ifsp, offer) == 0) {
			dhcpmsg(MSG_WARNING, "dhcp_requesting: dhcp_bound "
			    "failed for %s", ifsp->if_name);
			restart_dhcp(ifsp);
			return;
		}

		ipc_action_finish(ifsp, DHCP_IPC_SUCCESS);
		async_finish(ifsp);
		return;
	}

	/*
	 * if we got a message from the server, display it.
	 */

	if (offer->opts[CD_MESSAGE] != NULL)
		print_server_msg(ifsp, offer->opts[CD_MESSAGE]);

	/*
	 * assemble a DHCPREQUEST, with the ciaddr field set to 0,
	 * since we got here from the INIT state.
	 */

	dpkt = init_pkt(ifsp, REQUEST);

	/*
	 * grab the lease out of the OFFER; we know it's valid since
	 * select_best() already checked.
	 */

	(void) memcpy(&lease, offer->opts[CD_LEASE_TIME]->value,
	    sizeof (lease_t));

	add_pkt_opt32(dpkt, CD_LEASE_TIME, lease);
	add_pkt_opt16(dpkt, CD_MAX_DHCP_SIZE, htons(ifsp->if_max));
	add_pkt_opt32(dpkt, CD_REQUESTED_IP_ADDR, offer->pkt->yiaddr.s_addr);
	add_pkt_opt(dpkt, CD_SERVER_ID, offer->opts[CD_SERVER_ID]->value,
	    offer->opts[CD_SERVER_ID]->len);

	add_pkt_opt(dpkt, CD_CLASS_ID, class_id, class_id_len);
	add_pkt_opt(dpkt, CD_REQUEST_LIST, ifsp->if_prl, ifsp->if_prllen);
	add_pkt_opt(dpkt, CD_END, NULL, 0);

	/* all done with the offer */
	free_pkt_list(&offer);

	/*
	 * send out the REQUEST, trying retransmissions.  either a NAK
	 * or too many REQUEST attempts will revert us to SELECTING.
	 */

	ifsp->if_state = REQUESTING;
	(void) send_pkt(ifsp, dpkt, htonl(INADDR_BROADCAST), stop_requesting);

	/*
	 * wait for an ACK or NAK to come back from the server.  if
	 * we can't register this event handler, then we won't be able
	 * to see the server's responses.  the best we can really do
	 * in that case is drop back to INIT and hope someone notices.
	 */

	if (register_acknak(ifsp) == 0) {

		ifsp->if_state	 = INIT;
		ifsp->if_dflags |= DHCP_IF_FAILED;

		ipc_action_finish(ifsp, DHCP_IPC_E_MEMORY);
		async_finish(ifsp);

		dhcpmsg(MSG_ERROR, "dhcp_requesting: cannot register to "
		    "collect ACK/NAK packets, reverting to INIT on %s",
		    ifsp->if_name);
	}
}

/*
 * select_best(): selects the best OFFER packet from a list of OFFER packets
 *
 *   input: PKT_LIST **: a list of packets to select the best from
 *  output: PKT_LIST *: the best packet, or NULL if none are acceptable
 */

static PKT_LIST *
select_best(PKT_LIST **pkts)
{
	PKT_LIST	*current, *best = NULL;
	uint32_t	points, best_points = 0;

	/*
	 * pick out the best offer.  point system.
	 * what's important?
	 *
	 *	0) DHCP
	 *	1) no option overload
	 *	2) encapsulated vendor option
	 *	3) non-null sname and siaddr fields
	 *	4) non-null file field
	 *	5) hostname
	 *	6) subnetmask
	 *	7) router
	 */

	for (current = *pkts; current != NULL; current = current->next) {

		points = 0;

		if (current->opts[CD_DHCP_TYPE] == NULL) {
			dhcpmsg(MSG_VERBOSE, "valid BOOTP reply");
			goto valid_offer;
		}

		if (current->opts[CD_LEASE_TIME] == NULL) {
			dhcpmsg(MSG_WARNING, "select_best: OFFER without "
			    "lease time");
			continue;
		}

		if (current->opts[CD_LEASE_TIME]->len != sizeof (lease_t)) {
			dhcpmsg(MSG_WARNING, "select_best: OFFER with garbled "
			    "lease time");
			continue;
		}

		if (current->opts[CD_SERVER_ID] == NULL) {
			dhcpmsg(MSG_WARNING, "select_best: OFFER without "
			    "server id");
			continue;
		}

		if (current->opts[CD_SERVER_ID]->len != sizeof (ipaddr_t)) {
			dhcpmsg(MSG_WARNING, "select_best: OFFER with garbled "
			    "server id");
			continue;
		}

		/* valid DHCP OFFER.  see if we got our parameters. */
		dhcpmsg(MSG_VERBOSE, "valid OFFER packet");
		points += 30;

valid_offer:
		if (current->rfc1048)
			points += 5;

		/*
		 * also could be faked, though more difficult because
		 * the encapsulation is hard to encode on a BOOTP
		 * server; plus there's not as much real estate in the
		 * packet for options, so it's likely this option
		 * would get dropped.
		 */

		if (current->opts[CD_VENDOR_SPEC] != NULL)
			points += 80;

		if (current->opts[CD_SUBNETMASK] != NULL)
			points++;

		if (current->opts[CD_ROUTER] != NULL)
			points++;

		if (current->opts[CD_HOSTNAME] != NULL)
			points += 5;

		dhcpmsg(MSG_DEBUG, "select_best: OFFER had %d points", points);

		if (points >= best_points) {
			best_points = points;
			best = current;
		}
	}

	if (best != NULL) {
		dhcpmsg(MSG_DEBUG, "select_best: most points: %d", best_points);
		remove_from_pkt_list(pkts, best);
	} else
		dhcpmsg(MSG_DEBUG, "select_best: no valid OFFER/BOOTP reply");

	free_pkt_list(pkts);
	return (best);
}

/*
 * dhcp_acknak(): processes reception of an ACK or NAK packet on an interface
 *
 *   input: eh_t *: unused
 *	    int: the file descriptor the ACK/NAK arrived on
 *	    short: unused
 *	    eh_event_id_t: the id of this event callback with the handler
 *	    void *: the interface that received the ACK or NAK
 *  output: void
 */

/* ARGSUSED */
void
dhcp_acknak(eh_t *ehp, int fd, short events, eh_event_id_t id, void *arg)
{
	struct ifslist		*ifsp = (struct ifslist *)arg;
	PKT_LIST		*plp;

	if (check_ifs(ifsp) == 0) {
		/* unregister_acknak() does our release_ifs() */
		(void) unregister_acknak(ifsp);
		(void) ioctl(fd, I_FLUSH, FLUSHR|FLUSHW);
		return;
	}

	/*
	 * note that check_ifs() did our release_ifs() but we're not
	 * sure we're done yet; call hold_ifs() to reacquire our hold;
	 * if we're done, unregister_acknak() will release_ifs() below.
	 */

	hold_ifs(ifsp);

	if (recv_pkt(ifsp, fd, DHCP_PACK|DHCP_PNAK, B_FALSE) == 0)
		return;

	/*
	 * we've got a packet; make sure it's acceptable before
	 * cancelling the REQUEST retransmissions.
	 */

	plp = ifsp->if_recv_pkt_list;
	remove_from_pkt_list(&ifsp->if_recv_pkt_list, plp);

	if ((ifsp->if_state == RENEWING || ifsp->if_state == REBINDING) &&
	    *plp->opts[CD_DHCP_TYPE]->value == ACK)
		if (ifsp->if_addr.s_addr != plp->pkt->yiaddr.s_addr) {
			dhcpmsg(MSG_WARNING, "dhcp_acknak: renewal ACK packet "
			    "has a different IP address (%s), ignored",
			    inet_ntoa(plp->pkt->yiaddr));
			ifsp->if_bad_offers++;
			free_pkt_list(&plp);
			return;
		}

	/*
	 * looks good; cancel the retransmission timer and unregister
	 * the acknak handler. ACK to BOUND, NAK back to SELECTING.
	 */

	stop_pkt_retransmission(ifsp);
	(void) unregister_acknak(ifsp);

	if (*(plp->opts[CD_DHCP_TYPE]->value) == NAK) {
		dhcpmsg(MSG_WARNING, "dhcp_acknak: NAK on interface %s",
		    ifsp->if_name);
		ifsp->if_bad_offers++;
		free_pkt_list(&plp);
		restart_dhcp(ifsp);

		/*
		 * remove any bogus cached configuration we might have
		 * around (right now would only happen if we got here
		 * from INIT_REBOOT).
		 */

		(void) remove_hostconf(ifsp->if_name);
		return;
	}

	if (plp->opts[CD_SERVER_ID] == NULL ||
	    plp->opts[CD_SERVER_ID]->len != sizeof (ipaddr_t)) {
		dhcpmsg(MSG_ERROR, "dhcp_acknak: ACK with no valid server id, "
		    "restarting DHCP on %s", ifsp->if_name);
		ifsp->if_bad_offers++;
		free_pkt_list(&plp);
		restart_dhcp(ifsp);
		return;
	}

	if (plp->opts[CD_MESSAGE] != NULL)
		print_server_msg(ifsp, plp->opts[CD_MESSAGE]);

	if (dhcp_bound(ifsp, plp) == 0) {
		dhcpmsg(MSG_WARNING, "dhcp_acknak: dhcp_bound failed "
		    "for %s", ifsp->if_name);
		restart_dhcp(ifsp);
		return;
	}

	dhcpmsg(MSG_VERBOSE, "ACK on interface %s", ifsp->if_name);

	ipc_action_finish(ifsp, DHCP_IPC_SUCCESS);
	async_finish(ifsp);
}

/*
 * restart_dhcp(): restarts DHCP (from INIT) on a given interface
 *
 *   input: struct ifslist *: the interface to restart DHCP on
 *  output: void
 */

static void
restart_dhcp(struct ifslist *ifsp)
{
	if (tq_schedule_timer(tq, DHCP_RESTART_WAIT, dhcp_start, ifsp) == -1) {

		ifsp->if_state	 = INIT;
		ifsp->if_dflags |= DHCP_IF_FAILED;

		ipc_action_finish(ifsp, DHCP_IPC_E_MEMORY);
		async_finish(ifsp);

		dhcpmsg(MSG_ERROR, "restart_dhcp: cannot schedule dhcp_start, "
		    "reverting to INIT state on %s", ifsp->if_name);
	} else
		hold_ifs(ifsp);
}

/*
 * stop_requesting(): decides when to stop retransmitting REQUESTs
 *
 *   input: struct ifslist *: the interface REQUESTs are being sent on
 *	    unsigned int: the number of REQUESTs sent so far
 *  output: boolean_t: B_TRUE if retransmissions should stop
 */

static boolean_t
stop_requesting(struct ifslist *ifsp, unsigned int n_requests)
{
	if (n_requests >= DHCP_MAX_REQUESTS) {

		(void) unregister_acknak(ifsp);

		dhcpmsg(MSG_INFO, "no ACK/NAK to REQUESTING REQUEST, "
		    "restarting DHCP on %s", ifsp->if_name);

		dhcp_selecting(ifsp);
		return (B_TRUE);
	}

	return (B_FALSE);
}
