#ident	"@(#)ppp.c	1.18	97/02/24 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stropts.h>
#include <termios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ppp_diag.h>

#include "aspppd.h"
#include "auth.h"
#include "diag.h"
#include "fds.h"
#include "ifconfig.h"
#include "iflist.h"
#include "ip.h"
#include "log.h"
#include "path.h"
#include "ppp.h"
#include "ppp_ioctl.h"
#include "route.h"

#define	MAKEID(type, unit) ((unit) << 2 | (type))

static char *disc_reasons[] = {
	"Maximum number of configure requests exceeded",
	"Negotiation of mandatory options failed",
	"Authentication Failed",
	"Protocol closed",
	"Local Authentication Failed",
	"Remote Authentication Failed",
	"Loop back detected"
};

static void	set_conf_options(struct path *);
static void	set_if_values(struct path *);

/* start a PPP session on the specified device */

void
start_ppp(int index)
{
	int		n;
	struct path	*p;
	struct termios	tios;

	log(42, "start_ppp: PPP starting\n");

	if ((p = get_path_by_fd(fds[index].fd)) == NULL)
	    fail("start_ppp: can't find path\n");

	/* Set terminal parameters */

	if (tcgetattr(p->s, &tios) < 0)
	    fail("start_ppp: tcgetattr failed\n");
	tios.c_iflag = (IGNBRK | IGNPAR);
	tios.c_oflag = 0;
	tios.c_cflag &= ~PARENB;
	tios.c_cflag |= (CS8 | CRTSCTS | CRTSXOFF);
	tios.c_lflag = 0;
	if (tcsetattr(p->s, TCSAFLUSH, &tios) < 0)
	    fail("start_ppp: tcsetattr failed\n");

	/* Pop off ttcompat, ldterm, and anything else */

	if ((n = ioctl(p->s, I_LIST, NULL)) < 0)
	    fail("start_ppp: I_LIST failed\n");

	while (--n)
	    if (ioctl(p->s, I_POP, 0) < 0)
		fail("start_ppp: I_POP failed\n");

	/* Push on the PPP modules */

	if (debug > 7)		/* uucp debug goes up to 7 */
	    start_diag(p);

	if (ioctl(p->s, I_PUSH, "ppp") < 0)
	    fail("start_ppp: ppp not pushed\n");

	/* bug 1262630 */
	conn_cntr--;

	/* Set PPP options */

	set_conf_options(p);

	/* Authentication */

	set_authentication(p);

	/* Open NCP layer first -- "trickle down" method */

	send_ppp_event(p->s, PPP_UP, pppDEVICE);
	send_ppp_event(p->s, PPP_OPEN, pppIP_NCP);
	p->state = ppp;
	fds[index].events = (POLLIN | POLLHUP);
	change_callback(index, process_ppp_msg);
}

void
process_ppp_msg(int index)
{
	char			buf[sizeof (union PPPmessages)];
	struct strbuf		ctl;
	struct strbuf		data;
	char			dbuf[256];
	int			flags;
	union PPPmessages	*msg;
	struct path		*p;

	if ((p = get_path_by_fd(fds[index].fd)) == NULL)
	    fail("process_ppp_msg: can't find path\n");

	if ((fds[index].revents & POLLHUP) == POLLHUP) {
		log(42, "process_ppp_msg: hangup detected\n");
		terminate_path(p);
		return;
	}

	data.buf = dbuf;
	data.maxlen = sizeof (dbuf);
	ctl.buf = buf;
	ctl.maxlen = sizeof (buf);
	msg = (union PPPmessages *)ctl.buf;
	flags = 0;
	if (getmsg(p->s, &ctl, &data, &flags) < 0)
	    fail("process_ppp_msg: getmsg\n");

	if (debug > 9 && data.len > 0) {	/* hopefully temporary */
		int n;
		char pbuf[80];
		n = sprintf(pbuf, "ctl.len=%d data.len=%d", ctl.len, data.len);
		(void) sprintf(&pbuf[n], " data.buf=%x%x%x%x\0", dbuf[0],
		    dbuf[1], dbuf[2], dbuf[3]);
		log(42, "%s\n", pbuf);
		if (ctl.len < 1)
		    return;
	}

	if (ctl.len < sizeof (msg->ppp_message))
	    fail("process_ppp_msg: ctl.len < sizeof msg->ppp_message\n");

	switch (msg->ppp_message) {

	case PPP_TL_START:
		switch (msg->proto_start.protocol) {
		case pppLCP:
			log(42, "process_ppp_msg: LCP starting\n");
			break;
		case pppIP_NCP:
			log(42, "process_ppp_msg: IP_NCP starting\n");
			break;
		default:
			log(0, "process_ppp_msg: TL_START from unknown "
			    "protocol %d\n", msg->proto_start.protocol);
			break;
		}
		break;

	case PPP_TL_UP:
		switch (msg->proto_up.protocol) {
		case pppLCP:
			log(42, "process_ppp_msg: LCP up\n");
			break;
		case pppIP_NCP:	/* PPP is now up */
			log(42, "process_ppp_msg: IP_NCP up\n");
			/* No more messages will be sent by PPP */
			delete_from_fds(p->s);
			start_ip(p);
			if (p->default_route)
			    add_default_route(p);
			break;
		case pppDEVICE:
			log(42, "process_ppp_msg: DEVICE up\n");
			break;
		default:
			log(0, "process_ppp_msg: TL_UP from unknown "
			    "protocol %d\n", msg->proto_up.protocol);
			break;
		}
		break;

	case PPP_TL_DOWN:
		switch (msg->proto_down.protocol) {
		case pppLCP:
			log(42, "process_ppp_msg: LCP down\n");
			break;
		case pppIP_NCP:
			log(42, "process_ppp_msg: IP_NCP down\n");
			break;
		case pppDEVICE:
			log(42, "process_ppp_msg: DEVICE down\n");
			terminate_path(p);
			break;
		default:
			log(0, "process_ppp_msg: TL_DOWN from unknown "
			    "protocol %d\n", msg->proto_down.protocol);
			break;
		}
		break;

	case PPP_TL_FINISH:
		switch (msg->proto_finish.protocol) {
		case pppLCP:
			log(42, "process_ppp_msg: LCP finished\n");
			terminate_path(p);
			break;
		case pppIP_NCP:
			log(42, "process_ppp_msg: IP_NCP finished\n");
			break;
		default:
			log(0, "process_ppp_msg: TL_FINISH from unknown "
			    "protocol %d\n", msg->proto_finish.protocol);
			break;
		}
		break;

	case PPP_NEED_VALIDATION:
		log(42, "process_ppp_msg: PPP_NEED_VALIDATION\n");
		break;

	case PPP_CONFIG_CHANGED:
		/*
		 * PPP has advised us that the configuration (e.g. mru)
		 * has altered
		 */
		log(42, "process_ppp_msg: PPP_CONFIG_CHANGED\n");
		set_if_values(p);
		break;

	case PPP_ERROR_IND:
		/* PPP reports an error */
		log(0, "process_ppp_msg: PPP_ERROR_IND %s\n",
		    disc_reasons[msg->error_ind.code]);
		terminate_path(p);
		break;

	default:
		/* shouldn't get here */

		/*
		 * This messages bothers users so much I think we should
		 * ignore it until we can figure out what to do about it.
		 * Note: it used to be log level 0.
		 */
		log(42, "process_ppp_msg: unknown msg=%d\n", msg->ppp_message);
		break;
	}
}

void
terminate_path(struct path *p)
{
	char	ifname[80];

	while (p->state != inactive) {
		switch (p->state) {
		case dialing:
			if (p->pid != -1) {
				if (kill(p->pid, SIGTERM) == -1) {
					if (errno != ESRCH)
					    fail("terminate_path: kill "
						"failed\n");
				}
				p->pid = -1;
			}
			if (p->s > -1) {
				delete_from_fds(p->s);
				if (close(p->s) < 0)
				    fail("terminate_path: close failed"
					"in state %d\n", p->state);
				p->s = -1;
			}
			p->state = inactive;
			break;
		case connected:
			if (p->cns_id > -1) {
				delete_from_fds(p->cns_id);
				if (close(p->cns_id) < 0)
				    fail("terminate_path: close failed"
					"in state %d\n", p->state);
				p->cns_id = -1;
			}
			if (p->inf.wild_card) {
				/* bug 4014892 */
				sprintf(ifname, "%s%d", "ipdptp",
				    p->inf.ifunit);
				mark_interface_free(ifname);

				mark_if_down(p);
				p->inf.ifunit = (u_int) -1;
			}
			p->state = dialing;
			break;
		case ppp:
			if (p->s > -1) {
				delete_from_fds(p->s);
				if (close(p->s) < 0)
				    fail("terminate_path: close failed"
					"in state %d\n", p->state);
				p->s = -1;
			}
			p->state = connected;
			break;
		case ip:
			if (p->default_route)
			    delete_default_route(p);
			stop_ip(p);	/* Yuck.  This is all fouled up! */
			/* I'm not sure what it does */
			send_ppp_event(p->s, PPP_CLOSE, pppLCP);
			add_to_fds(p->s, POLLIN, process_ppp_msg);
			break;
		default:
			fail("terminate_path: unexpected "
			    "state=%d\n", p->state);
			break;
		}
	}
}

static void
set_conf_options(struct path *p)
{
	struct strioctl	cmio;
	pppLinkControlEntry_t	msg;

	memset((void *)&msg, 0, sizeof (msg));

	cmio.ic_cmd = PPP_GET_CONF;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (msg);
	cmio.ic_dp = (char *)&msg;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("set_conf_options: get configuration failed\n");

	msg.pppLinkMediaType = pppAsync;
	msg.pppLinkLocalMRU = p->lcp.mru;
	msg.pppLinkAllowHdrComp = p->ipcp.compression == vj ? 1 : 0;
	msg.pppLinkAllowPAComp = p->lcp.compression == on ? 1 : 0;
	msg.pppLinkLocalACCMap = p->ipcp.async_map;
	msg.pppIPLocalAddr = get_if_addr(p);
	if (p->inf.iftype == IPD_PTP) {
		if (p->inf.get_ipaddr)
		    msg.pppIPLocalAddr = 0;
		msg.pppIPRemoteAddr = get_if_dst_addr(p);
	} else
	    msg.pppIPRemoteAddr =
		((struct sockaddr_in *)&(p->inf.sa))->sin_addr.s_addr;

	cmio.ic_cmd = PPP_SET_CONF;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (msg);
	cmio.ic_dp = (char *)&msg;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("set_conf_options: set configuration failed\n");
}

static void
set_if_values(struct path * p)
{
	struct strioctl	cmio;
	pppLinkControlEntry_t	msg;
	int			ifp;
	char			ifname[80];
	struct in_addr		sin;
	ppp_diag_conf_t		dconf;

	memset((void *)&msg, 0, sizeof (msg));

	cmio.ic_cmd = PPP_GET_CONF;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (msg);
	cmio.ic_dp = (char *)&msg;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("set_if_values: get configuration failed\n");

	if ((msg.pppIPLocalAddr != 0) & (msg.pppIPLocalAddr != get_if_addr(p)))
	    set_if_addr(p, msg.pppIPLocalAddr);

	if (p->inf.iftype == IPD_PTP &&
	    msg.pppIPRemoteAddr != get_if_dst_addr(p)) {
		/* bug 4014892 */
		if (p->inf.wild_card) {
			sin.s_addr = msg.pppIPRemoteAddr;
			ifp = find_interface(msg.pppIPRemoteAddr);
			if (ifp < 0) {
				log(1, "The IP address wanted by the peer (%s) "
					"doesn't correspond to any unused "
					"dynamic ptp interface. "
					"Disconnecting.\n", inet_ntoa(sin));
			/*
			 * pb if Solaris PPP at the other end: It may be
			 * processing NCP-up while the link is brought
			 * down. The msg LCP down is then in the stream
			 * head p->s and is never read, causing the interf.
			 * to stay linked under ipdcm.
			 * I just wait until start_ip() is completed before
			 * breaking the link. This is a kludge, but would
			 * require modifying the kernel to really fix it.
			 */
				sleep(3);
				send_ppp_event(p->s, PPP_CLOSE, pppIP_NCP);
			/*
			 * must break the conn manually. NCP up msg is already
			 * in stream head so start_ip() is called. In the
			 * meantime LCP down arrives but is never read so the
			 * interface is never unlinked. Same pb as above, but
			 * on our side this time.
			 */
				terminate_path(p);
				return;
			}

			log(1, "The IP address wanted by the peer (%s) "
				"corresponds to ipdptp%d.\n",
				inet_ntoa(sin), ifp);
			log(1, "Changing interface from ipdptp%d to "
				"ipdptp%d.\n", p->inf.ifunit, ifp);
			mark_if_down(p);
			sprintf(ifname, "%s%d", "ipdptp", p->inf.ifunit);
			mark_interface_free(ifname);

			p->inf.ifunit = (u_int)ifp;

			mark_if_up(p);
			sprintf(ifname, "%s%d", "ipdptp", p->inf.ifunit);
			mark_interface_used(ifname);

			/*
			 * bug 4027529
			 * now reconfigure ppp_diag to display the right
			 * interface.
			 */
			/* avoid kernel bug */
			memset((void *)&dconf, 0, sizeof (dconf));

			cmio.ic_cmd = PPP_DIAG_GET_CONF;
			cmio.ic_timout = 1;
			cmio.ic_len = sizeof (dconf);
			cmio.ic_dp = (char *)&dconf;

			if (ioctl(p->s, I_STR, &cmio) < 0)
			    log(9, "set_if_values: get configuration failed\n");

			dconf.ifid = MAKEID(p->inf.iftype, p->inf.ifunit);

			cmio.ic_cmd = PPP_DIAG_SET_CONF;
			cmio.ic_timout = 1;
			cmio.ic_len = sizeof (dconf);
			cmio.ic_dp = (char *)&dconf;

			if (ioctl(p->s, I_STR, &cmio) < 0)
			    log(9, "set_if_values: set configuration failed\n");

		}
		/* end bug 4014892 */

		set_if_dst_addr(p, msg.pppIPRemoteAddr);
	}

	if (msg.pppLinkRemoteMRU != get_if_mtu(p))
	    set_if_mtu(p, msg.pppLinkRemoteMRU);
}

void
send_ppp_event(int ppp_fd, ppp_ioctl_t event, pppProtocol_t protocol)
{
	struct strioctl 	cmio;
	pppExEvent_t		event_data;

	if (ppp_fd < 0)
	    return;

	event_data.protocol = protocol;
	cmio.ic_cmd = event;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (event_data);
	cmio.ic_dp = (char *)&event_data;

	if (ioctl(ppp_fd, I_STR, &cmio) < 0)
	    fail("send_ppp_event: Event (place event here) failed\n");
}
