/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#ident	"@(#)authsys_prot.c	1.12	97/07/07 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)authsys_prot.c 1.24 89/02/07 Copyr 1984 Sun Micro";
#endif

/*
 * authsys_prot.c
 * XDR for UNIX style authentication parameters for RPC
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <rpc/types.h>
#include <rpc/trace.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_sys.h>

bool_t xdr_uid_t();
bool_t xdr_gid_t();

/*
 * XDR for unix authentication parameters.
 */
bool_t
xdr_authsys_parms(XDR *xdrs, struct authsys_parms *p)
{
	trace1(TR_xdr_authsys_parms, 0);
	if (xdr_u_int(xdrs, &(p->aup_time)) &&
	    xdr_string(xdrs, &(p->aup_machname), MAX_MACHINE_NAME) &&
	    xdr_uid_t(xdrs, (uid_t *)&(p->aup_uid)) &&
	    xdr_gid_t(xdrs, (gid_t *)&(p->aup_gid)) &&
	    xdr_array(xdrs, (caddr_t *)&(p->aup_gids),
	    &(p->aup_len), NGRPS, (u_int)sizeof (gid_t),
	    (xdrproc_t) xdr_gid_t)) {
		trace1(TR_xdr_authsys_parms, 1);
		return (TRUE);
	}
	trace1(TR_xdr_authsys_parms, 1);
	return (FALSE);
}

/*
 * XDR for loopback unix authentication parameters.
 */
bool_t
xdr_authloopback_parms(XDR *xdrs, struct authsys_parms *p)
{
	/* trace1(TR_xdr_authsys_parms, 0); */
	if (xdr_u_int(xdrs, &(p->aup_time)) &&
	    xdr_string(xdrs, &(p->aup_machname), MAX_MACHINE_NAME) &&
	    xdr_uid_t(xdrs, (uid_t *)&(p->aup_uid)) &&
	    xdr_gid_t(xdrs, (gid_t *)&(p->aup_gid)) &&
	    xdr_array(xdrs, (caddr_t *)&(p->aup_gids),
	    &(p->aup_len), NGRPS_LOOPBACK, sizeof (gid_t),
	    (xdrproc_t) xdr_gid_t)) {
		/* trace1(TR_xdr_authsys_parms, 1); */
		return (TRUE);
	}
	/* trace1(TR_xdr_authsys_parms, 1); */
	return (FALSE);
}

/*
 * XDR user id types (uid_t)
 */
bool_t
xdr_uid_t(XDR *xdrs, uid_t *ip)
{
	bool_t dummy;

	trace1(TR_xdr_uid_t, 0);
#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	dummy = xdr_int(xdrs, (int *)ip);
	trace1(TR_xdr_uid_t, 1);
	return (dummy);
#else
	if (sizeof (uid_t) == sizeof (int)) {
		dummy = xdr_int(xdrs, (int *)ip);
		trace1(TR_xdr_uid_t, 1);
		return (dummy);
	} else {
		dummy = xdr_short(xdrs, (short *)ip);
		trace1(TR_xdr_uid_t, 1);
		return (dummy);
	}
#endif
}

/*
 * XDR group id types (gid_t)
 */
bool_t
xdr_gid_t(XDR *xdrs, gid_t *ip)
{
	bool_t dummy;

	trace1(TR_xdr_gid_t, 0);
#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	dummy = xdr_int(xdrs, (int *)ip);
	trace1(TR_xdr_gid_t, 1);
	return (dummy);
#else
	if (sizeof (gid_t) == sizeof (int)) {
		dummy = xdr_int(xdrs, (int *)ip);
		trace1(TR_xdr_gid_t, 1);
		return (dummy);
	} else {
		dummy = xdr_short(xdrs, (short *)ip);
		trace1(TR_xdr_gid_t, 1);
		return (dummy);
	}
#endif
}
