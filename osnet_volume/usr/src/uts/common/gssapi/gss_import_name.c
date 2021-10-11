/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gss_import_name.c	1.11	98/01/22 SMI"

/*
 *  glue routine gss_import_name
 *
 */

#include "mechglueP.h"
#include <sys/errno.h>
OM_uint32
gss_import_name(
	OM_uint32 *minor_status,
	const gss_buffer_t input_name_buffer,
	const gss_OID input_name_type,
	gss_name_t *output_name)

{
	gss_union_name_t	union_name;
	OM_uint32		major_status = GSS_S_FAILURE;

	if (minor_status)
		*minor_status = 0;

	/* if output_name is NULL, simply return */

	if (output_name == NULL)
		return (GSS_S_COMPLETE);

	*output_name = 0;

	if (input_name_buffer == GSS_C_NO_BUFFER || input_name_type == NULL)
		return (GSS_S_BAD_NAME);

	/*
	 * First create the union name struct that will hold the external
	 * name and the name type.
	 */

	union_name = (gss_union_name_t) MALLOC(sizeof (gss_union_name_desc));

	if (!union_name) {
		*minor_status = ENOMEM;
		goto allocation_failure;
	}
	union_name->mech_type = 0;
	union_name->mech_name = 0;
	union_name->name_type = 0;
	union_name->external_name = 0;

	/*
	 * All we do here is record the external name and name_type.
	 * When the name is actually used, the underlying gss_import_name()
	 * is called for the appropriate mechanism.
	 * Since the name type may be a constant or comming from the
	 * rpc resoults, we must make a copy.
	 */
	union_name->external_name =
	(gss_buffer_t) MALLOC(sizeof (gss_buffer_desc));

	if (!union_name->external_name) {
		*minor_status = ENOMEM;
		goto allocation_failure;
	}

	union_name->external_name->length = input_name_buffer->length;
	union_name->external_name->value =
	(void *) MALLOC(input_name_buffer->length);

	if (!union_name->external_name->value) {
		*minor_status = ENOMEM;
		goto allocation_failure;
	}

	memcpy(union_name->external_name->value, input_name_buffer->value,
		input_name_buffer->length);

	/*
	 * making a copy of the name_type structure and elements
	 * we now delete it when calling gss_release_name
	 */
	union_name->name_type = (gss_OID) MALLOC(sizeof (gss_OID_desc));

	if (!union_name->name_type) {
		*minor_status = ENOMEM;
		goto allocation_failure;
	}

	union_name->name_type->elements = (void *)
		MALLOC(input_name_type->length);

	if (!union_name->name_type->elements) {
		*minor_status = ENOMEM;
		goto allocation_failure;
	}

	memcpy(union_name->name_type->elements,
		input_name_type->elements, input_name_type->length);
	union_name->name_type->length = input_name_type->length;

	*output_name = (gss_name_t) union_name;

	return (GSS_S_COMPLETE);

allocation_failure:
	if (union_name) {

		if (union_name->external_name) {
			if (union_name->external_name->value)
				FREE(union_name->external_name->value,
					union_name->external_name->length);
			FREE(union_name->external_name,
				sizeof (gss_buffer_desc));
		}

		if (union_name->name_type) {
			FREE(union_name->name_type, sizeof (gss_OID_desc));
		}
		FREE(union_name, sizeof (gss_union_name_desc));
	}
	return (major_status);
}
