/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_imp_sec_context.c	1.15	98/01/22 SMI"

/*
 *  glue routine gss_export_sec_context
 */

#include <mechglueP.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

OM_uint32
gss_import_sec_context(minor_status,
			interprocess_token,
			context_handle)

OM_uint32 *			minor_status;
const gss_buffer_t		interprocess_token;
gss_ctx_id_t *			context_handle;

{
	OM_uint32		length;
	OM_uint32		status;
	char			*p;
	gss_union_ctx_id_t	ctx;
	gss_buffer_desc		token;
	gss_mechanism		mech;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (context_handle == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE | GSS_S_NO_CONTEXT);
	*context_handle = GSS_C_NO_CONTEXT;

	if (GSS_EMPTY_BUFFER(interprocess_token))
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_DEFECTIVE_TOKEN);

	status = GSS_S_FAILURE;

	ctx = (gss_union_ctx_id_t) malloc(sizeof (gss_union_ctx_id_desc));
	if (!ctx)
		return (GSS_S_FAILURE);

	ctx->mech_type = (gss_OID) malloc(sizeof (gss_OID_desc));
	if (!ctx->mech_type) {
		free(ctx);
		return (GSS_S_FAILURE);
	}

	p = interprocess_token->value;
	length = (OM_uint32)*p++;
	length = (OM_uint32)(length << 8) + *p++;
	length = (OM_uint32)(length << 8) + *p++;
	length = (OM_uint32)(length << 8) + *p++;

	ctx->mech_type->length = length;
	ctx->mech_type->elements = malloc(length);
	if (!ctx->mech_type->elements) {
		goto error_out;
	}
	memcpy(ctx->mech_type->elements, p, length);
	p += length;

	token.length = interprocess_token->length - 4 - length;
	token.value = p;

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	mech = __gss_get_mechanism(ctx->mech_type);
	if (!mech) {
		status = GSS_S_BAD_MECH;
		goto error_out;
	}
	if (!mech->gss_import_sec_context) {
		status = GSS_S_UNAVAILABLE;
		goto error_out;
	}

	status = mech->gss_import_sec_context(mech->context, minor_status,
					&token, &ctx->internal_ctx_id);

	if (status == GSS_S_COMPLETE) {
		*context_handle = ctx;
		return (GSS_S_COMPLETE);
	}

error_out:
	if (ctx) {
		if (ctx->mech_type) {
			if (ctx->mech_type->elements)
				free(ctx->mech_type->elements);
			free(ctx->mech_type);
		}
		free(ctx);
	}
	return (status);
}
