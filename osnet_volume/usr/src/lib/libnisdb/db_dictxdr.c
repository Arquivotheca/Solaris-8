/*
 *	db_dictxdr.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_dictxdr.c	1.2	93/10/04 SMI"

#include "db_dictionary_c.h"
#include "db_vers_c.h"

extern vers db_update_version;

extern make_zero(vers*);

/* Special xdr_db_dict_desc that understands optional version number at end. */
bool_t
xdr_db_dict_desc(XDR *xdrs, db_dict_desc *objp)
{

	if (!xdr_db_dict_version(xdrs, &objp->impl_vers))
		return (FALSE);
	if (!xdr_array(xdrs, (char **)&objp->tables.tables_val,
		(u_int *) &objp->tables.tables_len, ~0,
		sizeof (db_table_desc_p), (xdrproc_t) xdr_db_table_desc_p))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->count))
		return (FALSE);

	if (xdrs->x_op == XDR_DECODE) {
		/* If no version was found, set version to 0. */
		if (!xdr_vers(xdrs, (void**) &db_update_version))
			make_zero(&db_update_version);
		return (TRUE);
	} else if (xdrs->x_op == XDR_ENCODE) {
		/* Always write out version */
		if (!xdr_vers(xdrs, (void**) &db_update_version))
			return (FALSE);
	} /* else XDR_FREE: do nothing */

	return (TRUE);
}
