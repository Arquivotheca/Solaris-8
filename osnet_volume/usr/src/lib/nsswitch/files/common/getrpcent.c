/*
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	files/getrpcent.c -- "files" backend for nsswitch "rpc" database
 */

#pragma ident "@(#)getrpcent.c	1.9	97/08/12 SMI"

#include <rpc/rpcent.h>
#include "files_common.h"
#include <strings.h>

static int
check_name(args)
	nss_XbyY_args_t		*args;
{
	struct rpcent		*rpc	= (struct rpcent *) args->returnval;
	const char		*name	= args->key.name;
	char			**aliasp;

	if (strcmp(rpc->r_name, name) == 0) {
		return (1);
	}
	for (aliasp = rpc->r_aliases;  *aliasp != 0;  aliasp++) {
		if (strcmp(*aliasp, name) == 0) {
			return (1);
		}
	}
	return (0);
}

static nss_status_t
getbyname(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, argp, 1, argp->key.name, check_name));
}

static int
check_rpcnum(argp)
	nss_XbyY_args_t		*argp;
{
	struct rpcent		*rpc = (struct rpcent *) argp->returnval;

	return (rpc->r_number == argp->key.number);
}


static nss_status_t
getbynumber(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char			numstr[12];

	sprintf(numstr, "%d", argp->key.number);
	return (_nss_files_XY_all(be, argp, 1, numstr, check_rpcnum));
}

static files_backend_op_t rpc_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname,
	getbynumber
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_rpc_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(rpc_ops,
				sizeof (rpc_ops) / sizeof (rpc_ops[0]),
				"/etc/rpc",
				NSS_LINELEN_RPC,
				NULL));
}
