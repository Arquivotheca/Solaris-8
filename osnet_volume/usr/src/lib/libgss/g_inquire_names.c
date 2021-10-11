/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_inquire_names.c	1.11	98/01/22 SMI"

/*
 *  glue routine for gss_inquire_context
 */

#include <mechglueP.h>

/* Last argument new for V2 */
OM_uint32
gss_inquire_names_for_mech(minor_status, mechanism, name_types)

OM_uint32 *		minor_status;
const gss_OID 		mechanism;
gss_OID_set *		name_types;

{
	OM_uint32		status;
	gss_mechanism		mech;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (name_types == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	mech = __gss_get_mechanism(mechanism);

	if (mech) {

		if (mech->gss_inquire_names_for_mech)
			status = mech->gss_inquire_names_for_mech(
					mech->context,
					minor_status,
					mechanism,
					name_types);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}
