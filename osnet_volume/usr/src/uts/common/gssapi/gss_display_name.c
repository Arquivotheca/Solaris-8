/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gss_display_name.c	1.9	98/01/22 SMI"

/*
 *  glue routine for gss_display_name()
 */

#include <mechglueP.h>
#include <gssapi/kgssapi_defs.h>
OM_uint32
gss_display_name(OM_uint32 *minor_status,
	const gss_name_t input_name,
	gss_buffer_t output_name_buffer,
	gss_OID *output_name_type)
{
	gss_union_name_t	union_name;

	if (input_name == 0)
		return (GSS_S_BAD_NAME);

	union_name = (gss_union_name_t) input_name;

	GSSLOG(8, "union_name value %s\n",
		(char *)union_name->external_name->value);

	/*
	 * copy the value of the external_name component of the union
	 * name into the output_name_buffer and point the output_name_type
	 * to the name_type component of union_name
	 */
	if (output_name_type != NULL)
		*output_name_type = union_name->name_type;

	if (output_name_buffer != NULL) {
		output_name_buffer->length = union_name->external_name->length;

		output_name_buffer->value =
			(void *) MALLOC(output_name_buffer->length);

		memcpy(output_name_buffer->value,
			union_name->external_name->value,
			output_name_buffer->length);
	}

	if (minor_status)
		*minor_status = 0;

	return (GSS_S_COMPLETE);
}
