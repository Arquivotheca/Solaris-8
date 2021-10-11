/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_process_context.c	1.12	98/01/22 SMI"

/*
 *  glue routine gss_process_context
 */

#include <mechglueP.h>

OM_uint32
gss_process_context_token(minor_status,
				context_handle,
				token_buffer)

OM_uint32 *			minor_status;
const gss_ctx_id_t		context_handle;
gss_buffer_t			token_buffer;

{
	OM_uint32		status;
	gss_union_ctx_id_t	ctx;
	gss_mechanism		mech;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

	if (GSS_EMPTY_BUFFER(token_buffer))
		return (GSS_S_CALL_INACCESSIBLE_READ);

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	ctx = (gss_union_ctx_id_t) context_handle;
	mech = __gss_get_mechanism(ctx->mech_type);

	if (mech) {

		if (mech->gss_process_context_token)
			status = mech->gss_process_context_token(
							mech->context,
							minor_status,
							ctx->internal_ctx_id,
							token_buffer);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}
