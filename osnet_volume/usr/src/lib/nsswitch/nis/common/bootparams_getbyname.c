/*
 *	nis/bootparams_getbyname.c -- "nis" backend for nsswitch "bootparams"
 *  database.
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)bootparams_getbyname.c	1.9	97/08/12 SMI"

#include <nss_dbdefs.h>
#include "nis_common.h"

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_nis_lookup(be, argp, 0, "bootparams",
				argp->key.name, 0));
}

static nis_backend_op_t bootparams_ops[] = {
	_nss_nis_destr,
	getbyname
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_bootparams_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(bootparams_ops,
		sizeof (bootparams_ops) / sizeof (bootparams_ops[0]),
		"bootparams"));
}
