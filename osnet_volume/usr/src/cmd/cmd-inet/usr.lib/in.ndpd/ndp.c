/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ndp.c	1.5	99/12/07 SMI"

#include "defs.h"
#include "tables.h"

static boolean_t verify_opt_len(struct nd_opt_hdr *opt, int optlen,
		    struct phyint *pi, struct sockaddr_in6 *from);

static void	incoming_rs(struct phyint *pi, struct nd_router_solicit *rs,
		    int len, struct sockaddr_in6 *from);

void		incoming_ra(struct phyint *pi, struct nd_router_advert *ra,
		    int len, struct sockaddr_in6 *from, boolean_t loopback);
static void	incoming_prefix_opt(struct phyint *pi, uchar_t *opt,
		    struct sockaddr_in6 *from, boolean_t loopback);
static void	incoming_prefix_onlink(struct phyint *pi, uchar_t *opt,
		    struct sockaddr_in6 *from, boolean_t loopback);
static boolean_t	incoming_prefix_addrconf(struct phyint *pi,
		    uchar_t *opt, struct sockaddr_in6 *from,
		    boolean_t loopback);
static void	incoming_mtu_opt(struct phyint *pi, uchar_t *opt,
		    struct sockaddr_in6 *from);
static void	incoming_lla_opt(struct phyint *pi, uchar_t *opt,
		    struct sockaddr_in6 *from, int isrouter);

static void	verify_ra_consistency(struct phyint *pi,
		    struct nd_router_advert *ra,
		    int len, struct sockaddr_in6 *from);
static void	verify_prefix_opt(struct phyint *pi, uchar_t *opt,
		    char *frombuf);
static void	verify_mtu_opt(struct phyint *pi, uchar_t *opt,
		    char *frombuf);

extern struct sockaddr_in6 ignoreaddr;

static uint_t	ra_flags;	/* Global to detect when to trigger DHCP */

/*
 * Return a pointer to the specified option buffer.
 * If not found return NULL.
 */
static void *
find_ancillary(struct msghdr *msg, int cmsg_type)
{
	struct cmsghdr *cmsg;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IPV6 &&
		    cmsg->cmsg_type == cmsg_type) {
			return (CMSG_DATA(cmsg));
		}
	}
	return (NULL);
}

void
in_data(struct phyint *pi)
{
	struct sockaddr_in6 from;
	struct icmp6_hdr *icmp;
	struct nd_router_solicit *rs;
	struct nd_router_advert *ra;
	static uint64_t in_packet[(IP_MAXPACKET + 1)/8];
	static uint64_t ancillary_data[(IP_MAXPACKET + 1)/8];
	int len;
	char abuf[INET6_ADDRSTRLEN];
	struct msghdr msg;
	struct iovec iov;
	uchar_t *opt;
	uint_t hoplimit;

	iov.iov_base = (char *)in_packet;
	iov.iov_len = sizeof (in_packet);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = (struct sockaddr *)&from;
	msg.msg_namelen = sizeof (from);
	msg.msg_control = ancillary_data;
	msg.msg_controllen = sizeof (ancillary_data);

	if ((len = recvmsg(pi->pi_sock, &msg, 0)) < 0) {
		logperror_pi(pi, "in_data: recvfrom");
		return;
	}
	if (len == 0)
		return;

	if (debug & (D_PKTIN|D_PKTBAD)) {
		(void) inet_ntop(AF_INET6, (void *)&from.sin6_addr,
		    abuf, sizeof (abuf));
	}
	if (IN6_ARE_ADDR_EQUAL(&from.sin6_addr, &ignoreaddr.sin6_addr)) {
		if (debug & D_PKTIN) {
			logdebug("Ignored packet from %s\n", abuf);
		}
		return;
	}
	/* Ignore packets > 64k or control buffers that don't fit */
	if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
		if (debug & D_PKTBAD) {
			logdebug("Truncated message: msg_flags 0x%x from %s\n",
			    msg.msg_flags, abuf);
		}
		return;
	}

	icmp = (struct icmp6_hdr *)in_packet;

	if (len < ICMP6_MINLEN) {
		logtrace("Too short ICMP packet: %d bytes "
		    "from %s on %s\n",
		    len, abuf, pi->pi_name);
		return;
	}

	opt = find_ancillary(&msg, IPV6_HOPLIMIT);
	if (opt == NULL) {
		/* Unknown hoplimit - must drop */
		logtrace("Unknown hop limit from %s on %s\n",
		    abuf, pi->pi_name);
		return;
	}
	hoplimit = *(uint_t *)opt;
	opt = find_ancillary(&msg, IPV6_RTHDR);
	if (opt != NULL) {
		/* Can't allow routing headers in ND messages */
		logtrace("ND message with routing header "
		    "from %s on %s\n",
		    abuf, pi->pi_name);
		return;
	}
	switch (icmp->icmp6_type) {
	case ND_ROUTER_SOLICIT:
		if (!pi->pi_AdvSendAdvertisements)
			return;
		if (pi->pi_flags & IFF_NORTEXCH) {
			if (debug & D_PKTIN) {
				logdebug("Ignore received RS packet on %s "
				    "(no route exchange on interface)\n",
				    pi->pi_name);
			}
			return;
		}

		/*
		 * Assumes that the kernel has verified the AH (if present)
		 * and the ICMP checksum.
		 */
		if (hoplimit != IPV6_MAX_HOPS) {
			logtrace("RS hop limit: %d from %s on %s\n",
			    hoplimit, abuf, pi->pi_name);
			return;
		}

		if (icmp->icmp6_code != 0) {
			logtrace("RS code: %d from %s on %s\n",
			    icmp->icmp6_code, abuf, pi->pi_name);
			return;
		}

		if (len < sizeof (struct nd_router_solicit)) {
			logtrace("RS too short: %d bytes "
			    "from %s on %s\n",
			    len, abuf, pi->pi_name);
			return;
		}
		rs = (struct nd_router_solicit *)icmp;
		if (len > sizeof (struct nd_router_solicit)) {
			if (!verify_opt_len((struct nd_opt_hdr *)&rs[1],
			    len - sizeof (struct nd_router_solicit), pi, &from))
				return;
		}
		if (debug & D_PKTIN) {
			print_route_sol("Received valid solicit from ", pi,
			    rs, len, &from);
		}
		incoming_rs(pi, rs, len, &from);
		break;

	case ND_ROUTER_ADVERT:
		if (pi->pi_flags & IFF_NORTEXCH) {
			if (debug & D_PKTIN) {
				logdebug("Ignore received RA packet on %s "
				    "(no route exchange on interface)\n",
				    pi->pi_name);
			}
			return;
		}

		/*
		 * Assumes that the kernel has verified the AH (if present)
		 * and the ICMP checksum.
		 */
		if (!IN6_IS_ADDR_LINKLOCAL(&from.sin6_addr)) {
			logtrace("RA from %s - not link local on %s\n",
			    abuf, pi->pi_name);
			return;
		}

		if (hoplimit != IPV6_MAX_HOPS) {
			logtrace("RA hop limit: %d from %s on %s\n",
			    hoplimit, abuf, pi->pi_name);
			return;
		}

		if (icmp->icmp6_code != 0) {
			logtrace("RA code: %d from %s on %s\n",
			    icmp->icmp6_code, abuf, pi->pi_name);
			return;
		}

		if (len < sizeof (struct nd_router_advert)) {
			logtrace("RA too short: %d bytes "
			    "from %s on %s\n",
			    len, abuf, pi->pi_name);
			return;
		}
		ra = (struct nd_router_advert *)icmp;
		if (len > sizeof (struct nd_router_advert)) {
			if (!verify_opt_len((struct nd_opt_hdr *)&ra[1],
			    len - sizeof (struct nd_router_advert), pi, &from))
				return;
		}
		if (debug & D_PKTIN) {
			print_route_adv("Received valid advert from ", pi,
			    ra, len, &from);
		}
		if (pi->pi_AdvSendAdvertisements)
			verify_ra_consistency(pi, ra, len, &from);
		else
			incoming_ra(pi, ra, len, &from, _B_FALSE);
		break;
	}
}

/*
 * Process a received router solicitation.
 * Check for source link-layer address option and check if it
 * is time to advertise.
 */
static void
incoming_rs(struct phyint *pi, struct nd_router_solicit *rs, int len,
    struct sockaddr_in6 *from)
{
	struct nd_opt_hdr *opt;
	int optlen;

	/* Process any options */
	len -= sizeof (struct nd_router_solicit);
	opt = (struct nd_opt_hdr *)&rs[1];
	while (len >= sizeof (struct nd_opt_hdr)) {
		optlen = opt->nd_opt_len * 8;
		switch (opt->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
			incoming_lla_opt(pi, (uchar_t *)opt,
			    from, NDF_ISROUTER_OFF);
			break;
		default:
			break;
		}
		opt = (struct nd_opt_hdr *)((char *)opt + optlen);
		len -= optlen;
	}
	/* Simple algorithm: treat unicast and multicast RSs the same */
	check_to_advertise(pi, RECEIVED_SOLICIT);
}

/*
 * Process a received router advertisement.
 * Called both when packets arrive as well as when we send RAs.
 * In the latter case 'loopback' is set.
 */
void
incoming_ra(struct phyint *pi, struct nd_router_advert *ra, int len,
    struct sockaddr_in6 *from, boolean_t loopback)
{
	struct nd_opt_hdr *opt;
	int optlen;
	struct lifreq lifr;
	boolean_t set_needed = _B_FALSE;
	struct router *dr;
	uint16_t router_lifetime;
	uint_t reachable, retrans;
	boolean_t reachable_time_changed = _B_FALSE;

	if (no_loopback && loopback)
		return;

	(void) strncpy(lifr.lifr_name, pi->pi_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(pi->pi_sock, SIOCGLIFLNKINFO, (char *)&lifr) < 0) {
		if (errno == ENXIO)
			return;
		logperror_pi(pi, "incoming_ra: SIOCGLIFLNKINFO");
		return;
	}
	if (ra->nd_ra_curhoplimit != 0 &&
	    ra->nd_ra_curhoplimit != pi->pi_CurHopLimit) {
		pi->pi_CurHopLimit = ra->nd_ra_curhoplimit;

		lifr.lifr_ifinfo.lir_maxhops = pi->pi_CurHopLimit;
		set_needed = _B_TRUE;
	}

	reachable = ntohl(ra->nd_ra_reachable);
	if (reachable != 0 &&
	    reachable != pi->pi_BaseReachableTime) {
		pi->pi_BaseReachableTime = reachable;
		reachable_time_changed = _B_TRUE;
	}

	if (pi->pi_reach_time_since_random < MIN_REACH_RANDOM_INTERVAL ||
	    reachable_time_changed) {
		phyint_reach_random(pi, _B_FALSE);
		set_needed = _B_TRUE;
	}
	lifr.lifr_ifinfo.lir_reachtime = pi->pi_ReachableTime;

	retrans = ntohl(ra->nd_ra_retransmit);
	if (retrans != 0 &&
	    pi->pi_RetransTimer != retrans) {
		pi->pi_RetransTimer = retrans;
		lifr.lifr_ifinfo.lir_reachretrans = pi->pi_RetransTimer;
		set_needed = _B_TRUE;
	}

	if (set_needed) {
		if (ioctl(pi->pi_sock, SIOCSLIFLNKINFO, (char *)&lifr) < 0) {
			logperror_pi(pi, "incoming_ra: SIOCSLIFLNKINFO");
			return;
		}
	}

	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) &&
	    !(ra_flags & ND_RA_FLAG_MANAGED)) {
		ra_flags |= ND_RA_FLAG_MANAGED;
		/* TODO trigger dhcpv6 */
		logtrace("incoming_ra: trigger dhcp MANAGED\n");
	}
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) &&
	    !(ra_flags & ND_RA_FLAG_OTHER)) {
		ra_flags |= ND_RA_FLAG_OTHER;
		if (!(ra_flags & ND_RA_FLAG_MANAGED)) {
			/* TODO trigger dhcpv6 for non-address info */
			logtrace("incoming_ra: trigger dhcp OTHER\n");
		}
	}
	/* Skip default router code if sent from ourselves */
	if (!loopback) {
		/* Find and update or add default router in list */
		dr = router_lookup(pi, from->sin6_addr);
		router_lifetime = ntohs(ra->nd_ra_router_lifetime);
		if (dr == NULL) {
			if (router_lifetime != 0) {
				dr = router_create(pi, from->sin6_addr,
				    1000 * router_lifetime);
				timer_schedule(dr->dr_lifetime);
			}
		} else {
			dr->dr_lifetime = 1000 * router_lifetime;
			if (dr->dr_lifetime != 0)
				timer_schedule(dr->dr_lifetime);
			if ((dr->dr_lifetime != 0 && !dr->dr_inkernel) ||
			    (dr->dr_lifetime == 0 && dr->dr_inkernel))
				router_update_k(dr);
		}
	}
	/* Process any options */
	len -= sizeof (struct nd_router_advert);
	opt = (struct nd_opt_hdr *)&ra[1];
	while (len >= sizeof (struct nd_opt_hdr)) {
		optlen = opt->nd_opt_len * 8;
		switch (opt->nd_opt_type) {
		case ND_OPT_PREFIX_INFORMATION:
			incoming_prefix_opt(pi, (uchar_t *)opt, from,
			    loopback);
			break;
		case ND_OPT_MTU:
			incoming_mtu_opt(pi, (uchar_t *)opt, from);
			break;
		case ND_OPT_SOURCE_LINKADDR:
			/* skip lla option if sent from ourselves! */
			if (!loopback) {
				incoming_lla_opt(pi, (uchar_t *)opt,
				    from, NDF_ISROUTER_ON);
			}
			break;
		default:
			break;
		}
		opt = (struct nd_opt_hdr *)((char *)opt + optlen);
		len -= optlen;
	}
	/* Stop sending solicitations */
	check_to_solicit(pi, SOLICIT_DONE);
}

/*
 * Process a received prefix option.
 * Unless addrconf is turned off we process both the addrconf and the
 * onlink aspects of the prefix option.
 *
 * Note that when a flag (onlink or auto) is turned off we do nothing -
 * the prefix will time out.
 */
static void
incoming_prefix_opt(struct phyint *pi, uchar_t *opt,
    struct sockaddr_in6 *from, boolean_t loopback)
{
	struct nd_opt_prefix_info *po = (struct nd_opt_prefix_info *)opt;
	boolean_t	good_prefix = _B_TRUE;

	if (8 * po->nd_opt_pi_len != sizeof (*po)) {
		char abuf[INET6_ADDRSTRLEN];

		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		logtrace("prefix option from %s on %s wrong size "
		    "(%d bytes)\n",
		    abuf, pi->pi_name,
		    8 * (int)po->nd_opt_pi_len);
		return;
	}
	if (IN6_IS_ADDR_LINKLOCAL(&po->nd_opt_pi_prefix)) {
		char abuf[INET6_ADDRSTRLEN];

		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		logtrace("RA from %s on %s contains link-local prefix "
		    "- ignored\n",
		    abuf, pi->pi_name);
		return;
	}
	if ((po->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO) && addrconf)
		good_prefix = incoming_prefix_addrconf(pi, opt, from, loopback);

	if ((po->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK) &&
	    good_prefix) {
		incoming_prefix_onlink(pi, opt, from, loopback);
	}
}

/*
 * Process prefix options with the onlink flag set.
 *
 * Note that on-link prefixes (and lifetimes) are not recorded
 * in the state file. If there are no routers ndpd will add an
 * onlink default route which will allow communication
 * between neighbors.
 */
/* ARGSUSED2 */
static void
incoming_prefix_onlink(struct phyint *pi, uchar_t *opt,
    struct sockaddr_in6 *from, boolean_t loopback)
{
	struct nd_opt_prefix_info *po = (struct nd_opt_prefix_info *)opt;
	int plen;
	struct prefix *pr;
	uint32_t validtime;	/* Without 2 hour rule */
	char abuf[INET6_ADDRSTRLEN];

	validtime = ntohl(po->nd_opt_pi_valid_time);

	plen = po->nd_opt_pi_prefix_len;
	pr = prefix_lookup(pi, po->nd_opt_pi_prefix, plen);
	if (pr == NULL) {
		if (validtime == 0)
			return;

		pr = prefix_create(pi, po->nd_opt_pi_prefix, plen);
		if (pr == NULL)
			return;
	} else {
		/* Exclude static prefixes */
		if (pr->pr_state & PR_STATIC)
			return;
	}
	if (validtime != 0)
		pr->pr_state |= PR_ONLINK;
	else
		pr->pr_state &= ~PR_ONLINK;

	/*
	 * Convert from seconds to milliseconds avoiding overflow.
	 * If the lifetime in the packet is e.g. PREFIX_INFINITY - 1
	 * (4 billion seconds - about 130 years) we will in fact time
	 * out the prefix after 4 billion milliseconds - 46 days).
	 * Thus the longest lifetime (apart from infinity) is 46 days.
	 * Note that this ensures that PREFIX_INFINITY still means "forever".
	 */
	if (validtime >= PREFIX_INFINITY / 1000)
		pr->pr_OnLinkLifetime = PREFIX_INFINITY - 1;
	else
		pr->pr_OnLinkLifetime = validtime * 1000;
	pr->pr_OnLinkFlag = _B_TRUE;
	if (debug & D_PREFIX) {
		logdebug("incoming_prefix_onlink(%s, %s/%u) "
		    "onlink %u\n",
		    pr->pr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
		    abuf, sizeof (abuf)), pr->pr_prefix_len,
		    pr->pr_OnLinkLifetime);
	}

	if (pr->pr_kernel_state != pr->pr_state)
		prefix_update_k(pr);

	if (pr->pr_OnLinkLifetime != 0)
		timer_schedule(pr->pr_OnLinkLifetime);
}


/*
 * Process prefix options with the autonomous flag set.
 * Returns false if this prefix results in a bad address (duplicate)
 */
/* ARGSUSED2 */
static boolean_t
incoming_prefix_addrconf(struct phyint *pi, uchar_t *opt,
    struct sockaddr_in6 *from, boolean_t loopback)
{
	struct nd_opt_prefix_info *po = (struct nd_opt_prefix_info *)opt;
	int plen;
	struct prefix *pr;
	struct prefix *other_pr;
	uint32_t validtime, preftime;	/* In seconds */
	uint32_t recorded_validtime;	/* In seconds */
	boolean_t new_prefix = _B_FALSE;
	char abuf[INET6_ADDRSTRLEN];
	char pbuf[INET6_ADDRSTRLEN];

	validtime = ntohl(po->nd_opt_pi_valid_time);
	preftime = ntohl(po->nd_opt_pi_preferred_time);
	plen = po->nd_opt_pi_prefix_len;

	/* Sanity checks */
	if (validtime < preftime) {
		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		(void) inet_ntop(AF_INET6,
		    (void *)&po->nd_opt_pi_prefix,
		    pbuf, sizeof (pbuf));
		logtrace("prefix option %s/%u from %s on %s: "
		    "valid %u < pref %u\n",
		    pbuf, plen, abuf, pi->pi_name,
		    validtime, preftime);
		return (_B_TRUE);
	}

	pr = prefix_lookup(pi, po->nd_opt_pi_prefix, plen);
	if (pr == NULL) {
		if (validtime == 0)
			return (_B_TRUE);

		pr = prefix_create(pi, po->nd_opt_pi_prefix, plen);
		if (pr == NULL)
			return (_B_TRUE);
		new_prefix = _B_TRUE;
	} else {
		/* Exclude static prefixes */
		if (pr->pr_state & PR_STATIC)
			return (_B_TRUE);

		/*
		 * Check 2 hour rule on valid lifetime.
		 * Follows: RFC 2462
		 * If we advertised this prefix ourselves we skip these
		 * checks. They are also skipped if we did not previously do
		 * addrconf on this prefix.
		 */
		recorded_validtime = pr->pr_ValidLifetime / 1000;

		if (loopback || !(pr->pr_state & PR_AUTO) ||
		    validtime >= MIN_VALID_LIFETIME ||
		    /* LINTED - statement has no consequent */
		    validtime >= recorded_validtime) {
			/* OK */
		} else if (recorded_validtime < MIN_VALID_LIFETIME &&
		    validtime < recorded_validtime) {
			/* Ignore the prefix */
			(void) inet_ntop(AF_INET6,
			    (void *)&from->sin6_addr,
			    abuf, sizeof (abuf));
			(void) inet_ntop(AF_INET6,
			    (void *)&po->nd_opt_pi_prefix,
			    pbuf, sizeof (pbuf));
			logtrace("prefix option %s/%u from %s on %s: "
			    "too short valid lifetime %u stored %u "
			    "- ignored\n",
			    pbuf, plen, abuf, pi->pi_name,
			    validtime, recorded_validtime);
			return (_B_TRUE);
		} else {
			/*
			 * If the router clock runs slower than the host
			 * by 1 second over 2 hours then this test will
			 * set the lifetime back to 2 hours once i.e.
			 * a lifetime decrementing in realtime might cause
			 * the prefix to live an extra 2 hours on the host.
			 */
			(void) inet_ntop(AF_INET6,
			    (void *)&from->sin6_addr,
			    abuf, sizeof (abuf));
			(void) inet_ntop(AF_INET6,
			    (void *)&po->nd_opt_pi_prefix,
			    pbuf, sizeof (pbuf));
			logtrace("prefix option %s/%u from %s on %s: "
			    "valid time %u stored %u rounded up "
			    "to %u\n",
			    pbuf, plen, abuf, pi->pi_name,
			    validtime, recorded_validtime,
			    MIN_VALID_LIFETIME);
			validtime = MIN_VALID_LIFETIME;
		}
	}
	if (!(pr->pr_state & PR_AUTO)) {
		int i;
		/*
		 * Form a new local address if the lengths match.
		 */
		if (pr->pr_prefix_len + pi->pi_token_length != IPV6_ABITS) {
			(void) inet_ntop(AF_INET6,
			    (void *)&from->sin6_addr,
			    abuf, sizeof (abuf));
			(void) inet_ntop(AF_INET6,
			    (void *)&po->nd_opt_pi_prefix,
			    pbuf, sizeof (pbuf));
			logtrace("prefix option %s/%u from %s on %s: "
			    "mismatched length %d token length %d\n",
			    pbuf, plen, abuf, pi->pi_name,
			    pr->pr_prefix_len, pi->pi_token_length);
			return (_B_TRUE);
		}
		for (i = 0; i < 16; i++) {
			/*
			 * prefix_create ensures that pr_prefix has all-zero
			 * bits after prefixlen.
			 */
			pr->pr_address.s6_addr[i] = pr->pr_prefix.s6_addr[i] |
			    pi->pi_token.s6_addr[i];
		}
		/*
		 * Check if any other physical interface has the same
		 * address configured already
		 */
		if ((other_pr = prefix_lookup_addr_match(pr)) != NULL) {
			/*
			 * Delete this prefix structure as kernel
			 * does not allow duplicated addresses
			 */

			logerr("incoming_prefix_addrconf: Duplicate "
			    "prefix  %s received on interface %s\n",
			    inet_ntop(AF_INET6,
			    (void *)&po->nd_opt_pi_prefix, abuf,
			    sizeof (abuf)), pi->pi_name);
			logerr("incoming_prefix_addrconf: Prefix already "
			    "exists in interface %s\n",
			    other_pr->pr_physical->pi_name);
			if (new_prefix) {
				prefix_delete(pr);
				return (_B_FALSE);
			}
			/* Ignore for addrconf purposes */
			validtime = preftime = 0;
		}
	}

	if (validtime != 0)
		pr->pr_state |= PR_AUTO;
	else
		pr->pr_state &= ~(PR_AUTO|PR_DEPRECATED);
	if (preftime != 0 || !(pr->pr_state & PR_AUTO))
		pr->pr_state &= ~PR_DEPRECATED;
	else
		pr->pr_state |= PR_DEPRECATED;

	/*
	 * Convert from seconds to milliseconds avoiding overflow.
	 * If the lifetime in the packet is e.g. PREFIX_INFINITY - 1
	 * (4 billion seconds - about 130 years) we will in fact time
	 * out the prefix after 4 billion milliseconds - 46 days).
	 * Thus the longest lifetime (apart from infinity) is 46 days.
	 * Note that this ensures that PREFIX_INFINITY still means "forever".
	 */
	if (validtime >= PREFIX_INFINITY / 1000)
		pr->pr_ValidLifetime = PREFIX_INFINITY - 1;
	else
		pr->pr_ValidLifetime = validtime * 1000;
	if (preftime >= PREFIX_INFINITY / 1000)
		pr->pr_PreferredLifetime = PREFIX_INFINITY - 1;
	else
		pr->pr_PreferredLifetime = preftime * 1000;
	pr->pr_AutonomousFlag = _B_TRUE;

	if (debug & D_PREFIX) {
		logdebug("incoming_prefix_addrconf(%s, %s/%u) "
		    "valid %u pref %u\n",
		    pr->pr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
		    abuf, sizeof (abuf)), pr->pr_prefix_len,
		    pr->pr_ValidLifetime, pr->pr_PreferredLifetime);
	}

	if (pr->pr_state & PR_AUTO) {
		/* Take the min of the two timeouts by calling it twice */
		if (pr->pr_ValidLifetime != 0)
			timer_schedule(pr->pr_ValidLifetime);
		if (pr->pr_PreferredLifetime != 0)
			timer_schedule(pr->pr_PreferredLifetime);
	}
	if (pr->pr_kernel_state != pr->pr_state) {
		/* Log a message when an addrconf prefix goes away */
		if ((pr->pr_kernel_state & PR_AUTO) &&
		    !(pr->pr_state & PR_AUTO)) {
			char abuf[INET6_ADDRSTRLEN];

			logwarn("Address removed due to zero "
			    "valid lifetime %s\n",
			    inet_ntop(AF_INET6, (void *)&pr->pr_address,
			    abuf, sizeof (abuf)));
		}
		prefix_update_k(pr);
	}
	if ((pr->pr_state & PR_AUTO) && new_prefix) {
		/* Update the state file */
		phyint_write_state_file(pi);
	}
	return (_B_TRUE);
}

/*
 * Process an MTU option received in a router advertisement.
 */
static void
incoming_mtu_opt(struct phyint *pi, uchar_t *opt,
    struct sockaddr_in6 *from)
{
	struct nd_opt_mtu *mo = (struct nd_opt_mtu *)opt;
	struct lifreq lifr;
	uint32_t mtu;

	if (8 * mo->nd_opt_mtu_len != sizeof (*mo)) {
		char abuf[INET6_ADDRSTRLEN];

		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		logtrace("mtu option from %s on %s wrong size "
		    "(%d bytes)\n",
		    abuf, pi->pi_name,
		    8 * (int)mo->nd_opt_mtu_len);
		return;
	}
	mtu = ntohl(mo->nd_opt_mtu_mtu);
	if (pi->pi_LinkMTU == mtu)
		return;	/* No change */
	if (mtu > pi->pi_mtu) {
		/* Can't exceed physical MTU */
		char abuf[INET6_ADDRSTRLEN];

		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		logtrace("mtu option from %s on %s too large MTU %d - %d\n",
		    abuf, pi->pi_name, mtu, pi->pi_mtu);
		return;
	}
	if (mtu < IPV6_MIN_MTU) {
		char abuf[INET6_ADDRSTRLEN];

		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		logtrace("mtu option from %s on %s too small MTU (%d)\n",
		    abuf, pi->pi_name, mtu);
		return;
	}

	pi->pi_LinkMTU = mtu;
	(void) strncpy(lifr.lifr_name, pi->pi_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(pi->pi_sock, SIOCGLIFLNKINFO, (char *)&lifr) < 0) {
		logperror_pi(pi, "incoming_mtu_opt: SIOCGLIFLNKINFO");
		return;
	}
	lifr.lifr_ifinfo.lir_maxmtu = pi->pi_LinkMTU;
	if (ioctl(pi->pi_sock, SIOCSLIFLNKINFO, (char *)&lifr) < 0) {
		logperror_pi(pi, "incoming_mtu_opt: SIOCSLIFLNKINFO");
		return;
	}
}

/*
 * Process a source link-layer address option received in a router
 * advertisement or solicitation.
 */
static void
incoming_lla_opt(struct phyint *pi, uchar_t *opt,
    struct sockaddr_in6 *from, int isrouter)
{
	struct nd_opt_lla *lo = (struct nd_opt_lla *)opt;
	struct lifreq lifr;
	struct sockaddr_in6 *sin6;
	int max_content_len;

	if (pi->pi_hdw_addr_len == 0)
		return;

	/*
	 * Can't remove padding since it is link type specific.
	 * However, we check against the length of our link-layer
	 * address.
	 * Note: assumes that all links have a fixed lengh address.
	 */
	max_content_len = lo->nd_opt_lla_len * 8 - sizeof (struct nd_opt_hdr);
	if (max_content_len < pi->pi_hdw_addr_len ||
	    (max_content_len >= 8 &&
	    max_content_len - 7 > pi->pi_hdw_addr_len)) {
		char abuf[INET6_ADDRSTRLEN];

		(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
		    abuf, sizeof (abuf));
		logtrace("lla option from %s on %s too long with bad "
		    "physaddr length (%d vs. %d bytes)\n",
		    abuf, pi->pi_name,
		    max_content_len, pi->pi_hdw_addr_len);
		return;
	}

	lifr.lifr_nd.lnr_hdw_len = pi->pi_hdw_addr_len;
	bcopy((char *)lo->nd_opt_lla_hdw_addr,
	    (char *)lifr.lifr_nd.lnr_hdw_addr,
	    lifr.lifr_nd.lnr_hdw_len);

	sin6 = (struct sockaddr_in6 *)&lifr.lifr_nd.lnr_addr;
	bzero(sin6, sizeof (struct sockaddr_in6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = from->sin6_addr;

	/*
	 * Set IsRouter flag if RA; clear if RS.
	 */
	lifr.lifr_nd.lnr_state_create = ND_STALE;
	lifr.lifr_nd.lnr_state_same_lla = ND_UNCHANGED;
	lifr.lifr_nd.lnr_state_diff_lla = ND_STALE;
	lifr.lifr_nd.lnr_flags = isrouter;
	(void) strncpy(lifr.lifr_name, pi->pi_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(pi->pi_sock, SIOCLIFSETND, (char *)&lifr) < 0) {
		logperror_pi(pi, "incoming_lla_opt: SIOCLIFSETND");
		return;
	}
}

/*
 * Verify the content of the received router advertisement against our
 * own configuration as specified in RFC 2461.
 */
static void
verify_ra_consistency(struct phyint *pi, struct nd_router_advert *ra, int len,
    struct sockaddr_in6 *from)
{
	char frombuf[INET6_ADDRSTRLEN];
	struct nd_opt_hdr *opt;
	int optlen;
	uint_t reachable, retrans;
	boolean_t pktflag, myflag;

	(void) inet_ntop(AF_INET6, (void *)&from->sin6_addr,
	    frombuf, sizeof (frombuf));

	if (ra->nd_ra_curhoplimit != 0 &&
	    pi->pi_AdvCurHopLimit != 0 &&
	    ra->nd_ra_curhoplimit != pi->pi_AdvCurHopLimit) {
		logtrace("RA from %s on %s inconsistent cur hop limit:\n\t"
		    "received %d configuration %d\n",
		    frombuf, pi->pi_name,
		    ra->nd_ra_curhoplimit, pi->pi_AdvCurHopLimit);
	}

	reachable = ntohl(ra->nd_ra_reachable);
	if (reachable != 0 && pi->pi_AdvReachableTime != 0 &&
	    reachable != pi->pi_AdvReachableTime) {
		logtrace("RA from %s on %s inconsistent reachable time:\n\t"
		    "received %d configuration %d\n",
		    frombuf, pi->pi_name,
		    reachable, pi->pi_AdvReachableTime);
	}

	retrans = ntohl(ra->nd_ra_retransmit);
	if (retrans != 0 && pi->pi_AdvRetransTimer != 0 &&
	    retrans != pi->pi_AdvRetransTimer) {
		logtrace("RA from %s on %s inconsistent retransmit timer:\n\t"
		    "received %d configuration %d\n",
		    frombuf, pi->pi_name,
		    retrans, pi->pi_AdvRetransTimer);
	}

	pktflag = ((ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) != 0);
	myflag = (pi->pi_AdvManagedFlag != 0);
	if (pktflag != myflag) {
		logtrace("RA from %s on %s inconsistent managed flag:\n\t"
		    "received %s configuration %s\n",
		    frombuf, pi->pi_name,
		    (pktflag ? "ON" : "OFF"),
		    (myflag ? "ON" : "OFF"));
	}
	pktflag = ((ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) != 0);
	myflag = (pi->pi_AdvOtherConfigFlag != 0);
	if (pktflag != myflag) {
		logtrace("RA from %s on %s inconsistent other config flag:\n\t"
		    "received %s configuration %s\n",
		    frombuf, pi->pi_name,
		    (pktflag ? "ON" : "OFF"),
		    (myflag ? "ON" : "OFF"));
	}

	/* Process any options */
	len -= sizeof (struct nd_router_advert);
	opt = (struct nd_opt_hdr *)&ra[1];
	while (len >= sizeof (struct nd_opt_hdr)) {
		optlen = opt->nd_opt_len * 8;
		switch (opt->nd_opt_type) {
		case ND_OPT_PREFIX_INFORMATION:
			verify_prefix_opt(pi, (uchar_t *)opt, frombuf);
			break;
		case ND_OPT_MTU:
			verify_mtu_opt(pi, (uchar_t *)opt, frombuf);
			break;
		default:
			break;
		}
		opt = (struct nd_opt_hdr *)((char *)opt + optlen);
		len -= optlen;
	}
}

/*
 * Verify that the lifetimes and onlink/auto flags are consistent
 * with our settings.
 */
static void
verify_prefix_opt(struct phyint *pi, uchar_t *opt, char *frombuf)
{
	struct nd_opt_prefix_info *po = (struct nd_opt_prefix_info *)opt;
	int plen;
	struct prefix *pr;
	uint32_t validtime, preftime;
	char prefixbuf[INET6_ADDRSTRLEN];
	int pktflag, myflag;

	if (8 * po->nd_opt_pi_len != sizeof (*po)) {
		logtrace("RA prefix option from %s on %s wrong size "
		    "(%d bytes)\n",
		    frombuf, pi->pi_name,
		    8 * (int)po->nd_opt_pi_len);
		return;
	}
	if (IN6_IS_ADDR_LINKLOCAL(&po->nd_opt_pi_prefix)) {
		logtrace("RA from %s on %s contains link-local prefix "
		    "- ignored\n",
		    frombuf, pi->pi_name);
		return;
	}
	plen = po->nd_opt_pi_prefix_len;
	pr = prefix_lookup(pi, po->nd_opt_pi_prefix, plen);
	if (pr == NULL)
		return;

	/* Exclude static prefixes */
	if (pr->pr_state & PR_STATIC)
		return;

	/* Ignore prefixes which we do not advertise */
	if (!pr->pr_AdvAutonomousFlag && !pr->pr_AdvOnLinkFlag)
		return;

	(void) inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
	    prefixbuf, sizeof (prefixbuf));
	pktflag = ((po->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO) != 0);
	myflag = (pr->pr_AdvAutonomousFlag != 0);
	if (pktflag != myflag) {
		logtrace(
		    "RA from %s on %s inconsistent autonumous flag for \n\t"
		    "prefix %s/%u: received %s configuration %s\n",
		    frombuf, pi->pi_name, prefixbuf, pr->pr_prefix_len,
		    (pktflag ? "ON" : "OFF"),
		    (myflag ? "ON" : "OFF"));
	}

	pktflag = ((po->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK) != 0);
	myflag = (pr->pr_AdvOnLinkFlag != 0);
	if (pktflag != myflag) {
		logtrace("RA from %s on %s inconsistent on link flag for \n\t"
		    "prefix %s/%u: received %s configuration %s\n",
		    frombuf, pi->pi_name, prefixbuf, pr->pr_prefix_len,
		    (pktflag ? "ON" : "OFF"),
		    (myflag ? "ON" : "OFF"));
	}
	validtime = ntohl(po->nd_opt_pi_valid_time);
	preftime = ntohl(po->nd_opt_pi_preferred_time);

	/*
	 * Take into account variation for lifetimes decrementing
	 * in real time. Allow +/- 10 percent and +/- 10 seconds.
	 */
#define	LOWER_LIMIT(val)	((val) - (val)/10 - 10)
#define	UPPER_LIMIT(val)	((val) + (val)/10 + 10)
	if (pr->pr_AdvValidRealTime) {
		if (pr->pr_AdvValidExpiration > 0 &&
		    (validtime < LOWER_LIMIT(pr->pr_AdvValidExpiration) ||
		    validtime > UPPER_LIMIT(pr->pr_AdvValidExpiration))) {
			logtrace("RA from %s on %s inconsistent valid "
			    "lifetime for\n\tprefix %s/%u: received %d "
			    "configuration %d\n",
			    frombuf, pi->pi_name, prefixbuf, pr->pr_prefix_len,
			    validtime, pr->pr_AdvValidExpiration);
		}
	} else {
		if (validtime != pr->pr_AdvValidLifetime) {
			logtrace("RA from %s on %s inconsistent valid "
			    "lifetime for\n\tprefix %s/%u: received %d "
			    "configuration %d\n",
			    frombuf, pi->pi_name, prefixbuf, pr->pr_prefix_len,
			    validtime, pr->pr_AdvValidLifetime);
		}
	}

	if (pr->pr_AdvPreferredRealTime) {
		if (pr->pr_AdvPreferredExpiration > 0 &&
		    (preftime < LOWER_LIMIT(pr->pr_AdvPreferredExpiration) ||
		    preftime > UPPER_LIMIT(pr->pr_AdvPreferredExpiration))) {
			logtrace("RA from %s on %s inconsistent preferred "
			    "lifetime for\n\tprefix %s/%u: received %d "
			    "configuration %d\n",
			    frombuf, pi->pi_name, prefixbuf, pr->pr_prefix_len,
			    preftime, pr->pr_AdvPreferredExpiration);
		}
	} else {
		if (preftime != pr->pr_AdvPreferredLifetime) {
			logtrace("RA from %s on %s inconsistent preferred "
			    "lifetime for\n\tprefix %s/%u: received %d "
			    "configuration %d\n",
			    frombuf, pi->pi_name, prefixbuf, pr->pr_prefix_len,
			    preftime, pr->pr_AdvPreferredLifetime);
		}
	}
}

/*
 * Verify the received MTU against our own configuration.
 */
static void
verify_mtu_opt(struct phyint *pi, uchar_t *opt, char *frombuf)
{
	struct nd_opt_mtu *mo = (struct nd_opt_mtu *)opt;
	uint32_t mtu;

	if (8 * mo->nd_opt_mtu_len != sizeof (*mo)) {
		logtrace("mtu option from %s on %s wrong size "
		    "(%d bytes)\n",
		    frombuf, pi->pi_name,
		    8 * (int)mo->nd_opt_mtu_len);
		return;
	}
	mtu = ntohl(mo->nd_opt_mtu_mtu);
	if (pi->pi_AdvLinkMTU != 0 &&
	    pi->pi_AdvLinkMTU != mtu) {
		logtrace("RA from %s on %s inconsistent MTU: "
		    "received %d configuration %d\n",
		    frombuf, pi->pi_name,
		    mtu, pi->pi_AdvLinkMTU);
	}
}

/*
 * Verify that all options have a non-zero length and that
 * the options fit within the total length of the packet (optlen).
 */
static boolean_t
verify_opt_len(struct nd_opt_hdr *opt, int optlen,
    struct phyint *pi, struct sockaddr_in6 *from)
{
	while (optlen > 0) {
		if (opt->nd_opt_len == 0) {
			char abuf[INET6_ADDRSTRLEN];

			(void) inet_ntop(AF_INET6,
			    (void *)&from->sin6_addr,
			    abuf, sizeof (abuf));

			logtrace("Zero length option type 0x%x "
			    "from %s on %s\n",
			    opt->nd_opt_type, abuf, pi->pi_name);
			return (_B_FALSE);
		}
		optlen -= 8 * opt->nd_opt_len;
		if (optlen < 0) {
			char abuf[INET6_ADDRSTRLEN];

			(void) inet_ntop(AF_INET6,
			    (void *)&from->sin6_addr,
			    abuf, sizeof (abuf));

			logtrace("Too large option: type 0x%x len %u "
			    "from %s on %s\n",
			    opt->nd_opt_type, opt->nd_opt_len,
			    abuf, pi->pi_name);
			return (_B_FALSE);
		}
		opt = (struct nd_opt_hdr *)((char *)opt +
		    8 * opt->nd_opt_len);
	}
	return (_B_TRUE);
}
