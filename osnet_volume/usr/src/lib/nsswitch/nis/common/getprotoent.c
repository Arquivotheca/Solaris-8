/*
 *	nis/getprotoent.c -- "nis" backend for nsswitch "protocols" database
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident "@(#)getprotoent.c	1.9	97/08/12	SMI"

#include <stdio.h>
#include "nis_common.h"
#include <synch.h>
#include <netdb.h>
#include <string.h>

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_nis_lookup(be, argp, 1, "protocols.byname",
		argp->key.name, 0));
}

static nss_status_t
getbynumber(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char			numstr[12];

	(void) sprintf(numstr, "%d", argp->key.number);
	return (_nss_nis_lookup(be, argp, 1, "protocols.bynumber",
				numstr, 0));
}

static nis_backend_op_t proto_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbyname,
	getbynumber
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_protocols_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(proto_ops,
				sizeof (proto_ops) / sizeof (proto_ops[0]),
				"protocols.bynumber"));
}
