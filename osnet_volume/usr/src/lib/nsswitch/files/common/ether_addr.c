/*
 *	files/ether_addr.c -- "files" backend for nsswitch "ethers" database
 *
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)ether_addr.c	1.11	97/08/12 SMI"

/*
 * All routines necessary to deal with the file /etc/ethers.  The file
 * contains mappings from 48 bit ethernet addresses to their corresponding
 * hosts names.  The addresses have an ascii representation of the form
 * "x:x:x:x:x:x" where x is a hex number between 0x00 and 0xff;  the
 * bytes are always in network order.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <nss_dbdefs.h>
#include "files_common.h"
#include <strings.h>

#define	_PATH_ETHERS	"/etc/ethers"

static int
check_host(args)
	nss_XbyY_args_t		*args;
{
	return (strcmp(args->buf.buffer, args->key.name) == 0);
}

static nss_status_t
getbyhost(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char	hostname[MAXHOSTNAMELEN];
	nss_status_t		res;

	argp->buf.buffer = hostname;
	argp->buf.buflen = MAXHOSTNAMELEN;

	res = _nss_files_XY_all(be, argp, 0, argp->key.name, check_host);

	argp->buf.buffer = NULL;
	argp->buf.buflen = 0;
	return (res);
}

static int
check_ether(args)
	nss_XbyY_args_t		*args;
{
	return (memcmp((char *) args->buf.result, (char *) (args->key.ether),
			sizeof (ether_addr_t)) == 0);
}

static nss_status_t
getbyether(be, a)
	files_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	ether_addr_t		etheraddr;
	nss_status_t		res;

	argp->buf.result	= etheraddr;

	res = _nss_files_XY_all(be, argp, 0, NULL, check_ether);

	argp->buf.result	= NULL;
	return (res);
}

static files_backend_op_t ethers_ops[] = {
	_nss_files_destr,
	getbyhost,
	getbyether
};

/*ARGSUSED*/
nss_backend_t *
_nss_files_ethers_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_files_constr(ethers_ops,
				sizeof (ethers_ops) / sizeof (ethers_ops[0]),
				_PATH_ETHERS,
				NSS_LINELEN_ETHERS,
				NULL));
}
