/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getauuser.c	1.1	99/04/07 SMI"

#include "files_common.h"
#include <bsm/libbsm.h>
#include <string.h>

/*
 *    files/getauuser.c --
 *           "files" backend for nsswitch "audit_user" database
 */
static int
check_name(nss_XbyY_args_t *args)
{
	au_user_str_t *au_user = (au_user_str_t *) args->returnval;
	const char *name = args->key.name;

	if (strcmp(au_user->au_name, name) == 0) {
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

static files_backend_op_t auuser_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname
};

nss_backend_t *
_nss_files_audit_user_constr(const char *dummy1, const char *dummy2,
    const char *dummy3)
{
	return (_nss_files_constr(auuser_ops,
	    sizeof (auuser_ops) / sizeof (auuser_ops[0]),
	    AUDITUSER_FILENAME,
	    NSS_LINELEN_AUDITUSER,
	    NULL));
}
