/*
 *
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	files/getnetent.c -- "files" backend for nsswitch "networks" database
 */

#pragma ident	"@(#)getnetent.c	1.8	97/08/12 SMI"


#include <netdb.h>
#include "files_common.h"
#include <strings.h>

static int
check_name(args)
	nss_XbyY_args_t	*args;
{
	struct netent	*net = (struct netent *)args->returnval;
	const char		*name = args->key.name;
	char			**aliasp;

	if (strcmp(net->n_name, name) == 0)
		return (1);
	for (aliasp = net->n_aliases; *aliasp != 0; aliasp++) {
		if (strcmp(*aliasp, name) == 0)
			return (1);
	}
	return (0);
}

static nss_status_t
getbyname(be, a)
	files_backend_ptr_t	be;
	void		*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, argp, 1, argp->key.name, check_name));
}

static int
check_addr(args)
	nss_XbyY_args_t	*args;
{
	struct netent	*net = (struct netent *)args->returnval;

	return ((net->n_addrtype == args->key.netaddr.type) &&
		(net->n_net == args->key.netaddr.net));
}

static nss_status_t
getbyaddr(be, a)
	files_backend_ptr_t	be;
	void		*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, argp, 1, 0, check_addr));
}

static files_backend_op_t net_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname,
	getbyaddr
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_networks_constr(dummy1, dummy2, dummy3)
	const char  *dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(net_ops,
				sizeof (net_ops) / sizeof (net_ops[0]),
				_PATH_NETWORKS,
				NSS_LINELEN_NETWORKS,
				NULL));
}
