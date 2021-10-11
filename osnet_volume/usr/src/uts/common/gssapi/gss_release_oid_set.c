/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gss_release_oid_set.c	1.8	98/01/22 SMI"

/*
 *  glue routine for gss_release_oid_set
 */

#include "mechglueP.h"

OM_uint32
gss_release_oid_set(OM_uint32 *minor_status, gss_OID_set *set)
{

	if (minor_status)
		*minor_status = 0;

	if (set == NULL)
		return (GSS_S_COMPLETE);

	if (*set == GSS_C_NULL_OID_SET)
		return (GSS_S_COMPLETE);

	FREE((*set)->elements, (*set)->count * sizeof (gss_OID_desc));
	FREE(*set, sizeof (gss_OID_set_desc));

	*set = GSS_C_NULL_OID_SET;

	return (GSS_S_COMPLETE);
}
