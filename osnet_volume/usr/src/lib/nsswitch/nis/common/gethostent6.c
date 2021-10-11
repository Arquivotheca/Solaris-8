/*
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nis/gethostent.c -- "nis" backend for nsswitch "ipnodes" database
 */

#pragma ident "@(#)gethostent6.c	1.1	99/03/21 SMI"


#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "nis_common.h"
#include <stdlib.h>


static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	nss_status_t	res;

	const char		*s;
	char			c;

	for (s = argp->key.name;  (c = *s) != '\0';  s++) {
		if (isupper(c)) {
			char		*copy;
			char		*mung;

			if ((copy = strdup(argp->key.name)) == 0) {
				return (NSS_UNAVAIL);
			}
			for (mung = copy + (s - argp->key.name);
			    (c = *mung) != '\0';  mung++) {
				if (isupper(c)) {
					*mung = _tolower(c);
				}
			}
			res = _nss_nis_lookup(be, argp, 1, "ipnodes.byname",
				copy, 0);
			if (res != NSS_SUCCESS)
				argp->h_errno = __nss2herrno(res);
			free(copy);
			return (res);
		}
	}
	res = _nss_nis_lookup(be, argp, 1,
				"ipnodes.byname", argp->key.name, 0);
	if (res != NSS_SUCCESS)
		argp->h_errno = __nss2herrno(res);
	return (res);
}

static nss_status_t
getbyaddr(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp	= (nss_XbyY_args_t *) a;
	struct in6_addr		addr;
	char			buf[INET6_ADDRSTRLEN + 1];
	nss_status_t	res;

	/* === Do we really want to be this pedantic? */
	if (argp->key.hostaddr.type != AF_INET6 ||
	    argp->key.hostaddr.len  != sizeof (addr)) {
		return (NSS_NOTFOUND);
	}
	memcpy(&addr, argp->key.hostaddr.addr, sizeof (addr));
	if (IN6_IS_ADDR_V4MAPPED(&addr)) {
		if (inet_ntop(AF_INET, (void *) &addr.s6_addr[12],
				(void *)buf, INET_ADDRSTRLEN) == NULL) {
			return (NSS_NOTFOUND);
		}
	} else {
		if (inet_ntop(AF_INET6, (void *)&addr, (void *)buf,
						INET6_ADDRSTRLEN) == NULL)
			return (NSS_NOTFOUND);
	}

	res = _nss_nis_lookup(be, argp, 1, "ipnodes.byaddr", buf, 0);
	if (res != NSS_SUCCESS)
		argp->h_errno = __nss2herrno(res);
	return (res);
}


static nis_backend_op_t ipnodes_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbyname,
	getbyaddr
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_ipnodes_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(ipnodes_ops,
			sizeof (ipnodes_ops) / sizeof (ipnodes_ops[0]),
			"ipnodes.byaddr"));
}
