/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getauuser.c	1.1	99/04/07 SMI"

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <bsm/libbsm.h>
#include "nis_common.h"

static nss_status_t
getbynam(nis_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t *argp = (nss_XbyY_args_t *)a;

	return (_nss_nis_lookup(be,
		argp, 1, NIS_MAP_AUDITUSER, argp->key.name, 0));
}


static nis_backend_op_t auuser_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbynam
};

nss_backend_t  *
_nss_nis_audit_user_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3)
{
	return (_nss_nis_constr(auuser_ops,
		sizeof (auuser_ops)/sizeof (auuser_ops[0]),
		NIS_MAP_AUDITUSER));
}
