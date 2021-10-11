/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)yp_rsvd.c	1.6	99/04/27 SMI"

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <netconfig.h>
#include <netdir.h>
#include <rpc/rpc.h>

static CLIENT *
__yp_clnt_create_rsvdport_netid_req(const char *hostname, rpcprog_t prog,
			rpcvers_t vers, const char *nettype,
			const uint_t sendsz, const uint_t recvsz)
{
	struct netconfig *nconf;
	struct netbuf *svcaddr;
	struct t_bind *tbind;
	CLIENT *clnt = NULL;
	int fd;
	const char *nt;

	if (nettype == NULL)
		return (0);
	else
		nt = nettype;

	if (strcmp(nt, "udp") && strcmp(nt, "tcp") &&
		strcmp(nt, "udp6") && strcmp(nt, "tcp6"))
		return (clnt_create(hostname, prog, vers, nt));

	if ((nconf = getnetconfigent((void *) nt)) == NULL)
		return (NULL);

	if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) == -1) {
		freenetconfigent(nconf);
		return (NULL);
	}

	/* Attempt to set reserved port, but we don't care if it fails */
	netdir_options(nconf, ND_SET_RESERVEDPORT, fd, NULL);

	if ((tbind = (struct t_bind *) t_alloc(fd, T_BIND, T_ADDR)) == NULL) {
		freenetconfigent(nconf);
		return (NULL);
	}

	svcaddr = &(tbind->addr);

	if (!rpcb_getaddr(prog, vers, nconf, svcaddr, hostname)) {
		t_close(fd);
		t_free((char *) tbind, T_BIND);
		freenetconfigent(nconf);
		return (NULL);
	}

	if ((clnt = clnt_tli_create(fd, nconf, svcaddr,
				prog, vers, sendsz, recvsz)) == NULL) {
		t_close(fd);
		t_free((char *) tbind, T_BIND);
	} else {
		t_free((char *) tbind, T_BIND);
		clnt_control(clnt, CLSET_FD_CLOSE, NULL);
	}
	freenetconfigent(nconf);
	return (clnt);
}


CLIENT *
__yp_clnt_create_rsvdport(const char *hostname, rpcprog_t prog,
			rpcvers_t vers, const char *nettype,
			const uint_t sendsz, const uint_t recvsz)
{
	if (nettype == 0) {
		CLIENT *ret;
		ret = __yp_clnt_create_rsvdport_netid_req(hostname, prog,
					vers, "udp6", sendsz, recvsz);
		if (ret == 0)
			ret = __yp_clnt_create_rsvdport_netid_req(hostname,
					prog, vers, "udp", sendsz, recvsz);
		return (ret);
	} else {
		return (__yp_clnt_create_rsvdport_netid_req(hostname, prog,
							vers, nettype,
							sendsz, recvsz));
	}
}
