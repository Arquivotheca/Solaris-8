/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)interfaces.c	1.86	99/10/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stropts.h>
#include <sys/dlpi.h>
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include "pf.h"
#include <v4_sum_impl.h>
#include <locale.h>

static const uchar_t magic_cookie[] = BOOTMAGIC;

static int strioctl(int, int, int, int, char *);
static int dev_ppa(char *);
static char *device_path(char *, char *, int);
static void disp_if(IF *);

/*
 * Network interface configuration. This file contains routines which
 * handle the input side of the DHCP/BOOTP/Relay agent. Multiple interfaces
 * are handled by identifying explicitly each interface, and creating a
 * stream for each. If only one usable interface exists, then a "normal"
 * UDP socket is used for simplicity's sake.
 */

IF	*if_head;		/* head of interfaces list */
mutex_t	if_head_mtx;		/* mutex for adding/deleting IF list entries */
char	*interfaces;		/* user specified interfaces */
static int	num_interfaces;	/* # of usable interfaces on the system */

extern struct packetfilt dhcppf;	/* packet filter in pf.c */

/*
 * DLPI routines we need.
 */
extern int dlinforeq(int, dl_info_ack_t *);
extern int dlattachreq(int, ulong_t);
extern int dldetachreq(int);
extern int dlbindreq(int, ulong_t, ulong_t, ushort_t, ushort_t);
extern int dlunbindreq(int);

#ifdef	DEBUG_PKTLIST
void
display_pktlist(IF *ifp)
{
	PKT_LIST	*wplp;
	char		ntoab[NTOABUF];

	assert(_mutex_held(&ifp->pkt_mtx));
	dhcpmsg(LOG_DEBUG,
	    "Thread %04d - plp\tpkt\tlen\tip\t\ticmp\tprev\tnext\n",
	    thr_self());
	for (wplp = ifp->pkthead; wplp != NULL; wplp = wplp->next) {
		dhcpmsg(LOG_DEBUG, "\t\t0x%x\t0x%x\t%d\t%14s\t%d\t0x%x\t0x%x\n",
		    wplp, wplp->pkt, wplp->len,
		    inet_ntoa_r(wplp->off_ip, ntoab), wplp->d_icmpflag,
		    wplp->prev, wplp->next);
	}
	dhcpmsg(LOG_DEBUG, "Thread %04d - end pktlist\n", thr_self());
}
#endif	DEBUG_PKTLIST

/*
 * Given two packets, match them based on BOOTP header operation, packet len,
 * hardware type, flags, ciaddr, DHCP type, client id, or chaddr.
 * Returns TRUE if they match, FALSE otherwise.
 */
static int
match_plp(PKT_LIST *alp, PKT_LIST *blp)
{
	DHCP_OPT	*a, *b;

	assert(_mutex_held(&blp->plp_mtx));

	if (alp->pkt->op != blp->pkt->op ||
	    alp->len != alp->len ||
	    alp->pkt->htype != blp->pkt->htype ||
	    alp->pkt->flags != blp->pkt->flags ||
	    alp->pkt->ciaddr.s_addr != blp->pkt->ciaddr.s_addr)
		return (FALSE);	/* not even the same BOOTP type. */

#ifdef DEBUG
	if (alp->pkt->giaddr.s_addr != blp->pkt->giaddr.s_addr) {
		dhcpmsg(LOG_DEBUG,
		    "%04d match_plp: giaddr mismatch on 0x%x, 0x%x\n",
		    thr_self());
	}
#endif	/* DEBUG */

	a = alp->opts[CD_DHCP_TYPE];
	b = blp->opts[CD_DHCP_TYPE];
	if (a == NULL && b == NULL) {
		/* bootp */
		if (memcmp(alp->pkt->chaddr, blp->pkt->chaddr,
		    alp->pkt->hlen) == 0)
			return (TRUE);
	} else if (a != NULL && b != NULL) {
		if (a->value[0] == b->value[0]) {
			/* dhcp - packet types match. */
			a = alp->opts[CD_CLIENT_ID];
			b = blp->opts[CD_CLIENT_ID];
			if (a != NULL && b != NULL) {
				if (memcmp(a->value, b->value, a->len) == 0)
					return (TRUE);
			} else {
				if (memcmp(alp->pkt->chaddr, blp->pkt->chaddr,
				    alp->pkt->hlen) == 0)
					return (TRUE);
			}
		}
	}
	return (FALSE);
}

/*
 * Given a DHCP_ICMP_NOENT packet, searches for an earlier, ICMP valid packet
 * in the interface's pkt list. If the search is successful, the argument
 * packet is deleted, and the earlier, ICMP validated packet is returned
 * with the appropriate fields/options modified, as per the following:
 *
 * DISCOVER/BOOTP: Matches based on match_plp() above. If argument plp has an
 * ICMP flag of DHCP_ICMP_NOENT, the list is scanned (in reverse order) until a
 * DISCOVER which "matches" is found. The first match replaces the argument
 * plp. It's assumed that this function is called only when the caller
 * determines that the argument plp has a icmp flag value of DHCP_ICMP_NOENT.
 * If the match has an icmp flag value of DHCP_ICMP_PENDING, the argument plp
 * is freed, and the DHCP_ICMP_PENDING plp is returned with its position in the
 * PKT_LIST preserved.
 *
 * REQUEST: Matches based on match_plp() above.  The list is checked in
 * reverse order until a REQUEST of the same form (INIT, INIT-REBOOT, RENEW,
 * REBIND) is found. The first match plp is then detached and returned.
 * The argument plp is destroyed.
 *
 * INFORM: Matches based on match_plp() above. The list is check in reverse
 * order until an INFORM is found. The first plp matching this criteria is then
 * detached and returned. The argument plp is destroyed.
 *
 * General Notes: After the candidate is found, the list is checked to the
 * head of the list for other matches, which are deleted. the duplicate
 * statistic is incremented for each one. If no candidate is found, then
 * the argument plp is returned.
 *
 * Caveats: What about length and contents of packets? By definition, a
 * client is not supposed to be altering this between frames, so we should
 * be ok. Since the argument plp may be destroyed, it is assumed to be
 * detached.
 */
PKT_LIST *
refresh_pktlist(IF *ifp, PKT_LIST *plp)
{
	PKT_LIST	*wplp, *tplp, *retplp = NULL;
	char		ntoab_a[NTOABUF], ntoab_b[NTOABUF];

	assert(_mutex_held(&ifp->pkt_mtx));

	wplp = ifp->pkttail;
	while (wplp != NULL) {
		(void) mutex_lock(&wplp->plp_mtx);
		if (match_plp(plp, wplp)) {
			(void) mutex_lock(&npkts_mtx);
			npkts--;
			(void) mutex_unlock(&npkts_mtx);

			(void) mutex_lock(&ifp->ifp_mtx);
			ifp->duplicate++;
			(void) mutex_unlock(&ifp->ifp_mtx);

			/*
			 * Note that tplp, retplp can be synonyms for
			 * wplp. Thus the mutex_unlocks below are actually
			 * unlocking the wplp->plp_mtx locked above. The
			 * synonyms are used because moldy plp's will be
			 * nuked, and the plp to return will be detacted.
			 */
			tplp = wplp;
			wplp = wplp->prev;

			if (retplp == NULL &&
			    tplp->d_icmpflag != DHCP_ICMP_NOENT) {
				retplp = tplp;
				if (retplp->d_icmpflag == DHCP_ICMP_PENDING) {
					(void) mutex_unlock(&retplp->plp_mtx);
					break;
				} else {
					detach_plp(ifp, retplp);
					(void) mutex_unlock(&retplp->plp_mtx);
				}
			} else {
				/* moldy duplicates */
				detach_plp(ifp, tplp);
				(void) mutex_unlock(&tplp->plp_mtx);
				free_plp(tplp);
			}
		} else {
			(void) mutex_unlock(&wplp->plp_mtx);
			wplp = wplp->prev;
		}
	}

	if (retplp == NULL)
		retplp = plp;
	else {
		if (debug) {
			dhcpmsg(LOG_DEBUG,
			    "%04d: Refreshed (0x%x,%d,%s) to (0x%x,%d,%s)\n",
			    thr_self(), plp, plp->d_icmpflag,
			    inet_ntoa_r(plp->off_ip, ntoab_a),
			    retplp, retplp->d_icmpflag,
			    inet_ntoa_r(retplp->off_ip, ntoab_b));
		}
		free_plp(plp);
		plp = NULL;
	}

	return (retplp);
}

/*
 * Queries the IP transport layer for configured interfaces. Those that
 * are acceptable for use by our daemon have these characteristics:
 *
 *	Not loopback
 *	Is UP
 *
 * Sets num_interfaces global to number of valid, selected interfaces.
 *
 * Returns: 0 for success, the appropriate errno on fatal failure.
 *
 * Notes: Code gleaned from the in.rarpd, solaris 2.2.
 */
static int
find_interfaces(void)
{
	int			i, found;
	int			ip;
	int			reqsize;
	ushort_t		mtu_tmp;
	int			numifs;
	struct ifreq		*reqbuf, *ifr;
	struct ifconf		ifconf;
	IF			*ifp, *if_tail;
	struct sockaddr_in	*sin;
	char			**user_if;
	ENCODE 			*hecp;

	if ((ip = open("/dev/ip", 0)) < 0) {
		dhcpmsg(LOG_ERR, "Error: opening /dev/ip: %s\n",
		    strerror(errno));
		return (1);
	}

#ifdef	SIOCGIFNUM
	if (ioctl(ip, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
		dhcpmsg(LOG_WARNING,
		    "Error discovering number of network interfaces: %s\n",
		    strerror(errno));
	}
#else
	numifs = MAXIFS;
#endif	/* SIOCGIFNUM */

	reqsize = numifs * sizeof (struct ifreq);
	/* LINTED [smalloc()/malloc returns longword aligned addresses] */
	reqbuf = (struct ifreq *)smalloc(reqsize);

	ifconf.ifc_len = reqsize;
	ifconf.ifc_buf = (caddr_t)reqbuf;

	if (ioctl(ip, SIOCGIFCONF, (char *)&ifconf) < 0) {
		dhcpmsg(LOG_ERR,
		    "Error getting network interface information: %s\n",
		    strerror(errno));
		free((char *)reqbuf);
		(void) close(ip);
		return (1);
	}

	/*
	 * Verify that user specified interfaces are valid.
	 */
	/* LINTED [smalloc()/malloc returns longword aligned addresses] */
	user_if = (char **)smalloc(numifs * sizeof (char *));
	if (interfaces != NULL) {
		for (i = 0; i < numifs; i++) {
			user_if[i] = strtok(interfaces, ",");
			if (user_if[i] == NULL)
				break;		/* we're done */
			interfaces = NULL; /* for next call to strtok() */

			for (found = 0, ifr = ifconf.ifc_req;
			    ifr < &ifconf.ifc_req[ifconf.ifc_len /
			    sizeof (struct ifreq)]; ifr++) {
				if (strcmp(user_if[i], ifr->ifr_name) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				dhcpmsg(LOG_ERR,
				    "Invalid network interface:  %s\n",
				    user_if[i]);
				free((char *)reqbuf);
				free((char *)user_if);
				(void) close(ip);
				return (1);
			}
		}
		if (i < numifs)
			user_if[i] = NULL;
	} else
		user_if[0] = NULL;

	/*
	 * For each interface, build an interface structure. Ignore any
	 * LOOPBACK or down interfaces.
	 */
	if_tail = if_head = NULL;
	for (ifr = ifconf.ifc_req;
	    ifr < &ifconf.ifc_req[ifconf.ifc_len / sizeof (struct ifreq)];
	    ifr++) {
		if (strchr(ifr->ifr_name, ':') != NULL)
			continue;	/* skip virtual interfaces */
		if (ioctl(ip, SIOCGIFFLAGS, (char *)ifr) < 0) {
			dhcpmsg(LOG_ERR,
"Error encountered getting interface: %s flags: %s\n",
			    ifr->ifr_name, strerror(errno));
			continue;
		}
		if ((ifr->ifr_flags & IFF_LOOPBACK) ||
		    !(ifr->ifr_flags & IFF_UP))
			continue;

		num_interfaces++;	/* all possible interfaces counted */

		/*
		 * If the user specified a list of interfaces,
		 * we'll only consider the ones specified.
		 */
		if (user_if[0] != NULL) {
			for (i = 0; i < numifs; i++) {
				if (user_if[i] == NULL)
					break; /* skip this interface */
				if (strcmp(user_if[i], ifr->ifr_name) == 0)
					break;	/* user wants this one */
			}
			if (i == numifs || user_if[i] == NULL)
				continue;	/* skip this interface */
		}

		/* LINTED [smalloc returns longword aligned addresses] */
		ifp = (IF *)smalloc(sizeof (IF));
		(void) strcpy(ifp->nm, ifr->ifr_name);

		/* flags */
		ifp->flags = ifr->ifr_flags;

		/*
		 * Broadcast address. Not valid for POINTOPOINT
		 * connections.
		 */
		if ((ifp->flags & IFF_POINTOPOINT) == 0) {
			if (ifp->flags & IFF_BROADCAST) {
				if (ioctl(ip, SIOCGIFBRDADDR,
				    (caddr_t)ifr) < 0) {
					dhcpmsg(LOG_ERR, "Error encountered \
getting interface: %s broadcast address: %s\n", ifp->nm, strerror(errno));
					free(ifp);
					num_interfaces--;
					continue;
				}
				/* LINTED [alignment ok] */
				sin = (struct sockaddr_in *)&ifr->ifr_addr;
				ifp->bcast = sin->sin_addr;
			} else
				ifp->bcast.s_addr = htonl(INADDR_ANY);

			hecp = make_encode(CD_BROADCASTADDR,
			    sizeof (struct in_addr), (void *)&ifp->bcast, 1);
			replace_encode(&ifp->ecp, hecp, ENC_DONT_COPY);
		}

		/* Subnet mask */
		if (ioctl(ip, SIOCGIFNETMASK, (caddr_t)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting \
interface: %s netmask: %s\n", ifp->nm, strerror(errno));
			free_encode_list(ifp->ecp);
			free(ifp);
			num_interfaces--;
			continue;
		}
		/* LINTED [alignment ok] */
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		ifp->mask = sin->sin_addr;
		hecp = make_encode(CD_SUBNETMASK, sizeof (struct in_addr),
		    (void *)&ifp->mask, 1);
		replace_encode(&ifp->ecp, hecp, ENC_DONT_COPY);

		/* Address */
		if (ioctl(ip, SIOCGIFADDR, (caddr_t)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting \
interface: %s address: %s\n", ifp->nm,  strerror(errno));
			free_encode_list(ifp->ecp);
			free(ifp);
			num_interfaces--;
			continue;
		}
		/* LINTED [alignment ok] */
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		ifp->addr = sin->sin_addr;

		/* MTU */
		if (ioctl(ip, SIOCGIFMTU, (caddr_t)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting \
interface: %s MTU: %s\n", ifp->nm, strerror(errno));
			free_encode_list(ifp->ecp);
			free(ifp);
			num_interfaces--;
			continue;
		}

		ifp->mtu = ifr->ifr_metric;
		mtu_tmp = htons(ifp->mtu);
		hecp = make_encode(CD_MTU, 2, (void *)&mtu_tmp, 1);
		replace_encode(&ifp->ecp, hecp, ENC_DONT_COPY);

		/* Attach to interface list */
		if (!if_tail) {
			(void) mutex_init(&if_head_mtx, USYNC_THREAD, 0);
			(void) mutex_lock(&if_head_mtx);
			if_tail = if_head = ifp;
			(void) mutex_unlock(&if_head_mtx);
		} else {
			(void) mutex_lock(&if_head_mtx);
			if_tail->next = ifp;
			if_tail = ifp;
			(void) mutex_unlock(&if_head_mtx);
		}
	}

	free((char *)reqbuf);
	free((char *)user_if);
	(void) close(ip);

	if (if_head == NULL) {
		num_interfaces = 0;
		dhcpmsg(LOG_ERR, "Cannot find any valid interfaces.\n");
		(void) mutex_destroy(&if_head_mtx);
		return (EINVAL);
	}
	return (0);
}

/*
 * Destroy an *uninitialized* IF structure - returns next ifp.
 */
static IF *
zap_ifp(IF **ifp_prevpp, IF *ifp)
{
	IF	*tifp;

	assert(_mutex_held(&if_head_mtx));

	if (*ifp_prevpp == ifp) {
		if_head = ifp->next;
		*ifp_prevpp = if_head;
	} else
		(*ifp_prevpp)->next = ifp->next;

	tifp = ifp->next;

	free(ifp);

	return (tifp);
}

/*
 * Monitor thread function. Poll on interface descriptor. Add valid BOOTP
 * packets to interfaces PKT_LIST.
 *
 * Because the buffer will potentially contain the ip/udp headers, we flag
 * this by setting the 'offset' field to the length of the two headers so that
 * free_plp() can "do the right thing"
 *
 * Monitor the given interface. Signals are handled by sig_handle thread.
 *
 * We make some attempt to deal with marginal interfaces as follows. We
 * keep track of system errors (errors) and protocol errors (ifp->errors).
 * If we encounter more than DHCP_MON_SYSERRS in DHCP_MON_ERRINTVL,
 * then the interface thread will put itself to sleep for DHCP_MON_SLEEP
 * minutes.
 *
 * MT SAFE
 */
static void *
monitor_interface(void *argp)
{
	PKT_LIST		*plp, *tplp;
	IF			*ifp = (IF *)argp, *tifp;
	int			errors, err, flags, i;
	uint_t			verify_len, pending;
	struct ip		*ipp;
	struct udphdr		*udpp;
	union DL_primitives	*dlp;
	ushort_t		ip_hlen;
	struct pollfd		pfd;
	struct in_addr		ta;
	char 			cbuf[BUFSIZ], ntoab[NTOABUF];
	struct strbuf		ctl, data;
	time_t			err_interval;

	if (debug) {
		dhcpmsg(LOG_DEBUG, "Monitor (%04d/%s) started...\n",
		    ifp->if_thread, ifp->nm);
	}

	if (verbose)
		disp_if(ifp);

	pfd.fd = ifp->recvdesc;
	pfd.events = POLLIN | POLLPRI;
	pfd.revents = 0;

	err_interval = time(NULL) + DHCP_MON_ERRINTVL;
	errors = 0;
	for (;;) {
		if (errors > DHCP_MON_SYSERRS) {
			if (time(NULL) < err_interval) {
				dhcpmsg(LOG_WARNING,
"Monitor (%04d/%s): Too many system errors (%d), pausing for %d minute(s)...\n",
				    ifp->if_thread, ifp->nm, errors,
				    DHCP_MON_SYSERRS);
				(void) sleep(DHCP_MON_SLEEP);
				err_interval = time(NULL) + DHCP_MON_ERRINTVL;
			}
			errors = 0;
		}
		if (poll(&pfd, (nfds_t)1, INFTIM) < 0) {
			dhcpmsg(LOG_ERR,
			    "Monitor (%04d/%s) Polling error: (%s).\n",
			    ifp->if_thread, ifp->nm, strerror(errno));
			errors++;
			continue;
		}
		/*
		 * See if we are to exit. We can't be holding any locks...
		 */
		(void) mutex_lock(&ifp->ifp_mtx);
		if (ifp->thr_exit) {
			if (debug) {
				dhcpmsg(LOG_DEBUG,
				    "Monitor (%04d/%s): exiting.\n",
				    ifp->if_thread, ifp->nm);
			}
			(void) mutex_unlock(&ifp->ifp_mtx);
			break;
		}
		(void) mutex_unlock(&ifp->ifp_mtx);

		if (!pfd.revents)
			continue;

		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			dhcpmsg(LOG_ERR,
			    "Network interface error on device: %s\n", ifp->nm);
			errors++;
			continue;
		}
		if (!(pfd.revents & (POLLIN | POLLRDNORM))) {
			dhcpmsg(LOG_INFO,
			    "Unsupported event on device %s: %d\n",
			    ifp->nm, pfd.revents);
			errors++;
			continue;
		}

		switch (ifp->type) {
		case DHCP_SOCKET:
			data.buf = smalloc(ifp->mtu);
			if ((data.len = recv(ifp->recvdesc, data.buf,
			    ifp->mtu, 0)) < 0) {
				dhcpmsg(LOG_ERR,
"Error: %s receiving UDP datagrams from socket (%s)\n",
				    strerror(errno), ifp->nm);
				free(data.buf);
				errors++;
				continue;
			} else
				verify_len = data.len;
			break;

		case DHCP_DLPI:
			/*
			 * We need to flush the socket we're using
			 * to send data because data will be received
			 * there also, even though we really want
			 * to use DLPI to receive packets.  (We use
			 * a socket for sending simply because we
			 * don't want to do checksums and routing
			 * ourselves!)
			 */
			(void) ioctl(ifp->senddesc, I_FLUSH, FLUSHR);
			flags = 0;
			ctl.maxlen = sizeof (cbuf);
			ctl.len = 0;
			ctl.buf = &cbuf[0];

			data.maxlen = ifp->mtu;
			data.len = 0;
			data.buf = smalloc(ifp->mtu);

			if (getmsg(ifp->recvdesc, &ctl, &data,
			    &flags) < 0) {
				dhcpmsg(LOG_ERR,
"Error receiving UDP datagrams on %s from DLPI: %s\n",
				    ifp->nm,
				    strerror(errno));
				free(data.buf);
				errors++;
				continue;
			}

			/* LINTED [buf is long word aligned] */
			dlp = (union DL_primitives *)ctl.buf;

			if (ctl.len < DL_UNITDATA_IND_SIZE ||
			    dlp->dl_primitive != DL_UNITDATA_IND) {
				dhcpmsg(LOG_ERR,
				    "Unexpected DLPI message on %s.\n",
				    ifp->nm);
				free(data.buf);
				errors++;
				continue;
			}

			/*
			 * Checksum IP header.
			 */
			/* LINTED [smalloc aligns on ptr boundaries] */
			ipp = (struct ip *)data.buf;
			ip_hlen = ipp->ip_hl << 2;
			if (ip_hlen < sizeof (struct ip)) {
				/* too short */
				free(data.buf);
				(void) mutex_lock(&ifp->ifp_mtx);
				ifp->errors++;
				(void) mutex_unlock(&ifp->ifp_mtx);
				continue;
			}
			if ((err = ipv4cksum((uint16_t *)ipp, ip_hlen)) != 0) {
				if (debug) {
					dhcpmsg(LOG_INFO,
					    "%s: IP checksum: 0x%x != 0x%x\n",
					    ifp->nm, ipp->ip_sum, err);
				}
				free(data.buf);
				(void) mutex_lock(&ifp->ifp_mtx);
				ifp->errors++;
				(void) mutex_unlock(&ifp->ifp_mtx);
				continue;
			}

			/*
			 * Verify that it is for us. We walk the
			 * interface list, checking various addresses.
			 * We don't lock this list because our access
			 * is readonly, and this list will not change
			 * once the monitoring threads are running.
			 */
			ta.s_addr = ipp->ip_dst.s_addr;
			if (ta.s_addr != ifp->addr.s_addr &&
			    ta.s_addr != INADDR_BROADCAST &&
			    ta.s_addr != ifp->bcast.s_addr) {
				(void) mutex_lock(&if_head_mtx);
				for (tifp = if_head; tifp != NULL;
				    tifp = tifp->next) {
					if (ta.s_addr ==
					    tifp->addr.s_addr ||
					    ta.s_addr ==
					    tifp->bcast.s_addr) {
						break;
					}
				}
				(void) mutex_unlock(&if_head_mtx);
				if (tifp == NULL) {
					free(data.buf);
					continue;	/* not ours */
				}
			}

			/*
			 * Checksum UDP Header plus data.
			 */
			udpp = (struct udphdr *)((uint_t)data.buf +
			    sizeof (struct ip));
			if (udpp->uh_sum != 0) {
				verify_len = data.len - sizeof (struct ip);
				if (verify_len != ntohs(udpp->uh_ulen)) {
					dhcpmsg(LOG_ERR,
					    "%s: Bad UDP length: %d < %d\n",
					    ifp->nm, verify_len,
					    ntohs(udpp->uh_ulen));
					free(data.buf);
					(void) mutex_lock(&ifp->ifp_mtx);
					ifp->errors++;
					(void) mutex_unlock(&ifp->ifp_mtx);
					continue;
				}
				if ((err = udp_chksum(udpp, &ipp->ip_src,
				    &ipp->ip_dst, ipp->ip_p)) != 0) {
					if (debug) {
						dhcpmsg(LOG_INFO,
"%s: UDP checksum:  0x%x != 0x%x, source: %s\n",
						    ifp->nm,
						    ntohs(udpp->uh_sum), err,
						    inet_ntoa_r(ipp->ip_src,
						    ntoab));
					}
					free(data.buf);
					(void) mutex_lock(&ifp->ifp_mtx);
					ifp->errors++;
					(void) mutex_unlock(&ifp->ifp_mtx);
					continue;
				}
			}
			verify_len = data.len - sizeof (struct ip) -
			    sizeof (struct udphdr);
			break;
		}

		(void) mutex_lock(&totpkts_mtx);
		totpkts++;
		(void) mutex_unlock(&totpkts_mtx);

		if (verify_len < sizeof (PKT)) {
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "Short packet %d < %d on %s ignored\n",
				    verify_len, sizeof (PKT), ifp->nm);
			}
			free(data.buf);
			(void) mutex_lock(&ifp->ifp_mtx);
			ifp->errors++;
			(void) mutex_unlock(&ifp->ifp_mtx);
			continue;
		}

		/* LINTED [smalloc returns long word aligned addresses */
		plp = (PKT_LIST *)smalloc(sizeof (PKT_LIST));
		if (ifp->type == DHCP_DLPI) {
			plp->offset = (uchar_t) (sizeof (struct ip) +
			    sizeof (struct udphdr));
			plp->len = data.len - plp->offset;
			plp->pkt = (PKT *)((uint_t)data.buf + plp->offset);
		} else {
			plp->offset = 0;
			plp->len = data.len;
			/* LINTED [smalloc returns long word aligned] */
			plp->pkt = (PKT *)data.buf;
		}

		plp->d_icmpflag = DHCP_ICMP_NOENT;

		if (plp->pkt->hops >= (uchar_t)(max_hops + 1)) {
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "%s: Packet dropped: too many hops: %d\n",
				    ifp->nm, plp->pkt->hops);
			}
			free_plp(plp);
			(void) mutex_lock(&ifp->ifp_mtx);
			ifp->errors++;
			(void) mutex_unlock(&ifp->ifp_mtx);
			continue;
		}

		/* validate hardware len */
		if (plp->pkt->hlen > sizeof (plp->pkt->chaddr))
			plp->pkt->hlen = sizeof (plp->pkt->chaddr);

		if (debug && plp->pkt->giaddr.s_addr != 0L &&
		    plp->pkt->giaddr.s_addr != ifp->addr.s_addr &&
		    plp->d_icmpflag == DHCP_ICMP_NOENT) {
			dhcpmsg(LOG_INFO,
			    "Packet received from relay agent: %s\n",
			    inet_ntoa_r(plp->pkt->giaddr, ntoab));
		}

		if (!server_mode) {
			/*
			 * Relay agent mode. No further processing
			 * required by main thread; we'll handle it
			 * here.
			 */
			if ((err = relay_agent(ifp, plp)) != 0) {
				dhcpmsg(LOG_ERR,
"Relay agent mode failed: %d (%s), interface: %s\n",
				    err, (plp->pkt->op == BOOTREPLY) ? "reply" :
				    "request",  ifp->nm);
				errors++; /* considered system error */
			} else {
				/* update statistics */
				(void) mutex_lock(&ifp->ifp_mtx);
				ifp->processed++;
				ifp->received++;
				(void) mutex_unlock(&ifp->ifp_mtx);
			}
			free_plp(plp);
			continue;
		}

		/* Packets destined for bootp and dhcp server modules */

		/*
		 * Allow packets without RFC1048 magic cookies.
		 * Just don't do an options scan on them,
		 * thus we treat them as plain BOOTP packets.
		 * The BOOTP server can deal with requests of
		 * this type.
		 */
		if (memcmp(plp->pkt->cookie, magic_cookie,
		    sizeof (magic_cookie)) != 0) {
			if (verbose) {
				dhcpmsg(LOG_INFO,
"%s: Client: %s using non-RFC1048 BOOTP cookie.\n",
				    ifp->nm, disp_cid(plp, cbuf,
				    sizeof (cbuf)));
			}
			plp->rfc1048 = FALSE;
		} else {
			/*
			 * Scan the options in the packet and
			 * fill in the opts and vs fields in the
			 * pktlist structure.  If there's a
			 * DHCP message type in the packet then
			 * it's a DHCP packet; otherwise it's
			 * a BOOTP packet. Standard options are
			 * RFC1048 style.
			 */
			if (plp->d_icmpflag == DHCP_ICMP_NOENT) {
				if (_dhcp_options_scan(plp) != 0) {
					dhcpmsg(LOG_ERR,
"Garbled DHCP/BOOTP datagram received on interface: %s\n",
					    ifp->nm);
					free_plp(plp);
					(void) mutex_lock(&ifp->ifp_mtx);
					ifp->errors++;
					(void) mutex_unlock(&ifp->ifp_mtx);
					continue;
				}
			}
			plp->rfc1048 = TRUE;
		}
		(void) mutex_init(&plp->plp_mtx, USYNC_THREAD, 0);

		(void) mutex_lock(&ifp->pkt_mtx);

		/*
		 * Serious performance testing (multiple interfaces, with
		 * hundreds of simultaneous clients) shows that interface
		 * PKT_LISTs can get *WAY* too large and effectively
		 * result in the main thread being unable to process any
		 * traffic. We address this problem here by ensuring that
		 * the PKT_LIST never exceeds DHCP_MON_THRESHOLD pkts in
		 * length. If it does, we prune it from the head of
		 * the list, dropping sequential packets. Note that
		 * since DHCP is a multi-transaction protocol, we would
		 * like to be sure not to discard a REQUEST for an OFFER
		 * we've extended. Unfortunately, there is no way to
		 * effectively verify this at this level - the main
		 * thread knows the client's STATE, not us. Rather than
		 * try too hard to "nicely" prune the list, we simply
		 * nuke enough packets to bring the threshold down
		 * below DHCP_MON_THRESHOLD + DHCP_MON_DUMP, regardless
		 * of their type. If we get into this state, the solution
		 * calls for drastic action. Hopefully, this will be a
		 * rare occurrence.
		 */

		(void) mutex_lock(&ifp->ifp_mtx);
		pending = ifp->received - (ifp->duplicate + ifp->dropped +
		    ifp->processed);
		if (pending > DHCP_MON_THRESHOLD) {
			i = 0;
			while (i < DHCP_MON_DUMP) {
				for (tplp = ifp->pkthead; tplp != NULL;
				    tplp = tplp->next) {
					(void) mutex_lock(&tplp->plp_mtx);
					if (tplp->d_icmpflag !=
					    DHCP_ICMP_PENDING) {
						(void) mutex_unlock(
						    &tplp->plp_mtx);
						break;
					}
					(void) mutex_unlock(&tplp->plp_mtx);
				}
				if (tplp == NULL) {
					/* No safe entries to free */
					errors++;
					break;
				}
				detach_plp(ifp, tplp);
				free_plp(tplp);
				i++;
			}
			ifp->dropped += i;
			(void) mutex_lock(&npkts_mtx);
			npkts -= i;
			(void) mutex_unlock(&npkts_mtx);
			dhcpmsg(LOG_WARNING,
"Monitor (%04d/%s): Overflow: Dropped %d packets out of %d pending.\n",
			    ifp->if_thread, ifp->nm, i, pending);
		}

		(void) mutex_unlock(&ifp->ifp_mtx);

		/*
		 * Link the new packet to the list of packets
		 * for this interface. No need to lock plp,
		 * since it isn't visible outside this function yet.
		 */
		if (ifp->pkthead == NULL)
			ifp->pkthead = plp;
		else {
			ifp->pkttail->next = plp;
			plp->prev = ifp->pkttail;
		}
		ifp->pkttail = plp;
		(void) mutex_unlock(&ifp->pkt_mtx);

		/*
		 * Update counters
		 */
		(void) mutex_lock(&ifp->ifp_mtx);
		ifp->received++;
		(void) mutex_unlock(&ifp->ifp_mtx);

		(void) mutex_lock(&npkts_mtx);
		npkts++;
		(void) cond_signal(&npkts_cv);	/* work for main thread */
		(void) mutex_unlock(&npkts_mtx);

		if (debug) {
			dhcpmsg(LOG_INFO,
			    "Datagram received on network device: %s\n",
			    ifp->nm);
		}
	}
	thr_exit(NULL);
	return ((void *)NULL);	/* NOTREACHED */
}

/*
 * Based on the list generated by find_interfaces(), possibly modified by
 * user arguments, open a stream for each valid / requested interface.
 *
 * If:
 *
 *	1) Only one interface exists, open a standard bidirectional UDP
 *		socket. Note that this is different than if only ONE
 *		interface is requested (but more exist).
 *
 *	2) If more than one valid interface exists, then attach to the
 *		datalink layer, push on the packet filter and buffering
 *		modules, and wait for fragment 0 IP packets that contain
 *		UDP packets with port 67 (server port).
 *
 *	Comments:
 *		Using DLPI to identify the interface thru which BOOTP
 *		packets pass helps in providing the correct response.
 *		Note that I will open a socket for use in transmitting
 *		responses, suitably specifying the destination relay agent
 *		or host. Note that if I'm unicasting to the client (broadcast
 *		flag not set), that somehow I have to clue the IP layer about
 *		the client's hw address. The only way I can see doing this is
 *		making the appropriate ARP table entry.
 *
 *		The only remaining unknown is dealing with clients that
 *		require broadcasting, and multiple interfaces exist. I assume
 *		that if I specify the interface's source address when
 *		opening the socket, that a limited broadcast will be
 *		directed to the correct net, and only the correct net.
 *
 *	Returns: 0 for success, errno for failure.
 */
int
open_interfaces(void)
{
	int			inum;
	int			err = 0, pf_initialized;
	char			*devpath;
	IF			*ifp, *ifp_prevp;
	extern int		errno;
	union DL_primitives	dl;
	struct sockaddr_in	sin;
	int			sndsock;
	int			sockoptbuf;
	char			devbuf[DHCP_SCRATCH];

	/* Uncover list of valid, user-selected interfaces to monitor */
	if ((err = find_interfaces()) != 0)
		return (err);

	(void) mutex_lock(&if_head_mtx);
	switch (num_interfaces) {
	case 1:
		/*
		 * Single valid interface.
		 */
		if_head->recvdesc = socket(AF_INET, SOCK_DGRAM, 0);
		if (if_head->recvdesc < 0) {
			dhcpmsg(LOG_ERR, "Error opening socket for \
receiving UDP datagrams: %s\n", strerror(errno));
			err = errno;
			(void) zap_ifp(&if_head, if_head);
			if_head = NULL;
			num_interfaces = 0;
			break;
		}

		if_head->senddesc = if_head->recvdesc;
		if (setsockopt(if_head->senddesc, SOL_SOCKET, SO_BROADCAST,
		    (char *)&sockoptbuf, (int)sizeof (sockoptbuf)) < 0) {
			dhcpmsg(LOG_ERR, "Setting socket option to allow \
broadcast on send descriptor failed: %s\n", strerror(errno));
			err = errno;
			(void) close(if_head->recvdesc);
			(void) zap_ifp(&if_head, if_head);
			if_head = NULL;
			num_interfaces = 0;
			break;
		}

		/*
		 * Ideally we'd have another socket of type SOCK_DGRAM
		 * that we could send on but this doesn't seem to work
		 * because the first socket is already bound to the port.
		 */
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons((short)IPPORT_BOOTPS);
		if (bind(if_head->recvdesc, (struct sockaddr *)&sin,
		    sizeof (sin)) < 0) {
			dhcpmsg(LOG_ERR,
			    "Error binding to UDP receive socket: %s\n",
			    strerror(errno));
			(void) close(if_head->recvdesc);
			err = errno;
			(void) zap_ifp(&if_head, if_head);
			if_head = NULL;
			num_interfaces = 0;
			break;
		}
		if_head->type = DHCP_SOCKET;

		/* OFFER list */
		if_head->of_head = NULL;

		/* Accounting */
		if_head->transmit = if_head->received = 0;
		if_head->duplicate = if_head->dropped = 0;
		if_head->processed = 0;

		/* Exit lock */
		(void) mutex_init(&if_head->ifp_mtx, USYNC_THREAD, 0);
		if_head->thr_exit = 0;

		/* PKT_LIST lock */
		(void) mutex_init(&if_head->pkt_mtx, USYNC_THREAD, 0);

		/* fire up monitor thread */
		if (thr_create(NULL, 0, monitor_interface, (void *)if_head,
		    THR_NEW_LWP, &if_head->if_thread) != 0) {
			dhcpmsg(LOG_ERR,
"Interface: %s - Error %s starting monitor thread.\n", if_head->nm,
			    strerror(errno));
			(void) close(if_head->recvdesc);
			(void) mutex_destroy(&if_head->ifp_mtx);
			(void) mutex_destroy(&if_head->pkt_mtx);
			if_head = zap_ifp(&if_head, if_head);
			num_interfaces = 0;
		}
		break;
	default:
		/* Set up a SOCK_DGRAM socket for sending packets. */
		if ((sndsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			dhcpmsg(LOG_ERR, "Error opening socket for \
sending UDP datagrams: %s\n", strerror(errno));
			err = errno;
			num_interfaces = 0; /* can't send */
			break;
		}

		if (setsockopt(sndsock, SOL_SOCKET, SO_BROADCAST,
		    (char *)&sockoptbuf, (int)sizeof (sockoptbuf)) < 0) {
			dhcpmsg(LOG_ERR, "Setting socket option to allow \
broadcast on send descriptor failed: %s\n", strerror(errno));
			(void) close(sndsock);
			err = errno;
			num_interfaces = 0; /* can't send */
			break;
		}

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons((short)IPPORT_BOOTPS);
		if (bind(sndsock, (struct sockaddr *)&sin,
		    sizeof (sin)) < 0) {
			dhcpmsg(LOG_ERR, "BIND: %s\n", strerror(errno));
			(void) close(sndsock);
			err = errno;
			num_interfaces = 0; /* can't send */
			break;
		}
		/*
		 * Multiple valid interfaces. Build DLPI receive streams for
		 * each. If we fail to prepare an interface for monitoring,
		 * we'll ignore it.
		 */
		ifp = ifp_prevp = if_head;
		err = inum = 0;
		pf_initialized = 0;
		while (ifp != NULL) {
			ifp->senddesc = sndsock;
			ifp->type = DHCP_DLPI;
			devpath = device_path(ifp->nm, devbuf, sizeof (devbuf));
			if ((ifp->recvdesc = open(devpath, O_RDWR)) < 0) {
				dhcpmsg(LOG_ERR, "Interface: %s Open: %s\n",
				    ifp->nm, strerror(errno));
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}

			/*
			 * Check for DLPI Version 2.
			 */
			if (dlinforeq(ifp->recvdesc,
			    (dl_info_ack_t *)&dl) != 0) {
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}
			if (dl.info_ack.dl_version != DL_VERSION_2) {
				dhcpmsg(LOG_ERR,
"Interface: %s - DLPI version 2 expected, not version %d\n",
				    ifp->nm, dl.info_ack.dl_version);
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}
			if (dl.info_ack.dl_provider_style != DL_STYLE2) {
				dhcpmsg(LOG_ERR,
"Interface: %s - DLPI style 2 expected, not style %d\n",
				    ifp->nm, dl.info_ack.dl_provider_style);
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}
			if (!(dl.info_ack.dl_service_mode & DL_CLDLS)) {
				dhcpmsg(LOG_ERR,
"Interface: %s does not support connectionless data link service mode.\n",
				    ifp->nm, dl.info_ack.dl_service_mode);
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}

			if (!pf_initialized) {
				initialize_pf();
				pf_initialized = 1;
			}

			if (dlattachreq(ifp->recvdesc, dev_ppa(ifp->nm)) != 0) {
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}

			/*
			 * Push and configure the packet filtering module.
			 */
			if (ioctl(ifp->recvdesc, I_PUSH, "pfmod") < 0) {
				(void) dldetachreq(ifp->recvdesc);
				(void) close(ifp->recvdesc);
				dhcpmsg(LOG_ERR, "I_PUSH: %s, on %s\n",
				    strerror(errno), devpath);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}
			if (strioctl(ifp->recvdesc, PFIOCSETF, -1,
			    sizeof (dhcppf), (char *)&dhcppf) < 0) {
				(void) dldetachreq(ifp->recvdesc);
				dhcpmsg(LOG_ERR,
"Interface: %s - Error setting BOOTP packet filter.\n", ifp->nm);
				(void) ioctl(ifp->recvdesc, I_POP, 0);
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}
			if (dlbindreq(ifp->recvdesc, (ulong_t)ETHERTYPE_IP,
			    0, DL_CLDLS, 0) != 0) {
				dhcpmsg(LOG_ERR,
"Interface: %s - Error binding for IP packets.\n", ifp->nm);
				(void) dldetachreq(ifp->recvdesc);
				(void) ioctl(ifp->recvdesc, I_POP, 0);
				(void) close(ifp->recvdesc);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}

			/* OFFER list */
			ifp->of_head = NULL;

			/* Accounting */
			ifp->transmit = ifp->received = 0;
			ifp->duplicate = ifp->dropped = 0;
			ifp->processed = 0;

			/* Exit lock */
			(void) mutex_init(&ifp->ifp_mtx, USYNC_THREAD, 0);
			ifp->thr_exit = 0;

			/* PKT_LIST lock */
			(void) mutex_init(&ifp->pkt_mtx, USYNC_THREAD, 0);

			/* fire up monitor thread */
			if (thr_create(NULL, 0, monitor_interface, (void *)ifp,
			    THR_NEW_LWP, &ifp->if_thread) != 0) {
				dhcpmsg(LOG_ERR,
"Interface: %s - Error %s starting monitor thread.\n", ifp->nm,
				    strerror(errno));
				(void) dlunbindreq(ifp->recvdesc);
				(void) dldetachreq(ifp->recvdesc);
				(void) ioctl(ifp->recvdesc, I_POP, 0);
				(void) close(ifp->recvdesc);
				(void) mutex_destroy(&ifp->ifp_mtx);
				(void) mutex_destroy(&ifp->pkt_mtx);
				ifp = zap_ifp(&ifp_prevp, ifp);
				num_interfaces--;
				continue;
			}

			inum++;
			ifp_prevp = ifp;
			ifp = ifp->next;
		}

		if (num_interfaces == 0) {
			if_head = NULL;	/* already deleted */
			(void) close(sndsock);
		}
		break;
	}
	(void) mutex_unlock(&if_head_mtx);

	/*
	 * We must succeed in configuring at least one interface
	 * to be considered successful.
	 */
	if (num_interfaces == 0) {
		err = EINVAL;
		dhcpmsg(LOG_ERR, "Cannot configure any interfaces.\n");
	}
	return (err);
}

/*
 * Detach the referenced plp from the interface's pkt list.
 */
void
detach_plp(IF *ifp, PKT_LIST *plp)
{
	assert(_mutex_held(&ifp->pkt_mtx));

	if (plp->prev == NULL) {
		ifp->pkthead = plp->next;
		if (ifp->pkthead != NULL)
			ifp->pkthead->prev = NULL;
	} else
		plp->prev->next = plp->next;

	if (plp->next != NULL)
		plp->next->prev = plp->prev;
	else {
		ifp->pkttail = plp->prev;
		if (ifp->pkttail != NULL)
			ifp->pkttail->next = NULL;
	}
	plp->prev = plp->next = NULL;
}

/*
 * Write a packet to an interface.
 *
 * Returns 0 on success otherwise an errno.
 */
int
write_interface(IF *ifp, PKT *pktp, int len, struct sockaddr_in *to)
{
	int err;

	to->sin_family = AF_INET;

	if ((err = sendto(ifp->senddesc, (caddr_t)pktp, len, 0,
	    (struct sockaddr *)to, sizeof (struct sockaddr))) < 0) {
		dhcpmsg(LOG_ERR, "SENDTO: %s.\n", strerror(errno));
		return (err);
	} else {
		(void) mutex_lock(&ifp->ifp_mtx);
		ifp->transmit++;
		(void) mutex_unlock(&ifp->ifp_mtx);
	}

	return (0);
}

/*
 * Pop any packet filters, buffering modules, close stream, free encode
 * list, terminate monitor thread, free ifp. Return ifp next ptr.
 */
static IF *
close_interface(IF *ifp)
{
	PKT_LIST	*plp, *tmpp;
	int		err = 0;
	IF		*tifp;

	if (ifp == NULL)
		return (NULL);

	assert(_mutex_held(&if_head_mtx));

	(void) mutex_lock(&ifp->ifp_mtx);
	ifp->thr_exit = 1;
	(void) close(ifp->recvdesc);	/* thread will exit poll... */
	ifp->recvdesc = -1;
	(void) mutex_unlock(&ifp->ifp_mtx);

	/*
	 * Wait for the thread to exit. We release the if_head_mtx
	 * lock, since the monitor thread(s) need to acquire it to traverse
	 * the list - and we don't want to deadlock. Once the monitor thread
	 * notices the thr_exit flag, it'll be gone anyway. Note that if_head
	 * is changing (in close_interfaces()). At this point, only monitor
	 * threads that haven't been reaped could be walking the interface
	 * list. They will "see" the change in if_head.
	 */
	(void) mutex_unlock(&if_head_mtx);
	if ((err = thr_join(ifp->if_thread, NULL, NULL)) != 0) {
		dhcpmsg(LOG_ERR,
		    "Error %d while waiting for monitor %d of %s\n",
		    err, ifp->if_thread, ifp->nm);
	}
	(void) mutex_lock(&if_head_mtx);

	(void) mutex_lock(&ifp->ifp_mtx);

	/* Free outstanding packets */
	(void) mutex_lock(&ifp->pkt_mtx);
	plp = ifp->pkthead;
	while (plp != NULL) {
		(void) mutex_lock(&plp->plp_mtx);
		tmpp = plp;
		plp = plp->next;
		(void) mutex_unlock(&tmpp->plp_mtx);
		free_plp(tmpp);
		(void) mutex_lock(&npkts_mtx);
		npkts--;
		(void) mutex_unlock(&npkts_mtx);
		++ifp->dropped;
	}
	ifp->pkthead = ifp->pkttail = NULL;
	(void) mutex_unlock(&ifp->pkt_mtx);
	(void) mutex_destroy(&ifp->pkt_mtx);

	/* free encode list */
	free_encode_list(ifp->ecp);

	/* display statistics */
	disp_if_stats(ifp);

	/* Free pending offers */
	free_offers(ifp);

	ifp->received = ifp->processed = 0;

	(void) mutex_unlock(&ifp->ifp_mtx);
	(void) mutex_destroy(&ifp->ifp_mtx);
	tifp = ifp->next;
	free(ifp);
	return (tifp);
}

/*
 * Close all interfaces, freeing up associated resources.
 */
void
close_interfaces(void)
{
	int	sendsock;

	(void) mutex_lock(&if_head_mtx);
	for (; if_head != NULL; if_head = close_interface(if_head)) {
		if (verbose) {
			dhcpmsg(LOG_INFO, "Closing interface: %s\n",
			    if_head->nm);
		}
		sendsock = if_head->senddesc;
	}
	(void) close(sendsock);
	(void) mutex_unlock(&if_head_mtx);
	(void) mutex_destroy(&if_head_mtx);
}

static int
strioctl(int fd, int cmd, int timeout, int len, char *dp)
{
	struct strioctl	si;

	si.ic_cmd = cmd;
	si.ic_timout = timeout;
	si.ic_len = len;
	si.ic_dp = dp;

	return (ioctl(fd, I_STR, &si));
}

/*
 * Convert a device id to a ppa value. From snoop.
 * e.g. "le0" -> 0
 */
static int
dev_ppa(char *device)
{
	char *p;

	p = strpbrk(device, "0123456789");
	if (p == NULL)
		return (0);
	return (atoi(p));
}

/*
 * Convert a device id to a pathname.
 * e.g. "le0" -> "/dev/le"
 */
static char *
device_path(char *device, char *bufp, int len)
{
	char *p;

	if (len < (strlen(device) + strlen("/dev/"))) {
		bufp[0] = '\0';
	} else {
		(void) strcpy(bufp, "/dev/");
		(void) strcat(bufp, device);
		for (p = bufp + (strlen(bufp) - 1); p > bufp; p--)
			if (isdigit(*p))
				*p = '\0';
	}
	return (bufp);
}

/*
 * display IF info. Must be MT Safe - called from monitor threads.
 */
static void
disp_if(IF *ifp)
{
	char ntoab[NTOABUF];

	dhcpmsg(LOG_INFO, "Thread Id: %04d - Monitoring Interface: %s *****\n",
	    ifp->if_thread, ifp->nm);
	dhcpmsg(LOG_INFO, "MTU: %d\tType: %s\n", ifp->mtu,
	    (ifp->type == DHCP_SOCKET) ? "SOCKET" : "DLPI");
	if ((ifp->flags & IFF_POINTOPOINT) == 0)
		dhcpmsg(LOG_INFO, "Broadcast: %s\n",
		    inet_ntoa_r(ifp->bcast, ntoab));
	dhcpmsg(LOG_INFO, "Netmask: %s\n", inet_ntoa_r(ifp->mask, ntoab));
	dhcpmsg(LOG_INFO, "Address: %s\n", inet_ntoa_r(ifp->addr, ntoab));
}

/*
 * Display IF statistics.
 */
void
disp_if_stats(IF *ifp)
{
	int	offers = 0;
	OFFLST	*offerp;

	dhcpmsg(LOG_INFO, "Interface statistics for: %s **************\n",
	    ifp->nm);
	for (offerp = ifp->of_head; offerp; offerp = offerp->next)
		offers++;
	dhcpmsg(LOG_INFO, "Pending DHCP offers: %d\n", offers);
	dhcpmsg(LOG_INFO, "Total Packets Transmitted: %d\n", ifp->transmit);
	dhcpmsg(LOG_INFO, "Total Packets Received: %d\n", ifp->received);
	dhcpmsg(LOG_INFO, "Total Packet Duplicates: %d\n", ifp->duplicate);
	dhcpmsg(LOG_INFO, "Total Packets Dropped: %d\n", ifp->dropped);
	dhcpmsg(LOG_INFO, "Total Packets Processed: %d\n", ifp->processed);
	dhcpmsg(LOG_INFO, "Total Protocol Errors: %d\n", ifp->errors);
}

/*
 * Setup the arp cache so that IP address 'ia' will be temporarily
 * bound to hardware address 'ha' of length 'len'.
 *
 * Returns: 0 if the arp entry was made, 1 otherwise.
 */
int
set_arp(IF *ifp, struct in_addr *ia, uchar_t *ha, int len, uchar_t flags)
{
	struct sockaddr_in	*si;
	struct arpreq		arpreq;
	int			err = 0;
	char			scratch[DHCP_SCRATCH];
	int			scratch_len;
	char			ntoab[NTOABUF];

	(void) memset((caddr_t)&arpreq, 0, sizeof (arpreq));

	arpreq.arp_pa.sa_family = AF_INET;

	/* LINTED [alignment is ok] */
	si = (struct sockaddr_in *)&arpreq.arp_pa;
	si->sin_family = AF_INET;
	si->sin_addr = *ia;	/* struct copy */

	switch (flags) {
	case DHCP_ARP_ADD:
		if (debug) {
			scratch_len = sizeof (scratch);
			if (octet_to_ascii(ha, len, scratch,
			    &scratch_len) != 0) {
				dhcpmsg(LOG_DEBUG, "Cannot convert ARP \
request to ASCII: %s: len: %d\n",
				    inet_ntoa_r(*ia, ntoab), len);
			} else {
				dhcpmsg(LOG_DEBUG,
				    "Adding ARP entry: %s == %s\n",
				    inet_ntoa_r(*ia, ntoab), scratch);
			}
		}
		arpreq.arp_flags = ATF_INUSE | ATF_COM;
		(void) memcpy(arpreq.arp_ha.sa_data, ha, len);

		if (ioctl(ifp->senddesc, SIOCSARP, (caddr_t)&arpreq) < 0) {
			dhcpmsg(LOG_ERR,
			    "ADD: Cannot modify ARP table to add: %s\n",
			    inet_ntoa_r(*ia, ntoab));
			err = 1;
		}
		break;
	case DHCP_ARP_DEL:
		/* give it a good effort, but don't worry... */
		(void) ioctl(ifp->senddesc, SIOCDARP, (caddr_t)&arpreq);
		break;
	default:
		err = 1;
		break;
	}

	return (err);
}

/*
 * Address and send a BOOTP reply packet appropriately. Does right thing
 * based on BROADCAST flag. Also checks if giaddr field is set, and
 * WE are the relay agent...
 *
 * Returns: 0 for success, nonzero otherwise (fatal)
 */
int
send_reply(IF *ifp, PKT *pp, int len, struct in_addr *dstp)
{
	int			local = FALSE;
	struct sockaddr_in	to;
	struct in_addr		if_in, cl_in;
	char			ntoab[NTOABUF];

	if (pp->giaddr.s_addr != 0L && ifp->addr.s_addr !=
	    pp->giaddr.s_addr) {
		/* Going thru a relay agent */
		to.sin_addr.s_addr = pp->giaddr.s_addr;
		to.sin_port = htons(IPPORT_BOOTPS);
	} else {
		to.sin_port = htons(IPPORT_BOOTPC);

		if (ntohs(pp->flags) & BCAST_MASK) {
			/*
			 * XXXX - what should we do if broadcast
			 * flag is set, but ptp connection?
			 */
			if (debug)
				dhcpmsg(LOG_INFO,
				    "Sending datagram to broadcast address.\n");
			to.sin_addr.s_addr = INADDR_BROADCAST;
		} else {
			/*
			 * By default, we assume unicast!
			 */
			to.sin_addr.s_addr = dstp->s_addr;

			if (debug) {
				dhcpmsg(LOG_INFO,
				    "Unicasting datagram to %s address.\n",
				    inet_ntoa_r(*dstp, ntoab));
			}
			if (ifp->addr.s_addr == pp->giaddr.s_addr) {
				/*
				 * No doubt a reply packet which we, as
				 * the relay agent, are supposed to deliver.
				 * Local Delivery!
				 */
				local = TRUE;
			} else {
				/*
				 * We can't use the giaddr field to
				 * determine whether the client is local
				 * or remote. Use the client's address,
				 * our interface's address,  and our
				 * interface's netmask to make this
				 * determination.
				 */
				if_in.s_addr = ntohl(ifp->addr.s_addr);
				if_in.s_addr &= ntohl(ifp->mask.s_addr);
				cl_in.s_addr = ntohl(dstp->s_addr);
				cl_in.s_addr &= ntohl(ifp->mask.s_addr);
				if (if_in.s_addr == cl_in.s_addr)
					local = TRUE;
			}

			if (local) {
				/*
				 * Local delivery. If we can make an
				 * ARP entry we'll unicast.
				 */
				if ((ifp->flags & IFF_NOARP) ||
				    set_arp(ifp, dstp, pp->chaddr,
				    pp->hlen, DHCP_ARP_ADD) == 0) {
					to.sin_addr.s_addr = dstp->s_addr;
				} else {
					to.sin_addr.s_addr =
					    INADDR_BROADCAST;
				}
			}
		}
	}
	return (write_interface(ifp, pp, len, &to));
}
