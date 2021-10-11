/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident		"@(#)g_export_name.c	1.10	97/11/09 SMI"

/*
 * glue routine gss_export_name
 *
 * Will either call the mechanism defined gss_export_name, or if one is
 * not defined will call a generic_gss_export_name routine.
 */

#include <mechglueP.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32
gss_export_name(minor_status,
			input_name,
			exported_name)
OM_uint32 *		minor_status;
const gss_name_t	input_name;
gss_buffer_t		exported_name;
{
	gss_union_name_t		union_name;


	if (minor_status)
		*minor_status = 0;

	/* check out parameter */
	if (!exported_name)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	exported_name->value = NULL;
	exported_name->length = 0;

	/* check input parameter */
	if (!input_name)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

	union_name = (gss_union_name_t)input_name;

	/* the name must be in mechanism specific format */
	if (!union_name->mech_type)
		return (GSS_S_NAME_NOT_MN);

	return __gss_export_internal_name(minor_status, union_name->mech_type,
					union_name->mech_name, exported_name);
}
