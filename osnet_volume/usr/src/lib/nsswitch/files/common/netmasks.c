/*
 *	files/netmasks.c -- "files" backend for nsswitch "netmasks" database
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)netmasks.c	1.6	97/08/27 SMI"

/*
 * All routines necessary to deal with the file /etc/inet/netmasks.  The file
 * contains mappings from 32 bit network internet addresses to their
 * corresponding 32 bit mask internet addresses. The addresses are in dotted
 * internet address form.
 */

#include "files_common.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <nss_dbdefs.h>

/*
 * Validate 'files' netmasks entry. The comparison objects are in IPv4
 * internet address format.
 */
static int
check_addr(args)
	nss_XbyY_args_t		*args;
{
	struct in_addr tmp;

	tmp.s_addr = inet_addr(args->key.name);
	return (memcmp(args->buf.buffer, (char *)&tmp,
	    sizeof (struct in_addr)) == 0);
}

static nss_status_t
getbynet(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	nss_status_t		res;
	char			tmpbuf[NSS_LINELEN_NETMASKS];

	argp->buf.buffer = tmpbuf;
	argp->buf.buflen = NSS_LINELEN_NETMASKS;
	res = _nss_files_XY_all(be, argp, 0, argp->key.name, check_addr);
	argp->buf.buffer = NULL;
	argp->buf.buflen = 0;

	return (res);
}

static files_backend_op_t netmasks_ops[] = {
	_nss_files_destr,
	getbynet
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_netmasks_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(netmasks_ops,
	    sizeof (netmasks_ops) / sizeof (netmasks_ops[0]),
	    _PATH_NETMASKS, NSS_LINELEN_NETMASKS, NULL));
}
