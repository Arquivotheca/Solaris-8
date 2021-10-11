/*
 * Copyright (c) 1999 by Sun Microsystems Inc
 * All Rights Reserved.
 */

#pragma ident	"@(#)getprinter.c	1.1	99/01/21 SMI"

#include <nss_dbdefs.h>
#include "nis_common.h"

#pragma weak _nss_nis__printers_constr = _nss_nis_printers_constr


static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	nss_status_t		res;

	return (_nss_nis_lookup(be, argp, 0, "printers.conf.byname",
				argp->key.name, 0));
}

static nis_backend_op_t printers_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_rigid,
	getbyname
};

nss_backend_t *
_nss_nis_printers_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(printers_ops,
		sizeof (printers_ops) / sizeof (printers_ops[0]),
		"printers.conf.byname"));
}
