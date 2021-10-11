/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_unseal.c	1.13	98/01/22 SMI"

/*
 *  glue routine gss_unseal
 */

#include <mechglueP.h>

OM_uint32
gss_unseal(minor_status,
		context_handle,
		input_message_buffer,
		output_message_buffer,
		conf_state,
		qop_state)

OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
gss_buffer_t		input_message_buffer;
gss_buffer_t		output_message_buffer;
int *			conf_state;
int *			qop_state;

{

	return (GSS_S_BAD_MECH);
}

OM_uint32
gss_unwrap(minor_status,
		context_handle,
		input_message_buffer,
		output_message_buffer,
		conf_state,
		qop_state)

OM_uint32 *		minor_status;
const gss_ctx_id_t	context_handle;
const gss_buffer_t	input_message_buffer;
gss_buffer_t		output_message_buffer;
int *			conf_state;
gss_qop_t *		qop_state;

{
	return (gss_unseal(minor_status, (gss_ctx_id_t)context_handle,
			(gss_buffer_t)input_message_buffer,
			output_message_buffer, conf_state, (int *) qop_state));
}
