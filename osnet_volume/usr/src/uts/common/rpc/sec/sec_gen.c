/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  From  common/syscall/systeminfo.c
 */

#ident	"@(#)sec_gen.c	1.10	99/07/18 SMI"

#include <sys/types.h>
#include <sys/systeminfo.h>	/* for SI_KERB stuff */

#include <sys/errno.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/svc_auth.h>


/*
 * authany_wrap() is a NO-OP routines for ah_wrap().
 */
/* ARGSUSED */
int
authany_wrap(AUTH *auth, caddr_t buf, u_int buflen,
    XDR *xdrs, xdrproc_t xfunc, caddr_t xwhere)
{
	return (*xfunc)(xdrs, xwhere);
}

/*
 * authany_unwrap() is a NO-OP routines for ah_unwrap().
 */
/* ARGSUSED */
int
authany_unwrap(AUTH *auth, XDR *xdrs, xdrproc_t xfunc, caddr_t xwhere)
{
	return (*xfunc)(xdrs, xwhere);
}
