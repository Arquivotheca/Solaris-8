#ident	"@(#)generic.c	1.47	99/08/16 SMI"

/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains routines that are shared between the DHCP server
 * implementation and BOOTP server compatibility.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <alloca.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <sys/syslog.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

extern int getnetmaskbyaddr(struct in_addr, struct in_addr *);

/*
 * Get the client id.
 *
 * Sets cid and len.
 */
void
get_client_id(PKT_LIST *plp, uchar_t *cid, uint_t *len)
{
	DHCP_OPT *optp;

	optp = plp->opts[CD_CLIENT_ID];	/* get pointer to options */

	/*
	 * If the client specified the client id option, use that,
	 * otherwise use the client's hardware type and hardware address.
	 */
	if (optp != NULL) {
		*len = optp->len;
		(void) memcpy(cid, optp->value, *len);
	} else {
		*cid++ = plp->pkt->htype;
		*len = plp->pkt->hlen + 1;
		(void) memcpy(cid, plp->pkt->chaddr, *len);
	}
}

/*
 * Return a string representing an ASCII version of the client_id.
 */
char *
disp_cid(PKT_LIST *plp, char *bufp, int len)
{
	DHCP_OPT	*optp = plp->opts[CD_CLIENT_ID];
	uchar_t	*cp;
	uchar_t cplen;
	int tlen;

	if (optp != (DHCP_OPT *)0) {
		cp =  optp->value;
		cplen = optp->len;
	} else {
		cp = plp->pkt->chaddr;
		cplen =  plp->pkt->hlen;
	}

	tlen = len;
	(void) octet_to_ascii((uchar_t *)cp, cplen, bufp, &tlen);
	return (bufp);
}

/*
 * Based on the contents of the PKT_LIST structure for an incoming
 * packet, determine the net address and subnet mask identifying the
 * dhcp-network database.
 *
 * Returns: 0 for success, 1 if unable to determine settings.
 */
int
determine_network(IF *ifp, PKT_LIST *plp, struct in_addr *netp,
    struct in_addr *subp)
{
	if (!netp || !subp || !ifp || !plp)
		return (1);

	if (plp->pkt->giaddr.s_addr != 0) {
		netp->s_addr = plp->pkt->giaddr.s_addr;
		/*
		 * Packet received thru a relay agent. Calculate the
		 * net's address using subnet mask and giaddr.
		 */
		(void) get_netmask(netp, &subp);
	} else {
		/* Locally connected net. */
		netp->s_addr = ifp->addr.s_addr;
		subp->s_addr = ifp->mask.s_addr;
	}
	return (0);
}

/*
 * Given a network-order address, calculate client's default net mask.
 * Consult netmasks database to see if net is further subnetted.
 * We'll only snag the first netmask that matches our criteria.
 *
 * Returns 0 for success, 1 otherwise.
 */
int
get_netmask(struct in_addr *n_addrp, struct in_addr **s_addrp)
{
	struct in_addr	ti, ts, tp;

	if (n_addrp == NULL || s_addrp == NULL)
		return (1);

	/*
	 * First check if VLSM is in use. Fall back on
	 * standard classed networks.
	 */
	if (getnetmaskbyaddr(*n_addrp, &tp) != 0) {
		ti.s_addr = ntohl(n_addrp->s_addr);
		if (IN_CLASSA(ti.s_addr)) {
			ts.s_addr = (ipaddr_t)IN_CLASSA_NET;
		} else if (IN_CLASSB(ti.s_addr)) {
			ts.s_addr = (ipaddr_t)IN_CLASSB_NET;
		} else {
			ts.s_addr = (ipaddr_t)IN_CLASSC_NET;
		}
		(*s_addrp)->s_addr = htonl(ts.s_addr); /* default */
	} else {
		(*s_addrp)->s_addr = tp.s_addr;
	}

	return (0);
}

/*
 * This function is charged with loading the options field with the
 * configured and/or asked for options. Note that if the packet is too
 * small to fit the options, then option overload is enabled.
 *
 * Note that the caller is expected for free any allocated ENCODE lists,
 * with the exception of locally-allocated lists in the case where ecp is
 * NULL, but vecp is not. In this case, the resultant ecp list (ecp == tvep)
 * is freed locally.
 *
 * Returns: The actual size of the utilized packet buffer.
 */
int
load_options(int flags, PKT_LIST *c_plp, PKT *r_pktp, int replen, uchar_t *optp,
    ENCODE *ecp, ENCODE *vecp)
{
	ENCODE		*ep, *prevep, *tvep = NULL;
	PKT		*c_pktp = c_plp->pkt;
	ushort_t	code;
	uint_t		vend_len;
	uchar_t		len, *vp, *vdata, *data, *endp, *main_optp, *opt_endp;
	uchar_t		overload = DHCP_OVRLD_CLR,
			    using_overload = DHCP_OVRLD_CLR;
	boolean_t	srv_using_file = B_FALSE, clnt_ovrld_file = B_FALSE;
	boolean_t	echo_clnt_file;

	if (c_plp->opts[CD_OPTION_OVERLOAD] != NULL &&
	    *c_plp->opts[CD_OPTION_OVERLOAD]->value & DHCP_OVRLD_FILE)
		clnt_ovrld_file = B_TRUE;

	opt_endp = (uchar_t *)((uint_t)r_pktp->options + replen -
	    BASE_PKT_SIZE);
	endp = opt_endp;

	/*
	 * We handle vendor options by fabricating an ENCODE of type
	 * CD_VENDOR_SPEC, and setting its datafield equal to vecp.
	 *
	 * We assume we've been handed the proper class list.
	 */
	if (vecp != NULL && (flags & DHCP_NON_RFC1048) == 0) {
		vend_len = 0;
		for (ep = vecp, vend_len = 0; ep != NULL; ep = ep->next)
			vend_len += (ep->len + 2);

		if (vend_len != 0) {
			if (vend_len > (uint_t)0xff) {
				dhcpmsg(LOG_WARNING,
				    "Warning: Too many Vendor options\n");
				vend_len = (uint_t)0xff;
			}
			vdata = (uchar_t *)smalloc(vend_len);

			for (vp = vdata, tvep = vecp; tvep != NULL &&
			    (uchar_t *)(vp + tvep->len + 2) <= &vdata[vend_len];
			    tvep = tvep->next) {
				*vp++ = tvep->code;
				*vp++ = tvep->len;
				(void) memcpy(vp, tvep->data, tvep->len);
				vp += tvep->len;
			}

			/* this make_encode *doesn't* copy data */
			tvep = make_encode(CD_VENDOR_SPEC, vend_len,
			    (void *)vdata, 0);

			/* Tack it on the end of standard list. */
			for (ep = prevep = ecp; ep != NULL; ep = ep->next)
				prevep = ep;
			if (prevep != NULL)
				prevep->next = tvep;
			else
				ecp = tvep;
		}
	}

	/*
	 * Scan the options first to determine if we could potentially
	 * option overload.
	 */
	if (flags & DHCP_DHCP_CLNT) {
		for (ep = ecp; ep != NULL; ep = ep->next) {
			switch (ep->code) {
			case CD_SNAME:
				overload |= DHCP_OVRLD_SNAME;
				break;
			case CD_BOOTFILE:
				overload |= DHCP_OVRLD_FILE;
				srv_using_file = B_TRUE;
				break;
			}
		}
	} else
		overload = ~DHCP_OVRLD_MASK;	/* No overload for BOOTP */

	if (c_pktp->file[0] != '\0' && !clnt_ovrld_file && !srv_using_file) {
		/*
		 * simply echo back client's boot file, and don't overload.
		 * if CD_BOOTPATH is set, we'll simply rewrite the r_pktp
		 * file field to include it along with the client's requested
		 * name during the load pass through the internal options.
		 * Here we let the overload code know we're not to overload
		 * the file field.
		 */
		(void) memcpy(r_pktp->file, c_pktp->file,
		    sizeof (r_pktp->file));
		overload |= DHCP_OVRLD_FILE;
		echo_clnt_file = B_TRUE;
	} else
		echo_clnt_file = B_FALSE;

	/* Now actually load the options! */
	for (ep = ecp; ep != NULL; ep = ep->next) {
		code = ep->code;
		len = ep->len;
		data = ep->data;

		/*
		 * non rfc1048 clients can only get packet fields and
		 * the CD_BOOTPATH internal pseudo opt, which only potentially
		 * affects the file field.
		 */
		if ((flags & DHCP_NON_RFC1048) &&
		    !((code >= CD_PACKET_START && code <= CD_PACKET_END) ||
		    code == CD_BOOTPATH)) {
			continue;
		}

		if ((flags & DHCP_SEND_LEASE) == 0 && (code == CD_T1_TIME ||
		    code == CD_T2_TIME || code == CD_LEASE_TIME)) {
			continue;
		}

		/* standard and site options */
		if (code >= DHCP_FIRST_OPT && code <= DHCP_LAST_OPT) {

			uchar_t	*need_optp;

			/*
			 * Keep an eye on option field. Option overload. Note
			 * that we need to keep track of the space necessary
			 * to place the Overload option in the options section
			 * (that's the 3 octets below.) The 2 octets cover the
			 * necessary code and len portion of the payload.
			 */
			if (using_overload == DHCP_OVRLD_CLR) {
				/* 2 for code/len, 3 for overload option */
				need_optp = &optp[len + 2 + 3];
			} else {
				/* Just need 2 for code/len */
				need_optp = &optp[len + 2];
			}
			if (need_optp > endp) {
				/*
				 * If overload is not possible, we will
				 * keep going, hoping to find an option
				 * that will fit in the remaining space,
				 * rather than just give up.
				 */
				if (overload != (uchar_t)~DHCP_OVRLD_MASK) {
					if (using_overload == DHCP_OVRLD_CLR) {
						*optp++ = CD_OPTION_OVERLOAD;
						*optp++ = 1;
						main_optp = optp;
					} else {
						if (optp < endp)
							*optp = CD_END;
						overload |= using_overload;
					}
				}
				switch (overload) {
				case DHCP_OVRLD_CLR:
					/* great, can use both */
					/* FALLTHRU */
				case DHCP_OVRLD_FILE:
					/* Can use sname. */
					optp = r_pktp->sname;
					endp = r_pktp->file;
					using_overload |= DHCP_OVRLD_SNAME;
					break;
				case DHCP_OVRLD_SNAME:
					/* Using sname, can use file. */
					optp = r_pktp->file;
					endp = r_pktp->cookie;
					using_overload |= DHCP_OVRLD_FILE;
					break;
				}
			} else {
				/* Load options. */
				*optp++ = (uchar_t)code;
				*optp++ = len;
				(void) memcpy(optp, data, len);
				optp += len;
			}
		} else if (code >= CD_PACKET_START && code <= CD_PACKET_END) {
			/* packet field pseudo options */
			switch (code) {
			case CD_SIADDR:
				/*
				 * Configuration includes Boot server addr
				 */
				(void) memcpy((void *)&r_pktp->siaddr, data,
				    len);
				break;
			case CD_SNAME:
				/*
				 * Configuration includes Boot server name
				 */
				(void) memcpy(r_pktp->sname, data, len);
				break;
			case CD_BOOTFILE:
				/*
				 * Configuration includes boot file.
				 * Always authoritative.
				 */
				(void) memset(r_pktp->file, 0,
				    sizeof (r_pktp->file));
				(void) memcpy(r_pktp->file, data, len);
				break;
			default:
				dhcpmsg(LOG_ERR,
				    "Unknown DHCP packet field: %d\n", code);
				break;
			}
		} else if (code >= CD_INTRNL_START && code <= CD_INTRNL_END) {
			/* Internal server pseudo options */
			switch (code) {
			case CD_BOOTPATH:
				/*
				 * Prefix for boot file. Only used if
				 * client provides bootfile and server doesn't
				 * specify one. Prepended on client's bootfile
				 * value. Otherwise ignored.
				 */
				if (echo_clnt_file) {
					uchar_t alen, flen;

					alen = sizeof (c_pktp->file);
					flen = alen - 1;
					if (c_pktp->file[flen] != '\0')
						flen++;
					else
						flen = strlen(
						    (char *)c_pktp->file);

					if ((len + flen + 1) > alen) {
						char *bp = alloca(alen + 1);
						char *bf = alloca(alen + 1);
						(void) memcpy(bp, data, len);
						bp[len] = '\0';
						(void) memcpy(bf, c_pktp->file,
						    flen);
						bf[flen] = '\0';
						dhcpmsg(LOG_ERR,
						    "BootPath(%1$s) + "
						    "BootFile(%2$s) too "
						    "long: %3$d > %4$d\n",
						    bp, bf, (len + flen), alen);
					} else {
						(void) memcpy(r_pktp->file,
						    data, len);
						r_pktp->file[len] = '/';
						(void) memcpy(
						    &r_pktp->file[len + 1],
						    c_pktp->file, flen);
					}
				}
				break;
			case CD_BOOL_HOSTNAME:
				/* FALLTHRU */
			case CD_BOOL_LEASENEG:
				/* FALLTHRU */
			case CD_BOOL_ECHO_VCLASS:
				/*
				 * These pseudo opts have had their
				 * affect elsewhere, such as dhcp.c.
				 */
				break;
			default:
				dhcpmsg(LOG_ERR,
				    "Unknown Internal pseudo opt: %d\n", code);
				break;
			}
		} else {
			dhcpmsg(LOG_ERR,
			    "Unrecognized option with code: %d\n", code);
		}
	}

	if (using_overload != DHCP_OVRLD_CLR) {
		*main_optp++ = using_overload;
		if (optp < endp)
			*optp = CD_END;
	} else
		main_optp = optp;	/* no overload */

	if (main_optp < opt_endp)
		*main_optp++ = CD_END;

	if (ecp == tvep)
		free_encode_list(ecp);

	return (BASE_PKT_SIZE + (uint_t)(main_optp - r_pktp->options));
}

/*
 * Note: if_head_mtx cannot be held by caller.
 */
int
idle(void)
{
	int	ttotpkts;
	int	err = 0;
	IF	*ifp;
	struct in_addr	zeroip;

	if (reinitialize || (abs_rescan != 0 && abs_rescan < time(NULL))) {
		/*
		 * Got a signal to reinitialize
		 */
		errno = 0;

		if (verbose)
			dhcpmsg(LOG_INFO, "Reinitializing server\n");

		if (!no_dhcptab) {
			if (checktab() != 0) {
				dhcpmsg(LOG_WARNING,
				    "WARNING: Cannot access dhcptab.\n");
			} else {
				if ((err = readtab(PRESERVE_DHCPTAB)) != 0) {
					dhcpmsg(LOG_ERR,
					    "Error reading dhcptab.\n");
					return (err);
				}
			}
		}
		if (verbose) {
			(void) mutex_lock(&totpkts_mtx);
			ttotpkts = totpkts;
			(void) mutex_unlock(&totpkts_mtx);
			dhcpmsg(LOG_INFO,
			    "Total Packets received on all interfaces: %d\n",
			    ttotpkts);
		}

		/*
		 * Drop all pending offers, display interface statistics.
		 */
		(void) mutex_lock(&if_head_mtx);
		for (ifp = if_head; ifp; ifp = ifp->next) {
			if (verbose)
				disp_if_stats(ifp);
			free_offers(ifp);
		}
		(void) mutex_unlock(&if_head_mtx);

		if (verbose)
			dhcpmsg(LOG_INFO, "Server reinitialized.\n");

		reinitialize = 0;	/* reset the flag */
		if (abs_rescan != 0)
			abs_rescan = (rescan_interval * 60L) + time(NULL);
	} else {
		/*
		 * Scan for expired offers; clean up.
		 */
		zeroip.s_addr = INADDR_ANY;
		(void) mutex_lock(&if_head_mtx);
		for (ifp = if_head; ifp; ifp = ifp->next)
			(void) check_offers(ifp, &zeroip);
		(void) mutex_unlock(&if_head_mtx);
	}
	return (err);
}
