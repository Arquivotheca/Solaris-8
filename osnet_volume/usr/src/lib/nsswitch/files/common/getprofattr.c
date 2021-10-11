/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getprofattr.c	1.1	99/04/07 SMI"

#include "files_common.h"
#include <prof_attr.h>
#include <string.h>

/*
 *    files/getprofattr.c --
 *           "files" backend for nsswitch "prof_attr" database
 */
static int
check_name(nss_XbyY_args_t *args)
{
	profstr_t *prof = (profstr_t *)args->returnval;
	const char *name = args->key.name;

	if (strcmp(prof->name, name) == 0) {
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

static files_backend_op_t profattr_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname
};

nss_backend_t *
_nss_files_prof_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
	return (_nss_files_constr(profattr_ops,
	    sizeof (profattr_ops) / sizeof (profattr_ops[0]),
	    PROFATTR_FILENAME,
	    NSS_LINELEN_PROFATTR,
	    NULL));
}
