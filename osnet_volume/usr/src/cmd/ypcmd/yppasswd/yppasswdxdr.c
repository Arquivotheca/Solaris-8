/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
							    
#pragma ident "@(#)yppasswdxdr.c	1.4	93/10/24 - SMI - mods by OpCom"

#include <rpc/rpc.h>
#include <rpcsvc/yppasswd.h>

extern bool_t xdr_uid_t(XDR *, uid_t *);
extern bool_t xdr_gid_t(XDR *, gid_t *);

bool_t
xdr_passwd(XDR *xdrs, struct passwd *pw)
{
	if (!xdr_wrapstring(xdrs, &pw->pw_name)) {
		return (FALSE);
	}
	if (!xdr_wrapstring(xdrs, &pw->pw_passwd)) {
		return (FALSE);
	}
	if (!xdr_uid_t(xdrs, &pw->pw_uid)) {
		return (FALSE);
	}
	if (!xdr_gid_t(xdrs, (&pw->pw_gid))) {
		return (FALSE);
	}
	if (!xdr_wrapstring(xdrs, &pw->pw_gecos)) {
		return (FALSE);
	}
	if (!xdr_wrapstring(xdrs, &pw->pw_dir)) {
		return (FALSE);
	}
	if (!xdr_wrapstring(xdrs, &pw->pw_shell)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_yppasswd(XDR *xdrs, struct yppasswd *yppw)
{
	if (!xdr_wrapstring(xdrs, &yppw->oldpass)) {
		return (FALSE);
	}
	if (!xdr_passwd(xdrs, &yppw->newpw)) {
		return (FALSE);
	}
	return (TRUE);
}
