/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_exp_sec_context.c	1.13	97/11/11 SMI"

/*
 *  glue routine for gss_export_sec_context
 */

#include <mechglueP.h>
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

OM_uint32
gss_export_sec_context(minor_status,
			context_handle,
			interprocess_token)

OM_uint32 *			minor_status;
gss_ctx_id_t *			context_handle;
gss_buffer_t			interprocess_token;

{
	OM_uint32		status;
	OM_uint32 		length;
	gss_union_ctx_id_t	ctx;
	gss_mechanism		mech;
	gss_buffer_desc		token;
	char			*buf;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (context_handle == NULL || *context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

	if (interprocess_token == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	ctx = (gss_union_ctx_id_t) *context_handle;
	mech = __gss_get_mechanism(ctx->mech_type);
	if (!mech)
		return (GSS_S_BAD_MECH);
	if (!mech->gss_export_sec_context)
		return (GSS_S_UNAVAILABLE);

	status = mech->gss_export_sec_context(mech->context, minor_status,
					&ctx->internal_ctx_id, &token);
	if (status != GSS_S_COMPLETE)
		return (status);

	length = token.length + 4 + ctx->mech_type->length;
	interprocess_token->length = length;
	interprocess_token->value = malloc(length);
	if (interprocess_token->value == 0) {
		(void) gss_release_buffer(minor_status, &token);
		return (GSS_S_FAILURE);
	}
	buf = interprocess_token->value;
	length = ctx->mech_type->length;
	buf[3] = (unsigned char) (length & 0xFF);
	length >>= 8;
	buf[2] = (unsigned char) (length & 0xFF);
	length >>= 8;
	buf[1] = (unsigned char) (length & 0xFF);
	length >>= 8;
	buf[0] = (unsigned char) (length & 0xFF);
	memcpy(buf+4, ctx->mech_type->elements,
			(size_t) ctx->mech_type->length);
	memcpy(buf+4+ctx->mech_type->length, token.value, token.length);

	(void) gss_release_buffer(minor_status, &token);

	free(ctx->mech_type->elements);
	free(ctx->mech_type);
	free(ctx);
	*context_handle = 0;

	return (GSS_S_COMPLETE);
}
