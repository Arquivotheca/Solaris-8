/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)main.c	1.10	99/12/06 SMI"

#include "defs.h"
#include "tables.h"

static void	initlog(void);
static void	run_timeouts(void);
static void	check_fallback(void);

static void	advertise(struct sockaddr_in6 *sin6, struct phyint *pi,
		    boolean_t no_prefixes);
static void	solicit(struct sockaddr_in6 *sin6, struct phyint *pi);
static void	initifs(boolean_t first);
static void	check_if_removed(struct phyint *pi);
static void	loopback_ra_enqueue(struct phyint *pi,
		    struct nd_router_advert *ra, int len);
static void	loopback_ra_dequeue(void);

struct in6_addr all_nodes_mcast = { { 0xff, 0x2, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x1 } };

struct in6_addr all_routers_mcast = { { 0xff, 0x2, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x2 } };

static struct sockaddr_in6 v6allnodes = { AF_INET6, 0, 0,
				    { 0xff, 0x2, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x1 } };

static struct sockaddr_in6 v6allrouters = { AF_INET6, 0, 0,
				    { 0xff, 0x2, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x0,
				    0x0, 0x0, 0x0, 0x2 } };

static char **argv0;		/* Saved for re-exec on SIGHUP */

static uint64_t packet[(IP_MAXPACKET + 1)/8];

static int	show_ifs = 0;
int		debug = 0;
int		no_loopback = 0; /* Do not send RA packets to ourselves */
boolean_t addrconf = _B_TRUE;	/* Stateless addrconf enabled? */

/*
 * Size of routing socket message used by in.ndpd which includes the header,
 * space for the RTA_DST, RTA_GATEWAY and RTA_NETMASK (each a sockaddr_in6)
 * plus space for the RTA_IFP (a sockaddr_dl).
 */
#define	NDP_RTM_MSGLEN	sizeof (struct rt_msghdr) +	\
			sizeof (struct sockaddr_in6) +	\
			sizeof (struct sockaddr_in6) +	\
			sizeof (struct sockaddr_in6) +	\
			sizeof (struct sockaddr_dl)

/*
 * These are referenced externally in tables.c in order to fill in the
 * dynamic portions of the routing socket message and then to send the message
 * itself.
 */
int	rtsock;				/* Routing socket */
struct	rt_msghdr	*rt_msg;	/* Routing socket message */
struct	sockaddr_in6	*rta_gateway;	/* RTA_GATEWAY sockaddr */
struct	sockaddr_dl	*rta_ifp;	/* RTA_IFP sockaddr */

/*
 * Return the current time in milliseconds truncated to
 * fit in an integer.
 */
static uint_t
getcurrenttime(void)
{
	struct timeval tp;

	if (gettimeofday(&tp, NULL) < 0) {
		logperror("getcurrenttime: gettimeofday failed");
		exit(1);
	}
	return (tp.tv_sec * 1000 + tp.tv_usec / 1000);
}

/*
 * Output a preformated packet from the packet[] buffer.
 */
static void
sendpacket(struct sockaddr_in6 *sin6, int sock, int size, int flags)
{
	int cc;
	char abuf[INET6_ADDRSTRLEN];

	cc = sendto(sock, (char *)packet, size, flags,
		(struct sockaddr *)sin6, sizeof (*sin6));
	if (cc < 0 || cc != size) {
		if (cc < 0) {
			logperror("sendpacket: sendto");
		}
		logerr("sendpacket: wrote %s %d chars, ret=%d\n",
		    inet_ntop(sin6->sin6_family,
		    (void *)&sin6->sin6_addr,
		    abuf, sizeof (abuf)),
		    size, cc);
	}
}

/* Send a Router Solicitation */
static void
solicit(struct sockaddr_in6 *sin6, struct phyint *pi)
{
	int packetlen = 0;
	struct	nd_router_solicit *rs = (struct nd_router_solicit *)packet;
	char *pptr = (char *)packet;

	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	rs->nd_rs_code = 0;
	rs->nd_rs_cksum = htons(0);
	rs->nd_rs_reserved = htonl(0);

	packetlen += sizeof (*rs);
	pptr += sizeof (*rs);

	/* Attach any options */
	if (pi->pi_hdw_addr_len != 0) {
		struct nd_opt_lla *lo = (struct nd_opt_lla *)pptr;
		int optlen;

		/* roundup to multiple of 8 and make padding zero */
		optlen = ((sizeof (struct nd_opt_hdr) +
		    pi->pi_hdw_addr_len + 7) / 8) * 8;
		bzero(pptr, optlen);

		lo->nd_opt_lla_type = ND_OPT_SOURCE_LINKADDR;
		lo->nd_opt_lla_len = optlen / 8;
		bcopy((char *)pi->pi_hdw_addr,
		    (char *)lo->nd_opt_lla_hdw_addr,
		    pi->pi_hdw_addr_len);
		packetlen += optlen;
		pptr += optlen;
	}

	if (debug & D_PKTOUT) {
		print_route_sol("Sending solicitation to ", pi, rs, packetlen,
		    sin6);
	}
	sendpacket(sin6, pi->pi_sock, packetlen, 0);
}

/*
 * Send a (set of) Router Advertisements and feed them back to ourselves
 * for processing. Unless no_prefixes is set all prefixes are included.
 * If there are too many prefix options to fit in one packet multiple
 * packets will be sent - each containing a subset of the prefix options.
 */
static void
advertise(struct sockaddr_in6 *sin6, struct phyint *pi, boolean_t no_prefixes)
{
	struct	nd_opt_prefix_info *po;
	char *pptr = (char *)packet;
	struct nd_router_advert *ra;
	struct prefix *pr;
	int packetlen = 0;

	ra = (struct nd_router_advert *)pptr;
	ra->nd_ra_type = ND_ROUTER_ADVERT;
	ra->nd_ra_code = 0;
	ra->nd_ra_cksum = htons(0);
	ra->nd_ra_curhoplimit = pi->pi_AdvCurHopLimit;
	ra->nd_ra_flags_reserved = 0;
	if (pi->pi_AdvManagedFlag)
		ra->nd_ra_flags_reserved |= ND_RA_FLAG_MANAGED;
	if (pi->pi_AdvOtherConfigFlag)
		ra->nd_ra_flags_reserved |= ND_RA_FLAG_OTHER;

	if (pi->pi_adv_state == FINAL_ADV)
		ra->nd_ra_router_lifetime = htons(0);
	else
		ra->nd_ra_router_lifetime = htons(pi->pi_AdvDefaultLifetime);
	ra->nd_ra_reachable = htonl(pi->pi_AdvReachableTime);
	ra->nd_ra_retransmit = htonl(pi->pi_AdvRetransTimer);

	packetlen = sizeof (*ra);
	pptr += sizeof (*ra);

	if (pi->pi_adv_state == FINAL_ADV) {
		if (debug & D_PKTOUT) {
			print_route_adv("Sending advert (FINAL) to ", pi,
			    ra, packetlen, sin6);
		}
		sendpacket(sin6, pi->pi_sock, packetlen, 0);
		/* Feed packet back in for router operation */
		loopback_ra_enqueue(pi, ra, packetlen);
		return;
	}

	/* Attach any options */
	if (pi->pi_hdw_addr_len != 0) {
		struct nd_opt_lla *lo = (struct nd_opt_lla *)pptr;
		int optlen;

		/* roundup to multiple of 8 and make padding zero */
		optlen = ((sizeof (struct nd_opt_hdr) +
		    pi->pi_hdw_addr_len + 7) / 8) * 8;
		bzero(pptr, optlen);

		lo->nd_opt_lla_type = ND_OPT_SOURCE_LINKADDR;
		lo->nd_opt_lla_len = optlen / 8;
		bcopy((char *)pi->pi_hdw_addr,
		    (char *)lo->nd_opt_lla_hdw_addr,
		    pi->pi_hdw_addr_len);
		packetlen += optlen;
		pptr += optlen;
	}

	if (pi->pi_AdvLinkMTU != 0) {
		struct nd_opt_mtu *mo = (struct nd_opt_mtu *)pptr;

		mo->nd_opt_mtu_type = ND_OPT_MTU;
		mo->nd_opt_mtu_len = sizeof (struct nd_opt_mtu) / 8;
		mo->nd_opt_mtu_reserved = 0;
		mo->nd_opt_mtu_mtu = htonl(pi->pi_AdvLinkMTU);

		packetlen += sizeof (struct nd_opt_mtu);
		pptr += sizeof (struct nd_opt_mtu);
	}

	if (!no_prefixes) {
		po = (struct nd_opt_prefix_info *)pptr;
		for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
			if (!pr->pr_AdvOnLinkFlag && !pr->pr_AdvAutonomousFlag)
				continue;

			/*
			 * If the prefix doesn't fit in packet send
			 * what we have so far and start with new packet.
			 */
			if (packetlen + sizeof (*po) >
			    pi->pi_LinkMTU - sizeof (struct ip6_hdr)) {
				if (debug & D_PKTOUT) {
					print_route_adv("Sending advert "
					    "(FRAG) to ",
					    pi, ra, packetlen, sin6);
				}
				sendpacket(sin6, pi->pi_sock, packetlen, 0);
				/* Feed packet back in for router operation */
				loopback_ra_enqueue(pi, ra, packetlen);
				packetlen = sizeof (*ra);
				pptr = (char *)packet + sizeof (*ra);
				po = (struct nd_opt_prefix_info *)pptr;
			}
			po->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
			po->nd_opt_pi_len = sizeof (*po)/8;
			po->nd_opt_pi_flags_reserved = 0;
			if (pr->pr_AdvOnLinkFlag) {
				po->nd_opt_pi_flags_reserved |=
				    ND_OPT_PI_FLAG_ONLINK;
			}
			if (pr->pr_AdvAutonomousFlag) {
				po->nd_opt_pi_flags_reserved |=
				    ND_OPT_PI_FLAG_AUTO;
			}
			po->nd_opt_pi_prefix_len = pr->pr_prefix_len;
			/*
			 * If both Adv*Expiration and Adv*Lifetime are
			 * set we prefer the former and make the lifetime
			 * decrement in real time.
			 */
			if (pr->pr_AdvValidRealTime) {
				po->nd_opt_pi_valid_time =
				    htonl(pr->pr_AdvValidExpiration);
			} else {
				po->nd_opt_pi_valid_time =
				    htonl(pr->pr_AdvValidLifetime);
			}
			if (pr->pr_AdvPreferredRealTime) {
				po->nd_opt_pi_preferred_time =
				    htonl(pr->pr_AdvPreferredExpiration);
			} else {
				po->nd_opt_pi_preferred_time =
				    htonl(pr->pr_AdvPreferredLifetime);
			}
			po->nd_opt_pi_reserved2 = htonl(0);
			po->nd_opt_pi_prefix = pr->pr_prefix;

			po++;
			packetlen += sizeof (*po);
		}
	}
	if (debug & D_PKTOUT) {
		print_route_adv("Sending advert to ", pi,
		    ra, packetlen, sin6);
	}
	sendpacket(sin6, pi->pi_sock, packetlen, 0);
	/* Feed packet back in for router operation */
	loopback_ra_enqueue(pi, ra, packetlen);
}

/* Poll support */
static int		pollfd_num = 0;	/* Allocated and initialized */
static struct pollfd	*pollfds = NULL;

/*
 * Add fd to the set being polled. Returns 0 if ok; -1 if failed.
 */
int
poll_add(int fd)
{
	int i;
	int new_num;
	struct pollfd *newfds;
retry:
	/* Check if already present */
	for (i = 0; i < pollfd_num; i++) {
		if (pollfds[i].fd == fd)
			return (0);
	}
	/* Check for empty spot already present */
	for (i = 0; i < pollfd_num; i++) {
		if (pollfds[i].fd == -1) {
			pollfds[i].fd = fd;
			return (0);
		}
	}

	/* Allocate space for 32 more fds and initialize to -1 */
	new_num = pollfd_num + 32;
	newfds = realloc(pollfds, new_num * sizeof (struct pollfd));
	if (newfds == NULL) {
		logperror("poll_add: realloc");
		return (-1);
	}
	for (i = pollfd_num; i < new_num; i++) {
		newfds[i].fd = -1;
		newfds[i].events = POLLIN;
	}
	pollfd_num = new_num;
	pollfds = newfds;
	goto retry;
}

/*
 * Remove fd from the set being polled. Returns 0 if ok; -1 if failed.
 */
int
poll_remove(int fd)
{
	int i;

	/* Check if already present */
	for (i = 0; i < pollfd_num; i++) {
		if (pollfds[i].fd == fd) {
			pollfds[i].fd = -1;
			return (0);
		}
	}
	return (-1);
}

/*
 * Extract information about the ifname (either a physical interface and
 * the ":0" logical interface or just a logical interface).
 * If the interface (still) exists in kernel set pi_in_use/pr_in_use
 * for caller to be able to detect interfaces that are removed.
 * Starts sending advertisements/solicitations when new physical interfaces
 * are detected.
 */
static void
if_process(int s, char *ifname, boolean_t first)
{
	struct lifreq lifr;
	struct phyint *pi;
	struct prefix *pr;
	char *cp;
	char phyintname[LIFNAMSIZ + 1];

	if (debug & D_IFSCAN)
		logdebug("if_process(%s)\n", ifname);

	(void) strncpy(lifr.lifr_name, ifname, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(s, SIOCGLIFFLAGS, (char *)&lifr) < 0) {
		if (errno == ENXIO) {
			/*
			 * Interface has disappeared - pi_in_use will make
			 * caller clean up.
			 */
			return;
		}
		logperror("if_process: ioctl (get interface flags)");
		return;
	}

	/*
	 * Ignore loopback and point-to-multipoint interfaces.
	 * Point-to-point interfaces always have IFF_MULTICAST set.
	 */
	if (!(lifr.lifr_flags & IFF_MULTICAST) ||
	    (lifr.lifr_flags & IFF_LOOPBACK))
		return;

	if (!(lifr.lifr_flags & IFF_IPV6))
		return;

	(void) strncpy(phyintname, ifname, sizeof (phyintname));
	phyintname[sizeof (phyintname) - 1] = '\0';
	if ((cp = strchr(phyintname, IF_SEPARATOR)) != NULL) {
		*cp = '\0';
	}

	pi = phyint_lookup(phyintname);
	if (pi == NULL) {
		/*
		 * Do not add anything for new interfaces until they are UP.
		 * For existing interfaces we track the up flag.
		 */
		if (!(lifr.lifr_flags & IFF_UP))
			return;

		pi = phyint_create(phyintname);
		if (pi == NULL) {
			logerr("if_process: out of memory\n");
			return;
		}
	}
	(void) phyint_init_from_k(pi);
	if (pi->pi_sock == -1 && !(pi->pi_kernel_state & PI_PRESENT)) {
		/* Interface is not yet present */
		if (debug & D_PHYINT) {
			logdebug("if_process: interface not yet present %s\n",
			    pi->pi_name);
		}
		return;
	}

	if (pi->pi_sock != -1) {
		if (poll_add(pi->pi_sock) == -1) {
			(void) close(pi->pi_sock);
			pi->pi_sock = -1;
			return;
		}
	}
	/*
	 * If interface (still) exists in kernel set pi_in_use and pi_state
	 * for caller
	 */
	if (pi->pi_kernel_state & PI_PRESENT) {
		pi->pi_in_use = _B_TRUE;
		pi->pi_state |= PI_PRESENT;
	}

	/*
	 * Check if IFF_ROUTER has been turned off in kernel in which
	 * case we have to turn off AdvSendAdvertisements.
	 * The kernel will automatically turn off IFF_ROUTER if
	 * ip6_forwarding is turned off.
	 * Note that we do not switch back should IFF_ROUTER be turned on.
	 */
	if (!first &&
	    pi->pi_AdvSendAdvertisements && !(pi->pi_flags & IFF_ROUTER)) {
		logtrace("No longer a router on %s\n", pi->pi_name);
		check_to_advertise(pi, START_FINAL_ADV);

		pi->pi_AdvSendAdvertisements = 0;
		pi->pi_sol_state = NO_SOLICIT;
	}
	if (pi->pi_AdvSendAdvertisements) {
		if (pi->pi_adv_state == NO_ADV)
			check_to_advertise(pi, START_INIT_ADV);
	} else {
		if (pi->pi_sol_state == NO_SOLICIT)
			check_to_solicit(pi, START_INIT_SOLICIT);
	}

	/*
	 * Track static kernel prefixes to prevent in.ndpd from clobbering
	 * them by creating a struct prefix for each prefix detected in the
	 * kernel.
	 */
	pr = prefix_lookup_name(pi, ifname);
	if (pr == NULL) {
		pr = prefix_create_name(pi, ifname);
		if (pr == NULL) {
			logerr("if_process: out of memory\n");
			return;
		}
		if (prefix_init_from_k(pr) == -1) {
			prefix_delete(pr);
			return;
		}
	}
	/* Detect prefixes which are removed */
	if (pr->pr_kernel_state != 0)
		pr->pr_in_use = _B_TRUE;
}

static int ifsock = -1;

/*
 * Scan all interfaces to detect changes as well as new and deleted intefaces
 * 'first' is set for the initial call only. Do not effect anything.
 */
static void
initifs(boolean_t first)
{
	char *buf;
	int bufsize;
	int numifs;
	int n;
	struct lifnum lifn;
	struct lifconf lifc;
	struct lifreq *lifr;
	struct phyint *pi;
	struct phyint *next_pi;
	struct prefix *pr;

	if (debug & D_IFSCAN)
		logdebug("Reading interface configuration\n");
	if (ifsock < 0) {
		ifsock = socket(AF_INET6, SOCK_DGRAM, 0);
		if (ifsock < 0) {
			logperror("initifs: socket");
			return;
		}
	}
	lifn.lifn_family = AF_INET6;
	lifn.lifn_flags = LIFC_NOXMIT;
	if (ioctl(ifsock, SIOCGLIFNUM, (char *)&lifn) < 0) {
		logperror("initifs: ioctl (get interface numbers)");
		return;
	}
	numifs = lifn.lifn_count;
	bufsize = numifs * sizeof (struct lifreq);

	buf = (char *)malloc(bufsize);
	if (buf == NULL) {
		logerr("initifs: out of memory\n");
		return;
	}

	/*
	 * Mark the interfaces so that we can find phyints and prefixes
	 * which have disappeared from the kernel.
	 * if_process will set {pi,pr}_in_use when it finds the interface
	 * in the kernel.
	 */
	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		pi->pi_in_use = _B_FALSE;
		for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
			pr->pr_in_use = _B_FALSE;
		}
	}

	lifc.lifc_family = AF_INET6;
	lifc.lifc_flags = LIFC_NOXMIT;
	lifc.lifc_len = bufsize;
	lifc.lifc_buf = buf;

	if (ioctl(ifsock, SIOCGLIFCONF, (char *)&lifc) < 0) {
		logperror("initifs: ioctl (get interface configuration)");
		free(buf);
		return;
	}

	lifr = (struct lifreq *)lifc.lifc_req;
	for (n = lifc.lifc_len / sizeof (struct lifreq); n > 0; n--, lifr++)
		if_process(ifsock, lifr->lifr_name, first);
	free(buf);

	/*
	 * Detect phyints that have been removed from the kernel.
	 * Since we can't recreate it here (would require ifconfig plumb
	 * logic) we just terminate use of that phyint.
	 */
	for (pi = phyints; pi != NULL; pi = next_pi) {
		next_pi = pi->pi_next;
		check_if_removed(pi);
	}
	if (show_ifs)
		phyint_print_all();
}


/*
 * Router advertisement state machine. Used for everything but timer
 * events which use advertise_event directly.
 */
void
check_to_advertise(struct phyint *pi, enum adv_events event)
{
	uint_t delay;
	enum adv_states old_state = pi->pi_adv_state;

	if (debug & D_STATE) {
		logdebug("check_to_advertise(%s, %d) state %d\n",
		    pi->pi_name, (int)event, (int)old_state);
	}
	delay = advertise_event(pi, event, 0);
	if (delay != TIMER_INFINITY) {
		/* Make sure the global next event is updated */
		timer_schedule(delay);
	}

	if (debug & D_STATE) {
		logdebug("check_to_advertise(%s, %d) state %d -> %d\n",
		    pi->pi_name, (int)event, (int)old_state,
		    (int)pi->pi_adv_state);
	}
}

/*
 * Router advertisement state machine.
 * Return the number of milliseconds until next timeout (TIMER_INFINITY
 * if never).
 * For the ADV_TIMER event the caller passes in the number of milliseconds
 * since the last timer event in the 'elapsed' parameter.
 */
uint_t
advertise_event(struct phyint *pi, enum adv_events event, uint_t elapsed)
{
	uint_t delay;

	if (debug & D_STATE) {
		logdebug("advertise_event(%s, %d, %d) state %d\n",
		    pi->pi_name, (int)event, elapsed, (int)pi->pi_adv_state);
	}
	if (!pi->pi_AdvSendAdvertisements)
		return (TIMER_INFINITY);
	if (pi->pi_flags & IFF_NORTEXCH) {
		if (debug & D_PKTOUT) {
			logdebug("Suppress sending RA packet on %s "
			    "(no route exchange on interface)\n",
			    pi->pi_name);
		}
		return (TIMER_INFINITY);
	}

	switch (event) {
	case ADV_OFF:
		pi->pi_adv_state = NO_ADV;
		return (TIMER_INFINITY);

	case START_INIT_ADV:
		if (pi->pi_adv_state == INIT_ADV)
			return (pi->pi_adv_time_left);
		pi->pi_adv_count = ND_MAX_INITIAL_RTR_ADVERTISEMENTS;
		pi->pi_adv_time_left = 0;
		pi->pi_adv_state = INIT_ADV;
		break;	/* send advertisement */

	case START_FINAL_ADV:
		if (pi->pi_adv_state == NO_ADV)
			return (TIMER_INFINITY);
		if (pi->pi_adv_state == FINAL_ADV)
			return (pi->pi_adv_time_left);
		pi->pi_adv_count = ND_MAX_FINAL_RTR_ADVERTISEMENTS;
		pi->pi_adv_time_left = 0;
		pi->pi_adv_state = FINAL_ADV;
		break;	/* send advertisement */

	case RECEIVED_SOLICIT:
		if (pi->pi_adv_state == NO_ADV)
			return (TIMER_INFINITY);
		if (pi->pi_adv_state == SOLICIT_ADV) {
			if (pi->pi_adv_time_left != 0)
				return (pi->pi_adv_time_left);
			break;
		}
		delay = GET_RANDOM(0, ND_MAX_RA_DELAY_TIME);
		if (delay < pi->pi_adv_time_left)
			pi->pi_adv_time_left = delay;
		if (pi->pi_adv_time_since_sent < ND_MIN_DELAY_BETWEEN_RAS) {
			/*
			 * Send an advertisement (ND_MIN_DELAY_BETWEEN_RAS
			 * plus random delay) after the previous
			 * advertisement was sent.
			 */
			pi->pi_adv_time_left = delay +
			    ND_MIN_DELAY_BETWEEN_RAS -
			    pi->pi_adv_time_since_sent;
		}
		pi->pi_adv_state = SOLICIT_ADV;
		break;

	case ADV_TIMER:
		if (pi->pi_adv_state == NO_ADV)
			return (TIMER_INFINITY);
		/* Decrease time left */
		if (pi->pi_adv_time_left >= elapsed)
			pi->pi_adv_time_left -= elapsed;
		else
			pi->pi_adv_time_left = 0;

		/* Increase time since last advertisement was sent */
		pi->pi_adv_time_since_sent += elapsed;
		break;
	default:
		logerr("advertise_event: Unknown event %d\n", (int)event);
		return (TIMER_INFINITY);
	}

	if (pi->pi_adv_time_left != 0)
		return (pi->pi_adv_time_left);

	/* Send advertisement and calculate next time to send */
	if (pi->pi_adv_state == FINAL_ADV) {
		/* Omit the prefixes */
		advertise(&v6allnodes, pi, _B_TRUE);
	} else {
		advertise(&v6allnodes, pi, _B_FALSE);
	}
	pi->pi_adv_time_since_sent = 0;

	switch (pi->pi_adv_state) {
	case SOLICIT_ADV:
		/*
		 * The solicited advertisement has been sent.
		 * Revert to periodic advertisements.
		 */
		pi->pi_adv_state = REG_ADV;
		/* FALLTHRU */
	case REG_ADV:
		pi->pi_adv_time_left =
		    GET_RANDOM(1000 * pi->pi_MinRtrAdvInterval,
		    1000 * pi->pi_MaxRtrAdvInterval);
		break;

	case INIT_ADV:
		if (--pi->pi_adv_count > 0) {
			delay = GET_RANDOM(1000 * pi->pi_MinRtrAdvInterval,
			    1000 * pi->pi_MaxRtrAdvInterval);
			if (delay > ND_MAX_INITIAL_RTR_ADVERT_INTERVAL)
				delay = ND_MAX_INITIAL_RTR_ADVERT_INTERVAL;
			pi->pi_adv_time_left = delay;
		} else {
			pi->pi_adv_time_left =
			    GET_RANDOM(1000 * pi->pi_MinRtrAdvInterval,
			    1000 * pi->pi_MaxRtrAdvInterval);
			pi->pi_adv_state = REG_ADV;
		}
		break;

	case FINAL_ADV:
		if (--pi->pi_adv_count > 0) {
			pi->pi_adv_time_left =
			    ND_MAX_INITIAL_RTR_ADVERT_INTERVAL;
		} else {
			pi->pi_adv_state = NO_ADV;
		}
		break;
	}
	if (pi->pi_adv_state != NO_ADV)
		return (pi->pi_adv_time_left);
	else
		return (TIMER_INFINITY);
}

/*
 * Router solicitation state machine. Used for everything but timer
 * events which use solicit_event directly.
 */
void
check_to_solicit(struct phyint *pi, enum solicit_events event)
{
	uint_t delay;
	enum solicit_states old_state = pi->pi_sol_state;

	if (debug & D_STATE) {
		logdebug("check_to_solicit(%s, %d) state %d\n",
		    pi->pi_name, (int)event, (int)old_state);
	}
	delay = solicit_event(pi, event, 0);
	if (delay != TIMER_INFINITY) {
		/* Make sure the global next event is updated */
		timer_schedule(delay);
	}

	if (debug & D_STATE) {
		logdebug("check_to_solicit(%s, %d) state %d -> %d\n",
		    pi->pi_name, (int)event, (int)old_state,
		    (int)pi->pi_sol_state);
	}
}

/*
 * Router solicitation state machine.
 * Return the number of milliseconds until next timeout (TIMER_INFINITY
 * if never).
 * For the SOL_TIMER event the caller passes in the number of milliseconds
 * since the last timer event in the 'elapsed' parameter.
 */
uint_t
solicit_event(struct phyint *pi, enum solicit_events event, uint_t elapsed)
{
	if (debug & D_STATE) {
		logdebug("solicit_event(%s, %d, %d) state %d\n",
		    pi->pi_name, (int)event, elapsed, (int)pi->pi_sol_state);
	}

	if (pi->pi_AdvSendAdvertisements)
		return (TIMER_INFINITY);
	if (pi->pi_flags & IFF_NORTEXCH) {
		if (debug & D_PKTOUT) {
			logdebug("Suppress sending RS packet on %s "
			    "(no route exchange on interface)\n",
			    pi->pi_name);
		}
		return (TIMER_INFINITY);
	}

	switch (event) {
	case SOLICIT_OFF:
		pi->pi_sol_state = NO_SOLICIT;
		return (TIMER_INFINITY);

	case SOLICIT_DONE:
		pi->pi_sol_state = DONE_SOLICIT;
		return (TIMER_INFINITY);

	case START_INIT_SOLICIT:
		if (pi->pi_sol_state == INIT_SOLICIT)
			return (pi->pi_sol_time_left);
		pi->pi_sol_count = ND_MAX_RTR_SOLICITATIONS;
		pi->pi_sol_time_left =
		    GET_RANDOM(0, ND_MAX_RTR_SOLICITATION_DELAY);
		pi->pi_sol_state = INIT_SOLICIT;
		break;

	case SOL_TIMER:
		if (pi->pi_sol_state == NO_SOLICIT)
			return (TIMER_INFINITY);
		/* Decrease time left */
		if (pi->pi_sol_time_left >= elapsed)
			pi->pi_sol_time_left -= elapsed;
		else
			pi->pi_sol_time_left = 0;
		break;
	default:
		logerr("solicit_event: Unknown event %d\n", (int)event);
		return (TIMER_INFINITY);
	}

	if (pi->pi_sol_time_left != 0)
		return (pi->pi_sol_time_left);

	/* Send solicitation and calculate next time */
	switch (pi->pi_sol_state) {
	case INIT_SOLICIT:
		solicit(&v6allrouters, pi);
		if (--pi->pi_sol_count == 0) {
			pi->pi_sol_state = DONE_SOLICIT;
			/* check state file for saved prefixes */
			check_fallback();
			return (TIMER_INFINITY);
		}
		pi->pi_sol_time_left = ND_RTR_SOLICITATION_INTERVAL;
		return (pi->pi_sol_time_left);
	case NO_SOLICIT:
	case DONE_SOLICIT:
		return (TIMER_INFINITY);
	default:
		return (pi->pi_sol_time_left);
	}
}

/*
 * If no interfaces are advertising and have gone to DONE_SOLICIT
 * and there are no default routers:
 *	We add an "everything is on-link" default route if there
 *	is only one non-point-to-point phyint in the kernel.
 *	XXX What to do when there are multiple phyints?
 *	XXX The router_delete_onlink checks only operate on one phyint!
 *
 * If a particular interface is soliciting and have gone to DONE_SOLICIT
 * and no received prefixes has caused us to create an address:
 *	We inspect the state file for that interface to look if there
 *	is a prefix with remaining lifetime.
 */
void
check_fallback(void)
{
	struct phyint *pi;
	struct prefix *pr;
	struct router *dr;
	boolean_t add_default;
	int	num_phyints;
	int	num_addrconf_prefixes;

	if (debug & D_PREFIX) {
		logdebug("check_fallback()\n");
	}
	add_default = _B_TRUE;
	num_phyints = 0;
	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		if (pi->pi_AdvSendAdvertisements ||
		    pi->pi_sol_state != DONE_SOLICIT) {
			add_default = _B_FALSE;
			break;
		}
		if (!(pi->pi_kernel_state & PI_PRESENT))
			continue;

		if (!(pi->pi_flags & IFF_POINTOPOINT))
			num_phyints++;
		for (dr = pi->pi_router_list; dr != NULL; dr = dr->dr_next) {
			if (dr->dr_inkernel) {
				add_default = _B_FALSE;
				break;
			}
		}
		if (!add_default)
			break;
	}
	if (num_phyints == 1 && add_default) {
		if (debug & D_ROUTER) {
			logdebug("check_fallback: create default router\n");
		}
		for (pi = phyints; pi != NULL; pi = pi->pi_next) {
			if (!(pi->pi_kernel_state & PI_PRESENT))
				continue;
			if (debug & D_ROUTER) {
				logdebug("check_fallback: %s default router\n",
				    pi->pi_name);
			}
			if (!(pi->pi_flags & IFF_POINTOPOINT))
				(void) router_create_onlink(pi);
		}
	}

	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		if (pi->pi_AdvSendAdvertisements ||
		    pi->pi_sol_state != DONE_SOLICIT)
			continue;

		num_addrconf_prefixes = 0;
		for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
			/* Skip link-local address */
			if ((pr->pr_state & PR_AUTO) &&
			    !IN6_IS_ADDR_LINKLOCAL(&pr->pr_prefix))
				num_addrconf_prefixes++;
		}
		if (debug & D_PREFIX) {
			logdebug("check_fallback: %d prefixes on %s\n",
			    num_addrconf_prefixes, pi->pi_name);
		}
		if (num_addrconf_prefixes == 0) {
			/* Add the prefixes that are in the state file */
			phyint_read_state_file(pi);
		}
	}
}

/*
 * Timer mechanism using relative time (in milliseconds) from the
 * previous timer event. Timers exceeding TIMER_INFINITY milliseconds
 * will fire after TIMER_INFINITY milliseconds.
 */
static uint_t timer_previous;	/* When last SIGALRM occurred */
static uint_t timer_next;	/* Currently scheduled timeout */

static void
timer_init(void)
{
	timer_previous = getcurrenttime();
	timer_next = TIMER_INFINITY;
	run_timeouts();
}

/*
 * Make sure the next SIGALRM occurs delay milliseconds from the current
 * time if not earlier.
 * Handles getcurrenttime (32 bit integer holding milliseconds) wraparound
 * by treating differences greater than 0x80000000 as negative.
 */
void
timer_schedule(uint_t delay)
{
	uint_t now;
	struct itimerval itimerval;

	now = getcurrenttime();
	if (debug & D_TIMER) {
		logdebug("timer_schedule(%u): now %u next %u\n",
		    delay, now, timer_next);
	}
	/* Will this timer occur before the currently scheduled SIGALRM? */
	if (delay >= timer_next - now) {
		if (debug & D_TIMER) {
			logdebug("timer_schedule(%u): no action - "
			    "next in %u ms\n",
			    delay, timer_next - now);
		}
		return;
	}
	if (delay == 0) {
		/* Minimum allowed delay */
		delay = 1;
	}
	timer_next = now + delay;

	itimerval.it_value.tv_sec = delay / 1000;
	itimerval.it_value.tv_usec = (delay % 1000) * 1000;
	itimerval.it_interval.tv_sec = 0;
	itimerval.it_interval.tv_usec = 0;
	if (debug & D_TIMER) {
		logdebug("timer_schedule(%u): sec %u usec %u\n",
		    delay,
		    itimerval.it_value.tv_sec, itimerval.it_value.tv_usec);
	}
	if (setitimer(ITIMER_REAL, &itimerval, NULL) < 0) {
		logperror("timer_schedule: setitimer");
		exit(2);
	}
}

/*
 * Conditional running of timer. If more than 'minimal_time' millseconds
 * since the timer routines were last run we run them.
 * Used when packets arrive.
 */
static void
conditional_run_timeouts(uint_t minimal_time)
{
	uint_t now;
	uint_t elapsed;

	now = getcurrenttime();
	elapsed = now - timer_previous;
	if (elapsed > minimal_time) {
		if (debug & D_TIMER) {
			logdebug("conditional_run_timeouts: elapsed %d\n",
			    elapsed);
		}
		run_timeouts();
	}
}

/*
 * Timer has fired.
 * Determine when the next timer event will occur by asking all
 * the timer routines.
 * Should not be called from a timer routine but in some cases this is
 * done because the code doesn't know that e.g. it was called from
 * ifconfig_timer(). In this case the nested run_timeouts will just return but
 * the running run_timeouts will ensure to call all the timer functions by
 * looping once more.
 */
static void
run_timeouts(void)
{
	uint_t now;
	uint_t elapsed;
	uint_t next;
	uint_t nexti;
	struct phyint *pi;
	struct phyint *next_pi;
	struct prefix *pr;
	struct prefix *next_pr;
	struct router *dr;
	struct router *next_dr;
	static boolean_t timeout_running;
	static boolean_t do_retry;

	if (timeout_running) {
		if (debug & D_TIMER)
			logdebug("run_timeouts: nested call\n");
		do_retry = _B_TRUE;
		return;
	}
	timeout_running = _B_TRUE;
retry:
	/* How much time since the last time we were called? */
	now = getcurrenttime();
	elapsed = now - timer_previous;
	timer_previous = now;

	if (debug & D_TIMER)
		logdebug("run_timeouts: elapsed %d\n", elapsed);

	next = TIMER_INFINITY;
	for (pi = phyints; pi != NULL; pi = next_pi) {
		next_pi = pi->pi_next;
		nexti = phyint_timer(pi, elapsed);
		if (nexti != TIMER_INFINITY && nexti < next)
			next = nexti;
		if (debug & D_TIMER) {
			logdebug("run_timeouts (pi %s): %d -> %u ms\n",
			    pi->pi_name, nexti, next);
		}
		for (pr = pi->pi_prefix_list; pr != NULL; pr = next_pr) {
			next_pr = pr->pr_next;
			nexti = prefix_timer(pr, elapsed);
			if (nexti != TIMER_INFINITY && nexti < next)
				next = nexti;
			if (debug & D_TIMER) {
				logdebug("run_timeouts (pr %s): %d -> %u ms\n",
				    pr->pr_name, nexti, next);
			}
		}
		for (dr = pi->pi_router_list; dr != NULL; dr = next_dr) {
			next_dr = dr->dr_next;
			nexti = router_timer(dr, elapsed);
			if (nexti != TIMER_INFINITY && nexti < next)
				next = nexti;
			if (debug & D_TIMER) {
				logdebug("run_timeouts (dr): %d -> %u ms\n",
				    nexti, next);
			}
		}
	}
	/*
	 * Make sure the timer functions are run at least once
	 * an hour.
	 */
	if (next == TIMER_INFINITY)
		next = 3600 * 1000;	/* 1 hour */

	if (debug & D_TIMER)
		logdebug("run_timeouts: %u ms\n", next);
	timer_schedule(next);
	if (do_retry) {
		if (debug & D_TIMER)
			logdebug("run_timeouts: retry\n");
		do_retry = _B_FALSE;
		goto retry;
	}
	timeout_running = _B_FALSE;
}

static int eventpipe_read = -1;	/* Used for synchronous signal delivery */
static int eventpipe_write = -1;

/*
 * Ensure that signals are processed synchronously with the rest of
 * the code by just writing a one character signal number on the pipe.
 * The poll loop will pick this up and process the signal event.
 */
static void
sig_handler(int signo)
{
	uchar_t buf = (uchar_t)signo;

	if (eventpipe_write == -1) {
		logerr("sig_handler: no pipe\n");
		return;
	}
	if (write(eventpipe_write, &buf, sizeof (buf)) < 0)
		logperror("sig_handler: write");
}

/*
 * Pick up a signal "byte" from the pipe and process it.
 */
static void
in_signal(int fd)
{
	uchar_t buf;
	struct phyint *pi;
	struct phyint *next_pi;

	switch (read(fd, &buf, sizeof (buf))) {
	case -1:
		logperror("in_signal: read");
		exit(1);
		/* NOTREACHED */
	case 1:
		break;
	case 0:
		logerr("in_signal: read eof\n");
		exit(1);
		/* NOTREACHED */
	default:
		logerr("in_signal: read > 1\n");
		exit(1);
	}

	if (debug & D_TIMER)
		logdebug("in_signal() got %d\n", buf);

	switch (buf) {
	case SIGALRM:
		if (debug & D_TIMER) {
			uint_t now = getcurrenttime();

			logdebug("in_signal(SIGALRM) delta %u\n",
			    now - timer_next);
		}
		timer_next = TIMER_INFINITY;
		run_timeouts();
		break;
	case SIGHUP:
		/* Re-read config file by exec'ing ourselves */
		for (pi = phyints; pi != NULL; pi = next_pi) {
			next_pi = pi->pi_next;
			if (pi->pi_AdvSendAdvertisements)
				check_to_advertise(pi, START_FINAL_ADV);

			phyint_delete(pi);
		}
		logerr("SIGHUP: restart and reread config file\n");
		(void) execv(argv0[0], argv0);
		_exit(0177);
		/* NOTREACHED */
	case SIGUSR1:
		logdebug("Printing configuration:\n");
		phyint_print_all();
		break;
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		for (pi = phyints; pi != NULL; pi = next_pi) {
			next_pi = pi->pi_next;
			if (pi->pi_AdvSendAdvertisements)
				check_to_advertise(pi, START_FINAL_ADV);

			phyint_delete(pi);
		}
		logerr("terminated\n");
		exit(0);
		/* NOTREACHED */
	case 255:
		/*
		 * Special "signal" from looback_ra_enqueue.
		 * Handle any queued loopback router advertisements.
		 */
		loopback_ra_dequeue();
		break;
	default:
		logerr("in_signal: unknown signal: %d\n", buf);
	}
}

/*
 * Create pipe for signal delivery and set up signal handlers.
 */
static void
setup_eventpipe(void)
{
	int fds[2];
	struct sigaction act;

	if ((pipe(fds)) < 0) {
		logperror("setup_eventpipe: pipe");
		exit(1);
	}
	eventpipe_read = fds[0];
	eventpipe_write = fds[1];
	if (poll_add(eventpipe_read) == -1) {
		exit(1);
	}
	act.sa_handler = sig_handler;
	act.sa_flags = SA_RESTART;
	(void) sigaction(SIGALRM, &act, NULL);

	(void) sigset(SIGHUP, sig_handler);
	(void) sigset(SIGUSR1, sig_handler);
	(void) sigset(SIGTERM, sig_handler);
	(void) sigset(SIGINT, sig_handler);
	(void) sigset(SIGQUIT, sig_handler);
}

/*
 * Create a routing socket for receiving RTM_IFINFO messages and initialize
 * the routing socket message header and as much of the sockaddrs as possible.
 */
static int
setup_rtsock(void)
{
	int s;
	char *cp;
	struct sockaddr_in6 *sin6;

	s = socket(PF_ROUTE, SOCK_RAW, AF_INET6);
	if (s == -1) {
		logperror("socket(PF_ROUTE)");
		exit(1);
	}
	if (poll_add(s) == -1) {
		exit(1);
	}

	/*
	 * Allocate storage for the routing socket message.
	 */
	rt_msg = (struct rt_msghdr *)malloc(NDP_RTM_MSGLEN);
	if (rt_msg == NULL) {
		logperror("malloc");
		exit(1);
	}

	/*
	 * Initialize the routing socket message by zero-filling it and then
	 * setting the fields where are constant through the lifetime of the
	 * process.
	 */
	bzero(rt_msg, NDP_RTM_MSGLEN);
	rt_msg->rtm_msglen = NDP_RTM_MSGLEN;
	rt_msg->rtm_version = RTM_VERSION;
	rt_msg->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFP;
	rt_msg->rtm_pid = getpid();
	if (rt_msg->rtm_pid < 0) {
		logperror("getpid");
		exit(1);
	}

	/*
	 * The RTA_DST sockaddr does not change during the lifetime of the
	 * process so it can be completely initialized at this time.
	 */
	cp = (char *)rt_msg + sizeof (struct rt_msghdr);
	sin6 = (struct sockaddr_in6 *)cp;
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = in6addr_any;

	/*
	 * Initialize the constant portion of the RTA_GATEWAY sockaddr.
	 */
	cp += sizeof (struct sockaddr_in6);
	rta_gateway = (struct sockaddr_in6 *)cp;
	rta_gateway->sin6_family = AF_INET6;

	/*
	 * The RTA_NETMASK sockaddr does not change during the lifetime of the
	 * process so it can be completely initialized at this time.
	 */
	cp += sizeof (struct sockaddr_in6);
	sin6 = (struct sockaddr_in6 *)cp;
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = in6addr_any;

	/*
	 * Initialize the constant portion of the RTA_IFP sockaddr.
	 */
	cp += sizeof (struct sockaddr_in6);
	rta_ifp = (struct sockaddr_dl *)cp;
	rta_ifp->sdl_family = AF_LINK;

	return (s);
}

/*
 * Retrieve one routing socket message. If RTM_IFINFO indicates
 * new phyint do a full scan of the interfaces. If RTM_IFINFO
 * indicates an existing phyint only scan that phyint and asociated
 * prefixes.
 */
static void
process_rtsock(int rtsock)
{
	int n;
	int64_t msg[2048 / 8];
	struct rt_msghdr *rtm;
	struct if_msghdr *ifm;
	struct phyint *pi;
	struct prefix *pr;

	n = read(rtsock, msg, sizeof (msg));
	if (n == -1) {
		logperror("process_rtsock: read");
		return;
	}
	rtm = (struct rt_msghdr *)msg;
	if (rtm->rtm_version != RTM_VERSION) {
		logerr("process_rtsock: version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	switch (rtm->rtm_type) {
	case RTM_NEWADDR:
	case RTM_DELADDR:
		/*
		 * Some logical interface has changed - have to scan everything
		 * to determine what actually changed.
		 */
		if (debug & D_IFSCAN) {
			logdebug("process_rtsock: message %d\n",
			    rtm->rtm_type);
		}
		initifs(_B_FALSE);
		return;
	case RTM_IFINFO:
		/* Handled below */
		break;
	default:
		/* Not interesting */
		return;
	}
	ifm = (struct if_msghdr *)rtm;
	if (debug & D_IFSCAN)
		logdebug("process_rtsock: index %d\n", ifm->ifm_index);

	pi = phyint_lookup_on_index(ifm->ifm_index);
	if (pi == NULL) {
		/*
		 * A new physical interface. Do a full scan of the
		 * to catch any new logical interfaces.
		 */
		initifs(_B_FALSE);
		return;
	}
	/*
	 * If any interface flags have changed clear pi_kernel_state
	 * to make sure we pick up new information from the kernel.
	 */
	if (ifm->ifm_flags != pi->pi_flags) {
		if (debug & D_IFSCAN) {
			logdebug("process_rtsock: clr for %s old flags 0x%x "
			    "new flags 0x%x\n", pi->pi_name, pi->pi_flags,
			    ifm->ifm_flags);
		}
		pi->pi_kernel_state &= ~PI_PRESENT;
	}
	/*
	 * Mark the interfaces so that we can find phyints and prefixes
	 * which have disappeared from the kernel.
	 * if_process will set {pi,pr}_in_use when it finds the interface
	 * in the kernel.
	 */
	pi->pi_in_use = _B_FALSE;
	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
		pr->pr_in_use = _B_FALSE;
	}

	if (ifsock < 0) {
		ifsock = socket(AF_INET6, SOCK_DGRAM, 0);
		if (ifsock < 0) {
			logperror("process_rtsock: socket");
			return;
		}
	}
	if_process(ifsock, pi->pi_name, _B_FALSE);
	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
		if_process(ifsock, pr->pr_name, _B_FALSE);
	}
	check_if_removed(pi);
	if (show_ifs)
		phyint_print_all();
}

/*
 * Look if the phyint or one of its prefixes have been removed from
 * the kernel and take appropriate action.
 * Uses {pi,pr}_in_use.
 */
static void
check_if_removed(struct phyint *pi)
{
	struct prefix *pr;
	struct prefix *next_pr;

	/*
	 * Detect phyints that have been removed from the kernel.
	 * Since we can't recreate it here (would require ifconfig plumb
	 * logic) we just terminate use of that phyint.
	 */
	if (!pi->pi_in_use && (pi->pi_state & PI_PRESENT)) {
		logerr("Interface %s has been removed from kernel. "
		    "in.ndpd will no longer use it\n", pi->pi_name);
		/*
		 * Clear state so that should the phyint reappear
		 * we will start with initial advertisements or
		 * solicitations.
		 */
		pi->pi_state &= ~PI_PRESENT;
		pi->pi_kernel_state &= ~PI_PRESENT;
		if (pi->pi_AdvSendAdvertisements) {
			check_to_advertise(pi, ADV_OFF);
		} else {
			check_to_solicit(pi, SOLICIT_OFF);
		}
		(void) poll_remove(pi->pi_sock);
		(void) close(pi->pi_sock);
		pi->pi_sock = -1;
		pi->pi_kernel_state = 0;
	}
	/*
	 * Detect prefixes which are removed.
	 * Static prefixes are just removed from our tables.
	 * Non-static prefixes are recreated i.e. in.ndpd is
	 * taking precedence over a manual ifconfig removing prefixes.
	 */
	for (pr = pi->pi_prefix_list; pr != NULL; pr = next_pr) {
		next_pr = pr->pr_next;
		if (!pr->pr_in_use) {
			/* Clear PR_AUTO and PR_ONLINK */
			pr->pr_kernel_state &= PR_STATIC;
			pr->pr_name[0] = '\0';
			if (pr->pr_state & PR_STATIC) {
				prefix_delete(pr);
			} else if (!(pi->pi_kernel_state & PI_PRESENT)) {
				/*
				 * Ensure that there are no future attempts
				 * do run prefix_update_k since the phyint is
				 * gone.
				 */
				pr->pr_state = pr->pr_kernel_state;
			} else if (pr->pr_state != pr->pr_kernel_state) {
				logtrace("Prefix manually removed "
				    "on %s - recreating it!\n",
				    pi->pi_name);
				prefix_update_k(pr);
			}
		}
	}
}


/*
 * Queuing mechanism for router advertisements that are sent by in.ndpd
 * and that also need to be processed by in.ndpd.
 * Uses "signal number" 255 to indicate to the main poll loop
 * that there is something to dequeue and send to incomining_ra().
 */
struct raq {
	struct raq	*raq_next;
	struct phyint	*raq_pi;
	int		raq_packetlen;
	uchar_t		*raq_packet;
};
static struct raq *raq_head = NULL;

/*
 * Allocate a struct raq and memory for the packet.
 * Send signal 255 to have poll dequeue.
 */
static void
loopback_ra_enqueue(struct phyint *pi, struct nd_router_advert *ra, int len)
{
	struct raq *raq;
	struct raq **raqp;

	if (no_loopback)
		return;

	if (debug & D_PKTOUT)
		logdebug("loopback_ra_enqueue for %s\n", pi->pi_name);

	raq = calloc(sizeof (struct raq), 1);
	if (raq == NULL) {
		logerr("loopback_ra_enqueue: out of memory\n");
		return;
	}
	raq->raq_packet = malloc(len);
	if (raq->raq_packet == NULL) {
		free(raq);
		logerr("loopback_ra_enqueue: out of memory\n");
		return;
	}
	bcopy(ra, raq->raq_packet, len);
	raq->raq_packetlen = len;
	raq->raq_pi = pi;

	/* Tail insert */
	raqp = &raq_head;
	while (*raqp != NULL)
		raqp = &((*raqp)->raq_next);
	*raqp = raq;

	/* Signal for poll loop */
	sig_handler(255);
}

/*
 * Dequeue and process all queued advertisements.
 */
static void
loopback_ra_dequeue(void)
{
	struct sockaddr_in6 from = IN6ADDR_LOOPBACK_INIT;
	struct raq *raq;

	if (debug & D_PKTIN)
		logdebug("loopback_ra_dequeue()\n");

	while ((raq = raq_head) != NULL) {
		raq_head = raq->raq_next;
		raq->raq_next = NULL;

		if (debug & D_PKTIN) {
			logdebug("loopback_ra_dequeue for %s\n",
			    raq->raq_pi->pi_name);
		}

		incoming_ra(raq->raq_pi,
		    (struct nd_router_advert *)raq->raq_packet,
		    raq->raq_packetlen, &from, _B_TRUE);
		free(raq->raq_packet);
		free(raq);
	}
}


static void
usage(char *cmd)
{
	(void) fprintf(stderr,
	    "usage: %s [ -adt ] [-f <config file>]\n", cmd);
}

struct sockaddr_in6 ignoreaddr;

int
main(int argc, char *argv[])
{
	int i;
	struct phyint *pi;
	int c;
	char *config_file = "/etc/inet/ndpd.conf";
	boolean_t file_required = _B_FALSE;

	argv0 = argv;
	srandom(gethostid());
	(void) umask(0022);

	while ((c = getopt(argc, argv, "adD:ntIf:i:")) != EOF) {
		switch (c) {
		case 'a':
			addrconf = _B_FALSE;
			break;
		case 'd':
			debug = D_ALL;
			break;
		case 'D':
			i = strtol((char *)optarg, NULL, 0);
			if (i == 0) {
				(void) fprintf(stderr, "Bad debug flags: %s\n",
				    (char *)optarg);
				exit(1);
			}
			debug |= i;
			break;
		case 'n':
			no_loopback = 1;
			break;
		case 'i': {
			/* Ignore packets from specified IP address */
			struct hostent *hp;
			int error_num;

			hp = getipnodebyname((char *)optarg, AF_INET6, 0,
			    &error_num);
			if (hp == NULL) {
				(void) fprintf(stderr, "Bad address: %s\n",
				    (char *)optarg);
				exit(1);
			}
			ignoreaddr.sin6_family = hp->h_addrtype;
			(void) memcpy((char *)&ignoreaddr.sin6_addr,
			    hp->h_addr,
			    hp->h_length);
			freehostent(hp);
			break;
		}
		case 'I':
			show_ifs = 1;
			break;
		case 't':
			debug |= D_PKTIN | D_PKTOUT | D_PKTBAD;
			break;
		case 'f':
			config_file = (char *)optarg;
			file_required = _B_TRUE;
			break;
		case '?':
			usage(argv[0]);
			exit(1);
		}
	}

	if (parse_config(config_file, file_required) == -1)
		exit(2);

	if (show_ifs)
		phyint_print_all();

	if (debug == 0) {
		switch (fork()) {
		case 0:
			/* Child */
			break;
		case -1:
			logperror("fork");
			exit(1);
		default:
			/* Parent */
			return (0);
		}
		(void) close(0);
		(void) close(1);
		(void) close(2);

		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		(void) setsid();

		initlog();
	}

	setup_eventpipe();
	rtsock = setup_rtsock();
	timer_init();
	initifs(_B_TRUE);

	for (;;) {
		if (poll(pollfds, pollfd_num, -1) < 0) {
			if (errno == EINTR)
				continue;
			logperror("main: poll");
			exit(1);
		}
		for (i = 0; i < pollfd_num; i++) {
			if (!(pollfds[i].revents & POLLIN))
				continue;
			if (pollfds[i].fd == eventpipe_read) {
				in_signal(eventpipe_read);
				break;
			}
			if (pollfds[i].fd == rtsock) {
				process_rtsock(rtsock);
				break;
			}
			/*
			 * Run timer routine to advance clock if more than
			 * half a second since the clock was advanced.
			 * This limits CPU usage under severe packet
			 * arrival rates but it creates a slight inaccuracy
			 * in the timer mechanism.
			 */
			conditional_run_timeouts(500U);
			for (pi = phyints; pi != NULL; pi = pi->pi_next) {
				if (pollfds[i].fd == pi->pi_sock) {
					in_data(pi);
					break;
				}
			}
		}
	}
	/* NOTREACHED */
}

/*
 * LOGGER
 */

#include <syslog.h>

static logging = 0;

static void
initlog(void)
{
	logging++;
	openlog("in.ndpd", LOG_PID | LOG_CONS, LOG_DAEMON);
}

/* Print the date/time without a trailing carridge return */
static void
fprintdate(FILE *file)
{
	char buf[BUFSIZ];
	struct tm tms;
	time_t now;

	now = time(NULL);
	(void) localtime_r(&now, &tms);
	(void) strftime(buf, sizeof (buf), "%h %d %X", &tms);
	(void) fprintf(file, "%s ", buf);
}

void
logerr(char *fmt, ...)
{
	va_list ap;
	/* CSTYLED */
	va_start(ap, );

	if (logging) {
		vsyslog(LOG_ERR, fmt, ap);
	} else {
		fprintdate(stderr);
		(void) vfprintf(stderr, fmt, ap);
	}
	va_end(ap);
}

void
logwarn(char *fmt, ...)
{
	va_list ap;
	/* CSTYLED */
	va_start(ap, );

	if (logging) {
		vsyslog(LOG_WARNING, fmt, ap);
	} else {
		fprintdate(stderr);
		(void) vfprintf(stderr, fmt, ap);
	}
	va_end(ap);
}

void
logtrace(char *fmt, ...)
{
	va_list ap;
	/* CSTYLED */
	va_start(ap, );

	if (logging) {
		vsyslog(LOG_INFO, fmt, ap);
	} else {
		fprintdate(stderr);
		(void) vfprintf(stderr, fmt, ap);
	}
	va_end(ap);
}

void
logdebug(char *fmt, ...)
{
	va_list ap;
	/* CSTYLED */
	va_start(ap, );

	if (logging) {
		vsyslog(LOG_DEBUG, fmt, ap);
	} else {
		fprintdate(stderr);
		(void) vfprintf(stderr, fmt, ap);
	}
	va_end(ap);
}

void
logperror(char *str)
{
	if (logging) {
		syslog(LOG_ERR, "%s: %m\n", str);
	} else {
		fprintdate(stderr);
		(void) fprintf(stderr, "%s: %s\n", str, strerror(errno));
	}
}

void
logperror_pi(struct phyint *pi, char *str)
{
	if (logging) {
		syslog(LOG_ERR, "%s (interface %s): %m\n",
		    str, pi->pi_name);
	} else {
		fprintdate(stderr);
		(void) fprintf(stderr, "%s (interface %s): %s\n",
		    str, pi->pi_name, strerror(errno));
	}
}

void
logperror_pr(struct prefix *pr, char *str)
{
	if (logging) {
		syslog(LOG_ERR, "%s (prefix %s if %s): %m\n",
		    str, pr->pr_name, pr->pr_physical->pi_name);
	} else {
		fprintdate(stderr);
		(void) fprintf(stderr, "%s (prefix %s if %s): %s\n",
		    str, pr->pr_name, pr->pr_physical->pi_name,
		    strerror(errno));
	}
}
