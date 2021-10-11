/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)icmp.c	1.26	99/02/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <thread.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <v4_sum_impl.h>
#include <locale.h>

#define	ICMP_ECHO_SIZE	(sizeof (struct icmp) + 36)

static void
icmp_disp_flag(enum dhcp_icmp_flag icmpflag, char *flagstr)
{
	switch (icmpflag) {
	case DHCP_ICMP_NOENT:
		/* No pending icmp check */
		(void) strcpy(flagstr, "NOENT");
		break;
	case DHCP_ICMP_PENDING:
		/* Echo check is still pending */
		(void) strcpy(flagstr, "PENDING");
		break;
	case DHCP_ICMP_AVAILABLE:
		/* Address is not in use */
		(void) strcpy(flagstr, "AVAILABLE");
		break;
	case DHCP_ICMP_IN_USE:
		/* Address is in use */
		(void) strcpy(flagstr, "IN_USE");
		break;
	case DHCP_ICMP_FAILED:
		/* Error; results unknown. */
		(void) strcpy(flagstr, "FAILED");
		break;
	case DHCP_ICMP_DONTCARE:
		/* icmp check in some !NOENT state */
		(void) strcpy(flagstr, "DONTCARE");
		break;
	default:
		/* Huh? */
		(void) strcpy(flagstr, "UNKNOWN");
		break;
	}
}

/*
 * An implementation of async ICMP ECHO for use in detecting addresses already
 * in use. Address argument expected in network order. The argument is
 * a PKT_LIST pointer to the plp (DISCOVER packet) which caused the main
 * thread to attempt to OFFER an IP address. If the thread discovers that
 * the address is in use, it changes the d_icmpflag field in the PKT_LIST
 * structure accordingly. See icmp.h for more details.
 *
 * NOTES: Not interface specific. We use our routing tables to route the
 * messages correctly, and collect responses. This may mean that we
 * receive an ICMP ECHO reply thru an interface the daemon has not been
 * directed to watch. However, I believe that *ANY* echo reply means
 * trouble, regardless of the route taken!
 */
static void *
icmp_echo_async(void *arg)
{
	PKT_LIST		*plp = (PKT_LIST *)arg;
	int			s;
	char			flagstr[DHCP_SCRATCH];
	u_long			outpack[DHCP_SCRATCH/sizeof (u_long)];
	u_long			inpack[DHCP_SCRATCH/sizeof (u_long)];
	struct icmp 		*icp;
	int			sequence = 0;
	struct sockaddr_in	to, from;
	socklen_t		fromlen;
	struct ip		*ipp;
	int			icmp_identifier;
	int			s_cnt, r_cnt;
	u_short			ip_hlen;
	clock_t			recv_intrvl;
	struct pollfd		pfd;
	int			i;
	char			offipstr[NTOABUF];

	(void) inet_ntoa_r(plp->off_ip, offipstr);

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		dhcpmsg(LOG_ERR,
		    "Error opening raw socket for ICMP (ping %s).\n", offipstr);
		(void) mutex_lock(&plp->plp_mtx);
		plp->d_icmpflag = DHCP_ICMP_FAILED;
		(void) mutex_unlock(&plp->plp_mtx);
		goto icmp_async_exit;
	}

	if (fcntl(s, F_SETFL, O_NDELAY) == -1) {
		dhcpmsg(LOG_ERR,
		    "Error setting ICMP socket to no delay. (ping %s)\n",
		    offipstr);
		(void) close(s);
		(void) mutex_lock(&plp->plp_mtx);
		plp->d_icmpflag = DHCP_ICMP_FAILED;
		(void) mutex_unlock(&plp->plp_mtx);
		goto icmp_async_exit;
	}

	pfd.fd = s;
	pfd.events = POLLIN | POLLPRI;
	pfd.revents = 0;

	icmp_identifier = (int)thr_self() & (u_short)-1;
	outpack[10] = 0x12345678;
	icp = (struct icmp *)outpack;
	icp->icmp_code = 0;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_id = icmp_identifier;

	(void) memset((void *)&to, 0, sizeof (struct sockaddr_in));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = plp->off_ip.s_addr;

	/*
	 * We make icmp_tries attempts to contact the target. We
	 * wait the same length of time for a response in both cases.
	 */
	for (i = 0; i < icmp_tries && plp->d_icmpflag != DHCP_ICMP_IN_USE;
	    i++) {
		icp->icmp_seq = sequence++;
		icp->icmp_cksum = 0;
		icp->icmp_cksum = ipv4cksum((uint16_t *)icp, ICMP_ECHO_SIZE);

		/* Deliver our ECHO. */
		s_cnt = sendto(s, (char *)outpack, ICMP_ECHO_SIZE, 0,
		    (struct sockaddr *)&to, sizeof (struct sockaddr));

		if (s_cnt < 0 || s_cnt != ICMP_ECHO_SIZE) {
			dhcpmsg(LOG_ERR,
			    "Error sending ICMP message. (ping %s).\n",
			    offipstr);
			(void) close(s);
			(void) mutex_lock(&plp->plp_mtx);
			plp->d_icmpflag = DHCP_ICMP_FAILED;
			(void) mutex_unlock(&plp->plp_mtx);
			goto icmp_async_exit;
		}

		/* Collect replies. */
		recv_intrvl = clock() + (clock_t)(icmp_timeout * 1000);
		while (clock() < recv_intrvl) {
			if (poll(&pfd, (nfds_t)1, icmp_timeout) < 0 ||
			    pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
				/* EINTR is masked  - must be serious */
				dhcpmsg(LOG_ERR,
				    "Poll error: ICMP reply for %s.\n",
				    offipstr);
				(void) close(s);
				(void) mutex_lock(&plp->plp_mtx);
				plp->d_icmpflag = DHCP_ICMP_FAILED;
				(void) mutex_unlock(&plp->plp_mtx);
				goto icmp_async_exit;
			}

			if (!pfd.revents)
				break;	/* no data, timeout */

			fromlen = sizeof (from);
			if ((r_cnt = recvfrom(s, (char *)inpack,
			    sizeof (inpack), 0, (struct sockaddr *)&from,
			    &fromlen)) < 0) {
				if (errno == EAGAIN)
					continue;
				/* EINTR is masked  - must be serious */
				dhcpmsg(LOG_ERR,
				    "Recvfrom error: ICMP reply for %s.\n",
				    offipstr);
				(void) close(s);
				(void) mutex_lock(&plp->plp_mtx);
				plp->d_icmpflag = DHCP_ICMP_FAILED;
				(void) mutex_unlock(&plp->plp_mtx);
				goto icmp_async_exit;
			}

			if (from.sin_addr.s_addr != plp->off_ip.s_addr)
				continue; /* Not from the IP of interest */
			/*
			 * We know we got an ICMP message of some type from
			 * the IP of interest. Be conservative and
			 * consider it in use. The following logic is just
			 * for identifying problems in the response.
			 */
			(void) mutex_lock(&plp->plp_mtx);
			plp->d_icmpflag = DHCP_ICMP_IN_USE;
			(void) mutex_unlock(&plp->plp_mtx);

			if (!debug)
				break;

			ipp = (struct ip *)inpack;
			if (r_cnt != ntohs(ipp->ip_len)) {
				/* bogus IP header */
				dhcpmsg(LOG_NOTICE,
"Malformed ICMP message received from host %s: len %d != %d\n",
				    offipstr, r_cnt, ntohs(ipp->ip_len));
				break;
			}
			ip_hlen = ipp->ip_hl << 2;
			if (r_cnt < (int)(ip_hlen + ICMP_MINLEN)) {
				dhcpmsg(LOG_NOTICE,
"ICMP message received from host %s is too small.\n",
				    offipstr);
				break;
			}
			icp = (struct icmp *)((u_int)inpack + ip_hlen);
			if (ipv4cksum((uint16_t *)icp,
			    ntohs(ipp->ip_len) - ip_hlen) != 0) {
				dhcpmsg(LOG_NOTICE,
"Bad checksum on incoming ICMP echo reply. (ping %s)\n", offipstr);
			}
			if (icp->icmp_type != ICMP_ECHOREPLY) {
				dhcpmsg(LOG_NOTICE,
				    "Unexpected ICMP type %d from %s.\n",
				    icp->icmp_type, offipstr);
			}
			if (icp->icmp_id != icmp_identifier) {
				dhcpmsg(LOG_NOTICE,
				    "ICMP message id mismatch (from %s).\n",
				    offipstr);
			}
			if (icp->icmp_seq != (sequence - 1)) {
				dhcpmsg(LOG_NOTICE,
"ICMP sequence mismatch: %d != %d (ping %s)\n",
				    icp->icmp_seq, sequence - 1, offipstr);
			}
			break;
		}
	}
	(void) close(s);

icmp_async_exit:
	(void) mutex_lock(&plp->plp_mtx);
	if (plp->d_icmpflag == DHCP_ICMP_PENDING)
		plp->d_icmpflag = DHCP_ICMP_AVAILABLE;
	(void) mutex_unlock(&plp->plp_mtx);
	if (debug) {
		icmp_disp_flag(plp->d_icmpflag, flagstr);
		dhcpmsg(LOG_DEBUG,
		    "ICMP thread %d exiting, IP: %s = plp->d_icmpflag: %s...\n",
		    thr_self(), offipstr, flagstr);
	}

	/*
	 * Notify main thread that there is a validated address in the list.
	 */
	(void) mutex_lock(&npkts_mtx);
	npkts++;
	(void) cond_signal(&npkts_cv);
	(void) mutex_unlock(&npkts_mtx);

	thr_exit(NULL);
	return (NULL);	/* NOTREACHED */
}

/*
 * Scan thru the packet list for the named interface, looking for entries
 * with the d_icmpflag value matching that of the argument icmpf. If ip is
 * nonnull, further limit the match to those entries with ip as the offered
 * ip address. You can also test the existence of a record with an
 * "logical NOT" of the d_icmpflag value.
 *
 * Returns: TRUE if one or more entries exist, FALSE if they don't.
 */
int
icmp_echo_status(IF *ifp, struct in_addr *ip, int bool,
    enum dhcp_icmp_flag icmpf)
{
	PKT_LIST	*plp;
	int		found = FALSE;

	(void) mutex_lock(&ifp->pkt_mtx);
	for (plp = ifp->pkthead; plp != NULL && !found; plp = plp->next) {
		(void) mutex_lock(&plp->plp_mtx);
		if (ip != NULL) {
			if (ip->s_addr == plp->off_ip.s_addr) {
				if (icmpf == DHCP_ICMP_DONTCARE ||
				    bool == (plp->d_icmpflag == icmpf))
					found = TRUE;
			}
		} else {
			if (bool == (plp->d_icmpflag == icmpf))
				found = TRUE;
		}
		(void) mutex_unlock(&plp->plp_mtx);
	}

#ifdef	DEBUG_PKTLIST
	display_pktlist(ifp);
#endif	/* DEBUG_PKTLIST */

	(void) mutex_unlock(&ifp->pkt_mtx);
	return (found);
}

/*
 * Register a request for an asynchronous ICMP echo check of the IP address
 * associated with pnp->clientip. Creates a thread to make the request and
 * wait for the reply. The main thread returns immediately for more processing.
 *
 * Returns: 0 for successful submission of request; nonzero if a failure
 * occurred.
 */
int
icmp_echo_register(IF *ifp, PN_REC *pnp, PKT_LIST *plp)
{
	char		flagstr[DHCP_SCRATCH];
	char		ntoab[NTOABUF];
	thread_t	ping_thread;

	if (pnp->clientip.s_addr == htonl(INADDR_ANY)) {
		dhcpmsg(LOG_ERR, "Rejecting ICMP validate of 0.0.0.0.\n");
		return (EINVAL);
	}

	/*
	 * Make sure we're not already pinging this client. We should never
	 * be attempting to reregister for an address which is pending.
	 * The main loop in main.c is supposed to avoid DHCP_ICMP_PENDING
	 * addresses. (Thus this should never fail....)
	 */
	if (icmp_echo_status(ifp, &pnp->clientip, FALSE,
	    DHCP_ICMP_NOENT) == TRUE) {
		dhcpmsg(LOG_ERR,
"Trying to ICMP validate IP which has an ICMP check already pending: %s\n",
		    inet_ntoa_r(pnp->clientip, ntoab));
		return (EEXIST);
	}

	if (thr_create(NULL, 0, icmp_echo_async, (void *)plp,
	    THR_SUSPENDED | THR_DETACHED, &ping_thread) != 0) {
		dhcpmsg(LOG_ERR,
		    "Cannot create async icmp thread to validate IP: %s\n",
		    inet_ntoa_r(plp->off_ip, ntoab));
		(void) mutex_lock(&plp->plp_mtx);
		plp->d_icmpflag = DHCP_ICMP_FAILED;
		(void) mutex_unlock(&plp->plp_mtx);
		return (errno);
	}

	/*
	 * put the packet back on the list, and let the thread go
	 * at it. We don't sweat duplicates - main() should weed
	 * these out for us.
	 */
	(void) mutex_lock(&ifp->pkt_mtx);

	(void) mutex_lock(&plp->plp_mtx);
	plp->d_icmpflag = DHCP_ICMP_PENDING;
	plp->off_ip.s_addr = pnp->clientip.s_addr;
	(void) mutex_unlock(&plp->plp_mtx);

	if (ifp->pkthead == NULL) {
		ifp->pkthead = ifp->pkttail = plp;
		plp->prev = NULL;
	} else {
		ifp->pkttail->next = plp;
		plp->prev = ifp->pkttail;
		ifp->pkttail = plp;
	}
	plp->next = NULL;
#ifdef	DEBUG_PKTLIST
	display_pktlist(ifp);
#endif	/* DEBUG_PKTLIST */

	(void) mutex_unlock(&ifp->pkt_mtx);

	if (debug) {
		icmp_disp_flag(plp->d_icmpflag, flagstr);
		dhcpmsg(LOG_DEBUG,
		    "Started ICMP thread %d to validate IP %s, %s\n",
		    ping_thread, inet_ntoa_r(pnp->clientip, ntoab), flagstr);
	}

	/* continue the thread */
	(void) thr_continue(ping_thread);
	return (0);
}
