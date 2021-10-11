/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_dsp_name.c	1.11	98/01/22 SMI"

/*
 *  glue routine for gss_display_name()
 *
 */

#include <mechglueP.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

OM_uint32
gss_display_name(minor_status,
			input_name,
			output_name_buffer,
			output_name_type)

OM_uint32 *			minor_status;
const gss_name_t		input_name;
gss_buffer_t			output_name_buffer;
gss_OID *			output_name_type;

{
	OM_uint32			major_status;
	gss_union_name_t		union_name;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (input_name == 0)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

	if (output_name_buffer == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if (output_name_type)
		*output_name_type = NULL;

	union_name = (gss_union_name_t) input_name;

	if (union_name->mech_type) {
		/*
		 * OK, we have a mechanism-specific name; let's use it!
		 */
		return (__gss_display_internal_name(minor_status,
							union_name->mech_type,
							union_name->mech_name,
							output_name_buffer,
							output_name_type));
	}

	/*
	 * copy the value of the external_name component of the union
	 * name into the output_name_buffer and point the output_name_type
	 * to the name_type component of union_name
	 */
	if (output_name_type != NULL) {
		major_status = generic_gss_copy_oid(minor_status,
						union_name->name_type,
						output_name_type);
		if (major_status != GSS_S_COMPLETE)
			return (major_status);
	}

	if ((output_name_buffer->value =
		malloc(union_name->external_name->length + 1)) == NULL) {
		if (output_name_type) {
			free((*output_name_type)->elements);
			free(*output_name_type);
			*output_name_type = NULL;
		}
		return (GSS_S_FAILURE);
	}
	output_name_buffer->length = union_name->external_name->length;
	memcpy(output_name_buffer->value, union_name->external_name->value,
		union_name->external_name->length);
	((char *)output_name_buffer->value)[output_name_buffer->length] = '\0';

	return (GSS_S_COMPLETE);
}
