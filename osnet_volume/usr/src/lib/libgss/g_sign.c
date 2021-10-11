/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_sign.c	1.14	98/04/23 SMI"

/*
 *  glue routine gss_sign
 */

#include <mechglueP.h>

OM_uint32
gss_sign(minor_status,
	context_handle,
	qop_req,
	message_buffer,
	msg_token)

OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
int			qop_req;
gss_buffer_t		message_buffer;
gss_buffer_t		msg_token;

{
	OM_uint32		status;
	gss_union_ctx_id_t	ctx;
	gss_mechanism		mech;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

	if (message_buffer == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	if (msg_token == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	msg_token->value = NULL;
	msg_token->length = 0;
	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	ctx = (gss_union_ctx_id_t) context_handle;
	mech = __gss_get_mechanism(ctx->mech_type);

	if (mech) {
		if (mech->gss_sign)
			status = mech->gss_sign(
						mech->context,
						minor_status,
						ctx->internal_ctx_id,
						qop_req,
						message_buffer,
						msg_token);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}

OM_uint32
gss_get_mic(minor_status,
		context_handle,
		qop_req,
		message_buffer,
		msg_token)

OM_uint32 *		minor_status;
const gss_ctx_id_t	context_handle;
gss_qop_t		qop_req;
const gss_buffer_t	message_buffer;
gss_buffer_t		msg_token;

{
	return (gss_sign(minor_status, (gss_ctx_id_t)context_handle,
		(int) qop_req, (gss_buffer_t)message_buffer, msg_token));
}
