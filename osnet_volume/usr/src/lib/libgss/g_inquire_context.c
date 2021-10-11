/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_inquire_context.c	1.14	98/01/22 SMI"

/*
 *  glue routine for gss_inquire_context
 */

#include <mechglueP.h>
#include <stdlib.h>

/* Last argument new for V2 */
OM_uint32
gss_inquire_context(
		minor_status,
		context_handle,
		src_name,
		targ_name,
		lifetime_rec,
		mech_type,
		ctx_flags,
		locally_initiated,
		open)

OM_uint32 *		minor_status;
const gss_ctx_id_t	context_handle;
gss_name_t *		src_name;
gss_name_t *		targ_name;
OM_uint32 *		lifetime_rec;
gss_OID *		mech_type;
OM_uint32 *		ctx_flags;
int *			locally_initiated;
int *			open;

{
	gss_union_ctx_id_t	ctx;
	gss_mechanism		mech;
	OM_uint32		status, temp_minor;
	gss_name_t localTargName = NULL, localSourceName = NULL;

	if (!minor_status)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*minor_status = 0;

	/* if the context_handle is Null, return NO_CONTEXT error */
	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

	/* set all output value to NULL */
	if (src_name)
		*src_name = NULL;

	if (targ_name)
		*targ_name = NULL;

	if (mech_type)
		*mech_type = NULL;

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	ctx = (gss_union_ctx_id_t) context_handle;
	mech = __gss_get_mechanism(ctx->mech_type);

	if (!mech || !mech->gss_inquire_context || !mech->gss_display_name ||
		!mech->gss_release_name) {
		return (GSS_S_UNAVAILABLE);
	}

	status = mech->gss_inquire_context(
				mech->context,
				minor_status,
				ctx->internal_ctx_id,
				(src_name ? &localSourceName : NULL),
				(targ_name ? &localTargName : NULL),
				lifetime_rec,
				NULL,
				ctx_flags,
				locally_initiated,
				open);

	if (status != GSS_S_COMPLETE) {
		return (status);
	}

	/* need to convert names */
	if (src_name) {
		status = __gss_convert_name_to_union_name(minor_status, mech,
						localSourceName, src_name);
		if (status != GSS_S_COMPLETE) {
			if (localTargName)
				mech->gss_release_name(mech->context,
						&temp_minor, &localTargName);
			return (status);
		}
	}

	if (targ_name) {
		status = __gss_convert_name_to_union_name(minor_status, mech,
						localTargName, targ_name);

		if (status != GSS_S_COMPLETE) {
			if (src_name)
				gss_release_name(&temp_minor, src_name);

			return (status);
		}
	}

	/* spec says mech type must point to static storage */
	if (mech_type)
		*mech_type = &mech->mech_type;
	return (GSS_S_COMPLETE);
}
