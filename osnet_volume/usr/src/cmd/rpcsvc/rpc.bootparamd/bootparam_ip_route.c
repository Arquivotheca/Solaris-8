/*
 * Copyright 1991 Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootparam_ip_route.c 1.10	98/05/24 SMI"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/timod.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <net/if.h>

#include <inet/common.h>
#include <inet/mib2.h>
#include <inet/ip.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>

#include <netdb.h>
#include <fcntl.h>

extern char *inet_ntoa();


typedef struct mib_item_s {
	struct mib_item_s	* next_item;
	long			group;
	long			mib_id;
	long			length;
	char			* valp;
} mib_item_t;

#ifdef	USE_STDARG
extern	void	fatal(char * fmt, ...);
#endif

extern	int	debug;

static mib_item_t *
mibget (sd)
	int		sd;
{
	char			buf[512];
	int			flags;
	int			i, j, getcode;
	struct strbuf		ctlbuf, databuf;
	struct T_optmgmt_req	* tor = (struct T_optmgmt_req *)buf;
	struct T_optmgmt_ack	* toa = (struct T_optmgmt_ack *)buf;
	struct T_error_ack	* tea = (struct T_error_ack *)buf;
	struct opthdr		* req;
	mib_item_t		* first_item = nilp(mib_item_t);
	mib_item_t		* last_item  = nilp(mib_item_t);
	mib_item_t		* temp;

	tor->PRIM_type = T_SVR4_OPTMGMT_REQ;
	tor->OPT_offset = sizeof (struct T_optmgmt_req);
	tor->OPT_length = sizeof (struct opthdr);
	tor->MGMT_flags = T_CURRENT;
	req = (struct opthdr *)&tor[1];
	req->level = MIB2_IP;		/* any MIB2_xxx value ok here */
	req->name  = 0;
	req->len   = 0;

	ctlbuf.buf = buf;
	ctlbuf.len = tor->OPT_length + tor->OPT_offset;
	flags = 0;
	if (putmsg(sd, &ctlbuf, nilp(struct strbuf), flags) == -1) {
		perror("mibget: putmsg(ctl) failed");
		goto error_exit;
	}
	/*
	 * each reply consists of a ctl part for one fixed structure
	 * or table, as defined in mib2.h.  The format is a T_OPTMGMT_ACK,
	 * containing an opthdr structure.  level/name identify the entry,
	 * len is the size of the data part of the message.
	 */
	req = (struct opthdr *)&toa[1];
	ctlbuf.maxlen = sizeof (buf);
	for (j = 1; 1; j++) {
		flags = 0;
		getcode = getmsg(sd, &ctlbuf, nilp(struct strbuf), &flags);
		if (getcode == -1) {
			perror("mibget getmsg(ctl) failed");
			if (debug) {
				msgout("#   level   name    len");
				i = 0;
				for (last_item = first_item; last_item;
					last_item = last_item->next_item)
					msgout("%d  %4d   %5d   %d", ++i,
						last_item->group,
						last_item->mib_id,
						last_item->length);
			}
			goto error_exit;
		}
		if ((getcode == 0) &&
		    (ctlbuf.len >= sizeof (struct T_optmgmt_ack))&&
		    (toa->PRIM_type == T_OPTMGMT_ACK) &&
		    (toa->MGMT_flags == T_SUCCESS) &&
		    (req->len == 0)) {
			if (debug)
				msgout("mibget getmsg() %d returned EOD (level %d, name %d)",
					j, req->level, req->name);
			return (first_item);		/* this is EOD msg */
		}

		if (ctlbuf.len >= sizeof (struct T_error_ack) &&
		    tea->PRIM_type == T_ERROR_ACK) {
			msgout("mibget %d gives T_ERROR_ACK: TLI_error = 0x%x, UNIX_error = 0x%x",
				j, getcode, tea->TLI_error, tea->UNIX_error);
			errno = (tea->TLI_error == TSYSERR)
				? tea->UNIX_error : EPROTO;
			goto error_exit;
		}

		if (getcode != MOREDATA ||
		    ctlbuf.len < sizeof (struct T_optmgmt_ack) ||
		    toa->PRIM_type != T_OPTMGMT_ACK ||
		    toa->MGMT_flags != T_SUCCESS) {
			msgout("mibget getmsg(ctl) %d returned %d, ctlbuf.len = %d, PRIM_type = %d",
				j, getcode, ctlbuf.len, toa->PRIM_type);
			if (toa->PRIM_type == T_OPTMGMT_ACK)
				msgout("T_OPTMGMT_ACK: MGMT_flags = 0x%x, req->len = %d",
					toa->MGMT_flags, req->len);
			errno = ENOMSG;
			goto error_exit;
		}

		temp = (mib_item_t *)malloc(sizeof (mib_item_t));
		if (!temp) {
			perror("mibget malloc failed");
			goto error_exit;
		}
		if (last_item)
			last_item->next_item = temp;
		else
			first_item = temp;
		last_item = temp;
		last_item->next_item = nilp(mib_item_t);
		last_item->group = req->level;
		last_item->mib_id = req->name;
		last_item->length = req->len;
		last_item->valp = (char *)malloc(req->len);
		if (debug)
			msgout(
			"msg %d:  group = %4d   mib_id = %5d   length = %d",
				j, last_item->group, last_item->mib_id,
				last_item->length);

		databuf.maxlen = last_item->length;
		databuf.buf    = last_item->valp;
		databuf.len    = 0;
		flags = 0;
		getcode = getmsg(sd, nilp(struct strbuf), &databuf, &flags);
		if (getcode == -1) {
			perror("mibget getmsg(data) failed");
			goto error_exit;
		} else if (getcode != 0) {
			msgout("xmibget getmsg(data) returned %d, databuf.maxlen = %d, databuf.len = %d",
				getcode, databuf.maxlen, databuf.len);
			goto error_exit;
		}
	}

error_exit:;
	while (first_item) {
		last_item = first_item;
		first_item = first_item->next_item;
		if (last_item->valp)
			free(last_item->valp);
		free(last_item);
	}
	return (first_item);
}

free_itemlist(item_list)
	mib_item_t	*item_list;
{
	mib_item_t	*item;

	while (item_list) {
		item = item_list;
		item_list = item->next_item;
		if (item->valp)
			free(item->valp);
		free(item);
	}
}

/*
 * If we are a router, return address of interface closest to client.
 * If we are not a router, look through our routing table and return
 * address of "best" router that is on same net as client.
 *
 * We expect the router flag to show up first, followed by interface
 * addr group, followed by the routing table.
 */

u_long
get_ip_route(client_addr)
	struct in_addr client_addr;
{
	mib_item_t	* item_list;
	mib_item_t	* item;
	int		sd;
	mib2_ip_t		*mip;
	mib2_ipAddrEntry_t	*map;
	mib2_ipRouteEntry_t	* rp;
	int			ip_forwarding = 2;	/* off */
	/* mask of interface used to route to client and best_router */
	struct in_addr		interface_mask;
	/* address of interface used to route to client and best_router */
	struct in_addr		interface_addr;
	/* address of "best router"; i.e. the answer */
	struct in_addr		best_router;
	u_long addr;
	u_long mask;

	interface_mask.s_addr = 0L;
	interface_addr.s_addr = 0L;
	best_router.s_addr = 0L;

	/* open a stream to IP */
	sd = open("/dev/ip", 2);
	if (sd == -1) {
		perror("ip open");
		close(sd);
		msgout("can't open mib stream");
		return ((u_long) 0);
	}

	/* send down a request and suck up all the mib info from IP */
	if ((item_list = mibget(sd)) == nilp(mib_item_t)) {
		msgout("mibget() failed");
		close(sd);
		return ((u_long) 0);
	}

	/*
	 * We make three passes through the list of collected IP mib
	 * information.  First we figure out if we are a router.  Next,
	 * we find which of our interfaces is on the same subnet as
	 * the client.  Third, we paw through our own routing table
	 * looking for a useful router address.
	 */

	/*
	 * The general IP group.
	 */
	for (item = item_list; item; item = item->next_item) {
		if ((item->group == MIB2_IP) && (item->mib_id == 0)) {
			/* are we an IP router? */
			mip = (mib2_ip_t *) item->valp;
			ip_forwarding = mip->ipForwarding;
			break;
		}
	}

	/*
	 * The interface group.
	 */
	for (item = item_list; item; item = item->next_item) {
		if ((item->group == MIB2_IP) && (item->mib_id == MIB2_IP_20)) {
			/*
			 * Try to find out which interface is on
			 * the same subnet as the client. Save its address
			 * and netmask.
			 */
			map = (mib2_ipAddrEntry_t *) item->valp;
			while ((char *)map < item->valp + item->length) {
				addr = map->ipAdEntAddr;
				mask =  map->ipAdEntNetMask;
				if ((addr & mask) ==
				    (client_addr.s_addr & mask)) {
					interface_addr.s_addr = addr;
					interface_mask.s_addr = mask;
				}
				map++;
			}
		}
	}

	/*
	 * If this exercise found no interface on the same subnet as
	 * the client, then we can't suggest any router address to
	 * use.
	 */
	if (interface_addr.s_addr == (u_long) 0) {
		if (debug)
			msgout("get_ip_route: no interface on same net as client");
		close(sd);
		free_itemlist(item_list);
		return ((u_long) 0);
	}

	/*
	 * If we are a router, we return to client the address of our
	 * interface on the same net as the client.
	 */
	if (ip_forwarding == 1) {
		if (debug)
			msgout("get_ip_route: returning local addr %s",
				inet_ntoa(interface_addr));
		close(sd);
		free_itemlist(item_list);
		return (interface_addr.s_addr);
	}

	if (debug) {
		msgout("interface_addr = %s.", inet_ntoa(interface_addr));
		msgout("interface_mask = %s", inet_ntoa(interface_mask));
	}


	/*
	 * The routing table group.
	 */
	for (item = item_list; item; item = item->next_item) {
		if ((item->group == MIB2_IP) && (item->mib_id == MIB2_IP_21)) {
			if (debug)
				msgout("%d records for ipRouteEntryTable",
					item->length/
					sizeof (mib2_ipRouteEntry_t));

			for (rp = (mib2_ipRouteEntry_t *)item->valp;
				(char *) rp < item->valp + item->length;
				rp++) {
				if (debug >= 2)
					msgout("ire_type = %d, next_hop = 0x%x",
						rp->ipRouteInfo.re_ire_type,
						rp->ipRouteNextHop);

				/*
				 * We are only interested in real
				 * gateway routes.
				 */
				if ((rp->ipRouteInfo.re_ire_type !=
				    IRE_DEFAULT) &&
				    (rp->ipRouteInfo.re_ire_type !=
				    IRE_PREFIX) &&
				    (rp->ipRouteInfo.re_ire_type !=
				    IRE_HOST) &&
				    (rp->ipRouteInfo.re_ire_type !=
				    IRE_HOST_REDIRECT))
					continue;

				/*
				 * We are only interested in routes with
				 * a next hop on the same subnet as
				 * the client.
				 */
				if ((rp->ipRouteNextHop &
					interface_mask.s_addr) !=
				    (interface_addr.s_addr &
					interface_mask.s_addr))
					continue;

				/*
				 * We have a valid route.  Give preference
				 * to default routes.
				 */
				if ((rp->ipRouteDest == (long) 0) ||
				    (best_router.s_addr == (long) 0))
					best_router.s_addr =
						rp->ipRouteNextHop;
			}
		}
	}

	if (debug && (best_router.s_addr == 0))
		msgout("get_ip_route: no route found for client");

	close(sd);
	free_itemlist(item_list);
	return (best_router.s_addr);
}
