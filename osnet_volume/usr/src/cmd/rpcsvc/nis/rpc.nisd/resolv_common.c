/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)resolv_common.c	1.6	99/04/27 SMI"

/*
 * Routines used by rpc.nisd and by rpc.nisd_resolv
 */

#include <rpc/rpc.h>
#include <netdb.h>
#include <rpcsvc/yp_prot.h>
#include <errno.h>
#include <sys/types.h>
#include "resolv_common.h"

bool_t
xdr_ypfwdreq_key4(XDR *xdrs, struct ypfwdreq_key4 *ps)
{
	return (xdr_ypmap_wrap_string(xdrs, &ps->map) &&
		xdr_datum(xdrs, &ps->keydat) &&
		xdr_u_long(xdrs, &ps->xid) &&
		xdr_u_long(xdrs, &ps->ip) &&
		xdr_u_short(xdrs, &ps->port));
}


bool_t
xdr_ypfwdreq_key6(XDR *xdrs, struct ypfwdreq_key6 *ps)
{
	u_int	addrsize = sizeof (struct in6_addr)/sizeof (uint32_t);
	char	**addrp = (caddr_t *)&(ps->addr);

	return (xdr_ypmap_wrap_string(xdrs, &ps->map) &&
		xdr_datum(xdrs, &ps->keydat) &&
		xdr_u_long(xdrs, &ps->xid) &&
		xdr_array(xdrs, addrp, &addrsize, addrsize,
			sizeof (uint32_t), xdr_uint32_t) &&
		xdr_u_short(xdrs, &ps->port));
}


u_long
svc_getxid(xprt)
register SVCXPRT *xprt;
{
	register struct bogus_data *su = getbogus_data(xprt);
	if (su == NULL)
		return (0);
	return (su->su_xid);
}
