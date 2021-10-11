/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)authu_prot.c	1.15	97/06/26 SMI"	/* SVr4.0 1.6	*/

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986, 1987, 1988, 1989, 1997  Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

/*
 * authunix_prot.c
 * XDR for UNIX style authentication parameters for RPC
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/utsname.h>

#include <rpc/types.h>
#include <rpc/rpc_sztypes.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/clnt.h>

/*
 * XDR for unix authentication parameters.
 */
bool_t
xdr_authunix_parms(XDR *xdrs, struct authunix_parms *p)
{
	if (xdr_u_int(xdrs, &p->aup_time) &&
	    xdr_string(xdrs, &p->aup_machname, MAX_MACHINE_NAME) &&
	    xdr_int(xdrs, (int *)&(p->aup_uid)) &&
	    xdr_int(xdrs, (int *)&(p->aup_gid)) &&
	    xdr_array(xdrs, (caddr_t *)&(p->aup_gids),
		    &(p->aup_len), NGRPS, sizeof (int),
		    (xdrproc_t) xdr_int)) {
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR user id types (uid_t)
 */
bool_t
xdr_uid_t(XDR *xdrs, uid_t *ip)
{
#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	return (xdr_int32(xdrs, (int32_t *)ip));
#else
	if (sizeof (uid_t) == sizeof (int32_t)) {
		return (xdr_int(xdrs, (int32_t *)ip));
	} else {
		return (xdr_short(xdrs, (short *)ip));
	}
#endif
}

/*
 * XDR group id types (gid_t)
 */
bool_t
xdr_gid_t(XDR *xdrs, gid_t *ip)
{
#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	return (xdr_int32(xdrs, (int32_t *)ip));
#else
	if (sizeof (gid_t) == sizeof (int32_t)) {
		return (xdr_int32(xdrs, (int32_t *)ip));
	} else {
		return (xdr_short(xdrs, (short *)ip));
	}
#endif
}

/*
 * XDR kernel unix auth parameters.
 * Goes out of the u struct directly.
 * NOTE: this is an XDR_ENCODE only routine.
 */
bool_t
xdr_authkern(XDR *xdrs)
{
	uid_t uid;
	gid_t gid;
	int len;
	caddr_t groups;
	char *name = (caddr_t)utsname.nodename;
	struct cred *cr;

	if (xdrs->x_op != XDR_ENCODE)
		return (FALSE);

	cr = CRED();
	uid = cr->cr_uid;
	gid = cr->cr_gid;
	len = cr->cr_ngroups;
	groups = (caddr_t)cr->cr_groups;
	if (xdr_uint32(xdrs, (uint32_t *)&hrestime.tv_sec) &&
	    xdr_string(xdrs, &name, MAX_MACHINE_NAME) &&
	    xdr_uid_t(xdrs, &uid) &&
	    xdr_gid_t(xdrs, &gid) &&
	    xdr_array(xdrs, &groups, (u_int *)&len, NGRPS, sizeof (int),
	    (xdrproc_t)xdr_int))
		return (TRUE);
	return (FALSE);
}

/*
 * XDR loopback unix auth parameters.
 * NOTE: this is an XDR_ENCODE only routine.
 */
bool_t
xdr_authloopback(XDR *xdrs)
{
	uid_t uid;
	gid_t gid;
	int len;
	caddr_t groups;
	char *name = (caddr_t)utsname.nodename;
	struct cred *cr;

	if (xdrs->x_op != XDR_ENCODE)
		return (FALSE);

	cr = CRED();
	uid = cr->cr_uid;
	gid = cr->cr_gid;
	len = cr->cr_ngroups;
	groups = (caddr_t)cr->cr_groups;
	if (xdr_uint32(xdrs, (uint32_t *)&hrestime.tv_sec) &&
	    xdr_string(xdrs, &name, MAX_MACHINE_NAME) &&
	    xdr_uid_t(xdrs, &uid) &&
	    xdr_gid_t(xdrs, &gid) &&
	    xdr_array(xdrs, &groups, (u_int *)&len, NGRPS_LOOPBACK,
	    sizeof (int), (xdrproc_t)xdr_int))
		return (TRUE);
	return (FALSE);
}
