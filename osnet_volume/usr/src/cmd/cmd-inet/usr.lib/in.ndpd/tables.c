/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tables.c	1.5	99/12/06 SMI"

#include "defs.h"
#include "tables.h"

struct phyint *phyints = NULL;

static void	phyint_print(struct phyint *pi);
static void	phyint_insert(struct phyint *pi);

static void	prefix_print(struct prefix *pr);
static void	prefix_insert(struct phyint *pi, struct prefix *pr);
static boolean_t prefix_equal(struct in6_addr p1, struct in6_addr p2, int bits);
static void	prefix_set(struct in6_addr *prefix, struct in6_addr addr,
		    int bits);
static char	*prefix_print_state(int state, char *buf);

static void	router_print(struct router *dr);
static void	router_insert(struct phyint *pi, struct router *dr);
static void	router_add_k(struct router *dr);
static void	router_delete_k(struct router *dr);
static void	router_delete(struct router *dr);
static void	router_delete_onlink(struct phyint *pi);

static int	rtmseq;				/* rtm_seq sequence number */

struct phyint *
phyint_lookup(char *name)
{
	struct phyint *pi;

	if (debug & D_PHYINT)
		logdebug("phyint_lookup(%s)\n", name);

	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		if (strcmp(pi->pi_name, name) == 0)
			break;
	}
	return (pi);
}

struct phyint *
phyint_lookup_on_index(uint_t ifindex)
{
	struct phyint *pi;

	if (debug & D_PHYINT)
		logdebug("phyint_lookup_on_index(%d)\n", ifindex);

	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		if (pi->pi_index == ifindex)
			break;
	}
	return (pi);
}

struct phyint *
phyint_create(char *name)
{
	struct phyint *pi;

	if (debug & D_PHYINT)
		logdebug("phyint_create(%s)\n", name);

	pi = (struct phyint *)calloc(sizeof (struct phyint), 1);
	if (pi == NULL) {
		logerr("phyint_create: out of memory\n");
		return (NULL);
	}
	(void) strncpy(pi->pi_name, name, sizeof (pi->pi_name));
	pi->pi_name[sizeof (pi->pi_name) - 1] = '\0';

	pi->pi_sock = -1;
	if (phyint_init_from_k(pi) == -1) {
		free(pi);
		return (NULL);
	}
	phyint_insert(pi);
	if (pi->pi_sock != -1) {
		if (poll_add(pi->pi_sock) == -1) {
			phyint_delete(pi);
			return (NULL);
		}
	}
	return (pi);
}

/* Insert in linked list */
static void
phyint_insert(struct phyint *pi)
{
	/* Insert in list */
	pi->pi_next = phyints;
	pi->pi_prev = NULL;
	if (phyints)
		phyints->pi_prev = pi;
	phyints = pi;
}

/*
 * Initialize both the phyint data structure and the pi_sock for
 * sending and receving on the interface.
 * Extract information from the kernel (if present) and set pi_kernel_state.
 */
int
phyint_init_from_k(pi)
	struct phyint *pi;
{
	struct ipv6_mreq v6mcastr;
	struct lifreq lifr;
	int fd;
	boolean_t newsock;
	uint_t ttl;
	struct sockaddr_in6 *sin6;

	if (debug & D_PHYINT)
		logdebug("phyint_init_from_k(%s)\n", pi->pi_name);

	if (pi->pi_sock < 0) {
		pi->pi_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
		if (pi->pi_sock < 0) {
			logperror_pi(pi, "phyint_init_from_k: socket");
			return (-1);
		}
		newsock = _B_TRUE;
	} else {
		newsock = _B_FALSE;
	}
	fd = pi->pi_sock;

	(void) strncpy(lifr.lifr_name, pi->pi_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(fd, SIOCGLIFINDEX, (char *)&lifr) < 0) {
		if (errno == ENXIO) {
			/* Interface does not (yet) exist in kernel */
			pi->pi_kernel_state &= ~PI_PRESENT;
			if (newsock) {
				(void) close(pi->pi_sock);
				pi->pi_sock = -1;
			}
			if (debug & D_PHYINT) {
				logdebug("phyint_init_from_k(%s): not exist\n",
				    pi->pi_name);
			}
			return (0);
		}
		logperror_pi(pi, "phyint_init_from_k: SIOCGLIFINDEX");
		goto error;
	}
	pi->pi_index = lifr.lifr_index;

	if (ioctl(fd, SIOCGLIFFLAGS, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: ioctl (get flags)");
		goto error;
	}
	pi->pi_flags = lifr.lifr_flags;

	/*
	 * If the  link local interface is not up yet or it's IFF_UP
	 * and the flag is set to IFF_NOLOCAL as Duplicate Address
	 * Detection is in progress.
	 * IFF_NOLOCAL is "normal" on other prefixes.
	 */

	if (!(pi->pi_flags & IFF_UP) || (pi->pi_flags & IFF_NOLOCAL)) {
		/* Pretend the interface does not (yet) exist in kernel */
		pi->pi_kernel_state &= ~PI_PRESENT;
		if (newsock) {
			(void) close(pi->pi_sock);
			pi->pi_sock = -1;
		}
		if (debug & D_PHYINT) {
			logdebug("phyint_init_from_k(%s): not IFF_UP\n",
			    pi->pi_name);
		}
		return (0);
	}
	pi->pi_kernel_state |= PI_PRESENT;

	if (ioctl(fd, SIOCGLIFMTU, (caddr_t)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: ioctl (get mtu)");
		goto error;
	}
	pi->pi_mtu = lifr.lifr_mtu;

	if (ioctl(fd, SIOCGLIFADDR, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: SIOCGLIFADDR");
		goto error;
	}
	sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
	pi->pi_ifaddr = sin6->sin6_addr;

	if (ioctl(fd, SIOCGLIFTOKEN, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: SIOCGLIFTOKEN");
		goto error;
	}
	/* Ignore interface if the token is all zeros */
	sin6 = (struct sockaddr_in6 *)&lifr.lifr_token;
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		logerr("phyint_init_from_k: zero token on %s\n", pi->pi_name);
		goto error;
	}
	pi->pi_token = sin6->sin6_addr;
	pi->pi_token_length = lifr.lifr_addrlen;

	/*
	 * Guess a remote token for POINTOPOINT by looking at
	 * the link-local destination address.
	 */
	if (pi->pi_flags & IFF_POINTOPOINT) {
		if (ioctl(fd, SIOCGLIFDSTADDR, (char *)&lifr) < 0) {
			logperror_pi(pi, "phyint_init_from_k: SIOCGLIFDSTADDR");
			goto error;
		}
		sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
		if (sin6->sin6_family != AF_INET6 ||
		    IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) ||
		    !IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
			pi->pi_dst_token = in6addr_any;
		} else {
			pi->pi_dst_token = sin6->sin6_addr;
			/* Clear link-local prefix (first 10 bits) */
			pi->pi_dst_token.s6_addr[0] = 0;
			pi->pi_dst_token.s6_addr[1] &= 0x3f;
		}
	} else {
		pi->pi_dst_token = in6addr_any;
	}

	/* Get link-layer address */
	if (!(pi->pi_flags & IFF_MULTICAST) ||
	    (pi->pi_flags & IFF_POINTOPOINT)) {
		pi->pi_hdw_addr_len = 0;
	} else {
		sin6 = (struct sockaddr_in6 *)&lifr.lifr_nd.lnr_addr;
		bzero(sin6, sizeof (struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_addr = pi->pi_ifaddr;

		if (ioctl(fd, SIOCLIFGETND, (char *)&lifr) < 0) {
			logperror_pi(pi, "phyint_init_from_k: SIOCLIFGETND");
			goto error;
		}

		pi->pi_hdw_addr_len = lifr.lifr_nd.lnr_hdw_len;

		if (lifr.lifr_nd.lnr_hdw_len != 0) {
			bcopy((char *)lifr.lifr_nd.lnr_hdw_addr,
			    (char *)pi->pi_hdw_addr,
			    lifr.lifr_nd.lnr_hdw_len);
		}
	}

	if (newsock) {
		icmp6_filter_t filter;
		int on = 1;

		/* Set default values */
		pi->pi_LinkMTU = pi->pi_mtu;
		pi->pi_CurHopLimit = IPV6_MAX_HOPS;
		pi->pi_BaseReachableTime = ND_REACHABLE_TIME;
		phyint_reach_random(pi, _B_FALSE);
		pi->pi_RetransTimer = ND_RETRANS_TIMER;

		/* Setup socket for transmission and reception */
		if (setsockopt(fd, IPPROTO_IPV6,
		    IPV6_BOUND_IF, (char *)&pi->pi_index,
		    sizeof (pi->pi_index)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "IPV6_BOUND_IF");
			goto error;
		}

		ttl = IPV6_MAX_HOPS;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
		    (char *)&ttl, sizeof (ttl)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "IPV6_UNICAST_HOPS");
			goto error;
		}

		if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		    (char *)&ttl, sizeof (ttl)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "IPV6_MULTICAST_HOPS");
			goto error;
		}

		if (!(pi->pi_kernel_state & PI_JOINED_ALLROUTERS)) {
			v6mcastr.ipv6mr_multiaddr = all_nodes_mcast;
			v6mcastr.ipv6mr_interface = pi->pi_index;
			if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			    (char *)&v6mcastr, sizeof (v6mcastr)) < 0) {
				logperror_pi(pi, "phyint_init_from_k: "
				    "setsockopt IPV6_JOIN_GROUP");
				goto error;
			}
			pi->pi_state |= PI_JOINED_ALLNODES;
			pi->pi_kernel_state |= PI_JOINED_ALLNODES;
		}

		/*
		 * Filter out so that we only receive router advertisements and
		 * router solicitations.
		 */
		ICMP6_FILTER_SETBLOCKALL(&filter);
		ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filter);
		ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filter);

		if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER,
		    (char *)&filter, sizeof (filter)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "ICMP6_FILTER");
			goto error;
		}

		/* Enable receipt of ancillary data */
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT,
		    (char *)&on, sizeof (on)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "IPV6_RECVHOPLIMIT");
			goto error;
		}
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVRTHDR,
		    (char *)&on, sizeof (on)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "IPV6_RECVRTHDR");
			goto error;
		}
	}

	if (pi->pi_AdvSendAdvertisements &&
	    !(pi->pi_kernel_state & PI_JOINED_ALLROUTERS)) {
		v6mcastr.ipv6mr_multiaddr = all_routers_mcast;
		v6mcastr.ipv6mr_interface = pi->pi_index;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
		    (char *)&v6mcastr, sizeof (v6mcastr)) < 0) {
			logperror_pi(pi, "phyint_init_from_k: setsockopt "
			    "IPV6_JOIN_GROUP");
			goto error;
		}
		pi->pi_state |= PI_JOINED_ALLROUTERS;
		pi->pi_kernel_state |= PI_JOINED_ALLROUTERS;
	}
	/* Set IFF_ROUTER based on AdvSendAdvertisements. */
	(void) strncpy(lifr.lifr_name, pi->pi_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(fd, SIOCGLIFFLAGS, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: SIOCGLIFFLAGS");
		goto error;
	}
	if (pi->pi_AdvSendAdvertisements)
		lifr.lifr_flags |= IFF_ROUTER;
	else
		lifr.lifr_flags &= ~IFF_ROUTER;
	if (ioctl(fd, SIOCSLIFFLAGS, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: SIOCSLIFFLAGS");
		goto error;
	}
	pi->pi_flags = lifr.lifr_flags;

	/* Set linkinfo parameters */
	(void) strncpy(lifr.lifr_name, pi->pi_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(fd, SIOCGLIFLNKINFO, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: SIOCGLIFLNKINFO");
		goto error;
	}
	lifr.lifr_ifinfo.lir_maxhops = pi->pi_CurHopLimit;
	lifr.lifr_ifinfo.lir_reachtime = pi->pi_ReachableTime;
	lifr.lifr_ifinfo.lir_reachretrans = pi->pi_RetransTimer;
	if (ioctl(fd, SIOCSLIFLNKINFO, (char *)&lifr) < 0) {
		logperror_pi(pi, "phyint_init_from_k: SIOCSLIFLNKINFO");
		goto error;
	}
	if (debug & D_PHYINT) {
		logdebug("phyint_init_from_k(%s): done\n",
		    pi->pi_name);
	}
	return (0);

error:
	/* Pretend the interface does not exist in the kernel */
	pi->pi_kernel_state &= ~PI_PRESENT;
	if (newsock) {
		(void) close(pi->pi_sock);
		pi->pi_sock = -1;
	}
	return (-1);
}

/*
 * Delete (unlink and free).
 * Handles delete of things that have not yet been inserted in the list.
 */
void
phyint_delete(struct phyint *pi)
{
	if (debug & D_PHYINT)
		logdebug("phyint_delete(%s)\n", pi->pi_name);

	while (pi->pi_router_list)
		router_delete(pi->pi_router_list);
	while (pi->pi_prefix_list)
		prefix_delete(pi->pi_prefix_list);

	if (pi->pi_sock != -1) {
		(void) poll_remove(pi->pi_sock);
		if (close(pi->pi_sock) < 0) {
			logperror_pi(pi, "phyint_delete: close");
		}
		pi->pi_sock = -1;
	}

	if (pi->pi_prev == NULL) {
		if (phyints == pi)
			phyints = pi->pi_next;
	} else {
		pi->pi_prev->pi_next = pi->pi_next;
	}
	if (pi->pi_next != NULL)
		pi->pi_next->pi_prev = pi->pi_prev;
	pi->pi_next = pi->pi_prev = NULL;
	free(pi);
}

/*
 * Called with the number of millseconds elapsed since the last call.
 * Determines if any timeout event has occurred and
 * returns the number of milliseconds until the next timeout event
 * for the phyint iself (excluding prefixes and routers).
 * Returns TIMER_INFINITY for "never".
 */
uint_t
phyint_timer(struct phyint *pi, uint_t elapsed)
{
	uint_t next = TIMER_INFINITY;

	if (pi->pi_AdvSendAdvertisements) {
		if (pi->pi_adv_state != NO_ADV) {
			int old_state = pi->pi_adv_state;

			if (debug & (D_STATE|D_PHYINT)) {
				logdebug("phyint_timer ADV(%s) state %d\n",
				    pi->pi_name, (int)old_state);
			}
			next = advertise_event(pi, ADV_TIMER, elapsed);
			if (debug & D_STATE) {
				logdebug("phyint_timer ADV(%s) "
				    "state %d -> %d\n",
				    pi->pi_name, (int)old_state,
				    (int)pi->pi_adv_state);
			}
		}
	} else {
		if (pi->pi_sol_state != NO_SOLICIT) {
			int old_state = pi->pi_sol_state;

			if (debug & (D_STATE|D_PHYINT)) {
				logdebug("phyint_timer SOL(%s) state %d\n",
				    pi->pi_name, (int)old_state);
			}
			next = solicit_event(pi, SOL_TIMER, elapsed);
			if (debug & D_STATE) {
				logdebug("phyint_timer SOL(%s) "
				    "state %d -> %d\n",
				    pi->pi_name, (int)old_state,
				    (int)pi->pi_sol_state);
			}
		}
	}
	pi->pi_prefix_time_since_saved += elapsed;
	if (pi->pi_prefix_time_since_saved >= NDP_STATE_FILE_SAVE_TIME)
		phyint_write_state_file(pi);

	pi->pi_reach_time_since_random += elapsed;
	if (pi->pi_reach_time_since_random >= MAX_REACH_RANDOM_INTERVAL)
		phyint_reach_random(pi, _B_TRUE);

	return (next);
}

static void
phyint_print(struct phyint *pi)
{
	struct prefix *pr;
	struct router *dr;
	char abuf[INET6_ADDRSTRLEN];
	char llabuf[BUFSIZ];

	logdebug("Phyint %s index %d state %x, kernel %x, onlink_def %d "
	    "num routers %d\n",
	    pi->pi_name, pi->pi_index,
	    pi->pi_state, pi->pi_kernel_state, (int)pi->pi_onlink_default,
	    pi->pi_num_k_routers);
	logdebug("\taddress: %s flags %x\n",
	    inet_ntop(AF_INET6, (void *)&pi->pi_ifaddr,
	    abuf, sizeof (abuf)), pi->pi_flags);
	logdebug("\tsock %d in_use %d mtu %d hdw_addr len %d <%s>\n",
	    pi->pi_sock, pi->pi_in_use, pi->pi_mtu, pi->pi_hdw_addr_len,
	    ((pi->pi_hdw_addr_len != 0) ?
	    fmt_lla(llabuf, sizeof (llabuf), pi->pi_hdw_addr,
	    pi->pi_hdw_addr_len) : "none"));
	logdebug("\ttoken: len %d %s\n",
	    pi->pi_token_length,
	    inet_ntop(AF_INET6, (void *)&pi->pi_token,
	    abuf, sizeof (abuf)));
	if (pi->pi_flags & IFF_POINTOPOINT) {
		logdebug("\tdst_token: %s\n",
		    inet_ntop(AF_INET6, (void *)&pi->pi_dst_token,
			abuf, sizeof (abuf)));
	}
	logdebug("\tLinkMTU %d CurHopLimit %d BaseReachableTime %d\n\t"
	    "ReachableTime %d RetransTimer %d\n",
	    pi->pi_LinkMTU, pi->pi_CurHopLimit, pi->pi_BaseReachableTime,
	    pi->pi_ReachableTime, pi->pi_RetransTimer);
	if (!pi->pi_AdvSendAdvertisements) {
		/* Solicit state */
		logdebug("\tSOLICIT: time_left %d state %d count %d\n",
		    pi->pi_sol_time_left, pi->pi_sol_state, pi->pi_sol_count);
	} else {
		/* Advertise state */
		logdebug("\tADVERT: time_left %d state %d count %d "
		    "since last %d\n",
		    pi->pi_adv_time_left, pi->pi_adv_state, pi->pi_adv_count,
		    pi->pi_adv_time_since_sent);
		print_iflist(pi->pi_config);
	}
	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next)
		prefix_print(pr);

	for (dr = pi->pi_router_list; dr != NULL; dr = dr->dr_next)
		router_print(dr);

	logdebug("\n");
}

/*
 * Save all addrconf prefix and lifetimes in a state file for this phyint.
 * Limits saved lifetimes to 1 week (NDP_MAX_SAVE_LIFETIME)
 */
void
phyint_write_state_file(struct phyint *pi)
{
	struct prefix *pr;
	FILE *fp;
	char filename[BUFSIZ];
	char abuf[INET6_ADDRSTRLEN];
	char pbuf[BUFSIZ], vbuf[BUFSIZ];
	struct tm tms;
	time_t expiry_time;

	if (debug & D_PHYINT)
		logdebug("phyint_write_state_file(%s)\n", pi->pi_name);

	(void) strncpy(filename, NDP_STATE_FILE, sizeof (filename));
	filename[sizeof (filename) - 1] = '\0';
	(void) strncat(filename, pi->pi_name,
	    sizeof (filename) - strlen(filename));
	filename[sizeof (filename) - 1] = '\0';
	fp = fopen(filename, "w");
	if (fp == NULL) {
		logperror_pi(pi, "phyint_write_state_file: fopen");
		logerr("phyint_write_state_file: can't open %s\n",
		    filename);
		return;
	}
	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
		if (!(pr->pr_state & PR_AUTO) ||
		    (pr->pr_state & PR_STATIC) ||
		    !(pr->pr_flags & IFF_UP) ||
		    IN6_IS_ADDR_LINKLOCAL(&pr->pr_address))
			continue;

		/* Limit the lifetimes recorded in the file to 1 week */
		if ((uint32_t)pr->pr_PreferredLifetime > NDP_MAX_SAVE_LIFETIME)
			expiry_time = NDP_MAX_SAVE_LIFETIME;
		else
			expiry_time = pr->pr_PreferredLifetime;
		expiry_time /= 1000;
		expiry_time += time(NULL);
		(void) localtime_r(&expiry_time, &tms);
		(void) strftime(pbuf, sizeof (pbuf), "%Y-%m-%d %R", &tms);

		if ((uint32_t)pr->pr_ValidLifetime > NDP_MAX_SAVE_LIFETIME)
			expiry_time = NDP_MAX_SAVE_LIFETIME;
		else
			expiry_time = pr->pr_ValidLifetime;
		expiry_time /= 1000;
		expiry_time += time(NULL);
		(void) localtime_r(&expiry_time, &tms);
		(void) strftime(vbuf, sizeof (vbuf), "%Y-%m-%d %R", &tms);

		(void) fprintf(fp, "%s/%u '%s' '%s'\n",
		    inet_ntop(AF_INET6, (void *)&pr->pr_address,
		    abuf, sizeof (abuf)), pr->pr_prefix_len, pbuf, vbuf);
	}
	if (fflush(fp) < 0)
		logperror_pi(pi, "phyint_write_state_file: fflush");
	if (fsync(fileno(fp)) < 0)
		logperror_pi(pi, "phyint_write_state_file: fsync");
	if (fclose(fp) < 0)
		logperror_pi(pi, "phyint_write_state_file: fclose");

	pi->pi_prefix_time_since_saved = 0;
}

/*
 * Read the state file for this phyint and create all the addrconf
 * prefixes it contains that still have remaining valid lifetime.
 */
void
phyint_read_state_file(struct phyint *pi)
{
	FILE *fp;
	char filename[BUFSIZ];
	struct prefix *pr;
	struct in6_addr addr;
	struct in6_addr prefix;
	int prefixlen;
	char line[MAXLINELEN];
	int argcount;
	char *argvec[MAXARGSPERLINE];
	struct tm tms;
	time_t ptime, vtime;
	int lineno = 0;

	if (debug & D_PHYINT)
		logdebug("phyint_read_state_file(%s)\n", pi->pi_name);

	(void) strncpy(filename, NDP_STATE_FILE, sizeof (filename));
	filename[sizeof (filename) - 1] = '\0';
	(void) strncat(filename, pi->pi_name,
	    sizeof (filename) - strlen(filename));
	filename[sizeof (filename) - 1] = '\0';
	fp = fopen(filename, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			if (debug & D_PHYINT)
				logdebug("phyint_read_state_file: no file\n");
			return;
		}
		logperror_pi(pi, "phyint_read_state_file: fopen");
		logerr("phyint_read_state_file: can't open %s\n",
		    filename);
		return;
	}
	while (readline(fp, line, sizeof (line), &lineno) != 0) {
		argcount = parse_line(line, argvec,
		    sizeof (argvec) / sizeof (argvec[0]), lineno);
		if (debug & D_PARSE) {
			int i;

			logdebug("scanned %d args\n", argcount);
			for (i = 0; i < argcount; i++)
				logdebug("arg[%d]: %s\n", i, argvec[i]);
		}
		if (argcount == 0) {
			/* Empty line - or comment only line */
			continue;
		}
		if (argcount != 3) {
			logerr("phyint_read_state_file: wrong number of "
			    "fields %d in %s\n",
			    argcount, filename);
			break;
		}
		prefixlen = parse_addrprefix(argvec[0], &addr);
		if (prefixlen == -1) {
			logerr("phyint_read_state_file: bad prefix <%s>\n",
			    argvec[0]);
			continue;
		}
		if (strptime(argvec[1], "%Y-%m-%d %R", &tms) == NULL) {
			logerr("phyint_read_state_file: bad date <%s>\n",
			    argvec[1]);
			continue;
		}
		ptime = mktime(&tms) - time(NULL);
		if (strptime(argvec[2], "%Y-%m-%d %R", &tms) == NULL) {
			logerr("phyint_read_state_file: bad date <%s>\n",
			    argvec[2]);
			continue;
		}
		vtime = mktime(&tms) - time(NULL);
		if (debug & D_PHYINT) {
			logdebug("phyint_read_state_file: vtime %d, ptime %d\n",
			    vtime, ptime);
		}
		/* Ignore the prefix if it is no longer valid. */
		if (vtime < 0)
			continue;
		if (ptime < 0)
			ptime = 0;
		/*
		 * Note: the file contains the address with /N where
		 * some of the (IPV6_ABITS-N) last bits are non-zero.
		 * Thus we need to truncate those when storing as a prefix.
		 */
		prefix_set(&prefix, addr, prefixlen);
		pr = prefix_create(pi, prefix, prefixlen);
		if (pr == NULL)
			continue;
		pr->pr_state |= PR_AUTO;
		pr->pr_address = addr;

		if (IN6_ARE_ADDR_EQUAL(&pr->pr_address, &pr->pr_prefix)) {
			char abuf[INET6_ADDRSTRLEN];

			logerr("phyint_read_state_file: addr == prefix "
			    "IGNORED if %s prefix %s/%u\n",
			    pr->pr_physical->pi_name,
			    inet_ntop(AF_INET6, (void *)&pr->pr_address,
			    abuf, sizeof (abuf)), pr->pr_prefix_len);
			prefix_delete(pr);
			continue;
		}

		if (ptime == 0 && (pr->pr_state & PR_AUTO))
			pr->pr_state |= PR_DEPRECATED;
		pr->pr_ValidLifetime = vtime * 1000;
		pr->pr_PreferredLifetime = ptime * 1000;
		if (pr->pr_kernel_state != pr->pr_state)
			prefix_update_k(pr);
	}

	if (fclose(fp) < 0)
		logperror_pi(pi, "phyint_read_state_file: fclose");

	if (debug & D_PHYINT)
		logdebug("phyint_read_state_file: done\n");

}

/*
 * Randomize pi->pi_ReachableTime.
 * Done periodically when there are no RAs and at a maximum frequency when
 * RA's arrive.
 * Assumes that caller has determined that it is time to generate
 * a new random ReachableTime.
 */
void
phyint_reach_random(struct phyint *pi, boolean_t set_needed)
{
	pi->pi_ReachableTime = GET_RANDOM(
	    (int)(ND_MIN_RANDOM_FACTOR * pi->pi_BaseReachableTime),
	    (int)(ND_MAX_RANDOM_FACTOR * pi->pi_BaseReachableTime));
	if (set_needed) {
		struct lifreq lifr;

		(void) strncpy(lifr.lifr_name, pi->pi_name,
		    sizeof (lifr.lifr_name));
		pi->pi_name[sizeof (pi->pi_name) - 1] = '\0';
		if (ioctl(pi->pi_sock, SIOCGLIFLNKINFO, (char *)&lifr) < 0) {
			logperror_pi(pi,
			    "phyint_reach_random: SIOCGLIFLNKINFO");
			return;
		}
		lifr.lifr_ifinfo.lir_reachtime = pi->pi_ReachableTime;
		if (ioctl(pi->pi_sock, SIOCSLIFLNKINFO, (char *)&lifr) < 0) {
			logperror_pi(pi,
			    "phyint_reach_random: SIOCSLIFLNKINFO");
			return;
		}
	}
	pi->pi_reach_time_since_random = 0;
}

/*
 * Lookup prefix structure that matches the prefix and prefix length.
 * Assumes that the bits after prefixlen might not be zero.
 */
struct prefix *
prefix_lookup(struct phyint *pi, struct in6_addr prefix, int prefixlen)
{
	struct prefix *pr;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_PREFIX) {
		logdebug("prefix_lookup(%s, %s/%u)\n", pi->pi_name,
		    inet_ntop(AF_INET6, (void *)&prefix,
		    abuf, sizeof (abuf)), prefixlen);
	}

	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
		if (pr->pr_prefix_len == prefixlen &&
		    prefix_equal(prefix, pr->pr_prefix, prefixlen))
			return (pr);
	}
	return (NULL);
}

/*
 * Compare two prefixes that have the same prefix length.
 * Fails if the prefix length is unreasonable.
 */
static boolean_t
prefix_equal(struct in6_addr p1, struct in6_addr p2, int prefix_len)
{
	uchar_t mask;
	int j;

	if (prefix_len < 0 || prefix_len > IPV6_ABITS)
		return (_B_FALSE);

	for (j = 0; prefix_len > 8; prefix_len -= 8, j++)
		if (p1.s6_addr[j] != p2.s6_addr[j])
			return (_B_FALSE);

	/* Make the N leftmost bits one */
	mask = 0xff << (8 - prefix_len);
	if ((p1.s6_addr[j] & mask) != (p2.s6_addr[j] & mask))
		return (_B_FALSE);

	return (_B_TRUE);
}

/*
 * Set a prefix from an address and a prefix length.
 * Force all the bits after the prefix length to be zero.
 */
static void
prefix_set(struct in6_addr *prefix, struct in6_addr addr, int prefix_len)
{
	uchar_t mask;
	int j;

	if (prefix_len < 0 || prefix_len > IPV6_ABITS)
		return;

	bzero((char *)prefix, sizeof (*prefix));

	for (j = 0; prefix_len > 8; prefix_len -= 8, j++)
		prefix->s6_addr[j] = addr.s6_addr[j];

	/* Make the N leftmost bits one */
	mask = 0xff << (8 - prefix_len);
	prefix->s6_addr[j] = addr.s6_addr[j] & mask;
}

/*
 * Lookup a prefix based on the kernel's interface name.
 */
struct prefix *
prefix_lookup_name(struct phyint *pi, char *name)
{
	struct prefix *pr;

	if (debug & D_PREFIX) {
		logdebug("prefix_lookup_name(%s, %s)\n", pi->pi_name, name);
	}
	if (name[0] == '\0')
		return (NULL);

	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
		if (strcmp(name, pr->pr_name) == 0)
			return (pr);
	}
	return (NULL);
}

/*
 * Search the phyints list to make sure that this new prefix does
 * not already exist in any  other physical interfaces that have
 * the same address as this one
 */
struct prefix *
prefix_lookup_addr_match(struct prefix *pr)
{
	char abuf[INET6_ADDRSTRLEN];
	struct phyint *pi;
	struct prefix *otherpr = NULL;
	struct in6_addr prefix;
	int	prefixlen;

	if (debug & D_PREFIX) {
		logdebug("prefix_lookup_addr_match(%s/%u)\n",
		    inet_ntop(AF_INET6, (void *)&pr->pr_address,
		    abuf, sizeof (abuf)), pr->pr_prefix_len);
	}
	prefix = pr->pr_prefix;
	prefixlen = pr->pr_prefix_len;
	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		otherpr = prefix_lookup(pi, prefix, prefixlen);
		if (otherpr == pr)
			continue;
		if (otherpr != NULL && (otherpr->pr_state & PR_AUTO) &&
		    IN6_ARE_ADDR_EQUAL(&pr->pr_address,
		    &otherpr->pr_address))
			return (otherpr);
	}
	return (NULL);
}

/*
 * Initialize a new prefix without setting lifetimes etc.
 */
struct prefix *
prefix_create(struct phyint *pi, struct in6_addr prefix, int prefixlen)
{
	struct prefix *pr;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_PREFIX) {
		logdebug("prefix_create(%s, %s/%u)\n", pi->pi_name,
		    inet_ntop(AF_INET6, (void *)&prefix,
		    abuf, sizeof (abuf)), prefixlen);
	}
	pr = (struct prefix *)calloc(sizeof (struct prefix), 1);
	if (pr == NULL) {
		logerr("prefix_create: out of memory\n");
		return (NULL);
	}
	/*
	 * The prefix might have non-zero bits after the prefix len bits.
	 * Force them to be zero.
	 */
	prefix_set(&pr->pr_prefix, prefix, prefixlen);
	pr->pr_prefix_len = prefixlen;
	pr->pr_PreferredLifetime = PREFIX_INFINITY;
	pr->pr_ValidLifetime = PREFIX_INFINITY;
	pr->pr_OnLinkLifetime = PREFIX_INFINITY;
	pr->pr_kernel_state = 0;
	prefix_insert(pi, pr);
	return (pr);
}

/*
 * Create a new named prefix. Caller should use prefix_init_from_k
 * to initialize the content.
 */
struct prefix *
prefix_create_name(struct phyint *pi, char *name)
{
	struct prefix *pr;

	if (debug & D_PREFIX) {
		logdebug("prefix_create_name(%s, %s)\n", pi->pi_name, name);
	}
	pr = (struct prefix *)calloc(sizeof (struct prefix), 1);
	if (pr == NULL) {
		logerr("prefix_create_name: out of memory\n");
		return (NULL);
	}
	(void) strncpy(pr->pr_name, name, sizeof (pr->pr_name));
	pr->pr_name[sizeof (pr->pr_name) - 1] = '\0';
	prefix_insert(pi, pr);
	return (pr);
}

/* Insert in linked list */
static void
prefix_insert(struct phyint *pi, struct prefix *pr)
{
	pr->pr_next = pi->pi_prefix_list;
	pr->pr_prev = NULL;
	if (pi->pi_prefix_list != NULL)
		pi->pi_prefix_list->pr_prev = pr;
	pi->pi_prefix_list = pr;
	pr->pr_physical = pi;
}

/*
 * Initialize the prefix from the content of the kernel.
 * If IFF_ADDRCONF is set we treat it as PR_AUTO (i.e. an addrconf
 * prefix). However, we not derive the lifetimes from
 * the kernel thus they are set to 1 week.
 * Ignore the prefix if the interface is not IFF_UP.
 */
int
prefix_init_from_k(struct prefix *pr)
{
	struct lifreq lifr;
	struct sockaddr_in6 *sin6;
	int sock = pr->pr_physical->pi_sock;

	(void) strncpy(lifr.lifr_name, pr->pr_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(sock, SIOCGLIFADDR, (char *)&lifr) < 0) {
		logperror_pr(pr, "prefix_init_from_k: ioctl (get addr)");
		goto error;
	}
	if (lifr.lifr_addr.ss_family != AF_INET6) {
		logerr("prefix_init_from_k: wrong af\n");
		goto error;
	}
	sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
	pr->pr_address = sin6->sin6_addr;

	if (ioctl(sock, SIOCGLIFFLAGS, (char *)&lifr) < 0) {
		logperror_pr(pr, "prefix_init_from_k: ioctl (get flags)");
		goto error;
	}
	pr->pr_flags = lifr.lifr_flags;

	if (ioctl(sock, SIOCGLIFSUBNET, (char *)&lifr) < 0) {
		logperror_pr(pr, "prefix_init_from_k: ioctl (get subnet)");
		goto error;
	}
	if (lifr.lifr_subnet.ss_family != AF_INET6) {
		logerr("prefix_init_from_k: wrong af\n");
		goto error;
	}
	/*
	 * Guard against the prefix having non-zero bits after the prefix
	 * len bits.
	 */
	sin6 = (struct sockaddr_in6 *)&lifr.lifr_subnet;
	pr->pr_prefix_len = lifr.lifr_addrlen;
	prefix_set(&pr->pr_prefix, sin6->sin6_addr, pr->pr_prefix_len);

	if (pr->pr_prefix_len != IPV6_ABITS && (pr->pr_flags & IFF_UP) &&
	    IN6_ARE_ADDR_EQUAL(&pr->pr_address, &pr->pr_prefix)) {
		char abuf[INET6_ADDRSTRLEN];

		logerr("prefix_init_from_k: addr == prefix "
		    "IGNORED if %s prefix %s/%u\n",
		    pr->pr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_address,
			abuf, sizeof (abuf)), pr->pr_prefix_len);
		goto error;
	}
	pr->pr_kernel_state = 0;
	if (pr->pr_prefix_len != IPV6_ABITS)
		pr->pr_kernel_state |= PR_ONLINK;
	if (!(pr->pr_flags & IFF_NOLOCAL))
		pr->pr_kernel_state |= PR_AUTO;
	if ((pr->pr_flags & IFF_DEPRECATED) && (pr->pr_kernel_state & PR_AUTO))
		pr->pr_kernel_state |= PR_DEPRECATED;
	if (!(pr->pr_flags & IFF_ADDRCONF)) {
		/* Prevent ndpd from stepping on this prefix */
		pr->pr_kernel_state |= PR_STATIC;
	}
	pr->pr_state = pr->pr_kernel_state;
	/* Adjust pr_prefix_len based if PR_AUTO is set */
	if (pr->pr_state & PR_AUTO) {
		pr->pr_prefix_len =
		    IPV6_ABITS - pr->pr_physical->pi_token_length;
		prefix_set(&pr->pr_prefix, pr->pr_prefix, pr->pr_prefix_len);
	}

	/* Can't extract lifetimes from the kernel - use 1 week */
	pr->pr_ValidLifetime = NDP_MAX_SAVE_LIFETIME;
	pr->pr_PreferredLifetime = NDP_MAX_SAVE_LIFETIME;
	pr->pr_OnLinkLifetime = NDP_MAX_SAVE_LIFETIME;
	if (pr->pr_kernel_state == 0)
		pr->pr_name[0] = '\0';
	return (0);

error:
	/* Pretend that the prefix does not exist in the kernel */
	pr->pr_kernel_state = 0;
	pr->pr_name[0] = '\0';
	return (-1);
}

/*
 * Delete (unlink and free) and remove from kernel if the prefix
 * was added by in.ndpd (i.e. PR_STATIC is not set).
 * Handles delete of things that have not yet been inserted in the list
 * i.e. pr_physical is NULL.
 */
void
prefix_delete(struct prefix *pr)
{
	struct phyint *pi;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_PREFIX) {
		logdebug("prefix_delete(%s, %s, %s/%u)\n",
		    pr->pr_physical->pi_name,
		    pr->pr_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
		    abuf, sizeof (abuf)), pr->pr_prefix_len);
	}
	/* Remove non-static prefixes from the kernel. */
	pr->pr_state &= PR_STATIC;
	if (pr->pr_kernel_state != pr->pr_state)
		prefix_update_k(pr);

	pi = pr->pr_physical;
	if (pr->pr_prev == NULL) {
		if (pi != NULL)
			pi->pi_prefix_list = pr->pr_next;
	} else {
		pr->pr_prev->pr_next = pr->pr_next;
	}
	if (pr->pr_next != NULL)
		pr->pr_next->pr_prev = pr->pr_prev;
	pr->pr_next = pr->pr_prev = NULL;
	free(pr);
}

/*
 * Toggle one or more IFF_ flags for a prefix. Turn on 'onflags' and
 * turn off 'offflags'.
 */
static int
prefix_modify_flags(struct prefix *pr, int onflags, int offflags)
{
	struct lifreq lifr;
	struct phyint *pi = pr->pr_physical;
	int old_flags;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_PREFIX) {
		logdebug("prefix_modify_flags(%s, %s, %s/%u) "
		    "flags %x on %x off %x\n",
		    pr->pr_physical->pi_name,
		    pr->pr_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
		    abuf, sizeof (abuf)), pr->pr_prefix_len,
		    pr->pr_flags, onflags, offflags);
	}
	/* Assumes that only the PR_STATIC link-local matches the pi_name */
	if (!(pr->pr_state & PR_STATIC) &&
	    strcmp(pr->pr_name, pi->pi_name) == 0) {
		logerr("prefix_modify_flags(%s, on %x, off %x): name matches "
		    "interface name\n",
		    pi->pi_name, onflags, offflags);
		return (-1);
	}

	(void) strncpy(lifr.lifr_name, pr->pr_name, sizeof (lifr.lifr_name));
	lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
	if (ioctl(pi->pi_sock, SIOCGLIFFLAGS, (char *)&lifr) < 0) {
		logperror_pr(pr, "prefix_modify_flags: SIOCGLIFFLAGS");
		logerr("prefix_modify_flags(%s, %s) old 0x%x "
		    "on 0x%x off 0x%x\n",
		    pr->pr_physical->pi_name,
		    pr->pr_name,
		    pr->pr_flags, onflags, offflags);
		return (-1);
	}
	old_flags = lifr.lifr_flags;
	lifr.lifr_flags |= onflags;
	lifr.lifr_flags &= ~offflags;
	pr->pr_flags = lifr.lifr_flags;
	if (ioctl(pi->pi_sock, SIOCSLIFFLAGS, (char *)&lifr) < 0) {
		logperror_pr(pr, "prefix_modify_flags: SIOCSLIFFLAGS");
		logerr("prefix_modify_flags(%s, %s) old 0x%x new 0x%x "
		    "on 0x%x off 0x%x\n",
		    pr->pr_physical->pi_name,
		    pr->pr_name,
		    old_flags, (int)lifr.lifr_flags, onflags, offflags);
		return (-1);
	}
	return (0);
}

/*
 * Make the kernel state match what is in the prefix structure.
 * This includes creating the prefix (allocating a new interface name)
 * as well as setting the local address and on-link subnet prefix
 * and controlling the IFF_ADDRCONF and IFF_DEPRECATED flags.
 */
void
prefix_update_k(struct prefix *pr)
{
	struct lifreq lifr;
	char abuf[INET6_ADDRSTRLEN];
	char buf1[512], buf2[512];
	struct phyint *pi = pr->pr_physical;
	struct sockaddr_in6 *sin6;

	if (debug & D_PREFIX) {
		logdebug("prefix_update_k(%s, %s, %s/%u) from %s to %s\n",
		    pr->pr_physical->pi_name,
		    pr->pr_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
		    abuf, sizeof (abuf)), pr->pr_prefix_len,
		    prefix_print_state(pr->pr_kernel_state, buf1),
		    prefix_print_state(pr->pr_state, buf2));
	}
	if (pr->pr_kernel_state == pr->pr_state)
		return;		/* No changes */

	/* Skip static prefixes */
	if (pr->pr_state & PR_STATIC)
		return;

	if (pr->pr_kernel_state == 0) {
		/*
		 * Create a new logical interface name and store in pr_name.
		 * Set IFF_ADDRCONF. Do not set an address (yet).
		 */
		if (pr->pr_name[0] != '\0') {
			/* Name already set! */
			logerr("prefix_update_k(%s, %s, %s/%u) from %s to %s "
				"name is already allocated\n",
				pr->pr_physical->pi_name,
				pr->pr_name,
				inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
				abuf, sizeof (abuf)), pr->pr_prefix_len,
				prefix_print_state(pr->pr_kernel_state, buf1),
				prefix_print_state(pr->pr_state, buf2));
			return;
		}

		(void) strncpy(lifr.lifr_name, pi->pi_name,
		    sizeof (lifr.lifr_name));
		lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
		lifr.lifr_addr.ss_family = AF_UNSPEC;
		if (ioctl(pi->pi_sock, SIOCLIFADDIF, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCLIFADDIF");
			return;
		}
		(void) strncpy(pr->pr_name, lifr.lifr_name,
		    sizeof (pr->pr_name));
		pr->pr_name[sizeof (pr->pr_name) - 1] = '\0';
		if (debug & D_PREFIX) {
			logdebug("prefix_update_k: new name %s\n",
			    pr->pr_name);
		}
		if (prefix_modify_flags(pr, IFF_ADDRCONF, 0) == -1)
			return;
	}
	if ((pr->pr_state & (PR_ONLINK|PR_AUTO)) == 0) {
		/* Remove the interface */
		if (prefix_modify_flags(pr, 0, IFF_UP|IFF_DEPRECATED) == -1)
			return;
		(void) strncpy(lifr.lifr_name, pr->pr_name,
		    sizeof (lifr.lifr_name));
		lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';

		if (debug & D_PREFIX) {
			logdebug("prefix_update_k: remove name %s\n",
			    pr->pr_name);
		}

		/*
		 * Assumes that only the PR_STATIC link-local matches
		 * the pi_name
		 */
		if (!(pr->pr_state & PR_STATIC) &&
		    strcmp(pr->pr_name, pi->pi_name) == 0) {
			logerr("prefix_update_k(%s): name matches if\n",
			    pi->pi_name);
			return;
		}

		/* Remove logical interface based on pr_name */
		lifr.lifr_addr.ss_family = AF_UNSPEC;
		if (ioctl(pi->pi_sock, SIOCLIFREMOVEIF, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCLIFREMOVEIF");
		}
		pr->pr_kernel_state = 0;
		pr->pr_name[0] = '\0';
		return;
	}
	if ((pr->pr_state & PR_AUTO) && !(pr->pr_kernel_state & PR_AUTO)) {
		/*
		 * Set local address and set the prefix length to 128.
		 * Turn off IFF_NOLOCAL in case it was set.
		 * Turn on IFF_UP.
		 */
		(void) strncpy(lifr.lifr_name, pr->pr_name,
		    sizeof (lifr.lifr_name));
		lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
		sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
		bzero(sin6, sizeof (struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_addr = pr->pr_address;
		if (debug & D_PREFIX) {
			logdebug("prefix_update_k(%s) set addr %s "
			    "for PR_AUTO on\n",
			    pr->pr_name,
			    inet_ntop(AF_INET6, (void *)&pr->pr_address,
				abuf, sizeof (abuf)));
		}
		if (ioctl(pi->pi_sock, SIOCSLIFADDR, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCSLIFADDR");
			return;
		}
		if (pr->pr_state & PR_ONLINK) {
			sin6->sin6_addr = pr->pr_prefix;
			lifr.lifr_addrlen = pr->pr_prefix_len;
		} else {
			sin6->sin6_addr = pr->pr_address;
			lifr.lifr_addrlen = IPV6_ABITS;
		}
		if (debug & D_PREFIX) {
			logdebug("prefix_update_k(%s) set subnet %s/%u "
			    "for PR_AUTO on\n",
			    pr->pr_name,
			    inet_ntop(AF_INET6, (void *)&sin6->sin6_addr,
				abuf, sizeof (abuf)), lifr.lifr_addrlen);
		}
		if (ioctl(pi->pi_sock, SIOCSLIFSUBNET, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCSLIFSUBNET");
			return;
		}
		/*
		 * For ptp interfaces, create a destination based on
		 * prefix and prefix len together with the remote token
		 * extracted from the remote pt-pt address.  This is used by
		 * ip to choose a proper source for outgoing packets.
		 */
		if (pi->pi_flags & IFF_POINTOPOINT) {
			int i;

			sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
			bzero(sin6, sizeof (struct sockaddr_in6));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = pr->pr_prefix;
			for (i = 0; i < 16; i++) {
				sin6->sin6_addr.s6_addr[i] |=
				    pi->pi_dst_token.s6_addr[i];
			}
			if (debug & D_PREFIX) {
				logdebug("prefix_update_k(%s) set dstaddr %s "
				    "for PR_AUTO on\n",
				    pr->pr_name,
				    inet_ntop(AF_INET6,
				    (void *)&sin6->sin6_addr,
				    abuf, sizeof (abuf)));
			}
			if (ioctl(pi->pi_sock, SIOCSLIFDSTADDR,
			    (char *)&lifr) < 0) {
				logperror_pr(pr,
				    "prefix_update_k: SIOCSLIFDSTADDR");
				return;
			}
		}
		if (prefix_modify_flags(pr, IFF_UP, IFF_NOLOCAL) == -1)
			return;
		pr->pr_kernel_state |= PR_AUTO;
		if (pr->pr_state & PR_ONLINK)
			pr->pr_kernel_state |= PR_ONLINK;
		else
			pr->pr_kernel_state &= ~PR_ONLINK;
	}
	if (!(pr->pr_state & PR_AUTO) && (pr->pr_kernel_state & PR_AUTO)) {
		/* Turn on IFF_NOLOCAL and set the local address to all zero */
		if (prefix_modify_flags(pr, IFF_NOLOCAL, 0) == -1)
			return;
		(void) strncpy(lifr.lifr_name, pr->pr_name,
		    sizeof (lifr.lifr_name));
		lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
		sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
		bzero(sin6, sizeof (struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		if (debug & D_PREFIX) {
			logdebug("prefix_update_k(%s) set addr %s "
			    "for PR_AUTO off\n",
			    pr->pr_name,
			    inet_ntop(AF_INET6, (void *)&sin6->sin6_addr,
				abuf, sizeof (abuf)));
		}
		if (ioctl(pi->pi_sock, SIOCSLIFADDR, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCSLIFADDR");
			return;
		}
		pr->pr_kernel_state &= ~PR_AUTO;
	}
	if ((pr->pr_state & PR_DEPRECATED) &&
	    !(pr->pr_kernel_state & PR_DEPRECATED) &&
	    (pr->pr_kernel_state & PR_AUTO)) {
		/* Only applies if PR_AUTO */
		if (prefix_modify_flags(pr, IFF_DEPRECATED, 0) == -1)
			return;
		pr->pr_kernel_state |= PR_DEPRECATED;
	}
	if (!(pr->pr_state & PR_DEPRECATED) &&
	    (pr->pr_kernel_state & PR_DEPRECATED)) {
		if (prefix_modify_flags(pr, 0, IFF_DEPRECATED) == -1)
			return;
		pr->pr_kernel_state &= ~PR_DEPRECATED;
	}
	if ((pr->pr_state & PR_ONLINK) && !(pr->pr_kernel_state & PR_ONLINK)) {
		/* Set the subnet and set IFF_UP */
		(void) strncpy(lifr.lifr_name, pr->pr_name,
		    sizeof (lifr.lifr_name));
		lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
		sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
		bzero(sin6, sizeof (struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_addr = pr->pr_prefix;
		lifr.lifr_addrlen = pr->pr_prefix_len;
		if (debug & D_PREFIX) {
			logdebug("prefix_update_k(%s) set subnet %s/%d "
			    "for PR_ONLINK on\n",
			    pr->pr_name,
			    inet_ntop(AF_INET6, (void *)&sin6->sin6_addr,
				abuf, sizeof (abuf)), lifr.lifr_addrlen);
		}
		if (ioctl(pi->pi_sock, SIOCSLIFSUBNET, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCSLIFSUBNET");
			return;
		}
		if (!(pr->pr_state & PR_AUTO)) {
			if (prefix_modify_flags(pr, IFF_NOLOCAL, 0) == -1)
				return;
		}
		if (prefix_modify_flags(pr, IFF_UP, 0) == -1)
			return;
		pr->pr_kernel_state |= PR_ONLINK;
	}
	if (!(pr->pr_state & PR_ONLINK) && (pr->pr_kernel_state & PR_ONLINK)) {
		/* Set the prefixlen to 128 */
		(void) strncpy(lifr.lifr_name, pr->pr_name,
		    sizeof (lifr.lifr_name));
		lifr.lifr_name[sizeof (lifr.lifr_name) - 1] = '\0';
		sin6 = (struct sockaddr_in6 *)&lifr.lifr_addr;
		bzero(sin6, sizeof (struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_addr = pr->pr_address;
		lifr.lifr_addrlen = IPV6_ABITS;
		if (debug & D_PREFIX) {
			logdebug("prefix_update_k(%s) set subnet %s/%d "
			    "for PR_ONLINK off\n",
			    pr->pr_name,
			    inet_ntop(AF_INET6, (void *)&sin6->sin6_addr,
				abuf, sizeof (abuf)), lifr.lifr_addrlen);
		}
		if (ioctl(pi->pi_sock, SIOCSLIFSUBNET, (char *)&lifr) < 0) {
			logperror_pr(pr, "prefix_update_k: SIOCSLIFSUBNET");
			return;
		}
		pr->pr_kernel_state &= ~PR_ONLINK;
	}
}

/*
 * Called with the number of millseconds elapsed since the last call.
 * Determines if any timeout event has occurred and
 * returns the number of milliseconds until the next timeout event.
 * Returns TIMER_INFINITY for "never".
 */
uint_t
prefix_timer(struct prefix *pr, uint_t elapsed)
{
	int seconds_elapsed = (elapsed + 500) / 1000;	/* Rounded */
	uint_t next = TIMER_INFINITY;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_PREFIX) {
		logdebug("prefix_timer(%s, %s/%u, %d) "
		    "valid %u pref %u onlink %u\n",
		    pr->pr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&pr->pr_prefix,
		    abuf, sizeof (abuf)), pr->pr_prefix_len,
		    elapsed, pr->pr_ValidLifetime, pr->pr_PreferredLifetime,
		    pr->pr_OnLinkLifetime);
	}

	/* Exclude static prefixes */
	if (pr->pr_state & PR_STATIC)
		return (next);

	/* Decrement Expire time left for real-time lifetimes */
	if (pr->pr_AdvValidRealTime) {
		if (pr->pr_AdvValidExpiration > seconds_elapsed)
			pr->pr_AdvValidExpiration -= seconds_elapsed;
		else
			pr->pr_AdvValidExpiration = 0;
	}
	if (pr->pr_AdvPreferredRealTime) {
		if (pr->pr_AdvPreferredExpiration > seconds_elapsed)
			pr->pr_AdvPreferredExpiration -= seconds_elapsed;
		else
			pr->pr_AdvPreferredExpiration = 0;
	}

	if (pr->pr_AutonomousFlag &&
	    (pr->pr_PreferredLifetime != PREFIX_INFINITY)) {
		if (pr->pr_PreferredLifetime <= elapsed) {
			pr->pr_PreferredLifetime = 0;
		} else {
			pr->pr_PreferredLifetime -= elapsed;
			if (pr->pr_PreferredLifetime < next)
				next = pr->pr_PreferredLifetime;
		}
	}
	if (pr->pr_AutonomousFlag &&
	    (pr->pr_ValidLifetime != PREFIX_INFINITY)) {
		if (pr->pr_ValidLifetime <= elapsed) {
			pr->pr_ValidLifetime = 0;
		} else {
			pr->pr_ValidLifetime -= elapsed;
			if (pr->pr_ValidLifetime < next)
				next = pr->pr_ValidLifetime;
		}
	}
	if (pr->pr_OnLinkFlag &&
	    (pr->pr_OnLinkLifetime != PREFIX_INFINITY)) {
		if (pr->pr_OnLinkLifetime <= elapsed) {
			pr->pr_OnLinkLifetime = 0;
		} else {
			pr->pr_OnLinkLifetime -= elapsed;
			if (pr->pr_OnLinkLifetime < next)
				next = pr->pr_OnLinkLifetime;
		}
	}
	if (pr->pr_AutonomousFlag && pr->pr_ValidLifetime == 0)
		pr->pr_state &= ~(PR_AUTO|PR_DEPRECATED);
	if (pr->pr_AutonomousFlag && pr->pr_PreferredLifetime == 0 &&
	    (pr->pr_state & PR_AUTO))
		pr->pr_state |= PR_DEPRECATED;
	if (pr->pr_OnLinkFlag && pr->pr_OnLinkLifetime == 0)
		pr->pr_state &= ~PR_ONLINK;

	if (pr->pr_state != pr->pr_kernel_state) {
		/* Might cause prefix to be deleted! */

		/* Log a message when an addrconf prefix goes away */
		if ((pr->pr_kernel_state & PR_AUTO) &&
		    !(pr->pr_state & PR_AUTO)) {
			char abuf[INET6_ADDRSTRLEN];

			logwarn("Address removed due to timeout %s\n",
			    inet_ntop(AF_INET6, (void *)&pr->pr_address,
			    abuf, sizeof (abuf)));
		}
		prefix_update_k(pr);
	}

	return (next);
}

static char *
prefix_print_state(int state, char *buf)
{
	char *cp;

	cp = buf;
	cp[0] = '\0';

	if (state & PR_ONLINK) {
		(void) strcat(cp, "ONLINK ");
		cp += strlen(cp);
	}
	if (state & PR_AUTO) {
		(void) strcat(cp, "AUTO ");
		cp += strlen(cp);
	}
	if (state & PR_DEPRECATED) {
		(void) strcat(cp, "DEPRECATED ");
		cp += strlen(cp);
	}
	if (state & PR_STATIC) {
		(void) strcat(cp, "STATIC ");
		cp += strlen(cp);
	}
	return (buf);
}

static void
prefix_print(struct prefix *pr)
{
	char abuf[INET6_ADDRSTRLEN];
	char buf1[512], buf2[512];

	logdebug("Prefix name: %s prefix %s/%u state %s kernel_state %s\n",
	    pr->pr_name,
	    inet_ntop(AF_INET6, (void *)&pr->pr_prefix, abuf, sizeof (abuf)),
	    pr->pr_prefix_len,
	    prefix_print_state(pr->pr_state, buf2),
	    prefix_print_state(pr->pr_kernel_state, buf1));
	logdebug("\tAddress: %s flags %x in_use %d\n",
	    inet_ntop(AF_INET6, (void *)&pr->pr_address, abuf, sizeof (abuf)),
	    pr->pr_flags, pr->pr_in_use);
	logdebug("\tValidLifetime %u PreferredLifetime %u OnLinkLifetime %u\n",
	    pr->pr_ValidLifetime, pr->pr_PreferredLifetime,
	    pr->pr_OnLinkLifetime);
	logdebug("\tOnLink %d Auto %d\n",
	    pr->pr_OnLinkFlag, pr->pr_AutonomousFlag);
	if (pr->pr_physical->pi_AdvSendAdvertisements)
		print_prefixlist(pr->pr_config);
	logdebug("\n");
}

/* Lookup router on its link-local IPv6 address */
struct router *
router_lookup(struct phyint *pi, struct in6_addr addr)
{
	struct router *dr;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_ROUTER) {
		logdebug("router_lookup(%s, %s)\n", pi->pi_name,
		    inet_ntop(AF_INET6, (void *)&addr,
		    abuf, sizeof (abuf)));
	}

	for (dr = pi->pi_router_list; dr != NULL; dr = dr->dr_next) {
		if (bcmp((char *)&addr, (char *)&dr->dr_address,
		    sizeof (addr)) == 0)
			return (dr);
	}
	return (NULL);
}

/*
 * Create a default router entry.
 * The lifetime parameter is in seconds.
 */
struct router *
router_create(struct phyint *pi, struct in6_addr addr, uint_t lifetime)
{
	struct router *dr;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_ROUTER) {
		logdebug("router_create(%s, %s, %u)\n", pi->pi_name,
		    inet_ntop(AF_INET6, (void *)&addr,
		    abuf, sizeof (abuf)), lifetime);
	}

	dr = (struct router *)calloc(sizeof (struct router), 1);
	if (dr == NULL) {
		logerr("router_create: out of memory\n");
		return (NULL);
	}
	dr->dr_address = addr;
	dr->dr_lifetime = lifetime;
	router_insert(pi, dr);
	if (dr->dr_lifetime != 0) {
		/*
		 * Delete an onlink default if it exists since we now have
		 * at least one default router.
		 */
		if (pi->pi_onlink_default)
			router_delete_onlink(dr->dr_physical);
		router_add_k(dr);
	}
	return (dr);
}

/* Insert in linked list */
static void
router_insert(struct phyint *pi, struct router *dr)
{
	dr->dr_next = pi->pi_router_list;
	dr->dr_prev = NULL;
	if (pi->pi_router_list != NULL)
		pi->pi_router_list->dr_prev = dr;
	pi->pi_router_list = dr;
	dr->dr_physical = pi;
}

/*
 * Delete (unlink and free).
 * Handles delete of things that have not yet been inserted in the list
 * i.e. dr_physical is NULL.
 */
static void
router_delete(struct router *dr)
{
	struct phyint *pi;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_ROUTER) {
		logdebug("router_delete(%s, %s, %u)\n",
		    dr->dr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&dr->dr_address,
		    abuf, sizeof (abuf)), dr->dr_lifetime);
	}
	if (dr->dr_inkernel) {
		if (!dr->dr_onlink && dr->dr_physical->pi_num_k_routers == 1)
			(void) router_create_onlink(dr->dr_physical);
		router_delete_k(dr);
	}
	pi = dr->dr_physical;
	if (dr->dr_onlink)
		pi->pi_onlink_default = _B_FALSE;

	if (dr->dr_prev == NULL) {
		if (pi != NULL)
			pi->pi_router_list = dr->dr_next;
	} else {
		dr->dr_prev->dr_next = dr->dr_next;
	}
	if (dr->dr_next != NULL)
		dr->dr_next->dr_prev = dr->dr_prev;
	dr->dr_next = dr->dr_prev = NULL;
	free(dr);
}


/* Create an onlink default route */
struct router *
router_create_onlink(struct phyint *pi)
{
	struct router *dr;
	struct prefix *pr;

	if (debug & D_ROUTER) {
		logdebug("router_create_onlink(%s)\n", pi->pi_name);
	}

	if (pi->pi_onlink_default) {
		logerr("router_create_onlink: already an onlink default: %s\n",
		    pi->pi_name);
		return (NULL);
	}

	/*
	 * Find the interface address to use for the route gateway.
	 * We need to use the link-local since the others ones might be
	 * deleted when the prefixes get invalidated.
	 */
	for (pr = pi->pi_prefix_list; pr != NULL; pr = pr->pr_next) {
		if ((pr->pr_state & PR_AUTO) &&
		    IN6_IS_ADDR_LINKLOCAL(&pr->pr_address))
			break;
	}
	if (pr == NULL) {
		logerr("router_create_onlink: no source address\n");
		return (NULL);
	}
	dr = (struct router *)calloc(sizeof (struct router), 1);
	if (dr == NULL) {
		logerr("router_create_onlink: out of memory\n");
		return (NULL);
	}
	dr->dr_address = pr->pr_address;
	dr->dr_lifetime = 1;	/* Not used */
	dr->dr_onlink = _B_TRUE;
	router_insert(pi, dr);

	router_add_k(dr);
	pi->pi_onlink_default = _B_TRUE;
	return (dr);
}

/* Remove an onlink default route */
static void
router_delete_onlink(struct phyint *pi)
{
	struct router *dr, *next_dr;

	if (debug & D_ROUTER) {
		logdebug("router_delete_onlink(%s)\n", pi->pi_name);
	}

	if (!pi->pi_onlink_default) {
		logerr("router_delete_onlink: no onlink default: %s\n",
		    pi->pi_name);
		return;
	}
	/* Find all onlink routes */
	for (dr = pi->pi_router_list; dr != NULL; dr = next_dr) {
		next_dr = dr->dr_next;
		if (dr->dr_onlink)
			router_delete(dr);
	}
}

/*
 * Update the kernel to match dr_lifetime
 */
void
router_update_k(struct router *dr)
{
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_ROUTER) {
		logdebug("router_update_k(%s, %s, %u)\n",
		    dr->dr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&dr->dr_address,
		    abuf, sizeof (abuf)), dr->dr_lifetime);
	}

	if (dr->dr_lifetime == 0 && dr->dr_inkernel) {
		/* Log a message when last router goes away */
		if (dr->dr_physical->pi_num_k_routers == 1) {
			logwarn("Last default router (%s) removed on %s\n",
			    inet_ntop(AF_INET6, (void *)&dr->dr_address,
			    abuf, sizeof (abuf)), dr->dr_physical->pi_name);
		}
		router_delete(dr);
	} else if (dr->dr_lifetime != 0 && !dr->dr_inkernel) {
		/*
		 * Delete an onlink default if it exists since we now have
		 * at least one default router.
		 */
		if (dr->dr_physical->pi_onlink_default)
			router_delete_onlink(dr->dr_physical);
		router_add_k(dr);
	}
}


/*
 * Called with the number of millseconds elapsed since the last call.
 * Determines if any timeout event has occurred and
 * returns the number of milliseconds until the next timeout event.
 * Returns TIMER_INFINITY for "never".
 */
uint_t
router_timer(struct router *dr, uint_t elapsed)
{
	uint_t next = TIMER_INFINITY;
	char abuf[INET6_ADDRSTRLEN];

	if (debug & D_ROUTER) {
		logdebug("router_timer(%s, %s, %u, %d)\n",
		    dr->dr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&dr->dr_address,
		    abuf, sizeof (abuf)), dr->dr_lifetime, elapsed);
	}
	if (dr->dr_onlink) {
		/* No timeout */
		return (next);
	}
	if (dr->dr_lifetime <= elapsed) {
		dr->dr_lifetime = 0;
	} else {
		dr->dr_lifetime -= elapsed;
		if (dr->dr_lifetime < next)
			next = dr->dr_lifetime;
	}

	if (dr->dr_lifetime == 0) {
		/* Log a message when last router goes away */
		if (dr->dr_physical->pi_num_k_routers == 1) {
			logwarn("Last default router (%s) timed out on %s\n",
			    inet_ntop(AF_INET6, (void *)&dr->dr_address,
			    abuf, sizeof (abuf)), dr->dr_physical->pi_name);
		}
		router_delete(dr);
	}
	return (next);
}

/*
 * Add a default route to the kernel (unless the lifetime is zero)
 * Handles onlink default routes.
 */
static void
router_add_k(struct router *dr)
{
	struct phyint *pi = dr->dr_physical;
	char abuf[INET6_ADDRSTRLEN];
	int rlen;

	if (debug & D_ROUTER) {
		logdebug("router_add_k(%s, %s, %u)\n",
		    dr->dr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&dr->dr_address,
		    abuf, sizeof (abuf)), dr->dr_lifetime);
	}

	if (dr->dr_onlink)
		rt_msg->rtm_flags = 0;
	else
		rt_msg->rtm_flags = RTF_GATEWAY;

	rta_gateway->sin6_addr = dr->dr_address;

	rta_ifp->sdl_index = if_nametoindex(pi->pi_name);
	if (rta_ifp->sdl_index == 0) {
		logperror_pi(pi, "router_add_k: if_nametoindex");
		return;
	}

	rt_msg->rtm_type = RTM_ADD;
	rt_msg->rtm_seq = ++rtmseq;
	rlen = write(rtsock, rt_msg, rt_msg->rtm_msglen);
	if (rlen < 0) {
		if (errno != EEXIST) {
			logperror_pi(pi, "router_add_k: RTM_ADD");
			return;
		}
	} else if (rlen < rt_msg->rtm_msglen) {
		logerr("router_add_k: write to routing socket got only %d for "
		    "rlen (interface %s)\n",
		    rlen, pi->pi_name);
		return;
	}
	dr->dr_inkernel = _B_TRUE;
	if (!dr->dr_onlink)
		pi->pi_num_k_routers++;
}

/*
 * Delete a route from the kernel.
 * Handles onlink default routes.
 */
static void
router_delete_k(struct router *dr)
{
	struct phyint *pi = dr->dr_physical;
	char abuf[INET6_ADDRSTRLEN];
	int rlen;

	if (debug & D_ROUTER) {
		logdebug("router_delete_k(%s, %s, %u)\n",
		    dr->dr_physical->pi_name,
		    inet_ntop(AF_INET6, (void *)&dr->dr_address,
		    abuf, sizeof (abuf)), dr->dr_lifetime);
	}

	if (dr->dr_onlink)
		rt_msg->rtm_flags = 0;
	else
		rt_msg->rtm_flags = RTF_GATEWAY;

	rta_gateway->sin6_addr = dr->dr_address;

	rta_ifp->sdl_index = if_nametoindex(pi->pi_name);
	if (rta_ifp->sdl_index == 0) {
		logperror_pi(pi, "router_delete_k: if_nametoindex");
		return;
	}

	rt_msg->rtm_type = RTM_DELETE;
	rt_msg->rtm_seq = ++rtmseq;
	rlen = write(rtsock, rt_msg, rt_msg->rtm_msglen);
	if (rlen < 0) {
		if (errno != ESRCH) {
			logperror_pi(pi, "router_delete_k: RTM_DELETE");
		}
	} else if (rlen < rt_msg->rtm_msglen) {
		logerr("router_delete_k: write to routing socket got only %d "
		    "for rlen (interface %s)\n",
		    rlen, pi->pi_name);
	}
	dr->dr_inkernel = _B_FALSE;
	if (!dr->dr_onlink)
		pi->pi_num_k_routers--;
}


static void
router_print(struct router *dr)
{
	char abuf[INET6_ADDRSTRLEN];

	logdebug("Router %s on %s inkernel %d onlink %d lifetime %u\n",
	    inet_ntop(AF_INET6, (void *)&dr->dr_address,
	    abuf, sizeof (abuf)),
	    dr->dr_physical->pi_name,
	    dr->dr_inkernel, dr->dr_onlink, dr->dr_lifetime);
}


void
phyint_print_all(void)
{
	struct phyint *pi;

	for (pi = phyints; pi != NULL; pi = pi->pi_next) {
		phyint_print(pi);
	}
}
