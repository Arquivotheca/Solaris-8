/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getauthattr.c	1.1	99/04/07 SMI"

#include "files_common.h"
#include <auth_attr.h>
#include <string.h>

/*
 *    files/getauthattr.c --
 *           "files" backend for nsswitch "auth_attr" database
 */
static int
check_name(nss_XbyY_args_t *args)
{
	authstr_t *auth = (authstr_t *) args->returnval;
	const char *name    = args->key.name;

	if (strcmp(auth->name, name) == 0) {
		args->h_errno = 0;
		return (1);
	}
	return (0);
}

static nss_status_t
getbyname(files_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t *argp = (nss_XbyY_args_t *) a;

	return (_nss_files_XY_all(be, argp, 1, argp->key.name, check_name));
}

static files_backend_op_t authattr_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname
};

nss_backend_t *
_nss_files_auth_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5,
    const char *dummy6)
{
	return (_nss_files_constr(authattr_ops,
	    sizeof (authattr_ops) / sizeof (authattr_ops[0]),
	    AUTHATTR_FILENAME,
	    NSS_LINELEN_AUTHATTR,
	    NULL));
}
