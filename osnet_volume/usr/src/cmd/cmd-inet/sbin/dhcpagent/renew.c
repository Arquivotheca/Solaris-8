/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)renew.c	1.4	99/09/21 SMI"

#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <dhcpmsg.h>

#include "packet.h"
#include "agent.h"
#include "timer_queue.h"
#include "interface.h"
#include "states.h"
#include "util.h"

/*
 * next_extend_time(): returns the next time an EXTEND request should be sent
 *
 *   input: monosec_t: the absolute time when the next state is entered
 *  output: uint32_t: the number of seconds in the future to send the next
 *		      EXTEND request
 */

static uint32_t
next_extend_time(monosec_t limit_monosec)
{
	monosec_t	current_monosec = monosec();

	if (limit_monosec - current_monosec < DHCP_REBIND_MIN)
		return (0);

	return ((limit_monosec - current_monosec) / 2);
}

/*
 * dhcp_renew(): attempts to renew a DHCP lease
 *
 *   input: tq_t *: unused
 *	    void *: the ifslist to renew the lease on
 *  output: void
 */

/* ARGSUSED */
void
dhcp_renew(tq_t *tqp, void *arg)
{
	struct ifslist *ifsp = (struct ifslist *)arg;
	uint32_t	next;


	ifsp->if_timer[DHCP_T1_TIMER] = -1;

	if (check_ifs(ifsp) == 0) {
		(void) release_ifs(ifsp);
		return;
	}

	/*
	 * sanity check: don't send packets if we're past t2.
	 */

	if (monosec() > (ifsp->if_curstart_monosec + ifsp->if_t2))
		return;

	next = next_extend_time(ifsp->if_curstart_monosec + ifsp->if_t2);

	/*
	 * if there isn't an async event pending, then try to renew.
	 */

	if (!async_pending(ifsp))
		if (async_start(ifsp, DHCP_EXTEND, B_FALSE) != 0)

			/*
			 * try to send extend.  if we don't succeed,
			 * async_timeout() will clean us up.
			 */

			(void) dhcp_extending(ifsp);

	/*
	 * if we're within DHCP_REBIND_MIN seconds of REBINDING, don't
	 * reschedule ourselves.
	 */

	if (next == 0)
		return;

	/*
	 * no big deal if we can't reschedule; we still have the REBIND
	 * state to save us.
	 */

	(void) schedule_ifs_timer(ifsp, DHCP_T1_TIMER, next, dhcp_renew);
}

/*
 * dhcp_rebind(): attempts to renew a DHCP lease from the REBINDING state
 *
 *   input: tq_t *: unused
 *	    void *: the ifslist to renew the lease on
 *  output: void
 */

/* ARGSUSED */
void
dhcp_rebind(tq_t *tqp, void *arg)
{
	struct ifslist *ifsp = (struct ifslist *)arg;
	uint32_t	next;

	ifsp->if_timer[DHCP_T2_TIMER] = -1;

	if (check_ifs(ifsp) == 0) {
		(void) release_ifs(ifsp);
		return;
	}

	/*
	 * sanity check: don't send packets if we've already expired.
	 */

	if (monosec() > (ifsp->if_curstart_monosec + ifsp->if_lease))
		return;

	next = next_extend_time(ifsp->if_curstart_monosec + ifsp->if_lease);

	/*
	 * if this is our first venture into the REBINDING state, then
	 * reset the server address.  we know the renew timer has
	 * already been cancelled (or we wouldn't be here).
	 */

	if (ifsp->if_state == RENEWING) {
		ifsp->if_state = REBINDING;
		ifsp->if_server.s_addr = htonl(INADDR_BROADCAST);
	}

	/*
	 * if there isn't an async event pending, then try to rebind.
	 */

	if (!async_pending(ifsp))
		if (async_start(ifsp, DHCP_EXTEND, B_FALSE) != 0)

			/*
			 * try to send extend.  if we don't succeed,
			 * async_timeout() will clean us up.
			 */

			(void) dhcp_extending(ifsp);

	/*
	 * if we're within DHCP_REBIND_MIN seconds of EXPIRE, don't
	 * reschedule ourselves.
	 */

	if (next == 0) {
		dhcpmsg(MSG_WARNING, "dhcp_rebind: lease on %s expires in less "
		    "than %i seconds!", ifsp->if_name, DHCP_REBIND_MIN);
		return;
	}

	if (schedule_ifs_timer(ifsp, DHCP_T2_TIMER, next, dhcp_rebind) == 0)

		/*
		 * we'll just end up in dhcp_expire(), but it sure sucks.
		 */

		dhcpmsg(MSG_CRIT, "dhcp_rebind: cannot reschedule another "
		    "rebind attempt; lease may expire for %s", ifsp->if_name);
}

/*
 * dhcp_expire(): expires a lease on a given interface and restarts DHCP
 *
 *   input: tq_t *: unused
 *	    void *: the ifslist to expire the lease on
 *  output: void
 */

/* ARGSUSED */
void
dhcp_expire(tq_t *tqp, void *arg)
{
	struct ifslist	*ifsp = (struct ifslist *)arg;

	ifsp->if_timer[DHCP_LEASE_TIMER] = -1;

	if (check_ifs(ifsp) == 0) {
		(void) release_ifs(ifsp);
		return;
	}

	if (async_pending(ifsp))

		if (async_cancel(ifsp) == 0) {

			dhcpmsg(MSG_WARNING, "dhcp_expire: cannot cancel "
			    "current asynchronous command against %s",
			    ifsp->if_name);

			/*
			 * try to schedule ourselves for callback.
			 * we're really situation critical here
			 * there's not much hope for us if this fails.
			 */

			if (tq_schedule_timer(tq, DHCP_EXPIRE_WAIT, dhcp_expire,
			    ifsp) != -1) {
				hold_ifs(ifsp);
				return;
			}

			dhcpmsg(MSG_CRIT, "dhcp_expire: cannot reschedule "
			    "dhcp_expire to get called back, proceeding...");
		}

	/*
	 * just march on if this fails; at worst someone will be able
	 * to async_start() while we're actually busy with our own
	 * asynchronous transaction.  better than not having a lease.
	 */

	if (async_start(ifsp, DHCP_START, B_FALSE) == 0)
		dhcpmsg(MSG_WARNING, "dhcp_expire: cannot start asynchronous "
		    "transaction on %s, continuing...", ifsp->if_name);

	dhcpmsg(MSG_INFO, "lease expired on %s -- restarting DHCP",
	    ifsp->if_name);

	/*
	 * in the case where the lease is less than DHCP_REBIND_MIN
	 * seconds, we will never enter dhcp_renew() and thus the packet
	 * counters will not be reset.  in that case, reset them here.
	 */

	if (ifsp->if_state == BOUND) {
		ifsp->if_bad_offers = 0;
		ifsp->if_sent	    = 0;
		ifsp->if_received   = 0;
	}

	(void) canonize_ifs(ifsp);

	/* reset_ifs() in dhcp_selecting() will clean up any leftover state */
	dhcp_selecting(ifsp);
}

/*
 * dhcp_extending(): sends a REQUEST to extend a lease on a given interface
 *		     and registers to receive the ACK/NAK server reply
 *
 *   input: struct ifslist *: the interface to send the REQUEST on
 *  output: int: 1 if the extension request was sent, 0 otherwise
 */

int
dhcp_extending(struct ifslist *ifsp)
{
	dhcp_pkt_t		*dpkt;

	if (ifsp->if_state == BOUND) {
		ifsp->if_neg_monosec	= monosec();
		ifsp->if_state		= RENEWING;
		ifsp->if_bad_offers	= 0;
		ifsp->if_sent		= 0;
		ifsp->if_received	= 0;
	}

	dhcpmsg(MSG_DEBUG, "dhcp_extending: registering dhcp_acknak on %s",
	    ifsp->if_name);

	if (register_acknak(ifsp) == 0) {

		ipc_action_finish(ifsp, DHCP_IPC_E_MEMORY);
		async_finish(ifsp);

		dhcpmsg(MSG_WARNING, "dhcp_extending: cannot register "
		    "dhcp_acknak for %s, not sending renew request",
		    ifsp->if_name);

		return (0);
	}

	/*
	 * assemble DHCPREQUEST message
	 */

	dpkt = init_pkt(ifsp, REQUEST);
	dpkt->pkt->ciaddr.s_addr = ifsp->if_addr.s_addr;

	add_pkt_opt16(dpkt, CD_MAX_DHCP_SIZE, htons(ifsp->if_max));
	add_pkt_opt32(dpkt, CD_LEASE_TIME, htonl(DHCP_PERM));

	add_pkt_opt(dpkt, CD_CLASS_ID, class_id, class_id_len);
	add_pkt_opt(dpkt, CD_REQUEST_LIST, ifsp->if_prl, ifsp->if_prllen);
	add_pkt_opt(dpkt, CD_END, NULL, 0);

	/*
	 * if we can't send the packet, leave the event handler registered
	 * anyway, since we're not expecting to get any other types of
	 * packets in other than ACKs/NAKs anyway.
	 */

	return (send_pkt(ifsp, dpkt, ifsp->if_server.s_addr, NULL));
}
