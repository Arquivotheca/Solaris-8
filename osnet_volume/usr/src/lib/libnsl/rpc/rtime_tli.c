/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#ident	"@(#)rtime_tli.c	1.14	97/04/29 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)rtime_tli.c 1.7 89/04/18 Copyr 1989 Sun Micro";
#endif

/*
 * rtime_tli.c - get time from remote machine
 *
 * gets time, obtaining value from host
 * on the (udp, tcp)/time tli connection. Since timeserver returns
 * with time of day in seconds since Jan 1, 1900, must
 * subtract seconds before Jan 1, 1970 to get
 * what unix uses.
 */
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <errno.h>
#include <sys/poll.h>
#include <rpc/nettype.h>
#include <netdir.h>
#include <stdio.h>

extern int __rpc_timeval_to_msec();

#ifdef DEBUG
#define	debug(msg)	t_error(msg)
#else
#define	debug(msg)
#endif

#define	NYEARS	(1970 - 1900)
#define	TOFFSET ((u_int)60*60*24*(365*NYEARS + (NYEARS/4)))

/*
 * This is based upon the internet time server, but it contacts it by
 * using TLI instead of socket.
 */
rtime_tli(host, timep, timeout)
	char *host;
	struct timeval *timep;
	struct timeval *timeout;
{
	uint32_t thetime;
	int flag;
	struct nd_addrlist *nlist = NULL;
	struct nd_hostserv rpcbind_hs;
	struct netconfig *nconf = NULL;
	int foundit = 0;
	int fd = -1;

	trace1(TR_rtime_tli, 0);
	nconf = __rpc_getconfip(timeout == NULL ? "tcp" : "udp");
	if (nconf == (struct netconfig *)NULL)
		goto error;

	if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) == -1) {
		debug("open");
		goto error;
	}
	if (t_bind(fd, (struct t_bind *) NULL, (struct t_bind *) NULL) < 0) {
		debug("bind");
		goto error;
	}

	/* Get the address of the rpcbind */
	rpcbind_hs.h_host = host;
	rpcbind_hs.h_serv = "time";
	/* Basically get the address of the remote machine on IP */
	if (netdir_getbyname(nconf, &rpcbind_hs, &nlist))
		goto error;

	if (nconf->nc_semantics == NC_TPI_CLTS) {
		struct t_unitdata tu_data;
		struct pollfd pfd;
		int res;
		int msec;

		tu_data.addr = *nlist->n_addrs;
		tu_data.udata.buf = (char *)&thetime;
		tu_data.udata.len = (u_int) sizeof (thetime);
		tu_data.udata.maxlen = tu_data.udata.len;
		tu_data.opt.len = 0;
		tu_data.opt.maxlen = 0;
		if (t_sndudata(fd, &tu_data) == -1) {
			debug("udp");
			goto error;
		}
		pfd.fd = fd;
		pfd.events = POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND;

		msec = __rpc_timeval_to_msec(timeout);
		do {
			res = poll(&pfd, 1, msec);
		} while (res < 0);
		if ((res <= 0) || (pfd.revents & POLLNVAL))
			goto error;
		if (t_rcvudata(fd, &tu_data, &flag) < 0) {
			debug("udp");
			goto error;
		}
		foundit = 1;
	} else {
		struct t_call sndcall;

		sndcall.addr = *nlist->n_addrs;
		sndcall.opt.len = sndcall.opt.maxlen = 0;
		sndcall.udata.len = sndcall.udata.maxlen = 0;

		if (t_connect(fd, &sndcall, NULL) == -1) {
			debug("tcp");
			goto error;
		}
		if (t_rcv(fd, (char *)&thetime, (u_int) sizeof (thetime), &flag)
				!= (u_int) sizeof (thetime)) {
			debug("tcp");
			goto error;
		}
		foundit = 1;
	}

	thetime = ntohl(thetime);
	timep->tv_sec = thetime - TOFFSET;
	timep->tv_usec = 0;

error:
	if (nconf) {
		(void) freenetconfigent(nconf);
		if (fd != -1) {
			(void) t_close(fd);
			if (nlist)
				netdir_free((char *)nlist, ND_ADDRLIST);
		}
	}
	trace1(TR_rtime_tli, 1);
	return (foundit);
}
