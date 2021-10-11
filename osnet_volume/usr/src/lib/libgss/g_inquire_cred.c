/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_inquire_cred.c	1.15	99/01/19 SMI"

/*
 *  glue routine for gss_inquire_cred
 */

#include <mechglueP.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

OM_uint32
gss_inquire_cred(minor_status,
			cred_handle,
			name,
			lifetime,
			cred_usage,
			mechanisms)

OM_uint32 *			minor_status;
const gss_cred_id_t		cred_handle;
gss_name_t *			name;
OM_uint32 *			lifetime;
int *				cred_usage;
gss_OID_set *			mechanisms;

{
	OM_uint32		status, elapsed_time, temp_minor_status;
	gss_union_cred_t	union_cred;
	gss_mechanism		mech;
	gss_name_t		internal_name;
	int			i;

	/* check parms and set to defaults */
	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (name)
		*name = NULL;

	if (mechanisms)
		*mechanisms = NULL;

	if (cred_handle == GSS_C_NO_CREDENTIAL) {
	/*
	 * No credential was supplied. This means we can't get a mechanism
	 * pointer to call the mechanism specific gss_inquire_cred.
	 * So, call get_mechanism with an arguement of GSS_C_NULL_OID.
	 * get_mechanism will return the first mechanism in the mech
	 * array, which becomes the default mechanism.
	 */

		if ((mech = __gss_get_mechanism(GSS_C_NULL_OID)) == NULL)
			return (GSS_S_DEFECTIVE_CREDENTIAL);

		if (!mech->gss_inquire_cred)
			return (GSS_S_UNAVAILABLE);

		status = mech->gss_inquire_cred(mech->context, minor_status,
						GSS_C_NO_CREDENTIAL,
						name ? &internal_name : NULL,
						lifetime, cred_usage,
						mechanisms);

		if (status != GSS_S_COMPLETE)
			return (status);

		if (name) {
		/*
		 * Convert internal_name into a union_name equivalent.
		 */
			status = __gss_convert_name_to_union_name(
						&temp_minor_status, mech,
						internal_name, name);
			if (status != GSS_S_COMPLETE) {
				*minor_status = temp_minor_status;
				if (mechanisms && *mechanisms) {
					gss_release_oid_set(&temp_minor_status,
							mechanisms);
				}
				return (status);
			}
		}
		return (GSS_S_COMPLETE);
	}

	/* get the cred_handle cast as a union_credentials structure */

	union_cred = (gss_union_cred_t) cred_handle;

	/*
	 * get the information out of the union_cred structure that was
	 * placed there during gss_acquire_cred.
	 */

	if (cred_usage != NULL)
		*cred_usage = union_cred->auxinfo.cred_usage;

	if (lifetime != NULL) {
		elapsed_time = time(0) - union_cred->auxinfo.creation_time;
		*lifetime = union_cred->auxinfo.time_rec < elapsed_time ? 0 :
		union_cred->auxinfo.time_rec - elapsed_time;
	}

	/*
	 * if name is non_null,
	 * call gss_import_name() followed by gss_canonicalize_name()
	 * to get a mechanism specific name passed back to the caller.
	 * If this call fails, return failure to our caller.
	 * XXX The cred_handle may contain an array of mechanism OID's
	 * but we only return the MN for the first mechanism to the caller.
	 * In theory, we should modify this to provide an array of MN's
	 * one per mechanism back to the caller.
	 */

	if (name != NULL) {
		if ((gss_import_name(minor_status,
					&union_cred->auxinfo.name,
					union_cred->auxinfo.name_type,
					name) != GSS_S_COMPLETE) ||
			(gss_canonicalize_name(minor_status, *name,
					&union_cred->mechs_array[0],
					NULL) != GSS_S_COMPLETE)) {
			status = GSS_S_DEFECTIVE_CREDENTIAL;
			goto error;
		}
	}

	/*
	 * copy the mechanism set in union_cred into an OID set and return in
	 * the mechanisms parameter.
	 */
	if (mechanisms != NULL) {
		status = GSS_S_FAILURE;
		*mechanisms = (gss_OID_set) malloc(sizeof (gss_OID_set_desc));
		if (*mechanisms == NULL)
			goto error;

		(*mechanisms)->count = 0;
		(*mechanisms)->elements =
			(gss_OID) malloc(sizeof (gss_OID_desc) *
						union_cred->count);

		if ((*mechanisms)->elements == NULL) {
			free(*mechanisms);
			*mechanisms = NULL;
			goto error;
		}

		for (i = 0; i < union_cred->count; i++) {
			(*mechanisms)->elements[i].elements = (void *)
				malloc(union_cred->mechs_array[i].length);
			if ((*mechanisms)->elements[i].elements == NULL)
				goto error;
			g_OID_copy(&(*mechanisms)->elements[i],
					&union_cred->mechs_array[i]);
			(*mechanisms)->count++;
		}
	}

	return (GSS_S_COMPLETE);

error:
	/*
	 * cleanup any allocated memory - we can just call
	 * gss_release_oid_set, because the set is constructed so that
	 * count always references the currently copied number of
	 * elements.
	 */
	if (mechanisms && *mechanisms != NULL)
		gss_release_oid_set(&temp_minor_status, mechanisms);

	if (name && *name != NULL)
		gss_release_name(&temp_minor_status, name);

	return (status);
}

OM_uint32
gss_inquire_cred_by_mech(minor_status, cred_handle, mech_type, name,
			initiator_lifetime, acceptor_lifetime, cred_usage)
	OM_uint32		*minor_status;
	const gss_cred_id_t	cred_handle;
	const gss_OID		mech_type;
	gss_name_t		*name;
	OM_uint32		*initiator_lifetime;
	OM_uint32		*acceptor_lifetime;
	gss_cred_usage_t	*cred_usage;
{
	gss_union_cred_t	union_cred;
	gss_cred_id_t		mech_cred;
	gss_mechanism		mech;
	OM_uint32		status, temp_minor_status;
	gss_name_t		internal_name;


	mech = __gss_get_mechanism(mech_type);
	if (!mech)
		return (GSS_S_BAD_MECH);
	if (!mech->gss_inquire_cred_by_mech)
		return (GSS_S_UNAVAILABLE);

	union_cred = (gss_union_cred_t) cred_handle;
	mech_cred = __gss_get_mechanism_cred(union_cred, mech_type);
	if (mech_cred == NULL)
		return (GSS_S_DEFECTIVE_CREDENTIAL);

	status = mech->gss_inquire_cred_by_mech(mech->context, minor_status,
						mech_cred, mech_type,
						name ? &internal_name : NULL,
						initiator_lifetime,
						acceptor_lifetime, cred_usage);

	if (status != GSS_S_COMPLETE)
		return (status);

	if (name) {
		/*
		 * Convert internal_name into a union_name equivalent.
		 */
		status = __gss_convert_name_to_union_name(
						&temp_minor_status, mech,
						internal_name, name);
		if (status != GSS_S_COMPLETE) {
			*minor_status = temp_minor_status;
			return (status);
		}
	}

	return (GSS_S_COMPLETE);
}
