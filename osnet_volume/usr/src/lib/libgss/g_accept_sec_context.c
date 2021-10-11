/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_accept_sec_context.c	1.14	99/04/23 SMI"

/*
 *  glue routine for gss_accept_sec_context
 */

#include <mechglueP.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32
gss_accept_sec_context(minor_status,
			context_handle,
			verifier_cred_handle,
			input_token_buffer,
			input_chan_bindings,
			src_name,
			mech_type,
			output_token,
			ret_flags,
			time_rec,
			delegated_cred_handle)

OM_uint32 *			minor_status;
gss_ctx_id_t *			context_handle;
const gss_cred_id_t		verifier_cred_handle;
const gss_buffer_t		input_token_buffer;
const gss_channel_bindings_t	input_chan_bindings;
gss_name_t *			src_name;
gss_OID *			mech_type;
gss_buffer_t			output_token;
OM_uint32 *			ret_flags;
OM_uint32 *			time_rec;
gss_cred_id_t *			delegated_cred_handle;

{
	OM_uint32		status, temp_status, temp_minor_status;
	gss_union_ctx_id_t	union_ctx_id;
	gss_union_cred_t	union_cred;
	gss_cred_id_t	input_cred_handle = GSS_C_NO_CREDENTIAL;
	gss_name_t		internal_name = GSS_C_NO_NAME;
	gss_OID_desc	token_mech_type_desc;
	gss_OID		token_mech_type = &token_mech_type_desc;
	gss_mechanism	mech;

	/* check parameters first */
	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (context_handle == NULL || output_token == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/* clear optional fields */
	output_token->value = NULL;
	output_token->length = 0;
	if (src_name)
		*src_name = NULL;

	if (mech_type)
		*mech_type = NULL;

	if (delegated_cred_handle)
		*delegated_cred_handle = NULL;
	/*
	 * if context_handle is GSS_C_NO_CONTEXT, allocate a union context
	 * descriptor to hold the mech type information as well as the
	 * underlying mechanism context handle. Otherwise, cast the
	 * value of *context_handle to the union context variable.
	 */

	if (*context_handle == GSS_C_NO_CONTEXT) {

		if (GSS_EMPTY_BUFFER(input_token_buffer))
			return (GSS_S_CALL_INACCESSIBLE_READ);

		/* Get the token mech type */
		status = __gss_get_mech_type(token_mech_type,
					input_token_buffer);

		if (status)
			return (status);

		status = GSS_S_FAILURE;
		union_ctx_id = (gss_union_ctx_id_t)
			malloc(sizeof (gss_union_ctx_id_desc));
		if (!union_ctx_id)
			return (GSS_S_FAILURE);

		union_ctx_id->internal_ctx_id = GSS_C_NO_CONTEXT;
		status = generic_gss_copy_oid(&temp_minor_status,
					token_mech_type,
					&union_ctx_id->mech_type);
		if (status != GSS_S_COMPLETE) {
			free(union_ctx_id);
			return (status);
		}

		/* set the new context handle to caller's data */
		*context_handle = (gss_ctx_id_t) union_ctx_id;
	} else {
		union_ctx_id = *context_handle;
		token_mech_type = union_ctx_id->mech_type;
	}

	/*
	 * get the appropriate cred handle from the union cred struct.
	 * defaults to GSS_C_NO_CREDENTIAL if there is no cred, which will
	 * use the default credential.
	 */
	union_cred = (gss_union_cred_t) verifier_cred_handle;
	input_cred_handle = __gss_get_mechanism_cred(union_cred,
						token_mech_type);

	/*
	 * now select the approprate underlying mechanism routine and
	 * call it.
	 */

	mech = __gss_get_mechanism(token_mech_type);
	if (mech && mech->gss_accept_sec_context) {
		status = mech->gss_accept_sec_context(
					mech->context,
					minor_status,
					&union_ctx_id->internal_ctx_id,
					input_cred_handle,
					input_token_buffer,
					input_chan_bindings,
					(src_name != NULL ?
						&internal_name : NULL),
					mech_type,
					output_token,
					ret_flags,
					time_rec,
					delegated_cred_handle);

		/* If there's more work to do, keep going... */
		if (status == GSS_S_CONTINUE_NEEDED)
			return (GSS_S_CONTINUE_NEEDED);

		/* if the call failed, return with failure */
		if (status != GSS_S_COMPLETE)
			goto error_out;

		/*
		* if src_name is non-NULL,
		* convert internal_name into a union name equivalent
		* First call the mechanism specific display_name()
		* then call gss_import_name() to create
		* the union name struct cast to src_name
		*/
		if (src_name != NULL && status == GSS_S_COMPLETE) {
			temp_status = __gss_convert_name_to_union_name(
				&temp_minor_status, mech,
				internal_name, src_name);
			if (temp_status != GSS_S_COMPLETE) {
				*minor_status = temp_minor_status;
				if (output_token->length)
					gss_release_buffer(&temp_minor_status,
						output_token);
				return (temp_status);
			}
		}

		return	(status);
	} else {

		status = GSS_S_BAD_MECH;
	}

error_out:
	if (union_ctx_id) {
		if (union_ctx_id->mech_type) {
			if (union_ctx_id->mech_type->elements)
				free(union_ctx_id->mech_type->elements);
			free(union_ctx_id->mech_type);
		}
		free(union_ctx_id);
		*context_handle = GSS_C_NO_CONTEXT;
	}

	return (status);
}
