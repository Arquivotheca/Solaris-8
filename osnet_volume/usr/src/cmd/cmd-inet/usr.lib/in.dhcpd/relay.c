#ident	"@(#)relay.c	1.36	99/08/03 SMI"

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

#define	MAX_RELAY_IP		5	/* Maximum of destinations */

static struct in_addr relay_ip[MAX_RELAY_IP];	/* IPs of targets */
static struct in_addr relay_net[MAX_RELAY_IP];	/* target nets */
static int relay_reply(IF *, PKT_LIST *);
static int relay_request(IF *, PKT_LIST *);

/*
 * This file contains the code which implements the BOOTP relay agent.
 */

/*
 * Parse arguments.  If an agument begins with a digit, then it's
 * an IP address, otherwise it's a hostname which needs to be
 * resolved into an IP address.
 *
 * Use the arguments to fill in relay_ip array.
 *
 * Only callable by main thread. MT UNSAFE
 */
int
relay_agent_init(char *args)
{
	register int		i;
	register struct in_addr	*ip;
	struct hostent		*hp;
	struct in_addr		*inp;
	char			ntoab[NTOABUF];

	for (i = 0; i <= MAX_RELAY_IP; i++) {
		if ((args = strtok(args, ",")) == NULL)
			break;		/* done */

		/*
		 * If there's more than MAX_RELAY_IP addresses
		 * specified that's an error.  If we can't
		 * resolve the host name, that's an error.
		 */
		if (i == MAX_RELAY_IP) {
			(void) fprintf(stderr,
			    gettext("Too many relay agent destinations.\n"));
			return (E2BIG);
		}

		if ((hp = gethostbyname(args)) == NULL) {
			(void) fprintf(stderr, gettext(
			    "Invalid relay agent destination name: %s\n"),
			    args);
			return (EINVAL);
		}
		/* LINTED [will be lw aligned] */
		ip = (struct in_addr *)hp->h_addr;

		/*
		 * Note: no way to guess at destination subnet mask,
		 * and verify that it's not a new broadcast addr.
		 */
		if (ip->s_addr == INADDR_ANY ||
		    ip->s_addr == INADDR_LOOPBACK ||
		    ip->s_addr == INADDR_BROADCAST) {
			(void) fprintf(stderr, gettext("Relay destination \
cannot be 0, loopback, or broadcast address.\n"));
			return (EINVAL);
		}

		relay_ip[i].s_addr = ip->s_addr;

		ip = &relay_ip[i];
		inp = &relay_net[i];
		(void) get_netmask(ip, &inp);
		inp->s_addr &= ip->s_addr;
		if (verbose) {
			(void) fprintf(stdout,
			    gettext("Relay destination: %s (%s)"),
			    inet_ntoa_r(relay_ip[i], ntoab), args);
			(void) fprintf(stdout, gettext("\t\tnetwork: %s\n"),
			    inet_ntoa_r(relay_net[i], ntoab));
		}
		args = NULL;	/* for next call to strtok() */
	}
	if (i == 0) {
		/*
		 * Gotta specify at least one IP addr.
		 */
		(void) fprintf(stderr,
		    gettext("Specify at least one relay agent destination.\n"));
		return (ENOENT);
	}
	if (i < MAX_RELAY_IP)
		relay_ip[i].s_addr = NULL; 	/* terminate the list */

	return (0);
}

/*
 * Note: if_head_mtx must be held by caller, as the interface list is
 * walked in relay_reply.
 *
 * MT SAFE
 */
int
relay_agent(IF *ifp, PKT_LIST *plp)
{
	if (plp->pkt->op == BOOTREQUEST)
		return (relay_request(ifp, plp));

	return (relay_reply(ifp, plp));
}

/*
 * MT SAFE
 */
static int
relay_request(IF *ifp, PKT_LIST *plp)
{
	PKT			*pkp;
	struct sockaddr_in	to;
	struct in_addr		ifnet, any;
	int			i;
	char			ntoab[NTOABUF], buf[DHCP_MAX_OPT_SIZE], *msg;

	pkp = plp->pkt;
	if (pkp->giaddr.s_addr == 0L)
		pkp->giaddr.s_addr = ifp->addr.s_addr;
	pkp->hops++;

	/*
	 * Send it on to the next relay(s)/servers
	 */
	to.sin_port = htons(IPPORT_BOOTPS);
	any.s_addr = htonl(INADDR_ANY);
	(void) disp_cid(plp, buf, sizeof (buf));

	for (i = 0; i < MAX_RELAY_IP; i++) {
		if (relay_ip[i].s_addr == 0L)
			break;		/* we're done */

		ifnet.s_addr = ifp->addr.s_addr & ifp->mask.s_addr;
		if (relay_net[i].s_addr == ifnet.s_addr) {
			if (verbose) {
				dhcpmsg(LOG_INFO, "Target's network: \
%1$s is the same as client %2$s network, ignored.\n",
				    inet_ntoa_r(relay_net[i], ntoab), buf);
			}
			continue;	/* skip this target */
		}

		to.sin_addr.s_addr = relay_ip[i].s_addr;

		if (to.sin_port == htons(IPPORT_BOOTPS))
			msg = "Relaying request %1$s to %2$s, server port.\n";
		else
			msg = "Relaying request %1$s to %2$s, client port.\n";

		if (debug) {
			dhcpmsg(LOG_INFO, msg, buf,
			    inet_ntoa_r(to.sin_addr, ntoab));
		}

		if (write_interface(ifp, pkp, plp->len, &to)) {
			dhcpmsg(LOG_INFO, "Cannot relay request %1$s to %2$s\n",
			    buf, inet_ntoa_r(to.sin_addr, ntoab));
		} else {
			logtrans(P_BOOTP, L_RELAY_REQ, 0, any, to.sin_addr,
			    plp);
		}
	}
	return (0);
}

/*
 * Note: if_head_mtx must be held by caller, as the interface list is
 * walked here.
 */
static int
relay_reply(IF *ifp, PKT_LIST *plp)
{
	int		err;
	IF		*tifp;
	PKT		*pkp = plp->pkt;
	struct in_addr	to;
	char		buf[DHCP_MAX_OPT_SIZE];
	char		ntoab_a[NTOABUF], ntoab_b[NTOABUF];

	assert(_mutex_held(&if_head_mtx));

	if (pkp->giaddr.s_addr == 0L) {
		/*
		 * Somehow we picked up a reply packet from a DHCP server
		 * on this net intended for a client on this net. Drop it.
		 */
		if (verbose) {
			dhcpmsg(LOG_INFO,
			    "Reply packet without giaddr set ignored.\n");
		}
		return (0);
	}

	/*
	 * We can assume that the message came directly from a dhcp/bootp
	 * server to us, and we are to address it directly to the client.
	 */
	if (pkp->giaddr.s_addr != ifp->addr.s_addr) {
		/*
		 * It is possible that this is a multihomed host. We'll
		 * check to see if this is the case, and handle it
		 * appropriately.
		 */
		for (tifp = if_head; tifp != NULL; tifp = tifp->next) {
			if (tifp->addr.s_addr == pkp->giaddr.s_addr)
				break;
		}

		if (tifp == NULL) {
			if (verbose) {
				dhcpmsg(LOG_INFO, "Received relayed reply \
not intended for this interface: %1$s giaddr: %2$s\n",
				    inet_ntoa_r(ifp->addr, ntoab_a),
				    inet_ntoa_r(pkp->giaddr, ntoab_b));
			}
			return (0);
		} else
			ifp = tifp;
	}

	(void) disp_cid(plp, buf, sizeof (buf));
	pkp->hops++;

	if (debug)
		dhcpmsg(LOG_INFO, "Relaying reply to client %s\n", buf);

	if ((ntohs(pkp->flags) & BCAST_MASK) == 0) {
		if (pkp->yiaddr.s_addr == htonl(INADDR_ANY)) {
			if (pkp->ciaddr.s_addr == htonl(INADDR_ANY)) {
				dhcpmsg(LOG_INFO, "No destination IP \
address or network IP address; cannot send reply to client: %s.\n", buf);
				return (0);
			}
			to.s_addr = pkp->ciaddr.s_addr;
		} else
			to.s_addr = pkp->yiaddr.s_addr;
	}

	if ((err = send_reply(ifp, pkp, plp->len, &to)) == 0)
		logtrans(P_BOOTP, L_RELAY_REP, 0, ifp->addr, to, plp);

	return (err);
}
