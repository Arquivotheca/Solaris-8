/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_rel_name.c	1.10	97/11/09 SMI"

/*
 *  glue routine for gss_release_name
 */

#include <mechglueP.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

OM_uint32
gss_release_name(minor_status,
			input_name)

OM_uint32 *			minor_status;
gss_name_t *			input_name;

{
	gss_union_name_t	union_name;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	/* if input_name is NULL, return error */
	if (input_name == 0)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

	/*
	 * free up the space for the external_name and then
	 * free the union_name descriptor
	 */

	union_name = (gss_union_name_t) *input_name;
	*input_name = 0;
	*minor_status = 0;

	if (union_name->name_type)
		gss_release_oid(minor_status, &union_name->name_type);

	free(union_name->external_name->value);
	free(union_name->external_name);

	if (union_name->mech_type) {
		__gss_release_internal_name(minor_status,
					union_name->mech_type,
					&union_name->mech_name);
		gss_release_oid(minor_status, &union_name->mech_type);
	}

	free(union_name);

	return (GSS_S_COMPLETE);
}
