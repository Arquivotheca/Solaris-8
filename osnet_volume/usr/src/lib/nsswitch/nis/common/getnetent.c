/*
 *	nis/getnetent.c -- "nis" backend for nsswitch "networks" database
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident "@(#)getnetent.c	1.9	97/08/12	SMI"

#include "nis_common.h"
#include <synch.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

static int nettoa(int anet, char *buf, int buflen);

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_nis_lookup(be, argp, 1, "networks.byname",
			      argp->key.name, 0));
}

static nss_status_t
getbyaddr(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char			addrstr[16];

	if (nettoa((int) argp->key.netaddr.net, addrstr, 16) != 0)
		return (NSS_UNAVAIL);	/* it's really ENOMEM */
	return (_nss_nis_lookup(be, argp, 1, "networks.byaddr", addrstr, 0));
}

static nis_backend_op_t net_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbyname,
	getbyaddr
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_networks_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(net_ops,
				sizeof (net_ops) / sizeof (net_ops[0]),
				"networks.byaddr"));
}

/*
 * Takes an unsigned integer in host order, and returns a printable
 * string for it as a network number.  To allow for the possibility of
 * naming subnets, only trailing dot-zeros are truncated.
 */
static int
nettoa(anet, buf, buflen)
	int		anet;
	char	*buf;
	int		buflen;		
{
	char *p;
	struct in_addr in;
	int addr;

	if (buf == 0)
		return (1);
	in = inet_makeaddr(anet, INADDR_ANY);
	addr = in.s_addr;
	(void) strncpy(buf, inet_ntoa(in), buflen);
	if ((IN_CLASSA_HOST & htonl(addr)) == 0) {
		p = strchr(buf, '.');
		if (p == NULL)
			return (1);
		*p = 0;
	} else if ((IN_CLASSB_HOST & htonl(addr)) == 0) {
		p = strchr(buf, '.');
		if (p == NULL)
			return (1);
		p = strchr(p+1, '.');
		if (p == NULL)
			return (1);
		*p = 0;
	} else if ((IN_CLASSC_HOST & htonl(addr)) == 0) {
		p = strrchr(buf, '.');
		if (p == NULL)
			return (1);
		*p = 0;
	}
	return (0);
}
