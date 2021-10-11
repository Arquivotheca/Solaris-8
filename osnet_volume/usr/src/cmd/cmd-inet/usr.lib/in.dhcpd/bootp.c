/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)bootp.c	1.55	99/08/27 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include <netdb.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include "icmp.h"
#include "ethers.h"
#include <locale.h>

/*
 * This file contains the code which implements the BOOTP compatibility.
 */

/*
 * We are guaranteed that the packet received is a BOOTP request packet,
 * e.g., *NOT* a DHCP packet.
 *
 * Returns nonzero if the plp is to be released, 0 if it is to be preserved,
 * due to a pending ICMP echo validation.
 */
int
bootp(IF *ifp, PKT_LIST *plp)
{
	int		release = TRUE;	/* discard by default */
	int		err = 0;
	int		pkt_len;
	int		records, write_needed = FALSE, write_error = 0;
	int		flags = 0;
	int		no_per_net = 0;
	DHCP_MSG_CATEGORIES	log;
	PKT		*rep_pktp;
	uchar_t		*optp;
	struct in_addr	netaddr, subnetaddr, tmpaddr, ciaddr;
	PN_REC		pn;
	PER_NET_DB	pndb;
	uchar_t		cid[DT_MAX_CID_LEN];
	uint_t		cid_len;
	ENCODE		*ecp, *hecp, *ethers_ecp = NULL;
	MACRO		*mp, *nmp, *cmp;
	char		network[NTOABUF], ntoab[NTOABUF];
	char		cidbuf[BUFSIZ];
	struct		hostent	h, *hp;
	char		hbuf[BUFSIZ];

#ifdef	DEBUG
	dhcpmsg(LOG_DEBUG, "BOOTP request received on %s\n", ifp->nm);
#endif	/* DEBUG */

	if (determine_network(ifp, plp, &netaddr, &subnetaddr) != 0)
		return (release);

	if ((err = open_per_net(&pndb, &netaddr, &subnetaddr)) != 0) {
		if (!ethers_compat) {
			if (verbose && err == ENOENT) {
				netaddr.s_addr &= subnetaddr.s_addr;
				dhcpmsg(LOG_INFO,
"There is no %s dhcp-network table for BOOTP client's network.\n",
				    inet_ntoa_r(netaddr, ntoab));
			}
			return (release);
		} else
			no_per_net = TRUE;
	}

	records = 0;

	/* don't need separate network address anymore. */
	netaddr.s_addr &= subnetaddr.s_addr;
	(void) inet_ntoa_r(netaddr, network);

	get_client_id(plp, cid, &cid_len);
	(void) disp_cid(plp, cidbuf, sizeof (cidbuf));

	/*
	 * let's check if we're ICMP ECHO validation failed this entry...
	 * We'll never see DHCP_ICMP_PENDING entries, because the packet
	 * demultiplexer in main.c bypasses these.
	 */

	assert(plp->d_icmpflag != DHCP_ICMP_PENDING);

	if (plp->d_icmpflag == DHCP_ICMP_IN_USE ||
	    plp->d_icmpflag == DHCP_ICMP_AVAILABLE) {
		if ((records = lookup_per_net(&pndb, PN_CLIENT_IP,
		    &plp->off_ip, sizeof (struct in_addr), NULL,
		    &pn)) != 0) {
			if (plp->d_icmpflag == DHCP_ICMP_IN_USE) {
				dhcpmsg(LOG_ERR,
"ICMP ECHO reply to BOOTP OFFER candidate: %s, disabling.\n",
				    inet_ntoa_r(pn.clientip, ntoab));
				pn.flags |= F_UNUSABLE;
				(void) put_per_net(&pndb, &pn, PN_CLIENT_IP);
				logtrans(P_BOOTP, L_ICMP_ECHO, 0, pn.clientip,
				    server_ip, plp);
				close_per_net(&pndb);
				return (release);
			}
		} else {
			dhcpmsg(LOG_ERR,
"Cannot find chosen / ICMP validated BOOTP record %s in dhcp network table.\n",
			    inet_ntoa_r(plp->off_ip, ntoab));
			close_per_net(&pndb);
			return (release);
		}
		write_needed = TRUE;
	} else {
		if (!no_per_net) {
			/*
			 * Try to find an entry for the client. We don't care
			 * about lease info here, since a BOOTP client always
			 * has a permanent lease. We also don't care about
			 * the entry owner either, unless we end up allocating
			 * a new entry for the client.
			 */
			if ((records = lookup_per_net(&pndb, PN_CID,
			    (void *)cid, cid_len, (struct in_addr *)NULL,
			    &pn)) < 0) {
				close_per_net(&pndb);
				return (release);
			}

			/*
			 * If the client's entry is unusable, then we just
			 * print a message, and give up. We don't try to
			 * allocate a new address to the client. We still
			 * consider F_AUTOMATIC to be ok, for compatibility
			 * reasons. We *won't* assign any more addresses of
			 * this type.
			 */
			if (records > 0 && ((pn.flags & F_UNUSABLE) ||
			    (pn.flags & (F_AUTOMATIC | F_BOOTP_ONLY)) == 0)) {
				dhcpmsg(LOG_INFO,
"The %1$s dhcp-network entry for BOOTP client: %2$s is marked as unusable.\n",
				    network, cidbuf);
				close_per_net(&pndb);
				return (release);	/* not fatal */
			}
		}
		if (records == 0 && ethers_compat) {
			/*
			 * Ethers mode. Try to produce a pn record. Also
			 * required is a valid boot file. If we fail on
			 * either of these tasks, we ignore the client, since
			 * it's clear from the existence of the ETHERS database
			 * entry that the Administrator intended for this
			 * client to be configured using ETHERS parameters.
			 *
			 * Note that we use chaddr directly here. We have to,
			 * since there is no dhcp-network database.
			 */
			records = lookup_ethers(&netaddr, &subnetaddr,
			    *(ether_addr_t *)&plp->pkt->chaddr[0], &pn);

			if (records != 0) {
				if (ethers_encode(ifp, &pn.clientip,
				    &ethers_ecp) != 0) {
					if (verbose) {
						dhcpmsg(LOG_INFO,
"No bootfile information for Ethers client: %1$s -> %2$s. Client ignored.\n",
						    cidbuf,
						    inet_ntoa_r(pn.clientip,
						    ntoab));
					}
					if (!no_per_net)
						close_per_net(&pndb);
					return (release);
				}
			}
			if (records != 0 && verbose) {
				dhcpmsg(LOG_INFO,
"Client: %1$s IP address binding (%2$s) found in ETHERS database.\n",
				    cidbuf, inet_ntoa_r(pn.clientip, ntoab));
			}
		}

		/*
		 * If the client thinks it knows who it is (ciaddr), and this
		 * doesn't match our registered IP address, then display an
		 * error message and give up.
		 */
		ciaddr.s_addr = plp->pkt->ciaddr.s_addr;
		if (records > 0 && ciaddr.s_addr != 0L && pn.clientip.s_addr !=
		    ciaddr.s_addr) {
			dhcpmsg(LOG_INFO,
"BOOTP client: %1$s notion of IP address (ciaddr = %2$s) is incorrect.\n",
			    cidbuf, inet_ntoa_r(ciaddr, ntoab));
			if (!no_per_net)
				close_per_net(&pndb);
			if (ethers_ecp != NULL)
				free_encode_list(ethers_ecp);
			return (release);
		}

		/*
		 * Neither the dhcp-network table nor the ethers table had any
		 * valid mappings. Try to allocate a new one if possible.
		 */
		if (records == 0) {
			if (no_per_net) {
				/* Nothing to allocate from. */
				if (verbose) {
					dhcpmsg(LOG_INFO,
"There is no %s dhcp-network table for BOOTP client's network.\n",
					    inet_ntoa_r(netaddr, ntoab));
				}
				return (release);
			}

			if (be_automatic == 0) {
				/*
				 * Not allowed. Sorry.
				 */
				if (verbose) {
					dhcpmsg(LOG_INFO,
"BOOTP client: %s is looking for a configuration.\n",
					    cidbuf);
				}
				close_per_net(&pndb);
				return (release);
			}

			/*
			 * The client doesn't have an entry, and we are free to
			 * give out F_BOOTP_ONLY addresses to BOOTP clients.
			 */
			write_needed = TRUE;

			/*
			 * If the client specified an IP address, then let's
			 * check if that one is available, since we have no
			 * CID mapping registered for this client.
			 */
			if (ciaddr.s_addr != 0L) {
				tmpaddr.s_addr = ciaddr.s_addr &
				    subnetaddr.s_addr;
				if (tmpaddr.s_addr != netaddr.s_addr) {
					dhcpmsg(LOG_INFO,
"BOOTP client: %1$s trying to boot on wrong net: %2$s\n", cidbuf,
					    inet_ntoa_r(tmpaddr, ntoab));
					close_per_net(&pndb);
					return (release);
				}
				if ((records = lookup_per_net(&pndb,
				    PN_CLIENT_IP, (void *)&ciaddr,
				    sizeof (struct in_addr), &server_ip,
				    &pn)) < 0) {
					close_per_net(&pndb);
					return (release);
				}
				if (records > 0 &&
				    ((pn.flags & F_BOOTP_ONLY) == 0 ||
				    (pn.flags & F_UNUSABLE) || cid_len != 0)) {
					/*
					 * This is not a free IP address. We
					 * would have found an assigned IP
					 * address earlier when we looked by
					 * CID, so it is not necessary to
					 * compare the CIDs of the client
					 * and pn record here.
					 */
					dhcpmsg(LOG_INFO,
"BOOTP client: %1$s wrongly believes it is using IP address: %2$s\n",
					    cidbuf, inet_ntoa_r(ciaddr, ntoab));
					close_per_net(&pndb);
					return (release);
				}
			}
			if (records == 0) {
				/* Still nothing. Try to pick one. */
				records = select_offer(&pndb, plp, ifp, &pn);
			}

			if (records == 0) {
				dhcpmsg(LOG_INFO, "(%1$s) No more IP addresses "
				    "for %2$s network.\n",
				    cidbuf, network);
				close_per_net(&pndb);
				return (release);
			}

		}

		/*
		 * check the address. But only if client doesn't
		 * know its address.
		 */
		if (ciaddr.s_addr == 0L) {
			if ((ifp->flags & IFF_NOARP) == 0) {
				(void) set_arp(ifp, &pn.clientip,
				    NULL, 0, DHCP_ARP_DEL);
			}
			if (!noping) {
				/*
				 * If you can't create the ping thread,
				 * let the plp fall by the wayside.
				 * Otherwise pretend we're done.
				 */
				if (icmp_echo_register(ifp, &pn,
				    plp) != 0) {
					dhcpmsg(LOG_ERR,
"ICMP ECHO reply check cannot be registered for: %s, ignoring\n",
					    inet_ntoa_r(pn.clientip,
					    ntoab));
				} else
					release = FALSE; /* preserve */
				close_per_net(&pndb);
				return (release);
			}
		}
	}

	/*
	 * It is possible that the client could specify a REQUEST list,
	 * but then it would be a DHCP client, wouldn't it? Only copy the
	 * std option list, since that potentially could be changed by
	 * load_options().
	 */
	ecp = NULL;
	if (!no_dhcptab) {
		if ((nmp = get_macro(network)) != NULL)
			ecp = copy_encode_list(nmp->head);
		if ((mp = get_macro(pn.macro)) != NULL)
			ecp = combine_encodes(ecp, mp->head, ENC_DONT_COPY);
		if ((cmp = get_macro(cidbuf)) != NULL)
			ecp = combine_encodes(ecp, cmp->head, ENC_DONT_COPY);

		/* If dhcptab configured to return hostname, do so. */
		if (find_encode(ecp, CD_BOOL_HOSTNAME) != NULL) {
			hp = gethostbyaddr_r((char *)&pn.clientip,
			    sizeof (struct in_addr), AF_INET, &h, hbuf,
			    sizeof (hbuf), &err);
			if (hp != NULL) {
				hecp = make_encode(CD_HOSTNAME,
				    strlen(hp->h_name), hp->h_name, 1);
				replace_encode(&ecp, hecp, ENC_DONT_COPY);
			}
		}
	}

	/* Add ethers "magic" data, if it exists */
	if (ethers_ecp != NULL)
		ecp = combine_encodes(ecp, ethers_ecp, ENC_DONT_COPY);

	/* Produce a BOOTP reply. */
	rep_pktp = gen_bootp_pkt(sizeof (PKT), plp->pkt);

	rep_pktp->op = BOOTREPLY;
	optp = rep_pktp->options;

	/* set the client's IP address */
	rep_pktp->yiaddr.s_addr = pn.clientip.s_addr;

	/*
	 * Omit lease time options implicitly, e. g.
	 * ~(DHCP_DHCP_CLNT | DHCP_SEND_LEASE)
	 */
	if (!plp->rfc1048)
		flags |= DHCP_NON_RFC1048;

	/* Now load in configured options */
	pkt_len = load_options(flags, plp, rep_pktp, sizeof (PKT), optp, ecp,
	    NULL);

	free_encode_list(ecp);
	if (ethers_ecp != NULL)
		free_encode_list(ethers_ecp);

	if (pkt_len < sizeof (PKT))
		pkt_len = sizeof (PKT);

	(void) memcpy(pn.cid, cid, cid_len);
	pn.cid_len = cid_len;
	pn.lease = htonl(DHCP_PERM);

	if (write_needed) {
		write_error = put_per_net(&pndb, &pn, PN_CLIENT_IP);
		log = L_ASSIGN;
	} else {
		if (verbose && !no_per_net) {
			dhcpmsg(LOG_INFO,
"Database write unnecessary for BOOTP client: %1$s, %2$s\n",
			    cidbuf, inet_ntoa_r(pn.clientip, ntoab));
		}
		log = L_REPLY;
	}

	if (write_error == 0) {
		if (send_reply(ifp, rep_pktp, pkt_len,
		    &rep_pktp->yiaddr) != 0) {
			dhcpmsg(LOG_ERR, "Reply to BOOTP client %s failed.\n",
			    cidbuf);
		}
		logtrans(P_BOOTP, log, ntohl(pn.lease), pn.clientip, server_ip,
		    plp);
	}

	free(rep_pktp);
	if (!no_per_net)
		close_per_net(&pndb);
	return (release);
}
