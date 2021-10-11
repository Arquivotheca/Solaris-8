/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_seal.c	1.19	98/04/21 SMI"

/*
 *  glue routine for gss_seal
 */

#include <mechglueP.h>

/*ARGSUSED*/
OM_uint32
gss_seal(minor_status,
		context_handle,
		conf_req_flag,
		qop_req,
		input_message_buffer,
		conf_state,
		output_message_buffer)

OM_uint32 *			minor_status;
gss_ctx_id_t			context_handle;
int				conf_req_flag;
int				qop_req;
gss_buffer_t			input_message_buffer;
int *				conf_state;
gss_buffer_t			output_message_buffer;
{

	return (GSS_S_BAD_MECH);
}

OM_uint32
gss_wrap(minor_status,
		context_handle,
		conf_req_flag,
		qop_req,
		input_message_buffer,
		conf_state,
		output_message_buffer)

OM_uint32 *			minor_status;
const gss_ctx_id_t		context_handle;
int				conf_req_flag;
gss_qop_t			qop_req;
const gss_buffer_t		input_message_buffer;
int *				conf_state;
gss_buffer_t			output_message_buffer;

{
	return gss_seal(minor_status, (gss_ctx_id_t)context_handle,
			conf_req_flag, (int) qop_req,
			(gss_buffer_t)input_message_buffer, conf_state,
			output_message_buffer);
}

/*
 * New for V2
 */
OM_uint32
gss_wrap_size_limit(minor_status, context_handle, conf_req_flag,
				qop_req, req_output_size, max_input_size)
	OM_uint32		*minor_status;
	const gss_ctx_id_t	context_handle;
	int			conf_req_flag;
	gss_qop_t		qop_req;
	OM_uint32		req_output_size;
	OM_uint32		*max_input_size;
{
	gss_union_ctx_id_t	ctx;
	gss_mechanism		mech;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

	if (max_input_size == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	ctx = (gss_union_ctx_id_t) context_handle;
	mech = __gss_get_mechanism(ctx->mech_type);

	if (!mech)
		return (GSS_S_BAD_MECH);

	if (!mech->gss_wrap_size_limit)
		return (GSS_S_UNAVAILABLE);

	return (mech->gss_wrap_size_limit(mech->context, minor_status,
				ctx->internal_ctx_id, conf_req_flag, qop_req,
				req_output_size, max_input_size));
}
