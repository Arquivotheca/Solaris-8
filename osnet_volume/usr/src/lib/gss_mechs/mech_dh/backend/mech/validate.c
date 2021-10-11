/*
 *	validate.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)validate.c	1.2	98/05/25 SMI"

#include "dh_gssapi.h"

/*
 * This module provides the interface to validating contexts, credentials,
 * and principals. The current implementation does nothing.
 */

/*
 * __dh_validate_context: Validate a context, i.e., check if the context is
 * in the database. If the context is non null then return success, else
 * return bad context.
 */

OM_uint32
__dh_validate_context(dh_gss_context_t ctx)
{
	if (ctx && ctx->state != BAD)
		return (DH_SUCCESS);
	return (DH_BAD_CONTEXT);
}

/*
 * __dh_install_context: Install the context in to the database of current
 * contexts.
 */
OM_uint32
__dh_install_context(dh_gss_context_t ctx)
{
	return (ctx ? DH_SUCCESS : DH_BAD_CONTEXT);
}

/*
 * __dh_remove_context: Deinstall the context from the database of current
 * contexts.
 */
OM_uint32
__dh_remove_context(dh_gss_context_t ctx)
{
	return (ctx ? DH_SUCCESS : DH_BAD_CONTEXT);
}

/*
 * __dh_validate_cred: Check the cred database if the supplied crediential
 * is present, valid.
 */

/*ARGSUSED*/
OM_uint32
__dh_validate_cred(dh_cred_id_t cred)
{
	return (DH_SUCCESS);
}

/*
 * __dh_install_cred: Installed the cred into the credential database
 */

/*ARGSUSED*/
OM_uint32
__dh_install_cred(dh_cred_id_t cred)
{
	return (DH_SUCCESS);
}

/*
 * __dh_remove_cred: Remove the supplied cred from the database.
 */

/*ARGSUSED*/
OM_uint32
__dh_remove_cred(dh_cred_id_t cred)
{
	return (DH_SUCCESS);
}

/*
 * Check if a principal is valid.
 *
 * XXX We could check for a valid netname.
 */

/*ARGSUSED*/
OM_uint32
__dh_validate_principal(dh_principal principal)
{
	return (DH_SUCCESS);
}
