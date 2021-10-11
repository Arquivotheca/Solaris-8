/*
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	files/getspent.c -- "files" backend for nsswitch "shadow" database
 */

#pragma ident "@(#)getspent.c	1.9	99/05/11 SMI"

#include <shadow.h>
#include "files_common.h"
#include <strings.h>

static int
check_spnamp(argp)
	nss_XbyY_args_t		*argp;
{
	struct spwd		*s = (struct spwd *)argp->returnval;

	/* +/- entries valid in compat source only */
	if (s->sp_namp != 0 && (s->sp_namp[0] == '+' || s->sp_namp[0] == '-'))
		return (0);
	return (strcmp(s->sp_namp, argp->key.name) == 0);
}

static nss_status_t
getbyname(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, argp, 0, argp->key.name, check_spnamp));
}

static files_backend_op_t shadow_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_rigid,
	getbyname
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_shadow_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(shadow_ops,
				sizeof (shadow_ops) / sizeof (shadow_ops[0]),
				SHADOW,
				NSS_LINELEN_SHADOW,
				NULL));
}
