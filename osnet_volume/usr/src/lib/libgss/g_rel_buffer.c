/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_rel_buffer.c	1.10	97/11/09 SMI"

/*
 *  glue routine for gss_release_buffer
 */

#include <mechglueP.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

OM_uint32
gss_release_buffer(minor_status, buffer)

OM_uint32 *			minor_status;
gss_buffer_t			buffer;
{
	if (minor_status)
		*minor_status = 0;

	/* if buffer is NULL, return */

	if (buffer == GSS_C_NO_BUFFER)
		return (GSS_S_COMPLETE);

	if ((buffer->length) && (buffer->value)) {
		free(buffer->value);
		buffer->length = 0;
		buffer->value = NULL;
	}

	return (GSS_S_COMPLETE);
}
