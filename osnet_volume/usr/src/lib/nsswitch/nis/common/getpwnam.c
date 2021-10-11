/*
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nis/getpwnam.c -- "nis" backend for nsswitch "passwd" database
 */

#pragma ident "@(#)getpwnam.c	1.6	97/08/12 SMI"

#include <pwd.h>
#include "nis_common.h"

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_nis_lookup(be, argp, 0,
				"passwd.byname", argp->key.name, 0));
}

static nss_status_t
getbyuid(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char			uidstr[12];	/* More than enough */

	sprintf(uidstr, "%d", argp->key.uid);
	return (_nss_nis_lookup(be, argp, 0, "passwd.byuid", uidstr, 0));
}

static nis_backend_op_t passwd_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_rigid,
	getbyname,
	getbyuid
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_passwd_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, dummy3;
{
	return (_nss_nis_constr(passwd_ops,
				sizeof (passwd_ops) / sizeof (passwd_ops[0]),
				"passwd.byname"));
}
