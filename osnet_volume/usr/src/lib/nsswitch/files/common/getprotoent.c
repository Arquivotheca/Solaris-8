/*
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	files/getprotoent.c -- "files" backend for nsswitch "protocols" database
 */

#pragma ident	"@(#)getprotoent.c	1.8	97/08/12 SMI"

#include <netdb.h>
#include "files_common.h"
#include <strings.h>

static int
check_name(args)
	nss_XbyY_args_t	*args;
{
	struct protoent	*proto = (struct protoent *)args->returnval;
	const char		*name = args->key.name;
	char			**aliasp;

	if (strcmp(proto->p_name, name) == 0)
		return (1);
	for (aliasp = proto->p_aliases; *aliasp != 0; aliasp++) {
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
	struct protoent	*proto = (struct protoent *)args->returnval;

	return (proto->p_proto == args->key.number);
}

static nss_status_t
getbynumber(be, a)
	files_backend_ptr_t	be;
	void		*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char		numstr[12];

	sprintf(numstr, "%d", argp->key.number);
	return (_nss_files_XY_all(be, argp, 1, 0, check_addr));
}

static files_backend_op_t proto_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname,
	getbynumber
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_protocols_constr(dummy1, dummy2, dummy3)
	const char  *dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(proto_ops,
				sizeof (proto_ops) / sizeof (proto_ops[0]),
				_PATH_PROTOCOLS,
				NSS_LINELEN_PROTOCOLS,
				NULL));
}
