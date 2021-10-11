/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_rel_oid_set.c	1.12	97/11/11 SMI"

/*
 *  glue routine for gss_release_oid_set
 */

#include <mechglueP.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

OM_uint32
gss_release_oid_set(minor_status, set)

OM_uint32 *			minor_status;
gss_OID_set *			set;
{
	OM_uint32 index;
	gss_OID oid;
	if (minor_status)
		*minor_status = 0;

	if (set == NULL)
		return (GSS_S_COMPLETE);

	if (*set == GSS_C_NULL_OID_SET)
		return (GSS_S_COMPLETE);

	for (index = 0; index < (*set)->count; index++) {
		oid = &(*set)->elements[index];
		free(oid->elements);
	}
	free((*set)->elements);
	free(*set);

	*set = GSS_C_NULL_OID_SET;

	return (GSS_S_COMPLETE);
}
