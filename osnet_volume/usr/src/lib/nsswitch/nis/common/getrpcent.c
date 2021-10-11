/*
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nis/getrpcent.c -- "nis" backend for nsswitch "rpc" database
 */

#pragma ident "@(#)getrpcent.c	1.9	97/08/12	SMI"

#include "nis_common.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <synch.h>
#include <rpc/rpcent.h>
#include <rpcsvc/ypclnt.h>
#include <thread.h>

static int
check_name(args)
	nss_XbyY_args_t		*args;
{
	struct rpcent		*rpc	= (struct rpcent *) args->returnval;
	const char		*name	= args->key.name;
	char			**aliasp;

	if (strcmp(rpc->r_name, name) == 0) {
		return (1);
	}
	for (aliasp = rpc->r_aliases;  *aliasp != 0;  aliasp++) {
		if (strcmp(*aliasp, name) == 0) {
			return (1);
		}
	}
	return (0);
}

static mutex_t	no_byname_lock	= DEFAULTMUTEX;
static int	no_byname_map	= 0;

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	int			no_map;
	sigset_t		oldmask, newmask;

	sigfillset(&newmask);
	(void) _thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	(void) _mutex_lock(&no_byname_lock);
	no_map = no_byname_map;
	(void) _mutex_unlock(&no_byname_lock);
	(void) _thr_sigsetmask(SIG_SETMASK, &oldmask, (sigset_t*)NULL);

	if (no_map == 0) {
		int		yp_status;
		nss_status_t	res;

		res = _nss_nis_lookup(be, argp, 1, "rpc.byname",
				      argp->key.name, &yp_status);
		if (yp_status == YPERR_MAP) {
			sigfillset(&newmask);
			_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
			_mutex_lock(&no_byname_lock);
			no_byname_map = 1;
			_mutex_unlock(&no_byname_lock);
			_thr_sigsetmask(SIG_SETMASK, &oldmask, (sigset_t*)NULL);
		} else /* if (res == NSS_SUCCESS) <==== */ {
			return (res);
		}
	}

	return (_nss_nis_XY_all(be, argp, 1, argp->key.name, check_name));
}

static nss_status_t
getbynumber(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char			numstr[12];

	sprintf(numstr, "%d", argp->key.number);
	return (_nss_nis_lookup(be, argp, 1, "rpc.bynumber", numstr, 0));
}

static nis_backend_op_t rpc_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbyname,
	getbynumber
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_rpc_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(rpc_ops,
				sizeof (rpc_ops) / sizeof (rpc_ops[0]),
				"rpc.bynumber"));
}
