/*
 *	Copyright (c) 1985 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)yppasswd_xdr.c	1.4	97/08/01 SMI"

#include <rpc/rpc.h>
#include <rpcsvc/yppasswd.h>

bool_t
xdr_passwd(xdrsp, pw)
	XDR *xdrsp;
	struct passwd *pw;
{
	if (xdr_wrapstring(xdrsp, &pw->pw_name) == 0)
		return (FALSE);
	if (xdr_wrapstring(xdrsp, &pw->pw_passwd) == 0)
		return (FALSE);
	if (xdr_int(xdrsp, (int *)&pw->pw_uid) == 0)
		return (FALSE);
	if (xdr_int(xdrsp, (int *)&pw->pw_gid) == 0)
		return (FALSE);
	if (xdr_wrapstring(xdrsp, &pw->pw_gecos) == 0)
		return (FALSE);
	if (xdr_wrapstring(xdrsp, &pw->pw_dir) == 0)
		return (FALSE);
	if (xdr_wrapstring(xdrsp, &pw->pw_shell) == 0)
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_yppasswd(xdrsp, pp)
	XDR *xdrsp;
	struct yppasswd *pp;
{
	if (xdr_wrapstring(xdrsp, &pp->oldpass) == 0)
		return (FALSE);
	if (xdr_passwd(xdrsp, &pp->newpw) == 0)
		return (FALSE);
	return (TRUE);
}
