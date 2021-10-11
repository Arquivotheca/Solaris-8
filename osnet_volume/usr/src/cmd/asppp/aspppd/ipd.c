#ident	"@(#)ipd.c	1.4	98/08/08 SMI"

/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stropts.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "call.h"
#include "fds.h"
#include "ip.h"
#include "ipd.h"
#include "ipd_ioctl.h"
#include "log.h"
#include "path.h"
#include "ppp.h"
#include "ppp_ioctl.h"
#include "route.h"

#define	CLOSE		B_TRUE
#define	NO_CLOSE	B_FALSE

int			ipdcm;

static boolean_t	already_connecting(ipd_con_dis_t);
static void		disconnect(ipd_con_dis_t, boolean_t);

/* wait for messages from IP/dialup */

void
process_ipd_msg(int index)
{
	char			buf[256];
	struct strbuf		ctl;
	int			flags;
	char			*ifname;
	enum ipd_iftype		iftype;
	int			ifunit;
	union ipd_messages	*ipdialup;
/* NPCTE fix for bugid 4089825, esc 512718. sgypsy@eng 21 April 98 */
	struct path		*p;
/* End CTE fix */

	ctl.buf = buf;
	ctl.maxlen = sizeof (buf);
	ipdialup = (union ipd_messages *)ctl.buf;
	flags = 0;
	if (getmsg(fds[index].fd, &ctl, NULL, &flags) < 0)
	    fail("process_ipd_msg: getmsg\n");

	if (ctl.len < sizeof (ipdialup->msg))
	    log(42, "process_ipd_msg: short message\n");

	iftype = ipdialup->con_dis.iftype;
	ifunit = ipdialup->con_dis.ifunit;
	ifname = iftype == IPD_MTP ? "ipd" : "ipdptp";

	switch (ipdialup->msg) {
	case IPD_CON_REQ:
		/* IP/dialup needs a connection */
		if (already_connecting(ipdialup->con_dis)) {
			log(42, "process_ipd_msg: still connecting...\n");
			return;
		}

		if (iftype == IPD_MTP)
		    log(1, "process_ipd_msg: %s%d needs connection for "
			"address %s\n", ifname, ifunit,
			inet_ntoa(((struct sockaddr_in *)&ipdialup->con_dis.sa)
			    ->sin_addr));
		else
		    log(1, "process_ipd_msg: %s%d needs connection\n",
			ifname, ifunit);

		place_call(ipdialup->con_dis);
		break;

	case IPD_DIS_REQ:
		/* IP/dialup reports a connection has timed out */
		log(1, "process_ipd_msg: interface %s%d has timed "
		    "out\n", ifname, ifunit);
		disconnect(ipdialup->con_dis, CLOSE);
		break;

	case IPD_DIS_IND:
		/* IP/dialup reports a connection has closed */
		log(1, "process_ipd_msg: interface %s%d has disconnected\n",
		    ifname, ifunit);
		disconnect(ipdialup->con_dis, NO_CLOSE);
/* NPCTE fix for bugid 4089825, esc 512718. sgypsy@eng 21 April 98 */
/*
 * this fix seems like a hack but fixes the problem as confirmed by
 * the field. Further investigation is needed
 */
		p = get_path_by_addr(ipdialup->con_dis);
		terminate_path(p);
/* End CTE fix */
		break;

	case IPD_ERR_IND:
		/* IP/dialup reports a connection has an error */
		log(1, "process_ipd_msg: interface %s%d reports an error\n",
		    ifname, ifunit);
		disconnect(ipdialup->con_dis, CLOSE);
		break;

	default:
		/* shouldn't get here */
		disconnect(ipdialup->con_dis, NO_CLOSE);
		fail("process_ipd_msg: unknown msg=%d\n", ipdialup->msg);
		break;
	}

}

/* check to see if we are in process of connecting */

static boolean_t
already_connecting(ipd_con_dis_t dst)
{
	struct path *p;

	p = get_path_by_addr(dst);

	if (p && p->state != inactive)
	    return (B_TRUE);
	else
	    return (B_FALSE);
}

/* disconnect from destination */

static void
disconnect(ipd_con_dis_t dst, boolean_t close_ppp)
{
	struct path	*p;

	/* find the path to be disconnected in the list of active paths */

	if ((p = get_path_by_addr(dst)) == NULL)
	    fail("disconnect: can't find path\n");

	if (p->default_route)
	    delete_default_route(p);

	stop_ip(p);

	if (close_ppp)
	    send_ppp_event(p->s, PPP_CLOSE, pppIP_NCP);

	add_to_fds(p->s, POLLIN, process_ppp_msg);

	log(1, "disconnect: disconnected connection from  %s%d\n",
	    dst.iftype == IPD_MTP ? "ipd" : "ipdptp", dst.ifunit);
}
