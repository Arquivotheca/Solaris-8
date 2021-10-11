/*
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nis/getgrent.c -- "nis" backend for nsswitch "group" database
 */

#pragma ident "@(#)getgrent.c	1.8	97/08/12 SMI"

#include <grp.h>
#include "nis_common.h"
#include <string.h>

#ifndef	NETID_GROUPIDS_VALID	/* OK to use netid.byname optimization? */
#define	NETID_GROUPIDS_VALID	0  /* No, need to write some code first */
#endif

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_nis_lookup(be, argp, 0,
				"group.byname", argp->key.name, 0));
}

static nss_status_t
getbygid(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char			gidstr[12];	/* More than enough */

	sprintf(gidstr, "%d", argp->key.gid);
	return (_nss_nis_lookup(be, argp, 0, "group.bygid", gidstr, 0));
}

static nss_status_t
getbymember(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	struct nss_groupsbymem	*argp = (struct nss_groupsbymem *) a;

	if (strcmp(argp->username, "root") == 0) {
		/*
		 * Assume that "root" can only sensibly be in /etc/group,
		 *   not in NIS or NIS+
		 * If we don't do this, a hung name-service may cause
		 *   a root login or su to hang.
		 */
		return (NSS_NOTFOUND);
	}

#if	NETID_GROUPIDS_VALID
	if (!argp->force_slow_way) {
		/* do the netid.byname thang */
		return (/* whatever you come up with */);
	}
#endif	/* NETID_GROUPIDS_VALID */

	return (_nss_nis_do_all(be, argp, argp->username,
				(nis_do_all_func_t) argp->process_cstr));
}

static nis_backend_op_t group_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_rigid,
	getbyname,
	getbygid,
	getbymember
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_group_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(group_ops,
				sizeof (group_ops) / sizeof (group_ops[0]),
				"group.byname"));
}
