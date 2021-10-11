/*
 *	getprotoent.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nisplus/getprotoent.c -- NIS+ backend for nsswitch "proto" database
 */

#pragma ident	"@(#)getprotoent.c	1.11	97/08/12 SMI"

#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include "nisplus_common.h"
#include "nisplus_tables.h"

static nss_status_t
getbyname(be, a)
	nisplus_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	/*
	 * Don't have to do anything for case-insensitivity;  the NIS+ table
	 * has the right flags enabled in the 'cname' and 'name' columns.
	 */
	return (_nss_nisplus_lookup(be, argp, PROTO_TAG_NAME, argp->key.name));
}

static nss_status_t
getbynumber(be, a)
	nisplus_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	char		numstr[12];

	sprintf(numstr, "%d", argp->key.number);
	return (_nss_nisplus_lookup(be, argp, PROTO_TAG_NUMBER, numstr));
}


/*
 * place the results from the nis_object structure into argp->buf.result
 * Returns NSS_STR_PARSE_{SUCCESS, ERANGE, PARSE}
 */
static int
nis_object2ent(nobj, obj, argp)
	int		nobj;
	nis_object	*obj;
	nss_XbyY_args_t	*argp;
{
	char	*buffer, *limit, *val;
	int		buflen = argp->buf.buflen;
	struct 	protoent *proto;
	int		len, ret;
	struct	entry_col *ecol;

	limit = argp->buf.buffer + buflen;
	proto = (struct protoent *)argp->buf.result;
	buffer = argp->buf.buffer;

	/*
	 * <-----buffer + buflen -------------->
	 * |-----------------|----------------|
	 * | pointers vector | aliases grow   |
	 * | for aliases     |                |
	 * | this way ->     | <- this way    |
	 * |-----------------|----------------|
	 *
	 *
	 * ASSUME: name, aliases and number columns in NIS+ tables ARE
	 * null terminated.
	 *
	 * get cname and aliases
	 */

	proto->p_aliases = (char **) ROUND_UP(buffer, sizeof (char **));
	if ((char *)proto->p_aliases >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}

	proto->p_name = NULL;

	/*
	 * Assume that CNAME is the first column and NAME the second.
	 */
	ret = netdb_aliases_from_nisobj(obj, nobj, NULL,
		proto->p_aliases, &limit, &(proto->p_name), &len);
	if (ret != NSS_STR_PARSE_SUCCESS)
		return (ret);

	/*
	 * get protocol number from the first object
	 *
	 */
	ecol = obj->EN_data.en_cols.en_cols_val;
	EC_SET(ecol, PROTO_NDX_NUMBER, len, val);
	if (len <= 0)
		return (NSS_STR_PARSE_PARSE);
	proto->p_proto = atoi(val);

	return (NSS_STR_PARSE_SUCCESS);
}

static nisplus_backend_op_t proto_ops[] = {
	_nss_nisplus_destr,
	_nss_nisplus_endent,
	_nss_nisplus_setent,
	_nss_nisplus_getent,
	getbyname,
	getbynumber
};

/*ARGSUSED*/
nss_backend_t *
_nss_nisplus_protocols_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nisplus_constr(proto_ops,
				    sizeof (proto_ops) / sizeof (proto_ops[0]),
				    PROTO_TBLNAME, nis_object2ent));
}
