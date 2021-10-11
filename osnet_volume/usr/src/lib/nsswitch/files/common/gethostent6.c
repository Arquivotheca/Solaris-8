/*
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	files/gethostent6.c -- "files" backend for nsswitch "hosts" database
 */

#pragma ident	"@(#)gethostent6.c	1.1	99/03/21 SMI"

#include <netdb.h>
#include "files_common.h"
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <ctype.h>

extern nss_status_t __nss_files_XY_hostbyname();
extern int __nss_files_2herrno();
extern int __nss_files_check_addr(nss_XbyY_args_t *);


static nss_status_t
getbyname(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	nss_status_t		res;

	res = __nss_files_XY_hostbyname(be, argp, argp->key.name, AF_INET6);
	if (res != NSS_SUCCESS)
		argp->h_errno = __nss_files_2herrno(res);
	return (res);
}



static nss_status_t
getbyaddr(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp	= (nss_XbyY_args_t *) a;
	nss_status_t		res;


	res = _nss_files_XY_all(be, argp, 1, 0, __nss_files_check_addr);
	if (res != NSS_SUCCESS)
		argp->h_errno = __nss_files_2herrno(res);
	return (res);
}


static files_backend_op_t ipnodes_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname,
	getbyaddr,
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_ipnodes_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(ipnodes_ops,
				sizeof (ipnodes_ops) / sizeof (ipnodes_ops[0]),
				_PATH_IPNODES,
				NSS_LINELEN_HOSTS,
				NULL));
}
