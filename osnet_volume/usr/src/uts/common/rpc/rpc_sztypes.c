/*
 * Copyright (c) 1993-1997 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rpc_sztypes.c	1.3	97/04/29 SMI"

/*
 * XDR routines for generic types that have explicit sizes.
 */

#include <rpc/rpc_sztypes.h>

/*
 * The new NFS protocol uses typedefs to name objects according to their
 * length (32 bits, 64 bits).  These objects appear in both the NFS and KLM
 * code, so the xdr routines live here.
 */

bool_t
xdr_uint64(XDR *xdrs, uint64 *objp)
{
	return (xdr_u_longlong_t(xdrs, objp));
}

bool_t
xdr_int64(XDR *xdrs, int64 *objp)
{
	return (xdr_longlong_t(xdrs, objp));
}

bool_t
xdr_uint32(XDR *xdrs, uint32 *objp)
{
	return (xdr_u_int(xdrs, objp));
}

bool_t
xdr_int32(XDR *xdrs, int32 *objp)
{
	return (xdr_int(xdrs, objp));
}
