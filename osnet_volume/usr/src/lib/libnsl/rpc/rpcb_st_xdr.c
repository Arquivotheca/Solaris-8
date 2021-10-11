/*
 * Copyright 1991, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rpcb_st_xdr.c	1.6	97/12/17 SMI"

/*
 * This file was generated from rpcb_prot.x, but includes only those
 * routines used with the rpcbind stats facility.
 */

#include <rpc/rpc.h>
#include <rpc/trace.h>

/* Link list of all the stats about getport and getaddr */

bool_t
xdr_rpcbs_addrlist(xdrs, objp)
	XDR *xdrs;
	rpcbs_addrlist *objp;
{
	trace1(TR_xdr_rpcbs_addrlist, 0);

	    if (!xdr_u_int(xdrs, (u_int *)&objp->prog)) {
		trace1(TR_xdr_rpcbs_addrlist, 1);
		return (FALSE);
	    }
	    if (!xdr_u_int(xdrs, (u_int *)&objp->vers)) {
		trace1(TR_xdr_rpcbs_addrlist, 1);
		return (FALSE);
	    }
	    if (!xdr_int(xdrs, &objp->success)) {
		trace1(TR_xdr_rpcbs_addrlist, 1);
		return (FALSE);
	    }
	    if (!xdr_int(xdrs, &objp->failure)) {
		trace1(TR_xdr_rpcbs_addrlist, 1);
		return (FALSE);
	    }
	    if (!xdr_string(xdrs, &objp->netid, ~0)) {
		trace1(TR_xdr_rpcbs_addrlist, 1);
		return (FALSE);
	    }

	    if (!xdr_pointer(xdrs, (char **)&objp->next,
			(u_int) sizeof (rpcbs_addrlist),
			(xdrproc_t)xdr_rpcbs_addrlist)) {
		trace1(TR_xdr_rpcbs_addrlist, 1);
		return (FALSE);
	    }

	return (TRUE);
}

/* Link list of all the stats about rmtcall */

bool_t
xdr_rpcbs_rmtcalllist(xdrs, objp)
	XDR *xdrs;
	rpcbs_rmtcalllist *objp;
{
	register rpc_inline_t *buf;

	trace1(TR_xdr_rpcbs_rmtcalllist, 0);
	if (xdrs->x_op == XDR_ENCODE) {
	buf = XDR_INLINE(xdrs, 6 * BYTES_PER_XDR_UNIT);
	if (buf == NULL) {
		if (!xdr_u_int(xdrs, (u_int *)&objp->prog)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_u_int(xdrs, (u_int *)&objp->vers)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_u_int(xdrs, (u_int *)&objp->proc)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->success)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->failure)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->indirect)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
	} else {
		IXDR_PUT_U_INT32(buf, objp->prog);
		IXDR_PUT_U_INT32(buf, objp->vers);
		IXDR_PUT_U_INT32(buf, objp->proc);
		IXDR_PUT_INT32(buf, objp->success);
		IXDR_PUT_INT32(buf, objp->failure);
		IXDR_PUT_INT32(buf, objp->indirect);
	}
	if (!xdr_string(xdrs, &objp->netid, ~0)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->next,
			(u_int) sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcbs_rmtcalllist, 1);
	return (TRUE);
	} else if (xdrs->x_op == XDR_DECODE) {
	buf = XDR_INLINE(xdrs, 6 * BYTES_PER_XDR_UNIT);
	if (buf == NULL) {
		if (!xdr_u_int(xdrs, (u_int *)&objp->prog)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_u_int(xdrs, (u_int *)&objp->vers)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_u_int(xdrs, (u_int *)&objp->proc)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->success)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->failure)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->indirect)) {
			trace1(TR_xdr_rpcbs_rmtcalllist, 1);
			return (FALSE);
		}
	} else {
		objp->prog = IXDR_GET_U_INT32(buf);
		objp->vers = IXDR_GET_U_INT32(buf);
		objp->proc = IXDR_GET_U_INT32(buf);
		objp->success = IXDR_GET_INT32(buf);
		objp->failure = IXDR_GET_INT32(buf);
		objp->indirect = IXDR_GET_INT32(buf);
	}
	if (!xdr_string(xdrs, &objp->netid, ~0)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->next,
			(u_int) sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcbs_rmtcalllist, 1);
	return (TRUE);
	}
	if (!xdr_u_int(xdrs, (u_int *)&objp->prog)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, (u_int *)&objp->vers)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, (u_int *)&objp->proc)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->success)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->failure)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->indirect)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->netid, ~0)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->next,
			(u_int) sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		trace1(TR_xdr_rpcbs_rmtcalllist, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcbs_rmtcalllist, 1);
	return (TRUE);
}

bool_t
xdr_rpcbs_proc(xdrs, objp)
	XDR *xdrs;
	rpcbs_proc objp;
{
	trace1(TR_xdr_rpcbs_proc, 0);
	if (!xdr_vector(xdrs, (char *)objp, RPCBSTAT_HIGHPROC, sizeof (int),
			(xdrproc_t)xdr_int)) {
		trace1(TR_xdr_rpcbs_proc, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcbs_proc, 1);
	return (TRUE);
}

bool_t
xdr_rpcbs_addrlist_ptr(xdrs, objp)
	XDR *xdrs;
	rpcbs_addrlist_ptr *objp;
{
	trace1(TR_xdr_rpcbs_addrlist_ptr, 0);
	if (!xdr_pointer(xdrs, (char **)objp, (u_int) sizeof (rpcbs_addrlist),
			(xdrproc_t)xdr_rpcbs_addrlist)) {
		trace1(TR_xdr_rpcbs_addrlist_ptr, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcbs_addrlist_ptr, 1);
	return (TRUE);
}

bool_t
xdr_rpcbs_rmtcalllist_ptr(xdrs, objp)
	XDR *xdrs;
	rpcbs_rmtcalllist_ptr *objp;
{
	trace1(TR_xdr_rpcbs_rmtcalllist_ptr, 0);
	if (!xdr_pointer(xdrs, (char **)objp, sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		trace1(TR_xdr_rpcbs_rmtcalllist_ptr, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcbs_rmtcalllist_ptr, 1);
	return (TRUE);
}

bool_t
xdr_rpcb_stat(xdrs, objp)
	XDR *xdrs;
	rpcb_stat *objp;
{

	trace1(TR_xdr_rpcb_stat, 0);
	if (!xdr_rpcbs_proc(xdrs, objp->info)) {
		trace1(TR_xdr_rpcb_stat, 1);
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->setinfo)) {
		trace1(TR_xdr_rpcb_stat, 1);
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->unsetinfo)) {
		trace1(TR_xdr_rpcb_stat, 1);
		return (FALSE);
	}
	if (!xdr_rpcbs_addrlist_ptr(xdrs, &objp->addrinfo)) {
		trace1(TR_xdr_rpcb_stat, 1);
		return (FALSE);
	}
	if (!xdr_rpcbs_rmtcalllist_ptr(xdrs, &objp->rmtinfo)) {
		trace1(TR_xdr_rpcb_stat, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcb_stat, 1);
	return (TRUE);
}

/*
 * One rpcb_stat structure is returned for each version of rpcbind
 * being monitored.
 */
bool_t
xdr_rpcb_stat_byvers(xdrs, objp)
    XDR *xdrs;
    rpcb_stat_byvers objp;
{
	trace1(TR_xdr_rpcb_stat_byvers, 0);
	if (!xdr_vector(xdrs, (char *)objp, RPCBVERS_STAT, sizeof (rpcb_stat),
	    (xdrproc_t)xdr_rpcb_stat)) {
		trace1(TR_xdr_rpcb_stat_byvers, 1);
		return (FALSE);
	}
	trace1(TR_xdr_rpcb_stat_byvers, 1);
	return (TRUE);
}
