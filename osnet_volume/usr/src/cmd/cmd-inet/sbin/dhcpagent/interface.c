/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)interface.c	1.6	99/09/22 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/dlpi.h>
#include <stdlib.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <string.h>
#include <unistd.h>
#include <netinet/if_ether.h>
#include <signal.h>
#include <dhcpmsg.h>

#include "interface.h"
#include "util.h"
#include "dlpi_io.h"
#include "packet.h"
#include "defaults.h"
#include "states.h"

/*
 * note to the reader:
 *
 * the terminology in here is slightly confusing.  in particular, the
 * term `ifslist' is used to refer both to the `struct ifslist' entry
 * that makes up a specific interface entry, and the `internal
 * ifslist' which is a linked list of struct ifslists.  to reduce
 * confusion, in the comments, a `struct ifslist' is referred to as
 * an `ifs', and `ifslist' refers to the internal ifslist.
 *
 */

static struct ifslist	*ifsheadp;
static unsigned int	ifscount;

static void	init_ifs(struct ifslist *);
static void	free_ifs(struct ifslist *);
static void	cancel_ifs_timer(struct ifslist *, int);

/*
 * insert_ifs(): creates a new ifs and chains it on the ifslist.  initializes
 *		 state which remains consistent across all use of the ifs entry
 *
 *   input: const char *: the name of the ifs entry (interface name)
 *	    boolean_t: if B_TRUE, we're adopting the interface
 *	    int *: ignored on input; if insert_ifs fails, set to a DHCP_IPC_E_*
 *		   error code with the reason why
 *  output: struct ifslist *: a pointer to the new ifs entry, or NULL on failure
 */

struct ifslist *
insert_ifs(const char *if_name, boolean_t is_adopting, int *error)
{
	uint32_t		buf[DLPI_BUF_MAX / sizeof (uint32_t)];
	dl_info_ack_t		*dlia = (dl_info_ack_t *)buf;
	caddr_t			dl_addr;
	struct ifreq    	ifr;
	unsigned int		i, client_id_len;
	uchar_t			*client_id;
	const char		*prl;
	struct ifslist		*ifsp, ifs = { 0 };

	ifsp = lookup_ifs(if_name);
	if (ifsp != NULL) {
		*error = DHCP_IPC_E_INT;	/* should never happen */
		return (NULL);
	}

	(void) strlcpy(ifs.if_name, if_name, IFNAMSIZ);

	/*
	 * okay, we've got a request to put a new interface under our
	 * control.  it's our job to set everything that doesn't
	 * change for the life of the interface.  (state that changes
	 * should be initialized in init_ifs() and reset by reset_ifs())
	 *
	 *  1. verify the interface can support DHCP
	 *  2. get the interface mtu
	 *  3. get the interface hardware type and hardware length
	 *  4. get the interface hardware address
	 *  5. get the interface broadcast address
	 *  6. get the interface flags
	 */

	/* step 1 */
	ifs.if_dlpi_fd = dlpi_open(if_name, dlia, sizeof (buf), ETHERTYPE_IP);
	if (ifs.if_dlpi_fd == -1) {
		dhcpmsg(MSG_ERR, "insert_ifs: cannot dlpi_open %s", if_name);
		*error = DHCP_IPC_E_INVIF;
		return (NULL);
	}

	init_ifs(&ifs);			/* ifs.if_dlpi_fd must be valid */
	ipc_action_init(&ifs);

	/* step 2 */
	ifs.if_max = dlia->dl_max_sdu;
	ifs.if_opt = ifs.if_max - BASE_PKT_SIZE;
	ifs.if_min = dlia->dl_min_sdu;

	if (ifs.if_max < DHCP_DEF_MAX_SIZE) {
		dhcpmsg(MSG_ERROR, "insert_ifs: %s does not have a large "
		    "enough maximum SDU to support DHCP", if_name);
		*error = DHCP_IPC_E_INVIF;
		goto failure;
	}

	/* step 3 */
	ifs.if_hwtype = dlpi_to_arp(dlia->dl_mac_type);
	ifs.if_hwlen  = dlia->dl_addr_length - abs(dlia->dl_sap_length);

	dhcpmsg(MSG_DEBUG, "insert_ifs: %s: sdumax %d, optmax %d, hwtype %d, "
	    "hwlen %d", if_name, ifs.if_max, ifs.if_opt, ifs.if_hwtype,
	    ifs.if_hwlen);

	/* step 4 */
	ifs.if_hwaddr = malloc(ifs.if_hwlen);
	if (ifs.if_hwaddr == NULL) {
		dhcpmsg(MSG_ERR, "insert_ifs: cannot allocate if_hwaddr "
		    "for %s", if_name);
		*error = DHCP_IPC_E_MEMORY;
		goto failure;
	}

	/*
	 * depending on the DLPI device, the sap and hardware addresses
	 * can be in either order within the dlsap address; find the
	 * location of the hardware address using dl_sap_length.  see the
	 * DLPI specification for more on this braindamage.
	 */

	dl_addr = (caddr_t)dlia + dlia->dl_addr_offset;
	if (dlia->dl_sap_length > 0) {
		ifs.if_sap_before++;
		dl_addr += dlia->dl_sap_length;
	}

	(void) memcpy(ifs.if_hwaddr, dl_addr, ifs.if_hwlen);

	/* step 5 */
	ifs.if_saplen = abs(dlia->dl_sap_length);
	ifs.if_daddr  = build_broadcast_dest(dlia, &ifs.if_dlen);
	if (ifs.if_daddr == NULL) {
		dhcpmsg(MSG_ERR, "insert_ifs: cannot allocate if_daddr "
		    "for %s", if_name);
		*error = DHCP_IPC_E_MEMORY;
		goto failure;
	}

	/* step 6 */
	(void) strlcpy(ifr.ifr_name, if_name, IFNAMSIZ);

	if (ioctl(ifs.if_sock_fd, SIOCGIFFLAGS, &ifr) == -1) {
		if (errno == ENXIO)
			*error = DHCP_IPC_E_INVIF;
		else
			*error = DHCP_IPC_E_INT;
		dhcpmsg(MSG_ERR, "insert_ifs: SIOCGIFFLAGS for %s", if_name);
		goto failure;
	}

	/*
	 * if DHCPRUNNING is already set on the interface and we're
	 * not adopting it, the agent probably crashed and burned.
	 * note it, but don't let it stop the proceedings.  we're
	 * pretty sure we're not already running, since we wouldn't
	 * have been able to bind to our IPC port.
	 */

	if ((is_adopting == B_FALSE) && (ifr.ifr_flags & IFF_DHCPRUNNING))
		dhcpmsg(MSG_WARNING, "insert_ifs: DHCP flag already set on %s",
		    if_name);

	ifr.ifr_flags |= IFF_DHCPRUNNING;
	(void) ioctl(ifs.if_sock_fd, SIOCSIFFLAGS, &ifr);

	ifs.if_send_pkt.pkt = malloc(ifs.if_max);
	if (ifs.if_send_pkt.pkt == NULL) {
		dhcpmsg(MSG_ERR, "insert_ifs: cannot allocate if_send_pkt "
		    "for %s", if_name);
		*error = DHCP_IPC_E_MEMORY;
		goto failure;
	}

	/*
	 * the cid by default is empty.  user can override this in
	 * the defaults file.
	 */

	ifs.if_cid = df_get_octet(if_name, DF_CLIENT_ID, &client_id_len);
	if (ifs.if_cid != NULL) {

		client_id = malloc(client_id_len);
		if (client_id == NULL) {
			dhcpmsg(MSG_ERR, "insert_ifs: cannot allocate client "
			    "id for %s", if_name);
			*error = DHCP_IPC_E_MEMORY;
			goto failure;
		}

		(void) memcpy(client_id, ifs.if_cid, client_id_len);
		ifs.if_cid	= client_id;
		ifs.if_cidlen	= client_id_len;
	}

	/*
	 * initialize the parameter request list, if there is one.
	 */

	prl = df_get_string(if_name, DF_PARAM_REQUEST_LIST);
	if (prl == NULL)
		ifs.if_prl = NULL;
	else {
		for (ifs.if_prllen = 1, i = 0; prl[i] != '\0'; i++)
			if (prl[i] == ',')
				ifs.if_prllen++;

		ifs.if_prl = malloc(ifs.if_prllen);
		if (ifs.if_prl == NULL) {
			dhcpmsg(MSG_WARNING, "insert_ifs: cannot allocate "
			    "parameter request list for %s (continuing)",
			    if_name);
		} else {

			for (i = 0; i < ifs.if_prllen; prl++, i++) {
				ifs.if_prl[i] = strtoul(prl, NULL, 0);
				while (*prl != ',' && *prl != '\0')
					prl++;
				if (*prl == '\0')
					break;
			}
		}
	}

	/*
	 * now finally create the ifs, initialize it with the template ifs,
	 * and chain it on to ifsheadp.
	 */

	ifsp = malloc(sizeof (struct ifslist));
	if (ifsp == NULL) {
		dhcpmsg(MSG_ERR, "insert_ifs: cannot allocate ifs entry for "
		    "%s", if_name);
		*error = DHCP_IPC_E_MEMORY;
		goto failure;
	}

	*ifsp		= ifs;		/* structure copy */
	ifsp->next	= ifsheadp;
	ifsp->prev	= NULL;
	ifsheadp	= ifsp;

	if (ifsheadp->next != NULL)
		ifsheadp->next->prev = ifsheadp;

	hold_ifs(ifsp);
	ifscount++;

	if (inactivity_id != -1)
		if (tq_cancel_timer(tq, inactivity_id, NULL) == 1)
			inactivity_id = -1;

	dhcpmsg(MSG_DEBUG, "insert_ifs: inserted interface %s", if_name);
	return (ifsp);

failure:
	free_ifs(&ifs);
	return (NULL);
}

/*
 * init_ifs(): puts an ifs in its initial state
 *
 *   input: struct ifslist *: the ifs to initialize
 *  output: void
 *    note: if the interface isn't fresh, use reset_ifs()
 */

static void
init_ifs(struct ifslist *ifsp)
{
	/*
	 * if_sock_ip_fd is created and bound in configure_if().
	 * if_sock_fd is bound in configure_if(); see comments in
	 * bound.c for more details on why.  if creation of if_sock_fd
	 * fails, we'll need more context anyway, so don't check.
	 */

	ifsp->if_sock_fd	= socket(AF_INET, SOCK_DGRAM, 0);
	ifsp->if_sock_ip_fd	= -1;
	ifsp->if_state		= INIT;
	ifsp->if_routers	= NULL;
	ifsp->if_nrouters	= 0;
	ifsp->if_ack		= NULL;
	ifsp->if_server.s_addr  = htonl(INADDR_BROADCAST);
	ifsp->if_neg_monosec 	= monosec();
	ifsp->if_lease 		= 0;
	ifsp->if_t1 		= 0;
	ifsp->if_t2 		= 0;

	ifsp->if_acknak_id		 = -1;
	ifsp->if_acknak_bcast_id	 = -1;
	ifsp->if_timer[DHCP_T1_TIMER]	 = -1;
	ifsp->if_timer[DHCP_T2_TIMER]    = -1;
	ifsp->if_timer[DHCP_LEASE_TIMER] = -1;

	set_packet_filter(ifsp->if_dlpi_fd, dhcp_filter, NULL, "DHCP");

	dhcpmsg(MSG_DEBUG, "init_ifs: initted interface %s", ifsp->if_name);
}

/*
 * reset_ifs(): resets an ifs to its initial state
 *
 *   input: struct ifslist *: the ifs to reset
 *  output: void
 */

void
reset_ifs(struct ifslist *ifsp)
{
	ifsp->if_dflags &= ~DHCP_IF_FAILED;

	if (ifsp->if_sock_fd != -1)
		(void) close(ifsp->if_sock_fd);

	if (ifsp->if_ack != NULL)
		free_pkt_list(&ifsp->if_ack);

	if (ifsp->if_sock_ip_fd != -1)
		(void) close(ifsp->if_sock_ip_fd);

	while (ifsp->if_nrouters > 0) {
		(void) del_default_route(ifsp->if_sock_fd,
		    &ifsp->if_routers[--ifsp->if_nrouters]);
		free(ifsp->if_routers);
	}

	(void) unregister_acknak(ifsp);		/* just in case */

	cancel_ifs_timers(ifsp);
	init_ifs(ifsp);
}

/*
 * lookup_ifs(): looks up an ifs, given its name
 *
 *   input: const char *: the name of the ifs entry (the interface name)
 *			  the name "" searches for the primary interface
 *  output: struct ifslist *: the corresponding ifs, or NULL if not found
 */

struct ifslist *
lookup_ifs(const char *if_name)
{
	struct ifslist	*ifs;

	for (ifs = ifsheadp; ifs != NULL; ifs = ifs->next)
		if (*if_name != '\0') {
			if (strcmp(ifs->if_name, if_name) == 0)
				break;
		} else if (ifs->if_dflags & DHCP_IF_PRIMARY)
			break;

	return (ifs);
}

/*
 * remove_ifs(): removes a given ifs from the ifslist.  marks the ifs
 *		 for being freed (but may not actually free it).
 *
 *   input: struct ifslist *: the ifs to remove
 *  output: void
 *    note: see interface.h for a discussion of ifs memory management
 */

void
remove_ifs(struct ifslist *ifsp)
{
	struct ifreq	ifr;

	if (ifsp->if_dflags & DHCP_IF_REMOVED)
		return;

	(void) memset(&ifr, 0, sizeof (struct ifreq));
	(void) strlcpy(ifr.ifr_name, ifsp->if_name, IFNAMSIZ);

	if (ioctl(ifsp->if_sock_fd, SIOCGIFFLAGS, &ifr) == 0) {
		ifr.ifr_flags &= ~IFF_DHCPRUNNING;
		(void) ioctl(ifsp->if_sock_fd, SIOCSIFFLAGS, &ifr);
	}

	while (ifsp->if_nrouters > 0) {
		(void) del_default_route(ifsp->if_sock_fd,
		    &ifsp->if_routers[--ifsp->if_nrouters]);
	}

	ifsp->if_dflags |= DHCP_IF_REMOVED;

	/*
	 * if we have long term timers, cancel them so that interface
	 * resources can be reclaimed in a reasonable amount of time.
	 */

	cancel_ifs_timers(ifsp);

	if (ifsp->prev != NULL)
		ifsp->prev->next = ifsp->next;
	else
		ifsheadp = ifsp->next;

	if (ifsp->next != NULL)
		ifsp->next->prev = ifsp->prev;

	ifscount--;
	(void) release_ifs(ifsp);

	/* no big deal if this fails */
	if (ifscount == 0) {
		inactivity_id = tq_schedule_timer(tq, DHCP_INACTIVITY_WAIT,
		    inactivity_shutdown, NULL);
	}
}

/*
 * hold_ifs(): acquires a hold on an ifs
 *
 *   input: struct ifslist *: the ifs entry to acquire a hold on
 *  output: void
 */

void
hold_ifs(struct ifslist *ifsp)
{
	ifsp->if_hold_count++;

	dhcpmsg(MSG_DEBUG2, "hold_ifs: hold count on %s: %d",
	    ifsp->if_name, ifsp->if_hold_count);
}

/*
 * release_ifs(): releases a hold previously acquired on an ifs.  if the
 *		  hold count reaches 0, the ifs is freed
 *
 *   input: struct ifslist *: the ifs entry to release the hold on
 *  output: int: the number of holds outstanding on the ifs
 */

int
release_ifs(struct ifslist *ifsp)
{
	if (ifsp->if_hold_count == 0) {
		dhcpmsg(MSG_CRIT, "release_ifs: extraneous release");
		return (0);
	}

	if (--ifsp->if_hold_count == 0) {
		free_ifs(ifsp);
		return (0);
	}

	dhcpmsg(MSG_DEBUG2, "release_ifs: hold count on %s: %d",
	    ifsp->if_name, ifsp->if_hold_count);

	return (ifsp->if_hold_count);
}

/*
 * free_ifs(): frees the memory occupied by an ifs entry
 *
 *   input: struct ifslist *: the ifs entry to free
 *  output: void
 */

static void
free_ifs(struct ifslist *ifsp)
{
	dhcpmsg(MSG_DEBUG, "free_ifs: freeing interface %s", ifsp->if_name);

	free_pkt_list(&ifsp->if_recv_pkt_list);
	free_pkt_list(&ifsp->if_ack);
	free(ifsp->if_send_pkt.pkt);
	free(ifsp->if_cid);
	free(ifsp->if_daddr);
	free(ifsp->if_hwaddr);
	free(ifsp->if_prl);
	free(ifsp->if_routers);

	if (ifsp->if_sock_fd != -1)
		(void) close(ifsp->if_sock_fd);

	if (ifsp->if_sock_ip_fd != -1)
		(void) close(ifsp->if_sock_ip_fd);

	if (ifsp->if_dlpi_fd != -1)
		(void) dlpi_close(ifsp->if_dlpi_fd);

	free(ifsp);
}

/*
 * verify_ifs(): verifies than an ifs is still valid (i.e., has not been
 *		 explicitly or implicitly dropped or released)
 *
 *   input: struct ifslist *: the ifs to verify
 *  output: int: 1 if the ifs is still valid, 0 if the interface is invalid
 */

int
verify_ifs(struct ifslist *ifsp)
{
	struct sockaddr_in	*sin;
	struct ifreq 		ifr;

	/* LINTED [ifr_addr is a sockaddr which will be aligned] */
	sin = (struct sockaddr_in *)&ifr.ifr_addr;

	if (ifsp->if_dflags & DHCP_IF_REMOVED)
		return (0);

	(void) memset(&ifr, 0, sizeof (struct ifreq));
	(void) strlcpy(ifr.ifr_name, ifsp->if_name, IFNAMSIZ);

	ifr.ifr_addr.sa_family = AF_INET;

	switch (ifsp->if_state) {

	case BOUND:
	case RENEWING:
	case REBINDING:

		if (ioctl(ifsp->if_sock_fd, SIOCGIFFLAGS, &ifr) == 0)
			if ((ifr.ifr_flags & (IFF_UP|IFF_DHCPRUNNING))
			    != (IFF_UP|IFF_DHCPRUNNING))
				goto abandon;

		/* FALLTHRU */

	case INIT_REBOOT:
	case SELECTING:
	case REQUESTING:

		/*
		 * if the IP address, netmask, or broadcast address
		 * have changed, or the interface has gone down, then
		 * we act like there has been an implicit drop.
		 */

		if (ioctl(ifsp->if_sock_fd, SIOCGIFADDR, &ifr) == 0)
			if (sin->sin_addr.s_addr != ifsp->if_addr.s_addr)
				goto abandon;

		if (ioctl(ifsp->if_sock_fd, SIOCGIFNETMASK, &ifr) == 0)
			if (sin->sin_addr.s_addr != ifsp->if_netmask.s_addr)
				goto abandon;

		if (ioctl(ifsp->if_sock_fd, SIOCGIFBRDADDR, &ifr) == 0)
			if (sin->sin_addr.s_addr != ifsp->if_broadcast.s_addr)
				goto abandon;
	}

	return (1);
abandon:
	dhcpmsg(MSG_WARNING, "verify_ifs: %s has changed properties, "
	    "abandoning", ifsp->if_name);

	remove_ifs(ifsp);
	return (0);
}

/*
 * canonize_ifs(): puts the interface in a canonical (zeroed) form
 *
 *   input: struct ifslist *: the interface to canonize
 *  output: int: 1 on success, 0 on failure
 */

int
canonize_ifs(struct ifslist *ifsp)
{
	struct sockaddr_in	*sin;
	struct ifreq		ifr;

	dhcpmsg(MSG_VERBOSE, "canonizing interface %s", ifsp->if_name);

	/* LINTED [ifr_addr is a sockaddr which will be aligned] */
	sin = (struct sockaddr_in *)&ifr.ifr_addr;

	(void) memset(&ifr, 0, sizeof (struct ifreq));
	(void) strlcpy(ifr.ifr_name, ifsp->if_name, IFNAMSIZ);

	if (ioctl(ifsp->if_sock_fd, SIOCGIFFLAGS, &ifr) == -1)
		return (0);

	/*
	 * clear the UP flag, but don't clear DHCPRUNNING since
	 * that should only be done when the interface is removed
	 * (see remove_ifs())
	 */

	ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(ifsp->if_sock_fd, SIOCSIFFLAGS, &ifr) == -1)
		return (0);

	/*
	 * since ifr is actually a union, we need to explicitly zero
	 * the flags field before we reuse the structure, or otherwise
	 * cruft may leak over into other members of the union.
	 */

	ifr.ifr_flags = 0;
	ifr.ifr_addr.sa_family = AF_INET;
	sin->sin_addr.s_addr = htonl(INADDR_ANY);

	if (ioctl(ifsp->if_sock_fd, SIOCSIFADDR, &ifr) == -1)
		return (0);

	if (ioctl(ifsp->if_sock_fd, SIOCSIFNETMASK, &ifr) == -1)
		return (0);

	if (ioctl(ifsp->if_sock_fd, SIOCSIFBRDADDR, &ifr) == -1)
		return (0);

	/*
	 * any time we change the IP address, netmask, or broadcast we
	 * must be careful to also reset bookkeeping of what these are
	 * set to.  this is so we can detect if these characteristics
	 * are changed by another process.
	 */

	ifsp->if_addr.s_addr	  = htonl(INADDR_ANY);
	ifsp->if_netmask.s_addr   = htonl(INADDR_ANY);
	ifsp->if_broadcast.s_addr = htonl(INADDR_ANY);

	return (1);
}

/*
 * check_ifs(): makes sure an ifs is still valid, and if it is, releases the
 *		ifs.  otherwise, it informs the caller the ifs is going away
 *		and expects the caller to perform the release
 *
 *   input: struct ifslist *: the ifs to check
 *  output: int: 1 if the interface is valid, 0 otherwise
 */

int
check_ifs(struct ifslist *ifsp)
{
	hold_ifs(ifsp);
	if (release_ifs(ifsp) == 1 || verify_ifs(ifsp) == 0) {

		/*
		 * this interface is going away.  if there's an
		 * uncancelled IPC event roaming around, cancel it
		 * now.  we leave the hold on in case anyone else has
		 * any cleanup work that needs to be done before the
		 * interface goes away.
		 */

		ipc_action_finish(ifsp, DHCP_IPC_E_UNKIF);
		async_finish(ifsp);
		return (0);
	}

	(void) release_ifs(ifsp);
	return (1);
}

/*
 * nuke_ifslist(): delete the ifslist (for use when the dhcpagent is exiting)
 *
 *   input: boolean_t: B_TRUE if the agent is exiting due to SIGTERM
 *  output: void
 */

void
nuke_ifslist(boolean_t onterm)
{
	struct ifslist	*ifsp, *ifsp_next;

	for (ifsp = ifsheadp; ifsp != NULL; ifsp = ifsp_next) {
		ifsp_next = ifsp->next;
		if (onterm && df_get_bool(ifsp->if_name, DF_RELEASE_ON_SIGTERM))
			if (dhcp_release(ifsp, "DHCP agent is exiting") == 1)
				continue;
		dhcp_drop(ifsp);
	}
}

/*
 * refresh_ifslist(): refreshes all finite leases under DHCP control
 *
 *   input: eh_t *: unused
 *	    int: unused
 *	    void *: unused
 *  output: void
 */

/* ARGSUSED */
void
refresh_ifslist(eh_t *eh, int sig, void *arg)
{
	struct ifslist *ifsp;

	for (ifsp = ifsheadp; ifsp != NULL; ifsp = ifsp->next) {

		if (ifsp->if_state != BOUND && ifsp->if_state != RENEWING &&
		    ifsp->if_state != REBINDING)
			continue;

		if (ifsp->if_lease == DHCP_PERM)
			continue;

		/*
		 * this interface has a finite lease and we do not know
		 * how long the machine's been off for.  refresh it.
		 */

		dhcpmsg(MSG_WARNING, "refreshing lease on %s", ifsp->if_name);
		cancel_ifs_timer(ifsp, DHCP_T1_TIMER);
		cancel_ifs_timer(ifsp, DHCP_T2_TIMER);
		(void) tq_adjust_timer(tq, ifsp->if_timer[DHCP_LEASE_TIMER], 0);
	}
}

/*
 * ifs_count(): returns the number of interfaces currently managed
 *
 *   input: void
 *  output: unsigned int: the number of interfaces currently managed
 */

unsigned int
ifs_count(void)
{
	return (ifscount);
}

/*
 * cancel_ifs_timer(): cancels a lease-related timer on an interface
 *
 *   input: struct ifslist *: the interface to operate on
 *	    int: the timer id of the timer to cancel
 *  output: void
 */

static void
cancel_ifs_timer(struct ifslist *ifsp, int timer_id)
{
	if (ifsp->if_timer[timer_id] != -1) {
		if (tq_cancel_timer(tq, ifsp->if_timer[timer_id], NULL) == 1) {
			(void) release_ifs(ifsp);
			ifsp->if_timer[timer_id] = -1;
		} else
			dhcpmsg(MSG_WARNING, "cancel_ifs_timer: cannot cancel "
			    "if_timer[%d]", timer_id);
	}
}

/*
 * cancel_ifs_timers(): cancels an interface's pending lease-related timers
 *
 *   input: struct ifslist *: the interface to operate on
 *  output: void
 */

void
cancel_ifs_timers(struct ifslist *ifsp)
{
	cancel_ifs_timer(ifsp, DHCP_T1_TIMER);
	cancel_ifs_timer(ifsp, DHCP_T2_TIMER);
	cancel_ifs_timer(ifsp, DHCP_LEASE_TIMER);
}

/*
 * schedule_ifs_timer(): schedules a lease-related timer on an interface
 *
 *   input: struct ifslist *: the interface to operate on
 *	    int: the timer to schedule
 *	    uint32_t: the number of seconds in the future it should fire
 *	    tq_callback_t *: the callback to call upon firing
 *  output: int: 1 if the timer was scheduled successfully, 0 on failure
 */

int
schedule_ifs_timer(struct ifslist *ifsp, int timer_id, uint32_t sec,
    tq_callback_t *expire)
{
	cancel_ifs_timer(ifsp, timer_id);		/* just in case */

	ifsp->if_timer[timer_id] = tq_schedule_timer(tq, sec, expire, ifsp);
	if (ifsp->if_timer[timer_id] == -1) {
		dhcpmsg(MSG_WARNING, "schedule_ifs_timer: cannot schedule "
		    "if_timer[%d]", timer_id);
		return (0);
	}

	hold_ifs(ifsp);
	return (1);
}
