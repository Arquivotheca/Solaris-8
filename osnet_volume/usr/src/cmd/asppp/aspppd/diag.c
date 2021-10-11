#ident	"@(#)diag.c	1.3	94/02/22 SMI"

/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#include <fcntl.h>
#include <stropts.h>
#include <sys/ppp_diag.h>
#include <sys/ppp_ioctl.h>
#include <sys/strlog.h>

#include "aspppd.h"
#include "diag.h"
#include "fds.h"
#include "log.h"
#include "path.h"
#include "ipd_ioctl.h"

#define	TYPE(sid) ((sid) & 3)
#define	UNIT(sid) ((sid) >> 2)
#define	MAKEID(type, unit) ((unit) << 2 | (type))

static void	open_strlog(void);
static void	read_strlog(int);

static void
open_strlog(void)
{
	static int		strlog = -1;
	struct strioctl		ioc;
	struct trace_ids	tr_id;

	if (strlog > -1)
	    return;

	if ((strlog = open("/dev/log", O_RDONLY)) < 0) {
	    log(0, "open_strlog: /dev/log open failed\n");
	    return;
	}

	tr_id.ti_mid = PPP_DG_MOD_ID;
	tr_id.ti_sid = -1;
	tr_id.ti_level = -1;

	ioc.ic_cmd = I_TRCLOG;
	ioc.ic_timout = 5;
	ioc.ic_len = sizeof (struct trace_ids);
	ioc.ic_dp = (char *)&tr_id;

	if (ioctl(strlog, I_STR, &ioc) < 0)
	    log(0, "open_strlog: I_TRCLOG failed\n");
	else
	    add_to_fds(strlog, POLLIN, read_strlog);
}

static void
read_strlog(int index)
{
	struct strbuf ctl, data;
	struct log_ctl logctl;

	char buf[4000];
	int num, flags;

	ctl.buf = (char *)&logctl;
	ctl.maxlen = sizeof (logctl);
	data.buf = buf;
	data.maxlen = sizeof (buf);

	flags = 0;
	if (getmsg(fds[index].fd, &ctl, &data, &flags) < 0)
		fail("read_strlog: getmsg failed\n");

	if (ctl.len < sizeof (struct log_ctl))
	    log(42, "read_strlog: short message received\n");

	log(debug, "%.6lu %s%d %s\n", logctl.seq_no,
	    TYPE(logctl.sid) == IPD_MTP ? "ipd" : "ipdptp",
	    UNIT(logctl.sid), buf);
}

void
start_diag(struct path *p)
{
	struct strioctl cmio;
	ppp_diag_conf_t dconf;

	if (ioctl(p->s, I_PUSH, "ppp_diag") < 0) {
		log(0, "start_diag: ppp_diag not pushed\n");
		return;
	}

	memset((void *)&dconf, 0, sizeof (dconf));	/* avoid kernel bug */
	cmio.ic_cmd = PPP_DIAG_GET_CONF;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (dconf);
	cmio.ic_dp = (char *)&dconf;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("set_diag_conf: get configuration failed\n");

	dconf.media_type = pppAsync;
	dconf.debuglevel = ((debug == 8) ? PPP_DG_STAND : PPP_DG_ALL);
	dconf.ifid = MAKEID(p->inf.iftype, p->inf.ifunit);

	cmio.ic_cmd = PPP_DIAG_SET_CONF;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (dconf);
	cmio.ic_dp = (char *)&dconf;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("set_diag_conf: set configuration failed\n");

	open_strlog();
}
