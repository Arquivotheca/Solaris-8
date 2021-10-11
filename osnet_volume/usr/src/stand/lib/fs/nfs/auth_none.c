/*
 *
 * Copyright (C) 1984,1996-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)auth_none.c	1.15	99/02/23 SMI" /* from SunOS 4.1 */

/*
 * modified for use by the boot program.
 *
 * auth_none.c
 * Creates a client authentication handle for passing "null"
 * credentials and verifiers to remote systems.
 */

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#define	MAX_MARSHEL_SIZE 20

static struct auth_ops *authnone_ops();

static struct authnone_private {
	AUTH	no_client;
	char	marshalled_client[MAX_MARSHEL_SIZE];
	u_int	mcnt;
} *authnone_private;

struct authnone_private authnone_local;		/* XXXX - Can't be dynamic */

AUTH *
authnone_create(void)
{
	register struct authnone_private *ap = authnone_private;
	extern struct authnone_private authnone_local;
	XDR xdr_stream;
	register XDR *xdrs;

	if (ap == 0) {
		ap = &authnone_local;
		authnone_private = ap;
	}
	if (!ap->mcnt) {
		ap->no_client.ah_cred = ap->no_client.ah_verf = _null_auth;
		ap->no_client.ah_ops = authnone_ops();
		xdrs = &xdr_stream;
		xdrmem_create(xdrs, ap->marshalled_client,
			(u_int)MAX_MARSHEL_SIZE, XDR_ENCODE);
		(void) xdr_opaque_auth(xdrs, &ap->no_client.ah_cred);
		(void) xdr_opaque_auth(xdrs, &ap->no_client.ah_verf);
		ap->mcnt = XDR_GETPOS(xdrs);
		XDR_DESTROY(xdrs);
	}
	return (&ap->no_client);
}

/*ARGSUSED*/
static bool_t
authnone_marshal(AUTH *client, XDR *xdrs, struct cred *cr)
{
	register struct authnone_private *ap = authnone_private;

	if (ap == 0)
		return (0);
	return ((*xdrs->x_ops->x_putbytes)(xdrs,
	    ap->marshalled_client, ap->mcnt));
}

/* ARGSUSED */
static void
authnone_verf(AUTH *foo)
{
}

/* ARGSUSED */
static bool_t
authnone_validate(AUTH *foo, struct opaque_auth *bar)
{
	return (TRUE);
}

/* ARGSUSED */
static bool_t
authnone_refresh(AUTH *foo, struct rpc_msg *bar, cred_t *cr)
{
	return (FALSE);
}

/* ARGSUSED */
static void
authnone_destroy(AUTH *foo)
{
}

static struct auth_ops *
authnone_ops(void)
{
	static struct auth_ops ops;

	if (ops.ah_nextverf == NULL) {
		ops.ah_nextverf = authnone_verf;
		ops.ah_marshal = authnone_marshal;
		ops.ah_validate = authnone_validate;
		ops.ah_refresh = authnone_refresh;
		ops.ah_destroy = authnone_destroy;
	}
	return (&ops);
}
