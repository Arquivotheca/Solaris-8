/*
 *	dhmec.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dhmech.c	1.5	98/06/11 SMI"

#include "dh_gssapi.h"
#include <stdlib.h>

/*
 * gss_config structure for Diffie-Hellman family of mechanisms.
 * This structure is defined in mechglueP.h and defines the entry points
 * that libgss uses to call a backend.
 */
static struct gss_config dh_mechanism = {
	{0, 0},				/* OID for mech type. */
	0,
	__dh_gss_acquire_cred,
	__dh_gss_release_cred,
	__dh_gss_init_sec_context,
	__dh_gss_accept_sec_context,
	__dh_gss_process_context_token,
	__dh_gss_delete_sec_context,
	__dh_gss_context_time,
	__dh_gss_display_status,
	NULL, /* Back ends don't implement this */
	__dh_gss_compare_name,
	__dh_gss_display_name,
	__dh_gss_import_name,
	__dh_gss_release_name,
	__dh_gss_inquire_cred,
	NULL, /* Back ends don't implement this */
	__dh_gss_export_sec_context,
	__dh_gss_import_sec_context,
	__dh_gss_inquire_cred_by_mech,
	__dh_gss_inquire_names_for_mech,
	__dh_gss_inquire_context,
	__dh_gss_internal_release_oid,
	__dh_gss_wrap_size_limit,
	__dh_pname_to_uid,
	__dh_gss_export_name,
	__dh_gss_sign,
	__dh_gss_verify,
};

/*
 * __dh_gss_initialize:
 * Each mechanism in the Diffie-Hellman family of mechanisms calls this
 * routine passing a pointer to a gss_config structure. This routine will
 * then check that the mech is not already initialized (If so just return
 * the mech). It will then assign the entry points that are common to the
 * mechanism family to the uninitialized mech. After which, it allocate space
 * for that mechanism's context. It will be up to the caller to fill in
 * its mechanism OID and fill in the corresponding fields in mechanism
 * specific context.
 */
gss_mechanism
__dh_gss_initialize(gss_mechanism mech)
{
	if (mech->context != NULL)
		return (mech);    /* already initialized */

	/* Copy the common entry points for this mechcanisms */
	*mech = dh_mechanism;

	/* Allocate space for this mechanism's context */
	mech->context = New(dh_context_desc, 1);
	if (mech->context == NULL)
		return (NULL);

	/* return the mech */
	return (mech);
}
