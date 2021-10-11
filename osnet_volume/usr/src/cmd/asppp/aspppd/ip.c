#ident	"@(#)ip.c	1.2	95/02/25 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <fcntl.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/dlpi.h>
#include <sys/socket.h>

#include "ipd.h"
#include "ipd_ioctl.h"
#include "log.h"
#include "path.h"

static void	send_dlpi_attachreq(int, ulong);
static void	send_dlpi_setphyaddr(int, struct sockaddr *);
static void	expect(int, ulong);

void
start_ip(struct path *p)
{
	struct strioctl	cmio;
	ipd_set_tim_t	req;

	if (p->mux >= 0)
	    fail("start_ip: p->mux\n");

	/* open a stream to IP/dialup */
	p->mux = open(p->inf.iftype == IPD_PTP ? IPDPTP_PATH : IPD_PATH,
			O_RDWR);
	if (p->mux < 0)
	    fail("start_ip: open ipd multiplexor\n");

	p->state = ip;

	/* attach the stream to the required IP/dialup interface */
	send_dlpi_attachreq(p->mux, p->inf.ifunit);

	expect(p->mux, DL_OK_ACK);

	/* set the destination address for the stream */
	send_dlpi_setphyaddr(p->mux, &p->inf.sa);

	expect(p->mux, DL_OK_ACK);

	if (ioctl(p->mux, I_FLUSH,  FLUSHR) < 0)
	    fail("start_ip: I_FLUSH failed\n");

	/* link the lower stream under IP/dialup */
	p->mux_id = ioctl(p->mux, I_LINK,  p->s);

	if (p->mux_id < 0)
	    fail("start_ip: I_LINK failed\n");

	/* initialize timeout values for IP/dialup */

	req.msg		= IPD_SET_TIM;
	req.iftype	= p->inf.iftype;
	req.ifunit	= p->inf.ifunit;
	req.sa		= p->inf.sa;
	req.timeout	= p->timeout;

	cmio.ic_cmd	= IPD_SET_TIM;
	cmio.ic_timout	= 1;
	cmio.ic_len	= sizeof (ipd_set_tim_t);
	cmio.ic_dp	= (char *) &req;

	if (ioctl(ipdcm, I_STR, &cmio) < 0)
	    fail("start_ip: IPD_SET_TIM failed\n");

	log(1, "start_ip: IP up on interface %s%d, timeout set for %d "
	    "seconds\n",
	    p->inf.iftype == IPD_MTP ? "ipd" : "ipdptp", p->inf.ifunit,
	    p->timeout);
}

/* remove a stream from IP/dialup */

void
stop_ip(struct path *p)
{
	if (p->mux < 0)
	    return;

	/* unlink the lower stream from IP/dialup */
	if (ioctl(p->mux, I_UNLINK,  p->mux_id) < 0) {
		fail("stop_ip: I_UNLINK failed\n");
	}

	if (close(p->mux) < 0)
	    fail("stop_ip: close failed\n");
	p->mux_id = p->mux = -1;

	p->state = ppp;
}

static void
send_dlpi_attachreq(int ipd_str, ulong ppa)
{
	struct strbuf		ctl;
	dl_attach_req_t	attach;

	log(42, "send_dlpi_attachreq: attaching...on fd %d \n", ipd_str);

	ctl.len = DL_ATTACH_REQ_SIZE;
	ctl.buf = (char *) &attach;

	attach.dl_primitive = DL_ATTACH_REQ;
	attach.dl_ppa = ppa;

	if (putmsg(ipd_str, &ctl, NULL, 0) < 0)
	    fail("send_dlpi_attachreq: putmsg failed\n");
}

static void
send_dlpi_setphyaddr(int ipd_str, struct sockaddr *addr)
{
	char			buf[DL_SET_PHYS_ADDR_REQ_SIZE+sizeof (*addr)];
	dl_set_phys_addr_req_t	*setaddr;
	struct strbuf		ctl;

	log(42, "send_dlpi_setphyaddr: setting physical addr...on fd %d \n",
	    ipd_str);

	setaddr = (dl_set_phys_addr_req_t *) buf;

	ctl.len = DL_SET_PHYS_ADDR_REQ_SIZE+sizeof (*addr);
	ctl.buf = (char *) setaddr;

	setaddr->dl_primitive = DL_SET_PHYS_ADDR_REQ;
	setaddr->dl_addr_length = sizeof (*addr);
	setaddr->dl_addr_offset = DL_SET_PHYS_ADDR_REQ_SIZE;
	memcpy(buf+DL_SET_PHYS_ADDR_REQ_SIZE, addr, sizeof (*addr));

	if (putmsg(ipd_str, &ctl, NULL, 0) < 0)
	    fail("send_dlpi_setphyaddr: putmsg failed\n");
}

static void
expect(int ipd_str, ulong prim)
{
	char			buf[200]; /* > sizeof biggest prim */
	struct strbuf		ctl;
	union DL_primitives	*dlp = (union DL_primitives *) buf;
	int			flags;

	ctl.maxlen = sizeof (buf);
	ctl.buf = buf;
	flags = 0;
	if (getmsg(ipd_str, &ctl, NULL, &flags) < 0)
	    fail("expect: getmsg failed\n");

	if (dlp->dl_primitive != prim)
	    fail("expect: bad incoming prim id\n");
}
