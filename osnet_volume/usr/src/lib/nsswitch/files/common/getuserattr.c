/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getuserattr.c	1.1	99/04/07 SMI"

#include "files_common.h"
#include <user_attr.h>
#include <string.h>

/*
 *    files/getuserattr.c --
 *           "files" backend for nsswitch "user_attr" database
 */
static int
check_name(nss_XbyY_args_t *args)
{
	userstr_t *user = (userstr_t *) args->returnval;
	const char *name    = args->key.name;

	if (strcmp(user->name, name) == 0) {
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

static files_backend_op_t userattr_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname
};

nss_backend_t *
_nss_files_user_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
	return (_nss_files_constr(userattr_ops,
	    sizeof (userattr_ops) / sizeof (userattr_ops[0]),
	    USERATTR_FILENAME,
	    NSS_LINELEN_USERATTR,
	    NULL));
}
