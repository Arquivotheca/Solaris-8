/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gss_release_buffer.c	1.7	98/01/22 SMI"

/*
 *  glue routine for gss_release_buffer
 */

#include "mechglueP.h"

OM_uint32
gss_release_buffer(OM_uint32 *minor_status, gss_buffer_t buffer)
{

	if (minor_status)
		*minor_status = 0;

	/* if buffer is NULL, return */

	if (buffer == GSS_C_NO_BUFFER)
		return (GSS_S_COMPLETE);

	if ((buffer->length) && (buffer->value)) {
		FREE(buffer->value, buffer->length);
		buffer->length = 0;
		buffer->value = NULL;
	}

	return (GSS_S_COMPLETE);
}
